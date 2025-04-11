/* main.h -- Definitions and constants for main.c module.
 * Author: Luis Colorado <luiscoloradourcola@gmail.com>
 * Date: Fri Apr 11 19:38:25 EEST 2025
 * Copyright: (c) 2025 Luis Colorado.  All rights reserved.
 * License: BSD
 */
#ifndef MAIN_H
#define MAIN_H

#define FLAG_VERBOSE   (1 << 0)
#define FLAG_LOGIN     (1 << 1)
#define FLAG_NOTCSET   (1 << 2)
#define FLAG_DOWINCH   (1 << 3)

extern volatile int flags;
extern size_t bufsz;
extern struct termios saved_tty;

#endif /* MAIN_H */
