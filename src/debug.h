#pragma once

#include <stdio.h>

#define OK(_expr, DO)                                        \
  if((_expr) < 0)                                            \
  {                                                          \
    printf("NEG: %s (%s:%d)\n", #_expr, __FILE__, __LINE__); \
    DO                                                       \
  }
#define NNUL(_expr, DO)                                       \
  if(!(_expr))                                                \
  {                                                           \
    printf("NULL: %s (%s:%d)\n", #_expr, __FILE__, __LINE__); \
    DO                                                        \
  }
#define IF(_expr, DO)                                        \
  if(_expr)                                                  \
  {                                                          \
    printf("BAD: %s (%s:%d)\n", #_expr, __FILE__, __LINE__); \
    DO                                                       \
  }
