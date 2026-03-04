#pragma once

typedef struct gfxctx gfxctx;

typedef struct
{
  volatile int quit;
  volatile int paused;
  gfxctx      *gfx;
} replctx;

replctx *replinit(gfxctx *);
void     replterm(replctx *);
void     reploop(replctx *);
