/* $XConsortium: input.h /main/22 1996/09/25 00:50:39 dpw $ */
/* $XFree86: xc/programs/Xserver/include/input.h,v 3.4 1996/12/23 07:09:28 dawes Exp $ */
/************************************************************

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

********************************************************/
#ifndef INPUT_H
#define INPUT_H

#include "misc.h"
#include "screenint.h"
#include "X11/Xmd.h"
#include "X11/Xproto.h"
#include "window.h"     /* for WindowPtr */

#define DEVICE_INIT	0
#define DEVICE_ON	1
#define DEVICE_OFF	2
#define DEVICE_CLOSE	3

#define MAP_LENGTH	256
#define DOWN_LENGTH	32	/* 256/8 => number of bytes to hold 256 bits */
#define NullGrab ((GrabPtr)NULL)
#define PointerRootWin ((WindowPtr)PointerRoot)
#define NoneWin ((WindowPtr)None)
#define NullDevice ((DevicePtr)NULL)

#ifndef FollowKeyboard
#define FollowKeyboard 		3
#endif
#ifndef FollowKeyboardWin
#define FollowKeyboardWin  ((WindowPtr) FollowKeyboard)
#endif
#ifndef RevertToFollowKeyboard
#define RevertToFollowKeyboard	3
#endif

typedef unsigned long Leds;
typedef struct _OtherClients *OtherClientsPtr;
typedef struct _InputClients *InputClientsPtr;
typedef struct _DeviceIntRec *DeviceIntPtr;

typedef int (*DeviceProc)(
#if NeedNestedPrototypes
    DeviceIntPtr /*device*/,
    int /*what*/
#endif
);

typedef void (*ProcessInputProc)(
#if NeedNestedPrototypes
    xEventPtr /*events*/,
    DeviceIntPtr /*device*/,
    int /*count*/
#endif
);

typedef struct _DeviceRec {
    pointer	devicePrivate;
    ProcessInputProc processInputProc;	/* current */
    ProcessInputProc realInputProc;	/* deliver */
    ProcessInputProc enqueueInputProc;	/* enqueue */
    Bool	on;			/* used by DDX to keep state */
} DeviceRec, *DevicePtr;

typedef struct {
    int			click, bell, bell_pitch, bell_duration;
    Bool		autoRepeat;
    unsigned char	autoRepeats[32];
    Leds		leds;
    unsigned char	id;
} KeybdCtrl;

typedef struct {
    KeySym  *map;
    KeyCode minKeyCode,
	    maxKeyCode;
    int     mapWidth;
} KeySymsRec, *KeySymsPtr;

typedef struct {
    int		num, den, threshold;
    unsigned char id;
} PtrCtrl;

typedef struct {
    int         resolution, min_value, max_value;
    int         integer_displayed;
    unsigned char id;
} IntegerCtrl;

typedef struct {
    int         max_symbols, num_symbols_supported;
    int         num_symbols_displayed;
    KeySym      *symbols_supported;
    KeySym      *symbols_displayed;
    unsigned char id;
} StringCtrl;

typedef struct {
    int         percent, pitch, duration;
    unsigned char id;
} BellCtrl;

typedef struct {
    Leds        led_values;
    Mask        led_mask;
    unsigned char id;
} LedCtrl;

extern KeybdCtrl	defaultKeyboardControl;
extern PtrCtrl		defaultPointerControl;

#undef  AddInputDevice
extern DevicePtr AddInputDevice(
#if NeedFunctionPrototypes
    DeviceProc /*deviceProc*/,
    Bool /*autoStart*/
#endif
);

#define AddInputDevice(deviceProc, autoStart) \
       _AddInputDevice(deviceProc, autoStart)

extern DeviceIntPtr _AddInputDevice(
#if NeedFunctionPrototypes
    DeviceProc /*deviceProc*/,
    Bool /*autoStart*/
#endif
);

extern Bool EnableDevice(
#if NeedFunctionPrototypes
    DeviceIntPtr /*device*/
#endif
);

extern Bool DisableDevice(
#if NeedFunctionPrototypes
    DeviceIntPtr /*device*/
#endif
);

extern int InitAndStartDevices(
#if NeedFunctionPrototypes
    void
#endif
);

extern void CloseDownDevices(
#if NeedFunctionPrototypes
    void
#endif
);

extern void RemoveDevice(
#if NeedFunctionPrototypes
    DeviceIntPtr /*dev*/
#endif
);

extern int NumMotionEvents(
#if NeedFunctionPrototypes
    void
#endif
);

#undef  RegisterPointerDevice
extern void RegisterPointerDevice(
#if NeedFunctionPrototypes
    DevicePtr /*device*/
#endif
);

#define RegisterPointerDevice(device) \
       _RegisterPointerDevice(device)

extern void _RegisterPointerDevice(
#if NeedFunctionPrototypes
    DeviceIntPtr /*device*/
#endif
);

#undef  RegisterKeyboardDevice
extern void RegisterKeyboardDevice(
#if NeedFunctionPrototypes
    DevicePtr /*device*/
#endif
);

#define RegisterKeyboardDevice(device) \
       _RegisterKeyboardDevice(device)

extern void _RegisterKeyboardDevice(
#if NeedFunctionPrototypes
    DeviceIntPtr /*device*/
#endif
);

extern DevicePtr LookupKeyboardDevice(
#if NeedFunctionPrototypes
    void
#endif
);

extern DevicePtr LookupPointerDevice(
#if NeedFunctionPrototypes
    void
#endif
);

extern DevicePtr LookupDevice(
#if NeedFunctionPrototypes
    int /* id */
#endif
);

extern void QueryMinMaxKeyCodes(
#if NeedFunctionPrototypes
    KeyCode* /*minCode*/,
    KeyCode* /*maxCode*/
#endif
);

extern Bool SetKeySymsMap(
#if NeedFunctionPrototypes
    KeySymsPtr /*dst*/,
    KeySymsPtr /*src*/
#endif
);

extern Bool InitKeyClassDeviceStruct(
#if NeedFunctionPrototypes
    DeviceIntPtr /*device*/,
    KeySymsPtr /*pKeySyms*/,
    CARD8 /*pModifiers*/[]
#endif
);

extern Bool InitButtonClassDeviceStruct(
#if NeedFunctionPrototypes
    DeviceIntPtr /*device*/,
    int /*numButtons*/,
    CARD8* /*map*/
#endif
);

typedef int (*ValuatorMotionProcPtr)(
#if NeedNestedPrototypes
		DeviceIntPtr /*pdevice*/,
		xTimecoord * /*coords*/,
		unsigned long /*start*/,
		unsigned long /*stop*/,
		ScreenPtr /*pScreen*/
#endif
);

extern Bool InitValuatorClassDeviceStruct(
#if NeedFunctionPrototypes
    DeviceIntPtr /*device*/,
    int /*numAxes*/,
    ValuatorMotionProcPtr /* motionProc */,
    int /*numMotionEvents*/,
    int /*mode*/
#endif
);

extern Bool InitFocusClassDeviceStruct(
#if NeedFunctionPrototypes
    DeviceIntPtr /*device*/
#endif
);

typedef void (*BellProcPtr)(
#if NeedNestedPrototypes
    int /*percent*/,
    DeviceIntPtr /*device*/,
    pointer /*ctrl*/,
    int
#endif
);

typedef void (*KbdCtrlProcPtr)(
#if NeedNestedPrototypes
    DeviceIntPtr /*device*/,
    KeybdCtrl * /*ctrl*/
#endif				     
);

extern Bool InitKbdFeedbackClassDeviceStruct(
#if NeedFunctionPrototypes
    DeviceIntPtr /*device*/,
    BellProcPtr /*bellProc*/,
    KbdCtrlProcPtr /*controlProc*/
#endif
);

typedef void (*PtrCtrlProcPtr)(
#if NeedNestedPrototypes
    DeviceIntPtr /*device*/,
    PtrCtrl * /*ctrl*/
#endif				     
);

extern Bool InitPtrFeedbackClassDeviceStruct(
#if NeedFunctionPrototypes
    DeviceIntPtr /*device*/,
    PtrCtrlProcPtr /*controlProc*/
#endif
);

typedef void (*StringCtrlProcPtr)(
#if NeedNestedPrototypes
    DeviceIntPtr /*device*/,
    StringCtrl * /*ctrl*/
#endif				     
);

extern Bool InitStringFeedbackClassDeviceStruct(
#if NeedFunctionPrototypes
    DeviceIntPtr /*device*/,
    StringCtrlProcPtr /*controlProc*/,
    int /*max_symbols*/,
    int /*num_symbols_supported*/,
    KeySym* /*symbols*/
#endif
);

typedef void (*BellCtrlProcPtr)(
#if NeedNestedPrototypes
    DeviceIntPtr /*device*/,
    BellCtrl * /*ctrl*/
#endif				     
);

extern Bool InitBellFeedbackClassDeviceStruct(
#if NeedFunctionPrototypes
    DeviceIntPtr /*device*/,
    BellProcPtr /*bellProc*/,
    BellCtrlProcPtr /*controlProc*/
#endif
);

typedef void (*LedCtrlProcPtr)(
#if NeedNestedPrototypes
    DeviceIntPtr /*device*/,
    LedCtrl * /*ctrl*/
#endif				     
);

extern Bool InitLedFeedbackClassDeviceStruct(
#if NeedFunctionPrototypes
    DeviceIntPtr /*device*/,
    LedCtrlProcPtr /*controlProc*/
#endif
);

typedef void (*IntegerCtrlProcPtr)(
#if NeedNestedPrototypes
    DeviceIntPtr /*device*/,
    IntegerCtrl * /*ctrl*/
#endif
);


extern Bool InitIntegerFeedbackClassDeviceStruct(
#if NeedFunctionPrototypes
    DeviceIntPtr /*device*/,
    IntegerCtrlProcPtr /*controlProc*/
#endif
);

extern Bool InitPointerDeviceStruct(
#if NeedFunctionPrototypes
    DevicePtr /*device*/,
    CARD8* /*map*/,
    int /*numButtons*/,
    ValuatorMotionProcPtr /*motionProc*/,
    PtrCtrlProcPtr /*controlProc*/,
    int /*numMotionEvents*/
#endif
);

extern Bool InitKeyboardDeviceStruct(
#if NeedFunctionPrototypes
    DevicePtr /*device*/,
    KeySymsPtr /*pKeySyms*/,
    CARD8 /*pModifiers*/[],
    BellProcPtr /*bellProc*/,
    KbdCtrlProcPtr /*controlProc*/
#endif
);

extern void SendMappingNotify(
#if NeedFunctionPrototypes
    unsigned int /*request*/,
    unsigned int /*firstKeyCode*/,
    unsigned int /*count*/,
    ClientPtr	/* client */
#endif
);

extern Bool BadDeviceMap(
#if NeedFunctionPrototypes
    BYTE* /*buff*/,
    int /*length*/,
    unsigned /*low*/,
    unsigned /*high*/,
    XID* /*errval*/
#endif
);

extern Bool AllModifierKeysAreUp(
#if NeedFunctionPrototypes
    DeviceIntPtr /*device*/,
    CARD8* /*map1*/,
    int /*per1*/,
    CARD8* /*map2*/,
    int /*per2*/
#endif
);

extern void NoteLedState(
#if NeedFunctionPrototypes
    DeviceIntPtr /*keybd*/,
    int /*led*/,
    Bool /*on*/
#endif
);

extern void MaybeStopHint(
#if NeedFunctionPrototypes
    DeviceIntPtr /*device*/,
    ClientPtr /*client*/
#endif
);

extern void ProcessPointerEvent(
#if NeedFunctionPrototypes
    xEventPtr /*xE*/,
    DeviceIntPtr /*mouse*/,
    int /*count*/
#endif
);

extern void ProcessKeyboardEvent(
#if NeedFunctionPrototypes
    xEventPtr /*xE*/,
    DeviceIntPtr /*keybd*/,
    int /*count*/
#endif
);

#ifdef XKB
extern void CoreProcessPointerEvent(
#if NeedFunctionPrototypes
    xEventPtr /*xE*/,
    DeviceIntPtr /*mouse*/,
    int /*count*/
#endif
);

extern void CoreProcessKeyboardEvent(
#if NeedFunctionPrototypes
    xEventPtr /*xE*/,
    DeviceIntPtr /*keybd*/,
    int /*count*/
#endif
);
#endif

extern Bool LegalModifier(
#if NeedFunctionPrototypes
    unsigned int /*key*/, 
    DevicePtr /*pDev*/
#endif
);

extern void ProcessInputEvents(
#if NeedFunctionPrototypes
    void
#endif
);

extern void InitInput(
#if NeedFunctionPrototypes
    int  /*argc*/,
    char ** /*argv*/
#endif
);

#endif /* INPUT_H */
