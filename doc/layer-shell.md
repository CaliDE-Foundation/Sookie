# Experimental layer shell

This work is not advertised to clients yet. The vendored upstream v4 XML
is used to generate bindings. The layer role captures state on commit,
tracks configure acknowledgements, and has a prototype Clutter scene
path with four dedicated groups. Its root surface actor is not reactive.

Before enabling the global: enforce exclusive zones, implement popup
parenting, pointer and keyboard focus with session-lock policy, integrate
Cali's wallpaper/panel stacking, support subsurfaces and output changes,
and verify full builds and nested-session rendering. Test configure
serials and delayed transactions, including unmap/remap and teardown.
The renderer has not been runtime validated.
