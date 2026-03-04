#define _GNU_SOURCE

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "include/display.h"
#include "include/defs.h"
#include "include/graphics.h"
#include "include/repl.h"

static replctx *init(gfxctx *gfx)
{
  replctx *ctx= calloc(1, sizeof *ctx);
  if(!ctx) return NULL;
  ctx->gfx= gfx;
  return ctx;
}

static void print_prompt(replctx *ctx)
{
  printf(ctx->paused ? "[paused]> " : "> ");
  fflush(stdout);
}

static void loop(replctx *ctx)
{
  print_prompt(ctx);

  char line[256];
  while(!ctx->quit && fgets(line, sizeof line, stdin))
  {
    char verb[64];
    int  argc= sscanf(line, "%63s", verb);
    if(argc < 1) { print_prompt(ctx); continue; }

    if(!strcmp(verb, "ls"))
    {
      puts("  pause     — pause rendering\n"
           "  resume    — resume rendering\n"
           "  quit      — exit");
    }
    else if(!strcmp(verb, "pause"))
    {
      gfxpause(ctx->gfx);
      ctx->paused= 1;
    }
    else if(!strcmp(verb, "resume"))
    {
      gfxresume(ctx->gfx);
      ctx->paused= 0;
    }
    else if(!strcmp(verb, "quit") || !strcmp(verb, "exit"))
    {
      puts("Farewell");
      ctx->quit= 1;
      continue;
    }
    else
      printf("unknown: %s\n", verb);

    print_prompt(ctx);
  }
}

static void term(replctx *ctx) { free(ctx); }

replctx *replinit(gfxctx *gfx) { return init(gfx); }
void     replterm(replctx *ctx) { term(ctx); }
void     reploop(replctx *ctx) { loop(ctx); }

static void *repl_thread_fn(void *arg) { reploop(arg); replterm(arg); return NULL; }

static volatile int quit= 0;
static void         on_close(void *u) { (void)u; quit= 1; }
static void         on_resize(i32 w, i32 h, void *gfx) { gfxupdate(gfx, w, h); }

int main(int argc, char **argv)
{
  int   rc     = 0;
  u32  *spv    = NULL;
  usize nrbytes= 0;

  {
    const char *path= argc > 1 ? argv[1] : "./assets/shader.spv";
    int         fd  = open(path, O_RDONLY);
    if(fd == -1)
    {
      puts("shader not found");
      return 1;
    }
    struct stat st;
    fstat(fd, &st);
    nrbytes= st.st_size;
    spv    = malloc(nrbytes);
    read(fd, spv, nrbytes);
    close(fd);
  }

  dpyctx *dpy= dpyinit("cdr", 800, 600, 0);
  gfxctx *gfx= gfxinit(dpy);

  if(gfxprep(gfx, spv, nrbytes) < 0) { rc= 1; goto exit; }
  free(spv);
  spv= NULL;

  dpysetrszcb(dpy, on_resize);
  dpysetclscb(dpy, on_close);
  dpysetusrdata(dpy, gfx);

  replctx  *repl= replinit(gfx);
  pthread_t thr;
  pthread_create(&thr, NULL, repl_thread_fn, repl);
  pthread_detach(thr);

  while(dpypoll(dpy) == 0 && !repl->quit && !quit)
  {
    int r= gfxrender(gfx);
    if(r < 0) break;
  }

  repl->quit= 1;

exit:
  gfxterm(gfx);
  dpyterm(dpy);
  free(spv);
  return rc;
}
