#include "defs.h"
#include "io.h"
#include "raster.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
	// make sure pointers exist at fail:
	FILE *f = NULL;
	struct rgb24 *out = NULL;
	int rc = EXIT_FAILURE;

	uint mag = 10;
	if (argc > 1) mag = strtoul(argv[1], NULL, 10);

	f = fopen("out.ppm", "wb");
	if (!f) goto exit;

	struct rgb24 W = {255, 255, 255};
	struct rgb24 B = {0, 0, 0};
	struct rgb24 R = {255, 0, 0};
	struct rgb24 BL = {0, 0, 255};
	struct rgb24 px[] = {R, W, B, BL};

	struct raster_3u8 small = {px, 2, 2, 2};

	const int bytes =
	    small.pxh * small.pxw * sizeof(struct rgb24);
	out = malloc(bytes * mag * mag);
	if (!out) goto exit;
	struct raster_3u8 img = upsample_3u8(&small, out, mag);

	rc = fwritebppm(
	    f, (struct rgb24 *)img.buf, img.pxw, img.pxh);
exit:
	if (f) fclose(f);
	if (out) free(out);
	return rc;
}
