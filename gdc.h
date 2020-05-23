/* gdc.h -- greatest common divisor.  Definitions.
 * Author: Luis Colorado <luiscoloradourcola@gmail.com>
 * Date: Fri Aug  9 09:29:28 EEST 2019
 * Copyright: (C) 2019 LUIS COLORADO.  All rights reserved.
 * License: BSD.
 */

#ifndef _GDC_H
#define _GDC_H

/* Calculates the maximum common divisor for
 * two unsigned values.
 *
 * @param a one of the values.
 * @param b the other value.
 *
 * @return the greatest common divisor of passed parameters.
 */
unsigned
gdc(
        unsigned a,
        unsigned b);

#endif /* _GDC_H */
