/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include "../wayland/meta-wayland-decoration-layout.h"

int
main (void)
{
  SookieFrameRect client = { -800, 100, 800, 600 }, frame, roundtrip;
  SookieFrameExtents e = sookie_frame_extents (true, false, false);
  assert (sookie_frame_convert (client, e, true, &frame));
  assert (frame.x == -804 && frame.y == 64);
  assert (frame.width == 808 && frame.height == 640);
  assert (sookie_frame_convert (frame, e, false, &roundtrip));
  assert (roundtrip.x == client.x && roundtrip.y == client.y &&
          roundtrip.width == client.width && roundtrip.height == client.height);

  assert (sookie_frame_hit_test (808, 640, e, 0, 0, true, true, true) == SOOKIE_FRAME_NORTH_WEST);
  assert (sookie_frame_hit_test (808, 640, e, 807, 639, true, true, true) == SOOKIE_FRAME_SOUTH_EAST);
  assert (sookie_frame_hit_test (808, 640, e, 100, 20, true, true, true) == SOOKIE_FRAME_TITLE);
  assert (sookie_frame_hit_test (808, 640, e, 790, 20, true, true, true) == SOOKIE_FRAME_CLOSE);
  assert (sookie_frame_hit_test (808, 640, e, 750, 20, true, true, true) == SOOKIE_FRAME_MAXIMIZE);
  assert (sookie_frame_hit_test (808, 640, e, 720, 20, true, true, true) == SOOKIE_FRAME_MINIMIZE);
  assert (sookie_frame_hit_test (808, 640, e, 100, 36, true, true, true) == SOOKIE_FRAME_CONTENT);
  assert (sookie_frame_hit_test (808, 640, e, 808, 0, true, true, true) == SOOKIE_FRAME_OUTSIDE);
  assert (sookie_frame_hit_test (48, 80, e, 30, 20, true, true, true) == SOOKIE_FRAME_TITLE);
  assert (sookie_frame_hit_test (808, 640, e, 0, 0, false, true, true) == SOOKIE_FRAME_OUTSIDE);

  e = sookie_frame_extents (true, false, true);
  assert (e.left == 0 && e.top == 32 && e.right == 0 && e.bottom == 0);
  assert (sookie_frame_convert (client, e, true, &frame));
  assert (frame.width == 800 && frame.height == 632);
  assert (sookie_frame_hit_test (800, 632, e, 0, 0, true, true, true) == SOOKIE_FRAME_TITLE);
  e = sookie_frame_extents (true, true, false);
  assert (sookie_frame_hit_test (800, 600, e, 0, 0, true, true, true) == SOOKIE_FRAME_CONTENT);
  e = sookie_frame_extents (false, false, false);
  assert (sookie_frame_convert (client, e, true, &frame));
  assert (frame.x == client.x && frame.width == client.width);

  e = sookie_frame_extents (true, false, false);
  assert (!sookie_frame_convert ((SookieFrameRect) { INT32_MIN, 0, 1, 1 }, e, true, &frame));
  assert (!sookie_frame_convert ((SookieFrameRect) { INT32_MAX - 1, 0, 1, 1 }, e, true, &frame));
  assert (!sookie_frame_convert ((SookieFrameRect) { 0, 0, 8, 40 }, e, false, &frame));
  assert (!sookie_frame_convert (client, (SookieFrameExtents) { .left = -1 }, true, &frame));
  return 0;
}
