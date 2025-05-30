/* slowtty.h -- program to slow down the output of characters
 * to be able to follow output as if a slow terminal were
 * attached to the computer.
 * Author: Luis Colorado <luiscoloradourcola@gmail.com>
 * Copyright: (C) 2015-2025 LUIS COLORADO.  All rights reserved.
 * License: BSD.
 */
#ifndef _SLOWTTY_H
#define _SLOWTTY_H

#include <termios.h>

#include "ring.h"

#ifndef FALSE
#define FALSE   (0)
#define TRUE    (!FALSE)
#endif

#ifndef TIC_DELAY  /* so we can change the value on the Makefile */
#define TIC_DELAY       (40000000) /* nsecs.  = 0.04s. */
#endif
#ifndef TICS_PER_SEC
#define TICS_PER_SEC    (25)
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

#define PIFLG_STOPPED   (1 << 0)

struct pthread_info {
    pthread_t       id;         /* id of pthread */

    /* FILE DESCRIPTORS */
    int             from_fd,    /* descriptor we must read from */
                    to_fd;      /* descriptor we must write to */

    int             flags;      /* flags of the communication
                                 * channel */
    volatile int    do_finish;  /* to tell thread when it's time
                                 * to finish. */

    /* RING BUFFER */
    char           *name;
    struct ring_buffer
                    b;          /* ring buffer */

    /* CHANNEL SAVED CONFIG */
    speed_t         svd_bauds;  /* saved baudrate */
    tcflag_t        svd_cflag;  /* saved cflag */

    /* TO CALCULATE TIMING */
    unsigned long   num;        /* numerator of integer chars to pass */
    unsigned long   den;        /* denominator of integer chars
                                 * to pass */
    unsigned long   acc;        /* fractional part of char to pass. */
    unsigned long   ctw;        /* whole chars to write */

    struct timespec tic;

    /* THE OTHER THREAD INFO (IN OPPOSITE DIRECTION) */
    struct pthread_info *other; /* the info of the other thread */

}; /* struct pthread_info */

extern struct termios saved_tty;

extern struct winsize saved_window_size;

void *pthread_body_writer(void *_pi),
     *pthread_body_reader(void *_pi);

extern int ptym, ptys;

#endif /* _SLOWTTY_H */
