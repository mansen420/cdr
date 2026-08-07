// Bouncing boxes demo -- exercises display + g2d + resize/close.
// Consumer app built against the public headers only.
#define _POSIX_C_SOURCE 199309L  // clock_gettime + CLOCK_MONOTONIC
#include "include/display.h"
#include "include/graphics.h"
#include "include/graphics2D.h"
#include "src/core/include/debug.h"

#include <stdlib.h>
#include <time.h>

#define NRBOXES 24
#define FBW     960
#define FBH     540

typedef struct
{
  t2f32 pos;
  t2f32 vel;
  t2f32 size;
  t4f32 color;
} box;

typedef struct
{
  volatile int quit;
  gfxctx      *gfx;
  i32          winw, winh;
} appstate;

static void on_close(void *u)
{
  appstate *st= u;
  st->quit    = 1;
}

static void on_resize(i32 w, i32 h, void *u)
{
  appstate *st= u;
  st->winw    = w;
  st->winh    = h;
  gfxupdate(st->gfx, w, h);
}

static f32 frand(f32 lo, f32 hi)
{
  return lo + (hi - lo) * ((f32)rand() / (f32)RAND_MAX);
}

static void box_init(box *b)
{
  f32 s   = frand(24.f, 72.f);
  b->size = (t2f32){.x= s, .y= s};
  b->pos  = (t2f32){.x= frand(0.f, FBW - s), .y= frand(0.f, FBH - s)};
  b->vel  = (t2f32){.x= frand(-220.f, 220.f), .y= frand(-220.f, 220.f)};
  b->color= (t4f32){
    .r= frand(0.2f, 1.f), .g= frand(0.2f, 1.f), .b= frand(0.2f, 1.f), .a= 1.f};
}

static void box_step(box *b, f32 dt)
{
  b->pos.x+= b->vel.x * dt;
  b->pos.y+= b->vel.y * dt;
  if(b->pos.x < 0.f)
  {
    b->pos.x= 0.f;
    b->vel.x= -b->vel.x;
  }
  if(b->pos.y < 0.f)
  {
    b->pos.y= 0.f;
    b->vel.y= -b->vel.y;
  }
  if(b->pos.x + b->size.x > FBW)
  {
    b->pos.x= FBW - b->size.x;
    b->vel.x= -b->vel.x;
  }
  if(b->pos.y + b->size.y > FBH)
  {
    b->pos.y= FBH - b->size.y;
    b->vel.y= -b->vel.y;
  }
}

static f64 now_sec(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (f64)ts.tv_sec + (f64)ts.tv_nsec * 1e-9;
}

int main(void)
{
  int          rc  = -1;
  dpyctx      *dpy = NULL;
  gfxctx      *gfx = NULL;
  g2dctx      *g2d = NULL;
  gfximg      *fb  = NULL;
  gfxpass     *pass= NULL;
  appstate     st  = {0};
  box          boxes[NRBOXES];
  g2ddesc_rect rects[NRBOXES];

  srand(1234);
  for(int i= 0; i < NRBOXES; i++) box_init(&boxes[i]);

  OK(dpyinit(&dpy, "boxes", FBW, FBH, 0), goto exit;)
  // (gfxdesc){0} gives a default implicit pool that is too small once g2d
  // shares it with our framebuffer image -- size devmem/hostmem in bytes.
  OK(gfxinit(
       &gfx, dpy,
       (gfxdesc){.devmem= 64u << 20, .hostmem= 16u << 20}),
     goto exit;)
  // memhint: undocumented units. Guess bytes; reserve generously so 24
  // rects don't overflow g2d's internal capacity.
  OK(g2dinit(&g2d, gfx, (g2ddesc){.memhint= 1u << 20}), goto exit;)
  OK(gfxinit_img(
       gfx, &fb, NULL,
       (gfxdesc_img){.w= FBW, .h= FBH, .fmt= GFXFMT_BGRA8_SRGB}),
     goto exit;)
  OK(gfxinit_pass(
       gfx, &pass,
       (gfxdesc_pass){.outputs= {{.img= fb, .clear= 1}}, .nroutputs= 1}),
     goto exit;)

  st.gfx = gfx;
  st.winw= FBW;
  st.winh= FBH;
  dpyset_usrdata(dpy, &st);
  dpyset_onrsz(dpy, on_resize);
  dpyset_onclose(dpy, on_close);

  f64 prev= now_sec();
  while(!st.quit)
  {
    dpypoll(dpy);

    f64 t = now_sec();
    f32 dt= (f32)(t - prev);
    prev  = t;
    if(dt > 0.05f) dt= 0.05f;

    for(int i= 0; i < NRBOXES; i++)
    {
      box_step(&boxes[i], dt);
      rects[i]= (g2ddesc_rect){
        .pos= boxes[i].pos, .size= boxes[i].size, .color= boxes[i].color};
    }

    OK(g2dupload(
         g2d,
         (gfxdesc_upload){
           .data= rects, .bytes= sizeof rects, .offset= 0},
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
