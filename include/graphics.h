#pragma once

#include "include/defs.h"

#include "include/display.h"

typedef struct gfxctx gfxctx;

gfxctx *gfxinit(dpyctx *dpy);
void    gfxterm(gfxctx *);
int     gfxinit_q(gfxctx *);
void    gfxpause(gfxctx *);
void    gfxresume(gfxctx *);
int     gfxrender(gfxctx *);
void    gfxupdate(gfxctx *, i32 w, i32 h);
int     gfxprep(gfxctx *, const u32 *spv, usize nrbytes);
