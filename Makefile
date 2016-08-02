# slowtty -- program to simulate the baudrate speed of a serial line on
# a local tty (or network based).
# Author: Luis Colorado <luiscoloradourcola@gmail.com>
# Copyright: (C) 2015 LUIS COLORADO.  This is open source copyrighted software.
# Date: Tue Aug  2 16:00:52 EEST 2016

prefix= $(HOME)
bindir= $(prefix)/bin
INSTALL=install
RM = rm -f

CFLAGS	+= -pthread
DMOD	= -m 0755
XMOD	= -m 0755
FMOD	= -m 0644
#UMOD	= -o root -g wheel

targets = slowtty

slowtty_objs = slowtty.o delay.o
slowtty_libs = -lutil

objs = $(slowtty_objs)

all: $(targets)
clean:
	$(RM) $(targets) $(objs)
install: $(targets)
	$(INSTALL) $(IFLAGS) $(DMOD) $(UMOD) -d $(bindir)
	$(INSTALL) $(IFLAGS) $(DMOD) $(XMOD) slowtty $(bindir)

slowtty: $(slowtty_objs)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $($@_objs) $($@_libs)

$(slowtty_objs): delay.h slowtty.h
