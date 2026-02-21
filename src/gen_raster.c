#include <string.h>

#include "include/defs.h"

enum rfmt
{
  FMT_RGB24,
  FMT_RGBA32
};
struct raster
{
  // offset view into the buffer
  byte *buf;
  // bytes per row of the underlying view
  unsigned stride;
  // width and height in pixels
  unsigned pxw, pxh;
  // data format
  enum rfmt fmt;
};

static inline unsigned bytespp(enum rfmt f)
{
  switch(f)
  {
    case FMT_RGB24:
      return 3;
    case FMT_RGBA32:
      return 4;
    default:
      return 0;
  }
}

// returns address of pixel at specified row and column
// works with offsets
static inline byte *at(
  const struct raster *r, unsigned row, unsigned col)
{
  return r->buf + (r->stride * row + col * bytespp(r->fmt));
}

//`out` must point to in.pxw*in.pxh*bpp(in)*mag^2 bytes
struct raster upsample(
  const struct raster *in, byte *restrict out, unsigned mag)
{
  const unsigned bpp      = bytespp(in->fmt);
  const byte *restrict src= in->buf;

  struct raster res= {
    out, in->pxw * bpp * mag, in->pxw * mag, in->pxh * mag, in->fmt};
  if(mag == 0) return res;
  if(mag == 1)
  {
    if(in->stride == in->pxw * bpp)  // tight packing
      memcpy(out, src, in->pxh * in->pxw * bpp);
    else
      for(unsigned i= 0; i < in->pxh; i++)
        memcpy(out, src + in->stride * i, in->pxw * bpp),
          out+= in->pxw * bpp;
    return res;
  }

  for(unsigned i= 0; i < in->pxh; i++)
  {
    byte *out_rowstart= out;
    for(unsigned j= 0; j < in->pxw; j++)
      // emit horizontal magnified row
      for(unsigned kx= 0; kx < mag; kx++)
      {
        const byte *srcpx= (src + in->stride * i + j * bpp);
        for(unsigned l= 0; l < bpp; l++) *out++= srcpx[l];
      }
    ptrdiff_t rowlen= out - out_rowstart;
    for(unsigned ky= 0; ky < mag - 1; ky++)
      memcpy(out, out_rowstart, rowlen), out+= rowlen;
  }
  return res;
}
