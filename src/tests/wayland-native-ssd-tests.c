/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "config.h"
#include "backends/meta-backend-private.h"
#include "core/window-private.h"
#include "tests/meta-test-utils.h"
#include "tests/meta-wayland-test-runner.h"
#include "tests/meta-wayland-test-utils.h"

static void
test_drag_frame (MetaWindow *window)
{
  MetaBackend *backend = meta_context_get_backend (test_context);
  ClutterSeat *seat = meta_backend_get_default_seat (backend);
  g_autoptr (ClutterVirtualInputDevice) pointer = NULL;
  MtkRectangle before, during, after;
  int x, y;

  meta_wait_for_effects (window);
  meta_window_move_frame (window, TRUE, 100, 100);
  meta_wait_for_update (test_context);
  meta_window_get_frame_rect (window, &before);
  g_assert_cmpint (before.x, ==, 100);
  g_assert_cmpint (before.y, ==, 100);

  pointer = clutter_seat_create_virtual_device (seat, CLUTTER_POINTER_DEVICE);
  x = before.x + 30;
  y = before.y + 16;
  clutter_virtual_input_device_notify_absolute_motion (pointer, g_get_monotonic_time (), x, y);
  meta_flush_input (test_context);
  clutter_virtual_input_device_notify_button (pointer, g_get_monotonic_time (),
                                              CLUTTER_BUTTON_PRIMARY, CLUTTER_BUTTON_STATE_PRESSED);
  meta_flush_input (test_context);
  clutter_virtual_input_device_notify_absolute_motion (pointer, g_get_monotonic_time (), x + 80, y + 60);
  meta_flush_input (test_context);
  meta_wait_for_update (test_context);
  meta_window_get_frame_rect (window, &during);
  clutter_virtual_input_device_notify_button (pointer, g_get_monotonic_time (),
                                              CLUTTER_BUTTON_PRIMARY, CLUTTER_BUTTON_STATE_RELEASED);
  meta_flush_input (test_context);
  meta_wait_for_update (test_context);
  meta_window_get_frame_rect (window, &after);

  /* Catch both a stationary drag and a jump that happens only at release. */
  g_assert_cmpint (during.x, ==, before.x + 80);
  g_assert_cmpint (during.y, ==, before.y + 60);
  g_assert_cmpint (after.x, ==, during.x);
  g_assert_cmpint (after.y, ==, during.y);
  g_assert_cmpint (after.width, ==, before.width);
  g_assert_cmpint (after.height, ==, before.height);
}

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
      /* The test runner creates a 640x480 scale-1 virtual monitor. */
      const int scale = 1;

      meta_wayland_test_driver_wait_for_sync_point (test_driver, i);
      window = meta_find_client_window (test_context, "native-ssd-test");
      g_assert_nonnull (window);
      /* The client waits for its frame callback before signaling us.
       * Do not require a still-pending configure or block on a new one. */

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
      if (i == 0)
        test_drag_frame (window);
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
