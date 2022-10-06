#!/usr/bin/env bash

# Ensure file names are sorted consistently across platforms.
LC_ALL=C
export LC_ALL

srcdir=`dirname "$0"`
test -z "$srcdir" && srcdir=.
cd "$srcdir"

if [ -f .version ]; then
	( cat .version 2>/dev/null ) | xargs printf '%s'
	exit
fi

GIT_COMMIT_HASH=`git rev-parse --short HEAD`
GIT_TAG=`git describe --abbrev=0 --tags 2>/dev/null`
OUTFILE="/dev/stdout"
GIT_HAS_TAG=$?

while [[ $# -gt 0 ]]; do
  case $1 in
    -t|--tagged)
      if [ $GIT_HAS_TAG -eq 0 ]; then
	printf "%s" $GIT_TAG &> $OUTFILE
	exit
      else
        echo "version.sh: git tag requested, but no tags on branch"
	exit 1
      fi
      ;;
    -o|--outfile)
      OUTFILE=$2
      shift
      shift
      ;;
    -*|--*)
      echo "version.sh: unknown option $1"
      exit 1
      ;;
  esac
done

if [ $GIT_HAS_TAG -eq 0 ]; then
  printf "%s-dev-%s" $GIT_TAG $GIT_COMMIT_HASH &> $OUTFILE
else
  printf "%s-git" $GIT_COMMIT_HASH &> $OUTFILE
fi
