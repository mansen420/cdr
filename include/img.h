#include "include/defs.h"

typedef enum
{
  BGRX8,
  BGRA8
} imgfmt;

typedef struct
{
  void  *data;
  usize  rowbytes;
  usize  pxw, pxh;
  imgfmt fmt;
} imgview;