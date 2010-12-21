#!/bin/bash

VERSION="0.9.13"

cd "$(dirname "$0")"

mv configure.ac configure.ac.LibVNCServer

cat configure.ac.LibVNCServer | \
egrep -v '(AC_CONFIG_COMMANDS|chmod).*libvncserver-config' | \
egrep -v '^[ 	]*libvncserver-config$' | \
sed -e "s/LibVNCServer, [^,)]*\([(,]\)*/x11vnc, $VERSION\1/g" \
    -e "s/\(contrib\|examples\|vncterm\|test\|client_examples\)\/Makefile//g" \
    -e "s/LibVNCServer.spec/x11vnc.spec/g" \
    -e "s/AC_PROG_LIBTOOL/AC_PROG_RANLIB/" \
    -e "s/PKG_CHECK/#PKG_CHECK/" \
    -e 's/if test "x$with_gnutls/with_gnutls=no; if test "x$with_gnutls/' \
    -e 's/if test "x$with_ipv6/with_ipv6=no; if test "x$with_ipv6/' \
> configure.ac

mv Makefile.am Makefile.am.LibVNCServer

echo "EXTRA_DIST=tightvnc-1.3dev5-vncviewer-alpha-cursor.patch RELEASE-NOTES README.LibVNCServer" > Makefile.am
echo "" >> Makefile.am
echo "if HAVE_SYSTEM_LIBVNCSERVER" >> Makefile.am
echo "SUBDIRS=x11vnc classes" >> Makefile.am
echo "DIST_SUBDIRS=x11vnc classes" >> Makefile.am
echo "else" >> Makefile.am
echo "SUBDIRS=libvncserver libvncclient x11vnc classes" >> Makefile.am
echo "DIST_SUBDIRS=libvncserver libvncclient x11vnc classes" >> Makefile.am
echo "endif" >> Makefile.am
echo "" >> Makefile.am

cat Makefile.am.LibVNCServer | \
sed -e "s/^SUBDIRS.*$/#SUBDIRS=libvncserver libvncclient x11vnc classes/" \
    -e "s/^DIST_SUBDIRS.*$/#DIST_SUBDIRS=libvncserver libvncclient x11vnc classes/" \
    -e "/^.*bin_SCRIPTS.*$/d" \
    -e "s/^include_HEADERS/if HAVE_SYSTEM_LIBVNCSERVER^else^include_HEADERS/" \
    -e "s/rfbclient\.h/rfbclient.h^endif/" \
    | tr '^' '\n' \
>> Makefile.am

mv README README.LibVNCServer
cp x11vnc/README ./README
cp x11vnc/RELEASE-NOTES ./RELEASE-NOTES

cat LibVNCServer.spec.in | \
sed -e "s/Johannes.Schindelin@gmx.de/runge@karlrunge.com/gi" \
    -e "s/Johannes.Schindelin/Karl Runge/g" \
    -e "s/a library to make writing a vnc server easy/a VNC server for the current X11 session/" \
    -e "/^%description$/,/%description devel$/d" \
    -e 's/^Static libraries.*$/%description\
x11vnc is to X Window System what WinVNC is to Windows, i.e. a server\
which serves the current Xwindows desktop via RFB (VNC) protocol\
to the user.\
\
Based on the ideas of x0rfbserver and on LibVNCServer, it has evolved\
into a versatile and performant while still easy to use program.\
\
x11vnc was put together and is (actively ;-) maintained by\
Karl Runge <runge@karlrunge.com>\
\
/i' \
> x11vnc.spec.in.tmp

perl -e '
    $s = 0;
    while (<>) {
	if ($s) {
		if (/^\s*$/) {
			$s = 0;
		}
	} else {
		if (/^%files\s*$/ || /^%files devel/) {
			$s = 1;
		}
	}
	next if $s;
	if (/^%files x11vnc/) {
		print "\%files\n";
		print "\%doc README x11vnc/ChangeLog\n";
		next;
	}
	print;
    }' < x11vnc.spec.in.tmp > x11vnc.spec.in

rm -f x11vnc.spec.in.tmp

mv libvncserver/Makefile.am libvncserver/Makefile.am.LibVNCServer

cat libvncserver/Makefile.am.LibVNCServer | \
sed -e "s/\(include\|LIB\|lib\)_/noinst_/g" \
    -e "s/_la_/_a_/" \
    -e "s/\.la/.a/" \
    -e "s/_LTLIBRARIES/_LIBRARIES/" \
> libvncserver/Makefile.am

mv libvncclient/Makefile.am libvncclient/Makefile.am.LibVNCServer

cat libvncclient/Makefile.am.LibVNCServer | \
sed -e "s/\(include\|LIB\|lib\)_/noinst_/g" \
    -e "s/_la_/_a_/" \
    -e "s/\.la/.a/" \
    -e "s/_LTLIBRARIES/_LIBRARIES/" \
> libvncclient/Makefile.am

mv x11vnc/Makefile.am x11vnc/Makefile.am.LibVNCServer

cat x11vnc/Makefile.am.LibVNCServer | \
sed -e "s/_la_/_a_/" \
    -e "s/\.la/.a/g" \
    -e "s/_LTLIBRARIES/_LIBRARIES/" \
> x11vnc/Makefile.am


cp classes/Makefile.am classes/Makefile.am.LibVNCServer
echo 'pkgdatadir = $(datadir)/@PACKAGE@/classes' >> classes/Makefile.am
echo 'pkgdata_DATA=VncViewer.jar index.vnc' >> classes/Makefile.am

cp classes/ssl/Makefile.am classes/ssl/Makefile.am.LibVNCServer
sed -e 's/EXTRA_DIST=/EXTRA_DIST=tightvnc-1.3dev7_javasrc-vncviewer-ssl.patch tightvnc-1.3dev7_javasrc-vncviewer-cursor-colors+no-tab-traversal.patch /' \
	classes/ssl/Makefile.am.LibVNCServer > classes/ssl/Makefile.am
echo 'pkgdatadir = $(datadir)/@PACKAGE@/classes/ssl' >> classes/ssl/Makefile.am
echo 'pkgdata_DATA=VncViewer.jar index.vnc SignedVncViewer.jar proxy.vnc README UltraViewerSSL.jar SignedUltraViewerSSL.jar ultra.vnc ultrasigned.vnc' >> classes/ssl/Makefile.am
echo 'pkgdata_SCRIPTS=ss_vncviewer' >> classes/ssl/Makefile.am

chmod 755 classes/ssl/ss_vncviewer

mv acinclude.m4 acinclude.m4.LibVNCServer

cat acinclude.m4.LibVNCServer | \
sed -e "s/^\(_PKG.*\)\$PACKAGE\(.*\)$/\1LibVNCServer\2/" \
> acinclude.m4

make x11vnc-${VERSION}.tar.gz
for f in configure.ac Makefile.am x11vnc/Makefile.am libvncserver/Makefile.am libvncclient/Makefile.am classes/Makefile.am classes/ssl/Makefile.am acinclude.m4 README; do
	mv -f $f.LibVNCServer $f
done
rm -f ./RELEASE-NOTES

