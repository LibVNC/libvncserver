# Process this file with autoconf to produce a configure script.
AC_INIT(LibVNCServer, 0.6pre, http://sourceforge.net/projects/libvncserver)
AM_INIT_AUTOMAKE(LibVNCServer, 0.6pre)
AM_CONFIG_HEADER([rfb/rfbconfig.h])

# Checks for programs.
AC_PROG_CC
if test -z "$CC"; then
	CCLD="\$(CC)"
else
	CCLD="$CC"
fi
AC_PROG_MAKE_SET
AC_PROG_RANLIB

# Options
AH_TEMPLATE(BACKCHANNEL, [Enable BackChannel communication])
AC_ARG_WITH(backchannel,
	[  --without-backchannel		disable backchannel method],
	,AC_DEFINE(BACKCHANNEL))
AH_TEMPLATE(ALLOW24BPP, [Enable 24 bit per pixel in native framebuffer])
AC_ARG_WITH(24bpp,
	[ --without-24bpp			disable 24 bpp framebuffers],
	,AC_DEFINE(ALLOW24BPP))

# Checks for X libraries
HAVE_X="false"
AC_PATH_XTRA
if test "$X_CFLAGS" != "-DX_DISPLAY_MISSING"; then
	AC_CHECK_LIB(Xtst, XTestFakeKeyEvent, HAVE_XTEST="true",
		HAVE_XTEST="false",
		$X_LIBS $X_PRELIBS -lX11 -lXext $X_EXTRA_LIBS)
	if test $HAVE_XTEST = "true"; then
		X_LIBS="$X_LIBS $X_PRELIBS -lXtst -lXext -lX11 $X_EXTRA_LIBS"
		HAVE_X="true"
	fi
fi
AC_SUBST(X_LIBS)
AM_CONDITIONAL(HAVE_X, test $HAVE_X != "false")

# Checks for libraries.
AC_CHECK_HEADER(jpeglib.h, HAVE_JPEGLIB_H="true")
AC_CHECK_HEADER(pthread.h, HAVE_PTHREAD_H="true")
AC_CHECK_HEADER(zlib.h, HAVE_ZLIB_H="true")
if test ! -z "$HAVE_JPEGLIB_H"; then
	AC_CHECK_LIB(jpeg, jpeg_CreateCompress)
fi
if test ! -z "$HAVE_PTHREAD_H"; then
	AC_CHECK_LIB(pthread, pthread_mutex_lock)
	AC_CHECK_LIB(pthread, pthread_mutex_lock, HAVE_LIBPTHREAD="true")
fi
AM_CONDITIONAL(HAVE_LIBPTHREAD, test ! -z "$HAVE_LIBPTHREAD")
AC_CHECK_PROGS(CXX,[g++ c++ gpp aCC CC cxx cc++ cl FCC KCC RCC xlC_r xlC],none)
AC_PROG_CXX
if test ! -z "$HAVE_ZLIB_H"; then
	AC_CHECK_LIB(z, deflate, , HAVE_ZLIB_H="")
	if test ! -z "$HAVE_ZLIB_H"; then
		# check for c++, but don't fail if not found
		AH_TEMPLATE(HAVE_ZRLE, [Use zrle compression; needs a c++ compiler])
		if test x$CXX != xnone; then
			AC_DEFINE(HAVE_ZRLE)
			if test -z "$CXX"; then
				CCLD="\$(CXX)"
			else
				CCLD="$CXX"
			fi
		fi
	fi
else
	CXX=none
fi
AM_CONDITIONAL(HAVE_CXX, test x$CXX != xnone -a ! -z "$HAVE_ZLIB_H")
AC_SUBST(CCLD)

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h stdlib.h string.h sys/socket.h sys/time.h sys/timeb.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_C_BIGENDIAN
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_HEADER_SYS_WAIT
AC_TYPE_SOCKLEN_T
AC_CREATE_STDINT_H(rfb/rfbint.h)

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_FUNC_VPRINTF
AC_CHECK_LIB(nsl,gethostbyname)
AC_CHECK_LIB(socket,socket)
AC_CHECK_FUNCS([ftime gethostbyname gethostname gettimeofday inet_ntoa memmove memset mkfifo select socket strchr strcspn strdup strerror strstr])

# Check if /dev/vcsa1 exists, if so, define LINUX
AM_CONDITIONAL(LINUX, test -c /dev/vcsa1)

# Check for OS X specific header
AC_CHECK_HEADER(ApplicationServices/ApplicationServices.h, HAVE_OSX="true")
AM_CONDITIONAL(OSX, test "$HAVE_OSX" = "true")

# Check for rpm SOURCES path
echo -n "checking for rpm sources path... "
RPMSOURCEDIR="NOT-FOUND"
for directory in packages OpenLinux redhat RedHat rpm RPM "" ; do
	if test -d /usr/src/${directory}/SOURCES; then
		RPMSOURCEDIR="/usr/src/${directory}/SOURCES/"
	fi
done
echo "$RPMSOURCEDIR"
AM_CONDITIONAL(HAVE_RPM, test "$RPMSOURCEDIR" != "NOT-FOUND")
AC_SUBST(RPMSOURCEDIR)

LDADD="-L.. -lvncserver"

AC_CONFIG_FILES([Makefile
                 contrib/Makefile
                 examples/Makefile
		 vncterm/Makefile
		 classes/Makefile
		 libvncclient/Makefile
		 test/Makefile
		 libvncserver.spec
		 libvncserver-config])
AC_OUTPUT
chmod a+x ./libvncserver-config

