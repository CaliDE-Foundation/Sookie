/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

/* Pure logical-coordinate geometry; no Wayland global is registered here. */
enum { LAYER_TOP=1, LAYER_BOTTOM=2, LAYER_LEFT=4, LAYER_RIGHT=8 };
typedef struct { int32_t x,y,width,height; } LayerRect;
typedef struct {
  uint32_t anchor, width, height;
  int32_t top,bottom,left,right,zone;
} LayerLayout;

static inline bool
layer_axis (int32_t origin, int32_t extent, uint32_t requested,
            bool near, bool far, int32_t a, int32_t b,
            int32_t *position, int32_t *size)
{
  int64_t n = requested, p = origin;
  if (extent <= 0 || (!requested && !(near && far))) return false;
  if (!requested) n = (int64_t)extent-a-b;
  if (near && far) p += ((int64_t)extent-a-b-n)/2+a;
  else if (near) p += a;
  else if (far) p += (int64_t)extent-b-n;
  else p += ((int64_t)extent-n)/2;
  if (n <= 0 || n > INT32_MAX || p < INT32_MIN || p > INT32_MAX || p+n > INT32_MAX) return false;
  *position = p; *size = n;
  return true;
}

static inline bool
layer_layout_geometry (LayerLayout s, LayerRect bounds, LayerRect *out)
{
  LayerRect r;
  if ((s.anchor & ~15u) ||
      !layer_axis(bounds.x,bounds.width,s.width,s.anchor&LAYER_LEFT,s.anchor&LAYER_RIGHT,s.left,s.right,&r.x,&r.width) ||
      !layer_axis(bounds.y,bounds.height,s.height,s.anchor&LAYER_TOP,s.anchor&LAYER_BOTTOM,s.top,s.bottom,&r.y,&r.height)) return false;
  *out=r; return true;
}

/* v1-v4 implicit exclusive edge. Corners do not reserve an edge. */
static inline uint32_t
layer_layout_exclusive_edge (LayerLayout s)
{
  if (s.zone <= 0) return 0;
  switch (s.anchor) {
    case LAYER_TOP: case LAYER_TOP|LAYER_LEFT|LAYER_RIGHT: return LAYER_TOP;
    case LAYER_BOTTOM: case LAYER_BOTTOM|LAYER_LEFT|LAYER_RIGHT: return LAYER_BOTTOM;
    case LAYER_LEFT: case LAYER_LEFT|LAYER_TOP|LAYER_BOTTOM: return LAYER_LEFT;
    case LAYER_RIGHT: case LAYER_RIGHT|LAYER_TOP|LAYER_BOTTOM: return LAYER_RIGHT;
    default: return 0;
  }
}
