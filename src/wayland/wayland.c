#define _GNU_SOURCE
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "src/core/include/debug.h"
#include "src/wayland/wayland.h"

static imgfmt wlfmt2img(uint32_t fmt)
{
  switch(fmt)
  {
    case WL_SHM_FORMAT_XRGB8888:
      return XRGB8;
    case WL_SHM_FORMAT_ARGB8888:
      return ARGB8;
    default:
      return IMG_DEFAULT_FMT;
  }
}
static imgfmt imgfmt2wl(uint32_t fmt)
{
  switch(fmt)
  {
    case XRGB8:
      return WL_SHM_FORMAT_XRGB8888;
    case ARGB8:
      return WL_SHM_FORMAT_ARGB8888;
    default:
      return WL_SHM_FORMAT_XRGB8888;
  }
}

static void fmt_h(void *data, struct wl_shm *shm, uint32_t fmt)
{
  if(!data) return;
  wlctx *ctx= data;
  (void)shm;

  imgfmt f= wlfmt2img(fmt);
  if(f == IMG_DEFAULT_FMT) return;

  if(ctx->nrfmts >= MAX_FMTS)
  {
    ctx->err= FORMAT_OVERFLOW;
    return;
  }
  ctx->fmts[ctx->nrfmts++]= f;
}
static const struct wl_shm_listener shmlstn= {.format= fmt_h};
static void toplvlcnf_h(
  void                *data,
  struct xdg_toplevel *xdg_toplevel,
  int32_t              width,
  int32_t              height,
  struct wl_array     *states)
{
  NNUL(data, return;);
  wlsrf *srf= (wlsrf *)data;

  srf->w_configured= width ? width : srf->w_pref;
  srf->h_configured= height ? height : srf->h_pref;

  if(srf->rszcb) srf->rszcb(srf->w_configured, srf->h_configured, srf->usrdata);
}
static void toplvlclose_h(void *data, struct xdg_toplevel *xdg_toplevel)
{
  NNUL(data, return;);
  wlsrf *srf= (wlsrf *)data;

  if(srf->clscb) srf->clscb(srf->usrdata);
}
static void toplvlbounds_h(
  void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height)
{
  NNUL(data, return;);
  wlsrf *srf= (wlsrf *)data;

  if(width && height) srf->w_max= width, srf->h_max= height;
}
static void toplvlcpb_h(
  void *data, struct xdg_toplevel *xdg_toplevel, struct wl_array *capabilities)
{
  // no-op
}

static void xdgsrf_h(
  void *data, struct xdg_surface *xdg_surface, uint32_t serial)
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
  else if(strcmp(interface, wl_shm_interface.name) == 0)
  {
    ctx->shm= wl_registry_bind(wl_registry, name, &wl_shm_interface, version);
    wl_shm_add_listener(ctx->shm, &shmlstn, ctx);
  }
}
// handle object removal
static void regobjr_h(
  void *data, struct wl_registry *wl_registry, uint32_t name)
{
  // no-op
}
static void xdgping_h(
  void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
  xdg_wm_base_pong(xdg_wm_base, serial);
}

void *dpyacq_nwin(dpysrf *srf) { return srf ? srf->surf : NULL; }
void *dpyacq_ncon(dpyctx *ctx) { return ctx ? ctx->disp : NULL; }

int dpyinit(dpyctx **out, u32 flags)
{
  int    rc = -1;
  wlctx *ctx= NULL;

  NNUL(out, goto exit;)
  *out= NULL;

  ctx= calloc(1, sizeof *ctx);
  NNUL(ctx, goto exit;)

  ctx->reglstn= (struct wl_registry_listener){&regobj_h, &regobjr_h};
  ctx->xdglstn= (struct xdg_wm_base_listener){&xdgping_h};
  ctx->flags  = flags;

  signed res;

  ctx->disp= wl_display_connect(NULL);
  NNUL(ctx->disp, goto exit;)

  ctx->reg= wl_display_get_registry(ctx->disp);
  NNUL(ctx->reg, goto exit;)
  res= wl_registry_add_listener(ctx->reg, &ctx->reglstn, ctx);
  OK(res, goto exit;)

  res= wl_display_roundtrip(ctx->disp);
  OK(res, goto exit;)
  NNUL(ctx->comp, goto exit;)
  NNUL(ctx->shell, goto exit;)

  OK(wl_display_roundtrip(ctx->disp), goto exit;)

  *out= ctx;
  rc  = 0;
exit:
  if(rc < 0)
  {
    if(ctx) ctx->err= DPYERR_INIT;
    dpyterm(ctx);
  }
  return rc;
}

int dpyinit_srf(dpyctx *ctx, dpysrf **out, dpydesc_srf desc)
{
  int    rc = -1;
  wlsrf *srf= NULL;

  NNUL(ctx && out, goto exit;)
  *out= NULL;

  srf= calloc(1, sizeof *srf);
  NNUL(srf, goto exit;)

  srf->xdgsrflstn= (struct xdg_surface_listener){&xdgsrf_h};
  srf->toplvllstn= (struct xdg_toplevel_listener){
    &toplvlcnf_h, &toplvlclose_h, &toplvlbounds_h, &toplvlcpb_h};
  srf->w_pref= (i32)desc.w;
  srf->h_pref= (i32)desc.h;

  signed res;

  srf->surf= wl_compositor_create_surface(ctx->comp);
  NNUL(srf->surf, goto exit;)

  srf->xdgsrf= xdg_wm_base_get_xdg_surface(ctx->shell, srf->surf);
  NNUL(srf->xdgsrf, goto exit;)
  res= xdg_surface_add_listener(srf->xdgsrf, &srf->xdgsrflstn, srf);
  OK(res, goto exit;)

  srf->toplvl= xdg_surface_get_toplevel(srf->xdgsrf);
  NNUL(srf->toplvl, goto exit;)
  res= xdg_toplevel_add_listener(srf->toplvl, &srf->toplvllstn, srf);
  OK(res, goto exit;)

  if(desc.title)
  {
    xdg_toplevel_set_title(srf->toplvl, desc.title);
    xdg_toplevel_set_app_id(srf->toplvl, desc.title);
  }

  wl_surface_commit(srf->surf);
  res= wl_display_roundtrip(ctx->disp);
  OK(res, goto exit;)
  wl_surface_commit(srf->surf);

  *out= srf;
  rc  = 0;
exit:
  if(rc < 0) dpyterm_srf(ctx, srf);
  return rc;
}

void dpyterm_srf(dpyctx *ctx, dpysrf *srf)
{
  NNUL(ctx, return;)
  if(!srf) return;

  if(srf->toplvl) xdg_toplevel_destroy(srf->toplvl);
  if(srf->xdgsrf) xdg_surface_destroy(srf->xdgsrf);
  if(srf->surf) wl_surface_destroy(srf->surf);
  memset(srf, 0, sizeof *srf);
  free(srf);
}

void dpyterm(dpyctx *ctx)
{
  if(!ctx) return;

  if(ctx->shm) wl_shm_destroy(ctx->shm);
  if(ctx->shell) xdg_wm_base_destroy(ctx->shell);
  if(ctx->comp) wl_compositor_destroy(ctx->comp);
  if(ctx->reg) wl_registry_destroy(ctx->reg);
  if(ctx->disp) wl_display_disconnect(ctx->disp);
  memset(ctx, 0, sizeof *ctx);
  free(ctx);
}

int dpypoll(dpyctx *ctx)
{
  int rc    = -1;
  int cancel= 0;
  NNUL(ctx && ctx->disp, goto exit;)

  while(wl_display_prepare_read(ctx->disp) == -1)
    OK(wl_display_dispatch_pending(ctx->disp), goto exit;)
  cancel= 1;  // failures from here on should cancel the read

  int events= POLLIN;
  {
    int rc= wl_display_flush(ctx->disp);
    if(rc == -1 && errno == EAGAIN)
      events|= POLLOUT;
    else if(rc < 0)
      goto exit;
  }

  int fd= wl_display_get_fd(ctx->disp);
  OK(fd, goto exit;)

  struct pollfd e= {.fd= fd, .events= events};
  OK(poll(&e, 1, 0), goto exit;)

  if(e.revents & POLLOUT)
    if(wl_display_flush(ctx->disp) == -1 && errno != EAGAIN) goto exit;
  if(e.revents & POLLIN)
  {
    cancel= 0;  // we read here, therefore no cancelling is required
    if(wl_display_read_events(ctx->disp) == 0)
    {  // events were read successfully
      OK(wl_display_dispatch_pending(ctx->disp), goto exit;)
    }
    else
      goto exit;
  }
  if(e.revents & (POLLERR | POLLHUP | POLLNVAL)) goto exit;

  rc= 0;
exit:
  if(cancel && ctx && ctx->disp) wl_display_cancel_read(ctx->disp);
  return rc;
}

int dpyqry_err(dpyctx *ctx) { return ctx ? (int)ctx->err : -1; }

const char *dpyqry_errstr(DPYERR err)
{
  const char *str= "unknown display error";

  if(err == DPYERR_INIT) str= "failed to initialize display";
  return str;
}

void dpyset_usrdata(dpysrf *srf, void *usr) { srf->usrdata= usr; }

void dpyset_onrsz(dpysrf *srf, resizefn_t cb) { srf->rszcb= cb; }
void dpyset_onclose(dpysrf *srf, closefn_t cb) { srf->clscb= cb; }

static void bufrel_h(void *data, struct wl_buffer *)
{
  if(!data) return;

  wlbuf *buf= data;
  buf->free = 1;
}
const static struct wl_buffer_listener buflstn= {.release= bufrel_h};
int dpyinit_swp(dpyctx *dpy, dpysrf *srf, dpyswp **out, dpydesc_swp d)
{
  int    rc = -1;
  wlswp *swp= NULL;
  NNUL(out, goto err;)
  *out= NULL;
  NNUL(dpy && srf, goto err;)        // invalid input
  NNUL(dpy->shm, rc= -2; goto err;)  // no capability

  swp= calloc(1, sizeof *swp);
  NNUL(swp, rc= -3; goto err;)  // OOM
  swp->mfd= -1;

  // match format
  imgfmt f= IMG_DEFAULT_FMT;
  if(d.fmt == IMG_DEFAULT_FMT)
    f= ARGB8;
  else
    for(usize i= 0; i < dpy->nrfmts; i++)
      if(dpy->fmts[i] == d.fmt) f= d.fmt;

  if(d.w == 0) d.w= srf->w_configured;
  if(d.h == 0) d.h= srf->h_configured;

  usize bytes= imgqry_imgbytes(f, d.w, d.h);
  NNUL(bytes, goto err;)
  u32 nrbufs= 3;  // arbitrary default

  IF(bytes > INT32_MAX / nrbufs, goto err;)
  swp->bytesmapped= bytes * nrbufs;

  swp->mfd= memfd_create("myballs", MFD_CLOEXEC);
  OK(swp->mfd, goto err;)

  OK(ftruncate(swp->mfd, swp->bytesmapped), goto err;)

  swp->mapped= mmap(
    NULL, swp->bytesmapped, PROT_WRITE | PROT_READ, MAP_SHARED, swp->mfd, 0);
  IF(swp->mapped == MAP_FAILED, goto err;)

  swp->w= d.w, swp->h= d.h;
  swp->fmt= f;

  swp->pool= wl_shm_create_pool(dpy->shm, swp->mfd, swp->bytesmapped);
  NNUL(swp->pool, goto err;)

  for(usize i= 0; i < nrbufs; i++)
  {
    i32               offset= bytes * i;
    struct wl_buffer *hnd   = wl_shm_pool_create_buffer(
      swp->pool,
      offset,
      swp->w,
      swp->h,
      swp->w * 4 /*XXX this assumes 4 bytes per pixel, fix*/,
      imgfmt2wl(swp->fmt));
    NNUL(hnd, goto err;)
    swp->bufs[i]= (wlbuf){
      .data  = swp->mapped + offset,
      .free  = 1,
      .h     = swp->h,
      .w     = swp->w,
      .stride= swp->w * 4,
      .hnd   = hnd};
    swp->nrbufs++;
    OK(wl_buffer_add_listener(swp->bufs[i].hnd, &buflstn, &swp->bufs[i]),
       goto err;)
  }

  *out= swp;
  return 0;
err:
  dpyterm_swp(swp);
  return rc;
}

void dpyterm_swp(dpyswp *swp)
{
  if(!swp) return;

  for(usize i= 0; i < swp->nrbufs; i++) wl_buffer_destroy(swp->bufs[i].hnd);

  if(swp->pool) wl_shm_pool_destroy(swp->pool);

  if(swp->mapped && swp->mapped != MAP_FAILED)
    munmap(swp->mapped, swp->bytesmapped);
  if(swp->mfd >= 0) close(swp->mfd);

  memset(swp, 0, sizeof *swp);
  free(swp);
}

imgview dpyacq_nextbuf(dpyswp *swp)
{
  imgview v= {0};
  NNUL(swp, goto err;)
  wlbuf *buf= &swp->bufs[swp->prsidx];
  if(!buf->free) goto err;

  v= (imgview){
    .data  = buf->data,
    .fmt   = swp->fmt,
    .pxh   = buf->h,
    .pxw   = buf->w,
    .stride= buf->stride};

  buf->acq= 1;
err:
  return v;
}
void dpyrel_buf(dpyswp *swp)
{
  NNUL(swp, return;)

  swp->bufs[swp->prsidx].acq= 0;
  swp->prsidx               = (swp->prsidx + 1) % swp->nrbufs;
}
int dpypresent(dpyctx *dpy, dpysrf *srf, dpyswp *swp)
{
  int rc= -1;
  NNUL(dpy && srf && swp, goto err;)

  wlbuf *buf= &swp->bufs[swp->prsidx];

  // buffer must be freed by the compositor, but must've been acquired by the
  // user without relinquishing
  if(!buf->free || !buf->acq) goto err;

  buf->acq= 0; // user relinquished buf

  wl_surface_attach(srf->surf, buf->hnd, 0, 0);
  wl_surface_damage_buffer(srf->surf, 0, 0, INT32_MAX, INT32_MAX);
  wl_surface_commit(srf->surf);

  buf->free= 0; // comp owns buf now

  swp->prsidx = (swp->prsidx + 1) % swp->nrbufs;
  rc= 0;
err:
  return rc;
}
