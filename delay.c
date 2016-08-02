/* delay.c -- routine to delay for amount 0.025s <= d <= 0.050s
 * and determine number of characters to emit from a baudrate.
 * Author: Luis Colorado <luiscoloradourcola@gmail.com>
 * Date: jue jun 25 08:35:40 EEST 2015
 * Version: 0.9
 * Disclaimer: (C) 2015 Luis Colorado <luiscoloradourcola@gmail.com>
 *             all rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <unistd.h>

#include "slowtty.h"
#include "delay.h"

#ifndef MIN_DELAY  /* so we can change the value on the command line */
#define MIN_DELAY   25000
#endif

#ifndef NDEBUG
#define NDEBUG 0
#endif

#define D(X) "%s:%d: %s: " X, __FILE__, __LINE__, __func__

#if NDEBUG
#  define DEB(X, args...) do { \
		if (flags & FLAG_VERBOSE) { \
			fprintf(stderr, D(X), args); \
		} \
	} while(0)
#else
#  define DEB(X, args...)
#endif /* DEBUG */

/* Get the integer number of bits per second from the c_lflag field
 * @param t struct termios pointer where to get the output baudrate.
 * @return the baudrate as an integer. */
static unsigned long getthebr(struct termios *t)
{
#define B(_n) case B##_n: do { DEB("setting %d baudios\r\n", _n); return (_n); } while(0)
    switch(cfgetospeed(t)) {
        B(50); B(75); B(110);
        B(134); B(150); B(200); B(300);
        B(600); B(1200); B(1800); B(2400);
        B(4800); B(9600); B(19200); B(38400);
        B(57600); B(115200); B(230400);
        default: return 300; /* else we assume 300 baud */
    } /* switch */
#undef B
} /* getthebr */

unsigned long delay(struct termios *t)
{
    static struct termios old_termios = {0}; /* initialized to all zeros */
    static unsigned long denominator = MIN_DELAY; /* initialize fo 1 char per MIN_DELAY */
    static unsigned long numerator_int = 1;
    static unsigned long numerator_mod = 0;
    static unsigned long numerator_acc = MIN_DELAY/2;
    unsigned long result;

    /* the recalculation of delay times depends on the change of termios
     * parameters.  Only when a change in termios parameters is made we
     * calculate the new values for the number of characters to output and
     * the delay time. We initialize it to all zeros, so in the first time
     * we get an update. */
    if (t) {
        if (memcmp(t, &old_termios, sizeof old_termios)) { /* has changed */
            unsigned long numerator = getthebr(t);

            denominator = 2; /* the start bit plus one stop bit */

            switch (t->c_cflag & CSIZE) { /* character size */
            default: /* NOBREAKHERE */
            case CS8: denominator++; /* NOBREAKHERE */
            case CS7: denominator++; /* NOBREAKHERE */
            case CS6: denominator++; /* NOBREAKHERE */
            case CS5: denominator += 5; /* NOBREAKHERE */
            } /* switch */
            if (t->c_cflag & PARENB) denominator++; /* parity bit */
            if (t->c_cflag & CSTOPB) denominator++; /* extra stop bit */

            numerator *= MIN_DELAY; /* denominator is at least 7*MIN_DELAY */
            denominator *= 1000000;

            /* now, numerator/denominator is the number of characters per MIN_DELAY */
            /* the output routine is capable of output characters at a speed under
             * the value imposed here, but not over */
            /* we now adjust the delay to be between MIN_DELAY and 2*MIN_DELAY */
            while (denominator < MIN_DELAY) { numerator <<= 1; denominator <<= 1; }
            while (denominator >= 2*MIN_DELAY) { numerator >>= 1; denominator >>= 1; }
            /* now, MIN_DELAY <= denominator < 2*MIN_DELAY */
            numerator_mod = numerator % denominator;
            numerator_int = numerator / denominator;
            numerator_acc = numerator_mod / 2; /* rounding */
            old_termios = *t; /* copy termios structure */
            DEB("numerator_mod=%ld, numerator_int=%ld, numerator_acc=%ld, denominator=%ld\r\n",
                    numerator_mod, numerator_int, numerator_acc, denominator);
        } /* if */
        usleep(denominator);
    } /* has changed */

    /* now, calculate the number of characters we admit for the next read */
    result = numerator_int;
    numerator_acc += numerator_mod;
    if (numerator_acc >= denominator) {
        result++;
        numerator_acc -= denominator;
    } /* if */
    return result;
} /* delay */
