#!/bin/sh
src/tools/buildlib.sh src/core libcdrcore.a "${1:?usage: build.sh <output-dir>}"
LFLAGS="$LFLAGS -lm"
