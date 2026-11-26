/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "wayland/meta-wayland-types.h"
#include <glib-object.h>

GType meta_wayland_layer_surface_get_type (void);

gboolean meta_wayland_layer_surface_is_toplevel (MetaWaylandSurface *surface);

/* Experimental implementation; do not call until focus, work areas,
 * shell/lock integration and integration tests are ready. */
void meta_wayland_init_layer_shell (MetaWaylandCompositor *compositor);
