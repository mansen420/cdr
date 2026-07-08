// AI-generated (Claude)
#include "src/rt/rt.c"
#include "src/core/include/io.h"

#include <stdio.h>
#include <stdlib.h>

#define W 640
#define H 480

int main(void)
{
  struct raster_3u8 img= {
    .buf   = calloc(W * H, sizeof(t3u8)),
    .stride= W,
    .pxw   = W,
    .pxh   = H};

  t3f32  cam_pos= {0.f, 0.f, -3.f};
  t3f32  lookat = {0.f, 0.f, 0.f};
  t3f32  up     = {0.f, 1.f, 0.f};
  f32    fov    = 60.f;
  sphere s      = {.c= {0.f, 0.f, 0.f}, .r= 1.f};

  t3u8 red= {255, 50, 50};
  t3u8 bg = {30, 30, 30};

  for(u32 y= 0; y < H; y++)
  {
    for(u32 x= 0; x < W; x++)
    {
      ray  r = pincam((t2u32){x, y}, (t2u32){W, H}, fov, lookat, cam_pos, up);
      isct hi= isct_sphere(r, s);
      img.buf[y * W + x]= hi.hit ? red : bg;
    }
  }

  FILE *f= fopen("sphere.ppm", "wb");
  fwritebppm(f, img.buf, W, H);
  fclose(f);
  free(img.buf);
  return 0;
}
