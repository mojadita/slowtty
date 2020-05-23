/* test_ring.c -- program to test module ring.c
 * Author: Luis Colorado <luiscoloradourcola@gmail.com>
 * Date: Thu Aug 15 16:44:40 EEST 2019
 * Copyright: (C) 2019 LUIS COLORADO.  All rights reserved.
 * License: BSD.
 */
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ring.h"

volatile int flags = 0;

#define F(_fmt) "%s:%d: " _fmt, __FILE__, __LINE__

int main(int argc, char **argv)
{
    int opt;

    while ((opt = getopt(argc, argv, "s:S")) != EOF) {
        switch(opt) {
        case 's': { int seed = atoi(optarg);
                printf(F("seed = %d\n"), seed);
                srandom(seed); break;
            }
        case 'S': { int seed = time(NULL);
                printf(F("seed = %d\n"), seed);
                srandom(seed); break;
            }
        } /* switch */
    } /* while */

    argc += optind; argv += optind;

    struct ring_buffer b;

    rb_init(&b);

    for (;;) {
        if (b.rb_size < RB_BUFFER_SIZE) {
            size_t to_read = random() % (RB_BUFFER_SIZE - b.rb_size) + 1;
            printf(F("b.rb_size = %zu; to_read = %zu\n"),
                b.rb_size, to_read);
            if (to_read) {
                ssize_t read_in = 0;
                printf(F("rb_read(&b, 0, to_read)\n"));
                read_in = rb_read(&b, 0, to_read);
                printf(F("read_in = %zu\n"), read_in);
                if (read_in < 0) {
                    fprintf(stderr,
                        F("rb_read: ERROR %d: %s\n"),
                            errno, strerror(errno));
                    exit(EXIT_FAILURE);
                } else if (read_in == 0) {
                    printf(F("EOF on stdin.\n"));
                    if (!b.rb_size) break;
                } /* else go on */
            }
        }
        if (b.rb_size) {
            size_t to_write = random() % (b.rb_size) + 1;
            printf(F("to_write = %zu\n"),
                to_write);
            if (to_write) {
                ssize_t written_out = 0;
                printf(F("rb_write(&b, 1, to_write)\n"));
                written_out = rb_write(&b, 1, to_write);
                printf(F("written_out = %zu\n"), written_out);
            }
        }
    }
} /* main */
