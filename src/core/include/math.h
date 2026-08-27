#include <math.h>
#include "include/defs.h"

static inline f32 dot_3(t3f32 u, t3f32 v)
{
  return u.x * v.x + u.y * v.y + u.z * v.z;
}

static inline t3f32 cross(t3f32 u, t3f32 v)
{
  return (t3f32){
    .x= u.y * v.z - u.z * v.y,
    .y= u.z * v.x - u.x * v.z,
    .z= u.x * v.y - u.y * v.x};
}

static const t2f32 RANGE_NORM = {0.f, 1.f};
static const t2f32 RANGE_CANON= {-1.f, 1.f};

static inline int zero_f32eps(f32 f, f32 eps) { return fabsf(f) <= eps; }
static inline int zero_f32(f32 f) { return zero_f32eps(f, 1e-8f); }
static inline int zero_3(t3f32 u) { return zero_f32(dot_3(u, u)); }
static inline f32 min_f32(f32 a, f32 b) { return a < b ? a : b; }
static inline f32 max_f32(f32 a, f32 b) { return a > b ? a : b; }

static inline f32 linmap_f32(f32 x, t2f32 fro, t2f32 to)
{
  f32 denom= fro.y - fro.x;
  if(zero_f32(denom)) return to.x;
  return (to.y - to.x) / (denom) * (x - fro.x) + to.x;
}

static inline t3f32 linmap_3(t3f32 x, t2f32 fro, t2f32 to)
{
  return (t3f32){
    .x= linmap_f32(x.x, fro, to),
    .y= linmap_f32(x.y, fro, to),
    .z= linmap_f32(x.z, fro, to)};
}

static inline f32 mag_3(t3f32 u)
{
  return sqrtf(u.x * u.x + u.y * u.y + u.z * u.z);
}

static inline t3f32 norm_3(t3f32 u)
{
  f32 mag= mag_3(u);
  return (t3f32){.x= u.x / mag, .y= u.y / mag, .z= u.z / mag};
}

static inline t3f32 add_3(t3f32 u, t3f32 v)
{
  return (t3f32){.x= u.x + v.x, .y= u.y + v.y, .z= u.z + v.z};
}

static inline t3f32 sub_3(t3f32 u, t3f32 v)
{
  return (t3f32){.x= u.x - v.x, .y= u.y - v.y, .z= u.z - v.z};
}

static inline t3f32 mul_3(t3f32 u, t3f32 v)
{
  return (t3f32){.x= u.x * v.x, .y= u.y * v.y, .z= u.z * v.z};
}

static inline t3f32 div_3(t3f32 u, t3f32 v)
{
  return (t3f32){.x= u.x / v.x, .y= u.y / v.y, .z= u.z / v.z};
}

static inline t3f32 adds_3(t3f32 u, f32 s)
{
  return (t3f32){.x= u.x + s, .y= u.y + s, .z= u.z + s};
}

static inline t3f32 subs_3(t3f32 u, f32 s)
{
  return (t3f32){.x= u.x - s, .y= u.y - s, .z= u.z - s};
}

static inline t3f32 muls_3(t3f32 u, f32 s)
{
  return (t3f32){.x= u.x * s, .y= u.y * s, .z= u.z * s};
}

static inline t3f32 divs_3(t3f32 u, f32 s)
{
  return (t3f32){.x= u.x / s, .y= u.y / s, .z= u.z / s};
}

static inline t3f32 fma_3(t3f32 a, f32 s, t3f32 b)
{
  return (t3f32){
    .x= fmaf(a.x, s, b.x), .y= fmaf(a.y, s, b.y), .z= fmaf(a.z, s, b.z)};
}

static inline t3f32 perm_3(t3f32 u, t3u32 idx)
{
  const f32 *v= &u.x;
  return (t3f32){.x= v[idx.x], .y= v[idx.y], .z= v[idx.z]};
}
