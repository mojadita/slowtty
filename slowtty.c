#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <pwd.h>
#include <termios.h>
#include <sys/types.h>
#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <pty.h>
#include <utmp.h>

int ptym, ptys;

#define D(x) __FILE__":%d: %s: " x, __LINE__, __func__
#define ERRNO ": %s (errno=%d)\n"
#define EPMTS strerror(errno), errno
#define ERR(x, args...) do { fprintf(stderr, D("ERROR: " x ), args); exit(EXIT_FAILURE); } while (0)
#define WARN(x, args...) do { fprintf(stderr, D("WARNING: " x), args); } while (0)
#define LOG(x, args...) do { fprintf(stderr, D("INFO: " x), args); } while (0)

int delay = 20000;
struct termios saved_tty;

struct pthread_info {
    pthread_t       id;
    int             from_fd,
                    to_fd;
    struct termios  saved_cfg;
    char           *name;
    unsigned char   buffer[BUFSIZ];
    size_t          buffsz;
};

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
}

void *pthread_body_writer(void *_pi)
{
    struct pthread_info *pi = _pi;
    int n;

    LOG("pthread %d: from_fd=%d, to_fd=%d, name=%s\r\n",
            pi->id, pi->from_fd, pi->to_fd, pi->name);

    while ((n = read(pi->from_fd, pi->buffer + pi->buffsz, sizeof pi->buffer - pi->buffsz)) > 0) {
        int i;
        pi->buffsz += n;
        for (i = 0; i < pi->buffsz; i++) {
            if (write(pi->to_fd, pi->buffer + i, 1) < 0) ERR("write" ERRNO, EPMTS);
            usleep(delay);
        }
        pi->buffsz = 0;
    } /* while */
    return NULL;
} /* pthread_body_writer */

void pass_winsz(struct pthread_info *pi)
{
    struct winsize ws;
    ioctl(pi->from_fd, TIOCGWINSZ, &ws);
    ioctl(pi->to_fd, TIOCSWINSZ, &ws);
} /* pass_winsz */


void *pthread_body_reader(void *_pi)
{
    struct pthread_info *pi = _pi;
    size_t r, w;
    int res;
    struct winsize ws;

    LOG("pthread %d: from_fd=%d, to_fd=%d, name=%s\r\n",
            pi->id, pi->from_fd, pi->to_fd, pi->name);

    pass_winsz(pi);

    for (;;) {
        size_t r = read(pi->from_fd, pi->buffer, sizeof pi->buffer);
        if (r < 0) {
           if (errno == EINTR)
               pass_winsz(pi);
           else ERR("%s: read" ERRNO, pi->name, EPMTS);
        } else if (r == 0) {
            break;
        } else { /* r > 0 */
            while (r) {
                size_t w = write(pi->to_fd, pi->buffer, r);
                if (w < 0) ERR("%s: write" ERRNO, pi->name, EPMTS);
                r -= w;
            } /* while */
        } /* if */
    }

    return NULL;
} /* pthread_body_reader */

int main(int argc, char **argv)
{
    int shelpid;
    int opt, res;

    while ((opt = getopt(argc, argv, "t:")) != EOF) {
        switch (opt) {
        case 't': delay = atoi(optarg); break;
        } /* switch */
    } /* while */

    argc -= optind; argv += optind;

    /* we obtain the tty settings from stdin.  In case
     * it's not available, we try stdout.  In case neither
     * input and output is not redirected from a tty, so
     * we give up. */
    res = tcgetattr(0, &saved_tty);
    if (res < 0) {
        res = tcgetattr(1, &saved_tty);
        if (res < 0) ERR("tcgetattr" ERRNO, EPMTS);
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

            /* redirections */
            if (!argc) { /* we are launching a shell, become a new process group */
                res = setsid();
                LOG("setsid() => %d\r\n", res);
            }
            close(0); close(1); close(2);
            if ((ptys = open(pn, O_RDWR)) < 0)
                ERR("open: %s" ERRNO, pn, EPMTS);
            res = tcsetattr(ptys, TCSAFLUSH, &saved_tty);
            if (res < 0) ERR("tcsetattr: %s" ERRNO, pn, EPMTS);

            LOG("open: %s: ptys=%d\n", pn, ptys);
            dup(ptys); dup(ptys); close(ptym);

            if (argc) {
                execvp(argv[0], argv);
                ERR("execvp: %s" ERRNO, argv[0], EPMTS);
                sleep(10);
            } else {
                char *shellenv = "SHELL";
                char *shell = getenv(shellenv);

                if (!shell) {
                    struct passwd *u;
                    uid_t uid = getuid();
                    LOG("%s variable not found, trying user info "
                            "from /etc/passwd\n",
                            shellenv);

                    while (u = getpwent())
                        if (u->pw_uid == uid)
                            break;

                    shell = u ? u->pw_shell : NULL;
                } /* if */
            
                execlp(shell, shell, NULL);
                ERR("execlp: %s" ERRNO, shell, EPMTS);
            } /* if */
            /* NOTREACHED */
        } /* case */
    default: { /* parent process */
            struct pthread_info p_in, p_out;
            int res;

            {   struct termios stty_raw = saved_tty;

                cfmakeraw(&stty_raw);
                res = tcsetattr(0, TCSAFLUSH, &stty_raw);
                if (res < 0) {
                    LOG("tcsetattr(0, ...): ERROR" ERRNO, EPMTS);
                    res = tcsetattr(1, TCSAFLUSH, &stty_raw);
                    if (res < 0)
                        ERR("tcsetattr(1, ...): ERROR" ERRNO, EPMTS);
                } /* if */
            } /* block */

            res = pthread_create(
                    &p_in.id,
                    NULL,
                    pthread_body_reader,
                    init_pthread_info(
                        0, ptym,
                        "READ",
                        &p_in));
            if (res < 0) ERR("pthread_create" ERRNO, EPMTS);

            res = pthread_create(
                    &p_out.id,
                    NULL,
                    pthread_body_writer,
                    init_pthread_info(
                        ptym, 1, 
                        "WRITE",
                        &p_out));
            if (res < 0) ERR("pthread_create" ERRNO, EPMTS);

            wait();

            /* restore the settings from the saved ones. We
             * follow the same procedure (first with stdin, then
             * with stdout) so we configure the same settings
             * to the same channel as in the beginning. */
            res = tcsetattr(0, TCSAFLUSH, &saved_tty);
            if (res < 0) {
                LOG("tcsetattr(0, ...): ERROR" ERRNO, EPMTS);
                res = tcsetattr(1, TCSAFLUSH, &saved_tty);
                if (res < 0)
                    ERR("tcsetattr(1, ...): ERROR" ERRNO, EPMTS);
            } /* if */

            exit(EXIT_SUCCESS);
        } /* case */
    } /* switch */
} /* main */
