#include <stdlib.h>

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
    const unsigned w= 543, h= 473;
    t3u8          *buf= malloc(w * h * sizeof *buf);
    img               = (struct raster_3u8){buf, w, w, h};
    drawimg(img);
    writeppm("minify.ppm", buf, w, h);
  }

  // dst img
  const unsigned    min= 3;
  struct raster_3u8 dst;
  {
    const unsigned w= img.pxw / min, h= img.pxh / min;
    t3u8          *dbuf= malloc(w * h * sizeof *dbuf);
    dst= (struct raster_3u8){dbuf, /*stride=*/w, /*pxw=*/w, h};
  }

  u32 *acc= malloc(dst.pxw * 3 * sizeof *acc);
  dnsamplebx_3u8(img, dst, min, acc);
  writeppm("minify_out.ppm", dst.buf, dst.pxw, dst.pxh);

  free(acc);
  free(img.buf);
  free(dst.buf);
  return 0;
}
