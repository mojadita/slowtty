# slowtty -- program to simulate the baudrate speed of a serial line on
# a local tty (or network based).
# Author: Luis Colorado <luiscoloradourcola@gmail.com>
# Copyright: (C) 2015 LUIS COLORADO.  This is open source copyrighted software.
# Date: Tue Aug  2 16:00:52 EEST 2016

# Uncomment the following line to compile this program on BSD
# systems, or systems that have the forkpty() function prototype
# defined there.  For example, this is required in FreeBSD to 
# eliminate a compiler warning.
#
# CFLAGS += -DHAS_LIBUTIL_H=1

# Uncomment the following line to compile this program on Linux
# systems, or systems that have the forkpty() function prototype
# defined in pty.h.  For example, this is required in Linux to
# eliminate a compiler warning.
#
# CFLAGS += -DHAS_PTY_H=1

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
	$(RM) $(man1dir)/slowtty.1

slowtty: $(slowtty_deps) $(slowtty_objs)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $($@_objs) $($@_ldflags) $($@_libs)

test_ring: $(slowtty_deps) $(test_ring_objs)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $($@_objs) $($@_ldflags) $($@_libs)

$(slowtty_objs): delay.h slowtty.h
