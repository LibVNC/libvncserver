/* XALLOC -- X's internal memory allocator.  Why does it return unsigned
 * long * instead of the more common char *?  Well, if you read K&R you'll
 * see they say that alloc must return a pointer "suitable for conversion"
 * to whatever type you really want.  In a full-blown generic allocator
 * there's no way to solve the alignment problems without potentially
 * wasting lots of space.  But we have a more limited problem. We know
 * we're only ever returning pointers to structures which will have to
 * be long word aligned.  So we are making a stronger guarantee.  It might
 * have made sense to make Xalloc return char * to conform with people's
 * expectations of malloc, but this makes lint happier.
 */

#ifdef WIN32
#include <X11/Xwinsock.h>
#endif
#include "X11/Xos.h"
#include <stdio.h>
#include "Xserver/misc.h"
#include "X11/X.h"
#include "Xserver/input.h"
#include "Xserver/opaque.h"
#ifdef X_POSIX_C_SOURCE
#define _POSIX_C_SOURCE X_POSIX_C_SOURCE
#include <signal.h>
#undef _POSIX_C_SOURCE
#else
#if defined(X_NOT_POSIX) || defined(_POSIX_SOURCE)
#include <signal.h>
#else
#define _POSIX_SOURCE
#include <signal.h>
#undef _POSIX_SOURCE
#endif
#endif
#if !defined(SYSV) && !defined(AMOEBA) && !defined(_MINIX) && !defined(WIN32) && !defined(Lynx)
#include <sys/resource.h>
#endif
#include <time.h>
#include <sys/stat.h>
#include <ctype.h>    /* for isspace */
#if NeedVarargsPrototypes
#include <stdarg.h>
#endif
#ifdef __sgi__
#undef abs
#endif
#include <stdlib.h>

#ifndef INTERNAL_MALLOC

Bool Must_have_memory = FALSE;

unsigned long * 
Xalloc (amount)
    unsigned long amount;
{
#if !defined(__STDC__) && !defined(AMOEBA)
    char                *malloc();
#endif
    register pointer  ptr;
        
    if ((long)amount <= 0) {
        return (unsigned long *)NULL;
    }
    /* aligned extra on long word boundary */
    amount = (amount + (sizeof(long) - 1)) & ~(sizeof(long) - 1);
#ifdef MEMBUG
    if (!Must_have_memory && Memory_fail &&
        ((random() % MEM_FAIL_SCALE) < Memory_fail))
        return (unsigned long *)NULL;
#endif
    if ((ptr = (pointer)malloc(amount))) {
        return (unsigned long *)ptr;
    }
    if (Must_have_memory)
        FatalError("Out of memory");
    return (unsigned long *)NULL;
}

/*****************
 * XNFalloc 
 * "no failure" realloc, alternate interface to Xalloc w/o Must_have_memory
 *****************/

unsigned long *
XNFalloc (amount)
    unsigned long amount;
{
#if !defined(__STDC__) && !defined(AMOEBA)
    char             *malloc();
#endif
    register pointer ptr;

    if ((long)amount <= 0)
    {
        return (unsigned long *)NULL;
    }
    /* aligned extra on long word boundary */
    amount = (amount + (sizeof(long) - 1)) & ~(sizeof(long) - 1);
    ptr = (pointer)malloc(amount);
    if (!ptr)
    {
        FatalError("Out of memory");
    }
    return ((unsigned long *)ptr);
}

/*****************
 * Xcalloc
 *****************/

unsigned long *
Xcalloc (amount)
    unsigned long   amount;
{
    unsigned long   *ret;

    ret = Xalloc (amount);
    if (ret)
        bzero ((char *) ret, (int) amount);
    return ret;
}

/*****************
 * Xrealloc
 *****************/

unsigned long *
Xrealloc (ptr, amount)
    register pointer ptr;
    unsigned long amount;
{
#if !defined(__STDC__) && !defined(AMOEBA)
    char *malloc();
    char *realloc();
#endif

#ifdef MEMBUG
    if (!Must_have_memory && Memory_fail &&
        ((random() % MEM_FAIL_SCALE) < Memory_fail))
        return (unsigned long *)NULL;
#endif
    if ((long)amount <= 0)
    {
        if (ptr && !amount)
            free(ptr);
        return (unsigned long *)NULL;
    }
    amount = (amount + (sizeof(long) - 1)) & ~(sizeof(long) - 1);
    if (ptr)
        ptr = (pointer)realloc((char *)ptr, amount);
    else
        ptr = (pointer)malloc(amount);
    if (ptr)
        return (unsigned long *)ptr;
    if (Must_have_memory)
        FatalError("Out of memory");
    return (unsigned long *)NULL;
}
                    
/*****************
 * XNFrealloc 
 * "no failure" realloc, alternate interface to Xrealloc w/o Must_have_memory
 *****************/

unsigned long *
XNFrealloc (ptr, amount)
    register pointer ptr;
    unsigned long amount;
{
    if (( ptr = (pointer)Xrealloc( ptr, amount ) ) == NULL)
    {
        FatalError( "Out of memory" );
    }
    return ((unsigned long *)ptr);
}

/*****************
 *  Xfree
 *    calls free 
 *****************/    

void
Xfree(ptr)
    register pointer ptr;
{
    if (ptr)
        free((char *)ptr); 
}

void
FatalError(char *f, ...)
{
    va_list args;

    fprintf(stderr, "\nFatal server error:\n");
    va_start(args, f);
    vfprintf(stderr, f, args);
    va_end(args);
    fprintf(stderr, "\n");

    exit(1);
}

#endif
