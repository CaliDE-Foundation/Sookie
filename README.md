<div align="center">

<img src=".github/assets/sookie.svg" width="96" alt="">

# Sookie

The compositor behind [Cali](https://github.com/CaliDE-Foundation/DE). A fork of GNOME's Mutter.

[![Build](https://github.com/CaliDE-Foundation/Sookie/actions/workflows/build.yml/badge.svg)](https://github.com/CaliDE-Foundation/Sookie/actions/workflows/build.yml)

<img src=".github/assets/devkit.png" width="640" alt="Sookie running nested">

</div>

## Build

```sh
meson setup _build -Dtests=disabled
meson compile -C _build
dbus-run-session ./_build/src/sookie --wayland --devkit
```

Or grab the [nightly build](https://github.com/CaliDE-Foundation/Sookie/releases/tag/nightly) (Fedora 45).

GPL-2.0-or-later. Based on [Mutter](https://gitlab.gnome.org/GNOME/mutter); not affiliated with GNOME.
