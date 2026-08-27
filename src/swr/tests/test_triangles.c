#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "include/display.h"
#include "include/swr.h"

static int quit;

static void onclose(void *) { quit= 1; }

static u64 now_ns(void)
{
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (u64)now.tv_sec * UINT64_C(1000000000) + (u64)now.tv_nsec;
}

static t4u8 argb_mask(u8 r, u8 g, u8 b, u8 a)
{
  r= (u8)((u32)r * a / 255);
  g= (u8)((u32)g * a / 255);
  b= (u8)((u32)b * a / 255);

  u32  packed= (u32)a << 24 | (u32)r << 16 | (u32)g << 8 | b;
  t4u8 mask;
  memcpy(&mask, &packed, sizeof mask);
  return mask;
}

static void fill(imgview img, u32 color)
{
  for(usize y= 0; y < img.pxh; y++)
  {
    u32 *row= (u32 *)((u8 *)img.data + y * img.stride);
    for(usize x= 0; x < img.pxw; x++) row[x]= color;
  }
}

int main(void)
{
  u64     startup_begin= now_ns();
  int     rc = 1;
  dpyctx *dpy= NULL;
  dpysrf *srf= NULL;
  dpyswp *swp= NULL;

  if(dpyinit(&dpy, 0) < 0) goto exit;
  if(dpyinit_srf(
       dpy,
       &srf,
       (dpydesc_srf){.w= 640, .h= 480, .title= "CDR software triangles"}) <
     0)
    goto exit;

  dpyset_onclose(srf, onclose);
  if(dpyinit_swp(dpy, srf, &swp, (dpydesc_swp){.fmt= ARGB8}) < 0) goto exit;

  u64 loop_sum    = 0;
  u64 report_begin= now_ns();
  u64 frame       = 0;
  u32 loop_count  = 0;
  u32 frame_count = 0;
  int startup_done= 0;
  while(!quit)
  {
    u64 loop_begin= now_ns();
    if(dpypoll(dpy) < 0) goto exit;

    imgview img= dpyacq_nextbuf(swp);
    if(img.data)
    {
      fill(img, UINT32_C(0xff181a20));

      raster_4u8 target= {
        .buf   = img.data,
        .stride= (u32)(img.stride / sizeof(t4u8)),
        .pxw   = (u32)img.pxw,
        .pxh   = (u32)img.pxh};

      u32 step= (u32)frame;
      swgeo_2tri moving[]= {
        {{(f32)((step * 2) % target.pxw),
          (f32)((step * 1) % target.pxh)},
         {(f32)((step * 2 + 20) % target.pxw),
          (f32)((step * 1 + 30) % target.pxh)},
         {(f32)((step * 2 + 40) % target.pxw),
          (f32)((step * 1 + 4) % target.pxh)}},
      };
      swgeo_2tri green[]= {
        {{260.f, 170.f}, {570.f, 430.f}, {100.f, 370.f}},
      };
      swgeo_2tri blue[]= {
        {{-100.f, 180.f}, {180.f, 520.f}, {230.f, 250.f}},
      };

      swrast_tri(moving, 1, target, argb_mask(235, 70, 80, 210));
      swrast_tri(green, 1, target, argb_mask(70, 220, 120, 210));
      swrast_tri(blue, 1, target, argb_mask(70, 120, 245, 210));

      if(dpypresent(dpy, srf, swp) < 0) goto exit;
      frame++;
      frame_count++;

      if(!startup_done)
      {
        printf(
          "startup: %llu ns\n",
          (unsigned long long)(now_ns() - startup_begin));
        startup_done= 1;
      }
    }

    u64 loop_end= now_ns();
    loop_sum+= loop_end - loop_begin;
    loop_count++;

    if(loop_end - report_begin >= UINT64_C(1000000000))
    {
      printf(
        "loop average: %llu ns (%u iterations, %u frames)\n",
        (unsigned long long)(loop_sum / loop_count),
        loop_count,
        frame_count);
      loop_sum    = 0;
      loop_count  = 0;
      frame_count = 0;
      report_begin= loop_end;
    }
  }

  rc= 0;
exit:
  dpyterm_swp(swp);
  dpyterm_srf(dpy, srf);
  dpyterm(dpy);
  return rc;
}
