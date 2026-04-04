#!/bin/sh
set -e

LIBDIR="lib"

src/core/build.sh    "$LIBDIR"
src/wayland/build.sh "$LIBDIR"
src/vulkan/build.sh  "$LIBDIR"
src/g2d/build.sh     "$LIBDIR"

gcc src/main.c -I. -L"$LIBDIR" \
    -lcdrg2d -lcdrgfx -lcdrdpy -lcdrcore \
    -lvulkan -lwayland-client -lm -o cdr
