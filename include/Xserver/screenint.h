/* $XConsortium: screenint.h /main/6 1996/06/17 10:55:15 mor $ */
/***********************************************************

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


Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/
/* $XFree86: xc/programs/Xserver/include/screenint.h,v 1.2 1997/01/14 22:22:40 dawes Exp $ */
#ifndef SCREENINT_H
#define SCREENINT_H

#include "misc.h"

typedef struct _PixmapFormat *PixmapFormatPtr;
typedef struct _Visual *VisualPtr;
typedef struct _Depth  *DepthPtr;
typedef struct _Screen *ScreenPtr;

extern void ResetScreenPrivates(
#if NeedFunctionPrototypes
    void
#endif
);

extern int AllocateScreenPrivateIndex(
#if NeedFunctionPrototypes
    void
#endif
);

extern void ResetWindowPrivates(
#if NeedFunctionPrototypes
    void
#endif
);

extern int AllocateWindowPrivateIndex(
#if NeedFunctionPrototypes
    void
#endif
);

extern Bool AllocateWindowPrivate(
#if NeedFunctionPrototypes
    ScreenPtr /* pScreen */,
    int /* index */,
    unsigned /* amount */
#endif
);

extern void ResetGCPrivates(
#if NeedFunctionPrototypes
    void
#endif
);

extern int AllocateGCPrivateIndex(
#if NeedFunctionPrototypes
    void
#endif
);

extern Bool AllocateGCPrivate(
#if NeedFunctionPrototypes
    ScreenPtr /* pScreen */,
    int /* index */,
    unsigned /* amount */
#endif
);

extern int AddScreen(
#if NeedFunctionPrototypes
    Bool (* /*pfnInit*/)(
#if NeedNestedPrototypes
	int /*index*/,
	ScreenPtr /*pScreen*/,
	int /*argc*/,
	char ** /*argv*/
#endif
    ),
    int /*argc*/,
    char** /*argv*/
#endif
);

#ifdef PIXPRIV

extern void ResetPixmapPrivates(
#if NeedFunctionPrototypes
    void
#endif
);

extern int AllocatePixmapPrivateIndex(
#if NeedFunctionPrototypes
    void
#endif
);

extern Bool AllocatePixmapPrivate(
#if NeedFunctionPrototypes
    ScreenPtr /* pScreen */,
    int /* index */,
    unsigned /* amount */
#endif
);

#endif /* PIXPRIV */

extern void ResetColormapPrivates(
#if NeedFunctionPrototypes
    void
#endif
);


typedef struct _ColormapRec *ColormapPtr;
typedef int (*InitCmapPrivFunc)(
#if NeedNestedPrototypes
	ColormapPtr
#endif
);

extern int AllocateColormapPrivateIndex(
#if NeedFunctionPrototypes
    InitCmapPrivFunc /* initPrivFunc */
#endif
);

#endif /* SCREENINT_H */
