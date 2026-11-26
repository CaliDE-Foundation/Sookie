/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "config.h"
#include "core/window-private.h"
#include "tests/meta-test-utils.h"
#include "tests/meta-wayland-test-runner.h"
#include "tests/meta-wayland-test-utils.h"
#include "wayland/meta-window-wayland.h"

static void
test_native_ssd (void)
{
  MetaWaylandTestClient *client =
    meta_wayland_test_client_new (test_context, "native-ssd-client");

  for (unsigned int i = 0; i < 6; i++)
    {
      MetaWindow *window;
      MtkRectangle frame, content;
      gboolean expected_ssd = i != 4;
      int left = (i == 0 || i == 3 || i == 5) ? 4 : 0;
      int top = (i == 0 || i == 3 || i == 5) ? 36 : i == 1 ? 32 : 0;
      int scale;

      meta_wayland_test_driver_wait_for_sync_point (test_driver, i);
      window = meta_find_client_window (test_context, "native-ssd-test");
      g_assert_nonnull (window);
      meta_wait_wayland_window_reconfigure (window);
      scale = meta_window_wayland_get_geometry_scale (window);

      g_assert_cmpint (window->ssd_frame, ==, expected_ssd);
      g_assert_cmpint (window->ssd_frame_extents.left, ==, left * scale);
      g_assert_cmpint (window->ssd_frame_extents.right, ==, left * scale);
      g_assert_cmpint (window->ssd_frame_extents.top, ==, top * scale);
      g_assert_cmpint (window->ssd_frame_extents.bottom, ==, left * scale);
      meta_window_get_frame_rect (window, &frame);
      meta_window_get_client_content_rect (window, &content);
      g_assert_cmpint (content.x, ==, frame.x + left * scale);
      g_assert_cmpint (content.y, ==, frame.y + top * scale);
      g_assert_cmpint (frame.width - content.width, ==, 2 * left * scale);
      g_assert_cmpint (frame.height - content.height, ==, (top + left) * scale);
      g_assert_cmpint (content.width, >, 0);
      g_assert_cmpint (content.height, >, 0);
      meta_wayland_test_driver_emit_sync_event (test_driver, i);
    }
  meta_wayland_test_client_finish (client);
}

static void
init_tests (void)
{
  g_test_add_func ("/wayland/native-ssd/lifecycle", test_native_ssd);
}

int
main (int argc, char **argv)
{
  g_setenv ("SOOKIE_EXPERIMENTAL_SSD", "1", TRUE);
  return meta_run_wayland_tests (argc, argv, init_tests);
}
