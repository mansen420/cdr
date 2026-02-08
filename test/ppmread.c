#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "io.h"
#include "raster.h"
#include "utils.h"

int writeppm(const char *path, t3u8 *buf, unsigned w, unsigned h)
{
  int   rc= -1;
  FILE *f = NULL;

  f= fopen(path, "wb");
  if(!f) goto exit;

  rc= fwritebppm(f, buf, w, h);

exit:
  if(f) fclose(f);
  return rc;
}
void drawimg(struct raster_3u8 img)
{
  for(unsigned i= 0; i < img.pxh; i++)
  {
    rgb_u8 pattern= {(f32)(i + 1) / img.pxh * 255, 112, 80};
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
    img      = (struct raster_3u8){buf, w, w, h, FMT_RGB};
    drawimg(img);
    writeppm("original.ppm", buf, w, h);
  }

  struct raster_3u8 readimg;
  {
    unsigned w, h;
    FILE    *f     = fopen("original.ppm", "rb");
    int      status= fprsbppmhdr(f, &w, &h);
    if(status != 0) printf("bad: %d", status);
    t3u8 *buf= malloc(w * h * sizeof *buf);
    readimg = (struct raster_3u8){buf, w, w, h, FMT_RGB};
    status= freadbppm(f, readimg);
    if(status !=0) 
      printf("bad %d", status);
    writeppm("readback.ppm", readimg.buf, readimg.pxw, readimg.pxh);
    fclose(f);
  }
  
  free(img.buf);
  free(readimg.buf);
  return 0;
}
