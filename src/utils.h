#pragma once 

#include "defs.h"
#include <string.h>

// divide vector by scalar in-place
static inline void vdivs_u32(u32 *v, usize n, const u32 d)
{
  const u32 *const end= v + n;
  while(v != end) *v++/= d;
}

static inline void vacc_4u32(u32 v[4], const u32 u[4])
{
  v[0]+= u[0];
  v[1]+= u[1];
  v[2]+= u[2];
  v[3]+= u[3];
}

// accumulates u into v
static inline void vacc_4u8(u8 v[4], const u8 u[4])
{
  v[0]+= u[0];
  v[1]+= u[1];
  v[2]+= u[2];
  v[3]+= u[3];
}

// fill a memory region with a repeating pattern src
// the memory region must contain at least n*src_size bytes
// assume dst has size >= src_size * n
static inline void memfill(
  void *restrict dst, const void *restrict src, usize src_size, usize n)
{
  if(src_size == 0) return;
  if(src_size == 1)  // if the pattern is 1 byte, defer to memset
  {
    memset(dst, *((char *)src), n);
    return;
  }

  memcpy(dst, src, src_size);
  size_t bytes= src_size;
  while(bytes + bytes <= n * src_size)
  {
    memcpy((char *)dst + bytes, dst, bytes);
    bytes+= bytes;
  }
  size_t remain= n * src_size - bytes;
  memcpy((char *)dst + bytes, dst, remain);
}
