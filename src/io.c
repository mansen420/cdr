#include "io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int prnbppm(
  const t3u8 *px, unsigned w, unsigned h, byte *img, unsigned imgcap)
{
  const int hdr=
    snprintf((char *)img, imgcap, "P6\n%u %u\n255\n", w, h);
  if(hdr < 0) return -1;
  if(hdr >= imgcap) return -2;

  const unsigned binsize= 3 * w * h;
  if(hdr + binsize > imgcap) return -2;

  memcpy(img + hdr, px, binsize);
  return binsize + hdr;
}

int fwritebppm(FILE *f, const t3u8 *px, unsigned w, unsigned h)
{
  int hdr= fprintf(f, "P6\n%u %u\n255\n", w, h);
  if(hdr < 0) return -1;
  size_t written= fwrite(px, sizeof *px, w * h, f);

  if(written != w * h) return -2;
  return 0;
}

int fprsbppmhdr(FILE *f, unsigned *w, unsigned *h)
{
  if(w == NULL || h == NULL) return -1;  // null data
  unsigned maxval;
  int      status= fscanf(f, "P6 %u %u %u", w, h, &maxval);
  if(status != 3 || status == EOF) return -2;  // parse error
  if(maxval != 255) return -3;                 // bad file
  if(fgetc(f) == EOF) return -3;
  return 0;
}

// only call this function after parsing the header
int freadbppm(FILE *f, struct raster_3u8 data)
{
  if(data.buf == NULL || data.pxw * data.pxh == 0)
    return -1;                        // invalid data
  if(data.fmt != FMT_RGB) return -2;  // bad fmt

  if(data.pxw == data.stride)
  {
    unsigned npx= fread(data.buf, 3, data.pxh * data.pxw, f);
    if(npx != data.pxh * data.pxw) return -3;  // parse error
  }
  else
    for(unsigned i= 0; i < data.pxh; i++)
    {
      unsigned npx= fread(data.buf + data.stride * i, 3, data.pxw, f);
      if(npx != data.pxw) return -3;
    }

  return 0;
}
