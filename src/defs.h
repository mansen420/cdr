#pragma once

#include <math.h>

struct rgbaf32{float r, g, b, a;};

struct rgbf32{float r, g, b;};

typedef unsigned char byte;
typedef unsigned int uint;
typedef unsigned int uint;

struct rgb24 {byte r, g, b;};
struct rgba32 {byte r, g, b, a;};

struct range{float lo, hi;};

static const struct range NORM = {.lo=0.f, .hi=1.f};
static const struct range CANON = {.lo=-1.f, .hi=1.f};

enum rfmt{FMT_RGB24, FMT_RGBA32};
struct raster
{
    //offset view into the buffer
    byte *buf;
    //bytes per row of the underlying view
    uint stride;
    //width and height in pixels
    uint pxw, pxh;
    //data format
    enum rfmt fmt;
};

struct raster_4u8
{
    struct rgba32 *buf;
    uint stride; //=pxw*sizeof(*buf)
    uint pxw, pxh;
};

struct raster_3u8
{
    struct rgb24 *buf;
    uint stride; //=pxw*sizeof(*buf)
    uint pxw, pxh;
};

struct raster_3f32
{
    struct rgbf32 *buf;
    uint stride; //=pxw*sizeof(*buf)
    uint pxw, pxh;
};

struct raster_4f32
{
    struct rgbaf32 *buf;
    uint stride; //=pxw*sizeof(*buf)
    uint pxw, pxh;
};

static inline float span(struct range r){return r.hi - r.lo;}

static inline int zeroeps(float f, float eps){return fabsf(f)<=eps;}
static inline int zero(float f){return zeroeps(f, 1e-8f);}

static inline float linmap(
    float x,
    struct range fro,
    struct range to)
{
    if(zero(span(fro))) return to.lo;
    return span(to)/span(fro) * (x - fro.lo) + to.lo;
}
static inline void linmapf3(
    const float in[3], 
    float out[3], 
    struct range fro, 
    struct range to)
{
    out[0]=linmap(in[0], fro, to);
    out[1]=linmap(in[1], fro, to);
    out[2]=linmap(in[2], fro, to);
}

static inline struct rgbaf32 rgb32c(struct rgba32 px)
{
    //px values 0..255, map to [0, 1]
    return (struct rgbaf32){
        px.r * (1.0f/255.0f),
        px.g * (1.0f/255.0f),
        px.b * (1.0f/255.0f),
        px.a * (1.0f/255.0f)};
}

//convert `n` rgba32 pixels to rgb24
static inline void px32to24(
    const struct rgba32 *restrict px32, 
    struct rgb24 *restrict px24, 
    unsigned int n)
{
    const struct rgba32* end = px32+n;
    while(px32 != end)
        *px24++ = (struct rgb24){px32->r, px32->g, px32->b}, px32++;
}
