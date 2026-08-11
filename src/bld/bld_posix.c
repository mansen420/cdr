#include "src/bld/bld.h"

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int run(const char *const argv[])
{
  int rc= -1;

  pid_t pid= fork();
  if(pid < 0)
  {
    perror("fork");
    goto exit;
  }
  if(pid == 0)
  {
    execvp(argv[0], (char *const *)argv);
    perror(argv[0]);
    _exit(127);
  }

  int   status;
  pid_t waited;
  do
  {
    waited= waitpid(pid, &status, 0);
  } while(waited < 0 && errno == EINTR);

  if(waited < 0)
  {
    perror("waitpid");
    goto exit;
  }

  if(WIFEXITED(status))
    rc= WEXITSTATUS(status);
  else if(WIFSIGNALED(status))
    fprintf(stderr, "%s killed by signal: %d\n", argv[0], WTERMSIG(status));

exit:
  return rc;
}

int bldcmpl(bldtc *cc, const char *path, const char *outpath)
{
  const char *argv[]= {
    cc->path,
    "-c",
    path,
    "-I.",
    "-o",
    outpath,
    NULL,
  };
  return run(argv);
}

// TODO assert nrobjs <= 64
int bldarch(const char *path, bldstrlist objs, const char *outpath)
{
  const char *argv[128];
  unsigned    argc= 0;
  argv[argc++]    = path;
  argv[argc++]    = "rcs";
  argv[argc++]    = outpath;
  for(unsigned i= 0; i < objs.nr; i++) argv[argc++]= objs.list[i];
  argv[argc]= NULL;
  return run(argv);
}

int bldlink(void);
