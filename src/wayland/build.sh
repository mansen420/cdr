#!/bin/sh
src/tools/buildlib.sh src/wayland libcdrdpy.a "${1:?usage: build.sh <output-dir>}"
LFLAGS="$LFLAGS -lwayland-client"
