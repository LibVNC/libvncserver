# Process this file with autoconf to produce a configure script.
AC_INIT(LibVNCServer, 0.5, http://sourceforge.net/projects/libvncserver)
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER([include/rfbconfig.h])

# Checks for programs.
AC_PROG_CC
CCLD=$CC
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
		X_LIBS="$X_LIBS $X_PRELIBS -lX11 $X_EXTRA_LIBS -lXext -lXtst"
		HAVE_X="true"
	fi
fi
AC_SUBST(X_LIBS)
AM_CONDITIONAL(HAVE_X, test $HAVE_X != "false")

# Checks for libraries.
AC_CHECK_HEADERS(jpeglib.h pthread.h zlib.h)
if test ! -z "$HAVE_JPEGLIB.H"; then
	AC_CHECK_LIB(jpeg, jpeg_CreateCompress)
fi
AM_CONDITIONAL(HAVE_LIBPTHREAD, test ! -z "$HAVE_PTHREAD.H")
if test ! -z "$HAVE_PTHREAD.H"; then
	AC_CHECK_LIB(pthread, pthread_mutex_lock)
fi
if test ! -z "$HAVE_ZLIB.H"; then
	AC_CHECK_LIB(z, deflate)
	# check for c++, but don't fail if not found
	AC_CHECK_PROGS(CXX,[g++ c++ gpp aCC CC cxx cc++ cl FCC KCC RCC xlC_r xlC],none)
	AH_TEMPLATE(HAVE_ZRLE, [Use zrle compression; needs a c++ compiler])
	if test x$CXX != xnone; then
		AC_DEFINE(HAVE_ZRLE)
		AC_PROG_CXX
		CCLD=$CXX
	fi
else
	CXX=none
fi
AM_CONDITIONAL(HAVE_CXX, test x$CXX != xnone)
AC_SUBST(CCLD)

VNCLIBS="-L.. -lvncserver"

LIBS="$VNCLIBS $LIBS"

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h stdlib.h string.h sys/socket.h sys/time.h sys/timeb.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_HEADER_TIME

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([ftime gethostbyname gethostname gettimeofday inet_ntoa memmove memset mkfifo select socket strchr strcspn strdup strerror strstr])

AC_CONFIG_FILES([Makefile
                 contrib/Makefile
                 examples/Makefile
		 libvncserver.spec])
AC_OUTPUT
