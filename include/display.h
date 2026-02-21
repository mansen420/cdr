#pragma once

#include "include/defs.h"

typedef enum
{
	DPYERR_INIT
} DPYERR;;
typedef struct dpyctx dpyctx;

dpyctx *dpyinit(
  const char *title, unsigned width, unsigned height, u32 flags);

int dpygeterr(dpyctx *ctx);
const char *dpyerrstr(DPYERR);

int dpypoll(dpyctx *ctx);

void dpyterm(dpyctx *ctx);
