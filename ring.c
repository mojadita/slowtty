/* ring.c -- ring buffer implementation.
 * Author: Luis Colorado <luiscoloradourcola@gmail.com>
 * Date: Wed Aug 14 19:36:25 EEST 2019
 * Copyright: (C) 2019 LUIS COLORADO.  All rights reserved.
 * License: BSD.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include "ring.h"
#include "slowtty.h"

#ifndef F
#define F(_fmt) "%s:%d:%s: "_fmt,__FILE__,__LINE__,__func__
#endif

static ssize_t
rb_io(
        struct ring_buffer
                    *rb,    /* ring buffer to operate on. */
        int          fd,    /* file descriptor involved in
                             * system call */
        size_t       nio,   /* number of bytes to io */
        char       **rph,   /* reference to pointer to
                             * operate on (passed by reference, we have
                             * to update it) */
        ssize_t    (*io_op)(/* function to call to do actual io */
                int          fd,    /* file descriptor */
                const struct iovec
                            *iov,   /* struct iovec array */
                int          niov), /* number of array elements. */
        char        *fname) /* function name to call (for error
                             * messages) */
{
    struct iovec iov[2],
                *piov  = iov;
    char *const  start = rb->rb_buffer,
         *const  end   = rb->rb_end;
    char        *ph    = *rph;

    piov->iov_base  = ph;
                ph += nio;
    if (ph >= end) {
        /* bytes left to process */
        size_t n = ph - end;

        (piov++)->iov_len = nio - n;
        nio = n;
        if (n > 0) {
            piov->iov_base = start;
            (piov++)->iov_len = nio;
        }
    } else {
        (piov++)->iov_len = nio;
    }
    ssize_t res = io_op(fd, iov, piov - iov);

#if 0
    /* THIS LOG IS TOO HEAVY TO USE IN PRODUCTION */
    fprintf(stderr, F("%s(fd=%d, {"), fname, fd);
    char *sep = "";
    for (struct iovec *p = iov; p < piov; p++) {
        fprintf(stderr,
            "%s{.iov_base=%p, .iov_len=%zu}",
            sep, p->iov_base, p->iov_len);
        sep = ",";
    }
    fprintf(stderr, "%zu) => %zd\n",
          (piov - iov), res);
#endif

    if (res < 0) {
#if 0
        /* AND THIS ONE ALSO */
        fprintf(stderr,
            F("%s: ERROR" ERRNO "\n"),
            fname, EPMTS);
#endif
        return res;
    }
    *rph += res;
    if (*rph >= end)
        *rph -= RB_BUFFER_SIZE;

    return res;
} /* rb_io */

ssize_t
rb_read(
        struct ring_buffer *rb,
        int fd,
        size_t n)
{
    if (n > RB_BUFFER_SIZE - rb->rb_size)
        n = RB_BUFFER_SIZE - rb->rb_size;

    ssize_t res = rb_io(rb, fd, n,
            &rb->rb_tail, readv, "readv");

    if (res > 0)
        rb->rb_size += res;

    return res;
} /* rb_read */

ssize_t
rb_write(
        struct ring_buffer *rb,
        int fd,
        size_t n)
{
    if (n > rb->rb_size)
        n = rb->rb_size;

    ssize_t res = rb_io(
            rb, fd, n,
            &rb->rb_head,
            writev, "writev");

    if (res > 0)
        rb->rb_size -= res;

    return res;
} /* rb_write */

void
rb_init(
        struct ring_buffer *rb)
{
    rb->rb_head = rb->rb_end
                = rb->rb_tail
                = rb->rb_buffer;
    rb->rb_end += RB_BUFFER_SIZE;
    rb->rb_size = 0;
} /* rb_init */
