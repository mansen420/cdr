#include "include/defs.h"

#define KB 1024
#define MB (KB*KB)

typedef struct
{
  usize size;
  usize head;
  usize slop;
} arnctx;

// returns offset to start of mem region, or -1
// TODO implement
isize arnalloc(arnctx, usize size, usize algn);
void  arnclear(arnctx);
