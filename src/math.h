#pragma once

#include <math.h>

#include "defs.h"

typedef struct range
{
  f32 lo, hi;
} range;

static const struct range NORM = {.lo= 0.f, .hi= 1.f};
static const struct range CANON= {.lo= -1.f, .hi= 1.f};

static inline f32 span(struct range r) { return r.hi - r.lo; }

static inline int zeroeps(f32 f, f32 eps) { return fabsf(f) <= eps; }
static inline int zero(f32 f) { return zeroeps(f, 1e-8f); }

static inline f32 linmap(f32 x, struct range fro, struct range to)
{
  if(zero(span(fro))) return to.lo;
  return span(to) / span(fro) * (x - fro.lo) + to.lo;
}
static inline void linmapf3(
  const f32 in[3], f32 out[3], struct range fro, struct range to)
{
  out[0]= linmap(in[0], fro, to);
  out[1]= linmap(in[1], fro, to);
  out[2]= linmap(in[2], fro, to);
}

static inline t4f32 rgb32c(const t4u8 px)
{
  // px values 0..255, map to [0, 1]
  return (t4f32){px.r * (1.0f / 255.0f),
                 px.g * (1.0f / 255.0f),
                 px.b * (1.0f / 255.0f),
                 px.a * (1.0f / 255.0f)};
}

// convert `n` t4u8 pixels to t3u8
static inline void px32to24(
  const t4u8 *restrict px32, t3u8 *restrict px24, unsigned n)
{
  const t4u8 *end= px32 + n;
  while(px32 != end) *px24++= (t3u8){px32->r, px32->g, px32->b}, px32++;
}

typedef struct irect
{
  t2i8 p;    // position
  t2u8 ext;  // extent
} irect;
typedef struct rect
{
  t2i8 p;
  t2u8 ext;
} rect;

// assumes origin at top left
static inline irect isct_irect(irect r1, irect r2)
{
  const signed right1 = r1.p.x + r1.ext.x;
  const signed right2 = r2.p.x + r2.ext.x;
  const signed bottom1= r1.p.y + r1.ext.y;
  const signed bottom2= r2.p.y + r2.ext.y;

  const t2i8 tl = {MAX(r1.p.x, r2.p.x), MAX(r1.p.y, r2.p.y)};
  const t2i8 br = {MIN(right1, right2), MIN(bottom1, bottom2)};
  const t2u8 ext= {
    br.x - tl.x > 0 ? (unsigned)(br.x - tl.x) : 0,
    br.y - tl.y > 0 ? (unsigned)(br.y - tl.y) : 0};
  return (irect){tl, ext};
}
