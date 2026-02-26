#pragma once

#include "include/defs.h"

typedef enum
{
  DPYERR_INIT
} DPYERR;

typedef struct dpyctx dpyctx;

dpyctx *dpyinit(const char *title, unsigned width, unsigned height, u32 flags);

int         dpygeterr(dpyctx *);
const char *dpyerrstr(DPYERR);

// get native window handle
void *dpygetnwin(dpyctx *);
// get native instance handle
void *dpygetncon(dpyctx *);

int dpypoll(dpyctx *);

void dpyterm(dpyctx *);

typedef void (*resizefn_t)(i32, i32, void *);
typedef void (*closefn_t)(void *);

void dpysetusrdata(dpyctx *, void *);
void dpysetrszcb(dpyctx *, resizefn_t);
void dpysetclscb(dpyctx *, closefn_t);

int dpyinit_q(dpyctx *);
