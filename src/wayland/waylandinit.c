#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/defs.h"
#include "include/display.h"
#include "src/wayland/xdg-shell-client-protocol.h"
#include "wayland-client.h"

#define NNUL(_expr, DO)                                               \
  if(!(_expr))                                                        \
  {                                                                   \
    printf("Null pointer: %s (%s:%d)\n", #_expr, __FILE__, __LINE__); \
    DO                                                                \
  }

#define OK(_expr, DO)                                             \
  if((_expr) < 0)                                                 \
  {                                                               \
    printf("Negative: %s (%s:%d)\n", #_expr, __FILE__, __LINE__); \
    DO                                                            \
  }

enum WLCTX_FLAGBITS
{
  WLCTX_RESIZE = 0x1 << 0,
  WLCTX_CLOSE  = 0x1 << 1,
  WLCTX_BOUNDS = 0x1 << 2,
  WLCTX_LOOPING= 0x1 << 3
};
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
  i32 w, h;
  i32 w_max, h_max;
  i32 w_new, h_new;
  i32 w_pref, h_pref;

  // flags
  u32 flags;
  u32 err;

  // fnc pointers
  struct xdg_wm_base_listener  xdglstn;
  struct wl_registry_listener  reglstn;
  struct xdg_surface_listener  xdgsrflstn;
  struct xdg_toplevel_listener toplvllstn;
} wlctx;

#include "src/wayland/handlers.inl"

wlctx *dpyinit(const char *title, unsigned w, unsigned h, u32 flags)
{
  wlctx *ctx= calloc(1, sizeof *ctx);

  ctx->reglstn   = (struct wl_registry_listener){&regobj_h, &regobjr_h};
  ctx->xdglstn   = (struct xdg_wm_base_listener){&xdgping_h};
  ctx->xdgsrflstn= (struct xdg_surface_listener){&xdgsrf_h};
  ctx->toplvllstn= (struct xdg_toplevel_listener){
    &toplvlcnf_h, &toplvlclose_h, &toplvlbounds_h, &toplvlcpb_h};
  ctx->w_pref= w, ctx->h_pref= h;

  signed res;

  ctx->disp= wl_display_connect(NULL);
  NNUL(ctx->disp, goto fail;)

  ctx->reg= wl_display_get_registry(ctx->disp);
  NNUL(ctx->reg, goto fail;)
  res= wl_registry_add_listener(ctx->reg, &ctx->reglstn, ctx);
  OK(res, goto fail;)

  res= wl_display_roundtrip(ctx->disp);
  OK(res, goto fail;)

  ctx->surf= wl_compositor_create_surface(ctx->comp);
  NNUL(ctx->surf, goto fail;)

  ctx->xdgsrf= xdg_wm_base_get_xdg_surface(ctx->shell, ctx->surf);
  NNUL(ctx->xdgsrf, goto fail;);
  res= xdg_surface_add_listener(ctx->xdgsrf, &ctx->xdgsrflstn, ctx);

  ctx->toplvl= xdg_surface_get_toplevel(ctx->xdgsrf);
  NNUL(ctx->toplvl, goto fail;);
  res= xdg_toplevel_add_listener(ctx->toplvl, &ctx->toplvllstn, ctx);
  OK(res, goto fail;)

  xdg_toplevel_set_title(ctx->toplvl, title);
  xdg_toplevel_set_app_id(ctx->toplvl, title);

  wl_surface_commit(ctx->surf);
  res= wl_display_roundtrip(ctx->disp);
  OK(res, goto fail;)
  wl_surface_commit(ctx->surf);

  return ctx;
fail:
  ctx->err= DPYERR_INIT;
  dpyterm(ctx);
  return NULL;
}
void dpyterm(dpyctx *ctx)
{
  if(!ctx) return;

  if(ctx->toplvl) xdg_toplevel_destroy(ctx->toplvl);
  if(ctx->xdgsrf) xdg_surface_destroy(ctx->xdgsrf);
  if(ctx->surf) wl_surface_destroy(ctx->surf);
  if(ctx->shell) xdg_wm_base_destroy(ctx->shell);
  if(ctx->comp) wl_compositor_destroy(ctx->comp);
  if(ctx->reg) wl_registry_destroy(ctx->reg);
  if(ctx->disp) wl_display_disconnect(ctx->disp);
  free(ctx);
}
