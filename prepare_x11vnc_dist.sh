#!/bin/bash

VERSION="0.7.2"

cd "$(dirname "$0")"

mv configure.ac configure.ac.LibVNCServer

cat configure.ac.LibVNCServer | \
sed -e "s/LibVNCServer, [^,)]*\([(,]\)*/x11vnc, $VERSION\1/g" \
    -e "s/\(contrib\|examples\|vncterm\|libvncclient\|test\|client_examples\)\/Makefile//g" \
    -e "s/LibVNCServer.spec/x11vnc.spec/g" \
    -e "s/^.*libvncserver-config//g" \
> configure.ac

mv Makefile.am Makefile.am.LibVNCServer

echo "EXTRA_DIST=tightvnc-1.3dev5-vncviewer-alpha-cursor.patch README.LibVNCServer" > Makefile.am
cat Makefile.am.LibVNCServer | \
sed -e "s/^SUBDIRS.*$/SUBDIRS=libvncserver x11vnc classes/" \
    -e "s/^DIST_SUBDIRS.*$/DIST_SUBDIRS=libvncserver x11vnc classes/" \
    -e "/all: make_config_executable/,\$d" \
    -e "/^.*bin_SCRIPTS.*$/d" \
    -e "s/include_/noinst_/" \
>> Makefile.am

mv README README.LibVNCServer
cp x11vnc/README ./README

cat LibVNCServer.spec.in | \
sed -e "s/Johannes.Schindelin@gmx.de/runge@karlrunge.com/gi" \
    -e "s/Johannes.Schindelin/Karl Runge/g" \
    -e "s/a library to make writing a vnc server easy/a VNC server for the current X11 session/" \
    -e "/%description/,/%prep/d" \
    -e '/%setup/s/^\(.*\)$/%description\
x11vnc is to Xwindows what WinVNC is to Windows, i.e. a server\
which serves the current Xwindows desktop via RFB (VNC) protocol\
to the user.\
\
Based on the ideas of x0rfbserver and on LibVNCServer, it has evolved\
into a versatile and performant while still easy to use program.\
\
x11vnc was put together and is (actively ;-) maintained by\
Karl Runge <runge@karlrunge.com>\
\
%prep\
\1/' \
> x11vnc.spec.in

mv libvncserver/Makefile.am libvncserver/Makefile.am.LibVNCServer

cat libvncserver/Makefile.am.LibVNCServer | \
sed -e "s/\(include\|LIB\|lib\)_/noinst_/g" \
> libvncserver/Makefile.am

cp classes/Makefile.am classes/Makefile.am.LibVNCServer
echo 'pkgdatadir = $(datadir)/@PACKAGE@/classes' >> classes/Makefile.am
echo 'pkgdata_DATA=VncViewer.jar index.vnc' >> classes/Makefile.am

mv acinclude.m4 acinclude.m4.LibVNCServer

cat acinclude.m4.LibVNCServer | \
sed -e "s/^\(_PKG.*\)\$PACKAGE\(.*\)$/\1LibVNCServer\2/" \
> acinclude.m4

make x11vnc-${VERSION}.tar.gz
for f in configure.ac Makefile.am libvncserver/Makefile.am classes/Makefile.am acinclude.m4 README; do
	mv -f $f.LibVNCServer $f
done

