/*
 * tight.c
 *
 * Routines to implement Tight Encoding
 */

/*
 *  Copyright (C) 2005-2008 Sun Microsystems, Inc.  All Rights Reserved.
 *  Copyright (C) 2004 Landmark Graphics Corporation.  All Rights Reserved.
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
#include <stdlib.h>
#include <string.h>
#include "rfb.h"
#include "turbojpeg.h"

/* Note: The following constant should not be changed. */
#define TIGHT_MIN_TO_COMPRESS 12

/* The parameters below may be adjusted. */
#define MIN_SPLIT_RECT_SIZE     4096
#define MIN_SOLID_SUBRECT_SIZE  2048
#define MAX_SPLIT_TILE_SIZE       16

/* This variable is set on every rfbSendRectEncodingTight() call. */
static Bool usePixelFormat24;


/* Compression level stuff. The following array contains various
   encoder parameters for each of 10 compression levels (0..9).
   Last three parameters correspond to JPEG quality levels (0..9). */

typedef struct TIGHT_CONF_s {
    int maxRectSize, maxRectWidth;
    int monoMinRectSize;
    int idxZlibLevel, monoZlibLevel, rawZlibLevel;
    int idxMaxColorsDivisor;
} TIGHT_CONF;

static TIGHT_CONF tightConf[2] = {
    { 65536, 2048,   6, 0, 0, 0,   4 },
#if 0
    {  2048,  128,   6, 1, 1, 1,   8 },
    {  6144,  256,   8, 3, 3, 2,  24 },
    { 10240, 1024,  12, 5, 5, 3,  32 },
    { 16384, 2048,  12, 6, 6, 4,  32 },
    { 32768, 2048,  12, 7, 7, 5,  32 },
    { 65536, 2048,  16, 7, 7, 6,  48 },
    { 65536, 2048,  16, 8, 8, 7,  64 },
    { 65536, 2048,  32, 9, 9, 8,  64 },
#endif
    { 65536, 2048,  32, 1, 1, 1,  96 }
};

static int compressLevel;
static int qualityLevel;
static int subsampLevel;

static const int subsampLevel2tjsubsamp[4] = {
    TJ_444, TJ_411, TJ_422, TJ_GRAYSCALE
};

/* Stuff dealing with palettes. */

typedef struct COLOR_LIST_s {
    struct COLOR_LIST_s *next;
    int idx;
    CARD32 rgb;
} COLOR_LIST;

typedef struct PALETTE_ENTRY_s {
    COLOR_LIST *listNode;
    int numPixels;
} PALETTE_ENTRY;

typedef struct PALETTE_s {
    PALETTE_ENTRY entry[256];
    COLOR_LIST *hash[256];
    COLOR_LIST list[256];
} PALETTE;

static int paletteNumColors, paletteMaxColors;
static CARD32 monoBackground, monoForeground;
static PALETTE palette;

/* Pointers to dynamically-allocated buffers. */

static int tightBeforeBufSize = 0;
static char *tightBeforeBuf = NULL;

static int tightAfterBufSize = 0;
static char *tightAfterBuf = NULL;

static int *prevRowBuf = NULL;


/* Prototypes for static functions. */

static void FindBestSolidArea (int x, int y, int w, int h,
                               CARD32 colorValue, int *w_ptr, int *h_ptr);
static void ExtendSolidArea   (int x, int y, int w, int h,
                               CARD32 colorValue,
                               int *x_ptr, int *y_ptr, int *w_ptr, int *h_ptr);
static Bool CheckSolidTile    (int x, int y, int w, int h,
                               CARD32 *colorPtr, Bool needSameColor);
static Bool CheckSolidTile8   (int x, int y, int w, int h,
                               CARD32 *colorPtr, Bool needSameColor);
static Bool CheckSolidTile16  (int x, int y, int w, int h,
                               CARD32 *colorPtr, Bool needSameColor);
static Bool CheckSolidTile32  (int x, int y, int w, int h,
                               CARD32 *colorPtr, Bool needSameColor);

static Bool SendRectSimple    (rfbClientPtr cl, int x, int y, int w, int h);
static Bool SendSubrect       (rfbClientPtr cl, int x, int y, int w, int h);
static Bool SendTightHeader   (rfbClientPtr cl, int x, int y, int w, int h);

static Bool SendSolidRect     (rfbClientPtr cl);
static Bool SendMonoRect      (rfbClientPtr cl, int w, int h);
static Bool SendIndexedRect   (rfbClientPtr cl, int w, int h);
static Bool SendFullColorRect (rfbClientPtr cl, int w, int h);

static Bool CompressData(rfbClientPtr cl, int streamId, int dataLen,
                         int zlibLevel, int zlibStrategy);
static Bool SendCompressedData(rfbClientPtr cl, char *buf, int compressedLen);

static void FillPalette8(int count);
static void FillPalette16(int count);
static void FillPalette32(int count);
static void FastFillPalette16(rfbClientPtr cl, CARD16 *data, int w, int pitch,
                              int h);
static void FastFillPalette32(rfbClientPtr cl, CARD32 *data, int w, int pitch,
                              int h);

static void PaletteReset(void);
static int PaletteInsert(CARD32 rgb, int numPixels, int bpp);

static void Pack24(char *buf, rfbPixelFormat *fmt, int count);

static void EncodeIndexedRect16(CARD8 *buf, int count);
static void EncodeIndexedRect32(CARD8 *buf, int count);

static void EncodeMonoRect8(CARD8 *buf, int w, int h);
static void EncodeMonoRect16(CARD8 *buf, int w, int h);
static void EncodeMonoRect32(CARD8 *buf, int w, int h);

static Bool SendJpegRect(rfbClientPtr cl, int x, int y, int w, int h,
                         int quality);

/*
 * Tight encoding implementation.
 */

int
rfbNumCodedRectsTight(cl, x, y, w, h)
    rfbClientPtr cl;
    int x, y, w, h;
{
    int maxRectSize, maxRectWidth;
    int subrectMaxWidth, subrectMaxHeight;

    /* No matter how many rectangles we will send if LastRect markers
       are used to terminate rectangle stream. */
    if (cl->enableLastRectEncoding && w * h >= MIN_SPLIT_RECT_SIZE)
      return 0;

    maxRectSize = tightConf[compressLevel].maxRectSize;
    maxRectWidth = tightConf[compressLevel].maxRectWidth;

    if (w > maxRectWidth || w * h > maxRectSize) {
        subrectMaxWidth = (w > maxRectWidth) ? maxRectWidth : w;
        subrectMaxHeight = maxRectSize / subrectMaxWidth;
        return (((w - 1) / maxRectWidth + 1) *
                ((h - 1) / subrectMaxHeight + 1));
    } else {
        return 1;
    }
}

Bool
rfbSendRectEncodingTight(cl, x, y, w, h)
    rfbClientPtr cl;
    int x, y, w, h;
{
    int nMaxRows;
    CARD32 colorValue;
    int dx, dy, dw, dh;
    int x_best, y_best, w_best, h_best;
    char *fbptr;

    compressLevel = cl->tightCompressLevel > 0 ? 1 : 0;
    qualityLevel = cl->tightQualityLevel;
    if (qualityLevel != -1) {
        compressLevel = 1;
        tightConf[compressLevel].idxZlibLevel = 1;
        tightConf[compressLevel].monoZlibLevel = 1;
        tightConf[compressLevel].rawZlibLevel = 1;
    } else {
        tightConf[compressLevel].idxZlibLevel = cl->tightCompressLevel;
        tightConf[compressLevel].monoZlibLevel = cl->tightCompressLevel;
        tightConf[compressLevel].rawZlibLevel = cl->tightCompressLevel;
    }
    subsampLevel = cl->tightSubsampLevel;

    if ( cl->format.depth == 24 && cl->format.redMax == 0xFF &&
         cl->format.greenMax == 0xFF && cl->format.blueMax == 0xFF ) {
        usePixelFormat24 = TRUE;
    } else {
        usePixelFormat24 = FALSE;
    }

    if (!cl->enableLastRectEncoding || w * h < MIN_SPLIT_RECT_SIZE)
        return SendRectSimple(cl, x, y, w, h);

    /* Make sure we can write at least one pixel into tightBeforeBuf. */

    if (tightBeforeBufSize < 4) {
        tightBeforeBufSize = 4;
        if (tightBeforeBuf == NULL)
            tightBeforeBuf = (char *)xalloc(tightBeforeBufSize);
        else
            tightBeforeBuf = (char *)xrealloc(tightBeforeBuf,
                                              tightBeforeBufSize);
    }

    /* Calculate maximum number of rows in one non-solid rectangle. */

    {
        int maxRectSize, maxRectWidth, nMaxWidth;

        maxRectSize = tightConf[compressLevel].maxRectSize;
        maxRectWidth = tightConf[compressLevel].maxRectWidth;
        nMaxWidth = (w > maxRectWidth) ? maxRectWidth : w;
        nMaxRows = maxRectSize / nMaxWidth;
    }

    /* Try to find large solid-color areas and send them separately. */

    for (dy = y; dy < y + h; dy += MAX_SPLIT_TILE_SIZE) {

        /* If a rectangle becomes too large, send its upper part now. */

        if (dy - y >= nMaxRows) {
            if (!SendRectSimple(cl, x, y, w, nMaxRows))
                return 0;
            y += nMaxRows;
            h -= nMaxRows;
        }

        dh = (dy + MAX_SPLIT_TILE_SIZE <= y + h) ?
            MAX_SPLIT_TILE_SIZE : (y + h - dy);

        for (dx = x; dx < x + w; dx += MAX_SPLIT_TILE_SIZE) {

            dw = (dx + MAX_SPLIT_TILE_SIZE <= x + w) ?
                MAX_SPLIT_TILE_SIZE : (x + w - dx);

            if (CheckSolidTile(dx, dy, dw, dh, &colorValue, FALSE)) {

                if (subsampLevel == TJ_GRAYSCALE && qualityLevel != -1) {
		    CARD32 r=(colorValue>>16)&0xFF;
		    CARD32 g=(colorValue>>8)&0xFF;
		    CARD32 b=(colorValue)&0xFF;
		    double y=(0.257*(double)r)+(0.504*(double)g)
		        +(0.098*(double)b)+16.;
		    colorValue=(int)y+(((int)y)<<8)+(((int)y)<<16);
		}

                /* Get dimensions of solid-color area. */

                FindBestSolidArea(dx, dy, w - (dx - x), h - (dy - y),
				  colorValue, &w_best, &h_best);

                /* Make sure a solid rectangle is large enough
                   (or the whole rectangle is of the same color). */

                if ( w_best * h_best != w * h &&
                     w_best * h_best < MIN_SOLID_SUBRECT_SIZE )
                    continue;

                /* Try to extend solid rectangle to maximum size. */

                x_best = dx; y_best = dy;
                ExtendSolidArea(x, y, w, h, colorValue,
                                &x_best, &y_best, &w_best, &h_best);

                /* Send rectangles at top and left to solid-color area. */

                if ( y_best != y &&
                     !SendRectSimple(cl, x, y, w, y_best-y) )
                    return FALSE;
                if ( x_best != x &&
                     !rfbSendRectEncodingTight(cl, x, y_best,
                                               x_best-x, h_best) )
                    return FALSE;

                /* Send solid-color rectangle. */

                if (!SendTightHeader(cl, x_best, y_best, w_best, h_best))
                    return FALSE;

                fbptr = (rfbScreen.pfbMemory +
                         (rfbScreen.paddedWidthInBytes * y_best) +
                         (x_best * (rfbScreen.bitsPerPixel / 8)));

                (*cl->translateFn)(cl->translateLookupTable, &rfbServerFormat,
                                   &cl->format, fbptr, tightBeforeBuf,
                                   rfbScreen.paddedWidthInBytes, 1, 1);

                if (!SendSolidRect(cl))
                    return FALSE;

                /* Send remaining rectangles (at right and bottom). */

                if ( x_best + w_best != x + w &&
                     !rfbSendRectEncodingTight(cl, x_best+w_best, y_best,
                                               w-(x_best-x)-w_best, h_best) )
                    return FALSE;
                if ( y_best + h_best != y + h &&
                     !rfbSendRectEncodingTight(cl, x, y_best+h_best,
                                               w, h-(y_best-y)-h_best) )
                    return FALSE;

                /* Return after all recursive calls are done. */

                return TRUE;
            }

        }

    }

    /* No suitable solid-color rectangles found. */

    return SendRectSimple(cl, x, y, w, h);
}

static void
FindBestSolidArea(x, y, w, h, colorValue, w_ptr, h_ptr)
    int x, y, w, h;
    CARD32 colorValue;
    int *w_ptr, *h_ptr;
{
    int dx, dy, dw, dh;
    int w_prev;
    int w_best = 0, h_best = 0;

    w_prev = w;

    for (dy = y; dy < y + h; dy += MAX_SPLIT_TILE_SIZE) {

        dh = (dy + MAX_SPLIT_TILE_SIZE <= y + h) ?
            MAX_SPLIT_TILE_SIZE : (y + h - dy);
        dw = (w_prev > MAX_SPLIT_TILE_SIZE) ?
            MAX_SPLIT_TILE_SIZE : w_prev;

        if (!CheckSolidTile(x, dy, dw, dh, &colorValue, TRUE))
            break;

        for (dx = x + dw; dx < x + w_prev;) {
            dw = (dx + MAX_SPLIT_TILE_SIZE <= x + w_prev) ?
                MAX_SPLIT_TILE_SIZE : (x + w_prev - dx);
            if (!CheckSolidTile(dx, dy, dw, dh, &colorValue, TRUE))
                break;
	    dx += dw;
        }

        w_prev = dx - x;
        if (w_prev * (dy + dh - y) > w_best * h_best) {
            w_best = w_prev;
            h_best = dy + dh - y;
        }
    }

    *w_ptr = w_best;
    *h_ptr = h_best;
}

static void
ExtendSolidArea(x, y, w, h, colorValue, x_ptr, y_ptr, w_ptr, h_ptr)
    int x, y, w, h;
    CARD32 colorValue;
    int *x_ptr, *y_ptr, *w_ptr, *h_ptr;
{
    int cx, cy;

    /* Try to extend the area upwards. */
    for ( cy = *y_ptr - 1;
          cy >= y && CheckSolidTile(*x_ptr, cy, *w_ptr, 1, &colorValue, TRUE);
          cy-- );
    *h_ptr += *y_ptr - (cy + 1);
    *y_ptr = cy + 1;

    /* ... downwards. */
    for ( cy = *y_ptr + *h_ptr;
          cy < y + h &&
              CheckSolidTile(*x_ptr, cy, *w_ptr, 1, &colorValue, TRUE);
          cy++ );
    *h_ptr += cy - (*y_ptr + *h_ptr);

    /* ... to the left. */
    for ( cx = *x_ptr - 1;
          cx >= x && CheckSolidTile(cx, *y_ptr, 1, *h_ptr, &colorValue, TRUE);
          cx-- );
    *w_ptr += *x_ptr - (cx + 1);
    *x_ptr = cx + 1;

    /* ... to the right. */
    for ( cx = *x_ptr + *w_ptr;
          cx < x + w &&
              CheckSolidTile(cx, *y_ptr, 1, *h_ptr, &colorValue, TRUE);
          cx++ );
    *w_ptr += cx - (*x_ptr + *w_ptr);
}

/*
 * Check if a rectangle is all of the same color. If needSameColor is
 * set to non-zero, then also check that its color equals to the
 * *colorPtr value. The result is 1 if the test is successfull, and in
 * that case new color will be stored in *colorPtr.
 */

static Bool
CheckSolidTile(x, y, w, h, colorPtr, needSameColor)
    int x, y, w, h;
    CARD32 *colorPtr;
    Bool needSameColor;
{
    switch(rfbServerFormat.bitsPerPixel) {
    case 32:
        return CheckSolidTile32(x, y, w, h, colorPtr, needSameColor);
    case 16:
        return CheckSolidTile16(x, y, w, h, colorPtr, needSameColor);
    default:
        return CheckSolidTile8(x, y, w, h, colorPtr, needSameColor);
    }
}

#define DEFINE_CHECK_SOLID_FUNCTION(bpp)                                      \
                                                                              \
static Bool                                                                   \
CheckSolidTile##bpp(x, y, w, h, colorPtr, needSameColor)                      \
    int x, y, w, h;                                                           \
    CARD32 *colorPtr;                                                         \
    Bool needSameColor;                                                       \
{                                                                             \
    CARD##bpp *fbptr;                                                         \
    CARD##bpp colorValue;                                                     \
    int dx, dy;                                                               \
                                                                              \
    fbptr = (CARD##bpp *)                                                     \
        &rfbScreen.pfbMemory[y * rfbScreen.paddedWidthInBytes + x * (bpp/8)]; \
                                                                              \
    colorValue = *fbptr;                                                      \
    if (needSameColor && (CARD32)colorValue != *colorPtr)                     \
        return FALSE;                                                         \
                                                                              \
    for (dy = 0; dy < h; dy++) {                                              \
        for (dx = 0; dx < w; dx++) {                                          \
            if (colorValue != fbptr[dx])                                      \
                return FALSE;                                                 \
        }                                                                     \
        fbptr = (CARD##bpp *)((CARD8 *)fbptr + rfbScreen.paddedWidthInBytes); \
    }                                                                         \
                                                                              \
    *colorPtr = (CARD32)colorValue;                                           \
    return TRUE;                                                              \
}

DEFINE_CHECK_SOLID_FUNCTION(8)
DEFINE_CHECK_SOLID_FUNCTION(16)
DEFINE_CHECK_SOLID_FUNCTION(32)

static Bool
SendRectSimple(cl, x, y, w, h)
    rfbClientPtr cl;
    int x, y, w, h;
{
    int maxBeforeSize, maxAfterSize;
    int maxRectSize, maxRectWidth;
    int subrectMaxWidth, subrectMaxHeight;
    int dx, dy;
    int rw, rh;

    maxRectSize = tightConf[compressLevel].maxRectSize;
    maxRectWidth = tightConf[compressLevel].maxRectWidth;

    maxBeforeSize = maxRectSize * (cl->format.bitsPerPixel / 8);
    maxAfterSize = maxBeforeSize + (maxBeforeSize + 99) / 100 + 12;

    if (tightBeforeBufSize < maxBeforeSize) {
        tightBeforeBufSize = maxBeforeSize;
        if (tightBeforeBuf == NULL)
            tightBeforeBuf = (char *)xalloc(tightBeforeBufSize);
        else
            tightBeforeBuf = (char *)xrealloc(tightBeforeBuf,
                                              tightBeforeBufSize);
    }

    if (tightAfterBufSize < maxAfterSize) {
        tightAfterBufSize = maxAfterSize;
        if (tightAfterBuf == NULL)
            tightAfterBuf = (char *)xalloc(tightAfterBufSize);
        else
            tightAfterBuf = (char *)xrealloc(tightAfterBuf,
                                             tightAfterBufSize);
    }

    if (w > maxRectWidth || w * h > maxRectSize) {
        subrectMaxWidth = (w > maxRectWidth) ? maxRectWidth : w;
        subrectMaxHeight = maxRectSize / subrectMaxWidth;

        for (dy = 0; dy < h; dy += subrectMaxHeight) {
            for (dx = 0; dx < w; dx += maxRectWidth) {
                rw = (dx + maxRectWidth < w) ? maxRectWidth : w - dx;
                rh = (dy + subrectMaxHeight < h) ? subrectMaxHeight : h - dy;
                if (!SendSubrect(cl, x+dx, y+dy, rw, rh))
                    return FALSE;
            }
        }
    } else {
        if (!SendSubrect(cl, x, y, w, h))
            return FALSE;
    }

    return TRUE;
}

static Bool
SendSubrect(cl, x, y, w, h)
    rfbClientPtr cl;
    int x, y, w, h;
{
    char *fbptr;
    Bool success = FALSE;

    /* Send pending data if there is more than 128 bytes. */
    if (ublen > 128) {
        if (!rfbSendUpdateBuf(cl))
            return FALSE;
    }

    if (!SendTightHeader(cl, x, y, w, h))
        return FALSE;

    fbptr = (rfbScreen.pfbMemory + (rfbScreen.paddedWidthInBytes * y)
             + (x * (rfbScreen.bitsPerPixel / 8)));

    if (subsampLevel == TJ_GRAYSCALE && qualityLevel != -1)
        return SendJpegRect(cl, x, y, w, h, qualityLevel);

    paletteMaxColors = w * h / tightConf[compressLevel].idxMaxColorsDivisor;
    if(qualityLevel != -1)
        paletteMaxColors = 24;
    if ( paletteMaxColors < 2 &&
         w * h >= tightConf[compressLevel].monoMinRectSize ) {
        paletteMaxColors = 2;
    }

    if (cl->format.bitsPerPixel == rfbServerFormat.bitsPerPixel &&
        cl->format.redMax == rfbServerFormat.redMax &&
        cl->format.greenMax == rfbServerFormat.greenMax && 
        cl->format.blueMax == rfbServerFormat.blueMax &&
        cl->format.bitsPerPixel >= 16) {

        /* This is so we can avoid translating the pixels when compressing
           with JPEG, since it is unnecessary */
        switch (cl->format.bitsPerPixel) {
        case 16:
            FastFillPalette16(cl, (CARD16 *)fbptr, w,
                              rfbScreen.paddedWidthInBytes/2, h);
            break;
        default:
            FastFillPalette32(cl, (CARD32 *)fbptr, w,
                              rfbScreen.paddedWidthInBytes/4, h);
        }

        if(paletteNumColors != 0 || qualityLevel == -1) {
            (*cl->translateFn)(cl->translateLookupTable, &rfbServerFormat,
                               &cl->format, fbptr, tightBeforeBuf,
                               rfbScreen.paddedWidthInBytes, w, h);
        }
    }
    else {
        (*cl->translateFn)(cl->translateLookupTable, &rfbServerFormat,
                           &cl->format, fbptr, tightBeforeBuf,
                           rfbScreen.paddedWidthInBytes, w, h);

        switch (cl->format.bitsPerPixel) {
        case 8:
            FillPalette8(w * h);
            break;
        case 16:
            FillPalette16(w * h);
            break;
        default:
            FillPalette32(w * h);
        }
    }

    switch (paletteNumColors) {
    case 0:
        /* Truecolor image */
        if (qualityLevel != -1) {
            success = SendJpegRect(cl, x, y, w, h, qualityLevel);
        } else {
            success = SendFullColorRect(cl, w, h);
        }
        break;
    case 1:
        /* Solid rectangle */
        success = SendSolidRect(cl);
        break;
    case 2:
        /* Two-color rectangle */
        success = SendMonoRect(cl, w, h);
        break;
    default:
        /* Up to 256 different colors */
        success = SendIndexedRect(cl, w, h);
    }
    return success;
}

static Bool
SendTightHeader(cl, x, y, w, h)
    rfbClientPtr cl;
    int x, y, w, h;
{
    rfbFramebufferUpdateRectHeader rect;

    if (ublen + sz_rfbFramebufferUpdateRectHeader > UPDATE_BUF_SIZE) {
        if (!rfbSendUpdateBuf(cl))
            return FALSE;
    }

    rect.r.x = Swap16IfLE(x);
    rect.r.y = Swap16IfLE(y);
    rect.r.w = Swap16IfLE(w);
    rect.r.h = Swap16IfLE(h);
    rect.encoding = Swap32IfLE(rfbEncodingTight);

    memcpy(&updateBuf[ublen], (char *)&rect,
           sz_rfbFramebufferUpdateRectHeader);
    ublen += sz_rfbFramebufferUpdateRectHeader;

    cl->rfbRectanglesSent[rfbEncodingTight]++;
    cl->rfbBytesSent[rfbEncodingTight] += sz_rfbFramebufferUpdateRectHeader;

    return TRUE;
}

/*
 * Subencoding implementations.
 */

static Bool
SendSolidRect(cl)
    rfbClientPtr cl;
{
    int len;

    if (usePixelFormat24) {
        Pack24(tightBeforeBuf, &cl->format, 1);
        len = 3;
    } else
        len = cl->format.bitsPerPixel / 8;

    if (ublen + 1 + len > UPDATE_BUF_SIZE) {
        if (!rfbSendUpdateBuf(cl))
            return FALSE;
    }

    updateBuf[ublen++] = (char)(rfbTightFill << 4);
    memcpy (&updateBuf[ublen], tightBeforeBuf, len);
    ublen += len;

    cl->rfbBytesSent[rfbEncodingTight] += len + 1;

    return TRUE;
}

static Bool
SendMonoRect(cl, w, h)
    rfbClientPtr cl;
    int w, h;
{
    int streamId = 1;
    int paletteLen, dataLen;

    if ( (ublen + TIGHT_MIN_TO_COMPRESS + 6 +
          2 * cl->format.bitsPerPixel / 8) > UPDATE_BUF_SIZE ) {
        if (!rfbSendUpdateBuf(cl))
            return FALSE;
    }

    /* Prepare tight encoding header. */
    dataLen = (w + 7) / 8;
    dataLen *= h;

    if (tightConf[compressLevel].monoZlibLevel == 0)
        updateBuf[ublen++] = (char)((rfbTightNoZlib | rfbTightExplicitFilter) << 4);
    else
        updateBuf[ublen++] = (streamId | rfbTightExplicitFilter) << 4;
    updateBuf[ublen++] = rfbTightFilterPalette;
    updateBuf[ublen++] = 1;

    /* Prepare palette, convert image. */
    switch (cl->format.bitsPerPixel) {

    case 32:
        EncodeMonoRect32((CARD8 *)tightBeforeBuf, w, h);

        ((CARD32 *)tightAfterBuf)[0] = monoBackground;
        ((CARD32 *)tightAfterBuf)[1] = monoForeground;
        if (usePixelFormat24) {
            Pack24(tightAfterBuf, &cl->format, 2);
            paletteLen = 6;
        } else
            paletteLen = 8;

        memcpy(&updateBuf[ublen], tightAfterBuf, paletteLen);
        ublen += paletteLen;
        cl->rfbBytesSent[rfbEncodingTight] += 3 + paletteLen;
        break;

    case 16:
        EncodeMonoRect16((CARD8 *)tightBeforeBuf, w, h);

        ((CARD16 *)tightAfterBuf)[0] = (CARD16)monoBackground;
        ((CARD16 *)tightAfterBuf)[1] = (CARD16)monoForeground;

        memcpy(&updateBuf[ublen], tightAfterBuf, 4);
        ublen += 4;
        cl->rfbBytesSent[rfbEncodingTight] += 7;
        break;

    default:
        EncodeMonoRect8((CARD8 *)tightBeforeBuf, w, h);

        updateBuf[ublen++] = (char)monoBackground;
        updateBuf[ublen++] = (char)monoForeground;
        cl->rfbBytesSent[rfbEncodingTight] += 5;
    }

    return CompressData(cl, streamId, dataLen,
                        tightConf[compressLevel].monoZlibLevel,
                        Z_DEFAULT_STRATEGY);
}

static Bool
SendIndexedRect(cl, w, h)
    rfbClientPtr cl;
    int w, h;
{
    int streamId = 2;
    int i, entryLen;

    if ( (ublen + TIGHT_MIN_TO_COMPRESS + 6 +
          paletteNumColors * cl->format.bitsPerPixel / 8) > UPDATE_BUF_SIZE ) {
        if (!rfbSendUpdateBuf(cl))
            return FALSE;
    }

    /* Prepare tight encoding header. */
    if (tightConf[compressLevel].idxZlibLevel == 0)
        updateBuf[ublen++] = (char)((rfbTightNoZlib | rfbTightExplicitFilter) << 4);
    else
        updateBuf[ublen++] = (streamId | rfbTightExplicitFilter) << 4;
    updateBuf[ublen++] = rfbTightFilterPalette;
    updateBuf[ublen++] = (char)(paletteNumColors - 1);

    /* Prepare palette, convert image. */
    switch (cl->format.bitsPerPixel) {

    case 32:
        EncodeIndexedRect32((CARD8 *)tightBeforeBuf, w * h);

        for (i = 0; i < paletteNumColors; i++) {
            ((CARD32 *)tightAfterBuf)[i] =
                palette.entry[i].listNode->rgb;
        }
        if (usePixelFormat24) {
            Pack24(tightAfterBuf, &cl->format, paletteNumColors);
            entryLen = 3;
        } else
            entryLen = 4;

        memcpy(&updateBuf[ublen], tightAfterBuf, paletteNumColors * entryLen);
        ublen += paletteNumColors * entryLen;
        cl->rfbBytesSent[rfbEncodingTight] += 3 + paletteNumColors * entryLen;
        break;

    case 16:
        EncodeIndexedRect16((CARD8 *)tightBeforeBuf, w * h);

        for (i = 0; i < paletteNumColors; i++) {
            ((CARD16 *)tightAfterBuf)[i] =
                (CARD16)palette.entry[i].listNode->rgb;
        }

        memcpy(&updateBuf[ublen], tightAfterBuf, paletteNumColors * 2);
        ublen += paletteNumColors * 2;
        cl->rfbBytesSent[rfbEncodingTight] += 3 + paletteNumColors * 2;
        break;

    default:
        return FALSE;           /* Should never happen. */
    }

    return CompressData(cl, streamId, w * h,
                        tightConf[compressLevel].idxZlibLevel,
                        Z_DEFAULT_STRATEGY);
}

static Bool
SendFullColorRect(cl, w, h)
    rfbClientPtr cl;
    int w, h;
{
    int streamId = 0;
    int len;

    if (ublen + TIGHT_MIN_TO_COMPRESS + 1 > UPDATE_BUF_SIZE) {
        if (!rfbSendUpdateBuf(cl))
            return FALSE;
    }

    if (tightConf[compressLevel].rawZlibLevel == 0)
        updateBuf[ublen++] = (char)(rfbTightNoZlib << 4);
    else
        updateBuf[ublen++] = 0x00;  /* stream id = 0, no flushing, no filter */
    cl->rfbBytesSent[rfbEncodingTight]++;

    if (usePixelFormat24) {
        Pack24(tightBeforeBuf, &cl->format, w * h);
        len = 3;
    } else
        len = cl->format.bitsPerPixel / 8;

    return CompressData(cl, streamId, w * h * len,
                        tightConf[compressLevel].rawZlibLevel,
                        Z_DEFAULT_STRATEGY);
}

static Bool
CompressData(cl, streamId, dataLen, zlibLevel, zlibStrategy)
    rfbClientPtr cl;
    int streamId, dataLen, zlibLevel, zlibStrategy;
{
    z_streamp pz;
    int err, i;

    if (dataLen < TIGHT_MIN_TO_COMPRESS) {
        memcpy(&updateBuf[ublen], tightBeforeBuf, dataLen);
        ublen += dataLen;
        cl->rfbBytesSent[rfbEncodingTight] += dataLen;
        return TRUE;
    }

    if (zlibLevel == 0)
        return SendCompressedData (cl, tightBeforeBuf, dataLen);

    pz = &cl->zsStruct[streamId];

    /* Initialize compression stream if needed. */
    if (!cl->zsActive[streamId]) {
        pz->zalloc = Z_NULL;
        pz->zfree = Z_NULL;
        pz->opaque = Z_NULL;

        err = deflateInit2 (pz, zlibLevel, Z_DEFLATED, MAX_WBITS,
                            MAX_MEM_LEVEL, zlibStrategy);
        if (err != Z_OK)
            return FALSE;

        cl->zsActive[streamId] = TRUE;
        cl->zsLevel[streamId] = zlibLevel;
    }

    /* Prepare buffer pointers. */
    pz->next_in = (Bytef *)tightBeforeBuf;
    pz->avail_in = dataLen;
    pz->next_out = (Bytef *)tightAfterBuf;
    pz->avail_out = tightAfterBufSize;

    /* Change compression parameters if needed. */
    if (zlibLevel != cl->zsLevel[streamId]) {
        if (deflateParams (pz, zlibLevel, zlibStrategy) != Z_OK) {
            return FALSE;
        }
        cl->zsLevel[streamId] = zlibLevel;
    }

    /* Actual compression. */
    if ( deflate (pz, Z_SYNC_FLUSH) != Z_OK ||
         pz->avail_in != 0 || pz->avail_out == 0 ) {
        return FALSE;
    }

    return SendCompressedData(cl, tightAfterBuf,
        tightAfterBufSize - pz->avail_out);
}

static Bool SendCompressedData(cl, buf, compressedLen)
    rfbClientPtr cl;
    char *buf;
    int compressedLen;
{
    int i, portionLen;

    updateBuf[ublen++] = compressedLen & 0x7F;
    cl->rfbBytesSent[rfbEncodingTight]++;
    if (compressedLen > 0x7F) {
        updateBuf[ublen-1] |= 0x80;
        updateBuf[ublen++] = compressedLen >> 7 & 0x7F;
        cl->rfbBytesSent[rfbEncodingTight]++;
        if (compressedLen > 0x3FFF) {
            updateBuf[ublen-1] |= 0x80;
            updateBuf[ublen++] = compressedLen >> 14 & 0xFF;
            cl->rfbBytesSent[rfbEncodingTight]++;
        }
    }

    portionLen = UPDATE_BUF_SIZE;
    for (i = 0; i < compressedLen; i += portionLen) {
        if (i + portionLen > compressedLen) {
            portionLen = compressedLen - i;
        }
        if (ublen + portionLen > UPDATE_BUF_SIZE) {
            if (!rfbSendUpdateBuf(cl))
                return FALSE;
        }
        memcpy(&updateBuf[ublen], &buf[i], portionLen);
        ublen += portionLen;
    }
    cl->rfbBytesSent[rfbEncodingTight] += compressedLen;
    return TRUE;
}

/*
 * Code to determine how many different colors used in rectangle.
 */

static void
FillPalette8(count)
    int count;
{
    CARD8 *data = (CARD8 *)tightBeforeBuf;
    CARD8 c0, c1;
    int i, n0, n1;

    paletteNumColors = 0;

    c0 = data[0];
    for (i = 1; i < count && data[i] == c0; i++);
    if (i == count) {
        paletteNumColors = 1;
        return;                 /* Solid rectangle */
    }

    if (paletteMaxColors < 2)
        return;

    n0 = i;
    c1 = data[i];
    n1 = 0;
    for (i++; i < count; i++) {
        if (data[i] == c0) {
            n0++;
        } else if (data[i] == c1) {
            n1++;
        } else
            break;
    }
    if (i == count) {
        if (n0 > n1) {
            monoBackground = (CARD32)c0;
            monoForeground = (CARD32)c1;
        } else {
            monoBackground = (CARD32)c1;
            monoForeground = (CARD32)c0;
        }
        paletteNumColors = 2;   /* Two colors */
    }
}

#define DEFINE_FILL_PALETTE_FUNCTION(bpp)                               \
                                                                        \
static void                                                             \
FillPalette##bpp(count)                                                 \
    int count;                                                          \
{                                                                       \
    CARD##bpp *data = (CARD##bpp *)tightBeforeBuf;                      \
    CARD##bpp c0, c1, ci;                                               \
    int i, n0, n1, ni;                                                  \
                                                                        \
    c0 = data[0];                                                       \
    for (i = 1; i < count && data[i] == c0; i++);                       \
    if (i >= count) {                                                   \
        paletteNumColors = 1;   /* Solid rectangle */                   \
        return;                                                         \
    }                                                                   \
                                                                        \
    if (paletteMaxColors < 2) {                                         \
        paletteNumColors = 0;   /* Full-color encoding preferred */     \
        return;                                                         \
    }                                                                   \
                                                                        \
    n0 = i;                                                             \
    c1 = data[i];                                                       \
    n1 = 0;                                                             \
    for (i++; i < count; i++) {                                         \
        ci = data[i];                                                   \
        if (ci == c0) {                                                 \
            n0++;                                                       \
        } else if (ci == c1) {                                          \
            n1++;                                                       \
        } else                                                          \
            break;                                                      \
    }                                                                   \
    if (i >= count) {                                                   \
        if (n0 > n1) {                                                  \
            monoBackground = (CARD32)c0;                                \
            monoForeground = (CARD32)c1;                                \
        } else {                                                        \
            monoBackground = (CARD32)c1;                                \
            monoForeground = (CARD32)c0;                                \
        }                                                               \
        paletteNumColors = 2;   /* Two colors */                        \
        return;                                                         \
    }                                                                   \
                                                                        \
    PaletteReset();                                                     \
    PaletteInsert (c0, (CARD32)n0, bpp);                                \
    PaletteInsert (c1, (CARD32)n1, bpp);                                \
                                                                        \
    ni = 1;                                                             \
    for (i++; i < count; i++) {                                         \
        if (data[i] == ci) {                                            \
            ni++;                                                       \
        } else {                                                        \
            if (!PaletteInsert (ci, (CARD32)ni, bpp))                   \
                return;                                                 \
            ci = data[i];                                               \
            ni = 1;                                                     \
        }                                                               \
    }                                                                   \
    PaletteInsert (ci, (CARD32)ni, bpp);                                \
}

DEFINE_FILL_PALETTE_FUNCTION(16)
DEFINE_FILL_PALETTE_FUNCTION(32)

#define DEFINE_FAST_FILL_PALETTE_FUNCTION(bpp)                          \
                                                                        \
static void                                                             \
FastFillPalette##bpp(cl, data, w, pitch, h)                             \
    rfbClientPtr cl;                                                    \
    CARD##bpp *data;                                                    \
    int w, pitch, h;                                                    \
{                                                                       \
    CARD##bpp c0, c1, ci, mask, c0t, c1t, cit;                          \
    int i, j, i2, j2, n0, n1, ni;                                       \
                                                                        \
    if (cl->translateFn != rfbTranslateNone) {                          \
        mask = rfbServerFormat.redMax << rfbServerFormat.redShift;      \
        mask |= rfbServerFormat.greenMax << rfbServerFormat.greenShift; \
        mask |= rfbServerFormat.blueMax << rfbServerFormat.blueShift;   \
    } else mask = ~0;                                                   \
                                                                        \
    c0 = data[0] & mask;                                                \
    for (j = 0; j < h; j++) {                                           \
        for (i = 0; i < w; i++) {                                       \
            if ((data[j * pitch + i] & mask) != c0)                     \
                goto done;                                              \
        }                                                               \
    }                                                                   \
    done:                                                               \
    if (j >= h) {                                                       \
        paletteNumColors = 1;   /* Solid rectangle */                   \
        return;                                                         \
    }                                                                   \
    if (paletteMaxColors < 2) {                                         \
        paletteNumColors = 0;   /* Full-color encoding preferred */     \
        return;                                                         \
    }                                                                   \
                                                                        \
    n0 = j * w + i;                                                     \
    c1 = data[j * pitch + i] & mask;                                    \
    n1 = 0;                                                             \
    i++;  if (i >= w) {i = 0;  j++;}                                    \
    for (j2 = j; j2 < h; j2++) {                                        \
        for (i2 = i; i2 < w; i2++) {                                    \
            ci = data[j2 * pitch + i2] & mask;                          \
            if (ci == c0) {                                             \
                n0++;                                                   \
            } else if (ci == c1) {                                      \
                n1++;                                                   \
            } else                                                      \
                goto done2;                                             \
        }                                                               \
        i = 0;                                                          \
    }                                                                   \
    done2:                                                              \
    (*cl->translateFn)(cl->translateLookupTable, &rfbServerFormat,      \
                       &cl->format, (char *)&c0, (char *)&c0t, bpp/8,   \
                       1, 1);                                           \
    (*cl->translateFn)(cl->translateLookupTable, &rfbServerFormat,      \
                       &cl->format, (char *)&c1, (char *)&c1t, bpp/8,   \
                       1, 1);                                           \
    if (j2 >= h) {                                                      \
        if (n0 > n1) {                                                  \
            monoBackground = (CARD32)c0t;                               \
            monoForeground = (CARD32)c1t;                               \
        } else {                                                        \
            monoBackground = (CARD32)c1t;                               \
            monoForeground = (CARD32)c0t;                               \
        }                                                               \
        paletteNumColors = 2;   /* Two colors */                        \
        return;                                                         \
    }                                                                   \
                                                                        \
    PaletteReset();                                                     \
    PaletteInsert (c0t, (CARD32)n0, bpp);                               \
    PaletteInsert (c1t, (CARD32)n1, bpp);                               \
                                                                        \
    ni = 1;                                                             \
    i2++;  if (i2 >= w) {i2 = 0;  j2++;}                                \
    for (j = j2; j < h; j++) {                                          \
        for (i = i2; i < w; i++) {                                      \
            if ((data[j * pitch + i] & mask) == ci) {                   \
                ni++;                                                   \
            } else {                                                    \
                (*cl->translateFn)(cl->translateLookupTable,            \
                                   &rfbServerFormat, &cl->format,       \
                                   (char *)&ci, (char *)&cit, bpp/8,    \
                                   1, 1);                               \
                if (!PaletteInsert (cit, (CARD32)ni, bpp))              \
                    return;                                             \
                ci = data[j * pitch + i] & mask;                        \
                ni = 1;                                                 \
            }                                                           \
        }                                                               \
        i2 = 0;                                                         \
    }                                                                   \
                                                                        \
    (*cl->translateFn)(cl->translateLookupTable, &rfbServerFormat,      \
                       &cl->format, (char *)&ci, (char *)&cit, bpp/8,   \
                       1, 1);                                           \
    PaletteInsert (cit, (CARD32)ni, bpp);                               \
}

DEFINE_FAST_FILL_PALETTE_FUNCTION(16)
DEFINE_FAST_FILL_PALETTE_FUNCTION(32)


/*
 * Functions to operate with palette structures.
 */

#define HASH_FUNC16(rgb) ((int)((((rgb) >> 8) + (rgb)) & 0xFF))
#define HASH_FUNC32(rgb) ((int)((((rgb) >> 16) + ((rgb) >> 8)) & 0xFF))

static void
PaletteReset(void)
{
    paletteNumColors = 0;
    memset(palette.hash, 0, 256 * sizeof(COLOR_LIST *));
}

static int
PaletteInsert(rgb, numPixels, bpp)
    CARD32 rgb;
    int numPixels;
    int bpp;
{
    COLOR_LIST *pnode;
    COLOR_LIST *prev_pnode = NULL;
    int hash_key, idx, new_idx, count;

    hash_key = (bpp == 16) ? HASH_FUNC16(rgb) : HASH_FUNC32(rgb);

    pnode = palette.hash[hash_key];

    while (pnode != NULL) {
        if (pnode->rgb == rgb) {
            /* Such palette entry already exists. */
            new_idx = idx = pnode->idx;
            count = palette.entry[idx].numPixels + numPixels;
            if (new_idx && palette.entry[new_idx-1].numPixels < count) {
                do {
                    palette.entry[new_idx] = palette.entry[new_idx-1];
                    palette.entry[new_idx].listNode->idx = new_idx;
                    new_idx--;
                }
                while (new_idx && palette.entry[new_idx-1].numPixels < count);
                palette.entry[new_idx].listNode = pnode;
                pnode->idx = new_idx;
            }
            palette.entry[new_idx].numPixels = count;
            return paletteNumColors;
        }
        prev_pnode = pnode;
        pnode = pnode->next;
    }

    /* Check if palette is full. */
    if (paletteNumColors == 256 || paletteNumColors == paletteMaxColors) {
        paletteNumColors = 0;
        return 0;
    }

    /* Move palette entries with lesser pixel counts. */
    for ( idx = paletteNumColors;
          idx > 0 && palette.entry[idx-1].numPixels < numPixels;
          idx-- ) {
        palette.entry[idx] = palette.entry[idx-1];
        palette.entry[idx].listNode->idx = idx;
    }

    /* Add new palette entry into the freed slot. */
    pnode = &palette.list[paletteNumColors];
    if (prev_pnode != NULL) {
        prev_pnode->next = pnode;
    } else {
        palette.hash[hash_key] = pnode;
    }
    pnode->next = NULL;
    pnode->idx = idx;
    pnode->rgb = rgb;
    palette.entry[idx].listNode = pnode;
    palette.entry[idx].numPixels = numPixels;

    return (++paletteNumColors);
}


/*
 * Converting 32-bit color samples into 24-bit colors.
 * Should be called only when redMax, greenMax and blueMax are 255.
 * Color components assumed to be byte-aligned.
 */

static void Pack24(buf, fmt, count)
    char *buf;
    rfbPixelFormat *fmt;
    int count;
{
    CARD32 *buf32;
    CARD32 pix;
    int r_shift, g_shift, b_shift;

    buf32 = (CARD32 *)buf;

    if (!rfbServerFormat.bigEndian == !fmt->bigEndian) {
        r_shift = fmt->redShift;
        g_shift = fmt->greenShift;
        b_shift = fmt->blueShift;
    } else {
        r_shift = 24 - fmt->redShift;
        g_shift = 24 - fmt->greenShift;
        b_shift = 24 - fmt->blueShift;
    }

    while (count--) {
        pix = *buf32++;
        *buf++ = (char)(pix >> r_shift);
        *buf++ = (char)(pix >> g_shift);
        *buf++ = (char)(pix >> b_shift);
    }
}


/*
 * Converting truecolor samples into palette indices.
 */

#define DEFINE_IDX_ENCODE_FUNCTION(bpp)                                 \
                                                                        \
static void                                                             \
EncodeIndexedRect##bpp(buf, count)                                      \
    CARD8 *buf;                                                         \
    int count;                                                          \
{                                                                       \
    COLOR_LIST *pnode;                                                  \
    CARD##bpp *src;                                                     \
    CARD##bpp rgb;                                                      \
    int rep = 0;                                                        \
                                                                        \
    src = (CARD##bpp *) buf;                                            \
                                                                        \
    while (count--) {                                                   \
        rgb = *src++;                                                   \
        while (count && *src == rgb) {                                  \
            rep++, src++, count--;                                      \
        }                                                               \
        pnode = palette.hash[HASH_FUNC##bpp(rgb)];                      \
        while (pnode != NULL) {                                         \
            if ((CARD##bpp)pnode->rgb == rgb) {                         \
                *buf++ = (CARD8)pnode->idx;                             \
                while (rep) {                                           \
                    *buf++ = (CARD8)pnode->idx;                         \
                    rep--;                                              \
                }                                                       \
                break;                                                  \
            }                                                           \
            pnode = pnode->next;                                        \
        }                                                               \
    }                                                                   \
}

DEFINE_IDX_ENCODE_FUNCTION(16)
DEFINE_IDX_ENCODE_FUNCTION(32)

#define DEFINE_MONO_ENCODE_FUNCTION(bpp)                                \
                                                                        \
static void                                                             \
EncodeMonoRect##bpp(buf, w, h)                                          \
    CARD8 *buf;                                                         \
    int w, h;                                                           \
{                                                                       \
    CARD##bpp *ptr;                                                     \
    CARD##bpp bg;                                                       \
    unsigned int value, mask;                                           \
    int aligned_width;                                                  \
    int x, y, bg_bits;                                                  \
                                                                        \
    ptr = (CARD##bpp *) buf;                                            \
    bg = (CARD##bpp) monoBackground;                                    \
    aligned_width = w - w % 8;                                          \
                                                                        \
    for (y = 0; y < h; y++) {                                           \
        for (x = 0; x < aligned_width; x += 8) {                        \
            for (bg_bits = 0; bg_bits < 8; bg_bits++) {                 \
                if (*ptr++ != bg)                                       \
                    break;                                              \
            }                                                           \
            if (bg_bits == 8) {                                         \
                *buf++ = 0;                                             \
                continue;                                               \
            }                                                           \
            mask = 0x80 >> bg_bits;                                     \
            value = mask;                                               \
            for (bg_bits++; bg_bits < 8; bg_bits++) {                   \
                mask >>= 1;                                             \
                if (*ptr++ != bg) {                                     \
                    value |= mask;                                      \
                }                                                       \
            }                                                           \
            *buf++ = (CARD8)value;                                      \
        }                                                               \
                                                                        \
        mask = 0x80;                                                    \
        value = 0;                                                      \
        if (x >= w)                                                     \
            continue;                                                   \
                                                                        \
        for (; x < w; x++) {                                            \
            if (*ptr++ != bg) {                                         \
                value |= mask;                                          \
            }                                                           \
            mask >>= 1;                                                 \
        }                                                               \
        *buf++ = (CARD8)value;                                          \
    }                                                                   \
}

DEFINE_MONO_ENCODE_FUNCTION(8)
DEFINE_MONO_ENCODE_FUNCTION(16)
DEFINE_MONO_ENCODE_FUNCTION(32)

/*
 * JPEG compression stuff.
 */

static unsigned long jpegDstDataLen;
static tjhandle j=NULL;

static Bool
SendJpegRect(cl, x, y, w, h, quality)
    rfbClientPtr cl;
    int x, y, w, h;
    int quality;
{
    int dy;
    unsigned char *srcbuf;
    int ps=rfbServerFormat.bitsPerPixel/8;
    int subsamp=subsampLevel2tjsubsamp[subsampLevel];
    unsigned long size=0;
    int flags=0, pitch;
    unsigned char *tmpbuf=NULL;

    if (rfbServerFormat.bitsPerPixel == 8)
        return SendFullColorRect(cl, w, h);


    if(ps<2) {
      rfbLog("Error: JPEG requires 16-bit, 24-bit, or 32-bit pixel format.\n");
      return 0;
    }
    if(!j) {
      if((j=tjInitCompress())==NULL) {
        rfbLog("JPEG Error: %s\n", tjGetErrorStr());  return 0;
      }
    }

    if (tightAfterBufSize < TJBUFSIZE(w,h)) {
        if (tightAfterBuf == NULL)
            tightAfterBuf = (char *)xalloc(TJBUFSIZE(w,h));
        else
            tightAfterBuf = (char *)xrealloc(tightAfterBuf,
                                             TJBUFSIZE(w,h));
        if(!tightAfterBuf) {
            rfbLog("Memory allocation failure!\n");
            return 0;
        }
        tightAfterBufSize = TJBUFSIZE(w,h);
    }

    if (ps == 2) {
        CARD16 *srcptr, pix;
        unsigned char *dst;
        int inRed, inGreen, inBlue, i, j;

        if((tmpbuf=(unsigned char *)malloc(w*h*3))==NULL)
            rfbLog("Memory allocation failure!\n");
        srcptr = (CARD16 *)
            &rfbScreen.pfbMemory[y * rfbScreen.paddedWidthInBytes +
                                 x * ps];
        dst = tmpbuf;
        for(j=0; j<h; j++) {
            CARD16 *srcptr2=srcptr;
            unsigned char *dst2=dst;
            for(i=0; i<w; i++) {
                pix = *srcptr2++;
                inRed = (int)
                    (pix >> rfbServerFormat.redShift   & rfbServerFormat.redMax);
                inGreen = (int)
                    (pix >> rfbServerFormat.greenShift & rfbServerFormat.greenMax);
                inBlue  = (int)
                    (pix >> rfbServerFormat.blueShift  & rfbServerFormat.blueMax);
                *dst2++ = (CARD8)((inRed   * 255 + rfbServerFormat.redMax / 2) /
                          rfbServerFormat.redMax);                          
               	*dst2++ = (CARD8)((inGreen * 255 + rfbServerFormat.greenMax / 2) /
                          rfbServerFormat.greenMax);
                *dst2++ = (CARD8)((inBlue  * 255 + rfbServerFormat.blueMax / 2) /
                          rfbServerFormat.blueMax);
            }
            srcptr+=rfbScreen.paddedWidthInBytes/ps;
            dst+=w*3;
        }
        srcbuf = tmpbuf;
        pitch = w*3;
        ps = 3;
    } else {
        if(rfbServerFormat.bigEndian && ps==4) flags|=TJ_ALPHAFIRST;
        if(rfbServerFormat.redShift==16 && rfbServerFormat.blueShift==0)
            flags|=TJ_BGR;
        if(rfbServerFormat.bigEndian) flags^=TJ_BGR;
        srcbuf=(unsigned char *)&rfbScreen.pfbMemory[y * 
            rfbScreen.paddedWidthInBytes + x * ps];
        pitch=rfbScreen.paddedWidthInBytes;
    }

    if(tjCompress(j, srcbuf, w, pitch, h, ps, (unsigned char *)tightAfterBuf,
      &size, subsamp, quality, flags)==-1) {
      rfbLog("JPEG Error: %s\n", tjGetErrorStr());
      if(tmpbuf) {free(tmpbuf);  tmpbuf=NULL;}
      return 0;
    }
    jpegDstDataLen=(int)size;

    if(tmpbuf) {free(tmpbuf);  tmpbuf=NULL;}

    if (ublen + TIGHT_MIN_TO_COMPRESS + 1 > UPDATE_BUF_SIZE) {
        if (!rfbSendUpdateBuf(cl))
            return FALSE;
    }

    updateBuf[ublen++] = (char)(rfbTightJpeg << 4);
    cl->rfbBytesSent[rfbEncodingTight]++;

    return SendCompressedData(cl, tightAfterBuf, jpegDstDataLen);
}
