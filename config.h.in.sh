#!/bin/sh
# Author: Luis Colorado <luis.colorado@spindrive.fi>
# Date: Thu Dec 12 08:57:33 EET 2024
# Copyright: (c) 2024 SpinDrive Oy, FI.  All rights reserved.

cat <<EOF
/* config.h -- configuration parameters for @PACKAGE@ program.
 * This file generated automatically from config.mk, so
 * please don't edit.
 * Author: @AUTHOR_NAME@ <@AUTHOR_EMAIL@>
 * Date: @BUILD_DATE@
 * Copyright: (c) @COPYRIGHT_YEARS@ @AUTHOR_NAME@.  All rights reserved.
 */
#ifndef CONFIG_H
#define CONFIG_H

EOF

sed -E -e '/^[ 	]*(#.*)?$/d' \
    -e '/^[ 	]*([A-Za-z_][A-Za-z0-9_]*).*/s//\1/' \
| awk '{
    if ($0 ~ /^UQ_/) {
        printf("#define %-25s @%s@\n", $0, $0);
    } else {
        printf("#define %-25s \"@%s@\"\n", $0, $0);
    }
}'


cat <<EOF

#endif /* CONFIG_H */
EOF
