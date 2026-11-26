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
