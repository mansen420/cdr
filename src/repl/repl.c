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

static int init(replctx **out, gfxctx *gfx)
{
  replctx *ctx= NULL;
  if(!gfx) goto err;

  ctx= calloc(1, sizeof *ctx);
  if(!ctx) goto err;

  ctx->sc= s7_init();
  if(!ctx->sc) goto err;

  ctx->gfx= gfx;

  *out= ctx;
  return 0;
err:
  term(ctx);
  return -1;
}

static void loop(replctx *ctx)
{
  s7_repl(ctx->sc);
  ctx->quit= 1;
}

int      replinit(replctx **out, gfxctx *gfx) { return init(out, gfx); }
void     replterm(replctx *ctx) { term(ctx); }
void     reploop(replctx *ctx) { loop(ctx); }
