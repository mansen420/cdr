#include "include/bld.h"
#include "src/core/bld.h"

// UNIX-ish definition
static const bldextdep libm= {
  .name= "libm",
  .lflags=
    {
      .list= {"-lm"},
      .nr  = 1,
    },
};

const bldmod core= {
  .name= "core",
  .sources=
    {
      .list=
        {
          "src/core/io.c",
          "src/core/mem.c",
          "src/core/raster.c",
          "src/core/utils.c",
        },
      .nr= 4,
    },
  .implh=
    {
      .list=
        {
          "src/core/include/arn.h",
          "src/core/include/math_depr.h",
        },
      .nr= 2,
    },
  .ifaceh=
    {
      .list=
        {
          "include/defs.h",
          "include/mem.h",
          "src/core/include/debug.h",
          "src/core/include/io.h",
          "src/core/include/lsys.h",
          "src/core/include/math.h",
          "src/core/include/raster.h",
          "src/core/include/utils.h",
        },
      .nr= 8,
    },
  .extdeps  = {&libm},
  .nrextdeps= 1,
};
