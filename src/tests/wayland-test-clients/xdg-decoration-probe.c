/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-client.h>

#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"

static struct wl_compositor *compositor;
static struct xdg_wm_base *wm;
static struct zxdg_decoration_manager_v1 *manager;
static unsigned int decoration_events, surface_events;
static uint32_t mode;

static void
global (void *data, struct wl_registry *registry, uint32_t name,
        const char *interface, uint32_t version)
{
  if (strcmp (interface, wl_compositor_interface.name) == 0)
    compositor = wl_registry_bind (registry, name, &wl_compositor_interface, 1);
  else if (strcmp (interface, xdg_wm_base_interface.name) == 0)
    wm = wl_registry_bind (registry, name, &xdg_wm_base_interface, 1);
  else if (strcmp (interface, zxdg_decoration_manager_v1_interface.name) == 0)
    manager = wl_registry_bind (registry, name, &zxdg_decoration_manager_v1_interface, 1);
}

static void global_remove (void *data, struct wl_registry *registry, uint32_t name) {}
static const struct wl_registry_listener registry_listener = {
  .global = global, .global_remove = global_remove,
};
static void ping (void *data, struct xdg_wm_base *base, uint32_t serial)
{
  xdg_wm_base_pong (base, serial);
}
static const struct xdg_wm_base_listener wm_listener = { .ping = ping };

static void
decoration_configure (void *data, struct zxdg_toplevel_decoration_v1 *decoration,
                       uint32_t configured_mode)
{
  mode = configured_mode;
  decoration_events++;
}
static const struct zxdg_toplevel_decoration_v1_listener decoration_listener = {
  .configure = decoration_configure,
};

static void
surface_configure (void *data, struct xdg_surface *surface, uint32_t serial)
{
  assert (decoration_events > 0);
  assert (mode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
  xdg_surface_ack_configure (surface, serial);
  surface_events++;
}
static const struct xdg_surface_listener surface_listener = {
  .configure = surface_configure,
};
static void toplevel_configure (void *data, struct xdg_toplevel *toplevel,
                                int32_t width, int32_t height, struct wl_array *states) {}
static void toplevel_close (void *data, struct xdg_toplevel *toplevel) {}
static const struct xdg_toplevel_listener toplevel_listener = {
  .configure = toplevel_configure, .close = toplevel_close,
};

int
main (int argc, char **argv)
{
  alarm (15);
  struct wl_display *display = wl_display_connect (NULL);
  assert (display);
  struct wl_registry *registry = wl_display_get_registry (display);
  wl_registry_add_listener (registry, &registry_listener, NULL);
  assert (wl_display_roundtrip (display) >= 0);
  if (argc > 1 && strcmp (argv[1], "disabled") == 0)
    {
      assert (!manager);
      wl_display_disconnect (display);
      return EXIT_SUCCESS;
    }
  assert (compositor && wm && manager);
  xdg_wm_base_add_listener (wm, &wm_listener, NULL);

  struct wl_surface *surface = wl_compositor_create_surface (compositor);
  struct xdg_surface *xdg_surface = xdg_wm_base_get_xdg_surface (wm, surface);
  xdg_surface_add_listener (xdg_surface, &surface_listener, NULL);
  struct xdg_toplevel *toplevel = xdg_surface_get_toplevel (xdg_surface);
  xdg_toplevel_add_listener (toplevel, &toplevel_listener, NULL);
  struct zxdg_toplevel_decoration_v1 *decoration =
    zxdg_decoration_manager_v1_get_toplevel_decoration (manager, toplevel);
  zxdg_toplevel_decoration_v1_add_listener (decoration, &decoration_listener, NULL);
  zxdg_toplevel_decoration_v1_set_mode (decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
  /* Manager destruction must not invalidate existing decoration objects. */
  zxdg_decoration_manager_v1_destroy (manager);
  wl_surface_commit (surface);
  while (!surface_events)
    assert (wl_display_dispatch (display) >= 0);
  assert (decoration_events == 1);

  zxdg_toplevel_decoration_v1_set_mode (decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
  assert (wl_display_roundtrip (display) >= 0);
  assert (decoration_events == 1);

  zxdg_toplevel_decoration_v1_destroy (decoration);
  xdg_toplevel_destroy (toplevel);
  xdg_surface_destroy (xdg_surface);
  wl_surface_destroy (surface);
  xdg_wm_base_destroy (wm);
  wl_compositor_destroy (compositor);
  wl_registry_destroy (registry);
  assert (wl_display_roundtrip (display) >= 0);
  wl_display_disconnect (display);
  return EXIT_SUCCESS;
}
