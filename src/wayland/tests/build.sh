#!/bin/sh
set -e

outdir="${1:-/tmp/cdr-wayland-presentation-tests}"
mkdir -p "$outdir"

for testsrc in \
  src/wayland/tests/test_present_gradient.c \
  src/wayland/tests/test_present_once.c \
  src/wayland/tests/test_present_solid.c
do
  testname=$(basename "${testsrc%.c}")
  tools/buildbin.sh "$testsrc" "$outdir/$testname" src/wayland
done

printf 'built presentation tests in %s\n' "$outdir"
