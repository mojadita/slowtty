/* ring.h -- ring buffer definitions.
 * Author: Luis Colorado <luiscoloradourcola@gmail.com>
 * Date: Wed Aug 14 19:22:38 EEST 2019
 * Copyright: (C) 2019 LUIS COLORADO.  All rights reserved.
 * License: BSD.
 */
#ifndef _RB_H
#define _RB_H

#include <unistd.h>
#include <sys/uio.h>

#define RB_BUFFER_SIZE      (1024)

struct ring_buffer {
    char           *rb_head,
                   *rb_tail,
                   *rb_end;
    size_t          rb_size;

    char            rb_buffer[RB_BUFFER_SIZE];
};

/* Initialize a ring buffer.
 *
 * @param rb the ring buffer to be initialized. */
void
rb_init(
        struct ring_buffer *rb);

/* Read bytes to a ring buffer.
 *
 * @param rb the ring buffer to be updated.
 * @param fd the file descriptor to be read from.
 * @param n the number of bytes to read.  It should
 *          be less than the buffer capacity, RB_BUFFER_SIZE
 *          minus the buffer size rb->rb_size, but a check is
 *          done inside the function and if you pass more bytes,
 *          the maximum available are read instead.
 * @return  The number of bytes actually read.
 */
ssize_t
rb_read(
        struct ring_buffer *rb,
        int fd,
        size_t n);

/* Write bytes to a ring buffer.
 *
 * @param rb the ring buffer to be updated.
 * @param fd the file descriptor to be written to.
 * @param n the number of bytes to write.  It should
 *          be less than the buffer size, rb->rb_size, but a check
 *          is done inside the function and if you pass more bytes,
 *          the maximum available are read instead.
 * @return  The number of bytes actually read.
 */
ssize_t
rb_write(
        struct ring_buffer *rb,
        int fd,
        size_t n);

#endif /* _RB_H */
