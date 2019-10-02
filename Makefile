# slowtty -- program to simulate the baudrate speed of a serial line on
# a local tty (or network based).
# Author: Luis Colorado <luiscoloradourcola@gmail.com>
# Copyright: (C) 2015 LUIS COLORADO.  This is open source copyrighted software.
# Date: Tue Aug  2 16:00:52 EEST 2016

prefix	?= $(HOME)
bindir	?= $(prefix)/bin
mandir	?= $(prefix)/man
man1dir	?= $(mandir)/man1

INSTALL	?=install
RM 		?= rm -f

CFLAGS	+= -pthread
DMOD	?= -m 0755
XMOD	?= -m 0711
FMOD	?= -m 0644
UMOD	?= -o `id -u` -g `id -g`

targets  = slowtty test_ring
toclean	+= $(targets)

test_ring_objs = test_ring.o ring.o
toclean += $(test_ring_objs)

slowtty_objs = slowtty.o delay.o ring.o gdc.o
toclean += $(slowtty_objs)
slowtty_libs = -lutil -lpthread

all: $(targets)

clean:
	$(RM) $(toclean)

install: $(targets)
	$(INSTALL) $(IFLAGS) $(DMOD) $(UMOD) -d $(bindir) $(man1dir)
	$(INSTALL) $(IFLAGS) $(DMOD) $(XMOD) slowtty $(bindir)
	$(INSTALL) $(IFLAGS) $(FMOD) slowtty.1 $(man1dir)

deinstall:
	$(RM) $(bindir)/slowtty

slowtty: $(slowtty_objs)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $($@_objs) $($@_libs)

test_ring: $(test_ring_objs)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $($@_objs) $($@_libs)

$(slowtty_objs): delay.h slowtty.h
