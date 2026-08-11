// self-contained C file for building CDR modules

// input looks like
// ./buildlib

#include <stdio.h>
#include <string.h>

#include "src/bld/bld_posix.c"
#include "src/core/bld.c"
#include "src/vulkan/bld.c"
#include "src/wayland/bld.c"

bldtc cc= {"cc"};

int main(int argc, char **argv)
{
  int rc= -1;

  char       outpath[64][4096];
  bldstrlist objs= {0};
  for(u32 i= 0; i < gfx.sources.nr; i++)
  {
    sprintf(outpath[i], ".cache/%s-%d.o", gfx.name, i);
    if(bldcmpl(&cc, gfx.sources.list[i], outpath[i])) break;
    objs.list[objs.nr++]= outpath[i];
  }
  char libname[128];
  sprintf(libname, "lib%s.a", gfx.name);
  bldarch("ar", objs, libname);

  rc= 0;
exit:
  return rc;
}
