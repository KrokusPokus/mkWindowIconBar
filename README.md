# mkWindowSwitcher

A minimalist window switcher for the KDE Plasma 6 panel. It replaces the
*Icons-Only Task Manager* and nothing else: application launcher, system tray
and the other panel widgets stay as they are.

Every open window is shown as one icon. A click switches to it.

## Why

Plasma's task managers show *apps*, not windows. They match each window to the
`.desktop` file of its application and always show that file's icon, even if the
window sets an icon of its own. Windows of the same program all look alike, and
an application cannot show its state or the current document through its window
icon. This is by design, see KDE bugs
[369658](https://bugs.kde.org/show_bug.cgi?id=369658) and
[476794](https://bugs.kde.org/show_bug.cgi?id=476794).

mkWindowSwitcher shows the icon KWin reports for each window: the icon the
application set, otherwise the one of its `.desktop` file, otherwise a generic
icon.

## Features

* One icon per window, in the order the windows were opened
* Left click: activate the window, or minimize it if it is the active one
* Middle click: close the window
* Tooltip with the window title
* Highlighting of the active window, minimized windows and windows demanding attention,
  drawn with the task bar graphics of the Plasma theme
* Minimize animation towards the window's icon
* Horizontal and vertical panels

Not included on purpose: pinned launchers, startup notifications, grouping.

## Requirements

* KDE Plasma 6 in a **Wayland** session (X11 is not supported)
* For building: CMake, extra-cmake-modules, Qt 6 (incl. Wayland client), libplasma,
  plasma-wayland-protocols

On Arch Linux:

```
sudo pacman -S --needed cmake extra-cmake-modules qt6-base qt6-declarative qt6-wayland libplasma plasma-wayland-protocols
```

## Build and install

```
./build.sh                # builds into build/
./install.sh              # installs (sudo) and restarts plasmashell
./install.sh --uninstall  # removes the installed files again
```

Then edit the panel, choose *Add Widgets…*, and drag *Window Switcher* into the panel.
The original task manager can be removed afterwards.

Plasma loads applet plugins only at startup: after installing a new version,
plasmashell has to be restarted (`install.sh` does that).

## License

GPL-2.0-or-later, see [LICENSE](LICENSE).

The window list (`src/windowmodel.*`) is adapted from `libtaskmanager` of
[plasma-workspace](https://invent.kde.org/plasma/plasma-workspace) and keeps its
license (LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL). Each file
states its license in its SPDX header; the license texts are in [LICENSES](LICENSES).
