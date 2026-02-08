#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#define MIN(a, b)        \
  ({                     \
    __auto_type _a= (a); \
    __auto_type _b= (b); \
    _a < _b ? _a : _b;   \
  })
#define MAX(a, b)        \
  ({                     \
    __auto_type _a= (a); \
    __auto_type _b= (b); \
    _a > _b ? _a : _b;   \
  })

#define CLAMP(x, hi, lo)                    \
  ({                                        \
    __auto_type _x = (x);                   \
    __auto_type _lo= (lo);                  \
    __auto_type _hi= (hi);                  \
    _x < _lo ? _lo : (_x > _hi ? _hi : _x); \
  })

typedef unsigned char byte;

typedef ptrdiff_t isize;
typedef size_t    usize;

typedef int32_t i32;
typedef int16_t i16;
typedef int64_t i64;
typedef int8_t  i8;

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint64_t u64;
typedef uint8_t  u8;

typedef float  f32;
typedef double f64;

typedef struct rgbaf32
{
  f32 r, g, b, a;
} rgbaf32;

typedef struct rgbf32
{
  f32 r, g, b;
} rgbf32;

typedef struct rgb24
{
  byte r, g, b;
} rgb24;
typedef struct rgba32
{
  byte r, g, b, a;
} rgba32;

typedef struct range
{
  f32 lo, hi;
} range;

static const struct range NORM = {.lo= 0.f, .hi= 1.f};
static const struct range CANON= {.lo= -1.f, .hi= 1.f};

enum pxfmt {FMT_RGB, FMT_RGBA, FMT_PLANAR};
struct raster_4u8
{
  rgba32  *buf;
  unsigned stride;  // elements of buf per row
  unsigned pxw, pxh;
  enum pxfmt fmt;
};

struct raster_3u8
{
  rgb24   *buf;
  unsigned stride;  // elements of buf per row
  unsigned pxw, pxh;
  enum pxfmt fmt;
};

struct raster_3f32
{
  rgbf32  *buf;
  unsigned stride;  // elements of buf per row
  unsigned pxw, pxh;
  enum pxfmt fmt;
};

struct raster_4f32
{
  rgbaf32 *buf;
  unsigned stride;  // elements of buf per row
  unsigned pxw, pxh;
  enum pxfmt fmt;
};

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

static inline struct rgbaf32 rgb32c(const struct rgba32 px)
{
  // px values 0..255, map to [0, 1]
  return (struct rgbaf32){px.r * (1.0f / 255.0f),
                          px.g * (1.0f / 255.0f),
                          px.b * (1.0f / 255.0f),
                          px.a * (1.0f / 255.0f)};
}

// convert `n` rgba32 pixels to rgb24
static inline void px32to24(
  const struct rgba32 *restrict px32,
  struct rgb24 *restrict px24,
  unsigned n)
{
  const struct rgba32 *end= px32 + n;
  while(px32 != end)
    *px24++= (struct rgb24){px32->r, px32->g, px32->b}, px32++;
}

typedef struct point2i
{
  signed x, y;
} point2i;
typedef struct span2u
{
  unsigned dx, dy;
} span2u;
typedef struct point2f
{
  f32 x, y;
} point2f;
typedef struct point2u
{
  unsigned x, y;
} point2u;
typedef struct span2f
{
  f32 dx, dy;
} span2f;

typedef struct irect
{
  point2i p;    // position
  span2u  ext;  // extent
} irect;
typedef struct rect
{
  point2f p;
  span2f  ext;
} rect;

// assumes origin at top left
static inline irect isct_irect(irect r1, irect r2)
{
  const signed right1 = r1.p.x + r1.ext.dx;
  const signed right2 = r2.p.x + r2.ext.dx;
  const signed bottom1= r1.p.y + r1.ext.dy;
  const signed bottom2= r2.p.y + r2.ext.dy;

  const point2i tl = {MAX(r1.p.x, r2.p.x), MAX(r1.p.y, r2.p.y)};
  const point2i br = {MIN(right1, right2), MIN(bottom1, bottom2)};
  const span2u  ext= {
    br.x - tl.x > 0 ? (unsigned)(br.x - tl.x) : 0,
    br.y - tl.y > 0 ? (unsigned)(br.y - tl.y) : 0};
  return (irect){tl, ext};
}

typedef rgb24   rgb_u8;
typedef rgba32  rgba_u8;
typedef rgbf32  rgb_f32;
typedef rgbaf32 rgba_f32;

typedef rgb24   t3u8;
typedef rgba32  t4u8;
typedef rgbf32  t3f32;
typedef rgbaf32 t4f32;

