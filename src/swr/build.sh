#!/bin/sh
tools/buildlib.sh src/swr libcdrswr.a "${1:?usage: build.sh <output-dir>}"
LFLAGS="$LFLAGS -lm"
