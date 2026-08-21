// Build from the repository root with MSVC:
// cl /nologo /std:c11 /W4 /I. src\win32\win32.c src\win32\tests\test_win32.c user32.lib /Fe:win32_test.exe

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <stdio.h>

#include "include/display.h"

typedef struct
{
  i32 width, height;
  int resized;
  int closed;
} teststate;

static void on_resize(i32 width, i32 height, void *usrdata)
{
  teststate *state= usrdata;

  state->width  = width;
  state->height = height;
  state->resized= 1;
}

static void on_close(void *usrdata)
{
  teststate *state= usrdata;
  state->closed= 1;
}

int main(void)
{
  int        rc   = -1;
  dpyctx    *dpy  = NULL;
  HWND       wnd  = NULL;
  teststate  state= {0};
  RECT       client;

  if(dpyinit(&dpy, "CDR Win32 test", 320, 240, 0) < 0)
  {
    fprintf(stderr, "dpyinit failed\n");
    goto exit;
  }

  wnd= dpyget_nwin(dpy);
  if(!wnd || !dpyget_ncon(dpy))
  {
    fprintf(stderr, "native handle query failed\n");
    goto exit;
  }

  dpyset_usrdata(dpy, &state);
  dpyset_onrsz(dpy, on_resize);
  dpyset_onclose(dpy, on_close);

  if(!SetWindowPos(
       wnd,
       NULL,
       0,
       0,
       640,
       360,
       SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE))
  {
    fprintf(stderr, "SetWindowPos failed: %lu\n", GetLastError());
    goto exit;
  }

  if(dpypoll(dpy) < 0)
  {
    fprintf(stderr, "dpypoll failed after resize\n");
    goto exit;
  }
  if(!GetClientRect(wnd, &client))
  {
    fprintf(stderr, "GetClientRect failed: %lu\n", GetLastError());
    goto exit;
  }

  i32 expected_width = (i32)(client.right - client.left);
  i32 expected_height= (i32)(client.bottom - client.top);
  if(!state.resized ||
     state.width != expected_width ||
     state.height != expected_height)
  {
    fprintf(
      stderr,
      "unexpected resize event: seen=%d, size=%ldx%ld\n",
      state.resized,
      (long)state.width,
      (long)state.height);
    goto exit;
  }

  if(!PostMessageA(wnd, WM_CLOSE, 0, 0))
  {
    fprintf(stderr, "PostMessageA failed: %lu\n", GetLastError());
    goto exit;
  }
  if(dpypoll(dpy) < 0)
  {
    fprintf(stderr, "dpypoll failed after close request\n");
    goto exit;
  }
  if(!state.closed)
  {
    fprintf(stderr, "close callback was not invoked\n");
    goto exit;
  }

  rc= 0;
exit:
  dpyterm(dpy);
  if(rc == 0) printf("Win32 display test passed.\n");
  return rc;
}
