# config.mk -- configurable parameters for PACKAGE (see below)
# Author: Luis Colorado <luis.colorado@spindrive.fi>
# Date: Thu Dec 12 09:25:11 EET 2024
# Copyright: (c) 2024 SpinDrive Oy, FI.  All rights reserved.

PROGRAM_NAME             ?= slowtty
BUILD_DATE               ?= $(build_date)
DOC_DATE                 ?= $(doc_date)
PACKAGE                  ?= $(PROGRAM_NAME)
AUTHOR_NAME              ?= Luis Colorado
AUTHOR_EMAIL             ?= luiscoloradourcola@gmail.com
COPYRIGHT_YEARS          ?= 2015-2025
PROGRAM_NAME_UC          ?= SLOWTTY
AUTHOR_CORP              ?= N.A.
AUTHOR_SITE              ?= https://github.com/mojadita/$(PACKAGE)
UQ_VERSION               ?= 0.9.1
VERSION                  ?= $(UQ_VERSION)
VERSION_DATE             ?= Fri Apr 11 19:11:36 EEST 2025
OPERATING_SYSTEM         ?= UNIX

prefix                   ?= /usr/local
exec_prefix              ?= $(prefix)
bindir					 ?= $(prefix)/bin
sbindir					 ?= $(exec_prefix)/sbin
datarootdir              ?= $(prefix)/share
pkgdatadir               ?= $(datarootdir)/$(PACKAGE)
mandir                   ?= $(datarootdir)/man
man1dir                  ?= $(mandir)/man1
docdir                   ?= $(datarootdir)/doc/$(PACKAGE)
vardir                   ?= $(exec_prefix)/var
logdir                   ?= $(vardir)/log

# define only one of these two
UQ_HAS_PTY_H             ?=  0
UQ_PATH_MAX              ?=  1024
UQ_HAS_LIBUTIL_H         ?=  1

UQ_MAX_PTY_NAME          ?= 64
UQ_DEFAULT_BUFSIZ        ?= 64
UQ_DEFAULT_FLAGS         ?= (FLAG_DOWINCH)

UQ_USE_COLORS            ?=  1
UQ_USE_LOCUS             ?=  1
UQ_USE_DEB				 ?=  1
UQ_USE_INF               ?=  1
UQ_USE_WRN               ?=  1
UQ_USE_ERR               ?=  1
UQ_USE_CRT               ?=  1
UQ_NSTACK                ?= 200000
UQ_NPROG                 ?= 2000
UQ_NFRAME                ?= 100000
UQ_TAB_SIZE              ?= 4

UQ_LAST_TOKENS_SZ        ?= 64
UQ_DEFAULT_LOGLEVEL      ?= 0
