#!/bin/sh

# Ensure file names are sorted consistently across platforms.
LC_ALL=C
export LC_ALL

srcdir=`dirname "$0"`
test -z "$srcdir" && srcdir=.
cd "$srcdir"

if [ -f .version ]; then
	( cat version 2>/dev/null ) | xargs printf '%s'
	exit
fi

LAST_TAG=`git describe --abbrev=0 --tags 2>/dev/null`
if [ $? -eq 0 ]; then
	git log -n1 --pretty='%h' | xargs printf "$LAST_TAG-%s"
else
	git log -n1 --pretty='%h' | xargs printf '%s'
fi
