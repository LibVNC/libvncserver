/*
 * cursor.c - support for cursor shape updates.
 */

/*
 *  Copyright (C) 2000, 2001 Const Kaplinsky.  All Rights Reserved.
 *  Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
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
#include "rfb.h"
#include "mipointer.h"
#include "sprite.h"
#include "cursorstr.h"
#include "servermd.h"


/* Copied from Xvnc/lib/font/util/utilbitmap.c */
static unsigned char _reverse_byte[0x100] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};


static int EncodeRichCursorData8 (char *buf, rfbPixelFormat *fmt,
				  CursorPtr pCursor);
static int EncodeRichCursorData16 (char *buf, rfbPixelFormat *fmt,
				   CursorPtr pCursor);
static int EncodeRichCursorData32 (char *buf, rfbPixelFormat *fmt,
				   CursorPtr pCursor);


/*
 * Send cursor shape either in X-style format or in client pixel format.
 */

Bool
rfbSendCursorShape(cl, pScreen)
    rfbClientPtr cl;
    ScreenPtr pScreen;
{
    CursorPtr pCursor;
    rfbFramebufferUpdateRectHeader rect;
    rfbXCursorColors colors;
    int saved_ublen;
    int bitmapRowBytes, paddedRowBytes, maskBytes, dataBytes;
    int i, j;
    CARD8 *bitmapData;
    CARD8 bitmapByte;

    if (cl->useRichCursorEncoding) {
	rect.encoding = Swap32IfLE(rfbEncodingRichCursor);
    } else {
	rect.encoding = Swap32IfLE(rfbEncodingXCursor);
    }

    pCursor = rfbSpriteGetCursorPtr(pScreen);

    /* If there is no cursor, send update with empty cursor data. */

    if ( pCursor->bits->width == 1 &&
	 pCursor->bits->height == 1 &&
	 pCursor->bits->mask[0] == 0 ) {
	pCursor = NULL;
    }

    if (pCursor == NULL) {
	if (ublen + sz_rfbFramebufferUpdateRectHeader > UPDATE_BUF_SIZE ) {
	    if (!rfbSendUpdateBuf(cl))
		return FALSE;
	}
	rect.r.x = rect.r.y = 0;
	rect.r.w = rect.r.h = 0;
	memcpy(&updateBuf[ublen], (char *)&rect,
	       sz_rfbFramebufferUpdateRectHeader);
	ublen += sz_rfbFramebufferUpdateRectHeader;

	cl->rfbCursorBytesSent += sz_rfbFramebufferUpdateRectHeader;
	cl->rfbCursorUpdatesSent++;

	if (!rfbSendUpdateBuf(cl))
	    return FALSE;

	return TRUE;
    }

    /* Calculate data sizes. */

    bitmapRowBytes = (pCursor->bits->width + 7) / 8;
    paddedRowBytes = PixmapBytePad(pCursor->bits->width, 1);
    maskBytes = bitmapRowBytes * pCursor->bits->height;
    dataBytes = (cl->useRichCursorEncoding) ?
	(pCursor->bits->width * pCursor->bits->height *
	 (cl->format.bitsPerPixel / 8)) : maskBytes;

    /* Send buffer contents if needed. */

    if ( ublen + sz_rfbFramebufferUpdateRectHeader +
	 sz_rfbXCursorColors + maskBytes + dataBytes > UPDATE_BUF_SIZE ) {
	if (!rfbSendUpdateBuf(cl))
	    return FALSE;
    }

    if ( ublen + sz_rfbFramebufferUpdateRectHeader +
	 sz_rfbXCursorColors + maskBytes + dataBytes > UPDATE_BUF_SIZE ) {
	return FALSE;		/* FIXME. */
    }

    saved_ublen = ublen;

    /* Prepare rectangle header. */

    rect.r.x = Swap16IfLE(pCursor->bits->xhot);
    rect.r.y = Swap16IfLE(pCursor->bits->yhot);
    rect.r.w = Swap16IfLE(pCursor->bits->width);
    rect.r.h = Swap16IfLE(pCursor->bits->height);

    memcpy(&updateBuf[ublen], (char *)&rect,sz_rfbFramebufferUpdateRectHeader);
    ublen += sz_rfbFramebufferUpdateRectHeader;

    /* Prepare actual cursor data (depends on encoding used). */

    if (!cl->useRichCursorEncoding) {
	/* XCursor encoding. */
	colors.foreRed   = (char)(pCursor->foreRed   >> 8);
	colors.foreGreen = (char)(pCursor->foreGreen >> 8);
	colors.foreBlue  = (char)(pCursor->foreBlue  >> 8);
	colors.backRed   = (char)(pCursor->backRed   >> 8);
	colors.backGreen = (char)(pCursor->backGreen >> 8);
	colors.backBlue  = (char)(pCursor->backBlue  >> 8);

	memcpy(&updateBuf[ublen], (char *)&colors, sz_rfbXCursorColors);
	ublen += sz_rfbXCursorColors;

	bitmapData = (CARD8 *)pCursor->bits->source;

	for (i = 0; i < pCursor->bits->height; i++) {
	    for (j = 0; j < bitmapRowBytes; j++) {
		bitmapByte = bitmapData[i * paddedRowBytes + j];
		if (screenInfo.bitmapBitOrder == LSBFirst) {
		    bitmapByte = _reverse_byte[bitmapByte];
		}
		updateBuf[ublen++] = (char)bitmapByte;
	    }
	}
    } else {
	/* RichCursor encoding. */
	switch (cl->format.bitsPerPixel) {
	case 8:
	    ublen += EncodeRichCursorData8(&updateBuf[ublen],
					   &cl->format, pCursor);
	    break;
	case 16:
	    ublen += EncodeRichCursorData16(&updateBuf[ublen],
					    &cl->format, pCursor);
	    break;
	case 32:
	    ublen += EncodeRichCursorData32(&updateBuf[ublen],
					    &cl->format, pCursor);
	    break;
	default:
	    return FALSE;
	}
    }

    /* Prepare transparency mask. */

    bitmapData = (CARD8 *)pCursor->bits->mask;

    for (i = 0; i < pCursor->bits->height; i++) {
	for (j = 0; j < bitmapRowBytes; j++) {
	    bitmapByte = bitmapData[i * paddedRowBytes + j];
	    if (screenInfo.bitmapBitOrder == LSBFirst) {
		bitmapByte = _reverse_byte[bitmapByte];
	    }
	    updateBuf[ublen++] = (char)bitmapByte;
	}
    }

    /* Send everything we have prepared in the updateBuf[]. */

    cl->rfbCursorBytesSent += (ublen - saved_ublen);
    cl->rfbCursorUpdatesSent++;

    if (!rfbSendUpdateBuf(cl))
	return FALSE;

    return TRUE;
}


/*
 * Code to convert cursor source bitmap to the desired pixel format.
 */

#define RGB48_TO_PIXEL(fmt,r,g,b)					\
    (((CARD32)(r) * ((fmt)->redMax + 1) >> 16) << (fmt)->redShift |	\
     ((CARD32)(g) * ((fmt)->greenMax + 1) >> 16) << (fmt)->greenShift |	\
     ((CARD32)(b) * ((fmt)->blueMax + 1) >> 16) << (fmt)->blueShift)

static int
EncodeRichCursorData8(buf, fmt, pCursor)
    char *buf;
    rfbPixelFormat *fmt;
    CursorPtr pCursor;
{
    int widthPixels, widthBytes;
    int x, y, b;
    CARD8 *src;
    char pix[2];
    CARD8 bitmapByte;

    pix[0] = (char)RGB48_TO_PIXEL(fmt, pCursor->backRed, pCursor->backGreen,
				  pCursor->backBlue);
    pix[1] = (char)RGB48_TO_PIXEL(fmt, pCursor->foreRed, pCursor->foreGreen,
				  pCursor->foreBlue);

    src = (CARD8 *)pCursor->bits->source;
    widthPixels = pCursor->bits->width;
    widthBytes = PixmapBytePad(widthPixels, 1);

    for (y = 0; y < pCursor->bits->height; y++) {
	for (x = 0; x < widthPixels / 8; x++) {
	    bitmapByte = src[y * widthBytes + x];
	    if (screenInfo.bitmapBitOrder == LSBFirst) {
		bitmapByte = _reverse_byte[bitmapByte];
	    }
	    for (b = 7; b >= 0; b--) {
		*buf++ = pix[bitmapByte >> b & 1];
	    }
	}
	if (widthPixels % 8) {
	    bitmapByte = src[y * widthBytes + x];
	    if (screenInfo.bitmapBitOrder == LSBFirst) {
		bitmapByte = _reverse_byte[bitmapByte];
	    }
	    for (b = 7; b > 7 - widthPixels % 8; b--) {
		*buf++ = pix[bitmapByte >> b & 1];
	    }
	}
    }

    return (widthPixels * pCursor->bits->height);
}

#define DEFINE_RICH_ENCODE(bpp)						 \
									 \
static int								 \
EncodeRichCursorData##bpp(buf, fmt, pCursor)				 \
    char *buf;								 \
    rfbPixelFormat *fmt;						 \
    CursorPtr pCursor;							 \
{									 \
    int widthPixels, widthBytes;					 \
    int x, y, b;							 \
    CARD8 *src;								 \
    CARD##bpp pix[2];							 \
    CARD8 bitmapByte;							 \
									 \
    pix[0] = (CARD##bpp)RGB48_TO_PIXEL(fmt, pCursor->backRed,		 \
				       pCursor->backGreen,		 \
				       pCursor->backBlue);		 \
    pix[1] = (CARD##bpp)RGB48_TO_PIXEL(fmt, pCursor->foreRed,		 \
				       pCursor->foreGreen,		 \
				       pCursor->foreBlue);		 \
    if (!rfbServerFormat.bigEndian != !fmt->bigEndian) {		 \
	pix[0] = Swap##bpp(pix[0]);					 \
	pix[1] = Swap##bpp(pix[1]);					 \
    }									 \
									 \
    src = (CARD8 *)pCursor->bits->source;				 \
    widthPixels = pCursor->bits->width;					 \
    widthBytes = PixmapBytePad(widthPixels, 1);				 \
									 \
    for (y = 0; y < pCursor->bits->height; y++) {			 \
	for (x = 0; x < widthPixels / 8; x++) {				 \
	    bitmapByte = src[y * widthBytes + x];			 \
	    if (screenInfo.bitmapBitOrder == LSBFirst) {		 \
		bitmapByte = _reverse_byte[bitmapByte];			 \
	    }								 \
	    for (b = 7; b >= 0; b--) {					 \
		memcpy (buf, (char *)&pix[bitmapByte >> b & 1],		 \
			sizeof(CARD##bpp));				 \
		buf += sizeof(CARD##bpp);				 \
	    }								 \
	}								 \
	if (widthPixels % 8) {						 \
	    bitmapByte = src[y * widthBytes + x];			 \
	    if (screenInfo.bitmapBitOrder == LSBFirst) {		 \
		bitmapByte = _reverse_byte[bitmapByte];			 \
	    }								 \
	    for (b = 7; b > 7 - widthPixels % 8; b--) {			 \
		memcpy (buf, (char *)&pix[bitmapByte >> b & 1],		 \
			sizeof(CARD##bpp));				 \
		buf += sizeof(CARD##bpp);				 \
	    }								 \
	}								 \
    }									 \
									 \
    return (widthPixels * pCursor->bits->height * (bpp / 8));		 \
}

DEFINE_RICH_ENCODE(16)
DEFINE_RICH_ENCODE(32)

