This is a very simple program o illustrate how `pty(3)`s work.

It allows us to emulate old, text based terminals which run at low
baud speeds.  When you use it, the terminal speed you configure with
`stty(1)` is commited and the terminal begins to output characters at
the speed mandated by the baud rate configured.  You'll see it
sending characters as baudrate mandates and see the screen updates
at low speed as they happened to be once upon a time.

As it is so simple to build it with `make(1)`, I have not considered
appropiate to embed it into `autoconf(1)`, `automake(1)` and/or
`autohead(1)` utils, so you have to compile it using _FreeBSD_
specific flags.

To compile on _FreeBSD_, just issue the command

    make CFLAGS=-DHAS_LIBUTIL_H=1 install

in the source code directory.  The program will be installed by
default in `${HOME}/bin`.  If you want to install it in the
`/usr/local` hierarchy (or elsewhere), just uncomment in the
`Makefile` the `UMOD=...` line and then, issue the command:

    make prefix="/usr/local" CFLAGS=-DHAS_LIBUTIL_H=1 install

and you'll have it installed properly.

##How to use it

At this moment there's no manual page for it.  Just test the three
options it has:

    -v displays traces of what the program is doing.  Very useful
        for debugging purposes.

    -t makes slowtty not to pass the current tty settings to the
        slave pty, so the initial settings of it are used instead.

    -w makes slowtty not to register a signal handler for the
        SIGWINCH signal.  In this case the program running on the
        slave terminal will not be signalled fo window size changes,
        as there is no correspondence between the real terminal
        window size and the slave.

Enjoy!
