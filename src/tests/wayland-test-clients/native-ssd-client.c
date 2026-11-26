/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "config.h"
#include <glib.h>
#include <string.h>
#include <unistd.h>
#include "wayland-test-client-utils.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"

static struct zxdg_decoration_manager_v1 *manager;
static struct wl_surface *surface;
static uint32_t mode;
static uint32_t expected_mode = ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
static int width = 200, height = 120;
static unsigned int configured, painted;
static gboolean maximized, fullscreen;

static void
registry_global (void *data, struct wl_registry *registry, uint32_t name,
                 const char *interface, uint32_t version)
{
  if (strcmp (interface, zxdg_decoration_manager_v1_interface.name) == 0)
    manager = wl_registry_bind (registry, name, &zxdg_decoration_manager_v1_interface, 1);
}

static void registry_remove (void *data, struct wl_registry *registry, uint32_t name) {}
static const struct wl_registry_listener registry_listener = {
  .global = registry_global, .global_remove = registry_remove,
};

static void
decoration_configure (void *data, struct zxdg_toplevel_decoration_v1 *decoration,
                       uint32_t value)
{
  mode = value;
}
static const struct zxdg_toplevel_decoration_v1_listener decoration_listener = {
  .configure = decoration_configure,
};

static void
toplevel_configure (void *data, struct xdg_toplevel *toplevel,
                    int32_t w, int32_t h, struct wl_array *states)
{
  uint32_t *state;
  maximized = fullscreen = FALSE;
  wl_array_for_each (state, states)
    {
      if (*state == XDG_TOPLEVEL_STATE_MAXIMIZED) maximized = TRUE;
      if (*state == XDG_TOPLEVEL_STATE_FULLSCREEN) fullscreen = TRUE;
    }
  width = w > 0 ? w : 200;
  height = h > 0 ? h : 120;
}

static void toplevel_close (void *data, struct xdg_toplevel *toplevel) { g_assert_not_reached (); }
static void toplevel_bounds (void *data, struct xdg_toplevel *toplevel, int32_t w, int32_t h) {}
static void toplevel_capabilities (void *data, struct xdg_toplevel *toplevel, struct wl_array *caps) {}
static const struct xdg_toplevel_listener toplevel_listener = {
  .configure = toplevel_configure, .close = toplevel_close,
  .configure_bounds = toplevel_bounds, .wm_capabilities = toplevel_capabilities,
};

static void
frame_done (void *data, struct wl_callback *callback, uint32_t time)
{
  painted++;
  wl_callback_destroy (callback);
}
static const struct wl_callback_listener frame_listener = { .done = frame_done };

static void
surface_configure (void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
  WaylandDisplay *display = data;
  g_assert_cmpuint (mode, !=, 0);
  xdg_surface_ack_configure (xdg_surface, serial);
  draw_surface (display, surface, width, height, 0xff365678);
  xdg_surface_set_window_geometry (xdg_surface, 0, 0, width, height);
  struct wl_callback *callback = wl_surface_frame (surface);
  wl_callback_add_listener (callback, &frame_listener, NULL);
  wl_surface_commit (surface);
  configured++;
}
static const struct xdg_surface_listener surface_listener = {
  .configure = surface_configure,
};

static void
sync_stage (WaylandDisplay *display, unsigned int stage)
{
  g_assert_cmpuint (mode, ==, expected_mode);
  unsigned int target = configured;
  while (painted < target)
    wayland_display_dispatch (display);
  g_assert_cmpint (wl_display_roundtrip (display->display), >=, 0);
  test_driver_sync_point (display->test_driver, stage, NULL);
  wait_for_sync_event (display, stage);
}

int
main (int argc, char **argv)
{
  alarm (45);
  g_autoptr (WaylandDisplay) display =
    wayland_display_new (WAYLAND_DISPLAY_CAPABILITY_TEST_DRIVER |
                         WAYLAND_DISPLAY_CAPABILITY_XDG_SHELL_V6);
  struct wl_registry *registry = wl_display_get_registry (display->display);
  wl_registry_add_listener (registry, &registry_listener, NULL);
  g_assert_cmpint (wl_display_roundtrip (display->display), >=, 0);
  g_assert_nonnull (manager);

  surface = wl_compositor_create_surface (display->compositor);
  struct xdg_surface *xdg_surface = xdg_wm_base_get_xdg_surface (display->xdg_wm_base, surface);
  xdg_surface_add_listener (xdg_surface, &surface_listener, display);
  struct xdg_toplevel *toplevel = xdg_surface_get_toplevel (xdg_surface);
  xdg_toplevel_add_listener (toplevel, &toplevel_listener, NULL);
  xdg_toplevel_set_title (toplevel, "native-ssd-test");

  struct zxdg_toplevel_decoration_v1 *decoration =
    zxdg_decoration_manager_v1_get_toplevel_decoration (manager, toplevel);
  zxdg_toplevel_decoration_v1_add_listener (decoration, &decoration_listener, NULL);
  zxdg_toplevel_decoration_v1_set_mode (decoration, expected_mode);
  zxdg_decoration_manager_v1_destroy (manager);
  wl_surface_commit (surface);

  while (!configured)
    wayland_display_dispatch (display);
  wait_for_window_shown (display, surface);
  sync_stage (display, 0);

  xdg_toplevel_set_maximized (toplevel);
  while (!maximized)
    wayland_display_dispatch (display);
  sync_stage (display, 1);

  xdg_toplevel_set_fullscreen (toplevel, NULL);
  while (!fullscreen)
    wayland_display_dispatch (display);
  sync_stage (display, 2);

  xdg_toplevel_unset_fullscreen (toplevel);
  xdg_toplevel_unset_maximized (toplevel);
  while (fullscreen || maximized)
    wayland_display_dispatch (display);
  sync_stage (display, 3);

  expected_mode = ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
  zxdg_toplevel_decoration_v1_set_mode (decoration, expected_mode);
  while (mode != expected_mode)
    wayland_display_dispatch (display);
  sync_stage (display, 4);

  expected_mode = ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
  zxdg_toplevel_decoration_v1_set_mode (decoration, expected_mode);
  while (mode != expected_mode)
    wayland_display_dispatch (display);
  sync_stage (display, 5);

  zxdg_toplevel_decoration_v1_destroy (decoration);
  xdg_toplevel_destroy (toplevel);
  xdg_surface_destroy (xdg_surface);
  wl_surface_destroy (surface);
  wl_registry_destroy (registry);
  g_assert_cmpint (wl_display_roundtrip (display->display), >=, 0);
  return EXIT_SUCCESS;
}
