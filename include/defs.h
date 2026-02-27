#pragma once

#include <stddef.h>
#include <stdint.h>

#define MIN(a, b)        \
  ({                     \
    __auto_type _a= (a); \
    __auto_type _b= (b); \
    _a < _b ? _a : _b;   \
  })
#define MAX(a, b)        \
  ({                     \
    __auto_type _a= (a); \
    __auto_type _b= (b); \
    _a > _b ? _a : _b;   \
  })

#define CLAMP(x, hi, lo)                    \
  ({                                        \
    __auto_type _x = (x);                   \
    __auto_type _lo= (lo);                  \
    __auto_type _hi= (hi);                  \
    _x < _lo ? _lo : (_x > _hi ? _hi : _x); \
  })

typedef unsigned char byte;

typedef ptrdiff_t isize;
typedef size_t    usize;

typedef int32_t i32;
typedef int16_t i16;
typedef int64_t i64;
typedef int8_t  i8;

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint64_t u64;
typedef uint8_t  u8;

typedef float  f32;
typedef double f64;

typedef union t4f64
{
  struct
  {
    float r, g, b, a;
  };
  struct
  {
    float x, y, z, w;
  };
} t4f64;

typedef union t3f64
{
  struct
  {
    float r, g, b;
  };
  struct
  {
    float x, y, z;
  };
} t3f64;

typedef union t2f64
{
  struct
  {
    float x, y;
  };
} t2f64;

typedef union t4f32
{
  struct
  {
    float r, g, b, a;
  };
  struct
  {
    float x, y, z, w;
  };
} t4f32;

typedef union t3f32
{
  struct
  {
    float r, g, b;
  };
  struct
  {
    float x, y, z;
  };
} t3f32;

typedef union t2f32
{
  struct
  {
    float x, y;
  };
} t2f32;

typedef union t3u8
{
  struct
  {
    u8 r, g, b;
  };
  struct
  {
    u8 x, y, z;
  };
} t3u8;

typedef union t4u8
{
  struct
  {
    u8 r, g, b, a;
  };
  struct
  {
    u8 x, y, z, w;
  };
} t4u8;

typedef union t2u8
{
  struct
  {
    u8 x, y;
  };
} t2u8;

typedef union t2i8
{
  struct
  {
    i8 x, y;
  };
} t2i8;
typedef union t3i8
{
  struct
  {
    i8 x, y, z;
  };
} t3i8;
typedef union t4i8
{
  struct
  {
    i8 x, y, z, w;
  };
} t4i8;

typedef union t3u16
{
  struct
  {
    u16 r, g, b;
  };
  struct
  {
    u16 x, y, z;
  };
} t3u16;

typedef union t4u16
{
  struct
  {
    u16 r, g, b, a;
  };
  struct
  {
    u16 x, y, z, w;
  };
} t4u16;

typedef union t2u16
{
  struct
  {
    u16 x, y;
  };
} t2u16;

typedef union t2i16
{
  struct
  {
    i16 x, y;
  };
} t2i16;
typedef union t3i16
{
  struct
  {
    i16 x, y, z;
  };
} t3i16;
typedef union t4i16
{
  struct
  {
    i16 x, y, z, w;
  };
} t4i16;

typedef union t3u32
{
  struct
  {
    u32 r, g, b;
  };
  struct
  {
    u32 x, y, z;
  };
} t3u32;

typedef union t4u32
{
  struct
  {
    u32 r, g, b, a;
  };
  struct
  {
    u32 x, y, z, w;
  };
} t4u32;

typedef union t2u32
{
  struct
  {
    u32 x, y;
  };
} t2u32;

typedef union t2i32
{
  struct
  {
    i32 x, y;
  };
} t2i32;
typedef union t3i32
{
  struct
  {
    i32 x, y, z;
  };
} t3i32;
typedef union t4i32
{
  struct
  {
    i32 x, y, z, w;
  };
} t4i32;

typedef union t3u64
{
  struct
  {
    u64 r, g, b;
  };
  struct
  {
    u64 x, y, z;
  };
} t3u64;

typedef union t4u64
{
  struct
  {
    u64 r, g, b, a;
  };
  struct
  {
    u64 x, y, z, w;
  };
} t4u64;

typedef union t2u64
{
  struct
  {
    u64 x, y;
  };
} t2u64;

typedef union t2i64
{
  struct
  {
    i64 x, y;
  };
} t2i64;
typedef union t3i64
{
  struct
  {
    i64 x, y, z;
  };
} t3i64;
typedef union t4i64
{
  struct
  {
    i64 x, y, z, w;
  };
} t4i64;

enum pxfmt
{
  FMT_RGB,
  FMT_RGBA,
  FMT_PRGB,
  FMT_PRGBA,
};
struct raster_4u8
{
  t4u8      *buf;
  unsigned   stride;  // elements of buf per row
  unsigned   pxw, pxh;
  enum pxfmt fmt;
};

struct raster_3u8
{
  t3u8      *buf;
  unsigned   stride;  // elements of buf per row
  unsigned   pxw, pxh;
  enum pxfmt fmt;
};

struct raster_3f32
{
  t3f32     *buf;
  unsigned   stride;  // elements of buf per row
  unsigned   pxw, pxh;
  enum pxfmt fmt;
};

struct raster_4f32
{
  t4f32     *buf;
  unsigned   stride;  // elements of buf per row
  unsigned   pxw, pxh;
  enum pxfmt fmt;
};
