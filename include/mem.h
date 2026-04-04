#pragma once

#include "include/defs.h"

void *cdralloc(void *ctx, usize bytes);
void  cdrfree(void *ctx, void *addr);

void cdrset_alloc(void *(*alloc)(void *ctx, usize bytes),
                  void (*free)(void *ctx, void *addr));
