/* $XConsortium: pixmap.h,v 5.6 94/04/17 20:25:53 dpw Exp $ */
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
#ifndef PIXMAP_H
#define PIXMAP_H

#include "misc.h"
#include "screenint.h"

/* types for Drawable */
#define DRAWABLE_WINDOW 0
#define DRAWABLE_PIXMAP 1
#define UNDRAWABLE_WINDOW 2
#define DRAWABLE_BUFFER 3

/* flags to PaintWindow() */
#define PW_BACKGROUND 0
#define PW_BORDER 1

#define NullPixmap ((PixmapPtr)0)

typedef struct _Drawable *DrawablePtr;	
typedef struct _Pixmap *PixmapPtr;

typedef union _PixUnion {
    PixmapPtr		pixmap;
    unsigned long	pixel;
} PixUnion;

#define SamePixUnion(a,b,isPixel)\
    ((isPixel) ? (a).pixel == (b).pixel : (a).pixmap == (b).pixmap)

#define EqualPixUnion(as, a, bs, b)				\
    ((as) == (bs) && (SamePixUnion (a, b, as)))

#define OnScreenDrawable(type) \
	((type == DRAWABLE_WINDOW) || (type == DRAWABLE_BUFFER))

#define WindowDrawable(type) \
	((type == DRAWABLE_WINDOW) || (type == UNDRAWABLE_WINDOW))

extern PixmapPtr GetScratchPixmapHeader(
#if NeedFunctionPrototypes
    ScreenPtr /*pScreen*/,
    int /*width*/,
    int /*height*/,
    int /*depth*/,
    int /*bitsPerPixel*/,
    int /*devKind*/,
    pointer /*pPixData*/
#endif
);

extern void FreeScratchPixmapHeader(
#if NeedFunctionPrototypes
    PixmapPtr /*pPixmap*/
#endif
);

extern Bool CreateScratchPixmapsForScreen(
#if NeedFunctionPrototypes
    int /*scrnum*/
#endif
);

extern void FreeScratchPixmapsForScreen(
#if NeedFunctionPrototypes
    int /*scrnum*/
#endif
);

extern PixmapPtr AllocatePixmap(
#if NeedFunctionPrototypes
    ScreenPtr /*pScreen*/,
    int /*pixDataSize*/
#endif
);

#endif /* PIXMAP_H */
