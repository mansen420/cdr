#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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
  f32    *zmem= NULL;
  rasterf zbuf= {0};

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
      fill(img, UINT32_C(0x00181a20));

      raster_4u8 target= {
        .buf   = img.data,
        .stride= (u32)(img.stride / sizeof(t4u8)),
        .pxw   = (u32)img.pxw,
        .pxh   = (u32)img.pxh};

      if(!zmem)
      {
        zmem= malloc(target.pxw * target.pxh * sizeof *zmem);
        if(!zmem) goto exit;
        zbuf= (rasterf){
          .buf= zmem, .stride= target.pxw, .pxw= target.pxw, .pxh= target.pxh};
      }
      for(usize i= 0; i < (usize)zbuf.pxw * zbuf.pxh; i++)
        zmem[i]= -INFINITY;

      static const t3f32 vertices[8]= {
        {-1.f, -1.f, -1.f},
        {1.f, -1.f, -1.f},
        {1.f, 1.f, -1.f},
        {-1.f, 1.f, -1.f},
        {-1.f, -1.f, 1.f},
        {1.f, -1.f, 1.f},
        {1.f, 1.f, 1.f},
        {-1.f, 1.f, 1.f},
      };
      static const u8 indices[12][3]= {
        {0, 2, 1}, {0, 3, 2},  // back
        {4, 5, 6}, {4, 6, 7},  // front
        {0, 4, 7}, {0, 7, 3},  // left
        {1, 2, 6}, {1, 6, 5},  // right
        {3, 7, 6}, {3, 6, 2},  // top
        {0, 1, 5}, {0, 5, 4},  // bottom
      };
      f32 bounds[3][2]= {
        {1.f, -1.f}, {.75f, -.75f}, {-1.f, -20.f}};
      t3f32 campos= {3.f, 2.f, 4.f};
      t3f32 lookat= {0.f, 0.f, 0.f};
      t3f32 up    = {0.f, 1.f, 0.f};

      t3f32 screenverts[8];
      for(u32 i= 0; i < 8; i++)
      {
        t4f32 p= {vertices[i].x, vertices[i].y, vertices[i].z, 1.f};
        p= swxform_cam(campos, lookat, up, p);
        p= swxform_ndc(bounds, p);
        p= swxform_scrspace(target.pxw, target.pxh, p);
        screenverts[i]= (t3f32){p.x, p.y, p.z};
      }

      swgeo_3tri cube[12];
      for(u32 tri= 0; tri < 12; tri++)
        for(u32 vert= 0; vert < 3; vert++)
          cube[tri][vert]= screenverts[indices[tri][vert]];

      swrast_tri(cube, 12, target, zbuf, argb_mask(255, 255, 255, 255));

      if(dpypresent(dpy, srf, swp) < 0) goto exit;
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
  free(zmem);
  dpyterm_swp(swp);
  dpyterm_srf(dpy, srf);
  dpyterm(dpy);
  return rc;
}
