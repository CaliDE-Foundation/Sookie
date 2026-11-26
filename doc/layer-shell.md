# Experimental layer shell

The global is intentionally not advertised. The vendored v4 XML generates
server/client bindings. Layer state and initial configure acknowledgement
are captured at commit and merged through the Wayland transaction system.
An unmap advances the generation so delayed old state cannot remap it.
Configure queues are bounded. The scene path uses four dedicated groups,
flattened subsurface trees, output clipping and non-reactive actors.
Monitor changes trigger relayout and configure events.

Still needed: compilation and nested-session integration validation,
exclusive-zone integration validation, pointer/keyboard focus, popup parenting,
Cali wallpaper/panel/lock stacking policy, and tests for delayed commits,
output removal and remapping. No claim of runtime compatibility yet.

Exclusive zones now use a per-display layer registry and separate core
workspace struts. Existing shell and window struts are preserved. Mapped
positive-zone surfaces reserve space in layer/creation order; zero-zone
surfaces use the remaining area and negative-zone surfaces use the full
output. Reservations are removed on unmap/destruction and recomputed on
monitor changes. The geometry tests cover multiple edge reservations and
oversized zones. Integration with Cali's own panel placement remains
unverified, and the global is still disabled.

Input implementation: reactive layer/subsurface actors, windowless pointer
focus, exclusive keyboard selection for top/overlay, click-to-focus for
on-demand and lower exclusive surfaces, normal window fallback, and
stage grab/key-focus/window-group visibility checks. These changes have
not been exercised in a live session. Popup parenting is still pending:
Mutter's existing xdg_popup implementation requires a managed parent
window, whereas the layer renderer uses independent scene actors. The
global remains disabled pending popup support and lock/input testing.

## Experimental activation

Set SOOKIE_EXPERIMENTAL_LAYER_SHELL=1 in the environment of Sookie
before starting a nested development session. The global is off by
default. The windowless XDG popup implementation accepts external parent
assignment, nested popups, configure acknowledgements, output-relative
flip/slide/resize, reposition requests, parent dismissal and existing
Wayland popup grabs. This is not runtime-certified; reactive reposition,
fractional scaling, popup grabs and lock isolation require testing.
Do not use an experimental session for sensitive work.

### First live smoke test

Build nightly with the normal Meson development instructions. Start Sookie
in a separate nested session with SOOKIE_EXPERIMENTAL_LAYER_SHELL=1, and
run wayland-info against the nested WAYLAND_DISPLAY (not the host socket).
Check for zwlr_layer_shell_v1 version 4. Try a panel such as Waybar and a
launcher such as Fuzzel, inspect the reserved work area, open/dismiss a
context menu, and repeat after moving or removing an output. Finally test
Cali's lock with a disposable session and no sensitive credentials: layer
clients must not receive input or obscure the lock. Restart without the
environment variable and confirm the global is absent.
