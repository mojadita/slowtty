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

---

# MANPAGE

SLOWTTY(1) - FreeBSD General Commands Manual

## NAME

**slowtty** - a program to simulate baudrates of old ttys over networked
lines.

## SYNOPSIS

**slowtty**
\[**-dltw**]
\[**-b**&nbsp;*bufsize*]
\[**command**&nbsp;\[*arguments*&nbsp;**...**]]

## DESCRIPTION

The
**slowtty**
takes a pseudo-tty and simulates on it the behaviour of a slow
tty line, by using the configured baudrate, and character frame
length to calculate the character transmission time and adapt
the output to it, so characters flow at a fixed pace as the
serial line would do.

While you are running the
**slowtty**
command, all the settings made with the
**stty(1)**
utility that result in changes in the baudrate or character size,
parity or the like, will be taken into account by
**slowtty**
and will be obeyed to simulate changes in the line configuration.

If you don't add any
**command**
to the parameter list, the
**slowtty**
utility just spawns a shell (as specified by the
**$SHELL**
environment variable.)  The shell can be made a login shell
with the option
**-l**
(see below)

**-b**

> Allows to set the internal buffer size used to read characters
> from the slave tty.  Normally this is adjusted dynamically so
> no more than one second characters get buffered on output.
> The idea is to allow for
> **^C**
> characters to be processed without having to print lots of
> buffered characters, while allowing for high speeds to allow to
> process the buffer in chunks to maintain the average stream flow.

**-d**

> This flag makes the
> **slowtty**
> program verbose, outputting log lines to stderr about what
> it is doing.
> It is useful for debugging purposes.

**-l**

> prependeds a
> **-**
> to the program name argument in the command to execute.
> This, when used with a shell command, has the effect of making
> the shell a login shell, so it will execute the login scripts
> and do user session initialization as if a normal login has been
> done.

**-t**

> With this option,
> **slowtty**
> does not use
> **tcsetattr(3)**
> to set the master terminal attributes, neither it passes the
> settings on the master to the slave pty.

**-w**

> Makes
> **slowtty**
> not transmit the *window size* attributes to the slave
> tty, so the program run is not aware of terminal window size
> changes.

## AUTHOR

Luis Colorado &lt;[luiscoloradourcola@gmail.com](mailto:luiscoloradourcola@gmail.com)&gt;

 \- August 8, 2019
