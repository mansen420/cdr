#!/bin/sh
tools/buildbin.sh tools/emitspv.c "${1:-emit}" \
    src/core
