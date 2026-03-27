#pragma once

#include "include/graphics.h"

typedef struct replctx replctx;

int      replinit(replctx **, gfxctx *);
void     replterm(replctx *);
void     reploop(replctx *);
