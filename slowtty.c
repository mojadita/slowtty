/* slowtty.c -- program to slow down the output of characters to be able
 * to follow output as if a slow terminal were attached to the computer.
 * Author: Luis Colorado <luiscoloradourcola@gmail.com>
 * Date: Wed Aug  3 08:12:26 EEST 2016
 * Copyright: (C) 2015 LUIS COLORADO.
 * This is open source copyrighted software according to the BSD license.
 * You can copy and distribute freely this piece of software, in source
 * or binary form, agreeing in that you are not going to eliminate the
 * above copyright notice and author from the source code or binary
 * announcements the program could make.
 * The software is distributed 'AS IS' which means that the author doesn't
 * accept any liabilities or responsibilities derived of the use the final
 * user or derived works could make of it.
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libutil.h>
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

int ptym, ptys;

/* to recover at the end and pass config to slave at beginning */
struct termios saved_tty;

struct winsize saved_window_size;

volatile int flags = FLAG_DOWINCH;
size_t bufsz;

static struct pthread_info*
init_pthread_info(
        struct pthread_info    *pi,
        int                     from_fd,
        int                     to_fd,
        char                   *name)
{
    pi->from_fd     = from_fd;
    pi->to_fd       = to_fd;
    pi->name        = name;
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
    LOG("tcsetattr(0, TCSADRAIN, &saved_tty) => %d\n", res);
    if (res < 0) {
        LOG("tcsetattr(0, ...): ERROR" ERRNO "\n", EPMTS);
    } /* if */
}

void pass_data(struct pthread_info *pi)
{
    for (;;) {

        int window = delay(pi); /* do the delay. */

        LOG("%s: window = %d\r\n", pi->name, window);

        if (window == 0) continue; /* nothing to do */

        ssize_t to_fill = window - pi->b.rb_size;

        if (to_fill > 0) { /* if we can read */
			LOG("%s: rb_read(&pi->b, to_fill=%lu, pi->from_fd=%d)\r\n",
				pi->name, to_fill, pi->from_fd);
			ssize_t res = rb_read(&pi->b, to_fill, pi->from_fd);
            if (res < 0) {
                ERR("%s: read" ERRNO "\n", pi->name, EPMTS);
            }
			clock_gettime(CLOCK_REALTIME, &pi->tic);
			if (res == 0) {
				LOG("%s: read: EOF on input\n", pi->name);
				break;
			}
			LOG("%s: rb_read ==> %ld bytes\r\n", pi->name, res);
        }

        size_t to_write = window <= pi->b.rb_size
                        ? window
                        : pi->b.rb_size;
        if (to_write > 0) {
			LOG("%s: rb_write(&pi->b, to_write=%lu, pi->to_fd=%d)\r\n",
				pi->name, to_write, pi->to_fd);
			ssize_t res = rb_write(&pi->b, to_write, pi->to_fd);
            if (res < 0) {
                ERR("%s: write" ERRNO "\n", pi->name, EPMTS);
            }
			LOG("%s: rb_write ==> %ld bytes\r\n", pi->name, res);
        }
    } /* for */
} /* pass_data */

void *pthread_body_writer(void *_pi)
{
    struct pthread_info *pi = _pi;

    LOG("id=%p, from_fd=%d, to_fd=%d, name=%s\r\n",
            pi->id, pi->from_fd, pi->to_fd, pi->name);
    pass_data(pi);
    return NULL;
} /* pthread_body_writer */

void *pthread_body_reader(void *_pi)
{
    struct pthread_info *pi = _pi;

    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    LOG("id=%p, from_fd=%d, to_fd=%d, name=%s\r\n",
            pi->id, pi->from_fd, pi->to_fd, pi->name);
    pass_data(pi);
    return NULL;
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
    switch(child_pid) {
    case -1:
        ERR("forkpty" ERRNO "\n", EPMTS);
		/* NOTREACHED */

    case 0: {
			/* child process */
            int res;

            if (argc) {
                int i;
                LOG("execvp:");
                for (i = 0; i < argc; i++) {
                    ADD("[%s]", argv[i]);
                }
                ADD("\n");
                execvp(argv[0], argv);
                ERR("execvp: %s" ERRNO "\n", argv[0], EPMTS);
            } else {
                char *shellenv = "SHELL";
                char *shell = getenv(shellenv);
				char cmd[PATH_MAX];
                if (shell) {
                    LOG("Got shell from environment variable SHELL\n");
                } else {
                    struct passwd *u = getpwnam(getlogin());
                    if (!u)
                        ERR("getpwnam failed\n");
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
            } /* if */
            /* NOTREACHED */
        } /* case */

    default: { /* parent process */
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
			stty_raw.c_cc[VMIN] = 1;
			stty_raw.c_cc[VTIME] = 0;
            atexit(atexit_handler); /* to restore tty settings */
            res = tcsetattr(0, TCSAFLUSH, &stty_raw);
            if (res < 0) {
                ERR("tcsetattr(0, ...)" ERRNO "\n", EPMTS);
            } /* if */

            if (flags & FLAG_DOWINCH) {
                LOG("installing signal handler\r\n");
                memset(&sa, 0, sizeof sa);
                sa.sa_handler = pass_winsz;
                sigaction(SIGWINCH, &sa, NULL);
            }

            /* create the subthreads to process info */
			clock_gettime(CLOCK_REALTIME, &p_in.tic);
            res = pthread_create(
                    &p_in.id,
                    NULL,
                    pthread_body_reader,
                    init_pthread_info(
                        &p_in,
                        0, ptym,
                        "READER"));
            LOG("pthread_create: id=%p, name=%s ==> res=%d\r\n", 
                    p_in.id, p_in.name, res);
            if (res < 0)
                ERR("pthread_create" ERRNO "\r\n", EPMTS);

			p_out.tic = p_in.tic;
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
                        ptym, 1, 
                        "WRITER"));
            LOG("pthread_create: id=%p, name=%s ==> res=%d\r\n", 
                    p_out.id, p_out.name, res);
            if (res < 0)
                ERR("pthread_create" ERRNO "\r\n", EPMTS);

            /* wait for subprocess to terminate */
            wait(&exit_code);
            LOG("wait(&exit_code == %d);\r\n", exit_code);

			/* we cannot wait for the slave device to close, as
			 * something (e.g. a daemon) can leave it open for
			 * some strange reason. So, reconfigure the output
			 * device with the master configuration and close
			 * the master. That should make the writer thread
			 * to fail, and we ignore that. Just join the
			 * writer thread. */

            LOG("pthread_join(%p, NULL);...\r\n", p_out.id);
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
        } /* case */
    } /* switch */
} /* main */
