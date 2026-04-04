#!/bin/sh
# usage: buildbin.sh <source.c> <output> [dep-paths...]
# e.g.: buildbin.sh src/main.c cdr src/core src/wayland src/vulkan
set -e

SRC="${1:?usage: buildbin.sh <source.c> <output> [dep-paths...]}"
OUT="${2:?usage: buildbin.sh <source.c> <output> [dep-paths...]}"
shift 2

mkdir -p "$(dirname "$OUT")"
TMPLIB=$(mktemp -d)
LFLAGS=""

for dep in "$@"; do
    . "$dep/build.sh" "$TMPLIB" || { rm -rf "$TMPLIB"; exit 1; }
done

gcc "$SRC" -I. -L"$TMPLIB" \
    $(for a in "$TMPLIB"/*.a; do basename "$a" .a | sed 's/^lib/-l/'; done) \
    $LFLAGS -o "$OUT"

rm -rf "$TMPLIB"
