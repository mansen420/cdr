#!/bin/sh
set -e

out="${1:-/tmp/cdr-swr-tests/test_triangles}"
mkdir -p "$(dirname "$out")"
tools/buildbin.sh src/swr/tests/test_triangles.c "$out" src/wayland src/swr
printf 'built %s\n' "$out"
