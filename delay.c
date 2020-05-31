/* delay.c -- routine to delay for amount 0.04s (25 updates/s.)and
 * calculate the number of characters that can be output in that time,
 * based on the baudrate used in the tty device.
 * Author: Luis Colorado <luiscoloradourcola@gmail.com>
 * Date: jue jun 25 08:35:40 EEST 2015
 * Version: 0.10
 * Disclaimer: (C) 2015 Luis Colorado <luiscoloradourcola@gmail.com>
 *             all rights reserved.
 */

/* We consider the time divided in tics of 0.04s (25 frames/sec)
 * and, to be precise, we stick on 0.04s tic marks, so that value
 * is, indeed, fixed as a constant (not configurable).
 *
 * Based on the baudrate (we check if baudrate, char size, stopbits or
 * parity has changed and only do this calculation in case of a
 * change.) we then calculate the amount of time a single character
 * needs to be sent over the line, and based on this we get the number
 * of characters that can be sent in the next tick time.
 *
 * We do accumulate the fraction of char allowable, if there's no
 * enough time to send another full character, and accumulate that
 * fraction, based on the amount resultant, so we get an exact number
 * of characters to be written to the output device, rounded to one
 * char. */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "gdc.h"

#ifndef NDEBUG
#define NDEBUG 0
#endif

#include "slowtty.h"
#include "delay.h"

/* Get the integer number of bits per second from the c_lflag field
 * @param t struct termios pointer where to get the output baudrate.
 * @return the baudrate as an integer. */
static unsigned long getthebr(struct termios *t)
{

#define B(_n) case B##_n: do {                    \
        /*LOG("setting is %d baudios\r\n", _n);*/ \
        return (_n);                              \
    } while(0)

    speed_t s = cfgetospeed(t);
    switch(s) {
        B(50); B(75); B(110);
        B(134); B(150); B(200); B(300);
        B(600); B(1200); B(1800); B(2400);
        B(4800); B(9600); B(19200); B(38400);
        B(57600); B(115200); B(230400);
        default: return B9600 == 9600 ? s : 0;
    } /* switch */
#undef B
} /* getthebr */

unsigned long delay(struct pthread_info *pi)
{
    unsigned long res;

    /* the recalculation of delay times depends on the change of termios
     * parameters.  Only when a change in termios parameters is made we
     * calculate the new values for the number of characters to output
     * and * the delay time.  We initialize it to all zeros, so in the
     * first time we get an update. */
    if ((res = tcgetattr(ptym, &saved_tty)) < 0) {
        ERR("%s: tcgetattr " ERRNO "\r\n", pi->name, EPMTS);
    }

    speed_t new_baudrate = getthebr(&saved_tty);
    tcflag_t  new_cflag = saved_tty.c_cflag;

    if (   pi->svd_bauds != new_baudrate
        || pi->svd_cflag != new_cflag) { /* changed parameters */

        int bits_per_char;
        switch (new_cflag & CSIZE) { /* character size */
        case CS8: bits_per_char = 10; break; /* START,8 DATA,STOP */
        case CS7: bits_per_char = 9; break; /* START,7 DATA,STOP */
        case CS6: bits_per_char = 8; break; /* START,6 DATA,STOP */
        case CS5: bits_per_char = 7; break; /* START,5 DATA,STOP */
        } /* switch */
        if (saved_tty.c_cflag & PARENB) bits_per_char++; /* PARITY bit */
        if (saved_tty.c_cflag & CSTOPB) bits_per_char++; /* 2ND_STOP */

        pi->num = new_baudrate;
        pi->den = bits_per_char * TICS_PER_SEC;  /* ticks/sec. */
        long common_div = gdc(pi->num, pi->den);
        if (common_div > 1) {
            pi->num /= common_div;
            pi->den /= common_div;
        }
        pi->acc = pi->den / 2; /* round to half a tic */

        LOG("%s: num==%ld, den=%ld, acc=%ld\r\n",
                pi->name, pi->num, pi->den, pi->acc);
        pi->svd_bauds = new_baudrate;
        pi->svd_cflag = new_cflag;
    }

    /* now, update */
    pi->acc += pi->num % pi->den;
    pi->ctw  = pi->num / pi->den;
    if (pi->acc >= pi->den) { /* carry */
        pi->ctw++;
        pi->acc -= pi->den;
    }
    LOG("%s: pi->acc==%ld, pi->den==%ld, pi->ctw==%ld\r\n",
        pi->name, pi->acc, pi->den, pi->ctw);

    /* add the tic delay */
    pi->tic.tv_nsec += TIC_DELAY;
    pi->tic.tv_nsec -= pi->tic.tv_nsec % TIC_DELAY;
    if (pi->tic.tv_nsec >= 1000000000) { /* carry */
        pi->tic.tv_sec++;
        pi->tic.tv_nsec -= 1000000000;
    }
    res = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &pi->tic, NULL);
    if (res < 0) {
        ERR("%s: clock_gettime" ERRNO "\r\n", pi->name, EPMTS);
    }

    return pi->ctw;
} /* delay */
