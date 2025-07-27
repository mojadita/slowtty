# slowtty -- program to simulate the baudrate speed of a serial line on
# a local tty (or network based).
# Author: Luis Colorado <luiscoloradourcola@gmail.com>
# Copyright: (C) 2015-2025 LUIS COLORADO.  All rights reserved.
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

OS             != uname -o

INSTALL	       ?= install
RM 		       ?= rm -f

CFLAGS	       += -pthread
DMOD	       ?= 0755
XMOD	       ?= 0755
FMOD	       ?= 0644

OWN-FreeBSD    ?= root
GRP-FreeBSD    ?= wheel

OWN-GNU/Linux  ?= bin
GRP-GNU/Linux  ?= bin

IFLAGS         ?= -o $(OWN-$(OS)) -g $(GRP-$(OS))

targets         = slowtty test_ring slowtty.1.gz
toclean	       += $(targets)

test_ring_objs  = test_ring.o ring.o
toclean        += $(test_ring_objs)

slowtty_objs    = slowtty.o delay.o ring.o gdc.o main.o
slowtty_libs    = -lutil -lpthread
toclean        += $(slowtty_objs)

all: $(targets)

include config-lib.mk

toinstall       = \
        $D$(bindir)/slowtty \
        $D$(man1dir)/slowtty.1.gz

clean:
	$(RM) $(toclean)

install: $(toinstall)

$D$(bindir)/slowtty: $(@:T) $(@:H)
	$(INSTALL) $(IFLAGS) -m $(XMOD) slowtty $@

$D$(man1dir)/slowtty.1.gz: $(@:T) $(@:H)
	$(INSTALL) $(IFLAGS) -m $(FMOD) slowtty.1.gz $@

$D$(bindir) $D$(man1dir):
	$(INSTALL) $(IFLAGS) -m $(DMOD) -d $@

deinstall:
	$(RM) $(toinstall)

slowtty: $(slowtty_deps) $(slowtty_objs)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $($@_objs) $($@_ldflags) $($@_libs)

test_ring: $(slowtty_deps) $(test_ring_objs)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $($@_objs) $($@_ldflags) $($@_libs)

# delay.c gdc.c main.c ring.c slowtty.c test_ring.c
delay.o: delay.c gdc.h main.h slowtty.h ring.h \
  delay.h
gdc.o: gdc.c gdc.h
main.o: main.c config.h slowtty.h ring.h main.h 
ring.o: ring.c ring.h slowtty.h 
slowtty.o: slowtty.c config.h main.h ring.h \
  slowtty.h delay.h
test_ring.o: test_ring.c ring.h 
