#pragma once

#include "include/defs.h"

int fw_ppm(const char *path, struct raster_3u8 img);

// reads path into caller-allocated *out, truncating to out's pxw/pxh when the
// file is larger. call fqry_ppmdim first if you need to size the buffer exactly.
int fr_ppm(const char *path, struct raster_3u8 *out);
int fqry_ppmdim(const char *path, unsigned *w, unsigned *h);

isize fqry_sz(const char *path);
int   fr_b(const char *path, u8 *buf, usize cap);
int   fw_spvh(const char *rpath, const char *wpath);
