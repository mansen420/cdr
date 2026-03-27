#include "include/display.h"
#include "include/graphics.h"
#include "include/repl.h"
#include "src/repl/s7.h"

#include <stdlib.h>

struct replctx
{
  s7_scheme   *sc;
  gfxctx      *gfx;
  dpyctx      *dpy;
  volatile int quit;
};

static void term(replctx *ctx)
{
  if(!ctx) return;
  if(ctx->sc) free(ctx->sc);
  free(ctx);
}

static replctx *init(gfxctx *gfx)
{
  replctx *ctx= NULL;
  if(!gfx) goto err;

  ctx= calloc(1, sizeof *ctx);
  if(!ctx) goto err;

  ctx->sc= s7_init();
  if(!ctx->sc) goto err;

  ctx->gfx= gfx;

  return ctx;
err:
  term(ctx);
  return NULL;
}

static void loop(replctx *ctx)
{
  s7_repl(ctx->sc);
  ctx->quit= 1;
}

replctx *replinit(gfxctx *gfx) { return init(gfx); }
void     replterm(replctx *ctx) { term(ctx); }
void     reploop(replctx *ctx) { loop(ctx); }
