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

## Phase 3: visual frame preview

Set `SOOKIE_EXPERIMENTAL_SSD_PREVIEW=1` before starting Sookie to draw a
non-reactive frame around floating normal Wayland windows. It is a separate
visual debugging flag, not server-side negotiation. Clients retain their
existing decorations, so a client titlebar may appear twice. The preview
uses the frame helper, draws title text and capability-dependent button
glyphs, changes color with focus, and follows window movement and sizing.
It hides for fullscreen, maximized, and side-tiled windows. Native input,
work-area constraints, actual frame extents, and configure/ack integration
are not changed. The buttons are intentionally not interactive. Because the
preview sits outside the current client geometry, placement near output
edges and overlap with client shadows may not be correct. Disable it for
normal desktop use. No server-side mode is advertised yet.

## Phase 4a: interactive preview

Set `SOOKIE_EXPERIMENTAL_SSD_INPUT=1` to enable pointer hits on the frame
preview. The core event filter routes primary presses on compositor-owned
frame edges to MetaWindow move, resize, close, minimize and maximize.
Existing compositor and client pointer grabs take precedence. Release
routing is unchanged. Buttons currently activate on press. Touch, hover
cursors, double-click and restore are not implemented. Client decorations
and actual window/work-area geometry remain unchanged; this is not yet
negotiated SSD. Use this flag only for testing.

## Phase 4b: negotiated SSD (experimental)

Set `SOOKIE_EXPERIMENTAL_SSD=1` to advertise the decoration global and
select server-side mode for clients that request it or have no preference.
Explicit CSD preferences remain CSD. Do not combine with the preview flags.

Configure records retain outer dimensions internally and snapshot logical
SSD extents with their serial. The client receives content dimensions;
the acknowledged buffer commit applies the corresponding extents. The
window actor includes the frame in its geometry and offsets the client
surface container. Maximize/tile retains a titlebar, fullscreen removes it.
Native controls are interactive and maximize toggles restore. Popup parent
origins account for applied SSD. This is an initial integration: validate
real clients, mode changes, fractional scaling, popup reposition, focus,
fullscreen, and work-area behavior before enabling by default.

## Negotiated SSD lifecycle regression test

The `wayland-native-ssd` headless test uses the repository's real Wayland
client/server test harness and exercises map, maximize, fullscreen, restore,
CSD fallback and renegotiation back to SSD. It checks the applied outer and
content rectangles and the configure-derived extents on the compositor side.
Run with `meson test -C build wayland-native-ssd --print-errorlogs` in a
working headless test environment. The normal CI compile includes this test;
its runtime is separate from geometry-only unit tests until the headless
runner environment is validated.

Window-control buttons now activate on a matching primary release over the
original button. Moving away, destroying the window, or encountering a shell
grab cancels activation. Press state is per pointer sprite and holds only a
weak reference to the frame actor. Earlier preview-only press activation is
superseded by this behavior. Touch and resize cursors remain separate work.

## Input polish and CI runtime validation

Native resize regions now own directional cursors, including corners, and
titlebar double-click uses Clutter's configured click interval/distance.
A double-click is accepted only for the same window and unchanged frame
position. The lifecycle client waits for frame callbacks before reporting
state; the server no longer assumes a configure is still pending. CI runs
the headless lifecycle test in a private D-Bus session with software GL.
That experimental runtime step is non-gating until the CI environment and
behavior are confirmed; inspect its result separately from build success.
Touch input, fractional scale, popup/grab testing and visual theme polish
remain to be validated or implemented before enabling SSD by default.
