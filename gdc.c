/* gdc.c -- greatest common divisor.
 * Author: Luis Colorado <luiscoloradourcola@gmail.com>
 * Date: Fri Aug  9 09:27:39 EEST 2019
 * Copyright: (C) 2019 LUIS COLORADO.  All rights reserved.
 * License: BSD.
 */

#include "gdc.h"

unsigned gdc(unsigned a, unsigned b)
{
    while (b != 0) {
        unsigned c = a % b;
        a = b; b = c;
    }
    return a;
} /* gdc */
