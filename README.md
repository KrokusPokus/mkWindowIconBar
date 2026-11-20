# mkWindowIconBar

A minimalist window bar widget for the KDE Plasma 6 panel, an alternative to the
*Icons-Only Task Manager*.

Every open window is shown as one icon. A click switches to it.

## Why

Plasma's task managers show *apps*, not windows. They match each window to the
`.desktop` file of its application and always show that file's icon, even if the
window sets an icon of its own. Windows of the same program all look alike, and
an application cannot show its state or the current document through its window
icon. This is by design, see KDE bugs
[369658](https://bugs.kde.org/show_bug.cgi?id=369658) and
[476794](https://bugs.kde.org/show_bug.cgi?id=476794).

mkWindowIconBar shows the icon KWin reports for each window: the icon the
application set, otherwise the one of its `.desktop` file, otherwise a generic
icon.

## Features

* One icon per window, in the order the windows were opened
* Left click: activate the window, or minimize it if it is the active one
* Middle click: close the window
* Tooltip as in the task manager without thumbnails: application name, window title, close button
* Context menu: move to another virtual desktop or screen, minimize, maximize, keep above others, close
* Options: only the windows of the current virtual desktop, only the windows of the current screen,
  move animation with a duration that depends on the distance
* Reordering by drag and drop
* Highlighting of the active window, minimized windows and windows demanding attention,
  drawn with the task bar graphics of the Plasma theme
* Minimize animation towards the window's icon
* Horizontal and vertical panels

Not included on purpose: pinned launchers, startup notifications, grouping.

## Requirements

* KDE Plasma 6 in a **Wayland** session (X11 is not supported)
* For building: CMake ≥ 3.22, extra-cmake-modules, Qt ≥ 6.8 (incl. Wayland client),
  KDE Frameworks ≥ 6.10 (KI18n, KService), libplasma, plasma-wayland-protocols

On Arch Linux:

```
sudo pacman -S --needed base-devel cmake extra-cmake-modules qt6-base qt6-declarative qt6-wayland \
    ki18n kservice libplasma plasma-wayland-protocols
```

## Build and install

```
git clone https://github.com/KrokusPokus/mkWindowIconBar.git
cd mkWindowIconBar
./build.sh                                    # builds into build/
sudo cmake --install build                    # installs system-wide (prefix /usr)
sudo xargs rm < build/install_manifest.txt    # uninstalls again
```

Then edit the panel, choose *Add Widgets…*, and drag *Window Icon Bar* (German: *Fensterleiste mit Fenstersymbolen*) into the panel.
Or swap an existing task manager in place: in the panel's edit mode, choose *Show Alternatives* on it.

Plasma loads applet plugins only at startup: after installing a new version,
restart plasmashell (`systemctl --user restart plasma-plasmashell.service`) or log out and back in.

## License

GPL-2.0-or-later, see [LICENSE](LICENSE).

The window list (`src/windowmodel.*`) is adapted from `libtaskmanager` of
[plasma-workspace](https://invent.kde.org/plasma/plasma-workspace) and keeps its
license (LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL). Each file
states its license in its SPDX header; the license texts are in [LICENSES](LICENSES).
