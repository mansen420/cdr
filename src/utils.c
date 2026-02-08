#include <string.h>

#include "defs.h"

// fill a memory region with a repeating pattern src
// the memory region must contain at least n*src_size bytes
// assume dst has size >= src_size * n
inline void memfill(
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
