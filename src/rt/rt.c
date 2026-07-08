#include "include/defs.h"
#include "src/core/include/math.h"
#include "src/rt/include/rt.h"

typedef rtray  ray;
typedef rtisct isct;

typedef struct
{
} rtctx;

typedef struct
{
  t2f32 bases_len;
} desc_cam;

static ray pincam(
  t2u32 xy, t2u32 wh, f32 fov, t3f32 lookat, t3f32 pos_c, t3f32 up)
{
  f32 ar    = (f32)wh.x / wh.y;
  f32 h_half= tanf(fov * (3.14159265f / 360.f));  // half height at distance 1
  f32 w_half= ar * h_half;

  // compute camera local coordinate space

  // if forward_c is parallel to world up, you get a zerovec here
  t3f32 forward_c= norm_3(sub_3(lookat, pos_c));
  t3f32 right_c  = norm_3(cross(up, forward_c));
  t3f32 up_c     = norm_3(cross(forward_c, right_c));

  // (x, y) mapping, (0, 0) is bottom left
  t3f32 pxcen= {((f32)xy.x + 0.5f) / wh.x, ((f32)xy.y + 0.5f) / wh.y, 0.f};
  pxcen      = linmap_3(pxcen, (t2f32){0, 1}, (t2f32){-1, 1});
  pxcen      = mul_3(pxcen, (t3f32){w_half, h_half, 0.f});

  t3f32 target= fma_3(right_c, pxcen.x, fma_3(up_c, pxcen.y, forward_c));

  return (ray){.dir= norm_3(target), .orig= pos_c};
}

typedef struct
{
  t3f32 c;
  f32   r;
} sphere;

static isct isct_sphere(ray r, sphere s, csys_3 objspace)
{
  t3f32 oc= sub_3(r.orig, s.c);
  f32   a = dot_3(r.dir, r.dir);
  f32   b = 2.f * dot_3(oc, r.dir);
  f32   c = dot_3(oc, oc) - s.r * s.r;
  f32   d = b * b - 4.f * a * c;

  if(d < 0.f) return (isct){0};

  f32 sd= sqrtf(d);
  f32 t0= (-b - sd) / (2.f * a);
  f32 t1= (-b + sd) / (2.f * a);

  f32 t= -1.f;
  if(t0 > 0.f)
    t= t0;
  else if(t1 > 0.f)
    t= t1;
  else
    return (isct){0};
	
	// clipped sphere

  t3f32 pos    = fma_3(r.dir, t, r.orig);
  t3f32 norm   = norm_3(sub_3(pos, s.c));
  int   outside= dot_3(r.dir, norm) < 0.f;
  if(!outside) norm= muls_3(norm, -1.f);
  return (isct){.hit= 1, .outside= outside, .t= t, .p= pos, .n= norm};
}
