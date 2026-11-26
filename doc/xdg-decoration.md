# Experimental xdg-decoration v1

Enable with `SOOKIE_EXPERIMENTAL_XDG_DECORATION=1` before starting Sookie.
The global is disabled by default. The first phase negotiates client-side
decorations only, even if a client prefers server-side mode. It does not
implement compositor-owned window frames and must not be described as SSD
support.

## Test plan

- Global absent by default, advertised at version 1 when enabled.
- Client creates decoration before first buffer and receives client-side
  mode before its initial xdg_surface configure; normal configure ack path.
- Request server-side mode and unset mode after mapping; remain client-side.
- Duplicate decoration, invalid mode, early buffer attach and destroying
  the toplevel before its decoration are rejected.
- Destroy manager first; existing decorations continue working.
- Destroy client or unmap/remap without stale resources or crashes.

## Remaining implementation

Native Wayland frame drawing, non-client hit testing and frame extents must
be integrated with the compositor geometry, maximize/fullscreen, tiling,
popup coordinates and fractional scaling before selecting server-side mode.
Then validate protocol lifecycle, pending state and buffer manipulation
edge cases with dedicated integration tests. X11 frame handling remains
unchanged.

## Phase 2: frame geometry and negotiation tests

The standalone decoration layout helper defines logical outer/client rectangle
conversion, fullscreen/maximized extents, resize-edge and title-button hit
regions. Unit tests exercise negative monitor coordinates, overflow, tiny
windows, and the round trip. The metrics (32 logical px titlebar, 4 px border)
are provisional defaults, not a user-visible theme. No frame actors consume
these helpers yet. Repeated identical client preferences are ignored.

Build and run the geometry test with:

    meson test -C build decoration-layout --print-errorlogs

For an actual Wayland connection, run the built
`src/tests/wayland-test-clients/xdg-decoration-probe` executable inside the
nested session (with the experimental global enabled). It checks initial
configure ordering, client-side fallback, manager destruction, duplicate
preference and clean teardown. Pass `disabled` to check a session without
the opt-in global. This probe does not map a buffer or test actual SSD.
