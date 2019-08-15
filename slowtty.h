/* slowtty.h -- program to slow down the output of characters to be able to follow
 * output as if a slow terminal were attached to the computer.
 * Author: Luis Colorado <luiscoloradourcola@gmail.com>
 * Copyright: (C) 2015 LUIS COLORADO.  This is open source copyrighted software.
 */
#ifndef _SLOWTTY_H
#define _SLOWTTY_H

#include <termios.h>

#include "ring.h"

#ifndef BUFFER_SIZE
#define BUFFER_SIZE     (4096)
#endif

#define F(X) "%s:%d: %s: " X, __FILE__, __LINE__, __func__
#define ERRNO ": ERROR %d: %s"
#define EPMTS errno, strerror(errno)

#define WARN(_fmt, args...) do {                      \
        fprintf(stderr, F("WARNING: " _fmt), ##args); \
    } while (0)

#define ERR(_fmt, args...) do {                       \
        fprintf(stderr, F("ERROR: " _fmt ), ##args);  \
        exit(EXIT_FAILURE);                           \
    } while (0)

#define LOG(_fmt, args...) do {                       \
        if (flags & FLAG_VERBOSE) {                   \
            fprintf(stderr,                           \
                F("INFO: " _fmt),                     \
                ##args);                              \
        }                                             \
    } while (0)

/* adds to a LOG() macro call (to continue) */
#define ADD(_fmt, args...) do {                       \
        if (flags & FLAG_VERBOSE) {                   \
            fprintf(stderr, _fmt, ##args);            \
        }                                             \
    } while (0)

#define FLAG_VERBOSE    (1 << 0)
#define FLAG_DOWINCH    (1 << 1)
#define FLAG_NOTCSET    (1 << 2)

extern volatile int flags;
extern size_t bufsz;
extern int ptym, ptys;
extern struct termios saved_tty;

struct pthread_info {
    pthread_t       id; /* id of pthread */

	/* FILE DESCRIPTORS */
    int             from_fd,/* descriptor we must read from */
                    to_fd;  /* descriptor we must write to */

    char           *name;
	struct ring_buffer
					b;		/* ring buffer */

	struct timespec	time; /* clock tick we are calculating for */

    speed_t svd_bauds; /* saved baudrate */
    tcflag_t svd_cflag; /* saved cflag */

    unsigned long	num; /* numerator of integer chars to pass */
    unsigned long	den; /* denominator of integer chars to pass */
    unsigned long	acc; /* fractional part of char to pass. */
    unsigned long   ctw; /* whole chars to write */

    struct timespec tic;

}; /* struct pthread_info */

#endif /* _SLOWTTY_H */
