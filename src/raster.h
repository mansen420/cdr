#pragma once

#include "defs.h"

static inline uint bytespp(enum rfmt f)
{
    switch (f) 
    {
        case FMT_RGB24:
            return 3;
        case FMT_RGBA32:
            return 4;
        default:
            return 0;
    }
}

//returns address of pixel at specified row and column
//works with offsets
static inline byte* at(const struct raster *r, uint row, uint col)
{
    return r->buf + (r->stride*row + col*bytespp(r->fmt));
}
//`out` must point to in.pxw*in.pxh*bpp(in)*mag^2 bytes
struct raster upsample(const struct raster *in, byte *out, uint mag);
