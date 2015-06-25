/* slowtty.c -- program to slow down the output of characters to be able to follow
 * output as if a slow terminal were attached to the computer.
 * Author: Luis Colorado <luiscoloradourcola@gmail.com>
 * Copyright: (C) 2015 LUIS COLORADO.  This is open source copyrighted software.
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <pty.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <utmp.h>
#include <wait.h>

#ifndef DEBUG
#define DEBUG 0
#endif

int ptym, ptys;

#define D(x) __FILE__":%d: %s: " x, __LINE__, __func__
#define ERRNO ": %s (errno=%d)"
#define EPMTS strerror(errno), errno
#define ERR(x, args...) do { \
        fprintf(stderr, D("ERROR: " x ), args); \
        exit(EXIT_FAILURE); \
    } while (0)

#if DEBUG
#define LOG(x, args...) do { fprintf(stderr, D("INFO: " x), args); } while (0)
#else
#define LOG(x, args...)
#endif

struct termios saved_tty;

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


    LOG("pthread %d: from_fd=%d, to_fd=%d, name=%s, flags=%#08x\r\n",
            pi->id, pi->from_fd, pi->to_fd, pi->name, flags);

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

    LOG("pthread %d: from_fd=%d, to_fd=%d, name=%s\r\n",
            pi->id, pi->from_fd, pi->to_fd, pi->name);

    tcsetattr(pi->to_fd, TCSAFLUSH, &saved_tty);

    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    while ((n = read(pi->from_fd,
                     pi->buffer,
                     sizeof pi->buffer)) > 0)
        write(pi->to_fd, pi->buffer, n);

    if (n < 0) LOG("read" ERRNO "\r\n", EPMTS);

    return NULL;
} /* pthread_body_reader */

int pass_winsz(int src_fd, int dst_fd)
{
    struct winsize ws;

    if (ioctl(src_fd, TIOCGWINSZ, &ws) < 0) return -1;
    if (ioctl(dst_fd, TIOCSWINSZ, &ws) < 0) return -1;

    return 0;
} /* pass_winsz */

int main(int argc, char **argv)
{
    int shelpid;
    int opt, res;

    while ((opt = getopt(argc, argv, "")) != EOF) {
        switch (opt) {
        } /* switch */
    } /* while */

    argc -= optind; argv += optind;

    /* we obtain the tty settings from stdout.  In case
     * it's not available, we try stdout.  In case neither
     * input and output is not redirected from a tty, so
     * we give up. */
    res = tcgetattr(0, &saved_tty);
    if (res < 0) {
        res = tcgetattr(1, &saved_tty);
        if (res < 0) ERR("tcgetattr" ERRNO "\n", EPMTS);
    } /* if */

    ptym = open("/dev/ptmx", O_RDWR);
    LOG("open: /dev/ptmx: ptym=%d\n", ptym);
    if (ptym < 0) ERR("/dev/ptmx" ERRNO, EPMTS);
    
    if (grantpt(ptym) < 0) ERR("grantpt" ERRNO, EPMTS);
    if (unlockpt(ptym) < 0) ERR("unlockpt" ERRNO, EPMTS);

    switch ((shelpid = fork())) {
    case -1: ERR("fork" ERRNO, EPMTS);
    case 0: { /* child process */
            int res;
            char *pn = ptsname(ptym);
            struct termios stty_raw = saved_tty;

            /* redirections */
            if (!argc) { /* we are launching a shell, become a new process group */
                res = setsid();
                LOG("setsid() => %d\n", res);
            }

            if ((ptys = open(pn, O_RDWR)) < 0)
                ERR("open: %s" ERRNO, pn, EPMTS);
            LOG("open: %s: ptys=%d\n", pn, ptys);

            cfmakeraw(&stty_raw);
            res = tcsetattr(0, TCSAFLUSH, &stty_raw);
            if (res < 0) {
                LOG("tcsetattr(0, ...): ERROR" ERRNO "\n", EPMTS);
                res = tcsetattr(1, TCSAFLUSH, &stty_raw);
                if (res < 0)
                    ERR("tcsetattr(1, ...): ERROR" ERRNO "\r\n", EPMTS);
            } /* if */

            if (pass_winsz(0, ptys) < 0) {
                pass_winsz(1, ptys);
            } /* if */

            /* redirect */
            close(0); close(1); close(2);
            dup(ptys); dup(ptys); dup(ptys);
            close(ptym);

            if (argc) {
                execvp(argv[0], argv);
                ERR("execvp: %s" ERRNO "\r\n", argv[0], EPMTS);
            } else {
                char *shellenv = "SHELL";
                char *shell = getenv(shellenv);

                if (!shell) {
                    struct passwd *u = getpwnam(getlogin());
                    if (!u)
                        ERR("getpwnam failed\r\n", 0);
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

            res = pthread_create(
                    &p_in.id,
                    NULL,
                    pthread_body_reader,
                    init_pthread_info(
                        0, ptym,
                        "READ",
                        &p_in));
            if (res < 0) ERR("pthread_create" ERRNO "\r\n", EPMTS);
            LOG("pthread_create: id=%d\r\n", p_in.id);

            res = pthread_create(
                    &p_out.id,
                    NULL,
                    pthread_body_writer,
                    init_pthread_info(
                        ptym, 1, 
                        "WRITE",
                        &p_out));
            if (res < 0) ERR("pthread_create" ERRNO "\r\n", EPMTS);
            LOG("pthread_create: id=%d\r\n", p_out.id);

            res = wait(&exit_code);
            LOG("wait(&exit_code == %d) => %d\r\n", exit_code, res);

            if (res = pthread_join(p_out.id, NULL) < 0)
                ERR("pthread_join" ERRNO "\r\n", EPMTS);
            LOG("pthread_join(%d, NULL) => %d\r\n", p_out.id, res);

            res = pthread_cancel(p_in.id);
            LOG("pthread_cancel(%d) => %d\r\n", p_in.id, res);

            res = pthread_join(p_in.id, NULL);
            LOG("pthread_join(%d, NULL) => %d\r\n", p_in.id, res);

            /* restore the settings from the saved ones. We
             * follow the same procedure (first with stdin, then
             * with stdout) so we configure the same settings
             * to the same channel as in the beginning. */
            res = tcsetattr(0, TCSAFLUSH, &saved_tty);
            if (res < 0) {
                LOG("tcsetattr(0, ...): ERROR" ERRNO "\r\n", EPMTS);
                res = tcsetattr(1, TCSAFLUSH, &saved_tty);
                if (res < 0)
                    ERR("tcsetattr(1, ...): ERROR" ERRNO "\r\n", EPMTS);
            } /* if */

            exit(WEXITSTATUS(exit_code));
        } /* case */
    } /* switch */
} /* main */
