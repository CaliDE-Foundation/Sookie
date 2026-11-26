/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "config.h"

#include "wayland/meta-wayland-layer-shell.h"

#include "backends/meta-logical-monitor-private.h"
#include "backends/meta-monitor-private.h"
#include "meta/compositor.h"
#include "meta/display.h"
#include "meta/meta-context.h"
#include "wayland/meta-wayland-actor-surface.h"
#include "wayland/meta-wayland-outputs.h"
#include "wayland/meta-wayland-private.h"
#include "wayland/meta-wayland-surface-private.h"
#include "wayland/meta-wayland.h"
#include "wlr-layer-shell-unstable-v1-server-protocol.h"

typedef struct _MetaWaylandLayerSurface
{
  MetaWaylandActorSurface parent;
  struct wl_resource *resource;
  MetaWaylandOutput *output;
  gulong output_destroy_id;
  LayerLayout requested;
  uint32_t initial_layer;
  uint32_t layer;
  uint32_t keyboard;
  GArray *serials;
  gboolean acked;
  gboolean mapped;
  gboolean closed;
  int configured_width;
  int configured_height;
} MetaWaylandLayerSurface;

typedef MetaWaylandActorSurfaceClass MetaWaylandLayerSurfaceClass;

G_DEFINE_TYPE (MetaWaylandLayerSurface, meta_wayland_layer_surface,
               META_TYPE_WAYLAND_ACTOR_SURFACE)

static MetaWaylandSurface *
get_surface (MetaWaylandLayerSurface *self)
{
  return meta_wayland_surface_role_get_surface (META_WAYLAND_SURFACE_ROLE (self));
}

static ClutterActor *
get_actor (MetaWaylandLayerSurface *self)
{
  return CLUTTER_ACTOR (meta_wayland_actor_surface_get_actor (META_WAYLAND_ACTOR_SURFACE (self)));
}

static MetaDisplay *
get_display (MetaWaylandLayerSurface *self)
{
  return meta_context_get_display (meta_wayland_compositor_get_context (get_surface (self)->compositor));
}

static void
unmap (MetaWaylandLayerSurface *self)
{
  ClutterActor *actor = get_actor (self);

  if (actor)
    {
      clutter_actor_hide (actor);
      if (clutter_actor_get_parent (actor))
        clutter_actor_remove_child (clutter_actor_get_parent (actor), actor);
    }
  if (self->mapped)
    meta_wayland_surface_notify_unmapped (get_surface (self));
  self->mapped = FALSE;
}

static void
close_surface (MetaWaylandLayerSurface *self)
{
  unmap (self);
  if (!self->closed && self->resource)
    zwlr_layer_surface_v1_send_closed (self->resource);
  self->closed = TRUE;
}

static void
on_output_destroyed (MetaWaylandOutput *output,
                     MetaWaylandLayerSurface *self)
{
  close_surface (self);
}

static gboolean
get_bounds (MetaWaylandLayerSurface *self, LayerRect *bounds)
{
  MetaDisplay *display = get_display (self);
  MtkRectangle rect;

  if (!display)
    return FALSE;
  if (self->output)
    {
      MetaMonitor *monitor = meta_wayland_output_get_monitor (self->output);
      MetaLogicalMonitor *logical = monitor ? meta_monitor_get_logical_monitor (monitor) : NULL;
      if (!logical)
        return FALSE;
      rect = meta_logical_monitor_get_layout (logical);
      meta_wayland_surface_set_main_monitor (get_surface (self), logical);
    }
  else
    {
      int monitor = meta_display_get_current_monitor (display);
      if (monitor < 0 || monitor >= meta_display_get_n_monitors (display))
        return FALSE;
      meta_display_get_monitor_geometry (display, monitor, &rect);
    }
  *bounds = (LayerRect) { rect.x, rect.y, rect.width, rect.height };
  return TRUE;
}

/* Dedicated scene groups, separate from the normal managed-window stack.
 * The experimental renderer is intentionally non-interactive until shell
 * and lock-screen policy is implemented. */
static ClutterActor *
get_layer_group (MetaCompositor *compositor, uint32_t layer)
{
  ClutterActor *stage = CLUTTER_ACTOR (meta_compositor_get_stage (compositor));
  ClutterActor *windows = meta_compositor_get_window_group (compositor);
  ClutterActor *top_windows = meta_compositor_get_top_window_group (compositor);
  const char *keys[] = { "sookie-layer-background", "sookie-layer-bottom",
                         "sookie-layer-top", "sookie-layer-overlay" };
  ClutterActor *groups[4];

  if (!stage || !windows || !top_windows || layer > 3)
    return NULL;
  if (!g_object_get_data (G_OBJECT (stage), keys[0]))
    {
      for (unsigned int i = 0; i < G_N_ELEMENTS (groups); i++)
        {
          groups[i] = clutter_actor_new ();
          clutter_actor_set_size (groups[i], 0, 0);
          g_object_set_data (G_OBJECT (stage), keys[i], groups[i]);
        }
      clutter_actor_insert_child_below (stage, groups[1], windows);
      clutter_actor_insert_child_below (stage, groups[0], groups[1]);
      clutter_actor_insert_child_above (stage, groups[2], windows);
      clutter_actor_insert_child_above (stage, groups[3], top_windows);
    }
  return g_object_get_data (G_OBJECT (stage), keys[layer]);
}

static void
render (MetaWaylandLayerSurface *self,
        LayerLayout layout,
        LayerRect bounds,
        uint32_t layer)
{
  MetaDisplay *display = get_display (self);
  MetaCompositor *compositor = display ? meta_display_get_compositor (display) : NULL;
  ClutterActor *actor = get_actor (self);
  ClutterActor *group;
  LayerRect geometry;
  MetaWaylandSurface *surface = get_surface (self);

  if (!compositor || !actor)
    return;
  group = get_layer_group (compositor, layer);
  layout.width = meta_wayland_surface_get_width (surface);
  layout.height = meta_wayland_surface_get_height (surface);
  if (!group || !layout.width || !layout.height ||
      !layer_layout_geometry (layout, bounds, &geometry))
    {
      unmap (self);
      return;
    }

  if (clutter_actor_get_parent (actor) != group)
    {
      if (clutter_actor_get_parent (actor))
        clutter_actor_remove_child (clutter_actor_get_parent (actor), actor);
      clutter_actor_add_child (group, actor);
    }
  clutter_actor_set_position (actor, geometry.x, geometry.y);
  clutter_actor_set_size (actor, geometry.width, geometry.height);
  clutter_actor_set_reactive (actor, FALSE);
  clutter_actor_show (actor);
  self->mapped = TRUE;
}

static void
commit_state (MetaWaylandSurfaceRole *role,
              MetaWaylandTransaction *transaction,
              MetaWaylandSurfaceState *pending)
{
  MetaWaylandLayerSurface *self = (MetaWaylandLayerSurface *) role;

  if (!self->resource || self->closed)
    return;
  pending->has_layer_state = TRUE;
  pending->layer_layout = self->requested;
  pending->layer_index = self->layer;
  pending->layer_keyboard = self->keyboard;
  pending->layer_acked = self->acked;
}

static void
apply_state (MetaWaylandSurfaceRole *role, MetaWaylandSurfaceState *pending)
{
  MetaWaylandLayerSurface *self = (MetaWaylandLayerSurface *) role;
  MetaWaylandSurface *surface = get_surface (self);
  LayerRect bounds, geometry;
  gboolean has_buffer = meta_wayland_surface_get_buffer (surface) != NULL;

  META_WAYLAND_SURFACE_ROLE_CLASS (meta_wayland_layer_surface_parent_class)->apply_state (role, pending);
  if (!self->resource || self->closed || !pending->has_layer_state)
    return;

  if (pending->newly_attached && !has_buffer && self->mapped)
    {
      unmap (self);
      self->acked = FALSE;
      self->configured_width = self->configured_height = 0;
      g_array_set_size (self->serials, 0);
      self->requested = (LayerLayout) { 0 };
      self->layer = self->initial_layer;
      self->keyboard = 0;
      return;
    }
  if (has_buffer && !pending->layer_acked)
    {
      wl_resource_post_error (self->resource, ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_SURFACE_STATE,
                              "Buffer committed before configure acknowledgement");
      return;
    }
  if (!get_bounds (self, &bounds))
    {
      close_surface (self);
      return;
    }
  if (!layer_layout_geometry (pending->layer_layout, bounds, &geometry))
    {
      wl_resource_post_error (self->resource, ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_SIZE,
                              "Invalid layer surface size or anchors");
      return;
    }

  if (self->configured_width != geometry.width ||
      self->configured_height != geometry.height)
    {
      uint32_t serial = wl_display_next_serial (surface->compositor->wayland_display);
      self->configured_width = geometry.width;
      self->configured_height = geometry.height;
      g_array_append_val (self->serials, serial);
      zwlr_layer_surface_v1_send_configure (self->resource, serial,
                                            geometry.width, geometry.height);
    }

  if (has_buffer)
    render (self, pending->layer_layout, bounds, pending->layer_index);
}

static MetaWaylandSurface *
get_toplevel (MetaWaylandSurfaceRole *role)
{
  return meta_wayland_surface_role_get_surface (role);
}

static int
get_geometry_scale (MetaWaylandActorSurface *surface)
{
  return 1;
}

static void
destroy_resource (struct wl_resource *resource)
{
  MetaWaylandLayerSurface *self = wl_resource_get_user_data (resource);
  self->resource = NULL;
  close_surface (self);
}

static void
dispose (GObject *object)
{
  MetaWaylandLayerSurface *self = (MetaWaylandLayerSurface *) object;
  if (self->resource)
    wl_resource_destroy (self->resource);
  unmap (self);
  if (self->output)
    g_clear_signal_handler (&self->output_destroy_id, self->output);
  g_clear_object (&self->output);
  g_clear_pointer (&self->serials, g_array_unref);
  G_OBJECT_CLASS (meta_wayland_layer_surface_parent_class)->dispose (object);
}

static void
meta_wayland_layer_surface_init (MetaWaylandLayerSurface *self)
{
  self->serials = g_array_new (FALSE, FALSE, sizeof (uint32_t));
}

static void
meta_wayland_layer_surface_class_init (MetaWaylandLayerSurfaceClass *klass)
{
  MetaWaylandSurfaceRoleClass *role_class = META_WAYLAND_SURFACE_ROLE_CLASS (klass);
  G_OBJECT_CLASS (klass)->dispose = dispose;
  role_class->commit_state = commit_state;
  role_class->apply_state = apply_state;
  role_class->get_toplevel = get_toplevel;
  META_WAYLAND_ACTOR_SURFACE_CLASS (klass)->get_geometry_scale = get_geometry_scale;
}

static void
set_size (struct wl_client *client, struct wl_resource *resource,
          uint32_t width, uint32_t height)
{
  MetaWaylandLayerSurface *self = wl_resource_get_user_data (resource);
  self->requested.width = width;
  self->requested.height = height;
}

static void
set_anchor (struct wl_client *client, struct wl_resource *resource, uint32_t anchor)
{
  MetaWaylandLayerSurface *self = wl_resource_get_user_data (resource);
  if (anchor & ~15u)
    {
      wl_resource_post_error (resource, ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_ANCHOR,
                              "Invalid anchor bitfield");
      return;
    }
  self->requested.anchor = anchor;
}

static void
set_exclusive_zone (struct wl_client *client, struct wl_resource *resource, int32_t zone)
{
  MetaWaylandLayerSurface *self = wl_resource_get_user_data (resource);
  self->requested.zone = zone;
}

static void
set_margin (struct wl_client *client, struct wl_resource *resource,
            int32_t top, int32_t right, int32_t bottom, int32_t left)
{
  MetaWaylandLayerSurface *self = wl_resource_get_user_data (resource);
  self->requested.top = top;
  self->requested.right = right;
  self->requested.bottom = bottom;
  self->requested.left = left;
}

static void
set_keyboard (struct wl_client *client, struct wl_resource *resource, uint32_t keyboard)
{
  MetaWaylandLayerSurface *self = wl_resource_get_user_data (resource);
  if (keyboard > (wl_resource_get_version (resource) >= 4 ? 2u : 1u))
    {
      wl_resource_post_error (resource, ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_KEYBOARD_INTERACTIVITY,
                              "Invalid keyboard interactivity");
      return;
    }
  self->keyboard = keyboard;
}

static void
get_popup (struct wl_client *client, struct wl_resource *resource, struct wl_resource *popup)
{
  wl_resource_post_error (resource, ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_SURFACE_STATE,
                          "Layer popup parenting is not implemented");
}

static void
ack_configure (struct wl_client *client, struct wl_resource *resource, uint32_t serial)
{
  MetaWaylandLayerSurface *self = wl_resource_get_user_data (resource);
  if (self->closed)
    return;
  for (unsigned int i = 0; i < self->serials->len; i++)
    if (g_array_index (self->serials, uint32_t, i) == serial)
      {
        g_array_remove_range (self->serials, 0, i + 1);
        self->acked = TRUE;
        return;
      }
  wl_resource_post_error (resource, ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_SURFACE_STATE,
                          "Unknown configure serial");
}

static void
destroy (struct wl_client *client, struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
set_layer (struct wl_client *client, struct wl_resource *resource, uint32_t layer)
{
  MetaWaylandLayerSurface *self = wl_resource_get_user_data (resource);
  if (layer > 3)
    {
      wl_resource_post_error (resource, ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_SURFACE_STATE,
                              "Invalid layer");
      return;
    }
  self->layer = layer;
}

static const struct zwlr_layer_surface_v1_interface layer_surface_impl = {
  .set_size = set_size,
  .set_anchor = set_anchor,
  .set_exclusive_zone = set_exclusive_zone,
  .set_margin = set_margin,
  .set_keyboard_interactivity = set_keyboard,
  .get_popup = get_popup,
  .ack_configure = ack_configure,
  .destroy = destroy,
  .set_layer = set_layer,
};

static void
get_layer_surface (struct wl_client *client, struct wl_resource *resource,
                   uint32_t id, struct wl_resource *surface_resource,
                   struct wl_resource *output_resource, uint32_t layer,
                   const char *namespace)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);
  MetaWaylandLayerSurface *self;
  MetaWaylandOutput *output = output_resource ? wl_resource_get_user_data (output_resource) : NULL;

  if (layer > 3)
    {
      wl_resource_post_error (resource, ZWLR_LAYER_SHELL_V1_ERROR_INVALID_LAYER, "Invalid layer");
      return;
    }
  if (!surface || surface->role)
    {
      wl_resource_post_error (resource, ZWLR_LAYER_SHELL_V1_ERROR_ROLE, "Surface already has a role");
      return;
    }
  if (meta_wayland_surface_get_buffer (surface) ||
      surface->committed_state.texture ||
      meta_wayland_surface_get_pending_state (surface)->buffer)
    {
      wl_resource_post_error (resource, ZWLR_LAYER_SHELL_V1_ERROR_ALREADY_CONSTRUCTED, "Surface already has a buffer");
      return;
    }
  if (!meta_wayland_surface_assign_role (surface, meta_wayland_layer_surface_get_type (), NULL))
    {
      wl_resource_post_error (resource, ZWLR_LAYER_SHELL_V1_ERROR_ROLE, "Cannot assign layer role");
      return;
    }
  self = (MetaWaylandLayerSurface *) surface->role;
  self->initial_layer = self->layer = layer;
  self->resource = wl_resource_create (client, &zwlr_layer_surface_v1_interface,
                                       wl_resource_get_version (resource), id);
  if (!self->resource)
    {
      wl_client_post_no_memory (client);
      return;
    }
  wl_resource_set_implementation (self->resource, &layer_surface_impl, self, destroy_resource);
  if (output)
    {
      self->output = g_object_ref (output);
      self->output_destroy_id = g_signal_connect (output, "output-destroyed",
                                                  G_CALLBACK (on_output_destroyed), self);
    }
  else if (output_resource)
    close_surface (self);
}

static const struct zwlr_layer_shell_v1_interface layer_shell_impl = {
  .get_layer_surface = get_layer_surface,
  .destroy = destroy,
};

static void
bind_layer_shell (struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
  struct wl_resource *resource = wl_resource_create (client, &zwlr_layer_shell_v1_interface, version, id);
  if (!resource)
    {
      wl_client_post_no_memory (client);
      return;
    }
  wl_resource_set_implementation (resource, &layer_shell_impl, data, NULL);
}

void
meta_wayland_init_layer_shell (MetaWaylandCompositor *compositor)
{
  if (!wl_global_create (compositor->wayland_display, &zwlr_layer_shell_v1_interface,
                         4, compositor, bind_layer_shell))
    g_warning ("Could not create layer-shell global");
}
