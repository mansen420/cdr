#!/bin/sh
# usage: buildlib.sh <module-dir> <lib-name> <output-dir>
# compiles all .c files in module-dir, caches .o files, produces a .a
set -e

MODDIR="${1:?usage: buildlib.sh <module-dir> <lib-name> <output-dir>}"
NAME="${2:?usage: buildlib.sh <module-dir> <lib-name> <output-dir>}"
OUTDIR="${3:?usage: buildlib.sh <module-dir> <lib-name> <output-dir>}"
CACHEDIR="$MODDIR/.cache"
TMPDIR=$(mktemp -d)

mkdir -p "$OUTDIR" "$CACHEDIR"

for src in $(find "$MODDIR" -name '*.c' -not -path '*/tests/*' -not -path '*/.cache/*'); do
    base=$(basename "${src%.c}")
    obj="$TMPDIR/$base.o"
    cached="$CACHEDIR/$base.o"
    if [ -f "$cached" ] && [ "$cached" -nt "$src" ]; then
        cp "$cached" "$obj"
    else
        gcc -c "$src" -I. $CFLAGS -o "$obj" || { rm -rf "$TMPDIR"; exit 1; }
        cp "$obj" "$cached"
    fi
done

ar rcs "$OUTDIR/$NAME" "$TMPDIR"/*.o
rm -rf "$TMPDIR"
