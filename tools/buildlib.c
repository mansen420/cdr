// self-contained C file for building CDR modules

// input looks like
// ./buildlib <libname> <module_path>
// e.g., ./buildlib gfx src/vulkan, builds libgfx.a from src/vulkan/build.c

#include <stdio.h>

#include "src/bld/bld_posix.c"

int main(int argc, char **argv)
{
  int rc= -1;

  bldtc gcc= {"cc"};

  if(bldcmpl(&gcc, "src/core/io.c", "io.o")) puts("io.c failed");
  if(bldcmpl(&gcc, "src/core/utils.c", "utils.o")) puts("utils.c failed");
  if(
    bldarch(
      "ar", (bldstrlist){.list= {"utils.o", "io.o"}, .nr= 2}, "libcore.a"))
    puts("archive failed");

  rc= 0;
exit:
  return rc;
}
