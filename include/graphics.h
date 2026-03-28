#pragma once

#include "include/defs.h"

#include "include/display.h"

typedef struct gfxctx gfxctx;

// -- vertex buffer ------------------------------------------------------------
#define GFX_MAX_VTXATTRS 8

typedef enum
{
  GFXFMT_2F32,
  GFXFMT_3F32,
  GFXFMT_4F32,
  GFXFMT_2U8,
  GFXFMT_3U8,
  GFXFMT_4U8,
} gfxfmt_vtx;

typedef struct
{
  u32        offset;
  gfxfmt_vtx fmt;
} gfxvtxattr;

typedef struct
{
  u32        nrvtx;
  u32        vtxstride;
  gfxvtxattr attrs[GFX_MAX_VTXATTRS];
  u32        nrattrs;
} gfxdesc_vtxbuf;

typedef struct gfxbuf gfxbuf;

int  gfxinit_vtxbuf(gfxbuf **, gfxctx *, gfxdesc_vtxbuf, const void *data);
void gfxterm_vtxbuf(gfxctx *, gfxbuf *);

// -- image --------------------------------------------------------------------
typedef enum
{
  GFXFMT_RGBA8_SRGB,
  GFXFMT_RGBA8_UNORM,
  GFXFMT_BGRA8_SRGB,
  GFXFMT_BGRA8_UNORM,
  GFXFMT_R8_UNORM,
  GFXFMT_D32_SFLOAT,
} gfxfmt_img;

typedef struct
{
  u32        w, h;
  gfxfmt_img fmt;
} gfxdesc_img;

typedef struct gfximg gfximg;

int  gfxinit_img(gfximg **, gfxctx *, gfxdesc_img);
void gfxterm_img(gfxctx *, gfximg *);

// -- pipeline -----------------------------------------------------------------
typedef struct
{
  const u32     *spv;
  usize          nrbytes;
  gfxdesc_vtxbuf vtxdesc;
} gfxdesc_ppl;

typedef struct gfxppl gfxppl;

int  gfxinit_ppl(gfxppl **, gfxctx *, gfxdesc_ppl);
void gfxterm_ppl(gfxctx *, gfxppl *);

// -- pass ---------------------------------------------------------------------
#define GFX_MAX_PASS_IMGS 16

typedef struct
{
  gfxppl *ppl;
  gfxbuf *vtx;
  gfximg *inputs[GFX_MAX_PASS_IMGS];
  u32     nrinputs;
  gfximg *outputs[GFX_MAX_PASS_IMGS];
  u32     nroutputs;
} gfxpass;

int gfxrender(gfxctx *, gfxpass *, u32 nrpasses);

// -- context ------------------------------------------------------------------
int  gfxinit(gfxctx **, dpyctx *);
void gfxterm(gfxctx *);
int  gfxqry_init(gfxctx *);
void gfxpause(gfxctx *);
void gfxresume(gfxctx *);
void gfxupdate(gfxctx *, i32 w, i32 h);
int  gfxprep(gfxctx *, const u32 *spv, usize nrbytes);
