#include "include/bld.h"
#include "src/core/bld.h"
#include "src/wayland/bld.h"

// UNIX-ish definition
static const bldextdep wayland= {
  .name= "wayland",
  .lflags=
    {
      .list= {"-lwayland-client"},
      .nr  = 1,
    },
};

const bldmod dpy= {
  .name= "dpy",
  .sources=
    {
      .list=
        {
          "src/wayland/wayland.c",
          "src/wayland/xdg-shell-client-protocol.c",
        },
      .nr= 2,
    },
  .implh=
    {
      .list=
        {
          "src/wayland/wayland.h",
          "src/wayland/xdg-shell-client-protocol.h",
        },
      .nr= 2,
    },
  .ifaceh=
    {
      .list= {"include/display.h"},
      .nr  = 1,
    },
  .deps     = {&core},
  .nrdeps   = 1,
  .extdeps  = {&wayland},
  .nrextdeps= 1,
};
