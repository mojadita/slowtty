/* ring.c -- ring buffer implementation.
 * Author: Luis Colorado <luiscoloradourcola@gmail.com>
 * Date: Wed Aug 14 19:36:25 EEST 2019
 * Copyright: (C) 2019 LUIS COLORADO.  All rights reserved.
 * License: BSD.
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "ring.h"

#define F(_fmt) "%s:%d:%s: " _fmt, __FILE__, __LINE__, __func__

static ssize_t
rb_io(
    struct ring_buffer *rb, /* ring buffer to operate on. */
    int fd,                 /* file descriptor involved in
                             * system call */
    char *fname,            /* function name (for error
                             * messages) */
    size_t nio,             /* number of bytes to io */
    char **ph,              /* first pointer to operate on (by
                             * reference, we have to update it) */
    char *pt,               /* second pointer acting as limit
                             * (passed by value) */
    ssize_t (*io_op)(       /* function to call to do actual io */
        int fd,             /* file descriptor */
        const struct iovec *iov, /* struct iovec array */
        int niov))      /* number of array elements. */
{
    int iov_n = 0;
    rb->rb_iov[0].iov_base = *ph;
    if (*ph < pt) {
        if (nio > 0)
            rb->rb_iov[iov_n++].iov_len = nio;
    } else if (pt < *ph || rb->rb_size == 0) {
        size_t n = rb->rb_buffer + RB_BUFFER_SIZE - *ph;
        if (nio > n) {
            rb->rb_iov[iov_n++].iov_len = n;
            nio -= n;
            rb->rb_iov[iov_n].iov_base = rb->rb_buffer;
        }
        rb->rb_iov[iov_n++].iov_len = nio;
    }
    if (iov_n > 0) {
        nio = io_op(fd, rb->rb_iov, iov_n);
        if (nio < 0) {
            fprintf(stderr,
                F("%s: ERROR %d: %s\n"),
                fname, errno, strerror(errno));
            return -1;
        }
        *ph += nio;
        if (*ph >= rb->rb_buffer + RB_BUFFER_SIZE)
            *ph -= RB_BUFFER_SIZE;
    }
    return nio;
} /* rb_io */

ssize_t rb_read(struct ring_buffer *rb, size_t n, int fd)
{
    ssize_t res = rb_io(rb, fd, "readv", n, &rb->rb_tail, rb->rb_head, readv);
    if (res > 0) rb->rb_size += res;
    return res;
} /* rb_read */

ssize_t rb_write(struct ring_buffer *rb, size_t n, int fd)
{
    ssize_t res = rb_io(rb, fd, "writev", n, &rb->rb_head, rb->rb_tail, writev);
    if (res > 0) rb->rb_size -= res;
    return res;
} /* rb_write */

void rb_init(struct ring_buffer *rb)
{
    rb->rb_head
        = rb->rb_tail
        = rb->rb_buffer;
    rb->rb_capa = RB_BUFFER_SIZE;
    rb->rb_size = 0;
} /* rb_init */
