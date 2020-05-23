/* slowtty.c -- program to slow down the output of characters to
 * be able to follow output as if a slow terminal were attached to
 * the computer.
 * Author: Luis Colorado <luiscoloradourcola@gmail.com>
 * Date: Wed Aug  3 08:12:26 EEST 2016
 * Copyright: (C) 2015 LUIS COLORADO.
 * License: This is open source copyrighted software according to
 * the BSD license.
 * You can copy and distribute freely this piece of software, in
 * source or binary form, agreeing in that you are not going to
 * eliminate the above copyright notice and author from the source
 * code or binary announcements the program could make.
 *
 * The software is distributed 'AS IS' which means that the author
 * doesn't accept any liabilities or responsibilities derived of the
 * use the final user or derived works could make of it.
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
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

#if HAS_PTY_H
#include <pty.h>
#endif

#if HAS_LIBUTIL_H
#include <libutil.h>
#endif

#include "ring.h"
#include "slowtty.h"
#include "delay.h"

#define MIN(_a, _b) ((_a)<(_b) ? (_a) : (_b))

int ptym, ptys;

/* to recover at the end and pass config to slave at beginning */
struct termios saved_tty;

struct winsize saved_window_size;

volatile int flags = FLAG_DOWINCH;

static struct pthread_info*
init_pthread_info(
        struct pthread_info    *pi,
        struct pthread_info    *other,
        int                     from_fd,
        int                     to_fd,
        char                   *name)
{
    pi->from_fd     = from_fd;
    pi->to_fd       = to_fd;
    pi->name        = name;
    pi->other       = other;
    pi->flags       = 0;
    rb_init(&pi->b);
    return pi;
} /* init_pthread_info */

void atexit_handler(void)
{
    /* restore the settings from the saved ones. We
     * follow the same procedure (first with stdin, then
     * with stdout) so we configure the same settings
     * to the same channel as in the beginning. */
    int res = tcsetattr(0, TCSADRAIN, &saved_tty);
    if (res < 0) {
        LOG("tcsetattr(0, ...): ERROR" ERRNO "\n", EPMTS);
    } /* if */
    LOG("tcsetattr(0, TCSADRAIN, &saved_tty) => %d\n", res);
}

void pass_data(struct pthread_info *pi)
{

    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

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
        ssize_t to_fill = pi->b.rb_capa - pi->b.rb_size;

        if (to_fill > 0) { /* if we can read */

            /* read as much as possible */
            ssize_t res = rb_read(&pi->b, to_fill, pi->from_fd);

            if (res < 0) {
                if (errno != EAGAIN) {
                    ERR("%s: read" ERRNO "\n", pi->name, EPMTS);
                    break;
                }
                res = 0;
            } else if (res == 0) {
                LOG("%s: read: EOF on input\n", pi->name);
                break;
            }

            /* good read */
            LOG("%s: rb_read(&pi->b, to_fill=%lu, "
                "pi->from_fd=%d) => %zd\r\n",
                pi->name, to_fill, pi->from_fd, res);

            clock_gettime(CLOCK_REALTIME, &pi->tic);
        } /* if (to_fill > 0) */

        size_t to_write = MIN(pi->b.rb_size, window);

        if (to_write > 0) {
            ssize_t res = rb_write(&pi->b, to_write, pi->to_fd);
            if (res < 0) {
                ERR("%s: write" ERRNO "\n", pi->name, EPMTS);
                break;
            }
            LOG("%s: rb_write(&pi->b, to_write=%lu, "
                "pi->to_fd=%d) => %zd\r\n",
                pi->name, to_write, pi->to_fd, res);
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

void *pthread_body_writer(void *_pi)
{
    struct pthread_info *pi = _pi;

    LOG("%s: id=%p, from_fd=%d, to_fd=%d, name=%s\r\n",
            pi->name, pi->id, pi->from_fd, pi->to_fd, pi->name);
    pass_data(pi);
    return pi;
} /* pthread_body_writer */

void *pthread_body_reader(void *_pi)
{
    struct pthread_info *pi = _pi;

    LOG("%s: id=%p, from_fd=%d, to_fd=%d, name=%s\r\n",
            pi->name, pi->id, pi->from_fd, pi->to_fd, pi->name);
    pass_data(pi);
    return pi;
} /* pthread_body_reader */

void pass_winsz(int sig)
{
    struct winsize ws;
    if ((flags & FLAG_DOWINCH)
            && (ioctl(0, TIOCGWINSZ, &ws)
                || ioctl(ptym, TIOCSWINSZ, &ws)))
    {
        WARN("cannot pass WINSZ from master pty to slave"
                ERRNO ", disabling it.\r\n", EPMTS);
        flags &= ~FLAG_DOWINCH; /* disable */
    }
} /* pass_winsz */

int main(int argc, char **argv)
{
    int shelpid;
    int opt, res;
    pid_t child_pid;
    char pty_name[PATH_MAX];
    size_t bufsz;

    while ((opt = getopt(argc, argv, "dltws:")) != EOF) {
        switch (opt) {
        case 'd': flags |=  FLAG_VERBOSE; break;
        case 'l': flags |=  FLAG_LOGIN;   break;
        case 't': flags |=  FLAG_NOTCSET; break;
        case 'w': flags &= ~FLAG_DOWINCH; break;
        case 's': bufsz = atoi(optarg);
            if (bufsz <= 0 || bufsz >= BUFSIZ) {
                WARN("buffer size set to default due to invalid value (%s) passed\n",
                    optarg);
                bufsz = BUFSIZ;
            } break;
        } /* switch */
    } /* while */

    argc -= optind; argv += optind;

    /* we obtain the tty settings from stdin . */
    LOG("tcgetattr(0, &saved_tty);\n");
    if (tcgetattr(0, &saved_tty) < 0) {
        ERR("tcgetattr" ERRNO "\n", EPMTS);
    }

    /* get the window size */
    if (flags & FLAG_DOWINCH) {
        LOG("ioctl(0, TIOCGWINSZ, &saved_window_size);\n");
        int res = ioctl(0, TIOCGWINSZ, &saved_window_size);
        if (res < 0) {
            WARN("winsize" ERRNO ", disabling\n", EPMTS);
            flags &= ~FLAG_DOWINCH;
        }
    }

    /* flush all descriptors before forking (so no repeated messages
     * on stdout). */
    fflush(NULL);

    child_pid = forkpty(&ptym, pty_name, &saved_tty, &saved_window_size);
    if (child_pid < 0) {
        ERR("forkpty" ERRNO "\n", EPMTS);
        /* NOTREACHED */
    } else if (child_pid == 0) {

        /* child process */
        int res;

        if (argc) {
            int i;
            LOG("execvp:");
            for (i = 0; i < argc; i++) {
                ADD(" [%s]", argv[i]);
            }
            ADD("\n");

            execvp(argv[0], argv);

            ERR("execvp: %s" ERRNO "\n", argv[0], EPMTS);
            /* NOTREACHED */
        } else {
            char *shellenv = "SHELL";
            char *shell = getenv(shellenv);
            char cmd[PATH_MAX];
            if (shell) {
                LOG("Got shell from environment variable SHELL\n");
            } else {
                struct passwd *u = getpwnam(getlogin());
                if (!u) {
                    ERR("getpwnam failed\n");
                    /* NOTREACHED */
                }
                shell = u->pw_shell;
                LOG("Got shell from /etc/passwd file\n");
            } /* if */
            snprintf(cmd, sizeof cmd, "%s%s",
                flags & FLAG_LOGIN
                    ? "-"
                    : "",
                shell);
            LOG("execlp: %s\n", cmd);
            execlp(shell, cmd, NULL);
            ERR("execlp: %s" ERRNO "\n", shell, EPMTS);
            /* NOTREACHED */
        } /* if */
        /* NOTREACHED */
    } else { /* PARENT */

        struct pthread_info p_in, p_out;
        struct timespec ts_now;
        int res, exit_code = 0;
        struct sigaction sa;
        struct termios stty_raw = saved_tty;

        LOG("forkpty: child_pid == %d, ptym=%d, "
                "pty_name=[%s]\n",
                child_pid, ptym, pty_name);

        /* from this point on, we have to use \r in addition
         * to \n, as we have switched to raw mode (in the parent
         * process)  So we first do a fflush(3) to dump all
         * data. */
        fflush(NULL);
        cfmakeraw(&stty_raw);
        stty_raw.c_iflag &= ~(IXON | IXOFF);
        stty_raw.c_cc[VMIN] = 1;
        stty_raw.c_cc[VTIME] = 0;
        /* RESTORE TTY SETTINGS AT END */
        atexit(atexit_handler);
        res = tcsetattr(0, TCSAFLUSH, &stty_raw);
        if (res < 0) {
            ERR("tcsetattr(0, ...)" ERRNO "\n", EPMTS);
        } /* if */

        /* SET THE O_NONBLOCK on stdin */
        res = fcntl(0, F_GETFL);
        if (res < 0) {
            ERR("fcntl(0, F_GETFL) => %d:" ERRNO "\n",
                res, EPMTS);
        }
        int res2 = fcntl(0, F_SETFL, res | O_NONBLOCK);
        if (res2 < 0) {
            ERR("fcntl(0, F_SETFL, 0x%04x) => %d:" ERRNO "\n",
                res | O_NONBLOCK, res2, EPMTS);
        }
		/* ... AND ON ptym */
		res = fcntl(ptym, F_GETFL);
		if (res < 0) {
			ERR("fcntl(ptym=%d, F_GETFL) => %d:" ERRNO "\n",
				ptym, res, EPMTS);
		}
		res2 = fcntl(ptym, F_SETFL, res | O_NONBLOCK);
		if (res2 < 0) {
			ERR("fcntl(ptym=%d, F_SETFL, 0x%04x) => %d:" ERRNO "\n",
				ptym, res | O_NONBLOCK, res2, EPMTS);
		}

        /* INSTALL SIGNAL HANDLER FOR SIGWINCH */
        if (flags & FLAG_DOWINCH) {
            LOG("installing signal handler\r\n");
            memset(&sa, 0, sizeof sa);
            sa.sa_handler = pass_winsz;
            sigaction(SIGWINCH, &sa, NULL);
        }

        /* CREATE THE SUBTHREADS TO PROCESS INFO */
        clock_gettime(CLOCK_REALTIME, &p_in.tic);
        res = pthread_create(
                &p_in.id,
                NULL,
                pthread_body_reader,
                init_pthread_info(
                    &p_in,
                    &p_out,
                    0, ptym,
                    "READER"));
        if (res < 0) {
            ERR("pthread_create" ERRNO "\r\n", EPMTS);
        }
        LOG("pthread_create: id=%p, name=%s ==> res=%d\r\n",
                p_in.id, p_in.name, res);

        p_out.tic = p_in.tic;
		/* ADVANCE THE TIC, SO IT AWAKES HALF A TICK LATER */
        p_out.tic.tv_nsec += TIC_DELAY / 2;
        if (p_out.tic.tv_nsec >= 1000000000) {
            p_out.tic.tv_sec++;
            p_out.tic.tv_nsec -= 1000000000;
        }
        res = pthread_create(
                &p_out.id,
                NULL,
                pthread_body_writer,
                init_pthread_info(
                    &p_out,
                    &p_in,
                    ptym, 1,
                    "WRITER"));
        if (res < 0) {
            ERR("pthread_create" ERRNO "\r\n", EPMTS);
        }
        LOG("pthread_create: id=%p, name=%s ==> res=%d\r\n",
                p_out.id, p_out.name, res);

        /* wait for subprocess to terminate */
        wait(&exit_code);
        LOG("wait(&exit_code == %d);\r\n", exit_code);

        /* we cannot wait for the slave device to close, as
         * something (e.g. a daemon) can leave it open for
         * some reason.  So, reconfigure the output
         * device with the master configuration and close
         * the master. That should make the writer thread
         * to fail, and we ignore that. Just join the
         * writer thread. */

		/* cancel writing thread */
		res = pthread_cancel(p_out.id);
		LOG("pthread_cancel(%p); => %d\r\n", p_out.id, res);

        if ((res = pthread_join(p_out.id, NULL)) < 0)
            ERR("pthread_join" ERRNO "\n", EPMTS);
        LOG("pthread_join(%p, NULL); => %d\r\n", p_out.id, res);

        /* cancel reading thread */
        res = pthread_cancel(p_in.id);
        LOG("pthread_cancel(%p); => %d\r\n", p_in.id, res);

        /* join it */
        res = pthread_join(p_in.id, NULL);
        LOG("pthread_join(%p, NULL); => %d\r\n", p_in.id, res);

        /* exit with the subprocess exit code */
        LOG("exit(%d);\r\n", WEXITSTATUS(exit_code));
        exit(WEXITSTATUS(exit_code));
    } /* else */
} /* main */
