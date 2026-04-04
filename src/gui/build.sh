#!/bin/sh
OUTDIR="${1:?usage: build.sh <output-dir>}"
NAME="libcdrgui.a"
TMPDIR=$(mktemp -d)

mkdir -p "$OUTDIR"

for src in $(find src/gui -name '*.c' -not -path '*/tests/*'); do
    obj="$TMPDIR/$(basename "${src%.c}").o"
    gcc -c "$src" -I. -o "$obj" || { rm -rf "$TMPDIR"; exit 1; }
done

ar rcs "$OUTDIR/$NAME" "$TMPDIR"/*.o
rm -rf "$TMPDIR"
