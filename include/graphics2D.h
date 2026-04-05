#include "include/graphics.h"

typedef struct g2dctx g2dctx;

typedef struct
{
  t2f32 pos;
  t2f32 size;
  t4f32 color;
} g2ddesc_rect;

enum
{
	G2D_NO_IMPLICIT_MEM= 1
};

typedef struct
{
	u32 flags;
	usize memhint;
} g2ddesc;

int  g2dinit(g2dctx **, gfxctx *, g2ddesc);
void g2dterm(g2dctx *);
// uploads into gfxmem, if NULL, uses own memory
int  g2dupload(g2dctx *, gfxdesc_upload, gfxmem *);
// records commands into pass, screen = display size in pixels
int  g2ddraw(g2dctx *, gfxpass *, t2f32 screen);
