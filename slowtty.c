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

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libutil.h>
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

#include "slowtty.h"
#include "delay.h"

#define D(x) __FILE__":%d: %s: " x, __LINE__, __func__
#define ERRNO ": %s (errno=%d)"
#define EPMTS strerror(errno), errno
/* XXX: fix with a portable parameter passing way */
#define WARN(x, args...) do { \
        fprintf(stderr, D("WARNING: " x), ##args); \
    } while (0)
#define ERR(x, args...) do { \
        fprintf(stderr, D("ERROR: " x ), ##args); \
        exit(EXIT_FAILURE); \
    } while (0)
#define LOG(x, args...) do { \
        if (flags & FLAG_VERBOSE) { \
            fprintf(stderr, \
                D("INFO: " x), \
                ##args); \
        } \
    } while (0)

int ptym, ptys;
/* to recover at the end and pass config to slave at beginning */
struct termios saved_tty;
struct winsize saved_window_size;

volatile int flags = FLAG_DOWINCH;

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

void pass_data(struct pthread_info *pi)
{
    for (;;) {
        int n;
        unsigned char *p;
        int res;
        struct termios t;

        /* the attributes of the line are got from the ptys
         * always, as the user can change them at any time. */
        tcgetattr(ptys, &t); /* to get speeds, etc */
        n = delay(&t); /* do the delay. */
        if (n) {
            /* security net. Should never happen */
            if (n > sizeof pi->buffer)
                n = sizeof pi->buffer;
            /* read the bytes indicated by the delay routine */
            n = read(pi->from_fd, pi->buffer, n);
            switch(n) {
            case -1:
                /* we can receive an interrupt from a SIGWINCH
                 * signal, ignore it and reloop. */
                if (errno != EINTR)
                    ERR("read: " ERRNO "\r\n", EPMTS);
                continue;
            case 0:
                /* should not happen, as terminals are in raw mode.
                 */
                LOG("EOF in %s thread\r\n", pi->name);
                return;
            default:
                /* we did read something. try to write to the
                 * other descriptor. */
                p = pi->buffer;
                while (n > 0) {
                    res = write(pi->to_fd, p, n);
                    if (res >= 0) {
                        n -= res;
                        p += res;
                        continue;
                    }
                    /* res < 0 */
                    if (errno != EINTR)
                        ERR("write: " ERRNO "\r\n", EPMTS);
                }
            } /* switch */
        } /* if */
    } /* for */
} /* pass_data */

void *pthread_body_writer(void *_pi)
{
    struct pthread_info *pi = _pi;

    LOG("id=%p, from_fd=%d, to_fd=%d, name=%s\r\n",
            pi->id, pi->from_fd, pi->to_fd, pi->name);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pass_data(pi);
    return NULL;
} /* pthread_body_writer */

void *pthread_body_reader(void *_pi)
{
    struct pthread_info *pi = _pi;

    LOG("id=%p, from_fd=%d, to_fd=%d, name=%s\r\n",
            pi->id, pi->from_fd, pi->to_fd, pi->name);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    /* pass the tty config to the slave process at the beginning. */
    tcsetattr(pi->to_fd, TCSAFLUSH, &saved_tty);
    pass_data(pi);
    return NULL;
} /* pthread_body_reader */

void pass_winsz(int sig)
{
    struct winsize ws;
    int res1, res2;

    if ((flags & FLAG_DOWINCH)
            && (ioctl(0, TIOCGWINSZ, &ws)
                || ioctl(ptym, TIOCSWINSZ, &ws)))
    {
        WARN("cannot pass WINSZ from stdin to master pty: "
                ERRNO ", disable it.\r\n", EPMTS);
        flags &= ~FLAG_DOWINCH; /* disable */
    }
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
        ERR("stdin is not a tty, aborting\n");
    }

    /* we obtain the tty settings from stdin . */
    if (tcgetattr(0, &saved_tty) < 0) {
        ERR("tcgetattr" ERRNO "\n", EPMTS);
    }

    /* get the window size */
    if (flags & FLAG_DOWINCH) {
        int res = ioctl(0, TIOCGWINSZ, &saved_window_size);
        if (res < 0) {
            WARN("winsize:" ERRNO ", disabling\n", EPMTS);
            flags &= ~FLAG_DOWINCH;
        }
    }

    res = openpty(&ptym, &ptys, pty_name, &saved_tty, &saved_window_size);
    LOG("openpty => %d\n", res);
    if (res < 0) {
        ERR("openpty: " ERRNO "\n", EPMTS);
    }

    child_pid = fork();

    switch(child_pid) {
    case -1:
        ERR("fork" ERRNO "\n", EPMTS);
        break; /* unneeded, but don't hurt */

    case 0: { /* child process */
            int res;

            res = login_tty(ptys);
            if (res < 0) {
                ERR("logintty:%s: " ERRNO "\n", pty_name, EPMTS);
            }

            close(ptym); close(ptys);

            if (argc) {
                execvp(argv[0], argv);
                ERR("execvp: %s" ERRNO "\r\n", argv[0], EPMTS);
            } else {
                char *shellenv = "SHELL";
                char *shell = getenv(shellenv);

                if (!shell) {
                    struct passwd *u = getpwnam(getlogin());
                    if (!u)
                        ERR("getpwnam failed\r\n");
                    shell = u->pw_shell;
                } /* if */
            
                LOG("execlp: %s\n", shell);
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

            LOG("fork: child_pid == %d, ptym=%d, ptys=%d, "
                    "pty_name=[%s]\n",
                    child_pid, ptym, ptys, pty_name);

            cfmakeraw(&stty_raw);
            res = tcsetattr(0, TCSAFLUSH, &stty_raw);
            if (res < 0) {
                ERR("tcsetattr(ptym, ...): " ERRNO "\n", EPMTS);
            } /* if */

            if (flags & FLAG_DOWINCH) {
                LOG("installing signal handler\n");
                memset(&sa, 0, sizeof sa);
                sa.sa_handler = pass_winsz;
                sigaction(SIGWINCH, &sa, NULL);
            }

            /* create the subthreads to process info */
            res = pthread_create(
                    &p_in.id,
                    NULL,
                    pthread_body_reader,
                    init_pthread_info(
                        0, ptym,
                        "READ",
                        &p_in));
            LOG("pthread_create: id=%p, res=%d\r\n", p_in.id, res);
            if (res < 0)
                ERR("pthread_create: " ERRNO "\r\n", EPMTS);

            res = pthread_create(
                    &p_out.id,
                    NULL,
                    pthread_body_writer,
                    init_pthread_info(
                        ptym, 1, 
                        "WRITE",
                        &p_out));
            LOG("pthread_create: id=%p, res=%d\r\n", p_in.id, res);
            if (res < 0)
                ERR("pthread_create: " ERRNO "\r\n", EPMTS);

            /* wait for subprocess to terminate */
            wait(&exit_code);
            LOG("wait(&exit_code <= %d);\r\n", exit_code);

            /* join writing thread */
            LOG("pthread_join(%p, NULL);...\r\n", p_out.id);
            if ((res = pthread_join(p_out.id, NULL)) < 0)
                ERR("pthread_join" ERRNO "\r\n", EPMTS);
            LOG("pthread_join(%p, NULL); => %d\r\n", p_out.id, res);

            /* cancel reading thread */
            res = pthread_cancel(p_in.id);
            LOG("pthread_cancel(%p); => %d\r\n", p_in.id, res);

            /* join it */
            LOG("pthread_join(%p, NULL);...\r\n", p_in.id);
            res = pthread_join(p_in.id, NULL);
            LOG("pthread_join(%p, NULL); => %d\r\n", p_in.id, res);

            /* restore the settings from the saved ones. We
             * follow the same procedure (first with stdin, then
             * with stdout) so we configure the same settings
             * to the same channel as in the beginning. */
            res = tcsetattr(0, TCSAFLUSH, &saved_tty);
            LOG("tcsetattr(0, TCSAFLUSH, &saved_tty) => %d\n", res);
            if (res < 0) {
                LOG("tcsetattr(0, ...): ERROR" ERRNO "\n", EPMTS);
            } /* if */

            /* exit with the subprocess exit code */
            LOG("exit(%d);\n", WEXITSTATUS(exit_code));
            exit(WEXITSTATUS(exit_code));
        } /* case */
    } /* switch */
} /* main */
