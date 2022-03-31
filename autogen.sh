#!/bin/sh

# Set directories
origdir="$(pwd -P)"
srcdir="$(dirname "$0")"
test -z "$srcdir" && srcdir=.

# Copy install-sh and run autoconf
cd "$srcdir"
cp "$(automake --print-libdir)/install-sh" .
autoconf

# Pull in thomtl's fork of lai, with improved real hw support
mkdir -p $srcdir/third_party
git clone https://github.com/thomtl/lai.git $srcdir/third_party/lai

# Pull in echfs, and build the tools
git clone https://github.com/echfs/echfs.git $srcdir/third_party/echfs
make -C $srcdir/third_party/echfs echfs-utils

# Pull in limine, which is the bootloader we currently use
git clone https://github.com/limine-bootloader/limine.git --branch=latest-binary --depth=1 $srcdir/third_party/limine
make -C $srcdir/third_party/limine

# Run 'configure' unless we're told not to...
if test -z "$NOCONFIGURE"; then
    cd "$origdir"
    exec "$srcdir"/configure "$@"
fi

