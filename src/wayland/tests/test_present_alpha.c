#include "src/wayland/tests/presentation_test.h"

static void draw_alpha_ramp(imgview img)
{
  for(usize y= 0; y < img.pxh; y++)
  {
    uint32_t *row= (uint32_t *)((u8 *)img.data + y * img.stride);
    for(usize x= 0; x < img.pxw; x++)
    {
      uint8_t a= img.pxw > 1 ? (uint8_t)(x * 255 / (img.pxw - 1)) : 255;
      uint8_t r= (uint8_t)(40 + y * 180 / img.pxh);
      uint8_t g= (uint8_t)(210 - y * 130 / img.pxh);
      uint8_t b= (uint8_t)(240 - y * 40 / img.pxh);
      row[x]  = prestest_argb_alpha(r, g, b, a);
    }
  }
}

int main(void)
{
  prestest test= {0};
  if(prestest_init(&test, "CDR presentation: alpha ramp") < 0) return 1;

  imgview img= dpyacq_nextbuf(test.swp);
  if(!img.data)
  {
    fprintf(stderr, "failed to acquire initial swapchain image\n");
    prestest_term(&test);
    return 1;
  }

  draw_alpha_ramp(img);
  if(dpypresent(test.dpy, test.srf, test.swp) < 0)
  {
    fprintf(stderr, "failed to queue alpha presentation\n");
    prestest_term(&test);
    return 1;
  }

  while(!test.quit)
  {
    if(dpypoll(test.dpy) < 0) break;
    prestest_sleep_ns(1000000L);
  }

  prestest_term(&test);
  return 0;
}
