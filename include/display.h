#pragma once

#include "include/defs.h"
#include "include/img.h"

typedef enum
{
  DPYERR_INIT
} DPYERR;

typedef struct dpyctx dpyctx;

int dpyinit(
  dpyctx **, const char *title, unsigned width, unsigned height, u32 flags);

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


typedef struct dpysrf dpysrf;
int  dpyinit_srf(dpyctx *, dpysrf **, u32 w, u32 h, u32 x, u32 y, const char *);
void dpyterm_srf(dpyctx *, dpysrf *);

void dpyset_usrdata(dpysrf *, void *);
void dpyset_onrsz(dpysrf *, resizefn_t);
void dpyset_onclose(dpysrf *, closefn_t);

// a swapchain belongs to a specific surface
typedef struct dpyswp dpyswp;
int                   dpyinit_swp(dpyctx *, dpysrf *, dpyswp **);

// acquire a drawable buffer from the swapchain. the imgview is writeable if the
// call succeeds. warn: blocks if no imgview is availalbe to select from the
// swapchain.
const imgview *dpyswp_acq(dpyctx *, dpysrf *, dpyswp *);

// commit an imgview to the swapchain. the imgview is no longer writeable after
// this.
void dpyswp_rel(dpyctx *, dpysrf *, dpyswp *, imgview *);

void dpypresent(dpyctx *, dpysrf *, dpyswp *);
