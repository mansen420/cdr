// Diagnostic: draw a SINGLE static axis-aligned rect and screenshot it.
#include "include/display.h"
#include "include/graphics.h"
#include "include/graphics2D.h"
#include "src/core/include/debug.h"

#define FBW 960
#define FBH 540

static volatile int quit= 0;
static void on_close(void *u) { (void)u; quit= 1; }

int main(void)
{
  int      rc  = -1;
  dpyctx  *dpy = NULL;
  gfxctx  *gfx = NULL;
  g2dctx  *g2d = NULL;
  gfximg  *fb  = NULL;
  gfxpass *pass= NULL;

  OK(dpyinit(&dpy, "onerect", FBW, FBH, 0), goto exit;)
  OK(gfxinit(&gfx, dpy, (gfxdesc){.devmem= 64u << 20, .hostmem= 16u << 20}),
     goto exit;)
  OK(g2dinit(&g2d, gfx, (g2ddesc){0}), goto exit;)
  OK(gfxinit_img(
       gfx, &fb, NULL,
       (gfxdesc_img){.w= FBW, .h= FBH, .fmt= GFXFMT_BGRA8_SRGB}),
     goto exit;)
  OK(gfxinit_pass(
       gfx, &pass,
       (gfxdesc_pass){.outputs= {{.img= fb, .clear= 1}}, .nroutputs= 1}),
     goto exit;)

  dpyset_usrdata(dpy, NULL);
  dpyset_onclose(dpy, on_close);

  g2ddesc_rect one[3]= {
    {.pos  = {.x= 80.f, .y= 80.f},
     .size = {.x= 120.f, .y= 120.f},
     .color= {.r= 1.f, .g= 0.f, .b= 0.f, .a= 1.f}},
    {.pos  = {.x= 400.f, .y= 200.f},
     .size = {.x= 120.f, .y= 120.f},
     .color= {.r= 0.f, .g= 1.f, .b= 0.f, .a= 1.f}},
    {.pos  = {.x= 700.f, .y= 350.f},
     .size = {.x= 120.f, .y= 120.f},
     .color= {.r= 0.f, .g= 0.f, .b= 1.f, .a= 1.f}}};

  while(!quit)
  {
    dpypoll(dpy);
    OK(g2dupload(
         g2d,
         (gfxdesc_upload){.data= one, .bytes= sizeof one, .offset= 0},
         NULL),
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
