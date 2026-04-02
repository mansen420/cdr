#pragma once

#include "include/defs.h"

#include "include/display.h"

typedef struct gfxctx gfxctx;

// -- memory -------------------------------------------------------------------
typedef struct gfxmem gfxmem;

typedef struct
{
  usize       bytes;
  usize       offset;
  const void *data;
} gfxdesc_mem;

int  gfxinit_mem(gfxctx *, gfxmem **, usize bytes);
int  gfxinit_mapped(gfxctx *, gfxmem **, usize bytes, void **ptr);
void gfxterm_mem(gfxctx *, gfxmem *);

// -- buffer -------------------------------------------------------------------
#define GFX_MAX_ATTRS 16
#define GFX_MAX_BINDINGS 16

typedef enum
{
  GFXFMT_2F32,
  GFXFMT_3F32,
  GFXFMT_4F32,
  GFXFMT_2U8,
  GFXFMT_3U8,
  GFXFMT_4U8,
} gfxfmt_attr;

typedef struct
{
  u32         location;
  u32         offset;
  gfxfmt_attr fmt;
} gfxattr;

enum
{
  GFX_RATE_VERTEX,
  GFX_RATE_INSTANCE
};

typedef struct
{
  u32     binding;
  u32     stride;
  u32     rate;  // GFX_RATE_VERTEX or GFX_RATE_INSTANCE
  gfxattr attrs[GFX_MAX_ATTRS];
  u32     nrattrs;
} gfxdesc_bufbind;

typedef struct
{
  u32 nrelems;  // number of vertices or instances
  u32 stride;
} gfxdesc_buf;

typedef struct gfxbuf gfxbuf;

int  gfxinit_buf(gfxctx *, gfxbuf **, gfxmem *, gfxdesc_buf, gfxdesc_mem);
int  gfxupload(gfxctx *, gfxbuf *, gfxmem *, gfxdesc_mem);
void gfxterm_buf(gfxctx *, gfxbuf *);

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

int  gfxinit_img(gfxctx *, gfximg **, gfxmem *, gfxdesc_img);
void gfxterm_img(gfxctx *, gfximg *);

gfximg *gfxget_display(gfxctx *);

// -- pipeline -----------------------------------------------------------------
#define GFX_MAX_RANGES 16
#define GFX_MAX_SHADERS 16

enum
{
  GFXSTAGE_VERT= 1 << 0,
  GFXSTAGE_FRAG= 1 << 1
};

typedef struct
{
  usize bytes;
  usize offset;
  u32   stages;
} gfxdesc_pushrange;

typedef struct
{
  const u32  *spv;
  usize       nrbytes;
  u32         stagebits;
  const char *entry;
} gfxdesc_shader;
typedef struct
{
  gfxdesc_shader    program[GFX_MAX_SHADERS];
	u32 nrshaders;
  gfxdesc_bufbind   bindings[GFX_MAX_BINDINGS];
  u32               nrbindings;
  gfxdesc_pushrange ranges[GFX_MAX_RANGES];
  u32               nrranges;
} gfxdesc_ppl;

typedef struct gfxppl gfxppl;

int  gfxinit_ppl(gfxctx *, gfxppl **, gfxdesc_ppl);
void gfxterm_ppl(gfxctx *, gfxppl *);

// -- pass ---------------------------------------------------------------------
#define GFX_MAX_PASS_IMGS 16

typedef struct
{
  gfximg *outputs[GFX_MAX_PASS_IMGS];
  u32     nroutputs;
} gfxdesc_pass;

typedef struct gfxpass gfxpass;

int  gfxinit_pass(gfxctx *, gfxpass **, gfxdesc_pass);
void gfxterm_pass(gfxctx *, gfxpass *);

// -- commands -----------------------------------------------------------------
#define GFX_MAX_DRAW_IMGS 8

typedef struct
{
  gfxppl *ppl;
  struct
  {
    u32     binding;
    gfxbuf *buf;
  } bufs[GFX_MAX_BINDINGS];
  u32     nrbufs;
  gfxbuf *idx;
  gfximg *inputs[GFX_MAX_DRAW_IMGS];
  u32     nrinputs;
  u32     nrvtx;
  u32     firstvtx;
  u32     nrinst;
  u32     firstinst;
  u32     nridx;
  u32     firstidx;
  struct
  {
    gfxdesc_mem mem;
    u32         stages;
  } push;
} gfxdesc_drawcmd;

void gfxcmd_begin(gfxpass *);
void gfxcmd_draw(gfxpass *, gfxdesc_drawcmd);
void gfxcmd_end(gfxpass *);
int  gfxsubmit(gfxctx *, gfxpass *);

// -- context ------------------------------------------------------------------

enum
{
  GFX_NONE,
  GFX_NO_IMPLICIT_MEM  // don't use implicit context memory
};

typedef struct
{
  usize      devmem;
  usize      hostmem;
  gfxfmt_img swpfmt;
  u32        flags;
} gfxdesc;
// TODO make implicit context memory a bit more sophisticated
//  add autogrowth with slabs, garbage collection, etc.

int  gfxinit(gfxctx **, dpyctx *, gfxdesc);
void gfxterm(gfxctx *);
int  gfxqry_init(gfxctx *);
void gfxpause(gfxctx *);
void gfxresume(gfxctx *);
void gfxupdate(gfxctx *, i32 w, i32 h);
void gfxsync(gfxctx *);
