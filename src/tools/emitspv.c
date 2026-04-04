#include "src/core/include/io.h"

#include <stdio.h>

int main(int argc, char **argv)
{
  if(argc != 3)
  {
    fprintf(stderr, "usage: emitspv <input.spv> <output.h>\n");
    return 1;
  }
  return femitspv(argv[1], argv[2]);
}
