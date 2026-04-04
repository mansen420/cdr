#include "include/mem.h"

#include <stdlib.h>

static void *default_alloc(void *ctx, usize bytes)
{
  (void)ctx;
  return malloc(bytes);
}

static void default_free(void *ctx, void *addr)
{
  (void)ctx;
  free(addr);
}

static void *(*alloc_fn)(void *, usize)= default_alloc;
static void (*free_fn)(void *, void *) = default_free;

void *cdralloc(void *ctx, usize bytes) { return alloc_fn(ctx, bytes); }
void  cdrfree(void *ctx, void *addr) { free_fn(ctx, addr); }

void cdrset_alloc(void *(*alloc)(void *ctx, usize bytes),
                  void (*free)(void *ctx, void *addr))
{
  alloc_fn= alloc ? alloc : default_alloc;
  free_fn = free ? free : default_free;
}
