AH_TEMPLATE(socklen_t, [The type for socklen])
AC_DEFUN([AC_TYPE_SOCKLEN_T],
[AC_CACHE_CHECK([for socklen_t], ac_cv_type_socklen_t,
[
  AC_TRY_COMPILE(
  [#include <sys/types.h>
   #include <sys/socket.h>],
  [socklen_t len = 42; return 0;],
  ac_cv_type_socklen_t=yes,
  ac_cv_type_socklen_t=no)
])
  if test $ac_cv_type_socklen_t != yes; then
    AC_DEFINE(socklen_t, int)
  fi
])

dnl Available from the GNU Autoconf Macro Archive at:
dnl http://www.gnu.org/software/ac-archive/htmldoc/ac_compile_check_sizeof.html
dnl
AC_DEFUN([AC_COMPILE_CHECK_SIZEOF],
[changequote(<<, >>)dnl
dnl The name to #define.
define(<<AC_TYPE_NAME>>, translit(sizeof_$1, [a-z *], [A-Z_P]))dnl
dnl The cache variable name.
define(<<AC_CV_NAME>>, translit(ac_cv_sizeof_$1, [ *], [_p]))dnl
changequote([, ])dnl
AC_MSG_CHECKING(size of $1)
AC_CACHE_VAL(AC_CV_NAME,
[for ac_size in 4 8 1 2 16 $2 ; do # List sizes in rough order of prevalence.
  AC_TRY_COMPILE([#include "confdefs.h"
#include <sys/types.h>
$2
], [switch (0) case 0: case (sizeof ($1) == $ac_size):;], AC_CV_NAME=$ac_size)
  if test x$AC_CV_NAME != x ; then break; fi
done
])
if test x$AC_CV_NAME = x ; then
  AC_MSG_ERROR([cannot determine a size for $1])
fi
AC_MSG_RESULT($AC_CV_NAME)
AC_DEFINE_UNQUOTED(AC_TYPE_NAME, $AC_CV_NAME, [The number of bytes in type $1])
undefine([AC_TYPE_NAME])dnl
undefine([AC_CV_NAME])dnl
])

dnl Available from the GNU Autoconf Macro Archive at:
dnl http://www.gnu.org/software/ac-archive/htmldoc/ac_create_stdint_h.html
dnl
AC_DEFUN([AC_CREATE_STDINT_H],
[# ------ AC CREATE STDINT H -------------------------------------
AC_MSG_CHECKING([for stdint-types....])
ac_stdint_h=`echo ifelse($1, , _stdint.h, $1)`
if test "$ac_stdint_h" = "stdint.h" ; then
 AC_MSG_RESULT("(are you sure you want them in ./stdint.h?)")
elif test "$ac_stdint_h" = "inttypes.h" ; then
 AC_MSG_RESULT("(are you sure you want them in ./inttypes.h?)")
else
 AC_MSG_RESULT("(putting them into $ac_stdint_h)")
fi

inttype_headers=`echo inttypes.h sys/inttypes.h sys/inttypes.h $2 \
| sed -e 's/,/ /g'`

 ac_cv_header_stdint_x="no-file"
 ac_cv_header_stdint_o="no-file"
 ac_cv_header_stdint_u="no-file"
 for i in stdint.h $inttype_headers ; do
   unset ac_cv_type_uintptr_t
   unset ac_cv_type_uint64_t
   _AC_CHECK_TYPE_NEW(uintptr_t,[ac_cv_header_stdint_x=$i],dnl
     continue,[#include <$i>])
   AC_CHECK_TYPE(uint64_t,[and64="(uint64_t too)"],[and64=""],[#include<$i>])
   AC_MSG_RESULT(... seen our uintptr_t in $i $and64)
   break;
 done
 if test "$ac_cv_header_stdint_x" = "no-file" ; then
 for i in stdint.h $inttype_headers ; do
   unset ac_cv_type_uint32_t
   unset ac_cv_type_uint64_t
   AC_CHECK_TYPE(uint32_t,[ac_cv_header_stdint_o=$i],dnl
     continue,[#include <$i>])
   AC_CHECK_TYPE(uint64_t,[and64="(uint64_t too)"],[and64=""],[#include<$i>])
   AC_MSG_RESULT(... seen our uint32_t in $i $and64)
   break;
 done
 if test "$ac_cv_header_stdint_o" = "no-file" ; then
 for i in sys/types.h $inttype_headers ; do
   unset ac_cv_type_u_int32_t
   unset ac_cv_type_u_int64_t
   AC_CHECK_TYPE(u_int32_t,[ac_cv_header_stdint_u=$i],dnl
     continue,[#include <$i>])
   AC_CHECK_TYPE(uint64_t,[and64="(u_int64_t too)"],[and64=""],[#include<$i>])
   AC_MSG_RESULT(... seen our u_int32_t in $i $and64)
   break;
 done
 fi
 fi

# ----------------- DONE inttypes.h checks MAYBE C basic types --------

if test "$ac_cv_header_stdint_x" = "no-file" ; then
   AC_COMPILE_CHECK_SIZEOF(char)
   AC_COMPILE_CHECK_SIZEOF(short)
   AC_COMPILE_CHECK_SIZEOF(int)
   AC_COMPILE_CHECK_SIZEOF(long)
   AC_COMPILE_CHECK_SIZEOF(void*)
   ac_cv_header_stdint_test="yes"
else
   ac_cv_header_stdint_test="no"
fi

# ----------------- DONE inttypes.h checks START header -------------
_ac_stdint_h=AS_TR_CPP(_$ac_stdint_h)
AC_MSG_RESULT(creating $ac_stdint_h : $_ac_stdint_h)
echo "#ifndef" $_ac_stdint_h >$ac_stdint_h
echo "#define" $_ac_stdint_h "1" >>$ac_stdint_h
echo "#ifndef" _GENERATED_STDINT_H >>$ac_stdint_h
echo "#define" _GENERATED_STDINT_H '"'$PACKAGE $VERSION'"' >>$ac_stdint_h
if test "$GCC" = "yes" ; then
  echo "/* generated using a gnu compiler version" `$CC --version` "*/" \
  >>$ac_stdint_h
else
  echo "/* generated using $CC */" >>$ac_stdint_h
fi
echo "" >>$ac_stdint_h

if test "$ac_cv_header_stdint_x" != "no-file" ; then
   ac_cv_header_stdint="$ac_cv_header_stdint_x"
elif  test "$ac_cv_header_stdint_o" != "no-file" ; then
   ac_cv_header_stdint="$ac_cv_header_stdint_o"
elif  test "$ac_cv_header_stdint_u" != "no-file" ; then
   ac_cv_header_stdint="$ac_cv_header_stdint_u"
else
   ac_cv_header_stdint="stddef.h"
fi

# ----------------- See if int_least and int_fast types are present
unset ac_cv_type_int_least32_t
unset ac_cv_type_int_fast32_t
AC_CHECK_TYPE(int_least32_t,,,[#include <$ac_cv_header_stdint>])
AC_CHECK_TYPE(int_fast32_t,,,[#include<$ac_cv_header_stdint>])

if test "$ac_cv_header_stdint" != "stddef.h" ; then
if test "$ac_cv_header_stdint" != "stdint.h" ; then
AC_MSG_RESULT(..adding include stddef.h)
   echo "#include <stddef.h>" >>$ac_stdint_h
fi ; fi
AC_MSG_RESULT(..adding include $ac_cv_header_stdint)
   echo "#include <$ac_cv_header_stdint>" >>$ac_stdint_h
echo "" >>$ac_stdint_h

# ----------------- DONE header START basic int types -------------
if test "$ac_cv_header_stdint_x" = "no-file" ; then
   AC_MSG_RESULT(... need to look at C basic types)
dnl ac_cv_header_stdint_test="yes" # moved up before creating the file
else
   AC_MSG_RESULT(... seen good stdint.h inttypes)
dnl ac_cv_header_stdint_test="no"  # moved up before creating the file
fi

if test "$ac_cv_header_stdint_u" != "no-file" ; then
   AC_MSG_RESULT(... seen bsd/sysv typedefs)
   cat >>$ac_stdint_h <<EOF

/* int8_t int16_t int32_t defined by inet code, redeclare the u_intXX types */
typedef u_int8_t uint8_t;
typedef u_int16_t uint16_t;
typedef u_int32_t uint32_t;
EOF
    cat >>$ac_stdint_h <<EOF

/* glibc compatibility */
#ifndef __int8_t_defined
#define __int8_t_defined
#endif
EOF
fi

ac_cv_sizeof_x="$ac_cv_sizeof_char:$ac_cv_sizeof_short"
ac_cv_sizeof_X="$ac_cv_sizeof_x:$ac_cv_sizeof_int"
ac_cv_sizeof_X="$ac_cv_sizeof_X:$ac_cv_sizeof_voidp:$ac_cv_sizeof_long"
if test "$ac_cv_header_stdint" = "stddef.h" ; then
#   we must guess all the basic types. Apart from byte-adressable system,
# there a few 32-bit-only dsp-systems. nibble-addressable systems are way off.
    cat >>$ac_stdint_h <<EOF
/* ------------ BITSPECIFIC INTTYPES SECTION --------------- */
EOF
    t="typedefs for a"
    case "$ac_cv_sizeof_X" in
     1:2:2:2:4) AC_MSG_RESULT(..adding $t normal 16-bit system)
                cat >>$ac_stdint_h <<EOF
/*              a normal 16-bit system                       */
typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;
typedef unsigned long   uint32_t;
#ifndef __int8_t_defined
#define __int8_t_defined
typedef          char    int8_t;
typedef          short   int16_t;
typedef          long    int32_t;
#endif
EOF
;;
     1:2:2:4:4) AC_MSG_RESULT(..adding $t 32-bit system derived from a 16-bit)
                cat >>$ac_stdint_h <<EOF
/*              a 32-bit system derived from a 16-bit        */
typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;
typedef unsigned int    uint32_t;
#ifndef __int8_t_defined
#define __int8_t_defined
typedef          char    int8_t;
typedef          short   int16_t;
typedef          int     int32_t;
#endif
EOF
;;
     1:2:4:4:4) AC_MSG_RESULT(..adding $t normal 32-bit system)
                cat >>$ac_stdint_h <<EOF
/*              a normal 32-bit system                       */
typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;
typedef unsigned int    uint32_t;
#ifndef __int8_t_defined
#define __int8_t_defined
typedef          char    int8_t;
typedef          short   int16_t;
typedef          int     int32_t;
#endif
EOF
;;
     1:2:4:4:8) AC_MSG_RESULT(..adding $t 32-bit system prepared for 64-bit)
                cat >>$ac_stdint_h <<EOF

/*              a 32-bit system prepared for 64-bit          */
typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;
typedef unsigned int    uint32_t;
#ifndef __int8_t_defined
#define __int8_t_defined
typedef          char    int8_t;
typedef          short   int16_t;
typedef          int     int32_t;
#endif
EOF
;;
     1:2:4:8:8) AC_MSG_RESULT(..adding $t normal 64-bit system)
                cat >>$ac_stdint_h <<EOF

/*              a normal 64-bit system                       */
typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;
typedef unsigned int    uint32_t;
#ifndef __int8_t_defined
#define __int8_t_defined
typedef          char    int8_t;
typedef          short   int16_t;
typedef          int     int32_t;
#endif
EOF
;;
     1:2:4:8:4) AC_MSG_RESULT(..adding $t 64-bit system derived from a 32-bit)
                cat >>$ac_stdint_h <<EOF

/*              a 64-bit system derived from a 32-bit system */
typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;
typedef unsigned int    uint32_t;
#ifndef __int8_t_defined
#define __int8_t_defined
typedef          char    int8_t;
typedef          short   int16_t;
typedef          int     int32_t;
#endif
EOF
;;
  *)
    AC_MSG_ERROR([ $ac_cv_sizeof_X dnl
 what is that a system? contact the author, quick! http://ac-archive.sf.net])
    exit 1
;;
   esac
fi

# ------------- DONE basic int types START int64_t types ------------
if test "$ac_cv_type_uint64_t" = "yes"
then AC_MSG_RESULT(... seen good uint64_t)
     cat >>$ac_stdint_h <<EOF

/* system headers have good uint64_t */
#ifndef _HAVE_UINT64_T
#define _HAVE_UINT64_T
#endif
EOF

elif test "$ac_cv_type_u_int64_t" = "yes"
then AC_MSG_RESULT(..adding typedef u_int64_t uint64_t)
     cat >>$ac_stdint_h <<EOF

/* system headers have an u_int64_t */
#ifndef _HAVE_UINT64_T
#define _HAVE_UINT64_T
typedef u_int64_t uint64_t;
#endif
EOF
else AC_MSG_RESULT(..adding generic uint64_t runtime checks)
     cat >>$ac_stdint_h <<EOF

/* -------------------- 64 BIT GENERIC SECTION -------------------- */
/* here are some common heuristics using compiler runtime specifics */
#if defined __STDC_VERSION__ && defined __STDC_VERSION__ > 199901L

#ifndef _HAVE_UINT64_T
#define _HAVE_UINT64_T
typedef long long int64_t;
typedef unsigned long long uint64_t;
#endif

#elif !defined __STRICT_ANSI__
#if defined _MSC_VER || defined __WATCOMC__ || defined __BORLANDC__

#ifndef _HAVE_UINT64_T
#define _HAVE_UINT64_T
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#endif

#elif defined __GNUC__ || defined __MWERKS__ || defined __ELF__
dnl /* note: all ELF-systems seem to have loff-support which needs 64-bit */

#if !defined _NO_LONGLONG
#ifndef _HAVE_UINT64_T
#define _HAVE_UINT64_T
typedef long long int64_t;
typedef unsigned long long uint64_t;
#endif
#endif

#elif defined __alpha || (defined __mips && defined _ABIN32)

#if !defined _NO_LONGLONG
#ifndef _HAVE_UINT64_T
#define _HAVE_UINT64_T
typedef long int64_t;
typedef unsigned long uint64_t;
#endif
#endif
  /* compiler/cpu type ... or just ISO C99 */
#endif
#endif
EOF

# plus a default 64-bit for systems that are likely to be 64-bit ready
  case "$ac_cv_sizeof_x:$ac_cv_sizeof_voidp:$ac_cv_sizeof_long" in
    1:2:8:8) AC_MSG_RESULT(..adding uint64_t default, normal 64-bit system)
cat >>$ac_stdint_h <<EOF
/* DEFAULT: */
/* seen normal 64-bit system, CC has sizeof(long and void*) == 8 bytes */
#ifndef _HAVE_UINT64_T
#define _HAVE_UINT64_T
typedef long int64_t;
typedef unsigned long uint64_t;
#endif
EOF
;;
    1:2:4:8) AC_MSG_RESULT(..adding uint64_t default, typedef to long)
cat >>$ac_stdint_h <<EOF
/* DEFAULT: */
/* seen 32-bit system prepared for 64-bit, CC has sizeof(long) == 8 bytes */
#ifndef _HAVE_UINT64_T
#define _HAVE_UINT64_T
typedef long int64_t;
typedef unsigned long uint64_t;
#endif
EOF
;;
    1:2:8:4) AC_MSG_RESULT(..adding uint64_t default, typedef long long)
cat >>$ac_stdint_h <<EOF
/* DEFAULT: */
/* seen 64-bit derived from a 32-bit, CC has sizeof(long) == 4 bytes */
#ifndef _HAVE_UINT64_T
#define _HAVE_UINT64_T
typedef long long int64_t;
typedef unsigned long long uint64_t;
#endif
EOF
;;
   *)
cat >>$ac_stdint_h <<EOF
/* NOTE: */
/* the configure-checks for the basic types did not make us believe */
/* that we could add a fallback to a 'long long' typedef to int64_t */
EOF
  esac
fi

# ------------- DONE int64_t types START intptr types ------------
if test "$ac_cv_header_stdint_x" = "no-file" ; then
  cat >>$ac_stdint_h <<EOF

/* -------------------------- INPTR SECTION --------------------------- */
EOF
  case "$ac_cv_sizeof_x:$ac_cv_sizeof_voidp" in
  1:2:2)
    a="int16_t" ; cat >>$ac_stdint_h <<EOF
/* we tested sizeof(void*) to be of 2 chars, hence we declare it 16-bit */

typedef uint16_t uintptr_t;
typedef  int16_t  intptr_t;
EOF
;;
  1:2:4)
    a="int32_t" ; cat >>$ac_stdint_h <<EOF
/* we tested sizeof(void*) to be of 4 chars, hence we declare it 32-bit */

typedef uint32_t uintptr_t;
typedef  int32_t  intptr_t;
EOF
;;
  1:2:8)
    a="int64_t" ; cat >>$ac_stdint_h <<EOF
/* we tested sizeof(void*) to be of 8 chars, hence we declare it 64-bit */

typedef uint64_t uintptr_t;
typedef  int64_t  intptr_t;
EOF
;;
  *)
    a="long" ; cat >>$ac_stdint_h <<EOF
/* we tested sizeof(void*) but got no guess, hence we declare it as if long */

typedef unsigned long uintptr_t;
typedef          long  intptr_t;
EOF
;;
  esac
AC_MSG_RESULT(..adding typedef $a intptr_t)
fi

# ------------- DONE intptr types START int_least types ------------
if test "$ac_cv_type_int_least32_t" = "no"; then
AC_MSG_RESULT(..adding generic int_least-types)
     cat >>$ac_stdint_h <<EOF

/* --------------GENERIC INT_LEAST ------------------ */

typedef  int8_t    int_least8_t;
typedef  int16_t   int_least16_t;
typedef  int32_t   int_least32_t;
#ifdef _HAVE_INT64_T
typedef  int64_t   int_least64_t;
#endif

typedef uint8_t   uint_least8_t;
typedef uint16_t  uint_least16_t;
typedef uint32_t  uint_least32_t;
#ifdef _HAVE_INT64_T
typedef uint64_t  uint_least64_t;
#endif
EOF
fi

# ------------- DONE intptr types START int_least types ------------
if test "$ac_cv_type_int_fast32_t" = "no"; then
AC_MSG_RESULT(..adding generic int_fast-types)
     cat >>$ac_stdint_h <<EOF

/* --------------GENERIC INT_FAST ------------------ */

typedef  int8_t    int_fast8_t;
typedef  int32_t   int_fast16_t;
typedef  int32_t   int_fast32_t;
#ifdef _HAVE_INT64_T
typedef  int64_t   int_fast64_t;
#endif

typedef uint8_t   uint_fast8_t;
typedef uint32_t  uint_fast16_t;
typedef uint32_t  uint_fast32_t;
#ifdef _HAVE_INT64_T
typedef uint64_t  uint_fast64_t;
#endif
EOF
fi

if test "$ac_cv_header_stdint_x" = "no-file" ; then
     cat >>$ac_stdint_h <<EOF

#ifdef _HAVE_INT64_T
typedef int64_t        intmax_t;
typedef uint64_t      uintmax_t;
#else
typedef long int       intmax_t;
typedef unsigned long uintmax_t;
#endif
EOF
fi

AC_MSG_RESULT(... DONE $ac_stdint_h)
   cat >>$ac_stdint_h <<EOF

  /* once */
#endif
#endif
EOF
])

dnl quote from SunOS-5.8 sys/inttypes.h:
dnl Use at your own risk.  As of February 1996, the committee is squarely
dnl behind the fixed sized types; the "least" and "fast" types are still being
dnl discussed.  The probability that the "fast" types may be removed before
dnl the standard is finalized is high enough that they are not currently
dnl implemented.

# AM_INIT_AUTOMAKE(PACKAGE,VERSION, [NO-DEFINE])
# ----------------------------------------------
AC_DEFUN([AM_INIT_AUTOMAKE],
[AC_REQUIRE([AC_PROG_INSTALL])dnl
# test to see if srcdir already configured
if test "`CDPATH=:; cd $srcdir && pwd`" != "`pwd`" &&
   test -f $srcdir/config.status; then
  AC_MSG_ERROR([source directory already configured; run \"make distclean\" there first])
fi

# Define the identity of the package.
PACKAGE=$1
AC_SUBST(PACKAGE)dnl
VERSION=$2
AC_SUBST(VERSION)dnl
ifelse([$3],,
[AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE", [Name of package])
AC_DEFINE_UNQUOTED(VERSION, "$VERSION", [Version number of package])])

# Autoconf 2.50 wants to disallow AM_ names.  We explicitly allow
# the ones we care about.
ifdef([m4_pattern_allow],
      [m4_pattern_allow([^AM_[A-Z]+FLAGS])])dnl

# Autoconf 2.50 always computes EXEEXT.  However we need to be
# compatible with 2.13, for now.  So we always define EXEEXT, but we
# don't compute it.
AC_SUBST(EXEEXT)
# Similar for OBJEXT -- only we only use OBJEXT if the user actually
# requests that it be used.  This is a bit dumb.
: ${OBJEXT=o}
AC_SUBST(OBJEXT)

# Some tools Automake needs.
AC_REQUIRE([AM_SANITY_CHECK])dnl
AC_REQUIRE([AC_ARG_PROGRAM])dnl
AM_MISSING_PROG(ACLOCAL, aclocal)
AM_MISSING_PROG(AUTOCONF, autoconf)
AM_MISSING_PROG(AUTOMAKE, automake)
AM_MISSING_PROG(AUTOHEADER, autoheader)
AM_MISSING_PROG(MAKEINFO, makeinfo)
AM_MISSING_PROG(AMTAR, tar)
AM_PROG_INSTALL_SH
AM_PROG_INSTALL_STRIP
# We need awk for the "check" target.  The system "awk" is bad on
# some platforms.
AC_REQUIRE([AC_PROG_AWK])dnl
AC_REQUIRE([AC_PROG_MAKE_SET])dnl
AC_REQUIRE([AM_DEP_TRACK])dnl
AC_REQUIRE([AM_SET_DEPDIR])dnl
AC_PROVIDE_IFELSE([AC_PROG_][CC],
                  [_AM_DEPENDENCIES(CC)],
                  [define([AC_PROG_][CC],
                          defn([AC_PROG_][CC])[_AM_DEPENDENCIES(CC)])])dnl
AC_PROVIDE_IFELSE([AC_PROG_][CXX],
                  [_AM_DEPENDENCIES(CXX)],
                  [define([AC_PROG_][CXX],
                          defn([AC_PROG_][CXX])[_AM_DEPENDENCIES(CXX)])])dnl
])

# AM_CONDITIONAL(NAME, SHELL-CONDITION)
# -------------------------------------
# Define a conditional.
#
# FIXME: Once using 2.50, use this:
# m4_match([$1], [^TRUE\|FALSE$], [AC_FATAL([$0: invalid condition: $1])])dnl
AC_DEFUN([AM_CONDITIONAL],
[ifelse([$1], [TRUE],
        [errprint(__file__:__line__: [$0: invalid condition: $1
])dnl
m4exit(1)])dnl
ifelse([$1], [FALSE],
       [errprint(__file__:__line__: [$0: invalid condition: $1
])dnl
m4exit(1)])dnl
AC_SUBST([$1_TRUE])
AC_SUBST([$1_FALSE])
if $2; then
  $1_TRUE=
  $1_FALSE='#'
else
  $1_TRUE='#'
  $1_FALSE=
fi])

# Like AC_CONFIG_HEADER, but automatically create stamp file.

# serial 3

# When config.status generates a header, we must update the stamp-h file.
# This file resides in the same directory as the config header
# that is generated.  We must strip everything past the first ":",
# and everything past the last "/".

AC_PREREQ([2.12])

AC_DEFUN([AM_CONFIG_HEADER],
[ifdef([AC_FOREACH],dnl
	 [dnl init our file count if it isn't already
	 m4_ifndef([_AM_Config_Header_Index], m4_define([_AM_Config_Header_Index], [0]))
	 dnl prepare to store our destination file list for use in config.status
	 AC_FOREACH([_AM_File], [$1],
		    [m4_pushdef([_AM_Dest], m4_patsubst(_AM_File, [:.*]))
		    m4_define([_AM_Config_Header_Index], m4_incr(_AM_Config_Header_Index))
		    dnl and add it to the list of files AC keeps track of, along
		    dnl with our hook
		    AC_CONFIG_HEADERS(_AM_File,
dnl COMMANDS, [, INIT-CMDS]
[# update the timestamp
echo timestamp >"AS_ESCAPE(_AM_DIRNAME(]_AM_Dest[))/stamp-h]_AM_Config_Header_Index["
][$2]m4_ifval([$3], [, [$3]]))dnl AC_CONFIG_HEADERS
		    m4_popdef([_AM_Dest])])],dnl
[AC_CONFIG_HEADER([$1])
  AC_OUTPUT_COMMANDS(
   ifelse(patsubst([$1], [[^ ]], []),
	  [],
	  [test -z "$CONFIG_HEADERS" || echo timestamp >dnl
	   patsubst([$1], [^\([^:]*/\)?.*], [\1])stamp-h]),dnl
[am_indx=1
for am_file in $1; do
  case " \$CONFIG_HEADERS " in
  *" \$am_file "*)
    am_dir=\`echo \$am_file |sed 's%:.*%%;s%[^/]*\$%%'\`
    if test -n "\$am_dir"; then
      am_tmpdir=\`echo \$am_dir |sed 's%^\(/*\).*\$%\1%'\`
      for am_subdir in \`echo \$am_dir |sed 's%/% %'\`; do
        am_tmpdir=\$am_tmpdir\$am_subdir/
        if test ! -d \$am_tmpdir; then
          mkdir \$am_tmpdir
        fi
      done
    fi
    echo timestamp > "\$am_dir"stamp-h\$am_indx
    ;;
  esac
  am_indx=\`expr \$am_indx + 1\`
done])
])]) # AM_CONFIG_HEADER

