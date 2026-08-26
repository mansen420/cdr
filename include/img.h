#include "include/defs.h"

typedef enum
{
  IMG_DEFAULT_FMT,
  XRGB8,
  ARGB8
} imgfmt;

typedef struct imgdesc_fmt
{
  struct
  {
    u32 blockw, blockh;  // logical width and height of each block
    u32 bytesperblock;
  } planes[16];
  u32 nrplanes;
} imgdesc_fmt;

static inline imgdesc_fmt imgqry_fmtdesc(imgfmt fmt)
{
  imgdesc_fmt desc= {0};

  switch(fmt)
  {
    case XRGB8:
    case ARGB8:
      desc= (imgdesc_fmt){.planes= {{1, 1, 4}}, .nrplanes= 1};
      break;
    default:
      break;
  }
  return desc;
}

// presupposes a packed representation
static inline usize
imgqry_imgbytes(imgfmt fmt, u32 pxw, u32 pxh)
{
  imgdesc_fmt desc = imgqry_fmtdesc(fmt);
  usize       bytes= 0;

  if(!desc.nrplanes || !pxw || !pxh) return 0;

  for(u32 i= 0; i < desc.nrplanes; i++)
  {
    u32 blockw       = desc.planes[i].blockw;
    u32 blockh       = desc.planes[i].blockh;
    u32 bytesperblock= desc.planes[i].bytesperblock;
    if(!blockw || !blockh || !bytesperblock) return 0;

    usize nrbx= pxw / blockw + (pxw % blockw != 0);
    usize nrby= pxh / blockh + (pxh % blockh != 0);
    if(nrbx > SIZE_MAX / bytesperblock) return 0;

    usize rowbytes= nrbx * bytesperblock;
    if(nrby > SIZE_MAX / rowbytes) return 0;

    usize planebytes= rowbytes * nrby;
    if(bytes > SIZE_MAX - planebytes) return 0;
    bytes+= planebytes;
  }
  return bytes;
}

typedef struct
{
  void  *data;
  usize  stride;  // bytes
  usize  pxw, pxh;
  imgfmt fmt;
} imgview;
