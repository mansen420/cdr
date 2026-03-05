#include "include/graphics.h"
#include "include/repl.h"
#include "src/repl/s7.h"

#include <stdlib.h>

struct replctx
{
  s7_scheme   *sc;
  gfxctx      *gfx;
  volatile int quit;
};

static replctx *init(gfxctx *gfx)
{
  replctx *ctx= calloc(1, sizeof *ctx);
  if(!ctx) return NULL;
  ctx->sc = s7_init();
  ctx->gfx= gfx;
  return ctx;
}

static void term(replctx *ctx)
{
  if(!ctx) return;
  free(ctx->sc);
  free(ctx);
}

static void loop(replctx *ctx)
{
  s7_repl(ctx->sc);
  ctx->quit= 1;
}

replctx *replinit(gfxctx *gfx) { return init(gfx); }
void     replterm(replctx *ctx) { term(ctx); }
void     reploop(replctx *ctx) { loop(ctx); }
