/* $XConsortium: opaque.h,v 1.19 94/04/17 20:25:51 dpw Exp $ */
/*

Copyright (c) 1987  X Consortium

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the X Consortium.

*/
/* $XFree86: xc/programs/Xserver/include/opaque.h,v 1.2.2.1 1997/06/01 12:33:43 dawes Exp $ */

#ifndef OPAQUE_H
#define OPAQUE_H

#include <X11/Xmd.h>

extern char *defaultFontPath;
extern char *defaultTextFont;
extern char *defaultCursorFont;
extern char *rgbPath;
extern int MaxClients;
extern char isItTimeToYield;
extern char dispatchException;

/* bit values for dispatchException */
#define DE_RESET     1
#define DE_TERMINATE 2
#define DE_PRIORITYCHANGE 4  /* set when a client's priority changes */

extern CARD32 TimeOutValue;
extern CARD32 ScreenSaverTime;
extern CARD32 ScreenSaverInterval;
extern int  ScreenSaverBlanking;
extern int  ScreenSaverAllowExposures;
extern int argcGlobal;
extern char **argvGlobal;

#if DPMSExtension
extern CARD32 defaultDPMSStandbyTime;
extern CARD32 defaultDPMSSuspendTime;
extern CARD32 defaultDPMSOffTime;
extern CARD32 DPMSStandbyTime;
extern CARD32 DPMSSuspendTime;
extern CARD32 DPMSOffTime;
extern CARD16 DPMSPowerLevel;
extern Bool defaultDPMSEnabled;
extern Bool DPMSEnabled;
extern Bool DPMSEnabledSwitch;
extern Bool DPMSDisabledSwitch;
extern Bool DPMSCapableFlag;
#endif

#endif /* OPAQUE_H */
