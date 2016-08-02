/* slowtty.c -- program to slow down the output of characters to be able
 * to follow output as if a slow terminal were attached to the computer.
 * Author: Luis Colorado <luiscoloradourcola@gmail.com>
 * Copyright: (C) 2015 LUIS COLORADO.
 * This is open source copyrighted software.  You can redistribute it in
 * source or binary form under the Berkeley Software Distribution license.
 * The author does not assume any liability or responsibility from the use
 * or misuse of this software.
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
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
#include <unistd.h>

#if HAS_PTY_H
#include <pty.h>
#endif

#if HAS_LIBUTIL_H
#include <libutil.h>
#endif

#include "slowtty.h"
#include "delay.h"

#ifndef NDEBUG
#define NDEBUG 0
#endif

int ptym, ptys;

#define D(x) __FILE__":%d: %s: " x, __LINE__, __func__
#define ERRNO ": %s (errno=%d)"
#define EPMTS strerror(errno), errno
#define WARN(x, args...) do { \
        fprintf(stderr, D("WARNING: " x), args); \
    } while (0)
#define ERR(x, args...) do { \
        fprintf(stderr, D("ERROR: " x ), args); \
        exit(EXIT_FAILURE); \
    } while (0)

#if NDEBUG
#define LOG(x, args...)
#else
#define LOG(x, args...) do { \
        if (flags & FLAG_VERBOSE) { \
            fprintf(stderr, \
                D("INFO: " x), \
                args); \
        } \
    } while (0)
#endif

struct termios saved_tty;
struct winsize window_size;

int flags = FLAG_DOWINCH;

struct pthread_info {
    pthread_t       id;
    int             from_fd,
                    to_fd;
    struct termios  saved_cfg;
    char           *name;
    unsigned char   buffer[BUFSIZ];
    size_t          buffsz;
}; /* struct pthread_info */

struct pthread_info *init_pthread_info(
        int                     from_fd,
        int                     to_fd,
        char                   *name,
        struct pthread_info    *pi)
{
    pi->from_fd     = from_fd;
    pi->to_fd       = to_fd;
    pi->name        = name;
    pi->buffsz      = 0;
    return pi;
} /* init_pthread_info */

void *pthread_body_writer(void *_pi)
{
    struct pthread_info *pi = _pi;
    int flags;
    struct termios t;


    LOG("from_fd=%d, to_fd=%d, name=%s, flags=%#08x\r\n",
            pi->from_fd, pi->to_fd, pi->name, flags);

    for (;;) {
        int n;

        tcgetattr(pi->from_fd, &t);
        n = delay(&t);
        if (n) {
            n = read(pi->from_fd, pi->buffer, n);
            if (n <= 0) {
                LOG("read" ERRNO "\r\n", EPMTS);
                break;
            } /* if */
            n = write(pi->to_fd, pi->buffer, n);
            if (n < 0) {
                LOG("write" ERRNO "\r\n", EPMTS);
                break;
            } /* if */
        } /* if */
    } /* while */

    return NULL;
} /* pthread_body_writer */

void *pthread_body_reader(void *_pi)
{
    struct pthread_info *pi = _pi;
    ssize_t n;
    int res;
    struct winsize ws, ws_old;

    LOG("from_fd=%d, to_fd=%d, name=%s, flags=%#08x\r\n",
            pi->from_fd, pi->to_fd, pi->name, flags);


    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    tcsetattr(pi->to_fd, TCSAFLUSH, &saved_tty);

    while ((n = read(pi->from_fd,
                     pi->buffer,
                     sizeof pi->buffer)) > 0)
    {
        write(pi->to_fd, pi->buffer, n);
    }

    if (n < 0) LOG("read" ERRNO "\r\n", EPMTS);

    return NULL;
} /* pthread_body_reader */

void pass_winsz(int sig)
{
    struct winsize ws;

    /* ioctl doesn't interact with any other part of
     * the system, so we do both ioctl(2) calls
     * asynchronously (as the ioctl is atomic). */

    (ioctl(0, TIOCGWINSZ, &ws) >= 0) &&
        (ioctl(1, TIOCGWINSZ, &ws) >= 0) &&
        (ioctl(ptym, TIOCSWINSZ, &ws));
} /* pass_winsz */

int main(int argc, char **argv)
{
    int shelpid;
    int opt, res;
    pid_t child_pid;
    char pty_name[2000];

    while ((opt = getopt(argc, argv, "tvw")) != EOF) {
        switch (opt) {
        case 't': flags |=  FLAG_NOTCSET; break;
        case 'v': flags |=  FLAG_VERBOSE; break;
        case 'w': flags &= ~FLAG_DOWINCH; break;
        } /* switch */
    } /* while */

    argc -= optind; argv += optind;

    if (!isatty(0)) {
        ERR("stdin is not a tty, aborting\n", NULL);
    }

    /* we obtain the tty settings from stdin . */
    if (tcgetattr(0, &saved_tty) < 0) {
        ERR("tcgetattr" ERRNO "\n", EPMTS);
    }

    /* get the window size */
    if (flags & FLAG_DOWINCH) {
        int res = ioctl(0, TIOCGWINSZ, &window_size);
        if (res < 0) {
            WARN("winsize:" ERRNO ", disabling\n", EPMTS);
            flags &= ~FLAG_DOWINCH;
        }
    }

    child_pid = forkpty(&ptym, pty_name, &saved_tty, &window_size);

    switch(child_pid) {
    case -1: ERR("fork" ERRNO, EPMTS);
    case 0: { /* child process */
            int res;

            if (argc) {
                execvp(argv[0], argv);
                ERR("execvp: %s" ERRNO "\r\n", argv[0], EPMTS);
            } else {
                char *shellenv = "SHELL";
                char *shell = getenv(shellenv);

                if (!shell) {
                    struct passwd *u = getpwnam(getlogin());
                    if (!u)
                        ERR("getpwnam failed\r\n", NULL);
                    shell = u->pw_shell;
                } /* if */
            
                execlp(shell, shell, NULL);
                ERR("execlp: %s" ERRNO "\r\n", shell, EPMTS);
            } /* if */
            /* NOTREACHED */
        } /* case */
    default: { /* parent process */
            struct pthread_info p_in, p_out;
            int res, exit_code = 0;
            struct sigaction sa;
            struct termios stty_raw = saved_tty;

	    LOG("forkpty: child_pid == %d\n", child_pid);

            cfmakeraw(&stty_raw);
            res = tcsetattr(0, TCSAFLUSH, &stty_raw);
            if (res < 0) {
                ERR("tcsetattr(ptym, ...): ERROR" ERRNO "\n", EPMTS);
            } /* if */

            memset(&sa, 0, sizeof sa);
            sa.sa_handler = pass_winsz;

            /* install signal handler */
            sigaction(SIGWINCH, &sa, NULL);

            /* create the subthreads to process info */
            res = pthread_create(
                    &p_in.id,
                    NULL,
                    pthread_body_reader,
                    init_pthread_info(
                        0, ptym,
                        "READ",
                        &p_in));
            if (res < 0)
                ERR("pthread_create" ERRNO "\r\n", EPMTS);

            res = pthread_create(
                    &p_out.id,
                    NULL,
                    pthread_body_writer,
                    init_pthread_info(
                        ptym, 1, 
                        "WRITE",
                        &p_out));
            LOG("pthread_create: id=%p, res=%d\r\n", p_in.id, res);
            if (res < 0) ERR("pthread_create" ERRNO "\r\n", EPMTS);

            /* wait for subprocess to terminate */
            res = wait(&exit_code);
            LOG("wait(&exit_code => %d) => %d\r\n", exit_code, res);

            /* join writing thread */
            if ((res = pthread_join(p_out.id, NULL)) < 0)
                ERR("pthread_join" ERRNO "\r\n", EPMTS);
            LOG("pthread_join(%p, NULL) => %d\r\n", p_out.id, res);

            /* cancel reading thread */
            res = pthread_cancel(p_in.id);
            LOG("pthread_cancel(%p) => %d\r\n", p_in.id, res);

            /* join it */
            res = pthread_join(p_in.id, NULL);
            LOG("pthread_join(%p, NULL) => %d\r\n", p_in.id, res);

            /* restore the settings from the saved ones. We
             * follow the same procedure (first with stdin, then
             * with stdout) so we configure the same settings
             * to the same channel as in the beginning. */
            res = tcsetattr(0, TCSAFLUSH, &saved_tty);
	    LOG("tcsetattr(0, TCSAFLUSH, &saved_tty) => %d\n", res);
            if (res < 0) {
                LOG("tcsetattr(0, ...): ERROR" ERRNO "\r\n", EPMTS);
            } /* if */

            /* exit with the subprocess exit code */
            LOG("exit(%d)\n", WEXITSTATUS(exit_code));
            exit(WEXITSTATUS(exit_code));
        } /* case */
    } /* switch */
} /* main */
