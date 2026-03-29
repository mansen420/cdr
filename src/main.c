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

typedef struct
{
  t2f32 pos;
  t3f32 color;
} vertex;
int main(int argc, char **argv)
{
  int      rc  = -1;
  u32     *spv = NULL;
  gfxctx  *gfx = NULL;
  dpyctx  *dpy = NULL;
  replctx *repl= NULL;
  gfxbuf  *vtx = NULL;
  gfxppl  *ppl = NULL;
  gfxpass *pass= NULL;

  if(dpyinit(&dpy, "CDR", 800, 600, 0) < 0) goto exit;
  if(gfxinit(&gfx, dpy, (gfxdesc){0}) < 0) goto exit;

  vertex triangle[3]= {
    {.pos= {.x= 0.0f, .y= -0.5f}, .color= {.r= 1.f, .g= 0.f, .b= 0.f}},
    {.pos= {.x= 0.5f, .y= 0.5f}, .color= {.r= 0.f, .g= 1.f, .b= 0.f}},
    {.pos= {.x= -0.5f, .y= 0.5f}, .color= {.r= 0.f, .g= 0.f, .b= 1.f}}};

  gfxdesc_vtxbuf vtxdesc= {
    .attrs=
      {(gfxvtxattr){.fmt= GFXFMT_2F32, .offset= 0},
       (gfxvtxattr){.fmt= GFXFMT_3F32, .offset= 8}},
    .nrattrs  = 2,
    .nrvtx    = 3,
    .vtxstride= sizeof *triangle};
  if(gfxinit_vtxbuf(gfx, &vtx, NULL, vtxdesc, triangle) < 0) goto exit;

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

    if(
      gfxinit_ppl(
        gfx,
        &ppl,
        (gfxdesc_ppl){.spv= spv, .nrbytes= nrbytes, .vtxdesc= vtxdesc}) < 0)
      goto exit;

    free(spv);
    spv= NULL;
  }
  if(
    gfxinit_pass(
      gfx,
      &pass,
      (gfxdesc_pass){.outputs= {gfxget_display(gfx)}, .nroutputs= 1}) < 0)
    goto exit;

  dpyset_onrsz(dpy, on_resize);
  dpyset_onclose(dpy, on_close);
  dpyset_usrdata(dpy, gfx);

  if(replinit(&repl, gfx) < 0) goto exit;

  while(!quit)
  {
    dpypoll(dpy);
    gfxcmd_begin(pass);
    gfxcmd_draw(pass, (gfxdesc_drawcmd){.ppl= ppl, .vtx= vtx, .nrvtx= 3});
    gfxcmd_end(pass);
    if(gfxsubmit(gfx, pass) < 0) goto exit;
  }
  // reploop(repl);

  rc= 0;
	gfxsync(gfx);
exit:
  replterm(repl);
	gfxterm_pass(gfx, pass);
	gfxterm_ppl(gfx, ppl);
	gfxterm_buf(gfx, vtx);
  gfxterm(gfx);
  dpyterm(dpy);
  free(spv);
  return rc;
}
