/*
 * $XConsortium: Xos.h /main/70 1996/11/15 16:00:41 kaleb $
 * $XFree86: xc/include/Xos.h,v 3.21.2.1 1998/01/23 12:35:11 dawes Exp $
 * 
 * 
Copyright (c) 1987  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.
 *
 * The X Window System is a Trademark of X Consortium.
 *
 */

/* This is a collection of things to try and minimize system dependencies
 * in a "signficant" number of source files.
 */

#ifndef _XOS_H_
#define _XOS_H_

#include <X11/Xosdefs.h>

/*
 * Get major data types (esp. caddr_t)
 */

#ifdef USG
#ifndef __TYPES__
#ifdef CRAY
#define word word_t
#endif /* CRAY */
#include <sys/types.h>			/* forgot to protect it... */
#define __TYPES__
#endif /* __TYPES__ */
#else /* USG */
#if defined(_POSIX_SOURCE) && (defined(MOTOROLA) || defined(AMOEBA))
#undef _POSIX_SOURCE
#include <sys/types.h>
#define _POSIX_SOURCE
#else
#include <sys/types.h>
#endif
#endif /* USG */


/*
 * Just about everyone needs the strings routines.  We provide both forms here,
 * index/rindex and strchr/strrchr, so any systems that don't provide them all
 * need to have #defines here.
 *
 * NOTE: The following ISN'T true for this XFree86 version of this file.
 *
 * These macros are defined this way, rather than, e.g.:
 *    #defined index(s,c) strchr(s,c)
 * because someone might be using them as function pointers, and such
 * a change would break compatibility for anyone who's relying on them
 * being the way they currently are. So we're stuck with them this way,
 * which can be really inconvenient. :-(
 */

#ifndef X_NOT_STDC_ENV

#include <string.h>
#ifdef __STDC__
#ifndef index
#define index(s,c) (strchr((s),(c)))
#endif
#ifndef rindex
#define rindex(s,c) (strrchr((s),(c)))
#endif
#else
#ifndef index
#define index strchr
#endif
#ifndef rindex
#define rindex strrchr
#endif
#endif

#else

#ifdef SYSV
#if defined(clipper) || defined(__clipper__)
#include <malloc.h>
#endif
#include <string.h>
#define index strchr
#define rindex strrchr
#else
#include <strings.h>
#define strchr index
#define strrchr rindex
#endif

#endif /* X_NOT_STDC_ENV */

/*
 * strerror()
 */
#if (defined(X_NOT_STDC_ENV) || (defined(sun) && !defined(SVR4)) || defined(macII)) && !defined(__GLIBC__)
#ifndef strerror
extern char *sys_errlist[];
extern int sys_nerr;
#define strerror(n) \
    (((n) >= 0 && (n) < sys_nerr) ? sys_errlist[n] : "unknown error")
#endif
#endif

/*
 * Get open(2) constants
 */
#ifdef X_NOT_POSIX
#include <fcntl.h>
#if defined(USL) || defined(CRAY) || defined(MOTOROLA) || (defined(i386) && (defined(SYSV) || defined(SVR4))) || defined(__sxg__)
#include <unistd.h>
#endif
#ifdef WIN32
#include <X11/Xw32defs.h>
#else
#include <sys/file.h>
#endif
#else /* X_NOT_POSIX */
#if !defined(_POSIX_SOURCE) && defined(macII)
#define _POSIX_SOURCE
#include <fcntl.h>
#include <unistd.h>
#undef _POSIX_SOURCE
#else
#include <fcntl.h>
#include <unistd.h>
#endif
#endif /* X_NOT_POSIX else */

#ifdef CSRG_BASED
#include <stdlib.h>
#include <unistd.h>
#endif /* CSRG_BASED */

/*
 * Get struct timeval
 */

#ifdef SYSV

#ifndef USL
#include <sys/time.h>
#endif
#include <time.h>
#ifdef CRAY
#undef word
#endif /* CRAY */
#if defined(USG) && !defined(CRAY) && !defined(MOTOROLA) && !defined(uniosu) && !defined(__sxg__) && !defined(clipper) && !defined(__clipper__)
struct timeval {
    long tv_sec;
    long tv_usec;
};
#ifndef USL_SHARELIB
struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};
#endif /* USL_SHARELIB */
#endif /* USG */

#ifdef _SEQUENT_
struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};
#endif /* _SEQUENT_ */

#else /* not SYSV */

#if defined(_ANSI_SOURCE) && defined(__bsdi__)
#undef _ANSI_SOURCE
#include <sys/time.h>
#define _ANSI_SOURCE
#endif

#if defined(_POSIX_SOURCE) && defined(SVR4)
/* need to omit _POSIX_SOURCE in order to get what we want in SVR4 */
#undef _POSIX_SOURCE
#include <sys/time.h>
#define _POSIX_SOURCE
#else /* defined(_POSIX_SOURCE) && defined(SVR4) */
#ifdef WIN32
#include <time.h>
#if !defined(_WINSOCKAPI_) && !defined(_WILLWINSOCK_)
struct timeval {
    long    tv_sec;         /* seconds */
    long    tv_usec;        /* and microseconds */
};
#endif
#include <sys/timeb.h>
#define gettimeofday(t) \
{ \
    struct _timeb _gtodtmp; \
    _ftime (&_gtodtmp); \
    (t)->tv_sec = _gtodtmp.time; \
    (t)->tv_usec = _gtodtmp.millitm * 1000; \
}
#else /* WIN32 */
#ifdef _SEQUENT_
#include <time.h>
#else /* _SEQUENT_ */
#ifdef AMOEBA
#include <time.h>
#include <sys/time.h>
#else /* AMOEBA */
#ifdef MINIX
#include <time.h>
#else /* !MINIX */
#ifndef Lynx
#include <sys/time.h>
#else
#include <time.h>
#endif /* Lynx */
#endif /* MINIX */
#endif /* AMOEBA */
#endif /* _SEQUENT_ */
#endif /* WIN32 else */
#endif /* defined(_POSIX_SOURCE) && defined(SVR4) */

#endif /* SYSV */

/* define X_GETTIMEOFDAY macro, a portable gettimeofday() */
#if defined(_XOPEN_XPG4) || defined(_XOPEN_UNIX) /* _XOPEN_UNIX is XPG4.2 */
#define X_GETTIMEOFDAY(t) gettimeofday(t, (struct timezone*)0)
#else
#if defined(SVR4) || defined(VMS) || defined(WIN32)
#define X_GETTIMEOFDAY(t) gettimeofday(t)
#else
#define X_GETTIMEOFDAY(t) gettimeofday(t, (struct timezone*)0)
#endif
#endif /* XPG4 else */

#ifdef MINIX
#include <errno.h>
#include <net/gen/in.h>
#include <net/gen/socket.h>
#include <net/gen/udp.h>
#include <net/gen/udp_hdr.h>

struct sockaddr
{
	u16_t sa_family;
	char sa_data[14];
};

struct sockaddr_in
{
	u16_t sin_family;
	u16_t sin_port;
	struct
	{
		ipaddr_t s_addr;
	} sin_addr;
	char sin_zero[8];
};

struct in_addr
{
	ipaddr_t s_addr;
};

typedef char *caddr_t;
typedef unsigned char u_char;
#endif /* MINIX */

#ifdef __EMX__
typedef unsigned long fd_mask;
#endif

/* use POSIX name for signal */
#if defined(X_NOT_POSIX) && defined(SYSV) && !defined(SIGCHLD) && !defined(ISC)
#define SIGCHLD SIGCLD
#endif

#ifdef ISC
#include <sys/bsdtypes.h>
#include <sys/limits.h>
#define NGROUPS 16
#endif

#if defined(ISC) || defined(__EMX__)
/*
 *	Some OS's may not have this
 */

#define X_NO_SYS_UN 1

struct sockaddr_un {
	short	sun_family;
	char	sun_path[108];
};
#endif

#endif /* _XOS_H_ */
