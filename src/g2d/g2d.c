#include "include/graphics.h"
#include "include/graphics2D.h"
#include "include/mem.h"
#include "src/core/include/debug.h"
#include "src/g2d/g2d.frag.spv.h"
#include "src/g2d/g2d.vert.spv.h"

#include <stdalign.h>
#include <stddef.h>
#include <string.h>

struct g2dctx
{
  gfxctx       *gpu;
  gfxppl       *ppl;  // later on we might need many pipelines
  gfxmem_dev   *mem;
  usize         memcap;
  gfxbuf       *buf;
  u32           bufcap;  // max rects the current buffer can hold
  g2ddesc_rect *rects;
  u32           nrrects;
};

typedef g2ddesc_rect desc_rect;

static void term(g2dctx *ctx)
{
  if(!ctx) return;
  if(ctx->buf) gfxterm_buf(ctx->gpu, ctx->buf);
  if(ctx->ppl) gfxterm_ppl(ctx->gpu, ctx->ppl);
  if(ctx->mem) gfxterm_mem(ctx->gpu, (gfxmem *)ctx->mem);
  if(ctx->rects) cdrfree(NULL, ctx->rects);
  cdrfree(NULL, ctx);
}

static int init(g2dctx **out, gfxctx *gpu, g2ddesc desc)
{
  int     rc   = -1;
  g2dctx *ctx  = NULL;

  NNUL(out, goto exit;)
  NNUL(gpu, goto exit;)

  ctx= cdralloc(NULL, sizeof *ctx);
  NNUL(ctx, goto exit;)

  ctx->gpu= gpu;

  // pipeline init
  {
    OK(gfxinit_ppl(
         gpu,
         &ctx->ppl,
         (gfxdesc_ppl){
           .nrshaders= 2,
           .program=
             {(gfxdesc_shader){
                .spv      = assets_g2d_frag_spv,
                .nrbytes  = len_assets_g2d_frag_spv,
                .stagebits= GFXSTAGE_FRAG,
                .entry    = "main"},
              (gfxdesc_shader){
                .spv      = assets_g2d_vert_spv,
                .nrbytes  = len_assets_g2d_vert_spv,
                .stagebits= GFXSTAGE_VERT,
                .entry    = "main"}},
           .nrranges  = 1,
           .ranges    = {(gfxdesc_pushrange){
             .bytes = sizeof(t2f32),
             .offset= 0,
             .stages= GFXSTAGE_VERT | GFXSTAGE_FRAG}},
           .nrbindings= 1,
           .bindings  = {(gfxdesc_bufbind){
             .binding= 0,
             .stride = sizeof(desc_rect),
             .rate   = GFX_RATE_INSTANCE,
             .nrattrs= 3,
             .attrs=
               {(gfxdesc_attr){
                  .location= 0,
                  .offset  = offsetof(desc_rect, pos),
                  .fmt     = GFXFMT_2F32},
                (gfxdesc_attr){
                  .location= 1,
                  .offset  = offsetof(desc_rect, size),
                  .fmt     = GFXFMT_2F32},
                (gfxdesc_attr){
                  .location= 2,
                  .offset  = offsetof(desc_rect, color),
                  .fmt     = GFXFMT_4F32}}}}}),
       goto exit;)
  }

  if(!(desc.flags & G2D_NO_IMPLICIT_MEM) && desc.memhint)
  {
    OK(gfxinit_devmem(ctx->gpu, &ctx->mem, desc.memhint), goto exit;)
    NNUL(ctx->mem, goto exit;)
    ctx->memcap= desc.memhint;
  }

  *out= ctx;
  rc  = 0;
exit:
  OK(rc, term(ctx);)
  return rc;
}

static int upload(g2dctx *ctx, gfxdesc_upload d, gfxmem *mem)
{
  int rc   = -1;
  u32 count= d.bytes / sizeof(desc_rect);

  NNUL(d.data, goto exit;)
  IF(d.bytes % sizeof(desc_rect) != 0, goto exit;)

  // ensure backing memory exists and is large enough
  // TODO maybe extract the realloc logic
  if(!mem)
  {
    if(!ctx->mem)
    {
      OK(gfxinit_devmem(ctx->gpu, &ctx->mem, d.bytes), goto exit;)
      NNUL(ctx->mem, goto exit;)
      ctx->memcap= d.bytes;
    }
    else if(ctx->memcap < d.bytes)
    {
      gfxterm_buf(ctx->gpu, ctx->buf);
      ctx->buf   = NULL;
      ctx->bufcap= 0;
      gfxterm_mem(ctx->gpu, (gfxmem *)ctx->mem);
      ctx->mem= NULL;
      OK(gfxinit_devmem(ctx->gpu, &ctx->mem, 2 * d.bytes), goto exit;)
      NNUL(ctx->mem, goto exit;)
      ctx->memcap= 2 * d.bytes;
    }
    mem= (gfxmem *)ctx->mem;
  }

  // fast path: reuse existing buffer
  if(ctx->buf && count <= ctx->bufcap)
    OK(gfxupload(ctx->gpu, ctx->buf, mem, d), goto exit;)
  // slow path: recreate buffer
  else
  {
    gfxbuf *newbuf= NULL;
    OK(gfxinit_buf(
         ctx->gpu, &newbuf, mem, (gfxdesc_buf){count, sizeof(desc_rect)}, d),
       goto exit;)
    NNUL(newbuf, goto exit;)

    if(ctx->buf) gfxterm_buf(ctx->gpu, ctx->buf);
    ctx->buf   = newbuf;
    ctx->bufcap= count;
  }

  // update CPU-side copy
  if(ctx->rects) cdrfree(NULL, ctx->rects);
  ctx->rects= cdralloc(NULL, d.bytes);
  NNUL(ctx->rects, goto exit;)
  memcpy(ctx->rects, d.data, d.bytes);
  ctx->nrrects= count;

  rc= 0;
exit:
  return rc;
}

static int draw(g2dctx *ctx, gfxpass *pass, t2f32 screen)
{
  int rc= -1;

  NNUL(ctx, goto exit;)
  NNUL(pass, goto exit;)
  NNUL(ctx->buf, goto exit;)

  gfxcmd_begin(pass);
  gfxcmd_draw(
    pass,
    (gfxdesc_drawcmd){
      .ppl   = ctx->ppl,
      .bufs  = {{.binding= 0, .buf= ctx->buf}},
      .nrbufs= 1,
      .nrvtx = 6,
      .nrinst= ctx->nrrects,
      .push  = {
        .mem   = {.data= &screen, .bytes= sizeof screen},
        .stages= GFXSTAGE_VERT | GFXSTAGE_FRAG}});
  gfxcmd_end(pass);

  rc= 0;
exit:
  return rc;
}

int g2dinit(g2dctx **out, gfxctx *gpu, g2ddesc desc)
{
  return init(out, gpu, desc);
}

void g2dterm(g2dctx *c) { term(c); }

int g2dupload(g2dctx *c, gfxdesc_upload d, gfxmem *m)
{
  return upload(c, d, m);
}

int g2ddraw(g2dctx *c, gfxpass *p, t2f32 screen) { return draw(c, p, screen); }
