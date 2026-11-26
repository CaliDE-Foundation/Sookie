/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "wayland/meta-wayland-types.h"
#include <glib-object.h>
#include "clutter/clutter.h"
#include "meta/types.h"

GType meta_wayland_layer_surface_get_type (void);

gboolean meta_wayland_layer_surface_is_toplevel (MetaWaylandSurface *surface);

/* Internal input policy. The keyboard selector is used only by the default
 * Wayland input handler, never to bypass a compositor grab. */
ClutterActor * meta_wayland_layer_surface_get_popup_parent (MetaWaylandSurface *surface);
gboolean meta_wayland_layer_surface_accepts_input (MetaWaylandSurface *surface);
MetaWaylandSurface * meta_wayland_layer_shell_get_keyboard_focus (MetaWaylandCompositor *compositor);
void meta_wayland_layer_shell_handle_press (MetaDisplay *display, ClutterActor *actor);
void meta_wayland_layer_shell_clear_demand_focus (MetaWaylandCompositor *compositor);

/* Experimental implementation; do not call until focus, work areas,
 * shell/lock integration and integration tests are ready. */
void meta_wayland_init_layer_shell (MetaWaylandCompositor *compositor);
