// AI-generated (Claude)
// g2d API integration test -- draws colored rectangles on screen
#include "include/display.h"
#include "include/graphics.h"
#include "include/graphics2D.h"
#include "src/core/include/debug.h"

static volatile int quit= 0;

static void on_close(void *u)
{
  (void)u;
  quit= 1;
}

static void on_resize(i32 w, i32 h, void *gfx) { gfxupdate(gfx, w, h); }

int main(void)
{
  int      rc  = -1;
  dpyctx  *dpy = NULL;
  gfxctx  *gfx = NULL;
  g2dctx  *g2d = NULL;
  gfxpass *pass= NULL;
  gfximg  *fb  = NULL;

  OK(dpyinit(&dpy, "g2d test", 800, 600, 0), goto exit;)
  OK(gfxinit(&gfx, dpy, (gfxdesc){0}), goto exit;)
  OK(g2dinit(&g2d, gfx, (g2ddesc){.memhint= 4096}), goto exit;)

  OK(
    gfxinit_img(
      gfx, &fb, NULL,
      (gfxdesc_img){.w= 800, .h= 600, .fmt= GFXFMT_BGRA8_SRGB}),
    goto exit;)
  OK(gfxinit_pass(gfx, &pass, (gfxdesc_pass){.outputs= {fb}, .nroutputs= 1}),
     goto exit;)

  g2ddesc_rect scene[]= {
    {.pos= {.x= 50.f,  .y= 50.f},
     .size= {.x= 200.f, .y= 150.f},
     .color= {.r= 1.f, .g= 0.2f, .b= 0.2f, .a= 1.f}},
    {.pos= {.x= 300.f, .y= 100.f},
     .size= {.x= 180.f, .y= 180.f},
     .color= {.r= 0.2f, .g= 0.8f, .b= 0.3f, .a= 1.f}},
    {.pos= {.x= 500.f, .y= 200.f},
     .size= {.x= 250.f, .y= 120.f},
     .color= {.r= 0.2f, .g= 0.4f, .b= 1.f, .a= 1.f}},
  };
  OK(g2dupload(
       g2d,
       (gfxdesc_upload){
         .data = scene,
         .bytes= sizeof scene},
       NULL),
     goto exit;)

  dpyset_onrsz(dpy, on_resize);
  dpyset_onclose(dpy, on_close);
  dpyset_usrdata(dpy, gfx);

  while(!quit)
  {
    dpypoll(dpy);
    OK(g2ddraw(g2d, pass, (t2f32){.x= 800.f, .y= 600.f}), goto exit;)
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
