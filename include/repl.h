#pragma once

#include "include/graphics.h"

typedef struct replctx replctx;

replctx *replinit(gfxctx *);
void     replterm(replctx *);
void     reploop(replctx *);
