#include "include/display.h"
#include "include/graphics.h"
#include "src/debug.h"

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
  gfxbuf  *vtx = NULL;
  gfxppl  *ppl = NULL;
  gfxpass *pass= NULL;
  gfximg  *fb  = NULL;

  OK(dpyinit(&dpy, "CDR", 800, 600, 0), goto exit;)
  OK(gfxinit(&gfx, dpy, (gfxdesc){0}), goto exit;)

  vertex triangle[3]= {
    {.pos= {.x= 0.0f, .y= -0.5f}, .color= {.r= 1.f, .g= 0.f, .b= 0.f}},
    {.pos= {.x= 0.5f, .y= 0.5f}, .color= {.r= 0.f, .g= 1.f, .b= 0.f}},
    {.pos= {.x= -0.5f, .y= 0.5f}, .color= {.r= 0.f, .g= 0.f, .b= 1.f}}};

  gfxdesc_bufbind vtxbind= {
    .binding= 0,
    .stride = sizeof *triangle,
    .rate   = GFX_RATE_VERTEX,
    .attrs=
      {(gfxdesc_attr){.location= 0, .fmt= GFXFMT_2F32, .offset= 0},
       (gfxdesc_attr){.location= 1, .fmt= GFXFMT_3F32, .offset= 8}},
    .nrattrs= 2};
  gfxdesc_buf bufdesc= {.nrelems= 3, .stride= sizeof *triangle};
  gfxdesc_mem vtxdata= {.data= triangle, .bytes= sizeof triangle, .offset= 0};
  OK(gfxinit_buf(gfx, &vtx, NULL, bufdesc, vtxdata), goto exit;)

  // prepare shader
  {
    usize       nrbytes= 0;
    const char *path   = argc > 1 ? argv[1] : "./assets/shader.spv";

    int fd= open(path, O_RDONLY);
    OK(fd, goto exit;)

    struct stat st;
    fstat(fd, &st);
    nrbytes= st.st_size;

    spv= malloc(nrbytes);
    read(fd, spv, nrbytes);

    close(fd);

    OK(gfxinit_ppl(
         gfx,
         &ppl,
         (gfxdesc_ppl){
           .program=
             {{.spv      = spv,
               .nrbytes  = nrbytes,
               .stagebits= GFXSTAGE_VERT,
               .entry    = "vertMain"},
              {.spv      = spv,
               .nrbytes  = nrbytes,
               .stagebits= GFXSTAGE_FRAG,
               .entry    = "fragMain"}},
           .nrshaders = 2,
           .bindings  = {vtxbind},
           .nrbindings= 1}),
       goto exit;)

    free(spv);
    spv= NULL;
  }
  OK(
    gfxinit_img(
      gfx, &fb, NULL, (gfxdesc_img){.w= 800, .h= 600, .fmt= GFXFMT_BGRA8_SRGB}),
    goto exit;)
  OK(gfxinit_pass(gfx, &pass, (gfxdesc_pass){.outputs= {fb}, .nroutputs= 1}),
     goto exit;)

  dpyset_onrsz(dpy, on_resize);
  dpyset_onclose(dpy, on_close);
  dpyset_usrdata(dpy, gfx);

  while(!quit)
  {
    dpypoll(dpy);
    gfxcmd_begin(pass);
    gfxcmd_draw(
      pass,
      (gfxdesc_drawcmd){
        .ppl= ppl, .bufs= {{.binding= 0, .buf= vtx}}, .nrbufs= 1, .nrvtx= 3});
    gfxcmd_end(pass);
    OK(gfxsubmit(gfx, pass), goto exit;)
    OK(gfxpresent(gfx, fb), goto exit;)
  }

  rc= 0;
exit:
  gfxsync(gfx);
  gfxterm_pass(gfx, pass);
  gfxterm_ppl(gfx, ppl);
  gfxterm_buf(gfx, vtx);
  gfxterm_img(gfx, fb);
  gfxterm(gfx);
  dpyterm(dpy);
  free(spv);
  return rc;
}
