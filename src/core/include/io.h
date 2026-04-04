#pragma once

#include <stdio.h>

#include "include/defs.h"

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

isize fsize(const char *path);
int fread_b(const char *path, u8 *buf, usize cap);
int femitspv(const char *rpath, const char *wpath);
