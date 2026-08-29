#pragma once

#include "include/defs.h"

typedef t3f32 swgeo_3tri[3];

t4f32 swxform_cam(t3f32 campos, t3f32 lookat, t3f32 up, t4f32 p);
t4f32 swxform_ndc(f32 bounds[3][2], t4f32 p);
t4f32 swxform_scrspace(u32 pxw, u32 pxh, t4f32 p);

// zbuf must cover the color raster and be cleared by the caller before the
// first draw of each frame. With the current depth convention, clear to
// -INFINITY.
void swrast_tri(
  swgeo_3tri *tris, u32 tricnt, raster_4u8 r, rasterf zbuf, t4u8 mask);
