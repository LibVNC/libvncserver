# Process this file with autoconf to produce a configure script.
AC_INIT(LibVNCServer, 0.9.10, http://sourceforge.net/projects/libvncserver)
AM_INIT_AUTOMAKE(LibVNCServer, 0.9.10)
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AM_CONFIG_HEADER(rfbconfig.h)
AX_PREFIX_CONFIG_H([rfb/rfbconfig.h])

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

# Checks for X libraries
HAVE_X11="false"
AC_PATH_XTRA
AH_TEMPLATE(HAVE_X11, [X11 build environment present])

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


# See if we are to build x11vnc:
AH_TEMPLATE(HAVE_SYSTEM_LIBVNCSERVER, [Use the system libvncserver build environment for x11vnc.])
AC_ARG_WITH(system-libvncserver,
[  --with-system-libvncserver     use installed libvncserver for x11vnc]
[  --with-system-libvncserver=DIR use libvncserver installed in DIR for x11vnc],,)
AC_ARG_WITH(x11vnc,
[  --with-x11vnc           configure for building the x11vnc subdir (if present)]
[                          you will need to cd to x11vnc and run 'make' etc.],,)

if test ! -z "$with_x11vnc" -a "$with_x11vnc" = "yes"; then
	build_x11vnc="yes"
elif test "$PACKAGE_NAME" = "x11vnc"; then
	build_x11vnc="yes"
else
	build_x11vnc="no"
fi
	
# x11vnc only:
if test "$build_x11vnc" = "yes"; then

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
AH_TEMPLATE(HAVE_DPMS, [DPMS extension build environment present])
AH_TEMPLATE(HAVE_LINUX_VIDEODEV_H, [video4linux build environment present])
AH_TEMPLATE(HAVE_LINUX_FB_H, [linux fb device build environment present])
AH_TEMPLATE(HAVE_LINUX_INPUT_H, [linux/input.h present])
AH_TEMPLATE(HAVE_LINUX_UINPUT_H, [linux uinput device build environment present])
AH_TEMPLATE(HAVE_MACOSX_NATIVE_DISPLAY, [build MacOS X native display support])
AH_TEMPLATE(HAVE_MACOSX_OPENGL_H, [MacOS X OpenGL present])

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
AC_ARG_WITH(dpms,
[  --without-dpms          disable dpms extension support],,)
AC_ARG_WITH(v4l,
[  --without-v4l           disable video4linux support],,)
AC_ARG_WITH(fbdev,
[  --without-fbdev         disable linux fb device support],,)
AC_ARG_WITH(uinput,
[  --without-uinput        disable linux uinput device support],,)
AC_ARG_WITH(macosx-native,
[  --without-macosx-native disable MacOS X native display support],,)

fi
# end x11vnc only.

if test "x$with_x" = "xno"; then
	HAVE_X11="false"
elif test "$X_CFLAGS" != "-DX_DISPLAY_MISSING"; then
	AC_CHECK_LIB(X11, XGetImage, [AC_DEFINE(HAVE_X11) HAVE_X11="true"],
		HAVE_X11="false",
		$X_LIBS $X_PRELIBS -lX11 $X_EXTRA_LIBS)

	# x11vnc only:
	if test $HAVE_X11 = "true" -a "$build_x11vnc" = "yes"; then
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

		if test "x$with_fbpm" != "xno"; then
			AC_CHECK_LIB(Xext, FBPMForceLevel, 
				[AC_DEFINE(HAVE_FBPM)], ,
				$X_LIBS $X_PRELIBS -lX11 $X_EXTRA_LIBS)
		fi

		if test "x$with_dpms" != "xno"; then
			AC_CHECK_LIB(Xext, DPMSForceLevel, 
				[AC_DEFINE(HAVE_DPMS)], ,
				$X_LIBS $X_PRELIBS -lX11 $X_EXTRA_LIBS)
		fi

		AC_CHECK_LIB(Xtst, XTestGrabControl, 
			X_PRELIBS="-lXtst $X_PRELIBS"
			[AC_DEFINE(HAVE_XTESTGRABCONTROL) HAVE_XTESTGRABCONTROL="true"], ,
			$X_LIBS $X_PRELIBS -lX11 $X_EXTRA_LIBS)

		AC_CHECK_LIB(Xtst, XTestFakeKeyEvent, 
			X_PRELIBS="-lXtst $X_PRELIBS"
			[AC_DEFINE(HAVE_XTEST) HAVE_XTEST="true"], ,
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
				[AC_DEFINE(HAVE_LIBXRANDR) HAVE_LIBXRANDR="true"], ,
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
		if test ! -z "$HAVE_LIBXRANDR"; then
			# also need /usr/X11/include for Solaris 10 10/08 and later
			case `(uname -sr) 2>/dev/null` in
			"SunOS 5"*) CPPFLAGS="$CPPFLAGS -I/usr/X11/include" ;;
			esac
		fi

		X_LIBS="$X_LIBS $X_PRELIBS -lX11 $X_EXTRA_LIBS"
	fi
	# end x11vnc only.
fi

AC_SUBST(X_LIBS)
AM_CONDITIONAL(HAVE_X11, test $HAVE_X11 != "false")

# x11vnc only:
if test "$build_x11vnc" = "yes"; then

if test "x$HAVE_X11" = "xfalse" -a "x$with_x" != "xno"; then
	AC_MSG_ERROR([
==========================================================================
*** A working X window system build environment is required to build ***
x11vnc.  Make sure any required X development packages are installed.
If they are installed in non-standard locations, one can use the
--x-includes=DIR and --x-libraries=DIR configure options or set the
CPPFLAGS and LDFLAGS environment variables to indicate where the X
window system header files and libraries may be found.  On 64+32 bit
machines you may need to point to lib64 or lib32 directories to pick up
the correct word size.

If you want to build x11vnc without X support (e.g. for -rawfb use only
or for native Mac OS X), specify the --without-x configure option.
==========================================================================
])
fi

if test "x$HAVE_X11" = "xtrue" -a "x$HAVE_XTEST" != "xtrue"; then
	AC_MSG_WARN([
==========================================================================
*** A working build environment for the XTEST extension was not found ***
(libXtst).  An x11vnc built this way will be *ONLY BARELY USABLE*.
You will be able to move the mouse but not click or type.  There can
also be deadlocks if an application grabs the X server.

It is recommended that you install the necessary development packages
for XTEST (perhaps it is named something like libxtst-dev) and run
configure again.
==========================================================================
])
	sleep 5
fi

if test "x$with_v4l" != "xno"; then
	AC_CHECK_HEADER(linux/videodev.h,
		[AC_DEFINE(HAVE_LINUX_VIDEODEV_H)],,)
fi
if test "x$with_fbdev" != "xno"; then
	AC_CHECK_HEADER(linux/fb.h,
		[AC_DEFINE(HAVE_LINUX_FB_H)],,)
fi
if test "x$with_uinput" != "xno"; then
	AC_CHECK_HEADER(linux/input.h,
		[AC_DEFINE(HAVE_LINUX_INPUT_H) HAVE_LINUX_INPUT_H="true"],,)
	if test "x$HAVE_LINUX_INPUT_H" = "xtrue"; then
		AC_CHECK_HEADER(linux/uinput.h,
			[AC_DEFINE(HAVE_LINUX_UINPUT_H)],, [#include <linux/input.h>])
	fi
fi

if test "x$with_macosx_native" != "xno"; then
	AC_DEFINE(HAVE_MACOSX_NATIVE_DISPLAY)
fi

# Check for OS X opengl header
AC_CHECK_HEADER(OpenGL/OpenGL.h,
	[AC_DEFINE(HAVE_MACOSX_OPENGL_H) HAVE_MACOSX_OPENGL_H="true"],,)

AH_TEMPLATE(HAVE_AVAHI, [Avahi/mDNS client build environment present])
AC_ARG_WITH(avahi,
[  --without-avahi         disable support for Avahi/mDNS]
[  --with-avahi=DIR        use avahi include/library files in DIR],,)
if test "x$with_avahi" != "xno"; then
	printf "checking for avahi... "
	if test ! -z "$with_avahi" -a "x$with_avahi" != "xyes"; then
		AVAHI_CFLAGS="-I$with_avahi/include"
		AVAHI_LIBS="-L$with_avahi/lib -lavahi-common -lavahi-client"
		echo "using $with_avahi"
		with_avahi=yes
	elif pkg-config --atleast-version=0.6.4 avahi-client >/dev/null 2>&1; then
		AVAHI_CFLAGS=`pkg-config --cflags avahi-client`
		AVAHI_LIBS=`pkg-config --libs avahi-client`
		with_avahi=yes
		echo yes
	else
		with_avahi=no
		echo no
	fi
fi
if test "x$with_avahi" = "xyes"; then
	AC_DEFINE(HAVE_AVAHI)
	AC_SUBST(AVAHI_CFLAGS)
	AC_SUBST(AVAHI_LIBS)
fi

fi
# end x11vnc only.

# only used in x11vnc/Makefile.am but needs to always be defined:
AM_CONDITIONAL(OSX_OPENGL, test "$HAVE_MACOSX_OPENGL_H" = "true")

# Checks for libraries.

if test ! -z "$with_system_libvncserver" -a "x$with_system_libvncserver" != "xno"; then
	printf "checking for system libvncserver... "
	vneed="0.9.1"
	if test "X$VNEED" != "X"; then
		vneed=$VNEED
	fi
	if test "x$with_system_libvncserver" != "xyes"; then
		rflag=""
		if test "x$ld_minus_R" = "xno"; then
			:
		elif test "x$GCC" = "xyes"; then
			rflag="-Xlinker -R$with_system_libvncserver/lib"
		else
			rflag="-R$with_system_libvncserver/lib"
		fi
		cmd="$with_system_libvncserver/bin/libvncserver-config"
		if $cmd --version 1>/dev/null 2>&1; then
			cvers=`$cmd --version 2>/dev/null`
			cscore=`echo "$cvers" | tr '.' ' ' | awk '{print 10000 * $1 + 100 * $2 + $3}'`
			nscore=`echo "$vneed" | tr '.' ' ' | awk '{print 10000 * $1 + 100 * $2 + $3}'`

			if test $cscore -lt $nscore; then
				echo "no"
				with_system_libvncserver=no
				AC_MSG_ERROR([
==========================================================================
*** Need libvncserver version $vneed, have version $cvers ***
You are building with a system installed libvncserver and it is not
new enough.
==========================================================================
])
			else
				SYSTEM_LIBVNCSERVER_CFLAGS="-I$with_system_libvncserver/include"
				SYSTEM_LIBVNCSERVER_LIBS="-L$with_system_libvncserver/lib $rflag -lvncserver -lvncclient"
				echo "using $with_system_libvncserver"
				with_system_libvncserver=yes
			fi
		else
			echo " *** cannot run $cmd *** " 1>&2
			with_system_libvncserver=no
			echo no
		fi
	elif libvncserver-config --version 1>/dev/null 2>&1; then
		rflag=""
		rprefix=`libvncserver-config --prefix`
		if test "x$ld_minus_R" = "xno"; then
			:
		elif test "x$GCC" = "xyes"; then
			rflag=" -Xlinker -R$rprefix/lib "
		else
			rflag=" -R$rprefix/lib "
		fi
		cvers=`libvncserver-config --version 2>/dev/null`
		cscore=`echo "$cvers" | tr '.' ' ' | awk '{print 10000 * $1 + 100 * $2 + $3}'`
		nscore=`echo "$vneed" | tr '.' ' ' | awk '{print 10000 * $1 + 100 * $2 + $3}'`

		if test $cscore -lt $nscore; then
			echo "no"
				AC_MSG_ERROR([
==========================================================================
*** Need libvncserver version $vneed, have version $cvers ***
You are building with a system installed libvncserver and it is not
new enough.
==========================================================================
])
		else
			SYSTEM_LIBVNCSERVER_CFLAGS=`libvncserver-config --cflags`
			SYSTEM_LIBVNCSERVER_LIBS="$rflag"`libvncserver-config --libs`
			with_system_libvncserver=yes
			echo yes
		fi
	else
		with_system_libvncserver=no
		echo no
	fi
fi

if test "x$with_system_libvncserver" = "xyes"; then
	AC_DEFINE(HAVE_SYSTEM_LIBVNCSERVER)
	AC_SUBST(SYSTEM_LIBVNCSERVER_CFLAGS)
	AC_SUBST(SYSTEM_LIBVNCSERVER_LIBS)
fi
AM_CONDITIONAL(HAVE_SYSTEM_LIBVNCSERVER, test "x$with_system_libvncserver" = "xyes")


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
	if test "$build_x11vnc" = "yes"; then
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
	if test "$build_x11vnc" = "yes"; then
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

# x11vnc only:
if test "$build_x11vnc" = "yes"; then
	AC_CHECK_HEADERS([pwd.h sys/wait.h utmpx.h termios.h sys/ioctl.h sys/stropts.h])
fi

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
# x11vnc only:
if test "$build_x11vnc" = "yes"; then
	AC_CHECK_FUNCS([setsid setpgrp getpwuid getpwnam getspnam getuid geteuid setuid setgid seteuid setegid initgroups waitpid setutxent grantpt shmat])
fi

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
AM_CONDITIONAL(WITH_X11VNC,  test "$build_x11vnc" = "yes")
AC_SUBST(RPMSOURCEDIR)

AC_CONFIG_FILES([Makefile
	libvncserver.pc
	libvncclient.pc
	libvncserver/Makefile
	examples/Makefile
	examples/android/Makefile
	vncterm/Makefile
	webclients/Makefile
	webclients/java-applet/Makefile
	webclients/java-applet/ssl/Makefile
	libvncclient/Makefile
	client_examples/Makefile
	test/Makefile
	libvncserver-config
	LibVNCServer.spec])
#
# x11vnc only:
#
if test "$build_x11vnc" = "yes"; then
	#
	# NOTE: if you are using the LibVNCServer-X.Y.Z.tar.gz source
	# tarball and nevertheless want to run autoconf (i.e. aclocal,
	# autoheader, automake, autoconf) AGAIN (perhaps you have a
	# special target system, e.g. embedded) then you will need to
	# comment out the following 'AC_CONFIG_FILES' line to avoid
	# automake error messages like:
	#
	# configure.ac:690: required file `x11vnc/Makefile.in' not found
	#
	AC_CONFIG_FILES([x11vnc/Makefile x11vnc/misc/Makefile x11vnc/misc/turbovnc/Makefile])

	if test ! -z "$with_system_libvncserver" -a "x$with_system_libvncserver" != "xno"; then
		# need to move local tarball rfb headers aside:
		hdrs="rfb.h rfbclient.h rfbproto.h rfbregion.h rfbint.h"
		echo "with-system-libvncserver: moving aside headers $hdrs"
		for hdr in $hdrs
		do
			if test -f "rfb/$hdr"; then
				echo "with-system-libvncserver: moving rfb/$hdr to rfb/$hdr.ORIG"
				mv rfb/$hdr rfb/$hdr.ORIG
			fi
		done
		echo "with-system-libvncserver: *NOTE* move them back manually to start over."
	fi
fi

AC_CONFIG_COMMANDS([chmod-libvncserver-config],[chmod a+x libvncserver-config])
AC_OUTPUT
chmod a+x ./libvncserver-config

