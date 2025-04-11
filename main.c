/* main.c -- Main code for the slowtty program.
 * Processing of options and initialization is done here.
 * Author: Luis Colorado <luiscoloradourcola@gmail.com>
 * Date: Fri Apr 11 19:29:56 EEST 2025
 * Copyright: (c) 2025 Luis Colorado.  All rights reserved.
 * License: BSD
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
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
#include <pwd.h>

#include "config.h"
#include "slowtty.h"
#include "main.h"

#ifndef   UQ_HAS_PTY_H /* {{ */
#warning  UQ_HAS_PTY_H should be defined in config.mk
#define   UQ_HAS_PTY_H     (0)
#endif /* UQ_HAS_PTY_H  * }} */

#if UQ_HAS_PTY_H
#include <pty.h>
#endif

#ifndef   UQ_HAS_LIBUTIL_H /* {{ */
#warning  UQ_HAS_LIBUTIL_H should be defined in config.mk
#define   UQ_HAS_LIBUTIL_H     (0)
#endif /* UQ_HAS_LIBUTIL_H  * }} */

#if UQ_HAS_LIBUTIL_H
#include <libutil.h>
#endif

#ifndef   UQ_MAX_PTY_NAME /* {{ */
#warning  UQ_MAX_PTY_NAME should be defined in config.mk
#define   UQ_MAX_PTY_NAME (64)
#endif /* UQ_MAX_PTY_NAME    }} */

#ifndef   UQ_DEFAULT_BUFSIZ /* {{ */
#warning  UQ_DEFAULT_BUFSIZ should be defined in config.mk
#define   UQ_DEFAULT_BUFSIZ (256)
#endif /* UQ_DEFAULT_BUFSIZ    }} */

#ifndef   UQ_DEFAULT_FLAGS /* {{ */
#warning  UQ_DEFAULT_FLAGS should be defined in config.mk
#define   UQ_DEFAULT_FLAGS (FLAG_DOWINCH)
#endif /* UQ_DEFAULT_FLAGS    }} */

#ifndef   UQ_PATH_MAX /* {{ */
#warning  UQ_PATH_MAX should be defined in config.mk
#define   UQ_PATH_MAX (1024)
#endif /* UQ_PATH_MAX    }} */

#define DO_FINISH_ITER  6 /* six reads without data */

volatile int flags = UQ_DEFAULT_FLAGS;

struct winsize saved_window_size;
struct termios saved_tty;

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
    pi->do_finish   = 0;
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
        LOG("tcsetattr(0, ...): ERROR" ERRNO "\n",
            EPMTS);
    } /* if */

    LOG("tcsetattr(0, TCSADRAIN, &saved_tty) => %d\n", res);
} /* atexit_handler */

void
pass_winsz(int sig)
{
    if (!(flags & FLAG_DOWINCH)) {
        LOG("Changed window size, "
            "but deactivated from config\r\n");
        return;
    }
    struct winsize ws;
    int res = ioctl(0, TIOCGWINSZ, &ws);
    if (res < 0) {
        WARN("cannot get the new window "
            "size, deactivating:" ERRNO "\r\n",
            EPMTS);
        flags &= ~FLAG_DOWINCH; /* disable */
        return;
    }
    res = ioctl(ptym, TIOCSWINSZ, &ws);
    if (res < 0) {
        WARN("cannot set the new window "
            "size to (r=%d, c=%d), deactivating"
            ERRNO "\r\n",
            ws.ws_row, ws.ws_col,
            EPMTS);
        flags &= ~FLAG_DOWINCH; /* disable */
        return;
    }
    LOG("Changed window size to (r=%d, c=%d)\r\n",
        ws.ws_row, ws.ws_col);
} /* pass_winsz */

int
main(
        int argc,
        char **argv)
{
    int    opt,
           res;
    pid_t  child_pid;
    char   pty_name[UQ_MAX_PTY_NAME];
    size_t bufsz;

    while ((opt = getopt(argc, argv, "dltws:")) != EOF) {
        switch (opt) {
        case 'd': flags ^=  FLAG_VERBOSE; break;
        case 'l': flags ^=  FLAG_LOGIN;   break;
        case 't': flags ^=  FLAG_NOTCSET; break;
        case 'w': flags ^=  FLAG_DOWINCH; break;
        case 's': bufsz  =  atoi(optarg);
            if (bufsz <= 0 || bufsz >= UQ_DEFAULT_BUFSIZ) {
                WARN("buffer size set to default(%d) due to "
                    "invalid value (%s) passed\n",
                    UQ_DEFAULT_BUFSIZ,
                    optarg);
                bufsz = UQ_DEFAULT_BUFSIZ;
            } break;
        } /* switch */
    } /* while */

    argc -= optind;
    argv += optind;

    /* we obtain the tty settings from stdin . */
    LOG("tcgetattr(0, &saved_tty);\n");
    if ( tcgetattr(0, &saved_tty) < 0) {
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

    /* flush all descriptors before forking
     * (so no repeated messages on stdout). */
    fflush(NULL);

    child_pid = forkpty(&ptym,
            pty_name, &saved_tty, &saved_window_size);
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
            char cmd[UQ_PATH_MAX];
            if (shell) {
                LOG("Got shell from environment variable SHELL\n");
            } else {
                struct passwd *u = getpwnam(getlogin());
                if (u) {
                    shell = u->pw_shell;
                    LOG("Got shell from /etc/passwd file\n");
                }
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
        stty_raw.c_iflag &= ~(IXON | IXOFF | IXANY );
        stty_raw.c_cc[VMIN] = 1;
        stty_raw.c_cc[VTIME] = 0;
        stty_raw.c_cc[VSTOP] = 0;
        stty_raw.c_cc[VSTART] = 0;

        /* RESTORE TTY SETTINGS AT END */
        atexit(atexit_handler);
        res = tcsetattr(0, TCSADRAIN, &stty_raw);
        if (res < 0) {
            ERR("tcsetattr(0, &stty_raw)"
                ERRNO "\n", EPMTS);
        } /* if */

        /* SET THE O_NONBLOCK on stdin */
        res = fcntl(0, F_GETFL);
        if (res < 0) {
            ERR("fcntl(0, F_GETFL) => %d:" ERRNO "\n",
                res, EPMTS);
        }
        int res2 = fcntl(0, F_SETFL, res | O_NONBLOCK);
        if (res2 < 0) {
            ERR("fcntl(0, F_SETFL, 0x%04x) => %d:"
                ERRNO "\n",
                res | O_NONBLOCK, res2,
                EPMTS);
        }

        /* ... AND ON ptym */
        res = fcntl(ptym, F_GETFL);
        if (res < 0) {
            ERR("fcntl(ptym=%d, F_GETFL) => %d:"
                ERRNO "\n",
                ptym, res, EPMTS);
        }
        res2 = fcntl(ptym, F_SETFL, res | O_NONBLOCK);
        if (res2 < 0) {
            ERR("fcntl(ptym=%d, F_SETFL, 0x%04x) => %d:"
                ERRNO "\n",
                ptym, res | O_NONBLOCK, res2, EPMTS);
        }

        /* INSTALL SIGNAL HANDLER FOR SIGWINCH */
        if (flags & FLAG_DOWINCH) {
            LOG("installing signal handler for SIGWINCH\r\n");
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
        while ((res = wait(&exit_code)) < 0
                && errno == EINTR)
        {
            LOG("Interrupt received, retry.\r\n");
        }
        LOG("wait(&exit_code == %d);\r\n", exit_code);

        /* WAIT FOR THE READING END */
        p_in.do_finish = DO_FINISH_ITER;
        res = pthread_join(p_in.id, NULL);
        if (res < 0) {
            LOG("pthread_join[%s]" ERRNO "\r\n",
                p_in.name, EPMTS);
        }

        /* WAIT FOR THE WRITING END */
        p_out.do_finish = TRUE;
        res = pthread_join(p_out.id, NULL);
        if (res < 0) {
            LOG("pthread_join[%s]" ERRNO "\r\n",
                p_out.name, EPMTS);
        }

        /* exit with the subprocess exit code */
        LOG("exit(%d);\r\n", WEXITSTATUS(exit_code));
        exit(WEXITSTATUS(exit_code));
    } /* PARENT */
} /* main */
