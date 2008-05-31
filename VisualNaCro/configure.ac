dnl Process this file with autoconf to produce a configure script.
dnl The macros which aren't shipped with the autotools are stored in the 
dnl Tools/config directory in .m4 files.

AC_INIT([VisualNaCro],[0.1],[http://libvncserver.sourceforge.net])
AC_PREREQ(2.54)
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE

dnl Checks for programs.
AC_CHECK_PROG(SWIG,swig,swig)
AC_CHECK_PROG(LIBVNCSERVERCONFIG,libvncserver-config,yes,no)
if test "$LIBVNCSERVERCONFIG" != "yes"; then
	AC_MSG_ERROR([Need to have libvncserver-config in PATH])
	exit 1
fi
AC_PROG_CC
AC_PROG_RANLIB
AC_EXEEXT
AC_OBJEXT

LIBVNCSERVERCFLAGS=`libvncserver-config --cflags`
LIBVNCSERVERLIBS=`libvncserver-config --libs`
AC_SUBST(LIBVNCSERVERCFLAGS)
AC_SUBST(LIBVNCSERVERLIBS)

dnl Checks for header files.
AC_HEADER_STDC

dnl How to specify include directories that may be system directories.
# -I should not be used on system directories (GCC)
if test "$GCC" = yes; then
    ISYSTEM="-isystem "
else
    ISYSTEM="-I"
fi


# Set info about shared libraries.
AC_SUBST(SO)
AC_SUBST(LDSHARED)
AC_SUBST(CCSHARED)
AC_SUBST(LINKFORSHARED)

# SO is the extension of shared libraries `(including the dot!)
AC_MSG_CHECKING(SO)
if test -z "$SO"
then
	case $host in
	*-*-hp*) SO=.sl;;
	*-*-darwin*) SO=.bundle;;
	*-*-cygwin* | *-*-mingw*) SO=.dll;;
	*) SO=.so;;
	esac
fi
AC_MSG_RESULT($SO)

# LDSHARED is the ld *command* used to create shared library
# -- "ld" on SunOS 4.x.x, "ld -G" on SunOS 5.x, "ld -shared" on IRIX 5
# (Shared libraries in this instance are shared modules to be loaded into
# Python, as opposed to building Python itself as a shared library.)
AC_MSG_CHECKING(LDSHARED)
if test -z "$LDSHARED"
then
	case $host in
	*-*-aix*) LDSHARED="\$(srcdir)/ld_so_aix \$(CC)";;
	*-*-cygwin* | *-*-mingw*)
            if test "$GCC" = yes; then
                LDSHARED="dllwrap --driver-name gcc --dlltool dlltool --export-all-symbols --as as --dllname \$(LIBPREFIX)\$(TARGET)\$(SO)"
            else
                if test "cl" = $CC ;  then
                    # Microsoft Visual C++ (MSVC)
                    LDSHARED="$CC -nologo -LD"
                else
                    # Unknown compiler try gcc approach
                    LDSHARED="$CC -shared"
                fi
            fi ;;
	*-*-irix5*) LDSHARED="ld -shared";;
	*-*-irix6*) LDSHARED="ld ${SGI_ABI} -shared -all";;
	*-*-sunos4*) LDSHARED="ld";;
	*-*-solaris*) LDSHARED="ld -G";;
	*-*-hp*) LDSHARED="ld -b";;
	*-*-osf*) LDSHARED="ld -shared -expect_unresolved \"*\"";;
	*-sequent-sysv4) LDSHARED="ld -G";;
	*-*-next*)
		if test "$ns_dyld"
		then LDSHARED='$(CC) $(LDFLAGS) -bundle -prebind'
		else LDSHARED='$(CC) $(CFLAGS) -nostdlib -r';
		fi
                if test "$with_next_framework" ; then
		    LDSHARED="$LDSHARED \$(LDLIBRARY)"
		fi ;;
	*-*-linux*) LDSHARED="gcc -shared";;
	*-*-dgux*) LDSHARED="ld -G";;
	*-*-freebsd3*) LDSHARED="gcc -shared";;
	*-*-freebsd* | *-*-openbsd*) LDSHARED="ld -Bshareable";;
	*-*-netbsd*)
		if [[ "`$CC -dM -E - </dev/null | grep __ELF__`" != "" ]]
		then
			LDSHARED="cc -shared"
		else
			LDSHARED="ld -Bshareable"
		fi;;
	*-sco-sysv*) LDSHARED="cc -G -KPIC -Ki486 -belf -Wl,-Bexport";;
	*-*-darwin*) LDSHARED="cc -bundle -undefined suppress -flat_namespace";;
	*)	LDSHARED="ld";;
	esac
fi
AC_MSG_RESULT($LDSHARED)
# CCSHARED are the C *flags* used to create objects to go into a shared
# library (module) -- this is only needed for a few systems
AC_MSG_CHECKING(CCSHARED)
if test -z "$CCSHARED"
then
	case $host in
	*-*-hp*) if test "$GCC" = yes;
		 then CCSHARED="-fpic";
		 else CCSHARED="+z";
		 fi;;
	*-*-linux*) CCSHARED="-fpic";;
	*-*-freebsd* | *-*-openbsd*) CCSHARED="-fpic";;
	*-*-netbsd*) CCSHARED="-fPIC";;
	*-sco-sysv*) CCSHARED="-KPIC -dy -Bdynamic";;
	*-*-irix6*)  case $CC in
		   *gcc*) CCSHARED="-shared";;
		   *) CCSHARED="";;
		   esac;;
	esac
fi
AC_MSG_RESULT($CCSHARED)

# RPATH is the path used to look for shared library files.
AC_MSG_CHECKING(RPATH)
if test -z "$RPATH"
then
	case $host in
	*-*-solaris*) RPATH='-R. -R$(exec_prefix)/lib';;
        *-*-irix*) RPATH='-rpath .:$(exec_prefix)/lib';;
	*-*-linux*) RPATH='-Xlinker -rpath $(exec_prefix)/lib -Xlinker -rpath .';;
	*)	RPATH='';;
	esac
fi
AC_MSG_RESULT($RPATH)
AC_SUBST(RPATH)

# LINKFORSHARED are the flags passed to the $(CC) command that links
# the a few executables -- this is only needed for a few systems

AC_MSG_CHECKING(LINKFORSHARED)
if test -z "$LINKFORSHARED"
then
	case $host in
	*-*-aix*)	LINKFORSHARED='-Wl,-bE:$(srcdir)/python.exp -lld';;
	*-*-hp*)
	    LINKFORSHARED="-Wl,-E -Wl,+s -Wl,+b\$(BINLIBDEST)/lib-dynload";;
	*-*-linux*) LINKFORSHARED="-Xlinker -export-dynamic";;
	*-*-next*) LINKFORSHARED="-u libsys_s";;
	*-sco-sysv*) LINKFORSHARED="-Bdynamic -dy -Wl,-Bexport";;
	*-*-irix6*) LINKFORSHARED="-all";;
	esac
fi
AC_MSG_RESULT($LINKFORSHARED)

# This variation is needed on OS-X because there is no (apparent) consistency in shared libary naming.
# Sometimes .bundle works, but sometimes .so is needed.  It depends on the target language

# Optional CFLAGS used to silence compiler warnings on some platforms.

AC_SUBST(PLATFLAGS)
case $host in
   *-*-darwin*) PLATFLAGS="-Wno-long-double";;
   *) PLATFLAGS="";;
esac

#----------------------------------------------------------------
# Look for Perl5
#----------------------------------------------------------------

PERLBIN=

AC_ARG_WITH(perl5,[  --with-perl5=path       Set location of Perl5 executable],[ PERLBIN="$withval"], [PERLBIN=])

# First figure out what the name of Perl5 is

if test -z "$PERLBIN"; then
AC_CHECK_PROGS(PERL, perl perl5.8.1 perl5.6.1 perl5.6.0 perl5.004 perl5.003 perl5.002 perl5.001 perl5 perl)
else
PERL="$PERLBIN"
fi


AC_MSG_CHECKING(for Perl5 header files)
if test -n "$PERL"; then
	PERL5DIR=`($PERL -e 'use Config; print $Config{archlib}, "\n";') 2>/dev/null`
	if test "$PERL5DIR" != ""; then
		dirs="$PERL5DIR $PERL5DIR/CORE"
		PERL5EXT=none
		for i in $dirs; do
			if test -r $i/perl.h; then
				AC_MSG_RESULT($i)
				PERL5EXT="$i"
				break;
			fi
		done
		if test "$PERL5EXT" = none; then
			PERL5EXT="$PERL5DIR/CORE"
			AC_MSG_RESULT(could not locate perl.h...using $PERL5EXT)
		fi

		AC_MSG_CHECKING(for Perl5 library)
		PERL5LIB=`($PERL -e 'use Config; $_=$Config{libperl}; s/^lib//; s/$Config{_a}$//; print $_, "\n"') 2>/dev/null`
		if test "$PERL5LIB" = "" ; then
			AC_MSG_RESULT(not found)
		else
			AC_MSG_RESULT($PERL5LIB)
		fi
    AC_MSG_CHECKING(for Perl5 compiler options)
 		PERL5CCFLAGS=`($PERL -e 'use Config; print $Config{ccflags}, "\n"' | sed "s/-I/$ISYSTEM/") 2>/dev/null`
 		if test "$PERL5CCFLAGS" = "" ; then
 			AC_MSG_RESULT(not found)
 		else
 			AC_MSG_RESULT($PERL5CCFLAGS)
 		fi
	else
		AC_MSG_RESULT(unable to determine perl5 configuration)
		PERL5EXT=$PERL5DIR
	fi
else
       	AC_MSG_RESULT(could not figure out how to run perl5)
fi

# Cygwin (Windows) needs the library for dynamic linking
case $host in
*-*-cygwin* | *-*-mingw*) PERL5DYNAMICLINKING="-L$PERL5EXT -l$PERL5LIB";;
*)PERL5DYNAMICLINKING="";;
esac

AC_SUBST(PERL)
AC_SUBST(PERL5EXT)
AC_SUBST(PERL5DYNAMICLINKING)
AC_SUBST(PERL5LIB)
AC_SUBST(PERL5CCFLAGS)

#----------------------------------------------------------------
# Miscellaneous
#----------------------------------------------------------------

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

dnl configure.in ends here
