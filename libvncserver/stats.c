/*
 * stats.c
 */

/*
 *  Copyright (C) 2002 RealVNC Ltd.
 *  OSXvnc Copyright (C) 2001 Dan McGuirk <mcguirk@incompleteness.net>.
 *  Original Xvnc code Copyright (C) 1999 AT&T Laboratories Cambridge.  
 *  All Rights Reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

#include <rfb/rfb.h>

static const char* encNames[] = {
    "raw", "copyRect", "RRE", "[encoding 3]", "CoRRE", "hextile",
    "zlib", "tight", "[encoding 8]", "[encoding 9]", "[encoding 10]",
    "[encoding 11]", "[encoding 12]", "[encoding 13]", "[encoding 14]",
#ifdef LIBVNCSERVER_BACKCHANNEL
    "BackChannel",
#else
    "[encoding 15]",
#endif
    "ZRLE", "[encoding 17]", "[encoding 18]", "[encoding 19]", "[encoding 20]"
};


void
rfbResetStats(rfbClientPtr cl)
{
    int i;
    for (i = 0; i < MAX_ENCODINGS; i++) {
        cl->bytesSent[i] = 0;
        cl->rectanglesSent[i] = 0;
    }
    cl->lastRectMarkersSent = 0;
    cl->lastRectBytesSent = 0;
    cl->cursorShapeBytesSent = 0;
    cl->cursorShapeUpdatesSent = 0;
    cl->cursorPosBytesSent = 0;
    cl->cursorPosUpdatesSent = 0;
    cl->framebufferUpdateMessagesSent = 0;
    cl->rawBytesEquivalent = 0;
    cl->keyEventsRcvd = 0;
    cl->pointerEventsRcvd = 0;
}

void
rfbPrintStats(rfbClientPtr cl)
{
    int i;
    int totalRectanglesSent = 0;
    int totalBytesSent = 0;

    rfbLog("Statistics:\n");

    if ((cl->keyEventsRcvd != 0) || (cl->pointerEventsRcvd != 0))
        rfbLog("  key events received %d, pointer events %d\n",
                cl->keyEventsRcvd, cl->pointerEventsRcvd);

    for (i = 0; i < MAX_ENCODINGS; i++) {
        totalRectanglesSent += cl->rectanglesSent[i];
        totalBytesSent += cl->bytesSent[i];
    }

    totalRectanglesSent += (cl->cursorShapeUpdatesSent +
                            cl->cursorPosUpdatesSent +
			    cl->lastRectMarkersSent);
    totalBytesSent += (cl->cursorShapeBytesSent +
                       cl->cursorPosBytesSent +
                       cl->lastRectBytesSent);

    rfbLog("  framebuffer updates %d, rectangles %d, bytes %d\n",
            cl->framebufferUpdateMessagesSent, totalRectanglesSent,
            totalBytesSent);

    if (cl->lastRectMarkersSent != 0)
	rfbLog("    LastRect and NewFBSize markers %d, bytes %d\n",
		cl->lastRectMarkersSent, cl->lastRectBytesSent);

    if (cl->cursorShapeUpdatesSent != 0)
	rfbLog("    cursor shape updates %d, bytes %d\n",
		cl->cursorShapeUpdatesSent, cl->cursorShapeBytesSent);

    if (cl->cursorPosUpdatesSent != 0)
	rfbLog("    cursor position updates %d, bytes %d\n",
		cl->cursorPosUpdatesSent, cl->cursorPosBytesSent);

    for (i = 0; i < MAX_ENCODINGS; i++) {
        if (cl->rectanglesSent[i] != 0)
            rfbLog("    %s rectangles %d, bytes %d\n",
                   encNames[i], cl->rectanglesSent[i], cl->bytesSent[i]);
    }

    if ((totalBytesSent - cl->bytesSent[rfbEncodingCopyRect]) != 0) {
        rfbLog("  raw bytes equivalent %d, compression ratio %f\n",
                cl->rawBytesEquivalent,
                (double)cl->rawBytesEquivalent
                / (double)(totalBytesSent
                           - cl->bytesSent[rfbEncodingCopyRect]-
			   cl->cursorShapeBytesSent -
                           cl->cursorPosBytesSent -
			   cl->lastRectBytesSent));
    }
}
