#include "src/core/bld.h"
#include "src/vulkan/bld.h"
#include "src/wayland/bld.h"

// UNIX-ish definition
static const bldextdep vulkan= {
  .name= "vulkan", .lflags= {.list= {"-lvulkan"}, .nr= 1}};

const bldmod gfx= {
  .name     = "gfx",
  .sources  = {.list= {"src/vulkan/vulkan.c"}, .nr= 1},
  .implh    = {.list= {"src/vulkan/vulkan.h"}, .nr= 1},
  .ifaceh   = {.list= {"include/graphics.h"}, .nr= 1},
  .deps     = {&core, &dpy},
  .nrdeps   = 2,
  .extdeps  = {&vulkan},
  .nrextdeps= 1};
