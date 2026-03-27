#pragma once

#include <string.h>

#include "include/defs.h"
#include "include/display.h"
#include "src/wayland/xdg-shell-client-protocol.h"
#include "wayland-client.h"

typedef struct dpyctx
{
  // handles
  struct wl_display    *disp;
  struct wl_registry   *reg;
  struct wl_compositor *comp;
  struct wl_surface    *surf;
  struct xdg_wm_base   *shell;
  struct xdg_surface   *xdgsrf;
  struct xdg_toplevel  *toplvl;

  // params
  i32 w_max, h_max;    // compositor bounds, if any
  i32 w_pref, h_pref;  // fallback for {0, 0} config event

  // flags
  u32 flags;
  u32 err;

  // fnc pointers
  struct xdg_wm_base_listener  xdglstn;
  struct wl_registry_listener  reglstn;
  struct xdg_surface_listener  xdgsrflstn;
  struct xdg_toplevel_listener toplvllstn;

  // callbacks
  resizefn_t rszcb;
  closefn_t  clscb;
  void      *usrdata;

  u32 alive;
} wlctx;

// handlers
void toplvlcnf_h(
  void *, struct xdg_toplevel *, int32_t, int32_t, struct wl_array *);
void toplvlclose_h(void *, struct xdg_toplevel *);
void toplvlbounds_h(void *, struct xdg_toplevel *, int32_t, int32_t);
void toplvlcpb_h(void *, struct xdg_toplevel *, struct wl_array *);
void xdgsrf_h(void *, struct xdg_surface *, uint32_t);
void regobj_h(void *, struct wl_registry *, uint32_t, const char *, uint32_t);
void regobjr_h(void *, struct wl_registry *, uint32_t);
void xdgping_h(void *, struct xdg_wm_base *, uint32_t);

// public API (mirrored from display.h for internal use)
int    dpyinit(dpyctx **, const char *, unsigned, unsigned, u32);
void   dpyterm(dpyctx *);
int    dpypoll(dpyctx *);
int    dpyqry_init(dpyctx *);
void  *dpyget_nwin(wlctx *);
void  *dpyget_ncon(wlctx *);
void   dpyset_usrdata(dpyctx *, void *);
void   dpyset_onrsz(dpyctx *, resizefn_t);
void   dpyset_onclose(dpyctx *, closefn_t);
