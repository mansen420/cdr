#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#include "include/defs.h"
#include "src/raster.h"

int main(int argc, char *argv[])
{
  const unsigned w= 1920, h= 1080;
  t4u8          *buf1= malloc(w * h * sizeof *buf1);
  memset(buf1, 1, w * h * sizeof *buf1);

  t4u8 *buf2= malloc(w * h * sizeof *buf2);
  memset(buf2, 2, w * h * sizeof *buf2);

  struct timespec start, end;
  // warm up caches
  rgba2planar(buf1, buf2, w * h);
  timespec_get(&start, TIME_UTC);
  const unsigned n_calls=10000;
  volatile byte sink;
  for(unsigned iter= 0; iter < n_calls; iter++)
  {
    rgba2planar(buf1, buf2, w*h);
    sink= ((byte*)buf2)[0];
  }
  timespec_get(&end, TIME_UTC);

  double elapsed=
    (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
  double per_call= elapsed / n_calls;

  printf("%.6f seconds per call\n", per_call);

  return 0;
}
