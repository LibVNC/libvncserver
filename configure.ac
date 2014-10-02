# Process this file with autoconf to produce a configure script.
AC_INIT(LibVNCServer, 0.9.10, https://github.com/LibVNC/libvncserver)
AM_INIT_AUTOMAKE([subdir-objects])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AM_CONFIG_HEADER(rfbconfig.h)
AX_PREFIX_CONFIG_H([rfb/rfbconfig.h])
AC_CONFIG_MACRO_DIR([m4])


# set detailed version info
AC_DEFINE(VERSION_MAJOR, 0, LibVNCServer major version)
AC_DEFINE(VERSION_MINOR, 9, LibVNCServer minor version)
AC_DEFINE(VERSION_PATCHLEVEL, 10, LibVNCServer patchlevel)

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
if test -z "$CC"; then
	CCLD="\$(CC)"
else
	CCLD="$CC"
fi
test "x$GCC" = "xyes" && CFLAGS="$CFLAGS -Wall"
AC_PROG_MAKE_SET
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
AC_PATH_PROG([AR], [ar], [/usr/bin/ar],
                  [$PATH:/usr/ccs/bin])

# Options
AH_TEMPLATE(WITH_TIGHTVNC_FILETRANSFER, [Disable TightVNCFileTransfer protocol])
AC_ARG_WITH(tightvnc-filetransfer,
	[  --without-filetransfer  disable TightVNC file transfer protocol],
	, [ with_tightvnc_filetransfer=yes ])
# AC_DEFINE moved to after libpthread check.

# WebSockets support
AC_CHECK_FUNC(__b64_ntop, HAVE_B64_IN_LIBC="true", HAVE_B64_IN_LIBC="false")
if test "x$HAVE_B64_IN_LIBC" != "xtrue"; then
	AC_CHECK_LIB(resolv, __b64_ntop, HAVE_B64_IN_LIBRESOLV="true", HAVE_B64_IN_LIBRESOLV="false")
	if test "x$HAVE_B64_IN_LIBRESOLV" = "xtrue"; then
		RESOLV_LIB="-lresolv"
		HAVE_B64="true"
	fi
else
	HAVE_B64="true"
fi
AH_TEMPLATE(WITH_WEBSOCKETS, [Disable WebSockets support])
AC_ARG_WITH(websockets,
	[  --without-websockets    disable WebSockets support],
	, [ with_websockets=yes ])
# AC_DEFINE moved to after libresolve check.

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

# set some ld -R nonsense
#
uname_s=`(uname -s) 2>/dev/null`
ld_minus_R="yes"
if test "x$uname_s" = "xHP-UX"; then
	ld_minus_R="no"
elif test "x$uname_s" = "xOSF1"; then
	ld_minus_R="no"
elif test "x$uname_s" = "xDarwin"; then
	ld_minus_R="no"
fi

# Check for OpenSSL
AH_TEMPLATE(HAVE_LIBCRYPT, [libcrypt library present])
AC_ARG_WITH(crypt,
[  --without-crypt         disable support for libcrypt],,)
if test "x$with_crypt" != "xno"; then
	AC_CHECK_FUNCS([crypt], HAVE_LIBC_CRYPT="true")
	if test -z "$HAVE_LIBC_CRYPT"; then
		AC_CHECK_LIB(crypt, crypt,
			CRYPT_LIBS="-lcrypt"
			[AC_DEFINE(HAVE_LIBCRYPT)], ,)
	fi
fi
AC_SUBST(CRYPT_LIBS)

# some OS's need both -lssl and -lcrypto on link line:
AH_TEMPLATE(HAVE_LIBCRYPTO, [openssl libcrypto library present])
AC_ARG_WITH(crypto,
[  --without-crypto        disable support for openssl libcrypto],,)

AH_TEMPLATE(HAVE_LIBSSL, [openssl libssl library present])
AC_ARG_WITH(ssl,
[  --without-ssl           disable support for openssl libssl]
[  --with-ssl=DIR          use openssl include/library files in DIR],,)

if test "x$with_crypto" != "xno" -a "x$with_ssl" != "xno"; then
	if test ! -z "$with_ssl" -a "x$with_ssl" != "xyes"; then
		saved_CPPFLAGS="$CPPFLAGS"
		saved_LDFLAGS="$LDFLAGS"
		CPPFLAGS="$CPPFLAGS -I$with_ssl/include"
		LDFLAGS="$LDFLAGS -L$with_ssl/lib"
		if test "x$ld_minus_R" = "xno"; then
			:
		elif test "x$GCC" = "xyes"; then
			LDFLAGS="$LDFLAGS -Xlinker -R$with_ssl/lib"
		else
			LDFLAGS="$LDFLAGS -R$with_ssl/lib"
		fi
	fi
	AC_CHECK_LIB(crypto, RAND_file_name,
		[AC_DEFINE(HAVE_LIBCRYPTO) HAVE_LIBCRYPTO="true"], ,)
	if test ! -z "$with_ssl" -a "x$with_ssl" != "xyes"; then
		if test "x$HAVE_LIBCRYPTO" != "xtrue"; then
			CPPFLAGS="$saved_CPPFLAGS"
			LDFLAGS="$saved_LDFLAGS"
		fi
	fi
fi

AH_TEMPLATE(HAVE_X509_PRINT_EX_FP, [open ssl X509_print_ex_fp available])
if test "x$with_ssl" != "xno"; then
	if test "x$HAVE_LIBCRYPTO" = "xtrue"; then
		AC_CHECK_LIB(ssl, SSL_library_init,
			SSL_LIBS="-lssl -lcrypto"
			[AC_DEFINE(HAVE_LIBSSL) HAVE_LIBSSL="true"], ,
			-lcrypto)
	else
		AC_CHECK_LIB(ssl, SSL_library_init,
			SSL_LIBS="-lssl"
			[AC_DEFINE(HAVE_LIBSSL) HAVE_LIBSSL="true"], ,)
	fi
fi
AC_SUBST(SSL_LIBS)
AM_CONDITIONAL(HAVE_LIBSSL, test ! -z "$SSL_LIBS")


# See if we want libva support
# TODO: check if library actually exists
AH_TEMPLATE(CONFIG_LIBVA, [Build libva support])
AC_ARG_WITH(libva,
[  --with-libva                   build libva support],,)
if test "x$with_libva" != "xno"; then
    AC_CHECK_LIB(va, vaInitialize,
        VA_LIBS="-lva -lva-x11"
        [AC_DEFINE(CONFIG_LIBVA) CONFIG_LIBVA="true"], ,)
fi
AC_SUBST(VA_LIBS)
AM_CONDITIONAL(CONFIG_LIBVA, test ! -z "$VA_LIBS")



AC_ARG_WITH(jpeg,
[  --without-jpeg          disable support for jpeg]
[  --with-jpeg=DIR         use jpeg include/library files in DIR],,)

# At this point:
#	no jpeg on command line   with_jpeg=""
#	-with-jpeg                with_jpeg="yes"
#	-without-jpeg             with_jpeg="no"
#	-with-jpeg=/foo/dir       with_jpeg="/foo/dir"

HAVE_LIBJPEG_TURBO="false"

if test "x$with_jpeg" != "xno"; then
	AC_ARG_VAR(JPEG_LDFLAGS,
		[Linker flags to use when linking with libjpeg, e.g. -L/foo/dir/lib -Wl,-static -ljpeg -Wl,-shared.  This overrides the linker flags set by --with-jpeg.])
	saved_CPPFLAGS="$CPPFLAGS"
	saved_LDFLAGS="$LDFLAGS"
	saved_LIBS="$LIBS"
	if test ! -z "$with_jpeg" -a "x$with_jpeg" != "xyes"; then
		# add user supplied directory to flags:
		CPPFLAGS="$CPPFLAGS -I$with_jpeg/include"
		LDFLAGS="$LDFLAGS -L$with_jpeg/lib"
		if test "x$ld_minus_R" = "xno"; then
			:
		elif test "x$GCC" = "xyes"; then
			# this is not complete... in general a rat's nest.
			LDFLAGS="$LDFLAGS -Xlinker -R$with_jpeg/lib"
		else
			LDFLAGS="$LDFLAGS -R$with_jpeg/lib"
		fi
	fi
	if test "x$JPEG_LDFLAGS" != "x"; then
		LDFLAGS="$saved_LDFLAGS"
		LIBS="$LIBS $JPEG_LDFLAGS"
	else
		LIBS="-ljpeg"
	fi
	AC_CHECK_HEADER(jpeglib.h, HAVE_JPEGLIB_H="true")
	AC_MSG_CHECKING(for jpeg_CreateCompress in libjpeg)
	if test "x$HAVE_JPEGLIB_H" = "xtrue"; then
		AC_LINK_IFELSE([AC_LANG_CALL([], [jpeg_CreateCompress])],
			[AC_MSG_RESULT(yes);
				AC_DEFINE(HAVE_LIBJPEG, 1, libjpeg support enabled)],
			[AC_MSG_RESULT(no); HAVE_JPEGLIB_H=""])
	fi
	if test "x$HAVE_JPEGLIB_H" != "xtrue"; then
			# restore old flags on failure:
			CPPFLAGS="$saved_CPPFLAGS"
			LDFLAGS="$saved_LDFLAGS"
			LIBS="$saved_LIBS"
			AC_MSG_WARN([
==========================================================================
*** The libjpeg compression library was not found. ***
This may lead to reduced performance, especially over slow links.
If libjpeg is in a non-standard location use --with-jpeg=DIR to
indicate the header file is in DIR/include/jpeglib.h and the library
in DIR/lib/libjpeg.a.  You can also set the JPEG_LDFLAGS variable to
specify more detailed linker flags.  A copy of libjpeg-turbo may be
obtained from:  https://sourceforge.net/projects/libjpeg-turbo/files/
A copy of libjpeg may be obtained from:  http://ijg.org/files/
==========================================================================
])
			sleep 5
		fi

	if test "x$HAVE_JPEGLIB_H" = "xtrue"; then
		AC_MSG_CHECKING(whether JPEG library is libjpeg-turbo)
		m4_define([LJT_TEST],
			[AC_LANG_PROGRAM([#include <stdio.h>
				#include <jpeglib.h>],
			[struct jpeg_compress_struct cinfo;
				struct jpeg_error_mgr jerr;
				cinfo.err=jpeg_std_error(&jerr);
				jpeg_create_compress(&cinfo);
				cinfo.input_components = 3;
				jpeg_set_defaults(&cinfo);
				cinfo.in_color_space = JCS_EXT_RGB;
				jpeg_default_colorspace(&cinfo);
				return 0;])]
		)
		if test "x$cross_compiling" != "xyes"; then
			AC_RUN_IFELSE([LJT_TEST],
				[HAVE_LIBJPEG_TURBO="true"; AC_MSG_RESULT(yes)],
				[AC_MSG_RESULT(no)])
		else
			AC_LINK_IFELSE([LJT_TEST],
				[HAVE_LIBJPEG_TURBO="true"; AC_MSG_RESULT(yes)],
				[AC_MSG_RESULT(no)])
		fi
	fi

	if test "x$HAVE_JPEGLIB_H" = "xtrue" -a "x$HAVE_LIBJPEG_TURBO" != "xtrue"; then
		AC_MSG_WARN([
==========================================================================
*** The libjpeg library you are building against is not libjpeg-turbo.
Performance will be reduced.  You can obtain libjpeg-turbo from:
https://sourceforge.net/projects/libjpeg-turbo/files/ ***
==========================================================================
])
  fi

fi

AC_ARG_WITH(png,
[  --without-png           disable support for png]
[  --with-png=DIR          use png include/library files in DIR],,)

# At this point:
#	no png on command line    with_png=""
#	-with-png                 with_png="yes"
#	-without-png              with_png="no"
#	-with-png=/foo/dir        with_png="/foo/dir"

if test "x$with_png" != "xno"; then
	if test ! -z "$with_png" -a "x$with_png" != "xyes"; then
		# add user supplied directory to flags:
		saved_CPPFLAGS="$CPPFLAGS"
		saved_LDFLAGS="$LDFLAGS"
		CPPFLAGS="$CPPFLAGS -I$with_png/include"
		LDFLAGS="$LDFLAGS -L$with_png/lib"
		if test "x$ld_minus_R" = "xno"; then
			:
		elif test "x$GCC" = "xyes"; then
			# this is not complete... in general a rat's nest.
			LDFLAGS="$LDFLAGS -Xlinker -R$with_png/lib"
		else
			LDFLAGS="$LDFLAGS -R$with_png/lib"
		fi
	fi
	AC_CHECK_HEADER(png.h, HAVE_PNGLIB_H="true")
	if test "x$HAVE_PNGLIB_H" = "xtrue"; then
		AC_CHECK_LIB(png, png_create_write_struct, , HAVE_PNGLIB_H="")
	fi
	if test ! -z "$with_png" -a "x$with_png" != "xyes"; then
		if test "x$HAVE_PNGLIB_H" != "xtrue"; then
			# restore old flags on failure:
			CPPFLAGS="$saved_CPPFLAGS"
			LDFLAGS="$saved_LDFLAGS"
		fi
	fi
		if test "x$HAVE_PNGLIB_H" != "xtrue"; then
			AC_MSG_WARN([
==========================================================================
*** The libpng compression library was not found. ***
This may lead to reduced performance, especially over slow links.
If libpng is in a non-standard location use --with-png=DIR to
indicate the header file is in DIR/include/png.h and the library
in DIR/lib/libpng.a.  A copy of libpng may be obtained from:
http://www.libpng.org/pub/png/libpng.html
==========================================================================
])
			sleep 5
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
		if test "x$ld_minus_R" = "xno"; then
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
		if test "x$HAVE_ZLIB_H" != "xtrue"; then
			AC_MSG_WARN([
==========================================================================
*** The libz compression library was not found. ***
This may lead to reduced performance, especially over slow links.
If libz is in a non-standard location use --with-zlib=DIR to indicate the
header file is in DIR/include/zlib.h and the library in DIR/lib/libz.a.
A copy of libz may be obtained from: http://www.gzip.org/zlib/
==========================================================================
])
			sleep 5
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

AC_MSG_CHECKING([for __thread])
AC_LINK_IFELSE([AC_LANG_PROGRAM(, [static __thread int p = 0])],
	       [AC_DEFINE(HAVE_TLS, 1,
		      Define to 1 if compiler supports __thread)
		AC_MSG_RESULT([yes])],
	       [AC_MSG_RESULT([no])])

# tightvnc-filetransfer implemented using threads:
if test -z "$HAVE_LIBPTHREAD"; then
	with_tightvnc_filetransfer=""
fi
if test "x$with_tightvnc_filetransfer" = "xyes"; then
	AC_DEFINE(WITH_TIGHTVNC_FILETRANSFER)
fi
AM_CONDITIONAL(WITH_TIGHTVNC_FILETRANSFER, test "$with_tightvnc_filetransfer" = "yes")

# websockets implemented using base64 from resolve
if test "x$HAVE_B64" != "xtrue"; then
	with_websockets=""
fi
if test "x$with_websockets" = "xyes"; then
	LIBS="$LIBS $RESOLV_LIB $SSL_LIBS"
	AC_DEFINE(WITH_WEBSOCKETS)
fi
AM_CONDITIONAL(WITH_WEBSOCKETS, test "$with_websockets" = "yes")

AM_CONDITIONAL(HAVE_LIBZ, test ! -z "$HAVE_ZLIB_H")
AM_CONDITIONAL(HAVE_LIBJPEG, test ! -z "$HAVE_JPEGLIB_H")
AM_CONDITIONAL(HAVE_LIBPNG, test ! -z "$HAVE_PNGLIB_H")


SDLCONFIG="sdl-config"
AC_ARG_WITH(sdl-config,
[[  --with-sdl-config=FILE
    Use the given path to sdl-config when determining
    SDL configuration; defaults to "sdl-config"]],
[
    if test "$withval" != "yes" -a "$withval" != ""; then
        SDLCONFIG=$withval
    fi
])

if test -z "$with_sdl"; then
	if $SDLCONFIG --version >/dev/null 2>&1; then
		with_sdl=yes
		SDL_CFLAGS=`$SDLCONFIG --cflags`
		SDL_LIBS=`$SDLCONFIG --libs`
	else
		with_sdl=no
	fi
fi
AM_CONDITIONAL(HAVE_LIBSDL, test "x$with_sdl" = "xyes")
AC_SUBST(SDL_CFLAGS)
AC_SUBST(SDL_LIBS)


# Check for GTK+. if present, build the GTK+ vnc viewer example
PKG_CHECK_MODULES([GTK], [gtk+-2.0],,:)
AM_CONDITIONAL(HAVE_LIBGTK, test ! -z "$GTK_LIBS")

AC_CANONICAL_HOST
MINGW=`echo $host_os | grep mingw32  2>/dev/null`
AM_CONDITIONAL(MINGW, test ! -z "$MINGW" )
if test ! -z "$MINGW"; then
	WSOCKLIB="-lws2_32"
	LDFLAGS="$LDFLAGS -no-undefined"
fi
AC_SUBST(WSOCKLIB)

# Check for libgcrypt
AH_TEMPLATE(WITH_CLIENT_GCRYPT, [Enable support for libgcrypt in libvncclient])
AC_ARG_WITH(gcrypt,
[  --without-gcrypt        disable support for gcrypt],,)
AC_ARG_WITH(client-gcrypt,
[  --without-client-gcrypt disable support for gcrypt in libvncclient],,)

if test "x$with_gcrypt" != "xno"; then
	AM_PATH_LIBGCRYPT(1.4.0, , with_client_gcrypt=no)
	CFLAGS="$CFLAGS $LIBGCRYPT_CFLAGS"
	LIBS="$LIBS $LIBGCRYPT_LIBS"
	if test "x$with_client_gcrypt" != "xno"; then
		AC_DEFINE(WITH_CLIENT_GCRYPT)
	fi
fi

# Checks for GnuTLS
AH_TEMPLATE(HAVE_GNUTLS, [GnuTLS library present])
AC_ARG_WITH(gnutls,
[  --without-gnutls        disable support for gnutls]
[  --with-gnutls=DIR       use gnutls include/library files in DIR],,)

if test "x$with_gnutls" != "xno"; then
	PKG_CHECK_MODULES(GNUTLS, gnutls >= 2.4.0,,:)
	CFLAGS="$CFLAGS $GNUTLS_CFLAGS"
	LIBS="$LIBS $GNUTLS_LIBS"
fi
AM_CONDITIONAL(HAVE_GNUTLS, test ! -z "$GNUTLS_LIBS")
if test ! -z "$GNUTLS_LIBS" ; then
  AC_DEFINE(HAVE_GNUTLS)
fi


# warn if neither GnuTLS nor OpenSSL are available
if test -z "$SSL_LIBS" -a -z "$GNUTLS_LIBS"; then
		AC_MSG_WARN([
==========================================================================
*** No encryption library could be found. ***
A libvncserver/libvncclient built this way will not support SSL encryption.
To enable SSL install the necessary development packages (perhaps it is named
something like libssl-dev or gnutls-dev) and run configure again.
==========================================================================
])
	sleep 5
fi


# IPv6
AH_TEMPLATE(IPv6, [Enable IPv6 support])
AC_ARG_WITH(ipv6,
[  --without-ipv6          disable IPv6 support],,)
if test "x$with_ipv6" != "xno"; then
	AC_CHECK_FUNC(getaddrinfo, AC_DEFINE(IPv6,1),
		AC_CHECK_LIB(socket, getaddrinfo, AC_DEFINE(IPv6,1), [
			AC_MSG_CHECKING([for getaddrinfo in -lws2_32])
		        LIBS="$LIBS -lws2_32"
		        AC_TRY_LINK([#include <ws2tcpip.h>], [getaddrinfo(0, 0, 0, 0);], [
				      AC_DEFINE(IPv6,1)
		                      AC_MSG_RESULT([yes])
				      ],
				    AC_MSG_RESULT([no]))
                   ]))
fi



# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h endian.h fcntl.h netdb.h netinet/in.h stdlib.h string.h sys/endian.h sys/socket.h sys/time.h sys/timeb.h syslog.h unistd.h ws2tcpip.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_C_BIGENDIAN
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_HEADER_SYS_WAIT
AX_TYPE_SOCKLEN_T
if test ! -d ./rfb; then
	echo "creating subdir ./rfb for rfbint.h"
	mkdir ./rfb 
fi
AX_CREATE_STDINT_H(rfb/rfbint.h)
AC_CACHE_CHECK([for in_addr_t],
        vnc_cv_inaddrt, [
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

AC_CHECK_FUNCS([ftime gethostbyname gethostname gettimeofday inet_ntoa memmove memset mmap mkfifo select socket strchr strcspn strdup strerror strstr])

# check, if shmget is in cygipc.a
AC_CHECK_LIB(cygipc,shmget)
AM_CONDITIONAL(CYGIPC, test "$HAVE_CYGIPC" = "true")

# Check if /usr/include/linux exists, if so, define LINUX
AM_CONDITIONAL(LINUX, test -d /usr/include/linux)

# Check for OS X specific header
AC_CHECK_HEADER(ApplicationServices/ApplicationServices.h, HAVE_OSX="true")
AM_CONDITIONAL(OSX, test "$HAVE_OSX" = "true")

# Check for Android specific header
AC_CHECK_HEADER(android/api-level.h, HAVE_ANDROID="true")
AM_CONDITIONAL(ANDROID, test "$HAVE_ANDROID" = "true")
if test "$HAVE_ANDROID" = "true"; then
	AC_DEFINE(HAVE_ANDROID, 1, [Android host system detected])
fi

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
	libvncserver.pc
	libvncclient.pc
	libvncserver/Makefile
	examples/Makefile
	examples/android/Makefile
	webclients/Makefile
	webclients/java-applet/Makefile
	webclients/java-applet/ssl/Makefile
	libvncclient/Makefile
	client_examples/Makefile
	test/Makefile
	libvncserver-config
	LibVNCServer.spec])


AC_CONFIG_COMMANDS([chmod-libvncserver-config],[chmod a+x libvncserver-config])
AC_OUTPUT
chmod a+x ./libvncserver-config

