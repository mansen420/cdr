#include <stdio.h>
#include <stdlib.h>

#include "src/wayland/wayland.h"

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

static void toplvlcnf_h(
  void                *data,
  struct xdg_toplevel *xdg_toplevel,
  int32_t              width,
  int32_t              height,
  struct wl_array     *states)
{
  NNUL(data, return;);
  wlctx *ctx= (wlctx *)data;

  i32 w_new, h_new;
  if(width && height)
    w_new= width, h_new= height;
  else
    w_new= ctx->w_pref, h_new= ctx->h_pref;

  if(ctx->rszcb) ctx->rszcb(w_new, h_new, ctx->usrdata);
}
static void toplvlclose_h(void *data, struct xdg_toplevel *xdg_toplevel)
{
  NNUL(data, return;);
  wlctx *ctx= (wlctx *)data;

  if(ctx->clscb) ctx->clscb(ctx->usrdata);
}
static void toplvlbounds_h(
  void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height)
{
  NNUL(data, return;);
  wlctx *ctx= (wlctx *)data;

  if(width && height) ctx->w_max= width, ctx->h_max= height;
}
static void toplvlcpb_h(
  void *data, struct xdg_toplevel *xdg_toplevel, struct wl_array *capabilities)
{
  // no-op
}

static void xdgsrf_h(void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
  xdg_surface_ack_configure(xdg_surface, serial);
}
static void regobj_h(
  void               *data,
  struct wl_registry *wl_registry,
  uint32_t            name,
  const char         *interface,
  uint32_t            version)
{
  NNUL(data, return;);

  wlctx *ctx= (wlctx *)(data);
  if(strcmp(interface, wl_compositor_interface.name) == 0)
    ctx->comp=
      wl_registry_bind(wl_registry, name, &wl_compositor_interface, version);
  else if(strcmp(interface, xdg_wm_base_interface.name) == 0)
  {
    ctx->shell=
      wl_registry_bind(wl_registry, name, &xdg_wm_base_interface, version);
    xdg_wm_base_add_listener(ctx->shell, &ctx->xdglstn, ctx);
  }
}
// handle object removal
static void regobjr_h(void *data, struct wl_registry *wl_registry, uint32_t name)
{
  // no-op
}
static void xdgping_h(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
  xdg_wm_base_pong(xdg_wm_base, serial);
}

void *dpyget_nwin(wlctx *ctx) { return ctx->surf; }
void *dpyget_ncon(wlctx *ctx) { return ctx->disp; }
int   dpyqry_init(dpyctx *ctx) { return ctx && ctx->alive; }

int dpyinit(dpyctx **out, const char *title, unsigned w, unsigned h, u32 flags)
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

  ctx->alive= 1;
  *out= ctx;
  return 0;
fail:
  ctx->err= DPYERR_INIT;
  dpyterm(ctx);
  return -1;
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
  memset(ctx, 0, sizeof *ctx);
  free(ctx);
}
int dpypoll(dpyctx *ctx)
{
  if(wl_display_flush(ctx->disp) < 0) return -1;
  if(wl_display_roundtrip(ctx->disp) < 0) return -1;
  return 0;
}

void dpyset_usrdata(dpyctx *ctx, void *usr) { ctx->usrdata= usr; }
void dpyset_onrsz(dpyctx *ctx, resizefn_t cb) { ctx->rszcb= cb; }
void dpyset_onclose(dpyctx *ctx, closefn_t cb) { ctx->clscb= cb; }

#undef OK
#undef NNUL
