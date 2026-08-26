#include "src/wayland/tests/presentation_test.h"

static void draw_gradient(imgview img, uint32_t frame)
{
  for(usize y= 0; y < img.pxh; y++)
  {
    uint32_t *row= (uint32_t *)((u8 *)img.data + y * img.stride);
    for(usize x= 0; x < img.pxw; x++)
    {
      uint8_t r= (uint8_t)(x * 255 / img.pxw + frame);
      uint8_t g= (uint8_t)(y * 255 / img.pxh + frame * 2);
      uint8_t b= (uint8_t)(frame * 3);
      row[x]  = prestest_argb(r, g, b);
    }
  }
}

int main(void)
{
  prestest test= {0};
  if(prestest_init(&test, "CDR presentation: animated gradient") < 0) return 1;

  uint32_t frame= 0;
  while(!test.quit)
  {
    if(dpypoll(test.dpy) < 0) break;

    imgview img= dpyacq_nextbuf(test.swp);
    if(img.data)
    {
      draw_gradient(img, frame++);
      if(dpypresent(test.dpy, test.srf, test.swp) < 0) break;
    }

    prestest_sleep_ns(16666667L);
  }

  prestest_term(&test);
  return 0;
}
