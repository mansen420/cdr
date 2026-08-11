#pragma once

#include "include/defs.h"

typedef struct
{
  char path[4096];
  // cflags
} bldtc;

typedef struct
{
  char        *list[64];
  unsigned int nr;
} bldstrlist;

typedef struct bldextdep bldextdep;
struct bldextdep
{
	// name, used for diagnostics
	char name[64];

  bldstrlist cflags;
  bldstrlist lflags;

  const bldextdep *deps[32];
  u32              nrdeps;
};

// these are compiled in the project
typedef struct bldmod bldmod;
struct bldmod
{
  // c language files, compiled
  bldstrlist sources;
  // private headers containing implementation details, not compiled directly
  bldstrlist implh;
  // public headers this module declares, not compiled
  bldstrlist ifaceh;

  char name[64];  // logical name of module

  const bldmod *deps[32];
  u32           nrdeps;

  // special flags this module needs to build, use sparingly
  bldstrlist cflags;

  const bldextdep *extdeps[32];
  u32              nrextdeps;
};

int bldcmpl(bldtc *cc, const char *path, const char *outpath);
int bldarch(const char *path, bldstrlist objs, const char *outpath);
