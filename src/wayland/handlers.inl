#pragma once

void toplvlcnf_h(
  void                *data,
  struct xdg_toplevel *xdg_toplevel,
  int32_t              width,
  int32_t              height,
  struct wl_array     *states)
{
  NNUL(data, return;);
  wlctx *ctx= (wlctx *)data;

  ctx->flags|= WLCTX_RESIZE;

  if(width && height)
    ctx->w_new= width, ctx->h_new= height;
  else
    ctx->w_new= ctx->w_pref, ctx->h_new= ctx->h_pref;
}
void toplvlclose_h(void *data, struct xdg_toplevel *xdg_toplevel)
{
  NNUL(data, return;);
  wlctx *ctx= (wlctx *)data;

  ctx->flags|= WLCTX_CLOSE;
}
void toplvlbounds_h(
  void                *data,
  struct xdg_toplevel *xdg_toplevel,
  int32_t              width,
  int32_t              height)
{
  NNUL(data, return;);
  wlctx *ctx= (wlctx *)data;

  ctx->flags|= WLCTX_BOUNDS;

  if(width && height) ctx->w_max= width, ctx->h_max= height;
}
void toplvlcpb_h(
  void                *data,
  struct xdg_toplevel *xdg_toplevel,
  struct wl_array     *capabilities)
{
  // no-op
}

void xdgsrf_h(
  void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
  xdg_surface_ack_configure(xdg_surface, serial);
}
void regobj_h(
  void               *data,
  struct wl_registry *wl_registry,
  uint32_t            name,
  const char         *interface,
  uint32_t            version)
{
  NNUL(data, return;);

  wlctx *ctx= (wlctx *)(data);
  if(strcmp(interface, wl_compositor_interface.name) == 0)
    ctx->comp= wl_registry_bind(
      wl_registry, name, &wl_compositor_interface, version);
  else if(strcmp(interface, xdg_wm_base_interface.name) == 0)
  {
    ctx->shell= wl_registry_bind(
      wl_registry, name, &xdg_wm_base_interface, version);
    xdg_wm_base_add_listener(ctx->shell, &ctx->xdglstn, ctx);
  }
}
// handle object removal
void regobjr_h(
  void *data, struct wl_registry *wl_registry, uint32_t name)
{
  // no-op
}
void xdgping_h(
  void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
  xdg_wm_base_pong(xdg_wm_base, serial);
}
