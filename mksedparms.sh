#!/bin/sh
# mksedparms.sh -- generate from config.mk the set of sed parameters
# to use in Makefile to change configuration options into their
# configured values.  The script is a filter, reads its standard input
# and writes to standard output, so maintainance is simpler.

sed -Ee '/^[ 	]*(#.*)?$/d' \
     -e 's"^[ 	]*([A-Za-z_][A-Za-z0-9_]*)[ 	]*\??=[ 	]*(.*)$"-e '\''s\"@\1@\"\2\"g'\''"'

echo
