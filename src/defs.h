#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>

typedef unsigned char byte;
typedef unsigned int uint;
typedef ptrdiff_t isize;
typedef size_t usize;

typedef int32_t i32;
typedef int16_t i16;
typedef int64_t i64;
typedef int8_t i8;

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint64_t u64;
typedef uint8_t u8;

typedef float f32;
typedef double f64;

struct rgbaf32
{
	f32 r, g, b, a;
};

struct rgbf32
{
	f32 r, g, b;
};

struct rgb24
{
	byte r, g, b;
};
struct rgba32
{
	byte r, g, b, a;
};

struct range
{
	f32 lo, hi;
};

static const struct range NORM = {.lo = 0.f, .hi = 1.f};
static const struct range CANON = {.lo = -1.f, .hi = 1.f};

struct raster_4u8
{
	struct rgba32 *buf;
	uint stride; // elements of buf per row
	uint pxw, pxh;
};

struct raster_3u8
{
	struct rgb24 *buf;
	uint stride; // elements of buf per row
	uint pxw, pxh;
};

struct raster_3f32
{
	struct rgbf32 *buf;
	uint stride; // elements of buf per row
	uint pxw, pxh;
};

struct raster_4f32
{
	struct rgbaf32 *buf;
	uint stride; // elements of buf per row
	uint pxw, pxh;
};

static inline f32 span(struct range r)
{
	return r.hi - r.lo;
}

static inline int zeroeps(f32 f, f32 eps)
{
	return fabsf(f) <= eps;
}
static inline int zero(f32 f)
{
	return zeroeps(f, 1e-8f);
}

static inline f32
linmap(f32 x, struct range fro, struct range to)
{
	if (zero(span(fro))) return to.lo;
	return span(to) / span(fro) * (x - fro.lo) + to.lo;
}
static inline void linmapf3(const f32 in[3],
                            f32 out[3],
                            struct range fro,
                            struct range to)
{
	out[0] = linmap(in[0], fro, to);
	out[1] = linmap(in[1], fro, to);
	out[2] = linmap(in[2], fro, to);
}

static inline struct rgbaf32 rgb32c(const struct rgba32 px)
{
	// px values 0..255, map to [0, 1]
	return (struct rgbaf32){px.r * (1.0f / 255.0f),
	                        px.g * (1.0f / 255.0f),
	                        px.b * (1.0f / 255.0f),
	                        px.a * (1.0f / 255.0f)};
}

// convert `n` rgba32 pixels to rgb24
static inline void
px32to24(const struct rgba32 *restrict px32,
         struct rgb24 *restrict px24,
         uint n)
{
	const struct rgba32 *end = px32 + n;
	while (px32 != end)
		*px24++ = (struct rgb24){px32->r, px32->g, px32->b},
		px32++;
}
