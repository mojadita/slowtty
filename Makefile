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
XMOD	= -m 0711
FMOD	= -m 0644
UMOD	= -o `id -u` -g `id -g`

targets = test_ring slowtty

test_ring_objs = test_ring.o ring.o

slowtty_objs = slowtty.o delay.o
slowtty_libs = -lutil -lpthread

objs = $(slowtty_objs)

all: $(targets)
clean:
	$(RM) $(targets) $(objs)
install: $(targets)
	$(INSTALL) $(IFLAGS) $(DMOD) $(UMOD) -d $(bindir)
	$(INSTALL) $(IFLAGS) $(DMOD) $(XMOD) slowtty $(bindir)
deinstall:
	$(RM) $(bindir)/slowtty

.for t in $(targets)
toclean += $($t_objs) $t
$t: $($t_objs)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $($@_objs) $($@_libs)
.endfor

$(slowtty_objs): delay.h slowtty.h
