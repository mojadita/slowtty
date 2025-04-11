/* slowtty.c -- program to slow down the input/output of characters to
 * be able to follow output as if a slow terminal were attached to
 * the computer.
 *
 * Author: Luis Colorado <luiscoloradourcola@gmail.com>
 * Date: Wed Aug  3 08:12:26 EEST 2016
 * Copyright: (C) 2015 LUIS COLORADO.
 * License: BSD
 *
 * The software is distributed 'AS IS' which means that the author
 * doesn't accept any liabilities or responsibilities derived of the
 * use the final user or derived works could make of it.
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "main.h"

#include "ring.h"
#include "slowtty.h"
#include "delay.h"


#define MIN_BUFFER      8

#define MIN(_a, _b) ((_a)<(_b) ? (_a) : (_b))

int ptym, ptys;

/* to recover at the end and pass config to slave at beginning */

/**
 * this routine is called on each thread to pass the data up or
 * down the channel.  The thread goes in a loop until it is
 * cancelled from main() in which a delay of 1/25th s. is
 * scheduled and the number of chars allowed to pass in such
 * interval is calculated.  This is the window of the tick
 * interval.  If the window is zero, we cannot pass any data on
 * this tick and so, nothing is done on this pass.
 *
 * if the window is greater than zero, a number or characters not
 * less than MIN_BUFFER and no more of one of the buffer capacity
 * less the buffer size of two windows is read (this is made to
 * buffer a small amount of characters in order to process
 * interrupt as fast as possible, and send a XOFF back to the
 * origin if more than two windows are buffered for output.
 * In case the buffer size descends below the window size, an
 * XON character is written back to the source in order to
 * restart the flow of characters from the source.
 * A number of characters (the buffer size or the window, which
 * is less) is written to the output side of the channel, so at
 * maximum, window chars are output per tick.
 *
 * @param pi is a reference to the thread global data to use.
 */
void
pass_data(
        struct pthread_info *pi)
{
    LOG("%s: START\n", pi->name);
    for (;;) {

        /* window is the number of characters we can write
         * in this loop pass. */
        int window = delay(pi); /* do the delay. */

        LOG("%s: window = %d\r\n", pi->name, window);

        if (window == 0) {
            /* nothing to do, wait until we have some window */
            continue;
        }

        /* we try to fill the full buffer every time. */

        /* READ TO FILL THE BUFFER UP TO TWO COMPLETE
         * WINDOWS, OR AT LEAST MIN_BUFFER CHARS. */
        ssize_t to_read = 2 * window;
        if (to_read < MIN_BUFFER)
            to_read = MIN_BUFFER;
        if (to_read > RB_BUFFER_SIZE)
            to_read = RB_BUFFER_SIZE;
        to_read -= pi->b.rb_size;
        if (to_read < 0)
            to_read = 0;
        if (to_read > 0) {
            ssize_t res = rb_read(&pi->b,
                    pi->from_fd, RB_BUFFER_SIZE);

            if (res == 0) {
                LOG("%s: rb_read: EOF on input\n", pi->name);
                break;
            } else if (res < 0) {
                if (errno != EAGAIN && errno != EINTR) {
                    ERR("%s: rb_read" ERRNO "\n", pi->name, EPMTS);
                    break;
                }
                res = 0;
            }

            /* good read */
            LOG("%s: rb_read(&pi->b, pi->from_fd=%d, "
                    "to_fill=%u) => %zd\r\n",
                pi->name, pi->from_fd, RB_BUFFER_SIZE, res);
        }

        clock_gettime(CLOCK_REALTIME, &pi->tic);

        if (   pi->do_finish
            && pi->b.rb_size == 0
            && !--pi->do_finish)
        {
            LOG("%s: do_finish && b.rb_size == 0 "
                "=> FINISH\r\n",
                pi->name);
            break;
        }

        size_t to_write = MIN(pi->b.rb_size, window);

        if (to_write > 0) {
            ssize_t res = rb_write(&pi->b, pi->to_fd, to_write);
            if (res < 0) {
                ERR("%s: write" ERRNO "\n", pi->name, EPMTS);
                break;
            }
            LOG("%s: rb_write(&pi->b, pi->to_fd=%d, "
                    "to_write=%lu) => %zd\r\n",
                pi->name, pi->to_fd, to_write, res);
        }

        /* check if we have to start/stop the channel */
        if (pi->flags & PIFLG_STOPPED && pi->b.rb_size < window) {

            /* THIS WRITE WILL GO INTERSPERSED BETWEEN THE CALLS
             * OF THE OTHER THREAD, AS THE INODE IS LOCKED BY THE
             * SYSTEM, NO TWO THREADS CAN EXECUTE IN PARALLEL TWO
             * WRITES TO THE SAME FILE AT THE SAME TIME.   THE
             * WRITE BELOW HAS EXACTLY THE SAME ISSUE*/
            write(pi->other->to_fd, "\021", 1); /* XON, ASCII DC1 */

            LOG("%s: automatic XON on pi->b.rb_size=%zu"
                " < window=%d\n",
                pi->name, pi->b.rb_size, window);
            pi->flags &= ~PIFLG_STOPPED;

        } else if (!(pi->flags & PIFLG_STOPPED)
                && pi->b.rb_size >= 2 * window)
        {
            /* SEE COMMENT ON WRITE ABOVE */
            write(pi->other->to_fd, "\023", 1); /* XOFF, ASCII DC3 */

            LOG("%s: automatic XOFF on pi->b.rb_size=%zu "
                ">= 2 * window=%d\n",
                pi->name, pi->b.rb_size, window);
            pi->flags |= PIFLG_STOPPED;
        }
    } /* for */
    LOG("%s: END\n", pi->name);
} /* pass_data */

void *
pthread_body_writer(
        void *_pi)
{
    struct pthread_info *pi = _pi;

    LOG("%s: id=%p, from_fd=%d, to_fd=%d, name=%s\r\n",
            pi->name, pi->id, pi->from_fd, pi->to_fd, pi->name);
    pass_data(pi);
    return pi;
} /* pthread_body_writer */

void *
pthread_body_reader(
        void *_pi)
{
    struct pthread_info *pi = _pi;

    LOG("%s: id=%p, from_fd=%d, to_fd=%d, name=%s\r\n",
            pi->name, pi->id, pi->from_fd, pi->to_fd, pi->name);
    pass_data(pi);
    return pi;
} /* pthread_body_reader */
