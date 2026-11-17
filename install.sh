#!/bin/bash
# Installs the applet built by build.sh (builds it first if needed) and restarts plasmashell,
# which only loads applet plugins at startup.
#
#   ./install.sh              install
#   ./install.sh --uninstall  remove the installed files again

set -e
cd "$(dirname "$0")"

restart_plasmashell() {
    if systemctl --user is-active --quiet plasma-plasmashell.service; then
        echo "Restarting plasmashell..."
        systemctl --user restart plasma-plasmashell.service
    else
        echo "plasmashell is not running as a systemd user service: restart it yourself to load the applet."
    fi
}

if [ "$1" = "--uninstall" ]; then
    if [ ! -f build/install_manifest.txt ]; then
        echo "build/install_manifest.txt not found: nothing was installed from this build directory." >&2
        exit 1
    fi
    echo "Removing:"
    cat build/install_manifest.txt
    echo
    xargs -d '\n' sudo rm -f -- < build/install_manifest.txt
    restart_plasmashell
    exit 0
fi

if [ ! -d build ]; then
    ./build.sh
fi

sudo cmake --install build
restart_plasmashell
