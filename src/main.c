#include "include/display.h"
#include "include/graphics.h"
#include "include/repl.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static volatile int quit= 0;

static void on_close(void *u)
{
  (void)u;
  quit= 1;
}
static void on_resize(i32 w, i32 h, void *gfx) { gfxupdate(gfx, w, h); }

int main(int argc, char **argv)
{
  int      rc  = -1;
  u32     *spv = NULL;
  gfxctx  *gfx = NULL;
  dpyctx  *dpy = NULL;
  replctx *repl= NULL;

  if(dpyinit(&dpy, "CDR", 800, 600, 0) < 0) goto exit;
  if(gfxinit(&gfx, dpy) < 0) goto exit;

  // prepare shader
  {
    usize       nrbytes= 0;
    const char *path   = argc > 1 ? argv[1] : "./assets/shader.spv";

    int fd= open(path, O_RDONLY);
    if(fd == -1) goto exit;

    struct stat st;
    fstat(fd, &st);
    nrbytes= st.st_size;

    spv= malloc(nrbytes);
    read(fd, spv, nrbytes);

    close(fd);

    if(gfxprep(gfx, spv, nrbytes) < 0) goto exit;
    free(spv);
    spv= NULL;
  }

  dpyset_onrsz(dpy, on_resize);
  dpyset_onclose(dpy, on_close);
  dpyset_usrdata(dpy, gfx);

  if(replinit(&repl, gfx) < 0) goto exit;

  while(!quit)
  {
		dpypoll(dpy);
    if(gfxrender(gfx, NULL, 0) < 0) break;
  }
  // reploop(repl);

  rc= 0;
exit:
  replterm(repl);
  gfxterm(gfx);
  dpyterm(dpy);
  free(spv);
  return rc;
}
