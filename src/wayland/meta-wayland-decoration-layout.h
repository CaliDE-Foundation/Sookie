/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

/* Logical coordinate geometry for future native Wayland SSD.
 * CSD shadow extents are separate and must never be reused as SSD extents.
 * Callers convert logical metrics to the geometry scale at the boundary. */
typedef struct
{
  int32_t x, y, width, height;
} SookieFrameRect;

typedef struct
{
  int32_t left, top, right, bottom;
} SookieFrameExtents;

typedef enum
{
  SOOKIE_FRAME_OUTSIDE,
  SOOKIE_FRAME_CONTENT,
  SOOKIE_FRAME_TITLE,
  SOOKIE_FRAME_CLOSE,
  SOOKIE_FRAME_MAXIMIZE,
  SOOKIE_FRAME_MINIMIZE,
  SOOKIE_FRAME_NORTH,
  SOOKIE_FRAME_SOUTH,
  SOOKIE_FRAME_WEST,
  SOOKIE_FRAME_EAST,
  SOOKIE_FRAME_NORTH_WEST,
  SOOKIE_FRAME_NORTH_EAST,
  SOOKIE_FRAME_SOUTH_WEST,
  SOOKIE_FRAME_SOUTH_EAST,
} SookieFrameHit;

enum
{
  SOOKIE_FRAME_TITLE_HEIGHT = 32,
  SOOKIE_FRAME_BORDER = 4,
  SOOKIE_FRAME_BUTTON_WIDTH = 32,
};

static inline SookieFrameExtents
sookie_frame_extents (bool server_side,
                      bool fullscreen,
                      bool maximized)
{
  if (!server_side || fullscreen)
    return (SookieFrameExtents) { 0 };
  if (maximized)
    return (SookieFrameExtents) { .top = SOOKIE_FRAME_TITLE_HEIGHT };

  return (SookieFrameExtents) {
    .left = SOOKIE_FRAME_BORDER,
    .right = SOOKIE_FRAME_BORDER,
    .top = SOOKIE_FRAME_TITLE_HEIGHT + SOOKIE_FRAME_BORDER,
    .bottom = SOOKIE_FRAME_BORDER,
  };
}

static inline bool
sookie_frame_rect_valid (int64_t x, int64_t y, int64_t w, int64_t h)
{
  return x >= INT32_MIN && x <= INT32_MAX &&
         y >= INT32_MIN && y <= INT32_MAX &&
         w > 0 && w <= INT32_MAX && h > 0 && h <= INT32_MAX &&
         x + w <= INT32_MAX && y + h <= INT32_MAX;
}

static inline bool
sookie_frame_convert (SookieFrameRect rect,
                      SookieFrameExtents e,
                      bool to_outer,
                      SookieFrameRect *out)
{
  if (!out || e.left < 0 || e.top < 0 || e.right < 0 || e.bottom < 0 ||
      !sookie_frame_rect_valid (rect.x, rect.y, rect.width, rect.height))
    return false;

  int64_t sign = to_outer ? 1 : -1;
  int64_t x = rect.x - sign * e.left;
  int64_t y = rect.y - sign * e.top;
  int64_t w = rect.width + sign * ((int64_t) e.left + e.right);
  int64_t h = rect.height + sign * ((int64_t) e.top + e.bottom);
  if (!sookie_frame_rect_valid (x, y, w, h))
    return false;

  *out = (SookieFrameRect) { x, y, w, h };
  return true;
}

/* Hit testing in outer-frame-local coordinates. Resize borders take
 * precedence, then buttons, then the drag region. Small windows omit
 * controls rather than overlap the client or each other. */
static inline SookieFrameHit
sookie_frame_hit_test (int32_t width, int32_t height,
                       SookieFrameExtents e,
                       int32_t x, int32_t y,
                       bool resizable, bool can_maximize, bool can_minimize)
{
  if (x < 0 || y < 0 || x >= width || y >= height ||
      e.left < 0 || e.right < 0 || e.top < 0 || e.bottom < 0 ||
      (int64_t) e.left + e.right >= width ||
      (int64_t) e.top + e.bottom >= height)
    return SOOKIE_FRAME_OUTSIDE;

  bool west = x < e.left;
  bool east = x >= width - e.right;
  bool north = y < e.top && y < SOOKIE_FRAME_BORDER && e.left > 0;
  bool south = y >= height - e.bottom;

  if (resizable)
    {
      if (north && west) return SOOKIE_FRAME_NORTH_WEST;
      if (north && east) return SOOKIE_FRAME_NORTH_EAST;
      if (south && west) return SOOKIE_FRAME_SOUTH_WEST;
      if (south && east) return SOOKIE_FRAME_SOUTH_EAST;
      if (north) return SOOKIE_FRAME_NORTH;
      if (south) return SOOKIE_FRAME_SOUTH;
      if (west) return SOOKIE_FRAME_WEST;
      if (east) return SOOKIE_FRAME_EAST;
    }
  if (west || east || south || north)
    return SOOKIE_FRAME_OUTSIDE;
  if (y >= e.top)
    return SOOKIE_FRAME_CONTENT;

  int32_t available = width - e.left - e.right;
  int32_t end = width - e.right;
  if (available >= SOOKIE_FRAME_BUTTON_WIDTH * 2)
    {
      end -= SOOKIE_FRAME_BUTTON_WIDTH;
      if (x >= end) return SOOKIE_FRAME_CLOSE;
      if (can_maximize && end - e.left >= SOOKIE_FRAME_BUTTON_WIDTH * 2)
        {
          end -= SOOKIE_FRAME_BUTTON_WIDTH;
          if (x >= end) return SOOKIE_FRAME_MAXIMIZE;
        }
      if (can_minimize && end - e.left >= SOOKIE_FRAME_BUTTON_WIDTH * 2)
        {
          end -= SOOKIE_FRAME_BUTTON_WIDTH;
          if (x >= end) return SOOKIE_FRAME_MINIMIZE;
        }
    }
  return SOOKIE_FRAME_TITLE;
}
