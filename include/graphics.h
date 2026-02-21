#pragma once

#include "include/defs.h"

typedef struct gfxctx gfxctx;

gfxctx *gfxinit(void *dpyctx);

void gfxterm(gfxctx *);
