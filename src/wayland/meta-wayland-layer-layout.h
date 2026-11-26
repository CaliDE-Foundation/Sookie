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

/* Reserve an edge of the remaining monitor area. The band spans the original
 * monitor's perpendicular axis so the core work-area solver sees a complete
 * edge, even when horizontal and vertical panels coexist. Leave at least one
 * logical pixel for other surfaces. The zone already includes the margin. */
static inline bool
layer_layout_reserve (LayerLayout s, LayerRect full, LayerRect *usable,
                      LayerRect *band)
{
  uint32_t edge = layer_layout_exclusive_edge (s);
  if (!edge || usable->width <= 0 || usable->height <= 0)
    return false;
  int32_t extent = (edge == LAYER_TOP || edge == LAYER_BOTTOM)
    ? usable->height : usable->width;
  int32_t n = s.zone < extent ? s.zone : extent - 1;
  if (n <= 0) return false;
  LayerRect r = full;
  switch (edge) {
    case LAYER_TOP:
      r.y = usable->y; r.height = n;
      usable->y += n; usable->height -= n;
      break;
    case LAYER_BOTTOM:
      r.y = usable->y + usable->height - n; r.height = n;
      usable->height -= n;
      break;
    case LAYER_LEFT:
      r.x = usable->x; r.width = n;
      usable->x += n; usable->width -= n;
      break;
    case LAYER_RIGHT:
      r.x = usable->x + usable->width - n; r.width = n;
      usable->width -= n;
      break;
    default: return false;
  }
  *band = r;
  return true;
}

/* Popup axes use -1 for the leading edge, +1 for the trailing edge.
 * Flip is tried first; slide and resize are applied only when requested. */
static inline int64_t
layer_popup_origin (int64_t start, int64_t span, int64_t size,
                    int anchor, int gravity, int64_t offset)
{
  return start + (anchor < 0 ? 0 : anchor > 0 ? span : span / 2)
    - (gravity < 0 ? size : gravity > 0 ? 0 : size / 2) + offset;
}

static inline bool
layer_popup_axis (int32_t start, int32_t span, int32_t size,
                  int anchor, int gravity, int32_t offset,
                  int64_t lo, int64_t hi, bool flip, bool slide, bool resize,
                  int32_t *position, int32_t *length)
{
  if (span <= 0 || size <= 0 || hi <= lo) return false;
  int64_t n = size, p = layer_popup_origin (start, span, n, anchor, gravity, offset);
  if (flip && (p < lo || p + n > hi)) {
    int64_t q = layer_popup_origin (start, span, n, -anchor, -gravity, offset);
    if (q >= lo && q + n <= hi) p = q;
  }
  if (slide) {
    if (p + n > hi) p = hi - n;
    if (p < lo) p = lo;
  }
  if (resize) {
    if (p < lo) { n -= lo - p; p = lo; }
    if (p + n > hi) n = hi - p;
  }
  if (n <= 0 || n > INT32_MAX || p < INT32_MIN || p > INT32_MAX ||
      p+n > INT32_MAX) return false;
  *position = p; *length = n;
  return true;
}
