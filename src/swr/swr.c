#include "include/swr.h"
#include "src/core/include/debug.h"
#include "src/core/include/math.h"

// evaluate p on the edge between i and j
static inline f32 edge(t2f32 i, t2f32 j, t2f32 p)
{
  return (i.y - j.y) * p.x + (j.x - i.x) * p.y + (i.x * j.y) - (j.x * i.y);
}
// writes mask into raster where the triangle covers
// coordinate space is: [(0, 0), (pxw - 1, pxh - 1)]. (0, 0) at top left corner,
// positive x right, positive y down. (0.0, 0.0) is the center of pixel (0, 0)
static void rasterize_tri(t2f32 tri[3], raster_4u8 r, t4u8 mask)
{
  NNUL(r.buf && r.pxh && r.pxw, return;)

  f32 p21= edge(
    (t2f32){tri[2].x, tri[2].y},
    (t2f32){tri[1].x, tri[1].y},
    (t2f32){tri[0].x, tri[0].y});
  f32 p01= edge(
    (t2f32){tri[0].x, tri[0].y},
    (t2f32){tri[1].x, tri[1].y},
    (t2f32){tri[2].x, tri[2].y});
  f32 p20= edge(
    (t2f32){tri[2].x, tri[2].y},
    (t2f32){tri[0].x, tri[0].y},
    (t2f32){tri[1].x, tri[1].y});
  // invalid geometry test
  if(zero_f32(p20) || zero_f32(p01) || zero_f32(p21)) return;

  u32 x_max, x_min, y_max, y_min;
  {
    f32 tx_max= tri[0].x;
    if(tri[1].x > tx_max) tx_max= tri[1].x;
    if(tri[2].x > tx_max) tx_max= tri[2].x;
    f32 ty_max= tri[0].y;
    if(tri[1].y > ty_max) ty_max= tri[1].y;
    if(tri[2].y > ty_max) ty_max= tri[2].y;

    tx_max= min_f32(tx_max, (f32)(r.pxw - 1));
    ty_max= min_f32(ty_max, (f32)(r.pxh - 1));

    f32 tx_min= tri[0].x;
    if(tri[1].x < tx_min) tx_min= tri[1].x;
    if(tri[2].x < tx_min) tx_min= tri[2].x;
    f32 ty_min= tri[0].y;
    if(tri[1].y < ty_min) ty_min= tri[1].y;
    if(tri[2].y < ty_min) ty_min= tri[2].y;

    tx_min= max_f32(tx_min, 0.f);
    ty_min= max_f32(ty_min, 0.f);

    if(tx_min > tx_max || ty_min > ty_max) return;

    x_min= ceil(tx_min), x_max= floor(tx_max);
    y_min= ceil(ty_min), y_max= floor(ty_max);
  }

  for(u32 y= y_min; y <= y_max; y++)
  {
    t4u8 *row= r.buf + (r.stride * y);
    for(u32 x= x_min; x <= x_max; x++)
    {
      f32 alp, bet, gam;
      alp= edge(
             (t2f32){tri[2].x, tri[2].y},
             (t2f32){tri[1].x, tri[1].y},
             (t2f32){x, y}) /
           p21;
      bet= edge(
             (t2f32){tri[2].x, tri[2].y},
             (t2f32){tri[0].x, tri[0].y},
             (t2f32){x, y}) /
           p20;
      gam= edge(
             (t2f32){tri[0].x, tri[0].y},
             (t2f32){tri[1].x, tri[1].y},
             (t2f32){x, y}) /
           p01;
      if(alp < 0.f || bet < 0.f || gam < 0.f) continue;

      row[x]= mask;
    }
  }
}

void swrast_tri(swgeo_2tri *tris, u32 tricnt, raster_4u8 r, t4u8 mask)
{
  NNUL(tris, return;)

  for(u32 i= 0; i < tricnt; i++) rasterize_tri(tris[i], r, mask);
}
