
prefix= /usr
bindir= $(prefix)/bin
INSTALL=install

CFLAGS += -D_XOPEN_SOURCE=700 -pthread

targets = slowtty

slowtty_objs = slowtty.o delay.o

objs = $(foreach i, $(targets), $($(i)_objs))

all: $(targets)
clean:
	$(RM) $(targets) $(objs)
install: $(targets)
	$(INSTALL) $(IFLAGS) -m 555 -o root -g root -d $(bindir)
	$(INSTALL) $(IFLAGS) -m 111 -o root -g root slowtty $(bindir)

slowtty: $(slowtty_objs)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $($@_objs) $($@_libs)

$(slowtty_objs): delay.h
