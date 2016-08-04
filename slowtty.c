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
#define ADD(x, args...) do { \
        if (flags & FLAG_VERBOSE) { \
            fprintf(stderr, x, ##args); \
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
        int n;
        unsigned char *p;
        int res;
        struct termios t;

        /* the attributes of the line are got from the ptym
         * always, as the user can change them at any time. */
        tcgetattr(ptym, &t); /* to get speeds, etc */
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
                    ERR("read(fd=%d)" ERRNO "\r\n", pi->from_fd, EPMTS);
                continue;
            case 0:
                /* should not happen, as terminals are in raw mode.
                 */
                LOG("EOF(fd=%d) in %s thread\r\n", pi->from_fd, pi->name);
                return;
            default:
                /* we did read something. try to write to the
                 * other descriptor. */
                p = pi->buffer;
                while (n > 0) {
                    res = write(pi->to_fd, p, n);
                    /* res < 0 */
                    if ((res < 0) && (errno != EINTR))
                        ERR("write(fd=%d)" ERRNO "\r\n", pi->to_fd, EPMTS);
                    if (res > 0) {
                        n -= res; p += res;
                    }
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

    /* initialize the pty config with the saved one. */
    tcsetattr(ptym, TCSAFLUSH, &saved_tty);

    /* ... and run */
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
        WARN("cannot pass WINSZ from stdin to master pty"
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

    LOG("isatty(0)\n");
    if (!isatty(0)) {
        ERR("stdin is not a tty, aborting\n");
    }

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
        ERR("fork" ERRNO "\n", EPMTS);
        break; /* unneeded, but don't hurt */

    case 0: { /* child process */
            int res;

            if (argc) {
                int i;
                LOG("execvp: %s", argv[0]);
                for (i = 1; i < argc; i++) {
                    ADD(" %s", argv[i]);
                }
                ADD("\n");
                execvp(argv[0], argv);
                ERR("execvp: %s" ERRNO "\n", argv[0], EPMTS);
            } else {
                char *shellenv = "SHELL";
                char *shell = getenv(shellenv);
                if (shell) {
                    LOG("Got shell from environment variable SHELL\n");
                } else {
                    struct passwd *u = getpwnam(getlogin());
                    if (!u)
                        ERR("getpwnam failed\n");
                    shell = u->pw_shell;
                    LOG("Got shell from /etc/passwd file\n");
                } /* if */
                LOG("execlp: %s\n", shell);
                execlp(shell, shell, NULL);
                ERR("execlp: %s" ERRNO "\n", shell, EPMTS);
            } /* if */
            /* NOTREACHED */
        } /* case */

    default: { /* parent process */
            struct pthread_info p_in, p_out;
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
            atexit(atexit_handler);
            cfmakeraw(&stty_raw);
            res = tcsetattr(0, TCSAFLUSH, &stty_raw);
            if (res < 0) {
                ERR("tcsetattr(ptym, ...)" ERRNO "\n", EPMTS);
            } /* if */

            if (flags & FLAG_DOWINCH) {
                LOG("installing signal handler\r\n");
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
            LOG("pthread_create: id=%p, name=%s, res=%d\r\n", 
                    p_in.id, p_in.name, res);
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
            LOG("pthread_create: id=%p, name=%s, res=%d\r\n", 
                    p_out.id, p_out.name, res);
            if (res < 0)
                ERR("pthread_create" ERRNO "\r\n", EPMTS);

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

            /* exit with the subprocess exit code */
            LOG("exit(%d);\r\n", WEXITSTATUS(exit_code));
            exit(WEXITSTATUS(exit_code));
        } /* case */
    } /* switch */
} /* main */
