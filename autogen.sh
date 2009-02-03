#! /bin/sh
# Run this to generate all the initial makefiles, etc.

set -e

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

DIE=0

AUTOMAKE=automake-1.4
ACLOCAL=aclocal-1.4

($AUTOMAKE --version) < /dev/null > /dev/null 2>&1 || {
        AUTOMAKE=automake
        ACLOCAL=aclocal
}

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have autoconf installed to compile libvncserver."
	echo "Download the appropriate package for your distribution,"
	echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
	DIE=1
}

($AUTOMAKE --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have automake installed to compile libvncserver."
	echo "Get ftp://sourceware.cygnus.com/pub/automake/automake-1.4.tar.gz"
	echo "(or a newer version if it is available)"
	DIE=1
}

if test "$DIE" -eq 1; then
	exit 1
fi

(test -f $srcdir/rfb/rfb.h) || {
	echo "You must run this script in the top-level libvncserver directory"
	exit 1
}

if test -z "$*"; then
	echo "I am going to run ./configure with no arguments - if you wish "
        echo "to pass any to it, please specify them on the $0 command line."
fi

$ACLOCAL $ACLOCAL_FLAGS
autoheader
$AUTOMAKE --add-missing --copy
autoconf

echo "Running ./configure --enable-maintainer-mode" "$@"
$srcdir/configure --enable-maintainer-mode "$@"

echo "Now type 'make' to compile libvncserver."
