#include "defs.h"
#include "raster.h"

//`out` must point to in.pxw*in.pxh*bpp(in)*mag^2 bytes
struct raster upsample(const struct raster *in, byte *out, uint mag)
{
    const uint bpp = bytespp(in->fmt);
    
    struct raster res = {out, in->pxw*bpp*mag,
        in->pxw*mag, in->pxh*mag, in->fmt};

    for(uint i=0; i < res.pxh; i++)
        for(uint j=0; j < res.pxw; j++)
        {
            const byte *p_in = at(in, i/mag, j/mag);
            byte *p_out = at(&res, i, j);
            for(uint k=0; k < bpp; k++)
                p_out[k]=p_in[k];
        }
    return res;
}
