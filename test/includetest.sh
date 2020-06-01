#!/bin/sh
#
# This tests if using the **installed** headers works.
#

TMPDIR=$(mktemp -d)

make install DESTDIR=$TMPDIR

echo \
"
#include <rfb/rfb.h>
#include <rfb/rfbclient.h>

int main()
{
    return 0;
}
" > $TMPDIR/includetest.c

cc -I $TMPDIR/usr/local/include $TMPDIR/includetest.c
