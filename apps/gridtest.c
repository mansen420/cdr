// Diagnostic: draw N static non-overlapping rects in a grid. N from argv.
#include "include/display.h"
#include "include/graphics.h"
#include "include/graphics2D.h"
#include "src/core/include/debug.h"
#include <stdlib.h>

#define FBW 960
#define FBH 540
#define MAXN 256

static volatile int quit= 0;
static void on_close(void *u) { (void)u; quit= 1; }

int main(int argc, char **argv)
{
  int      rc  = -1;
  dpyctx  *dpy = NULL;
  gfxctx  *gfx = NULL;
  g2dctx  *g2d = NULL;
  gfximg  *fb  = NULL;
  gfxpass *pass= NULL;
  int      n   = argc > 1 ? atoi(argv[1]) : 16;
  if(n < 1) n= 1;
  if(n > MAXN) n= MAXN;

  g2ddesc_rect r[MAXN];
  int cols= 8;
  int rows= (n + cols - 1) / cols;
  f32 cw  = (f32)FBW / cols;
  f32 ch  = (f32)FBH / (rows < 1 ? 1 : rows);
  for(int i= 0; i < n; i++)
  {
    int cx= i % cols, cy= i / cols;
    r[i]= (g2ddesc_rect){
      .pos  = {.x= cx * cw + 4.f, .y= cy * ch + 4.f},
      .size = {.x= cw - 8.f, .y= ch - 8.f},
      .color= {.r= (i % 3 == 0), .g= (i % 3 == 1), .b= (i % 3 == 2),
               .a= 1.f}};
  }

  OK(dpyinit(&dpy, "gridtest", FBW, FBH, 0), goto exit;)
  OK(gfxinit(&gfx, dpy, (gfxdesc){.devmem= 64u << 20, .hostmem= 16u << 20}),
     goto exit;)
  OK(g2dinit(&g2d, gfx, (g2ddesc){.memhint= 1u << 20}), goto exit;)
  OK(gfxinit_img(
       gfx, &fb, NULL, (gfxdesc_img){.w= FBW, .h= FBH, .fmt= GFXFMT_BGRA8_SRGB}),
     goto exit;)
  OK(gfxinit_pass(
       gfx, &pass,
       (gfxdesc_pass){.outputs= {{.img= fb, .clear= 1}}, .nroutputs= 1}),
     goto exit;)
  dpyset_onclose(dpy, on_close);

  while(!quit)
  {
    dpypoll(dpy);
    OK(g2dupload(
         g2d,
         (gfxdesc_upload){.data= r, .bytes= n * sizeof *r, .offset= 0}, NULL),
       goto exit;)
    gfxcmd_begin(pass);
    OK(g2ddraw(g2d, pass, (t2f32){.x= FBW, .y= FBH}), goto exit;)
    gfxcmd_end(pass);
    OK(gfxsubmit(gfx, pass), goto exit;)
    OK(gfxpresent(gfx, fb), goto exit;)
  }
  rc= 0;
exit:
  gfxsync(gfx);
  gfxterm_pass(gfx, pass);
  gfxterm_img(gfx, fb);
  g2dterm(g2d);
  gfxterm(gfx);
  dpyterm(dpy);
  return rc;
}
