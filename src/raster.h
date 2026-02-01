#pragma once

#include "defs.h"

//`out` must point to in.pxw*in.pxh*bpp(in)*mag^2 bytes
//and not overlap with in->buf
struct raster_4u8 upsample_4u8(const struct raster_4u8 *in,
                           struct rgba32 *restrict out,
                           uint mag);
struct raster_3u8 upsample_3u8(const struct raster_3u8 *in,
                           struct rgb24 *restrict out,
                           uint mag);
