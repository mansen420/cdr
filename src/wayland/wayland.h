#pragma once

#include <string.h>

#include "include/defs.h"
#include "include/display.h"
#include "src/wayland/xdg-shell-client-protocol.h"
#include "wayland-client.h"

#define MAX_FMTS 256

typedef enum
{
  INIT_ERR,
  FORMAT_OVERFLOW
} wlerr;

typedef struct dpyctx
{
  // connection-global handles
  struct wl_display    *disp;
  struct wl_registry   *reg;
  struct wl_compositor *comp;
  struct xdg_wm_base   *shell;
  struct wl_shm        *shm;

  // flags
  u32    flags;
  u32    err;
  imgfmt fmts[MAX_FMTS];
  u32    nrfmts;

  // fnc pointers
  struct xdg_wm_base_listener xdglstn;
  struct wl_registry_listener reglstn;
} wlctx;

typedef struct dpysrf
{
  // handles
  struct wl_surface   *surf;
  struct xdg_surface  *xdgsrf;
  struct xdg_toplevel *toplvl;

  // params
  i32 w_max, h_max;                // compositor bounds, if any
  i32 w_pref, h_pref;              // application-preferred size
  i32 w_configured, h_configured;  // latest compositor-configured size

  // fnc pointers
  struct xdg_surface_listener  xdgsrflstn;
  struct xdg_toplevel_listener toplvllstn;

  // callbacks
  resizefn_t rszcb;
  closefn_t  clscb;
  void      *usrdata;
} wlsrf;

typedef struct
{
  void *data;
  usize stride;
  u32   w, h;  // in pixels
  // free is 1 when compositor is not using this
  int   free;
  // acq is 1 when user has acquired this buffer
  int   acq;

  struct wl_buffer *hnd;
} wlbuf;

typedef struct dpyswp
{
  void *mapped;
  int   mfd;
  usize bytesmapped;

  u32   nrbufs;
  wlbuf bufs[8];
  u32   prsidx;

  struct wl_shm_pool *pool;
  u32                 w, h;
  imgfmt              fmt;
} wlswp;

// handlers
static void toplvlcnf_h(
  void *, struct xdg_toplevel *, int32_t, int32_t, struct wl_array *);
static void toplvlclose_h(void *, struct xdg_toplevel *);
static void toplvlbounds_h(void *, struct xdg_toplevel *, int32_t, int32_t);
static void toplvlcpb_h(void *, struct xdg_toplevel *, struct wl_array *);
static void xdgsrf_h(void *, struct xdg_surface *, uint32_t);
static void regobj_h(
  void *, struct wl_registry *, uint32_t, const char *, uint32_t);
static void regobjr_h(void *, struct wl_registry *, uint32_t);
static void xdgping_h(void *, struct xdg_wm_base *, uint32_t);
static void fmt_h(void *, struct wl_shm *, uint32_t);
static void bufrel_h(void *, struct wl_buffer *);
