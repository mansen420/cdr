#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <stdlib.h>
#include <windows.h>

#include "include/display.h"
#include "src/core/include/debug.h"

typedef struct dpyctx
{
  WNDCLASSEXA wndc;
  ATOM        wndcatm;
  HINSTANCE   prog;
  u32         flags;
  u32         err;

  dpysrf *srflst;
} win32ctx;

typedef struct dpysrf
{
  HWND       wnd;
  resizefn_t rszcb;
  closefn_t  clscb;
  void      *usrdata;

  win32srf *next;
  dpyctx   *parent;
} win32srf;

static LRESULT CALLBACK
wndproc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  win32srf *ctx= NULL;
  LRESULT   res= 0;

  if(msg == WM_NCCREATE)
  {
    ctx= ((CREATESTRUCTA *)(lparam))->lpCreateParams;
    if(!ctx) goto exit;

    ctx->wnd= wnd;
    SetWindowLongPtrA(wnd, GWLP_USERDATA, (LONG_PTR)ctx);

    res= TRUE;
    goto exit;
  }

  ctx= (win32srf *)GetWindowLongPtrA(wnd, GWLP_USERDATA);
  if(!ctx) goto def;

  if(msg == WM_SIZE)
  {
    i32 w= (i32)LOWORD(lparam);
    i32 h= (i32)HIWORD(lparam);

    if(wparam == SIZE_MINIMIZED) goto exit;
    if(w && h && ctx->rszcb) ctx->rszcb(w, h, ctx->usrdata);
    goto exit;
  }
  if(msg == WM_CLOSE)
  {
    if(ctx->clscb) ctx->clscb(ctx->usrdata);
    goto exit;
  }
  if(msg == WM_PAINT)
  {
    PAINTSTRUCT paint;

    BeginPaint(wnd, &paint);
    EndPaint(wnd, &paint);
    goto exit;
  }
  if(msg == WM_NCDESTROY)
  {
    SetWindowLongPtrA(wnd, GWLP_USERDATA, 0);
    ctx->wnd= NULL;
    goto def;
  }
  goto def;

def:
  res= DefWindowProcA(wnd, msg, wparam, lparam);
exit:
  return res;
}

int dpyinit(
  win32ctx **out, const char *title, unsigned width, unsigned height, u32 flags)
{
  int       rc = -1;
  win32ctx *ctx= NULL;

  NNUL(out, goto exit;)
  *out= NULL;

  ctx= calloc(1, sizeof *ctx);
  NNUL(ctx, goto exit;)
  ctx->flags= flags;

  ctx->prog= GetModuleHandleA(NULL);
  IF(ctx->prog == NULL, goto exit;)

  ctx->wndc= (WNDCLASSEXA){
    .lpszClassName= "mansen/cdrwnd",
    .lpfnWndProc  = wndproc,
    .hInstance    = ctx->prog,
    .cbSize       = sizeof(ctx->wndc),
    .hCursor      = LoadCursorA(NULL, IDC_ARROW)};
  ctx->wndcatm= RegisterClassExA(&ctx->wndc);
  IF(!ctx->wndcatm && GetLastError() != ERROR_CLASS_ALREADY_EXISTS, goto exit;)

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

int dpyinit_srf(
  dpyctx *ctx, dpysrf **out, u32 w, u32 h, u32 x, u32 y, const char *title)
{
  int       rc = -1;
  win32srf *srf= NULL;

  NNUL(ctx && out, goto exit;)
  *out= NULL;

  srf= calloc(1, sizeof *srf);
  NNUL(srf, goto exit;)

  srf->wnd= CreateWindowExA(
    0,
    ctx->wndc.lpszClassName,
    title,
    WS_OVERLAPPED,  // or use POPUP?
    (int)x,
    (int)y,
    (int)w,
    (int)h,
    NULL,
    NULL,
    ctx->prog,
    srf);
  IF(srf->wnd == NULL, goto exit;)

  ShowWindow(srf->wnd, SW_SHOW);

  *out= srf;
  {  // add out to parent's surface list
    win32srf *p= ctx->srflst, *prev;
    while(p) prev= p++;
    prev->next= out;
  }
  rc= 0;
exit:
  if(rc < 0)
  {
    if(srf && srf->wnd) DestroyWindow(srf->wnd);
    free(srf);
  }
  return rc;
}

void dpyterm_srf(dpyctx *ctx, dpysrf *srf)
{
  NNUL(ctx, return;)
  if(!srf) return;

  {
    win32srf *p= ctx->srflst, *prev;
    while(p)
    {
      prev= p;
      if(p++ == srf) break;
    }
    NNUL(p, return;)
    prev->next= p->next;
  }

  if(srf->wnd) DestroyWindow(srf->wnd);
  free(srf);
}

void dpyterm(dpyctx *ctx)
{
  if(!ctx) return;

  {
    win32srf *p= ctx->srflst;
    while(p) dpyterm_srf(ctx, p++);
    ctx->srflst= NULL;
  }
  if(ctx->wndcatm) UnregisterClassA(ctx->wndc.lpszClassName, ctx->prog);

  free(ctx);
}

int dpypoll(dpysrf *ctx)
{
  int rc= -1;
  MSG msg;

  NNUL(ctx, goto exit;)

  while(PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
  {
    if(msg.message == WM_QUIT)
    {
      if(ctx->clscb) ctx->clscb(ctx->usrdata);
      continue;
    }

    TranslateMessage(&msg);
    DispatchMessageA(&msg);
  }

  rc= 0;
exit:
  return rc;
}

void *dpyget_nwin(dpysrf *ctx) { return ctx ? ctx->wnd : NULL; }
void *dpyget_ncon(dpyctx *ctx) { return ctx ? ctx->prog : NULL; }

int dpyget_err(dpyctx *ctx) { return ctx ? (int)ctx->err : -1; }

const char *dpyget_errstr(DPYERR err)
{
  const char *str= "unknown display error";

  if(err == DPYERR_INIT) str= "failed to initialize display";
  return str;
}

void dpyset_usrdata(dpysrf *ctx, void *usr) { ctx->usrdata= usr; }
void dpyset_onrsz(dpysrf *ctx, resizefn_t cb) { ctx->rszcb= cb; }
void dpyset_onclose(dpysrf *ctx, closefn_t cb) { ctx->clscb= cb; }