/* flow.c -- pthread control flow (downstream & upstream)
 * Author: Luis Colorado <luiscoloradourcola@gmail.com>
 * Date: Mon Nov  9 07:35:04 EET 2026
 * Copyright: (c) 2026 Luis Colorado.  All rights reserved.
 * License: BSD v2
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#include "slowtty.h"

#include "flow.h"

void downstream_flow_control(struct pthread_info *pi)
{
    if (!(pi->flags & PIFLG_STOPPED)
            && (pi->b.rb_size >= HIGH_WATER_MARK))
    {
        /* we must stop the slave writer pty */
        int res = ioctl(ptys, TIOCSTOP);
        if (res < 0) {
            WARN("Cannot stop slave pty "
                 "DOWNSTREAM flow.  " ERRNO "\n",
                 errno, strerror(errno));
        }
        pi->flags |= PIFLG_STOPPED;
    } else if (pi->flags & PIFLG_STOPPED
            && pi->b.rb_size < LOW_WATER_MARK)
    {
        /* we must start the slave writer pty */
        int res = ioctl(ptys, TIOCSTART);
        if (res < 0) {
            WARN("Cannot start slave pty "
                 "UPSTREAM flow.  " ERRNO "\n",
                 errno, strerror(errno));
        }
    }
} /* downstream_flow_control */

void upstream_flow_control(struct pthread_info *pi)
{
    if (!(pi->flags & PIFLG_STOPPED)
            && (pi->b.rb_size >= HIGH_WATER_MARK))
    {
        /* we'll not read from pi->from_fd while flags
         * stay on stop state */
        pi->flags |= PIFLG_STOPPED;
    } else if (pi->flags & PIFLG_STOPPED
            && pi->b.rb_size < LOW_WATER_MARK)
    {
        /* just zero the flag, this will make the
         * reader to read again */
        pi->flags &= ~PIFLG_STOPPED;
    }
} /* upstream_flow_control */
