#!/bin/bash
# Builds the applet into build/. Install afterwards with:
#   sudo cmake --install build && systemctl --user restart plasma-plasmashell

set -e
cd "$(dirname "$0")"

cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build --parallel
