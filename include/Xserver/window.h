/* $XConsortium: window.h /main/8 1996/03/21 13:35:33 mor $ */
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

#ifndef WINDOW_H
#define WINDOW_H

#include "misc.h"
#include "region.h"
#include "screenint.h"
#include "X11/Xproto.h"

#define TOTALLY_OBSCURED 0
#define UNOBSCURED 1
#define OBSCURED 2

#define VisibilityNotViewable   3

/* return values for tree-walking callback procedures */
#define WT_STOPWALKING          0
#define WT_WALKCHILDREN         1
#define WT_DONTWALKCHILDREN     2
#define WT_NOMATCH 3
#define NullWindow ((X11WindowPtr) 0)

typedef struct _BackingStore *BackingStorePtr;
typedef struct _Window *X11WindowPtr;   /* conflict with CoreGraphics */

typedef int (*VisitWindowProcPtr)(
#if NeedNestedPrototypes
    X11WindowPtr /*pWin*/,
    pointer /*data*/
#endif
);

extern int TraverseTree(
#if NeedFunctionPrototypes
    X11WindowPtr /*pWin*/,
    VisitWindowProcPtr /*func*/,
    pointer /*data*/
#endif
);

extern int WalkTree(
#if NeedFunctionPrototypes
    ScreenPtr /*pScreen*/,
    VisitWindowProcPtr /*func*/,
    pointer /*data*/
#endif
);

extern X11WindowPtr AllocateWindow(
#if NeedFunctionPrototypes
    ScreenPtr /*pScreen*/
#endif
);

extern Bool CreateRootWindow(
#if NeedFunctionPrototypes
    ScreenPtr /*pScreen*/
#endif
);

extern void InitRootWindow(
#if NeedFunctionPrototypes
    X11WindowPtr /*pWin*/
#endif
);

extern void ClippedRegionFromBox(
#if NeedFunctionPrototypes
    X11WindowPtr /*pWin*/,
    RegionPtr /*Rgn*/,
    int /*x*/,
    int /*y*/,
    int /*w*/,
    int /*h*/
#endif
);

extern X11WindowPtr RealChildHead(
#if NeedFunctionPrototypes
    X11WindowPtr /*pWin*/
#endif
);

extern X11WindowPtr CreateWindow(
#if NeedFunctionPrototypes
    Window /*wid*/,
    X11WindowPtr /*pParent*/,
    int /*x*/,
    int /*y*/,
    unsigned int /*w*/,
    unsigned int /*h*/,
    unsigned int /*bw*/,
    unsigned int /*class*/,
    Mask /*vmask*/,
    XID* /*vlist*/,
    int /*depth*/,
    ClientPtr /*client*/,
    VisualID /*visual*/,
    int* /*error*/
#endif
);

extern int DeleteWindow(
#if NeedFunctionPrototypes
    pointer /*pWin*/,
    XID /*wid*/
#endif
);

extern void DestroySubwindows(
#if NeedFunctionPrototypes
    X11WindowPtr /*pWin*/,
    ClientPtr /*client*/
#endif
);

extern int X11ChangeWindowAttributes(
#if NeedFunctionPrototypes
    X11WindowPtr /*pWin*/,
    Mask /*vmask*/,
    XID* /*vlist*/,
    ClientPtr /*client*/
#endif
);

extern void X11GetWindowAttributes(
#if NeedFunctionPrototypes
    X11WindowPtr /*pWin*/,
    ClientPtr /*client*/,
    xGetWindowAttributesReply* /* wa */
#endif
);

extern RegionPtr CreateUnclippedWinSize(
#if NeedFunctionPrototypes
    X11WindowPtr /*pWin*/
#endif
);

extern void GravityTranslate(
#if NeedFunctionPrototypes
    int /*x*/,
    int /*y*/,
    int /*oldx*/,
    int /*oldy*/,
    int /*dw*/,
    int /*dh*/,
    unsigned /*gravity*/,
    int* /*destx*/,
    int* /*desty*/
#endif
);

extern int ConfigureWindow(
#if NeedFunctionPrototypes
    X11WindowPtr /*pWin*/,
    Mask /*mask*/,
    XID* /*vlist*/,
    ClientPtr /*client*/
#endif
);

extern int CirculateWindow(
#if NeedFunctionPrototypes
    X11WindowPtr /*pParent*/,
    int /*direction*/,
    ClientPtr /*client*/
#endif
);

extern int ReparentWindow(
#if NeedFunctionPrototypes
    X11WindowPtr /*pWin*/,
    X11WindowPtr /*pParent*/,
    int /*x*/,
    int /*y*/,
    ClientPtr /*client*/
#endif
);

extern int MapWindow(
#if NeedFunctionPrototypes
    X11WindowPtr /*pWin*/,
    ClientPtr /*client*/
#endif
);

extern void MapSubwindows(
#if NeedFunctionPrototypes
    X11WindowPtr /*pParent*/,
    ClientPtr /*client*/
#endif
);

extern int UnmapWindow(
#if NeedFunctionPrototypes
    X11WindowPtr /*pWin*/,
    Bool /*fromConfigure*/
#endif
);

extern void UnmapSubwindows(
#if NeedFunctionPrototypes
    X11WindowPtr /*pWin*/
#endif
);

extern void HandleSaveSet(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern Bool VisibleBoundingBoxFromPoint(
#if NeedFunctionPrototypes
    X11WindowPtr /*pWin*/,
    int /*x*/,
    int /*y*/,
    BoxPtr /*box*/
#endif
);

extern Bool PointInWindowIsVisible(
#if NeedFunctionPrototypes
    X11WindowPtr /*pWin*/,
    int /*x*/,
    int /*y*/
#endif
);

extern RegionPtr NotClippedByChildren(
#if NeedFunctionPrototypes
    X11WindowPtr /*pWin*/
#endif
);

extern void SendVisibilityNotify(
#if NeedFunctionPrototypes
    X11WindowPtr /*pWin*/
#endif
);

extern void SaveScreens(
#if NeedFunctionPrototypes
    int /*on*/,
    int /*mode*/
#endif
);

extern X11WindowPtr FindWindowWithOptional(
#if NeedFunctionPrototypes
    X11WindowPtr /*w*/
#endif
);

extern void CheckWindowOptionalNeed(
#if NeedFunctionPrototypes
    X11WindowPtr /*w*/
#endif
);

extern Bool MakeWindowOptional(
#if NeedFunctionPrototypes
    X11WindowPtr /*pWin*/
#endif
);

extern void DisposeWindowOptional(
#if NeedFunctionPrototypes
    X11WindowPtr /*pWin*/
#endif
);

extern X11WindowPtr MoveWindowInStack(
#if NeedFunctionPrototypes
    X11WindowPtr /*pWin*/,
    X11WindowPtr /*pNextSib*/
#endif
);

void SetWinSize(
#if NeedFunctionPrototypes
    X11WindowPtr /*pWin*/
#endif
);

void SetBorderSize(
#if NeedFunctionPrototypes
    X11WindowPtr /*pWin*/
#endif
);

void ResizeChildrenWinSize(
#if NeedFunctionPrototypes
    X11WindowPtr /*pWin*/,
    int /*dx*/,
    int /*dy*/,
    int /*dw*/,
    int /*dh*/
#endif
);

#endif /* WINDOW_H */
