#!/bin/sh
tools/buildlib.sh src/vulkan libcdrgfx.a "${1:?usage: build.sh <output-dir>}"
LFLAGS="$LFLAGS -lvulkan"
