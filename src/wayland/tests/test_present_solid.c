#include "src/wayland/tests/presentation_test.h"

int main(void)
{
  static const uint32_t colors[]= {
    UINT32_C(0xffd64045),
    UINT32_C(0xff35a853),
    UINT32_C(0xff3478db),
    UINT32_C(0xffd6a932),
    UINT32_C(0xff8d55cc),
  };

  prestest test= {0};
  if(prestest_init(&test, "CDR presentation: one-second colors") < 0) return 1;

  usize    coloridx= 0;
  uint64_t next    = 0;
  while(!test.quit)
  {
    if(dpypoll(test.dpy) < 0) break;

    uint64_t now= prestest_now_ns();
    if(now >= next)
    {
      imgview img= dpyacq_nextbuf(test.swp);
      if(img.data)
      {
        prestest_fill(img, colors[coloridx]);
        if(dpypresent(test.dpy, test.srf, test.swp) < 0) break;
        coloridx= (coloridx + 1) % (sizeof colors / sizeof colors[0]);
        next    = now + UINT64_C(1000000000);
      }
    }

    prestest_sleep_ns(1000000L);
  }

  prestest_term(&test);
  return 0;
}
