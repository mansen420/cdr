#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io.h"

int prnbppm(const struct rgb24 *px, unsigned int w, unsigned int h, byte *img, unsigned int imgcap)
{
    const int hdr = snprintf((char*)img, imgcap, "P6\n%u %u\n255\n", w, h);
    if(hdr<0) return -1;
    if(hdr>=imgcap) return -2;
    
    const unsigned int binsize = 3*w*h;
    if(hdr + binsize > imgcap) return -2;

    memcpy(img + hdr, px, binsize); 
    return binsize+hdr;
}

int fwritebppm(FILE *f, const struct rgb24 *px, unsigned int w, unsigned int h)
{
    int hdr = fprintf(f, "P6\n%u %u\n255\n", w, h);
    if(hdr<0) return -1;
    size_t written = fwrite(px, sizeof(struct rgb24), w*h, f);
    
    if(written != w*h) return -2;
    return 0;
}
