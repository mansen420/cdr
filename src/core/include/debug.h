#pragma once

#include <stdio.h>

// debug builds report every fired check (with file:line) as an author-facing
// trace aid; release builds (NDEBUG) are silent, leaving only the control flow.
#ifdef NDEBUG
  #define DBGLOG(...) ((void)0)
#else
  #define DBGLOG(...) printf(__VA_ARGS__)
#endif

#define OK(_expr, DO)                                          \
  if((_expr) >= 0) {}                                          \
  else                                                         \
  {                                                            \
    DBGLOG("NEG: %s (%s:%d)\n", #_expr, __FILE__, __LINE__);   \
    DO                                                         \
  }
#define NNUL(_expr, DO)                                         \
  if((_expr)) {}                                                \
  else                                                          \
  {                                                             \
    DBGLOG("NULL: %s (%s:%d)\n", #_expr, __FILE__, __LINE__);   \
    DO                                                          \
  }
#define IF(_expr, DO)                                          \
  if(!(_expr)) {}                                              \
  else                                                         \
  {                                                             \
    DBGLOG("BAD: %s (%s:%d)\n", #_expr, __FILE__, __LINE__);   \
    DO                                                         \
  }
