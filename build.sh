#!/bin/sh
src/tools/buildbin.sh src/main.c "${1:-cdr}" \
    src/core src/wayland src/vulkan src/g2d src/gui src/repl
