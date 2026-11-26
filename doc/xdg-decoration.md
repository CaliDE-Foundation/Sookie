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
