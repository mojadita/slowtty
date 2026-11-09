# slowtty -- program to simulate the baudrate speed of a serial line on
#            a local tty (or network based).
# Author: Luis Colorado <luiscoloradourcola@gmail.com>
# Date: Tue Aug  2 16:00:52 EEST 2016
# Copyright: (C) 2015-2026 LUIS COLORADO.  All rights reserved.
# License: BSD v2

OS               != uname -o
CFLAGS-FreeBSD   ?= -DHAS_LIBUTIL_H=1
CFLAGS-GNU/Linux ?= -DHAS_PTY_H=1

CFLAGS           += $(CFLAGS-$(OS))

INSTALL	         ?= install
RM 		         ?= rm -f

CFLAGS	         += -pthread
DMOD	         ?= -m 0755
XMOD	         ?= -m 0755
FMOD	         ?= -m 0644

OWN-FreeBSD      ?= -o root
GRP-FreeBSD      ?= -g wheel

OWN-GNU/Linux    ?= -o bin
GRP-GNU/Linux    ?= -g bin

IFLAGS         ?= $(OWN-$(OS)) $(GRP-$(OS))

targets         = slowtty test_ring slowtty.1.gz
toclean	       += $(targets)

test_ring_objs  = test_ring.o ring.o
toclean        += $(test_ring_objs)

slowtty_objs    = slowtty.o delay.o ring.o gdc.o main.o flow.o
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
	$(INSTALL) $(IFLAGS) $(XMOD) slowtty $@

$D$(man1dir)/slowtty.1.gz: $(@:T) $(@:H)
	$(INSTALL) $(IFLAGS) $(FMOD) slowtty.1.gz $@

$D$(bindir) $D$(man1dir):
	$(INSTALL) $(IFLAGS) $(DMOD) -d $@

deinstall uninstall:
	$(RM) $(toinstall)

slowtty: $(slowtty_deps) $(slowtty_objs)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $($@_ldflags) \
			$($@_objs) $($@_libs)

test_ring: $(test_ring_deps) $(test_ring_objs)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $($@_ldflags) \
			$($@_objs) $($@_libs)

# delay.c flow.c gdc.c main.c ring.c slowtty.c test_ring.c
delay.o: delay.c gdc.h main.h slowtty.h flow.h ring.h delay.h
flow.o: flow.c slowtty.h flow.h ring.h
gdc.o: gdc.c gdc.h
main.o: main.c config.h slowtty.h flow.h ring.h main.h
ring.o: ring.c ring.h slowtty.h flow.h
slowtty.o: slowtty.c config.h main.h ring.h slowtty.h flow.h delay.h
test_ring.o: test_ring.c ring.h
