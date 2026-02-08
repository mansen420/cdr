#pragma once

#include <stdio.h>

#include "defs.h"

int prnbppm(
  const struct rgb24 *px,
  unsigned            w,
  unsigned            h,
  byte               *img,
  unsigned            imgcap);

int fwritebppm(FILE *f, const struct rgb24 *px, unsigned w, unsigned h);

//always call fprsbppmhdr before this
int freadbppm(FILE *f, struct raster_3u8 data);
//do not call freadbppm before this 
int fprsbppmhdr(FILE *f, unsigned *w, unsigned *h);
