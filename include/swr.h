#pragma once

#include "include/defs.h"

typedef t2f32 swgeo_2tri[3];

void swrast_tri(swgeo_2tri *tris, u32 tricnt, raster_4u8 r, t4u8 mask);
