#pragma once

#include "include/defs.h"

//`out` must point to in.pxw*in.pxh*bpp(in)*mag^2 bytes
// and not overlap with in->buf
struct raster_4u8 upsample_4u8(
  const struct raster_4u8 in, t4u8 *restrict out, unsigned mag);
struct raster_3u8 upsample_3u8(
  const struct raster_3u8 in, t3u8 *restrict out, unsigned mag);

// downsample with box. acc should have dst.pxw*sizeof(px_t)
// u32's.
// the last src.pxw%min rows and src.pxh%columns are
// ignored
void dnsamplebx_4u8(
  const struct raster_4u8 src,
  struct raster_4u8       dst,
  unsigned                min,
  u32 *restrict acc);

// downsample with box. acc should have dst.pxw*sizeof(px_t)
// u32's.
// the last src.pxw%min rows and src.pxh%columns are
// ignored
void dnsamplebx_3u8(
  const struct raster_3u8 src,
  struct raster_3u8       dst,
  unsigned                min,
  u32 *restrict acc);

// converts from RGBA format to [RR...GG...BB...AA] format
// both buffers must be at least n * 4 bytes large
static inline void rgba2planar(
  const t4u8 *restrict rgba, t4u8 *restrict out, usize n)
{
  byte *restrict outb= (byte *)out;
  for(usize i= 0; i < n; i++)
  {
    *(outb + i)            = (rgba + i)->r;
    *(outb + i + n)        = (rgba + i)->g;
    *(outb + i + n + n)    = (rgba + i)->b;
    *(outb + i + n + n + n)= (rgba + i)->a;
  }
}
