# Process this file with autoconf to produce a configure script.
AC_INIT(LibVNCServer, 0.9pre, http://sourceforge.net/projects/libvncserver)
AM_INIT_AUTOMAKE(LibVNCServer, 0.9pre)
AM_CONFIG_HEADER(rfbconfig.h)
AX_PREFIX_CONFIG_H([rfb/rfbconfig.h])

# Checks for programs.
AC_PROG_CC
if test -z "$CC"; then
	CCLD="\$(CC)"
else
	CCLD="$CC"
fi
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_PATH_PROG([AR], [ar], [/usr/bin/ar],
                  [$PATH:/usr/ccs/bin])

# Options
AH_TEMPLATE(WITH_TIGHTVNC_FILETRANSFER, [Disable TightVNCFileTransfer protocol])
AC_ARG_WITH(tightvnc-filetransfer,
	[  --without-filetransfer  disable TightVNC file transfer protocol],
	, [ with_tightvnc_filetransfer=yes ])
# AC_DEFINE moved to after libpthread check.

AH_TEMPLATE(ALLOW24BPP, [Enable 24 bit per pixel in native framebuffer])
AC_ARG_WITH(24bpp,
	[  --without-24bpp         disable 24 bpp framebuffers],
	, [ with_24bpp=yes ])
if test "x$with_24bpp" = "xyes"; then
	AC_DEFINE(ALLOW24BPP)
fi
AH_TEMPLATE(FFMPEG, [Use ffmpeg (for vnc2mpg)])
AC_ARG_WITH(ffmpeg,
	[  --with-ffmpeg=dir       set ffmpeg home directory],,)
AC_SUBST(with_ffmpeg)
AM_CONDITIONAL(WITH_FFMPEG, test ! -z "$with_ffmpeg")
if test ! -z "$with_ffmpeg"; then
	AC_CHECK_LIB(mp3lame, lame_init, HAVE_MP3LAME="true", HAVE_MP3LAME="false" )
fi
AM_CONDITIONAL(HAVE_MP3LAME, test "$HAVE_MP3LAME" = "true")

# Seem to need this dummy here to induce the 'checking for egrep... grep -E', etc.
# before it seemed to be inside the with_jpeg conditional. 
AC_CHECK_HEADER(thenonexistentheader.h, HAVE_THENONEXISTENTHEADER_H="true")

# Checks for X libraries
HAVE_X="false"
AC_PATH_XTRA
AH_TEMPLATE(HAVE_XSHM, [MIT-SHM extension build environment present])
AH_TEMPLATE(HAVE_XTEST, [XTEST extension build environment present])
AH_TEMPLATE(HAVE_XTESTGRABCONTROL, [XTEST extension has XTestGrabControl])
AH_TEMPLATE(HAVE_XKEYBOARD, [XKEYBOARD extension build environment present])
AH_TEMPLATE(HAVE_LIBXINERAMA, [XINERAMA extension build environment present])
AH_TEMPLATE(HAVE_LIBXRANDR, [XRANDR extension build environment present])
AH_TEMPLATE(HAVE_LIBXFIXES, [XFIXES extension build environment present])
AH_TEMPLATE(HAVE_LIBXDAMAGE, [XDAMAGE extension build environment present])
AH_TEMPLATE(HAVE_LIBXTRAP, [DEC-XTRAP extension build environment present])
AH_TEMPLATE(HAVE_RECORD, [RECORD extension build environment present])
AH_TEMPLATE(HAVE_SOLARIS_XREADSCREEN, [Solaris XReadScreen available])
AH_TEMPLATE(HAVE_IRIX_XREADDISPLAY, [IRIX XReadDisplay available])
AH_TEMPLATE(HAVE_FBPM, [FBPM extension build environment present])
AH_TEMPLATE(HAVE_LINUX_VIDEODEV_H, [video4linux build environment present])
AH_TEMPLATE(HAVE_LINUX_FB_H, [linux fb device build environment present])

AC_ARG_WITH(xkeyboard,
[  --without-xkeyboard     disable xkeyboard extension support],,)
AC_ARG_WITH(xinerama,
[  --without-xinerama      disable xinerama extension support],,)
AC_ARG_WITH(xrandr,
[  --without-xrandr        disable xrandr extension support],,)
AC_ARG_WITH(xfixes,
[  --without-xfixes        disable xfixes extension support],,)
AC_ARG_WITH(xdamage,
[  --without-xdamage       disable xdamage extension support],,)
AC_ARG_WITH(xtrap,
[  --without-xtrap         disable xtrap extension support],,)
AC_ARG_WITH(xrecord,
[  --without-xrecord       disable xrecord extension support],,)
AC_ARG_WITH(fbpm,
[  --without-fbpm          disable fbpm extension support],,)
AC_ARG_WITH(v4l,
[  --without-v4l           disable video4linux support],,)
AC_ARG_WITH(fbdev,
[  --without-fbdev         disable linux fb device support],,)

if test "$X_CFLAGS" != "-DX_DISPLAY_MISSING"; then
	AC_CHECK_LIB(X11, XGetImage, HAVE_X="true",
		HAVE_X="false",
		$X_LIBS $X_PRELIBS -lX11 $X_EXTRA_LIBS)

	if test $HAVE_X = "true"; then
		X_PRELIBS="$X_PRELIBS -lXext"

		AC_CHECK_LIB(Xext, XShmGetImage, 
			[AC_DEFINE(HAVE_XSHM)], ,
			$X_LIBS $X_PRELIBS -lX11 $X_EXTRA_LIBS)

		AC_CHECK_LIB(Xext, XReadScreen, 
			[AC_DEFINE(HAVE_SOLARIS_XREADSCREEN)], ,
			$X_LIBS $X_PRELIBS -lX11 $X_EXTRA_LIBS)

		AC_CHECK_HEADER(X11/extensions/readdisplay.h,
			[AC_DEFINE(HAVE_IRIX_XREADDISPLAY)], ,
			[#include <X11/Xlib.h>])

		AC_CHECK_HEADER(X11/extensions/fbpm.h,
			[AC_DEFINE(HAVE_FBPM)], ,
			[#include <X11/Xlib.h>
#include <X11/Xmd.h>])

		AC_CHECK_LIB(Xtst, XTestGrabControl, 
			X_PRELIBS="-lXtst $X_PRELIBS"
			[AC_DEFINE(HAVE_XTESTGRABCONTROL) HAVE_XTESTGRABCONTROL="true"], ,
			$X_LIBS $X_PRELIBS -lX11 $X_EXTRA_LIBS)

		AC_CHECK_LIB(Xtst, XTestFakeKeyEvent, 
			X_PRELIBS="-lXtst $X_PRELIBS"
			[AC_DEFINE(HAVE_XTEST)], ,
			$X_LIBS $X_PRELIBS -lX11 $X_EXTRA_LIBS)

		if test "x$with_xrecord" != "xno"; then
			AC_CHECK_LIB(Xtst, XRecordEnableContextAsync, 
				X_PRELIBS="-lXtst $X_PRELIBS"
				[AC_DEFINE(HAVE_RECORD)], ,
				$X_LIBS $X_PRELIBS -lX11 $X_EXTRA_LIBS)
		fi

		# we use XTRAP on X11R5, or user can set X11VNC_USE_XTRAP
		if test "x$with_xtrap" != "xno"; then
		    if test ! -z "$X11VNC_USE_XTRAP" -o -z "$HAVE_XTESTGRABCONTROL"; then
			AC_CHECK_LIB(XTrap, XETrapSetGrabServer, 
				X_PRELIBS="$X_PRELIBS -lXTrap"
				[AC_DEFINE(HAVE_LIBXTRAP)], ,
				$X_LIBS $X_PRELIBS -lX11 $X_EXTRA_LIBS)
			# tru64 uses libXETrap.so
			AC_CHECK_LIB(XETrap, XETrapSetGrabServer, 
				X_PRELIBS="$X_PRELIBS -lXETrap"
				[AC_DEFINE(HAVE_LIBXTRAP)], ,
				$X_LIBS $X_PRELIBS -lX11 $X_EXTRA_LIBS)
		    fi
		fi

		if test "x$with_xkeyboard" != "xno"; then
			saved_CPPFLAGS="$CPPFLAGS"
			CPPFLAGS="$CPPFLAGS $X_CFLAGS"
			AC_CHECK_HEADER(X11/XKBlib.h, HAVE_XKBLIB_H="true",
				HAVE_XKBLIB_H="false", [#include <X11/Xlib.h>])
			CPPFLAGS="$saved_CPPFLAGS"
			if test $HAVE_XKBLIB_H = "true"; then
				AC_CHECK_LIB(X11, XkbSelectEvents,
					[AC_DEFINE(HAVE_XKEYBOARD)], ,
					$X_LIBS $X_PRELIBS -lX11 $X_EXTRA_LIBS)
			fi
		fi

		if test "x$with_xinerama" != "xno"; then
			AC_CHECK_LIB(Xinerama, XineramaQueryScreens,
				X_PRELIBS="$X_PRELIBS -lXinerama"
				[AC_DEFINE(HAVE_LIBXINERAMA)], ,
				$X_LIBS $X_PRELIBS -lX11 $X_EXTRA_LIBS)
		fi

		if test "x$with_xrandr" != "xno"; then
			AC_CHECK_LIB(Xrandr, XRRSelectInput,
				X_PRELIBS="$X_PRELIBS -lXrandr"
				[AC_DEFINE(HAVE_LIBXRANDR)], ,
				$X_LIBS $X_PRELIBS -lX11 $X_EXTRA_LIBS)
		fi

		if test "x$with_xfixes" != "xno"; then
			AC_CHECK_LIB(Xfixes, XFixesGetCursorImage,
				X_PRELIBS="$X_PRELIBS -lXfixes"
				[AC_DEFINE(HAVE_LIBXFIXES) HAVE_LIBXFIXES="true"], ,
				$X_LIBS $X_PRELIBS -lX11 $X_EXTRA_LIBS)
		fi

		if test "x$with_xdamage" != "xno"; then
			AC_CHECK_LIB(Xdamage, XDamageQueryExtension,
				X_PRELIBS="$X_PRELIBS -lXdamage"
				[AC_DEFINE(HAVE_LIBXDAMAGE) HAVE_LIBXDAMAGE="true"], ,
				$X_LIBS $X_PRELIBS -lX11 $X_EXTRA_LIBS)
		fi

		if test ! -z "$HAVE_LIBXFIXES" -o ! -z "$HAVE_LIBXDAMAGE"; then
			# need /usr/sfw/lib in RPATH for Solaris 10 and later
			case `(uname -sr) 2>/dev/null` in
			"SunOS 5"*) X_EXTRA_LIBS="$X_EXTRA_LIBS -R/usr/sfw/lib" ;;
			esac
		fi

		# currently only x11vnc uses crypt() or libssl:
		AH_TEMPLATE(HAVE_LIBCRYPT, [libcrypt library present])
		AC_ARG_WITH(crypt,
		[  --without-crypt         disable support for libcrypt],,)
		if test "x$with_crypt" != "xno"; then
			AC_CHECK_FUNCS([crypt], HAVE_LIBC_CRYPT="true")
			if test -z "$HAVE_LIBC_CRYPT"; then
				AC_CHECK_LIB(crypt, crypt,
					X_PRELIBS="$X_PRELIBS -lcrypt"
					[AC_DEFINE(HAVE_LIBCRYPT)], ,)
			fi
		fi

		# some OS's need both -lssl and -lcrypto on link line:
		AH_TEMPLATE(HAVE_LIBCRYPTO, [openssl libcrypto library present])
		AC_ARG_WITH(crypto,
		[  --without-crypto        disable support for openssl libcrypto],,)

		AH_TEMPLATE(HAVE_LIBSSL, [openssl libssl library present])
		AC_ARG_WITH(ssl,
		[  --without-ssl           disable support for openssl libssl],,)

		if test "x$with_crypto" != "xno" -a "x$with_ssl" != "xno"; then
			AC_CHECK_LIB(crypto, RAND_file_name,
				[AC_DEFINE(HAVE_LIBCRYPTO) HAVE_LIBCRYPTO="true"], ,)
		fi

		if test "x$with_ssl" != "xno"; then
			if test "x$HAVE_LIBCRYPTO" = "xtrue"; then
				AC_CHECK_LIB(ssl, SSL_library_init,
					X_PRELIBS="$X_PRELIBS -lssl -lcrypto"
					[AC_DEFINE(HAVE_LIBSSL) HAVE_LIBSSL="true"], ,
					-lcrypto)
			else
				AC_CHECK_LIB(ssl, SSL_library_init,
					X_PRELIBS="$X_PRELIBS -lssl"
					[AC_DEFINE(HAVE_LIBSSL) HAVE_LIBSSL="true"], ,)
			fi
		fi

		if test "x$with_v4l" != "xno"; then
			AC_CHECK_HEADER(linux/videodev.h,
				[AC_DEFINE(HAVE_LINUX_VIDEODEV_H)],,)
		fi
		if test "x$with_fbdev" != "xno"; then
			AC_CHECK_HEADER(linux/fb.h,
				[AC_DEFINE(HAVE_LINUX_FB_H)],,)
		fi

		X_LIBS="$X_LIBS $X_PRELIBS -lX11 $X_EXTRA_LIBS"
	fi
fi
AC_SUBST(X_LIBS)
AM_CONDITIONAL(HAVE_X, test $HAVE_X != "false")

if test "$PACKAGE_NAME" = "x11vnc" -a "x$HAVE_X" = "xfalse"; then
	AC_MSG_ERROR([
==========================================================================
A working X window system build environment is required to build x11vnc.
Make sure any required X development packages are installed.  If they are
installed in non-standard locations, one can use the --x-includes=DIR
and --x-libraries=DIR configure options or set the CPPFLAGS and LDFLAGS
environment variables to indicate where the X window system header files
and libraries may be found.  On 64+32 bit machines you may need to point
to lib64 or lib32 directories to pick up the correct word size.
==========================================================================
])
fi

# Checks for libraries.

AC_ARG_WITH(jpeg,
[  --without-jpeg          disable support for jpeg]
[  --with-jpeg=DIR         use jpeg include/library files in DIR],,)

# At this point:
#	no jpeg on command line   with_jpeg=""
#	-with-jpeg                with_jpeg="yes"
#	-without-jpeg             with_jpeg="no"
#	-with-jpeg=/foo/dir       with_jpeg="/foo/dir"

if test "x$with_jpeg" != "xno"; then
	if test ! -z "$with_jpeg" -a "x$with_jpeg" != "xyes"; then
		# add user supplied directory to flags:
		saved_CPPFLAGS="$CPPFLAGS"
		saved_LDFLAGS="$LDFLAGS"
		CPPFLAGS="$CPPFLAGS -I$with_jpeg/include"
		LDFLAGS="$LDFLAGS -L$with_jpeg/lib"
		uname_s=`(uname -s) 2>/dev/null`
		if test "x$uname_s" = "xHP-UX"; then
			:
		elif test "x$uname_s" = "xOSF1"; then
			:
		elif test "x$GCC" = "xyes"; then
			# this is not complete... in general a rat's nest.
			LDFLAGS="$LDFLAGS -Xlinker -R$with_jpeg/lib"
		else
			LDFLAGS="$LDFLAGS -R$with_jpeg/lib"
		fi
	fi
	AC_CHECK_HEADER(jpeglib.h, HAVE_JPEGLIB_H="true")
	if test "x$HAVE_JPEGLIB_H" = "xtrue"; then
		AC_CHECK_LIB(jpeg, jpeg_CreateCompress, , HAVE_JPEGLIB_H="")
	fi
	if test ! -z "$with_jpeg" -a "x$with_jpeg" != "xyes"; then
		if test "x$HAVE_JPEGLIB_H" != "xtrue"; then
			# restore old flags on failure:
			CPPFLAGS="$saved_CPPFLAGS"
			LDFLAGS="$saved_LDFLAGS"
		fi
	fi
	if test "$PACKAGE_NAME" = "x11vnc"; then
		if test "x$HAVE_JPEGLIB_H" != "xtrue"; then
			AC_MSG_WARN([
==========================================================================
The libjpeg compression library was not found: this may lead to reduced
performance, especially over slow links.  If libjpeg is in a non-standard
location use --with-jpeg=DIR to indicate the header file is in
DIR/include/jpeglib.h and the library in DIR/lib/libjpeg.a.  A copy of
libjpeg may be obtained from: ftp://ftp.uu.net/graphics/jpeg/
==========================================================================
])
		fi
	fi
fi

AC_ARG_WITH(libz,
[  --without-libz          disable support for deflate],,)
AC_ARG_WITH(zlib,
[  --without-zlib          disable support for deflate]
[  --with-zlib=DIR         use zlib include/library files in DIR],,)

if test "x$with_zlib" != "xno" -a "x$with_libz" != "xno"; then
	if test ! -z "$with_zlib" -a "x$with_zlib" != "xyes"; then
		saved_CPPFLAGS="$CPPFLAGS"
		saved_LDFLAGS="$LDFLAGS"
		CPPFLAGS="$CPPFLAGS -I$with_zlib/include"
		LDFLAGS="$LDFLAGS -L$with_zlib/lib"
		uname_s=`(uname -s) 2>/dev/null`
		if test "x$uname_s" = "xHP-UX"; then
			:
		elif test "x$uname_s" = "xOSF1"; then
			:
		elif test "x$GCC" = "xyes"; then
			LDFLAGS="$LDFLAGS -Xlinker -R$with_zlib/lib"
		else
			LDFLAGS="$LDFLAGS -R$with_zlib/lib"
		fi
	fi
	AC_CHECK_HEADER(zlib.h, HAVE_ZLIB_H="true")
	if test "x$HAVE_ZLIB_H" = "xtrue"; then
		AC_CHECK_LIB(z, deflate, , HAVE_ZLIB_H="")
	fi
	if test ! -z "$with_zlib" -a "x$with_zlib" != "xyes"; then
		if test "x$HAVE_ZLIB_H" != "xtrue"; then
			CPPFLAGS="$saved_CPPFLAGS"
			LDFLAGS="$saved_LDFLAGS"
		fi
	fi
	if test "$PACKAGE_NAME" = "x11vnc"; then
		if test "x$HAVE_ZLIB_H" != "xtrue"; then
			AC_MSG_WARN([
==========================================================================
The libz compression library was not found: this may lead to reduced
performance, especially over slow links.  If libz is in a non-standard
location use --with-zlib=DIR to indicate the header file is in
DIR/include/zlib.h and the library in DIR/lib/libz.a.  A copy of
libz may be obtained from: http://www.gzip.org/zlib/
==========================================================================
])
		fi
	fi
fi

AC_ARG_WITH(pthread,
[  --without-pthread       disable support for libpthread],,)

if test "x$with_pthread" != "xno"; then
	AC_CHECK_HEADER(pthread.h, HAVE_PTHREAD_H="true")
	if test ! -z "$HAVE_PTHREAD_H"; then
		AC_CHECK_LIB(pthread, pthread_mutex_lock)
		AC_CHECK_LIB(pthread, pthread_mutex_lock, HAVE_LIBPTHREAD="true")
	fi
fi
AM_CONDITIONAL(HAVE_LIBPTHREAD, test ! -z "$HAVE_LIBPTHREAD")

# tightvnc-filetransfer implemented using threads:
if test -z "$HAVE_LIBPTHREAD"; then
	with_tightvnc_filetransfer=""
fi
if test "x$with_tightvnc_filetransfer" = "xyes"; then
	AC_DEFINE(WITH_TIGHTVNC_FILETRANSFER)
fi
AM_CONDITIONAL(WITH_TIGHTVNC_FILETRANSFER, test "$with_tightvnc_filetransfer" = "yes")

AM_CONDITIONAL(HAVE_LIBZ, test ! -z "$HAVE_ZLIB_H")
AM_CONDITIONAL(HAVE_LIBJPEG, test ! -z "$HAVE_JPEGLIB_H")

if test -z "$with_sdl"; then
	if sdl-config --version >/dev/null 2>&1; then
		with_sdl=yes
		SDL_CFLAGS=`sdl-config --cflags`
		SDL_LIBS=`sdl-config --libs`
	else
		with_sdl=no
	fi
fi
AM_CONDITIONAL(HAVE_LIBSDL, test "x$with_sdl" = "xyes")
AC_SUBST(SDL_CFLAGS)
AC_SUBST(SDL_LIBS)

MINGW=`uname -s | grep MINGW 2>/dev/null`
AM_CONDITIONAL(MINGW, test ! -z "$MINGW" )
if test ! -z "$MINGW"; then
	WSOCKLIB="-lws2_32"
fi
AC_SUBST(WSOCKLIB)

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h stdlib.h string.h sys/socket.h sys/time.h sys/timeb.h syslog.h unistd.h pwd.h sys/wait.h utmpx.h termios.h sys/ioctl.h sys/stropts.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_C_BIGENDIAN
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_HEADER_SYS_WAIT
AC_TYPE_SOCKLEN_T
if test ! -d ./rfb; then
	echo "creating subdir ./rfb for rfbint.h"
	mkdir ./rfb 
fi
AC_CREATE_STDINT_H(rfb/rfbint.h)
AC_CACHE_CHECK([for in_addr_t],
        inaddrt, [
                AC_TRY_COMPILE([#include <sys/types.h>
#include <netinet/in.h>],
                        [in_addr_t foo; return 0;],
                                        [inaddrt=yes],
                                        [inaddrt=no]),
        ])
AH_TEMPLATE(NEED_INADDR_T, [Need a typedef for in_addr_t])
if test $inaddrt = no ; then
  AC_DEFINE(NEED_INADDR_T)
fi

# Checks for library functions.
AC_FUNC_MEMCMP
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_FUNC_VPRINTF
AC_FUNC_FORK
AC_CHECK_LIB(nsl,gethostbyname)
AC_CHECK_LIB(socket,socket)

uname_s=`(uname -s) 2>/dev/null`
if test "x$uname_s" = "xHP-UX"; then
	# need -lsec for getspnam()
	LDFLAGS="$LDFLAGS -lsec"
fi

AC_CHECK_FUNCS([ftime gethostbyname gethostname gettimeofday inet_ntoa memmove memset mmap mkfifo select socket strchr strcspn strdup strerror strstr setsid setpgrp getpwuid getpwnam getspnam getuid geteuid setuid setgid seteuid setegid waitpid setutxent grantpt])

# check, if shmget is in cygipc.a
AC_CHECK_LIB(cygipc,shmget)
AM_CONDITIONAL(CYGIPC, test "$HAVE_CYGIPC" = "true")

# Check if /dev/vcsa1 exists, if so, define LINUX
AM_CONDITIONAL(LINUX, test -c /dev/vcsa1)

# Check for OS X specific header
AC_CHECK_HEADER(ApplicationServices/ApplicationServices.h, HAVE_OSX="true")
AM_CONDITIONAL(OSX, test "$HAVE_OSX" = "true")

# On Solaris 2.7, write() returns ENOENT when it really means EAGAIN
AH_TEMPLATE(ENOENT_WORKAROUND, [work around when write() returns ENOENT but does not mean it])
case `(uname -sr) 2>/dev/null` in
    "SunOS 5.7")
	AC_DEFINE(ENOENT_WORKAROUND)
    ;;
esac

# Check for rpm SOURCES path
printf "checking for rpm sources path... "
RPMSOURCEDIR="NOT-FOUND"
for directory in packages OpenLinux redhat RedHat rpm RPM "" ; do
	if test -d /usr/src/${directory}/SOURCES; then
		RPMSOURCEDIR="/usr/src/${directory}/SOURCES/"
	fi
done
echo "$RPMSOURCEDIR"
AM_CONDITIONAL(HAVE_RPM, test "$RPMSOURCEDIR" != "NOT-FOUND")
AC_SUBST(RPMSOURCEDIR)

AC_CONFIG_FILES([Makefile
                 libvncserver/Makefile
                 contrib/Makefile
                 x11vnc/Makefile
                 x11vnc/misc/Makefile
                 examples/Makefile
		 vncterm/Makefile
		 classes/Makefile
		 classes/ssl/Makefile
		 libvncclient/Makefile
		 client_examples/Makefile
		 test/Makefile
		 LibVNCServer.spec
		 libvncserver-config])
AC_CONFIG_COMMANDS([chmod-libvncserver-config],[chmod a+x libvncserver-config])
AC_OUTPUT
chmod a+x ./libvncserver-config

