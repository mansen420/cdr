#include "include/swr.h"
#include "src/core/include/debug.h"
#include "src/core/include/math.h"

// TODO: eventually migrate the t{1-4}{f/u/i} types into array accessors, like
// typedef float[4] t4f32
// and keep vector types with xyzw accessors.
typedef t4f32 vec4f;
typedef vec4f mat4f[4];
typedef t3f32 vec3f;
typedef t2f32 vec2f;

static vec4f matmulv4(mat4f m, vec4f v)
{
  return (vec4f){dot4(m[0], v), dot4(m[1], v), dot4(m[2], v), dot4(m[3], v)};
}

static void matmul4(mat4f out, mat4f a, mat4f b)
{
  mat4f res= {0};

  for(u32 row= 0; row < 4; row++)
  {
    f32 *dst= (f32 *)&res[row];
    for(u32 col= 0; col < 4; col++)
    {
      const f32 *lhs= (const f32 *)&a[row];
      for(u32 k= 0; k < 4; k++)
      {
        const f32 *rhs= (const f32 *)&b[k];
        dst[col]+= lhs[k] * rhs[col];
      }
    }
  }

  for(u32 row= 0; row < 4; row++) out[row]= res[row];
}

// TOOD: eventually, transforms, coordinate systems, and matrix operations
// should be core math utils

// transforms p from view-space (bounds), into NDC
// bounds are right, left, top, bottom, near, far
vec4f swxform_ndc(f32 bounds[3][2], vec4f p)
{
  f32 r= bounds[0][0], l= bounds[0][1], t= bounds[1][0], b= bounds[1][1],
      n= bounds[2][0], f= bounds[2][1];
  // malformed geometry. watch out for numerical issues
  if(r - l <= 0 || t - b <= 0 || n - f <= 0) return (vec4f){0};

  f32   rl= 1.f / (r - l), tb= 1.f / (t - b), nf= 1.f / (n - f);
  mat4f per= {
    {n, 0.f, 0.f, 0.f},
    {0.f, n, 0.f, 0.f},
    {0.f, 0.f, (n + f), -f * n},
    {0.f, 0.f, 1.f, 0.f}};
  mat4f ortho= {
    {2.f * rl, 0.f, 0.f, (r + l) * -rl},
    {0.f, 2.f * tb, 0.f, (t + b) * -tb},
    {0.f, 0.f, 2.f * nf, (n + f) * -nf},
    {0.f, 0.f, 0.f, 1.f}};
  mat4f projection;
  matmul4(projection, ortho, per);

  vec4f clip= matmulv4(projection, p);
  if(zerof(clip.w)) return (vec4f){0};
  return (vec4f){clip.x / clip.w, clip.y / clip.w, clip.z / clip.w, 1.f};
}

// maps NDC into raster coordinates with a top-left origin
vec4f swxform_scrspace(u32 pxw, u32 pxh, vec4f p)
{
  if(!pxw || !pxh) return (vec4f){0};

  return (vec4f){
    (p.x + 1.f) * .5f * (f32)(pxw - 1),
    (1.f - p.y) * .5f * (f32)(pxh - 1),
    p.z,
    p.w};
}
vec4f swxform_cam(vec3f campos, vec3f lookat, vec3f up, vec4f p)
{
  vec3f u, v, w;
  vec3f gaze= sub3(lookat, campos);

  if(zero3(gaze)) return (vec4f){0};

  w= neg3(norm3(gaze));
  u= cross(up, w);
  if(zero3(u)) return (vec4f){0};  // up and w are parallel here
  u= norm3(u);
  v= cross(w, u);

  mat4f translate= {
    {1.f, 0.f, 0.f, -campos.x},
    {0.f, 1.f, 0.f, -campos.y},
    {0.f, 0.f, 1.f, -campos.z},
    {0.f, 0.f, 0.f, 1.f}};
  mat4f changebasis= {
    {u.x, u.y, u.z, 0.f},
    {v.x, v.y, v.z, 0.f},
    {w.x, w.y, w.z, 0.f},
    {0.f, 0.f, 0.f, 1.f}};
  mat4f xform= {0};
  matmul4(xform, changebasis, translate);
  return matmulv4(xform, p);
}
// evaluate p on the edge between i and j
static inline f32 edge(t2f32 i, t2f32 j, t2f32 p)
{
  return (i.y - j.y) * p.x + (j.x - i.x) * p.y + (i.x * j.y) - (j.x * i.y);
}
// writes mask into raster where the triangle covers
// coordinate space is: [(0, 0), (pxw - 1, pxh - 1)]. (0, 0) at top left corner,
// positive x right, positive y down. (0.0, 0.0) is the center of pixel (0, 0)
// triangle coordinates are {x_scr, y_scr, z_clip}
static void rasterize_tri(t3f32 tri[3], raster_4u8 r, t4u8 mask, rasterf zbuf)
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
  if(zerof(p20) || zerof(p01) || zerof(p21)) return;

  u32 x_max, x_min, y_max, y_min;
  {
    f32 tx_max= tri[0].x;
    if(tri[1].x > tx_max) tx_max= tri[1].x;
    if(tri[2].x > tx_max) tx_max= tri[2].x;
    f32 ty_max= tri[0].y;
    if(tri[1].y > ty_max) ty_max= tri[1].y;
    if(tri[2].y > ty_max) ty_max= tri[2].y;

    tx_max= minf(tx_max, (f32)(r.pxw - 1));
    ty_max= minf(ty_max, (f32)(r.pxh - 1));

    f32 tx_min= tri[0].x;
    if(tri[1].x < tx_min) tx_min= tri[1].x;
    if(tri[2].x < tx_min) tx_min= tri[2].x;
    f32 ty_min= tri[0].y;
    if(tri[1].y < ty_min) ty_min= tri[1].y;
    if(tri[2].y < ty_min) ty_min= tri[2].y;

    tx_min= maxf(tx_min, 0.f);
    ty_min= maxf(ty_min, 0.f);

    if(tx_min > tx_max || ty_min > ty_max) return;

    x_min= ceil(tx_min), x_max= floor(tx_max);
    y_min= ceil(ty_min), y_max= floor(ty_max);
  }

  for(u32 y= y_min; y <= y_max; y++)
  {
    t4u8 *row = r.buf + (r.stride * y);
    f32  *zrow= zbuf.buf + (zbuf.stride * y);
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
      // compute z value for current point
      f32 depth= alp * tri[0].z + bet * tri[1].z + gam * tri[2].z;
      if(depth < zrow[x]) continue;
      zrow[x]= depth;
      // testing
      mask.x= alp * 255;
      mask.y= bet * 255;
      mask.z= gam * 255;
      row[x]= mask;
    }
  }
}

void swrast_tri(
  swgeo_3tri *tris, u32 tricnt, raster_4u8 r, rasterf zbuf, t4u8 mask)
{
  NNUL(tris && zbuf.buf, return;)
  if(zbuf.pxw < r.pxw || zbuf.pxh < r.pxh || zbuf.stride < r.pxw) return;

  for(u32 i= 0; i < tricnt; i++) rasterize_tri(tris[i], r, mask, zbuf);
}
