/* $XConsortium: regionstr.h,v 1.8 94/04/17 20:26:01 dpw Exp $ */
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
#ifndef REGIONSTRUCT_H
#define REGIONSTRUCT_H

#include "miscstruct.h"

/* Return values from RectIn() */

#define rgnOUT 0
#define rgnIN  1
#define rgnPART 2

#define NullRegion ((RegionPtr)0)

/* 
 *   clip region
 */

typedef struct _RegData {
    long	size;
    long 	numRects;
/*  BoxRec	rects[size];   in memory but not explicitly declared */
} RegDataRec, *RegDataPtr;

typedef struct _Region {
    BoxRec 	extents;
    RegDataPtr	data;
} RegionRec, *RegionPtr;

extern BoxRec miEmptyBox;
extern RegDataRec miEmptyData;

#define REGION_NIL(reg) ((reg)->data && !(reg)->data->numRects)
#define REGION_NUM_RECTS(reg) ((reg)->data ? (reg)->data->numRects : 1)
#define REGION_SIZE(reg) ((reg)->data ? (reg)->data->size : 0)
#define REGION_RECTS(reg) ((reg)->data ? (BoxPtr)((reg)->data + 1) \
			               : &(reg)->extents)
#define REGION_BOXPTR(reg) ((BoxPtr)((reg)->data + 1))
#define REGION_BOX(reg,i) (&REGION_BOXPTR(reg)[i])
#define REGION_TOP(reg) REGION_BOX(reg, (reg)->data->numRects)
#define REGION_END(reg) REGION_BOX(reg, (reg)->data->numRects - 1)
#define REGION_SZOF(n) (sizeof(RegDataRec) + ((n) * sizeof(BoxRec)))

#ifdef NEED_SCREEN_REGIONS

#define REGION_CREATE(_pScreen, _rect, _size) \
    (*(_pScreen)->RegionCreate)(_rect, _size)

#define REGION_INIT(_pScreen, _pReg, _rect, _size) \
    (*(_pScreen)->RegionInit)(_pReg, _rect, _size)

#define REGION_COPY(_pScreen, dst, src) \
    (*(_pScreen)->RegionCopy)(dst, src)

#define REGION_DESTROY(_pScreen, _pReg) \
    (*(_pScreen)->RegionDestroy)(_pReg)

#define REGION_UNINIT(_pScreen, _pReg) \
    (*(_pScreen)->RegionUninit)(_pReg)

#define REGION_INTERSECT(_pScreen, newReg, reg1, reg2) \
    (*(_pScreen)->Intersect)(newReg, reg1, reg2)

#define REGION_UNION(_pScreen, newReg, reg1, reg2) \
    (*(_pScreen)->Union)(newReg, reg1, reg2)

#define REGION_SUBTRACT(_pScreen, newReg, reg1, reg2) \
    (*(_pScreen)->Subtract)(newReg, reg1, reg2)

#define REGION_INVERSE(_pScreen, newReg, reg1, invRect) \
    (*(_pScreen)->Inverse)(newReg, reg1, invRect)

#define REGION_RESET(_pScreen, _pReg, _pBox) \
    (*(_pScreen)->RegionReset)(_pReg, _pBox)

#define REGION_TRANSLATE(_pScreen, _pReg, _x, _y) \
    (*(_pScreen)->TranslateRegion)(_pReg, _x, _y)

#define RECT_IN_REGION(_pScreen, _pReg, prect) \
    (*(_pScreen)->RectIn)(_pReg, prect)

#define POINT_IN_REGION(_pScreen, _pReg, _x, _y, prect) \
    (*(_pScreen)->PointInRegion)(_pReg, _x, _y, prect)

#define REGION_NOTEMPTY(_pScreen, _pReg) \
    (*(_pScreen)->RegionNotEmpty)(_pReg)

#define REGION_EMPTY(_pScreen, _pReg) \
    (*(_pScreen)->RegionEmpty)(_pReg)

#define REGION_EXTENTS(_pScreen, _pReg) \
    (*(_pScreen)->RegionExtents)(_pReg)

#define REGION_APPEND(_pScreen, dstrgn, rgn) \
    (*(_pScreen)->RegionAppend)(dstrgn, rgn)

#define REGION_VALIDATE(_pScreen, badreg, pOverlap) \
    (*(_pScreen)->RegionValidate)(badreg, pOverlap)

#define BITMAP_TO_REGION(_pScreen, pPix) \
    (*(_pScreen)->BitmapToRegion)(pPix)

#define RECTS_TO_REGION(_pScreen, nrects, prect, ctype) \
    (*(_pScreen)->RectsToRegion)(nrects, prect, ctype)

#else /* !NEED_SCREEN_REGIONS */

#define REGION_CREATE(_pScreen, _rect, _size) \
    miRegionCreate(_rect, _size)

#define REGION_COPY(_pScreen, dst, src) \
    miRegionCopy(dst, src)

#define REGION_DESTROY(_pScreen, _pReg) \
    miRegionDestroy(_pReg)

#define REGION_INTERSECT(_pScreen, newReg, reg1, reg2) \
    miIntersect(newReg, reg1, reg2)

#define REGION_UNION(_pScreen, newReg, reg1, reg2) \
    miUnion(newReg, reg1, reg2)

#define REGION_SUBTRACT(_pScreen, newReg, reg1, reg2) \
    miSubtract(newReg, reg1, reg2)

#define REGION_INVERSE(_pScreen, newReg, reg1, invRect) \
    miInverse(newReg, reg1, invRect)

#define REGION_TRANSLATE(_pScreen, _pReg, _x, _y) \
    miTranslateRegion(_pReg, _x, _y)

#define RECT_IN_REGION(_pScreen, _pReg, prect) \
    miRectIn(_pReg, prect)

#define POINT_IN_REGION(_pScreen, _pReg, _x, _y, prect) \
    miPointInRegion(_pReg, _x, _y, prect)

#define REGION_APPEND(_pScreen, dstrgn, rgn) \
    miRegionAppend(dstrgn, rgn)

#define REGION_VALIDATE(_pScreen, badreg, pOverlap) \
    miRegionValidate(badreg, pOverlap)

#define BITMAP_TO_REGION(_pScreen, pPix) \
    (*(_pScreen)->BitmapToRegion)(pPix) /* no mi version?! */

#define RECTS_TO_REGION(_pScreen, nrects, prect, ctype) \
    miRectsToRegion(nrects, prect, ctype)

#ifdef DONT_INLINE_REGION_OPS

#define REGION_INIT(_pScreen, _pReg, _rect, _size) \
    miRegionInit(_pReg, _rect, _size)

#define REGION_UNINIT(_pScreen, _pReg) \
    miRegionUninit(_pReg)

#define REGION_RESET(_pScreen, _pReg, _pBox) \
    miRegionReset(_pReg, _pBox)

#define REGION_NOTEMPTY(_pScreen, _pReg) \
    miRegionNotEmpty(_pReg)

#define REGION_EMPTY(_pScreen, _pReg) \
    miRegionEmpty(_pReg)

#define REGION_EXTENTS(_pScreen, _pReg) \
    miRegionExtents(_pReg)

#else /* inline certain simple region ops for performance */

#define REGION_INIT(_pScreen, _pReg, _rect, _size) \
{ \
    if (_rect) \
    { \
	(_pReg)->extents = *(_rect); \
	(_pReg)->data = (RegDataPtr)NULL; \
    } \
    else \
    { \
	(_pReg)->extents = miEmptyBox; \
	if (((_size) > 1) && ((_pReg)->data = \
			     (RegDataPtr)xalloc(REGION_SZOF(_size)))) \
	{ \
	    (_pReg)->data->size = (_size); \
	    (_pReg)->data->numRects = 0; \
	} \
	else \
	    (_pReg)->data = &miEmptyData; \
    } \
}

#define REGION_UNINIT(_pScreen, _pReg) \
{ \
    if ((_pReg)->data && (_pReg)->data->size) xfree((_pReg)->data); \
}

#define REGION_RESET(_pScreen, _pReg, _pBox) \
{ \
    (_pReg)->extents = *(_pBox); \
    REGION_UNINIT(_pScreen, _pReg); \
    (_pReg)->data = (RegDataPtr)NULL; \
}

#define REGION_NOTEMPTY(_pScreen, _pReg) \
    !REGION_NIL(_pReg)

#define REGION_EMPTY(_pScreen, _pReg) \
{ \
    REGION_UNINIT(_pScreen, _pReg); \
    (_pReg)->extents.x2 = (_pReg)->extents.x1; \
    (_pReg)->extents.y2 = (_pReg)->extents.y1; \
    (_pReg)->data = &miEmptyData; \
}

#define REGION_EXTENTS(_pScreen, _pReg) \
    &(_pReg)->extents

#endif /* DONT_INLINE_REGION_OPS */

#endif /* NEED_SCREEN_REGIONS */

/* moved from mi.h */

extern RegionPtr miRegionCreate(
#if NeedFunctionPrototypes
    BoxPtr /*rect*/,
    int /*size*/
#endif
);

extern void miRegionInit(
#if NeedFunctionPrototypes
    RegionPtr /*pReg*/,
    BoxPtr /*rect*/,
    int /*size*/
#endif
);

extern void miRegionDestroy(
#if NeedFunctionPrototypes
    RegionPtr /*pReg*/
#endif
);

extern void miRegionUninit(
#if NeedFunctionPrototypes
    RegionPtr /*pReg*/
#endif
);

extern Bool miRegionCopy(
#if NeedFunctionPrototypes
    RegionPtr /*dst*/,
    RegionPtr /*src*/
#endif
);

extern Bool miIntersect(
#if NeedFunctionPrototypes
    RegionPtr /*newReg*/,
    RegionPtr /*reg1*/,
    RegionPtr /*reg2*/
#endif
);

extern Bool miUnion(
#if NeedFunctionPrototypes
    RegionPtr /*newReg*/,
    RegionPtr /*reg1*/,
    RegionPtr /*reg2*/
#endif
);

extern Bool miRegionAppend(
#if NeedFunctionPrototypes
    RegionPtr /*dstrgn*/,
    RegionPtr /*rgn*/
#endif
);

extern Bool miRegionValidate(
#if NeedFunctionPrototypes
    RegionPtr /*badreg*/,
    Bool * /*pOverlap*/
#endif
);

extern RegionPtr miRectsToRegion(
#if NeedFunctionPrototypes
    int /*nrects*/,
    xRectanglePtr /*prect*/,
    int /*ctype*/
#endif
);

extern Bool miSubtract(
#if NeedFunctionPrototypes
    RegionPtr /*regD*/,
    RegionPtr /*regM*/,
    RegionPtr /*regS*/
#endif
);

extern Bool miInverse(
#if NeedFunctionPrototypes
    RegionPtr /*newReg*/,
    RegionPtr /*reg1*/,
    BoxPtr /*invRect*/
#endif
);

extern int miRectIn(
#if NeedFunctionPrototypes
    RegionPtr /*region*/,
    BoxPtr /*prect*/
#endif
);

extern void miTranslateRegion(
#if NeedFunctionPrototypes
    RegionPtr /*pReg*/,
    int /*x*/,
    int /*y*/
#endif
);

extern void miRegionReset(
#if NeedFunctionPrototypes
    RegionPtr /*pReg*/,
    BoxPtr /*pBox*/
#endif
);

extern Bool miPointInRegion(
#if NeedFunctionPrototypes
    RegionPtr /*pReg*/,
    int /*x*/,
    int /*y*/,
    BoxPtr /*box*/
#endif
);

extern Bool miRegionNotEmpty(
#if NeedFunctionPrototypes
    RegionPtr /*pReg*/
#endif
);

extern void miRegionEmpty(
#if NeedFunctionPrototypes
    RegionPtr /*pReg*/
#endif
);

extern BoxPtr miRegionExtents(
#if NeedFunctionPrototypes
    RegionPtr /*pReg*/
#endif
);

#endif /* REGIONSTRUCT_H */
