<div align="center">

<img src=".github/assets/sookie.svg" width="120" alt="Sookie logo">

# Sookie

**The compositor behind the [Cali Desktop Environment](https://github.com/CaliDE-Foundation/DE).**

It's Mutter's rock-solid core, opened up so the desktop can do more.

[![Build](https://github.com/CaliDE-Foundation/Sookie/actions/workflows/build.yml/badge.svg)](https://github.com/CaliDE-Foundation/Sookie/actions/workflows/build.yml)
[![License: GPL v2+](https://img.shields.io/badge/license-GPL--2.0--or--later-blue.svg)](COPYING)
[![Based on Mutter 51.1](https://img.shields.io/badge/based%20on-Mutter%2051.1-4a86cf.svg)](https://gitlab.gnome.org/GNOME/mutter)
![Wayland](https://img.shields.io/badge/Wayland-%2B%20Xwayland-ff8a5b.svg)

[Features](#features) · [What's different](#whats-different-from-mutter) · [Building](#building) · [Running](#running-a-nested-session) · [Docs](#documentation)

</div>

---

Sookie is a Wayland display server and window manager. It's a fork of GNOME's
[Mutter](https://gitlab.gnome.org/GNOME/mutter), and it powers **CaliDE**, a desktop
that feels like modern GNOME and gives users far more control.

We keep Mutter's foundations: the smooth frame scheduling, the Clutter scene graph and
years of hardware support. On top of that we add the hooks and options that a
customizable desktop needs. Sookie tracks upstream closely so it keeps getting Mutter's
fixes.

## Features

Inherited from Mutter and ready today:

| | |
|---|---|
| 🖥️ **Display** | KMS/DRM, multi-GPU, multi-monitor, HDR & color management, VRR, adaptive backlight |
| ✨ **Rendering** | Clutter scene graph and Cogl (OpenGL / GLES2) for smooth, frame-accurate animation |
| 🖱️ **Input** | libinput, touch, tablets (libwacom), gestures, pointer constraints, input capture |
| 🪟 **Window management** | Focus, stacking, workspaces, keybindings, placement constraints |
| 🧩 **Compatibility** | Xwayland for X11 apps, plus a broad set of Wayland protocols |
| 📡 **Sharing** | Screen casting and remote desktop over PipeWire |
| ♿ **Accessibility** | Screen-reader hooks, idle and power handling |

## What's different from Mutter

Sookie exists to serve CaliDE. Planned changes:

- [x] **Sookie branding.** `libsookie-52`, `sookie`, `sookie-devkit` and the install paths drop the Mutter name
- [ ] **Configurable behavior.** Expose settings that Mutter hard-codes
- [ ] **More window management hooks.** Tiling, rules and custom layouts for the shell
- [ ] **Animation control.** Users can tune or replace the default transitions
- [ ] **New code in Zig.** Added through C-ABI objects next to the existing C
- [ ] **X11 session.** Mutter dropped its X11 backend in 50.0, so this comes after Wayland

> [!NOTE]
> Sookie is early in development. Apart from the rename, it matches upstream Mutter 51.1.

### Compatibility with GNOME

Sookie keeps Mutter's **D-Bus interfaces** (`org.gnome.Mutter.*`) and **GSettings
schemas** (`org.gnome.mutter.*`). GNOME Settings, screen-sharing portals, display
configuration and `gdctl` all work with Sookie unchanged. The C API (`meta_*`,
`clutter_*`, `cogl_*`) and the GObject introspection namespaces (`Meta-52`, `Clutter-52`)
also stay the same.

Because the schemas are shared, a Sookie package replaces the Mutter package. The two
can't be installed side by side.

| Mutter | Sookie |
|---|---|
| `mutter` | `sookie` |
| `libmutter-52.so`, `libmutter-52.pc` | `libsookie-52.so`, `libsookie-52.pc` |
| `libmutter-{clutter,cogl,mtk}-52` | `libsookie-{clutter,cogl,mtk}-52` |
| `/usr/lib/mutter-52/`, `/usr/include/mutter-52/` | `/usr/lib/sookie-52/`, `/usr/include/sookie-52/` |
| `mutter-devkit`, `mutter-x11-frames`, `mutter-backlight-helper` | `sookie-devkit`, `sookie-x11-frames`, `sookie-backlight-helper` |

## Building

Sookie is a [Meson](https://mesonbuild.com) project written in C, with Zig for new code.

### Using GitHub Actions

Every push to `main` and every pull request builds on **Fedora 45** in
[GitHub Actions](https://github.com/CaliDE-Foundation/Sookie/actions/workflows/build.yml).
Open a finished run and download the **`sookie-fedora45-x86_64`** artifact for a
ready-to-install `/usr` tree. You can also start a build by hand from the Actions tab
with **Run workflow**.

### Locally

The easiest way to get the dependencies is a Fedora [Toolbx](https://containertoolbx.org) container:

```sh
toolbox create --distro fedora --release 45 sookie
toolbox enter sookie
sudo dnf builddep -y mutter
```

Then configure and build:

```sh
meson setup _build --prefix=/usr -Dtests=disabled
meson compile -C _build
```

> [!TIP]
> Low on CPU cooling? Use `nice -n 19 ninja -C _build -j2`.

## Running a nested session

You can test without leaving your current desktop. Devkit opens Sookie in a window:

```sh
dbus-run-session ./_build/src/sookie --wayland --devkit
```

Open apps inside it by pointing them at the nested display that it prints, for example `wayland-1`:

```sh
WAYLAND_DISPLAY=wayland-1 gnome-text-editor
```

## Project layout

| Path | What's inside |
|---|---|
| `src/` | Window manager core, compositor, Wayland and Xwayland support, native backend |
| `clutter/` | Scene graph and animation framework |
| `cogl/` | GPU abstraction (OpenGL / GLES2) |
| `mtk/` | Shared toolkit utilities |
| `mdk/` | Devkit for running nested sessions |
| `doc/` | Developer documentation |

## Documentation

- [Building and running](doc/building-and-running.md)
- [Code overview](doc/code-overview.md)
- [Coding style](doc/coding-style.md) · [Git conventions](doc/git-conventions.md)
- [Debugging](doc/debugging.md) · [Multi-GPU](doc/multi-gpu.md) · [Monitor configuration](doc/monitor-configuration.md)
- Upstream API reference: [Meta](https://mutter.gnome.org/meta/) · [Clutter](https://mutter.gnome.org/clutter/) · [Cogl](https://mutter.gnome.org/cogl/) · [Mtk](https://mutter.gnome.org/mtk/)

## Contributing

Issues and pull requests are welcome at
[CaliDE-Foundation/Sookie](https://github.com/CaliDE-Foundation/Sookie). A fix that
isn't specific to Cali usually belongs upstream: please report it to
[Mutter](https://gitlab.gnome.org/GNOME/mutter) as well.

## License

Sookie is distributed under the **GNU General Public License, version 2 or later**,
the same as Mutter. See [COPYING](COPYING).

Sookie is based on [Mutter](https://gitlab.gnome.org/GNOME/mutter) by the GNOME
Project and its contributors. It is an independent project and is not affiliated with
or endorsed by GNOME.
