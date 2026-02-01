#include "raster.h"
#include "defs.h"
#include <string.h>


enum rfmt
{
	FMT_RGB24,
	FMT_RGBA32
};
struct raster
{
	// offset view into the buffer
	byte *buf;
	// bytes per row of the underlying view
	uint stride;
	// width and height in pixels
	uint pxw, pxh;
	// data format
	enum rfmt fmt;
};

static inline uint bytespp(enum rfmt f)
{
	switch (f)
	{
	case FMT_RGB24:
		return 3;
	case FMT_RGBA32:
		return 4;
	default:
		return 0;
	}
}

// returns address of pixel at specified row and column
// works with offsets
static inline byte *
at(const struct raster *r, uint row, uint col)
{
	return r->buf + (r->stride * row + col * bytespp(r->fmt));
}

//`out` must point to in.pxw*in.pxh*bpp(in)*mag^2 bytes
struct raster
upsample(const struct raster * in, byte *restrict out, uint mag)
{
	const uint bpp = bytespp(in->fmt);
    const byte *restrict src=in->buf;

	struct raster res = {out,
	                     in->pxw * bpp * mag,
	                     in->pxw * mag,
	                     in->pxh * mag,
	                     in->fmt};
	if (mag == 0) return res;
	if (mag == 1)
	{
		if (in->stride == in->pxw * bpp) // tight packing
			memcpy(out, src, in->pxh * in->pxw * bpp);
		else
			for (uint i = 0; i < in->pxh; i++)
				memcpy(
				    out, src + in->stride * i, in->pxw * bpp),
				    out += in->pxw * bpp;
		return res;
	}

	for (uint i = 0; i < in->pxh; i++)
	{
		byte *out_rowstart = out;
		for (uint j = 0; j < in->pxw; j++)
			// emit horizontal magnified row
			for (uint kx = 0; kx < mag; kx++)
			{
				const byte *srcpx =
				    (src + in->stride * i + j * bpp);
				for (uint l = 0; l < bpp; l++) *out++ = srcpx[l];
			}
		ptrdiff_t rowlen = out - out_rowstart;
		for (uint ky = 0; ky < mag - 1; ky++)
			memcpy(out, out_rowstart, rowlen), out += rowlen;
	}
	return res;
}

struct raster_4u8 upsample_4u8(const struct raster_4u8 *in,
                               struct rgba32 *restrict out,
                               uint mag)
{
	typedef struct rgba32 px_t;
	typedef struct raster_4u8 ret_t;
    const px_t *restrict src=in->buf;

	ret_t res = {
	    out, in->pxw * mag, in->pxw * mag, in->pxh * mag};
	if (mag == 0) return res;
	if (mag == 1)
	{
		if (in->stride == in->pxw) // tight packing
			memcpy(
			    out, src, in->pxh * in->pxw * sizeof(px_t));
		else
			for (uint i = 0; i < in->pxh; i++)
				memcpy(out,
				       src + in->stride * i,
				       in->pxw * sizeof(px_t)),
				    out += in->pxw;
		return res;
	}

	for (uint i = 0; i < in->pxh; i++)
	{
		px_t *out_rowstart = out;
		for (uint j = 0; j < in->pxw; j++)
		{
			const px_t srcpx = *(src + in->stride * i + j);
			// emit horizontal magnified row
			for (uint kx = 0; kx < mag; kx++) *out++ = srcpx;
		}
		ptrdiff_t rowlen = out - out_rowstart;
		for (uint ky = 0; ky < mag - 1; ky++)
			memcpy(out, out_rowstart, rowlen * sizeof(px_t)),
			    out += rowlen;
	}
	return res;
}

struct raster_3u8 upsample_3u8(const struct raster_3u8 *in,
                               struct rgb24 *restrict out,
                               uint mag)
{
	typedef struct rgb24 px_t;
	typedef struct raster_3u8 ret_t;
    const px_t *restrict src=in->buf;

	ret_t res = {
	    out, in->pxw * mag, in->pxw * mag, in->pxh * mag};
	if (mag == 0) return res;
	if (mag == 1)
	{
		if (in->stride == in->pxw) // tight packing
			memcpy(
			    out, src, in->pxh * in->pxw * sizeof(px_t));
		else
			for (uint i = 0; i < in->pxh; i++)
				memcpy(out,
				       src + in->stride * i,
				       in->pxw * sizeof(px_t)),
				    out += in->pxw;
		return res;
	}

	for (uint i = 0; i < in->pxh; i++)
	{
		px_t *out_rowstart = out;
		for (uint j = 0; j < in->pxw; j++)
		{
			const px_t srcpx = *(src + in->stride * i + j);
			// emit horizontal magnified row
			for (uint kx = 0; kx < mag; kx++) *out++ = srcpx;
		}
		ptrdiff_t rowlen = out - out_rowstart;
		for (uint ky = 0; ky < mag - 1; ky++)
			memcpy(out, out_rowstart, rowlen * sizeof(px_t)),
			    out += rowlen;
	}
	return res;
}
