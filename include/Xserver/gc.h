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
/* $XConsortium: gc.h /main/16 1996/08/01 19:18:17 dpw $ */

#ifndef GC_H
#define GC_H 

#include "misc.h"	/* for Bool */
#include "X11/X.h"	/* for GContext, Mask */
#include "X11/Xproto.h"
#include "screenint.h"	/* for ScreenPtr */
#include "pixmap.h"	/* for DrawablePtr */

/* clientClipType field in GC */
#define CT_NONE			0
#define CT_PIXMAP		1
#define CT_REGION		2
#define CT_UNSORTED		6
#define CT_YSORTED		10
#define CT_YXSORTED		14
#define CT_YXBANDED		18

#define GCQREASON_VALIDATE	1
#define GCQREASON_CHANGE	2
#define GCQREASON_COPY_SRC	3
#define GCQREASON_COPY_DST	4
#define GCQREASON_DESTROY	5

#define GC_CHANGE_SERIAL_BIT        (((unsigned long)1)<<31)
#define GC_CALL_VALIDATE_BIT        (1L<<30)
#define GCExtensionInterest   (1L<<29)

#define DRAWABLE_SERIAL_BITS        (~(GC_CHANGE_SERIAL_BIT))

#define MAX_SERIAL_NUM     (1L<<28)

#define NEXT_SERIAL_NUMBER ((++globalSerialNumber) > MAX_SERIAL_NUM ? \
	    (globalSerialNumber  = 1): globalSerialNumber)

typedef struct _GCInterest *GCInterestPtr;
typedef struct _GC    *GCPtr;
typedef struct _GCOps *GCOpsPtr;

extern void ValidateGC(
#if NeedFunctionPrototypes
    DrawablePtr /*pDraw*/,
    GCPtr /*pGC*/
#endif
);

extern int ChangeGC(
#if NeedFunctionPrototypes
    GCPtr/*pGC*/,
    BITS32 /*mask*/,
    XID* /*pval*/
#endif
);

extern int DoChangeGC(
#if NeedFunctionPrototypes
    GCPtr/*pGC*/,
    BITS32 /*mask*/,
    XID* /*pval*/,
    int /*fPointer*/
#endif
);

typedef union {
    CARD32 val;
    pointer ptr;
} ChangeGCVal, *ChangeGCValPtr;

extern int dixChangeGC(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    GCPtr /*pGC*/,
    BITS32 /*mask*/,
    CARD32 * /*pval*/,
    ChangeGCValPtr /*pCGCV*/
#endif
);

extern GCPtr CreateGC(
#if NeedFunctionPrototypes
    DrawablePtr /*pDrawable*/,
    BITS32 /*mask*/,
    XID* /*pval*/,
    int* /*pStatus*/
#endif
);

extern int CopyGC(
#if NeedFunctionPrototypes
    GCPtr/*pgcSrc*/,
    GCPtr/*pgcDst*/,
    BITS32 /*mask*/
#endif
);

extern int FreeGC(
#if NeedFunctionPrototypes
    pointer /*pGC*/,
    XID /*gid*/
#endif
);

extern void SetGCMask(
#if NeedFunctionPrototypes
    GCPtr /*pGC*/,
    Mask /*selectMask*/,
    Mask /*newDataMask*/
#endif
);

extern GCPtr CreateScratchGC(
#if NeedFunctionPrototypes
    ScreenPtr /*pScreen*/,
    unsigned /*depth*/
#endif
);

extern void FreeGCperDepth(
#if NeedFunctionPrototypes
    int /*screenNum*/
#endif
);

extern Bool CreateGCperDepth(
#if NeedFunctionPrototypes
    int /*screenNum*/
#endif
);

extern Bool CreateDefaultStipple(
#if NeedFunctionPrototypes
    int /*screenNum*/
#endif
);

extern void FreeDefaultStipple(
#if NeedFunctionPrototypes
    int /*screenNum*/
#endif
);

extern int SetDashes(
#if NeedFunctionPrototypes
    GCPtr /*pGC*/,
    unsigned /*offset*/,
    unsigned /*ndash*/,
    unsigned char* /*pdash*/
#endif
);

extern int VerifyRectOrder(
#if NeedFunctionPrototypes
    int /*nrects*/,
    xRectangle* /*prects*/,
    int /*ordering*/
#endif
);

extern int SetClipRects(
#if NeedFunctionPrototypes
    GCPtr /*pGC*/,
    int /*xOrigin*/,
    int /*yOrigin*/,
    int /*nrects*/,
    xRectangle* /*prects*/,
    int /*ordering*/
#endif
);

extern GCPtr GetScratchGC(
#if NeedFunctionPrototypes
    unsigned /*depth*/,
    ScreenPtr /*pScreen*/
#endif
);

extern void FreeScratchGC(
#if NeedFunctionPrototypes
    GCPtr /*pGC*/
#endif
);

#endif /* GC_H */
