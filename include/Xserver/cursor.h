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
/* $XConsortium: cursor.h,v 1.22 94/04/17 20:25:34 dpw Exp $ */
#ifndef CURSOR_H
#define CURSOR_H 

#include "misc.h"
#include "screenint.h"
#include "window.h"

#define NullCursor ((CursorPtr)NULL)

typedef struct _Cursor *CursorPtr;
typedef struct _CursorMetric *CursorMetricPtr;

extern CursorPtr rootCursor;

extern int FreeCursor(
#if NeedFunctionPrototypes
    pointer /*pCurs*/,
    XID /*cid*/
#endif
);

extern CursorPtr X11AllocCursor(
#if NeedFunctionPrototypes
    unsigned char* /*psrcbits*/,
    unsigned char* /*pmaskbits*/,
    CursorMetricPtr /*cm*/,
    unsigned /*foreRed*/,
    unsigned /*foreGreen*/,
    unsigned /*foreBlue*/,
    unsigned /*backRed*/,
    unsigned /*backGreen*/,
    unsigned /*backBlue*/
#endif
);

extern int AllocGlyphCursor(
#if NeedFunctionPrototypes
    Font /*source*/,
    unsigned int /*sourceChar*/,
    Font /*mask*/,
    unsigned int /*maskChar*/,
    unsigned /*foreRed*/,
    unsigned /*foreGreen*/,
    unsigned /*foreBlue*/,
    unsigned /*backRed*/,
    unsigned /*backGreen*/,
    unsigned /*backBlue*/,
    CursorPtr* /*ppCurs*/,
    ClientPtr /*client*/
#endif
);

extern CursorPtr CreateRootCursor(
#if NeedFunctionPrototypes
    char* /*pfilename*/,
    unsigned int /*glyph*/
#endif
);

extern int ServerBitsFromGlyph(
#if NeedFunctionPrototypes
    FontPtr /*pfont*/,
    unsigned int /*ch*/,
    register CursorMetricPtr /*cm*/,
    unsigned char ** /*ppbits*/
#endif
);

extern Bool CursorMetricsFromGlyph(
#if NeedFunctionPrototypes
    FontPtr /*pfont*/,
    unsigned /*ch*/,
    CursorMetricPtr /*cm*/
#endif
);

extern void CheckCursorConfinement(
#if NeedFunctionPrototypes
    X11WindowPtr /*pWin*/
#endif
);

extern void NewCurrentScreen(
#if NeedFunctionPrototypes
    ScreenPtr /*newScreen*/,
    int /*x*/,
    int /*y*/
#endif
);

extern Bool PointerConfinedToScreen(
#if NeedFunctionPrototypes
    void
#endif
);

extern void GetSpritePosition(
#if NeedFunctionPrototypes
    int * /*px*/,
    int * /*py*/
#endif
);

#endif /* CURSOR_H */
