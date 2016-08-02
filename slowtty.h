/* slowtty.h -- program to slow down the output of characters to be able to follow
 * output as if a slow terminal were attached to the computer.
 * Author: Luis Colorado <luiscoloradourcola@gmail.com>
 * Copyright: (C) 2015 LUIS COLORADO.  This is open source copyrighted software.
 */

#ifndef _SLOWTTY_H
#define _SLOWTTY_H

#define FLAG_VERBOSE    (1 << 0)
#define FLAG_DOWINCH    (1 << 1)
#define FLAG_NOTCSET    (1 << 2)
extern int flags;

#endif /* _SLOWTTY_H */
