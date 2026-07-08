#include "include/defs.h"

typedef struct
{
  t3f32 dir, orig;
} rtray;

typedef struct
{
  int   hit;
  int   outside;
  f32   t;
  t3f32 p;
  t3f32 n;
} rtisct;

typedef t4u8 (*rtfn_pxsh)(u32 x, u32 y, u32 w, u32 h, void *);

void rtexec_pxsh(rtfn_pxsh, raster_4u8, void * /*passed into pxsh*/);

typedef rtray (*rtfn_cam)(
  u32 x, u32 y, u32 w, u32 h, t3f32 lookat, t3f32 orig, t3f32 up);

typedef rtisct (*rtfn_isctr)(rtray, void *);

// math.h

typedef struct
{
	t3f32 orig;
	t3f32 i, j, k;
} csys_3;

int orthonormal_3(csys_3);
t3f32 chframe_3(t3f32, csys_3 fro, csys_3 to)
{
}
