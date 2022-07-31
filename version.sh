#!/bin/sh

# Ensure file names are sorted consistently across platforms.
LC_ALL=C
export LC_ALL

srcdir=`dirname "$0"`
test -z "$srcdir" && srcdir=.
cd "$srcdir"

echo "version.sh is a stub!!!"

