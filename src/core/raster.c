#include "src/core/include/raster.h"

#include <string.h>

#include "src/core/include/utils.h"

#define raster_t struct raster_4u8
#define px_t t4u8
// raster_t rsubview_4u8(const raster_t r, const irect view)
//{
//   const irect bounds = {{0}, {r.pxw, r.pxh}};
//   const irect clipped= isct_irect(bounds, view);
//   if(clipped.ext.dx == 0 || clipped.ext.dy == 0)
//     return (raster_t){r.buf, r.stride, 0, 0};
//   return (raster_t){r.buf + clipped.p.y * r.stride + clipped.p.x,
//                     r.stride,
//                     clipped.ext.dx,
//                     clipped.ext.dy};
// }
//
// void blit_4u8(
//   const raster_t src, raster_t dst, irect srect, point2i dpos);

void copy_4u8(const raster_t src, raster_t dst)
{
  if(dst.pxw < src.pxw || dst.pxh < src.pxh)
    return;  // consider returning opres

  if(
    src.stride == src.pxw && dst.stride == dst.pxw &&
    src.stride == dst.stride)
  {
    memcpy(dst.buf, src.buf, src.stride * src.pxh * sizeof(px_t));
    return;
  }

  const px_t *const end      = src.buf + (src.pxh * src.stride);
  const px_t *restrict srcrow= src.buf;
  px_t *restrict dstrow      = dst.buf;
  for(; srcrow != end; srcrow+= src.stride, dstrow+= dst.stride)
    memcpy(dstrow, srcrow, src.pxw * sizeof(px_t));
}

raster_t upsample_4u8(
  const raster_t in, px_t *restrict out, unsigned mag)
{
  const px_t *restrict src= in.buf;

  raster_t res= {
    out, in.pxw * mag, in.pxw * mag, in.pxh * mag, FMT_RGBA};
  if(mag == 0) return res;
  if(mag == 1)
  {
    if(in.stride == in.pxw)  // tight packing
      memcpy(out, src, in.pxh * in.pxw * sizeof(px_t));
    else
      for(unsigned i= 0; i < in.pxh; i++)
        memcpy(out, src + in.stride * i, in.pxw * sizeof(px_t)),
          out+= in.pxw;
    return res;
  }

  for(unsigned i= 0; i < in.pxh; i++)
  {
    px_t *out_rowstart= out;
    for(unsigned j= 0; j < in.pxw; j++)
    {
      const px_t srcpx= *(src + in.stride * i + j);
      // emit horizontal magnified row
      for(unsigned kx= 0; kx < mag; kx++) *out++= srcpx;
    }
    ptrdiff_t rowlen= out - out_rowstart;
    for(unsigned ky= 0; ky < mag - 1; ky++)
      memcpy(out, out_rowstart, rowlen * sizeof(px_t)), out+= rowlen;
  }
  return res;
}

// downsample with nearest neighbor
void dnsamplenn_4u8(const raster_t src, raster_t dst, unsigned min);
// downsample with box. acc should have dst.pxw*sizeof(px_t)
// u32's.
// the last src.pxw%min rows and src.pxh%columns are
// ignored
void dnsamplebx_4u8(
  const raster_t src, raster_t dst, unsigned min, u32 *restrict acc)
{
  if(min == 0) return;
  if(dst.pxh < src.pxh / min || dst.pxw < src.pxw / min) return;

  if(min == 1)
  {
    copy_4u8(src, dst);
    return;
  }

  const px_t *restrict sptr= src.buf;
  px_t *restrict dptr      = dst.buf;

  const static unsigned nr_chn= 4;
  for(unsigned quo    = src.pxh / min,
               w_out  = src.pxw / min,
               nr_accs= w_out * nr_chn,
               i      = 0;
      i < quo;
      i++)
  {
    memset(acc, 0, nr_accs * sizeof *acc);
    for(unsigned j= 0; j < min; j++)
    {
      const px_t *rptr= sptr + (i * min + j) * src.stride;
      for(unsigned k= 0; k < w_out; k++)
        for(unsigned l= 0; l < min; l++)
        {
          // XXX this part of the loop is type
          // dependent!
          *(acc + k * nr_chn + 0)+= (rptr + k * min + l)->r;
          *(acc + k * nr_chn + 1)+= (rptr + k * min + l)->g;
          *(acc + k * nr_chn + 2)+= (rptr + k * min + l)->b;
          *(acc + k * nr_chn + 3)+= (rptr + k * min + l)->a;
          //
        }
    }
    // divide each channel by min
    // TODO consider rounding instead of truncation
    vdivs_u32(acc, nr_accs, min * min);
    // write each channel back
    px_t *drowptr= dptr + i * dst.stride;
    for(unsigned k= 0; k < w_out; k++)
    {
      // XXX type dependent block
      (drowptr + k)->r= *(acc + k * nr_chn + 0);
      (drowptr + k)->g= *(acc + k * nr_chn + 1);
      (drowptr + k)->b= *(acc + k * nr_chn + 2);
      (drowptr + k)->a= *(acc + k * nr_chn + 3);
      //
    }
  }
}
#undef raster_t
#undef px_t
#define raster_t struct raster_3u8
#define px_t t3u8
// TODO change this to rcopy_3u8
void copy_3u8(const raster_t src, raster_t dst)
{
  if(dst.pxw < src.pxw || dst.pxh < src.pxh)
    return;  // consider returning opres

  if(
    src.stride == src.pxw && dst.stride == dst.pxw &&
    src.stride == dst.stride)
  {
    memcpy(dst.buf, src.buf, src.stride * src.pxh * sizeof(px_t));
    return;
  }

  const px_t *const end      = src.buf + (src.pxh * src.stride);
  const px_t *restrict srcrow= src.buf;
  px_t *restrict dstrow      = dst.buf;
  for(; srcrow != end; srcrow+= src.stride, dstrow+= dst.stride)
    memcpy(dstrow, srcrow, src.pxw * sizeof(px_t));
}

raster_t upsample_3u8(
  const raster_t in, px_t *restrict out, unsigned mag)
{
  const px_t *restrict src= in.buf;

  raster_t res= {
    out, in.pxw * mag, in.pxw * mag, in.pxh * mag, FMT_RGB};
  if(mag == 0) return res;
  if(mag == 1)
  {
    if(in.stride == in.pxw)  // tight packing
      memcpy(out, src, in.pxh * in.pxw * sizeof(px_t));
    else
      for(unsigned i= 0; i < in.pxh; i++)
        memcpy(out, src + in.stride * i, in.pxw * sizeof(px_t)),
          out+= in.pxw;
    return res;
  }

  for(unsigned i= 0; i < in.pxh; i++)
  {
    px_t *out_rowstart= out;
    for(unsigned j= 0; j < in.pxw; j++)
    {
      const px_t srcpx= *(src + in.stride * i + j);
      // emit horizontal magnified row
      for(unsigned kx= 0; kx < mag; kx++) *out++= srcpx;
    }
    ptrdiff_t rowlen= out - out_rowstart;
    for(unsigned ky= 0; ky < mag - 1; ky++)
      memcpy(out, out_rowstart, rowlen * sizeof(px_t)), out+= rowlen;
  }
  return res;
}

// downsample with box. acc should have dst.pxw*sizeof(px_t)
// u32's.
// the last src.pxw%min rows and src.pxh%columns are
// ignored
void dnsamplebx_3u8(
  const raster_t src, raster_t dst, unsigned min, u32 *restrict acc)
{
  if(min == 0) return;
  if(dst.pxh < src.pxh / min || dst.pxw < src.pxw / min) return;

  if(min == 1)
  {
    copy_3u8(src, dst);
    return;
  }

  const px_t *restrict sptr= src.buf;
  px_t *restrict dptr      = dst.buf;

  // XXX
  const static unsigned nr_chn= 3;
  for(unsigned quo    = src.pxh / min,
               w_out  = src.pxw / min,
               nr_accs= w_out * nr_chn,
               i      = 0;
      i < quo;
      i++)
  {
    memset(acc, 0, nr_accs * sizeof *acc);
    for(unsigned j= 0; j < min; j++)
    {
      const px_t *rptr= sptr + (i * min + j) * src.stride;
      for(unsigned k= 0; k < w_out; k++)
        for(unsigned l= 0; l < min; l++)
        {
          // XXX this part of the loop is type
          // dependent!
          *(acc + k * nr_chn + 0)+= (rptr + k * min + l)->r;
          *(acc + k * nr_chn + 1)+= (rptr + k * min + l)->g;
          *(acc + k * nr_chn + 2)+= (rptr + k * min + l)->b;
          //
        }
    }
    // divide each channel by min
    // TODO consider rounding instead of truncation
    vdivs_u32(acc, nr_accs, min * min);
    // write each channel back
    px_t *drowptr= dptr + i * dst.stride;
    for(unsigned k= 0; k < w_out; k++)
    {
      // XXX type dependent block
      (drowptr + k)->r= *(acc + k * nr_chn + 0);
      (drowptr + k)->g= *(acc + k * nr_chn + 1);
      (drowptr + k)->b= *(acc + k * nr_chn + 2);
      //
    }
  }
}
#undef raster_t
