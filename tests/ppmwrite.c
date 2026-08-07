#include <stdio.h>
#include <stdlib.h>

#include "defs.h"
#include "io.h"
#include "raster.h"

int main(int argc, char *argv[])
{
  t3u8 *out= NULL;
  u32  *acc= NULL;
  int   rc = EXIT_FAILURE;

  unsigned mag= 10;
  unsigned min= 2;
  if(argc > 1) mag= strtoul(argv[1], NULL, 10);
  if(argc > 2) min= strtoul(argv[2], NULL, 10);

  t3u8 W   = {255, 255, 255};
  t3u8 B   = {0, 0, 0};
  t3u8 R   = {255, 0, 0};
  t3u8 BL  = {0, 0, 255};
  t3u8 px[]= {R, W, B, BL};

  struct raster_3u8 small= {px, 2, 2, 2};

  const int bytes= small.pxh * small.pxw * sizeof *small.buf;
  out            = malloc(bytes * mag * mag);
  if(!out) goto exit;
  struct raster_3u8 img= upsample_3u8(small, out, mag);

  rc= fw_ppm("out_mag.ppm", img);
  if(rc != 0) goto exit;

  rc= EXIT_FAILURE;

  free(out);
  out= malloc((img.pxw / min) * (img.pxh / min) * sizeof *out);
  struct raster_3u8 min_img= {
    out, img.pxw / min, img.pxw / min, img.pxh / min};
  acc= malloc(min_img.pxw * 3 * sizeof *acc);
  dnsamplebx_3u8(img, min_img, min, acc);

  rc= fw_ppm("out_min.ppm", min_img);
exit:
  if(out) free(out);
  if(acc) free(acc);
  return rc;
}
