/*
 * stats.c
 */

/*
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

#include <stdio.h>
#include <stdlib.h>
#include "rfb.h"

static const char* encNames[] = {
    "raw", "copyRect", "RRE", "[encoding 3]", "CoRRE", "hextile",
    "zlib", "tight", "[encoding 8]", "[encoding 9]"
};


void
rfbResetStats(rfbClientPtr cl)
{
    int i;
    for (i = 0; i < MAX_ENCODINGS; i++) {
        cl->rfbBytesSent[i] = 0;
        cl->rfbRectanglesSent[i] = 0;
    }
    cl->rfbLastRectMarkersSent = 0;
    cl->rfbLastRectBytesSent = 0;
    cl->rfbCursorBytesSent = 0;
    cl->rfbCursorUpdatesSent = 0;
    cl->rfbFramebufferUpdateMessagesSent = 0;
    cl->rfbRawBytesEquivalent = 0;
    cl->rfbKeyEventsRcvd = 0;
    cl->rfbPointerEventsRcvd = 0;
}

void
rfbPrintStats(rfbClientPtr cl)
{
    int i;
    int totalRectanglesSent = 0;
    int totalBytesSent = 0;

    rfbLog("Statistics:\n");

    if ((cl->rfbKeyEventsRcvd != 0) || (cl->rfbPointerEventsRcvd != 0))
        rfbLog("  key events received %d, pointer events %d\n",
                cl->rfbKeyEventsRcvd, cl->rfbPointerEventsRcvd);

    for (i = 0; i < MAX_ENCODINGS; i++) {
        totalRectanglesSent += cl->rfbRectanglesSent[i];
        totalBytesSent += cl->rfbBytesSent[i];
    }

    totalRectanglesSent += (cl->rfbCursorUpdatesSent +
			    cl->rfbLastRectMarkersSent);
    totalBytesSent += (cl->rfbCursorBytesSent + cl->rfbLastRectBytesSent);

    rfbLog("  framebuffer updates %d, rectangles %d, bytes %d\n",
            cl->rfbFramebufferUpdateMessagesSent, totalRectanglesSent,
            totalBytesSent);

    if (cl->rfbLastRectMarkersSent != 0)
	rfbLog("    LastRect markers %d, bytes %d\n",
		cl->rfbLastRectMarkersSent, cl->rfbLastRectBytesSent);

    if (cl->rfbCursorUpdatesSent != 0)
	rfbLog("    cursor shape updates %d, bytes %d\n",
		cl->rfbCursorUpdatesSent, cl->rfbCursorBytesSent);

    for (i = 0; i < MAX_ENCODINGS; i++) {
        if (cl->rfbRectanglesSent[i] != 0)
            rfbLog("    %s rectangles %d, bytes %d\n",
                   encNames[i], cl->rfbRectanglesSent[i], cl->rfbBytesSent[i]);
    }

    if ((totalBytesSent - cl->rfbBytesSent[rfbEncodingCopyRect]) != 0) {
        rfbLog("  raw bytes equivalent %d, compression ratio %f\n",
                cl->rfbRawBytesEquivalent,
                (double)cl->rfbRawBytesEquivalent
                / (double)(totalBytesSent
                           - cl->rfbBytesSent[rfbEncodingCopyRect]-
			   cl->rfbCursorBytesSent -
			   cl->rfbLastRectBytesSent));
    }
}
