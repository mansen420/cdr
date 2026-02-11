#pragma once

#include <stdio.h>

#include "defs.h"

int prnbppm(
  const t3u8 *px,
  unsigned            w,
  unsigned            h,
  byte               *img,
  unsigned            imgcap);

int fwritebppm(FILE *f, const t3u8 *px, unsigned w, unsigned h);

//always call fprsbppmhdr before this
int freadbppm(FILE *f, struct raster_3u8 data);
//do not call freadbppm before this 
int fprsbppmhdr(FILE *f, unsigned *w, unsigned *h);
