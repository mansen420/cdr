#pragma once

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "include/display.h"

typedef struct
{
  dpyctx *dpy;
  dpysrf *srf;
  dpyswp *swp;
  int     quit;
} prestest;

static void prestest_onclose(void *data)
{
  prestest *test= data;
  test->quit     = 1;
}

static int prestest_init(prestest *test, const char *title)
{
  if(dpyinit(&test->dpy, 0) < 0) goto err;

  dpydesc_srf srfdesc= {.w= 640, .h= 480, .title= title};
  if(dpyinit_srf(test->dpy, &test->srf, srfdesc) < 0) goto err;

  dpyset_usrdata(test->srf, test);
  dpyset_onclose(test->srf, prestest_onclose);

  dpydesc_swp swpdesc= {.fmt= ARGB8};
  if(dpyinit_swp(test->dpy, test->srf, &test->swp, swpdesc) < 0) goto err;

  return 0;
err:
  fprintf(stderr, "failed to initialize presentation test\n");
  return -1;
}

static void prestest_term(prestest *test)
{
  dpyterm_swp(test->swp);
  dpyterm_srf(test->dpy, test->srf);
  dpyterm(test->dpy);
}

static uint32_t prestest_argb(uint8_t r, uint8_t g, uint8_t b)
{
  return UINT32_C(0xff000000) | (uint32_t)r << 16 | (uint32_t)g << 8 | b;
}

static void prestest_fill(imgview img, uint32_t color)
{
  for(usize y= 0; y < img.pxh; y++)
  {
    uint32_t *row= (uint32_t *)((u8 *)img.data + y * img.stride);
    for(usize x= 0; x < img.pxw; x++) row[x]= color;
  }
}

static uint64_t prestest_now_ns(void)
{
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (uint64_t)now.tv_sec * UINT64_C(1000000000) + (uint64_t)now.tv_nsec;
}

static void prestest_sleep_ns(long ns)
{
  struct timespec delay= {.tv_sec= ns / 1000000000L,
                          .tv_nsec= ns % 1000000000L};
  nanosleep(&delay, NULL);
}
