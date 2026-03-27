#pragma once

#include "include/defs.h"

typedef enum
{
  DPYERR_INIT
} DPYERR;

typedef struct dpyctx dpyctx;

dpyctx *dpyinit(const char *title, unsigned width, unsigned height, u32 flags);

int         dpyget_err(dpyctx *);
const char *dpyget_errstr(DPYERR);

// get native window handle
void *dpyget_nwin(dpyctx *);
// get native instance handle
void *dpyget_ncon(dpyctx *);

int dpypoll(dpyctx *);

void dpyterm(dpyctx *);

typedef void (*resizefn_t)(i32, i32, void *);
typedef void (*closefn_t)(void *);

void dpyset_usrdata(dpyctx *, void *);
void dpyset_onrsz(dpyctx *, resizefn_t);
void dpyset_onclose(dpyctx *, closefn_t);

int dpyqry_init(dpyctx *);
