// AI-generated (Claude)
// g2d stress test -- ramps rect count and prints frame times
#include "include/display.h"
#include "include/graphics.h"
#include "include/graphics2D.h"
#include "include/mem.h"
#include "src/core/include/debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define W 800
#define H 600

static volatile int quit= 0;

static void on_close(void *u)
{
  (void)u;
  quit= 1;
}

static void on_resize(i32 w, i32 h, void *gfx) { gfxupdate(gfx, w, h); }

static u32 xorshift(u32 *state)
{
  u32 x= *state;
  x^= x << 13;
  x^= x >> 17;
  x^= x << 5;
  *state= x;
  return x;
}

static f32 randf(u32 *state, f32 lo, f32 hi)
{
  return lo + (f32)(xorshift(state) & 0xFFFF) / 65535.f * (hi - lo);
}

static g2ddesc_rect *gen_rects(u32 count, u32 seed)
{
  g2ddesc_rect *rects= cdralloc(NULL, count * sizeof *rects);
  if(!rects) return NULL;

  u32 rng= seed;
  for(u32 i= 0; i < count; i++)
  {
    f32 w= randf(&rng, 2.f, 40.f);
    f32 h= randf(&rng, 2.f, 40.f);
    rects[i]= (g2ddesc_rect){
      .pos  = {.x= randf(&rng, 0.f, W - w), .y= randf(&rng, 0.f, H - h)},
      .size = {.x= w, .y= h},
      .color= {
        .r= randf(&rng, 0.1f, 1.f),
        .g= randf(&rng, 0.1f, 1.f),
        .b= randf(&rng, 0.1f, 1.f),
        .a= 1.f}};
  }
  return rects;
}

static f64 now_ms(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (f64)ts.tv_sec * 1000.0 + (f64)ts.tv_nsec / 1e6;
}

int main(void)
{
  int      rc  = -1;
  dpyctx  *dpy = NULL;
  gfxctx  *gfx = NULL;
  g2dctx  *g2d = NULL;
  gfxpass *pass= NULL;
  gfximg  *fb  = NULL;

  OK(dpyinit(&dpy, "g2d stress", W, H, 0), goto exit;)
  OK(gfxinit(&gfx, dpy, (gfxdesc){0}), goto exit;)
  OK(g2dinit(&g2d, gfx, (g2ddesc){.memhint= 100 * 1024 * 1024}), goto exit;)

  OK(
    gfxinit_img(
      gfx, &fb, NULL,
      (gfxdesc_img){.w= W, .h= H, .fmt= GFXFMT_BGRA8_SRGB}),
    goto exit;)
  OK(gfxinit_pass(gfx, &pass, (gfxdesc_pass){.outputs= {fb}, .nroutputs= 1}),
     goto exit;)

  dpyset_onrsz(dpy, on_resize);
  dpyset_onclose(dpy, on_close);
  dpyset_usrdata(dpy, gfx);

  u32 tiers[]= {100, 500, 1000, 5000, 10000, 50000, 100000, 500000};
  u32 ntiers = sizeof tiers / sizeof tiers[0];

  printf("g2d stress test\n");
  printf("%-10s  %-10s  %-10s  %-10s\n",
         "rects", "upload ms", "frame ms", "fps");
  printf("-------------------------------------------\n");

  for(u32 t= 0; t < ntiers && !quit; t++)
  {
    u32           n    = tiers[t];
    g2ddesc_rect *rects= gen_rects(n, 0xDEADBEEF + t);
    NNUL(rects, goto exit;)

    // upload
    f64 t0= now_ms();
    OK(g2dupload(
         g2d,
         (gfxdesc_upload){.data= rects, .bytes= n * sizeof *rects},
         NULL),
       { cdrfree(NULL, rects); goto exit; })
    f64 upload_ms= now_ms() - t0;

    // warm up: draw a few frames to stabilize
    for(int w= 0; w < 3 && !quit; w++)
    {
      dpypoll(dpy);
      OK(g2ddraw(g2d, pass, (t2f32){.x= W, .y= H}), goto exit;)
      OK(gfxsubmit(gfx, pass), goto exit;)
      OK(gfxpresent(gfx, fb), goto exit;)
    }
    gfxsync(gfx);

    // measure: average over 30 frames
    u32 nframes= 30;
    f64 start  = now_ms();
    for(u32 f= 0; f < nframes && !quit; f++)
    {
      dpypoll(dpy);
      OK(g2ddraw(g2d, pass, (t2f32){.x= W, .y= H}), goto exit;)
      OK(gfxsubmit(gfx, pass), goto exit;)
      OK(gfxpresent(gfx, fb), goto exit;)
    }
    gfxsync(gfx);
    f64 elapsed= now_ms() - start;
    f64 frame  = elapsed / nframes;
    f64 fps    = 1000.0 / frame;

    printf("%-10u  %-10.2f  %-10.2f  %-10.1f\n",
           n, upload_ms, frame, fps);

    cdrfree(NULL, rects);
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
