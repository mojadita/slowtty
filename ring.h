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

#define RB_BUFFER_SIZE      (4096)

struct ring_buffer {
    char           *rb_head,
                   *rb_tail;
    size_t          rb_capa,
                    rb_size,
                    rb_iov_n;
    struct iovec    rb_iov[2];
    char            rb_buffer[RB_BUFFER_SIZE];
};

void rb_init(struct ring_buffer *rb);
ssize_t rb_read(struct ring_buffer *rb, size_t n, int fd);
ssize_t rb_write(struct ring_buffer *rb, size_t n, int fd);

#endif /* _RB_H */
