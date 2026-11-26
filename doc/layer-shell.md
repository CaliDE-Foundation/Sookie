# Experimental layer shell

The global is intentionally not advertised. The vendored v4 XML generates
server/client bindings. Layer state and initial configure acknowledgement
are captured at commit and merged through the Wayland transaction system.
An unmap advances the generation so delayed old state cannot remap it.
Configure queues are bounded. The scene path uses four dedicated groups,
flattened subsurface trees, output clipping and non-reactive actors.
Monitor changes trigger relayout and configure events.

Still needed: compilation and nested-session integration validation,
exclusive-zone enforcement, pointer/keyboard focus, popup parenting,
Cali wallpaper/panel/lock stacking policy, and tests for delayed commits,
output removal and remapping. No claim of runtime compatibility yet.
