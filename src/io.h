#pragma once

#include <stdio.h>

#include "defs.h"

int prnbppm(
    const struct rgb24 *px, 
    unsigned int w, 
    unsigned int h, 
    byte *img, 
    unsigned int imgcap);

int fwritebppm(
    FILE *f, 
    const struct rgb24 *px, 
    unsigned int w, 
    unsigned int h);
