#include "src/wayland/tests/presentation_test.h"

int main(void)
{
  prestest test= {0};
  if(prestest_init(&test, "CDR presentation: present once") < 0) return 1;

  imgview img= dpyacq_nextbuf(test.swp);
  if(!img.data)
  {
    fprintf(stderr, "failed to acquire initial swapchain image\n");
    prestest_term(&test);
    return 1;
  }

  prestest_fill(img, prestest_argb(35, 110, 210));
  if(dpypresent(test.dpy, test.srf, test.swp) < 0)
  {
    fprintf(stderr, "failed to queue initial presentation\n");
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
