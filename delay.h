/* delay.h -- interface to module delay.
 * Author: Luis Colorado <luiscoloradourcola@gmail.com>
 * Date: jue jun 25 12:15:52 EEST 2015
 * Copyright: (C) 2015 LUIS COLORADO.  This is open source
 *            according to GPLv2.
 */
#ifndef _DELAY_H
#define _DELAY_H

/* This routine makes a delay according to the struct termios passed
 * and return the number of characters allowed to be output for the
 * next round.  It's based on a delay between MIN_DELAY and 2*MIN_DELAY
 * and the change of the termios structure against the last value.
 *
 * @param t is the thread info, with parameters of one direction
 *          in the communications link
 * @return  The value of the window (the number or characters
 *          that can be sent in one delay tic) */
extern unsigned long
delay(
        struct pthread_info *t);

#endif /* _DELAY_H */
