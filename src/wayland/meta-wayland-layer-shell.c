/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "config.h"

#include "wayland/meta-wayland-layer-shell.h"

#include "backends/meta-logical-monitor-private.h"
#include "backends/meta-monitor-manager-private.h"
#include "meta/meta-backend.h"
#include "backends/meta-monitor-private.h"
#include "meta/compositor.h"
#include "core/workspace-private.h"
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
  gulong surface_destroy_id;
  ClutterActor *container;
  MetaDisplay *registered_display;
  uint64_t generation;
  gboolean committed_buffer;
  gboolean has_applied_layout;
  LayerLayout applied_layout;
  uint32_t applied_layer;
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

gboolean
meta_wayland_layer_surface_is_toplevel (MetaWaylandSurface *surface)
{
  return surface && surface->role &&
    G_TYPE_CHECK_INSTANCE_TYPE (surface->role, meta_wayland_layer_surface_get_type ());
}

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

typedef struct
{
  GList *surfaces; /* weak: resource destructor unregisters each surface */
  gboolean arranging;
} LayerRegistry;

static void arrange_layers (MetaDisplay *display);

static void
free_registry (gpointer data)
{
  LayerRegistry *registry = data;
  g_list_free (registry->surfaces);
  g_free (registry);
}

static LayerRegistry *
get_registry (MetaDisplay *display)
{
  LayerRegistry *registry = g_object_get_data (G_OBJECT (display),
                                               "sookie-layer-registry");
  if (!registry)
    {
      registry = g_new0 (LayerRegistry, 1);
      g_object_set_data_full (G_OBJECT (display), "sookie-layer-registry",
                              registry, free_registry);
    }
  return registry;
}

static void
register_surface (MetaWaylandLayerSurface *self)
{
  MetaDisplay *display = get_display (self);
  if (!display)
    return;
  self->registered_display = display;
  g_object_add_weak_pointer (G_OBJECT (display),
                             (gpointer *) &self->registered_display);
  LayerRegistry *registry = get_registry (display);
  registry->surfaces = g_list_append (registry->surfaces, self);
}

static void
unregister_surface (MetaWaylandLayerSurface *self)
{
  MetaDisplay *display = self->registered_display;
  if (!display)
    return;
  LayerRegistry *registry = get_registry (display);
  registry->surfaces = g_list_remove (registry->surfaces, self);
  g_object_remove_weak_pointer (G_OBJECT (display),
                                (gpointer *) &self->registered_display);
  self->registered_display = NULL;
  arrange_layers (display);
}

static void
unmap (MetaWaylandLayerSurface *self)
{
  ClutterActor *actor = get_actor (self);

  if (actor)
    clutter_actor_hide (actor);
  if (self->container)
    {
      clutter_actor_hide (self->container);
      if (clutter_actor_get_parent (self->container))
        clutter_actor_remove_child (clutter_actor_get_parent (self->container),
                                    self->container);
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
  self->has_applied_layout = FALSE;
  if (self->registered_display)
    arrange_layers (self->registered_display);
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
      MetaContext *context = meta_display_get_context (display);
      MetaMonitorManager *manager =
        meta_backend_get_monitor_manager (meta_context_get_backend (context));
      MetaLogicalMonitor *logical = meta_wayland_surface_get_main_monitor (get_surface (self));

      if (!logical)
        logical = meta_monitor_manager_get_logical_monitor_from_number (
          manager, meta_display_get_current_monitor (display));
      if (!logical)
        logical = meta_monitor_manager_get_primary_logical_monitor (manager);
      if (!logical)
        return FALSE;
      rect = meta_logical_monitor_get_layout (logical);
      meta_wayland_surface_set_main_monitor (get_surface (self), logical);
    }
  meta_wayland_surface_notify_preferred_scale_monitor (get_surface (self));
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

/* Flatten the subsurface tree in protocol stacking order, like Mutter's
 * managed-window surface container. Keep every actor non-reactive until
 * the shell can enforce input and lock policy. */
static gboolean
collect_surface_actor (GNode *node, gpointer data)
{
  GPtrArray *actors = data;
  MetaWaylandSurface *surface = node->data;
  MetaSurfaceActor *actor = meta_wayland_surface_get_actor (surface);

  if (actor)
    g_ptr_array_add (actors, actor);
  return FALSE;
}

static void
rebuild_surface_tree (MetaWaylandLayerSurface *self)
{
  MetaWaylandSurface *surface = get_surface (self);
  g_autoptr (GPtrArray) actors = g_ptr_array_new ();
  g_autoptr (GList) children = NULL;

  if (!self->container)
    return;
  g_node_traverse (surface->applied_state.subsurface_branch_node,
                   G_IN_ORDER, G_TRAVERSE_LEAVES, -1,
                   collect_surface_actor, actors);
  children = clutter_actor_get_children (self->container);
  for (GList *l = children; l; l = l->next)
    if (!g_ptr_array_find (actors, l->data, NULL))
      clutter_actor_remove_child (self->container, l->data);
  for (unsigned int i = 0; i < actors->len; i++)
    {
      ClutterActor *actor = g_ptr_array_index (actors, i);
      ClutterActor *parent = clutter_actor_get_parent (actor);
      if (parent != self->container)
        {
          if (parent)
            clutter_actor_remove_child (parent, actor);
          clutter_actor_add_child (self->container, actor);
        }
      clutter_actor_set_child_at_index (self->container, actor, i);
      clutter_actor_set_reactive (actor, FALSE);
    }
}

static void
notify_subsurface_changed (MetaWaylandSurfaceRole *role)
{
  rebuild_surface_tree ((MetaWaylandLayerSurface *) role);
}

static void
render (MetaWaylandLayerSurface *self,
        LayerLayout layout,
        LayerRect bounds,
        LayerRect output_bounds,
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

  if (!self->container)
    {
      self->container = g_object_ref_sink (clutter_actor_new ());
      clutter_actor_set_reactive (self->container, FALSE);
    }
  if (clutter_actor_get_parent (self->container) != group)
    {
      if (clutter_actor_get_parent (self->container))
        clutter_actor_remove_child (clutter_actor_get_parent (self->container),
                                    self->container);
      clutter_actor_add_child (group, self->container);
    }
  rebuild_surface_tree (self);
  clutter_actor_set_position (self->container, geometry.x, geometry.y);
  clutter_actor_set_size (self->container, geometry.width, geometry.height);
  /* Limit the entire tree to its chosen output, including subsurfaces. */
  clutter_actor_set_clip (self->container,
                          (float) ((int64_t) output_bounds.x - geometry.x),
                          (float) ((int64_t) output_bounds.y - geometry.y),
                          output_bounds.width, output_bounds.height);
  clutter_actor_set_position (actor, 0, 0);
  clutter_actor_set_size (actor, geometry.width, geometry.height);
  clutter_actor_set_reactive (actor, FALSE);
  clutter_actor_show (actor);
  clutter_actor_show (self->container);
  self->mapped = TRUE;
}

typedef struct
{
  LayerRect full;
  LayerRect usable;
} LayerMonitorArea;

static void
send_configure (MetaWaylandLayerSurface *self, LayerRect geometry)
{
  MetaWaylandSurface *surface = get_surface (self);
  if (self->configured_width == geometry.width &&
      self->configured_height == geometry.height)
    return;
  if (self->serials->len >= 256)
    {
      close_surface (self);
      return;
    }
  uint32_t serial = wl_display_next_serial (surface->compositor->wayland_display);
  self->configured_width = geometry.width;
  self->configured_height = geometry.height;
  g_array_append_val (self->serials, serial);
  zwlr_layer_surface_v1_send_configure (self->resource, serial,
                                        geometry.width, geometry.height);
}

static MetaSide
edge_to_side (uint32_t edge)
{
  switch (edge)
    {
    case LAYER_TOP: return META_SIDE_TOP;
    case LAYER_BOTTOM: return META_SIDE_BOTTOM;
    case LAYER_LEFT: return META_SIDE_LEFT;
    default: return META_SIDE_RIGHT;
    }
}

/* Arrange exclusive panels first, by layer priority then creation order.
 * Zero-zone surfaces avoid the resulting reserved area. Negative zones use
 * the whole output. Unmapped surfaces receive a configure but reserve nothing. */
static void
arrange_layers (MetaDisplay *display)
{
  LayerRegistry *registry = get_registry (display);
  g_autoptr (GHashTable) areas = NULL;
  GSList *struts = NULL;

  if (registry->arranging)
    return;
  registry->arranging = TRUE;
  areas = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
  for (int pass = 0; pass < 2; pass++)
    for (int layer = 3; layer >= 0; layer--)
      for (GList *l = registry->surfaces; l; l = l->next)
        {
          MetaWaylandLayerSurface *self = l->data;
          LayerRect full, bounds, geometry, band;
          LayerMonitorArea *area;
          MetaLogicalMonitor *monitor;
          LayerLayout layout;
          uint32_t edge;
          gboolean exclusive;

          if (!self->resource || self->closed || !self->has_applied_layout ||
              self->applied_layer != (uint32_t) layer)
            continue;
          layout = self->applied_layout;
          edge = layer_layout_exclusive_edge (layout);
          exclusive = edge && meta_wayland_surface_get_buffer (get_surface (self));
          if ((pass == 0) != exclusive)
            continue;
          if (!get_bounds (self, &full))
            {
              close_surface (self);
              continue;
            }
          monitor = meta_wayland_surface_get_main_monitor (get_surface (self));
          if (!monitor)
            continue;
          area = g_hash_table_lookup (areas, monitor);
          if (!area)
            {
              area = g_new0 (LayerMonitorArea, 1);
              area->full = area->usable = full;
              g_hash_table_insert (areas, monitor, area);
            }
          bounds = layout.zone < 0 ? area->full : area->usable;
          if (!layer_layout_geometry (layout, bounds, &geometry))
            {
              /* An output can shrink after a valid initial request.
               * Do not disconnect the client for a compositor-side change. */
              unmap (self);
              continue;
            }
          send_configure (self, geometry);
          if (self->closed)
            continue;
          if (meta_wayland_surface_get_buffer (get_surface (self)))
            render (self, layout, bounds, area->full, self->applied_layer);
          if (exclusive && self->mapped &&
              layer_layout_reserve (layout, area->full, &area->usable, &band))
            {
              MetaStrut *strut = g_new0 (MetaStrut, 1);
              strut->rect = (MtkRectangle) { band.x, band.y, band.width, band.height };
              strut->side = edge_to_side (edge);
              struts = g_slist_prepend (struts, strut);
            }
        }
  meta_workspace_set_layer_struts (display, struts);
  g_slist_free_full (struts, g_free);
  registry->arranging = FALSE;
}

static void
commit_state (MetaWaylandSurfaceRole *role,
              MetaWaylandTransaction *transaction,
              MetaWaylandSurfaceState *pending)
{
  MetaWaylandLayerSurface *self = (MetaWaylandLayerSurface *) role;

  if (!self->resource || self->closed)
    return;
  /* Reset at request/commit time, not when a delayed transaction applies:
   * otherwise a later request can be erased by an earlier buffer detach. */
  pending->has_layer_state = TRUE;
  pending->layer_reset = pending->newly_attached &&
    !pending->buffer && self->committed_buffer;
  if (pending->layer_reset)
    {
      self->generation++;
      self->acked = FALSE;
      self->configured_width = self->configured_height = 0;
      g_array_set_size (self->serials, 0);
      self->requested = (LayerLayout) { 0 };
      self->layer = self->initial_layer;
      self->keyboard = 0;
    }
  if (pending->newly_attached)
    self->committed_buffer = pending->buffer != NULL;

  pending->layer_layout = self->requested;
  pending->layer_index = self->layer;
  pending->layer_keyboard = self->keyboard;
  pending->layer_acked = self->acked;
  pending->layer_generation = self->generation;
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

  if (pending->layer_generation != self->generation || pending->layer_reset)
    {
      unmap (self);
      self->has_applied_layout = FALSE;
      if (self->registered_display)
        arrange_layers (self->registered_display);
      return;
    }
  if (!has_buffer)
    unmap (self);
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

  self->applied_layout = pending->layer_layout;
  self->applied_layer = pending->layer_index;
  self->has_applied_layout = TRUE;
  if (self->registered_display)
    arrange_layers (self->registered_display);
}

static MetaLogicalMonitor *
get_preferred_monitor (MetaWaylandSurfaceRole *role)
{
  return meta_wayland_surface_get_main_monitor (meta_wayland_surface_role_get_surface (role));
}

static void
on_monitors_changed (MetaMonitorManager *manager, MetaWaylandLayerSurface *self)
{
  if (self->registered_display)
    arrange_layers (self->registered_display);
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
  unregister_surface (self);
}

static void
on_surface_destroyed (MetaWaylandSurface *surface, MetaWaylandLayerSurface *self)
{
  if (self->resource)
    wl_resource_destroy (self->resource);
  else
    {
      unmap (self);
      unregister_surface (self);
    }
}

static void
dispose (GObject *object)
{
  MetaWaylandLayerSurface *self = (MetaWaylandLayerSurface *) object;
  if (self->resource)
    wl_resource_destroy (self->resource);
  unmap (self);
  unregister_surface (self);
  if (self->output)
    g_clear_signal_handler (&self->output_destroy_id, self->output);
  g_clear_object (&self->output);
  if (self->surface_destroy_id)
    g_clear_signal_handler (&self->surface_destroy_id, get_surface (self));
  if (self->container)
    {
      g_autoptr (GList) children = clutter_actor_get_children (self->container);
      for (GList *l = children; l; l = l->next)
        clutter_actor_remove_child (self->container, l->data);
      g_clear_object (&self->container);
    }
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
  role_class->get_preferred_scale_monitor = get_preferred_monitor;
  role_class->notify_subsurface_state_changed = notify_subsurface_changed;
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
  register_surface (self);
  self->surface_destroy_id = g_signal_connect (surface, "destroy",
                                              G_CALLBACK (on_surface_destroyed), self);
  MetaContext *context = meta_wayland_compositor_get_context (surface->compositor);
  MetaMonitorManager *manager =
    meta_backend_get_monitor_manager (meta_context_get_backend (context));
  g_signal_connect_object (manager, "monitors-changed",
                           G_CALLBACK (on_monitors_changed), self, 0);
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
