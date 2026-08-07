#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "io.h"
#include "raster.h"
#include "utils.h"

int writeppm(const char *path, t3u8 *buf, unsigned w, unsigned h)
{
  return fw_ppm(path, (struct raster_3u8){buf, w, w, h});
}
void drawimg(struct raster_3u8 img)
{
  for(unsigned i= 0; i < img.pxh; i++)
  {
    t3u8 pattern= {(f32)(i + 1) / img.pxh * 255, 112, 80};
    memfill(
      img.buf + img.stride * i, &pattern, sizeof pattern, img.pxw);
  }
}
int main(int argc, char *argv[])
{
  // src img
  struct raster_3u8 img;
  {
    const unsigned w= 1280, h= 720;

    t3u8 *buf= malloc(w * h * sizeof *buf);
    img      = (struct raster_3u8){buf, w, w, h};
    drawimg(img);
    writeppm("original.ppm", buf, w, h);
  }

  struct raster_3u8 readimg;
  {
    unsigned w, h;
    int      status= fqry_ppmdim("original.ppm", &w, &h);
    if(status != 0) printf("bad: %d", status);
    t3u8 *buf= malloc(w * h * sizeof *buf);
    readimg  = (struct raster_3u8){buf, w, w, h};
    status   = fr_ppm("original.ppm", &readimg);
    if(status != 0) printf("bad %d", status);
    writeppm("readback.ppm", readimg.buf, readimg.pxw, readimg.pxh);
  }

  free(img.buf);
  free(readimg.buf);
  return 0;
}
