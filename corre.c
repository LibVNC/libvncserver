/*
 * corre.c
 *
 * Routines to implement Compact Rise-and-Run-length Encoding (CoRRE).  This
 * code is based on krw's original javatel rfbserver.
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

/*
 * rreBeforeBuf contains pixel data in the client's format.
 * rreAfterBuf contains the RRE encoded version.  If the RRE encoded version is
 * larger than the raw data or if it exceeds rreAfterBufSize then
 * raw encoding is used instead.
 */

static int rreBeforeBufSize = 0;
static char *rreBeforeBuf = NULL;

static int rreAfterBufSize = 0;
static char *rreAfterBuf = NULL;
static int rreAfterBufLen;

static int subrectEncode8(CARD8 *data, int w, int h);
static int subrectEncode16(CARD16 *data, int w, int h);
static int subrectEncode32(CARD32 *data, int w, int h);
static CARD32 getBgColour(char *data, int size, int bpp);
static Bool rfbSendSmallRectEncodingCoRRE(rfbClientPtr cl, int x, int y,
                                          int w, int h);


/*
 * rfbSendRectEncodingCoRRE - send an arbitrary size rectangle using CoRRE
 * encoding.
 */

Bool
rfbSendRectEncodingCoRRE(cl, x, y, w, h)
    rfbClientPtr cl;
    int x, y, w, h;
{
    if (h > cl->correMaxHeight) {
        rfbSendRectEncodingCoRRE(cl, x, y, w, cl->correMaxHeight  );
        rfbSendRectEncodingCoRRE(cl, x, y + cl->correMaxHeight, w,
                                 h - cl->correMaxHeight);
        return FALSE;
    }

    if (w > cl->correMaxWidth) {
        rfbSendRectEncodingCoRRE(cl, x, y, cl->correMaxWidth, h);
        rfbSendRectEncodingCoRRE(cl, x + cl->correMaxWidth, y,
                                 w - cl->correMaxWidth, h);
        return FALSE;
    }

    rfbSendSmallRectEncodingCoRRE(cl, x, y, w, h);
    return TRUE;
}



/*
 * rfbSendSmallRectEncodingCoRRE - send a small (guaranteed < 256x256)
 * rectangle using CoRRE encoding.
 */

static Bool
rfbSendSmallRectEncodingCoRRE(cl, x, y, w, h)
    rfbClientPtr cl;
    int x, y, w, h;
{
    rfbFramebufferUpdateRectHeader rect;
    rfbRREHeader hdr;
    int nSubrects;
    int i;
    char *fbptr = (cl->screen->frameBuffer + (cl->screen->paddedWidthInBytes * y)
                   + (x * (cl->screen->bitsPerPixel / 8)));

    int maxRawSize = (cl->screen->width * cl->screen->height
                      * (cl->format.bitsPerPixel / 8));

    if (rreBeforeBufSize < maxRawSize) {
        rreBeforeBufSize = maxRawSize;
        if (rreBeforeBuf == NULL)
            rreBeforeBuf = (char *)malloc(rreBeforeBufSize);
        else
            rreBeforeBuf = (char *)realloc(rreBeforeBuf, rreBeforeBufSize);
    }

    if (rreAfterBufSize < maxRawSize) {
        rreAfterBufSize = maxRawSize;
        if (rreAfterBuf == NULL)
            rreAfterBuf = (char *)malloc(rreAfterBufSize);
        else
            rreAfterBuf = (char *)realloc(rreAfterBuf, rreAfterBufSize);
    }

    (*cl->translateFn)(cl->translateLookupTable,&(cl->screen->rfbServerFormat),
                       &cl->format, fbptr, rreBeforeBuf,
                       cl->screen->paddedWidthInBytes, w, h);

    switch (cl->format.bitsPerPixel) {
    case 8:
        nSubrects = subrectEncode8((CARD8 *)rreBeforeBuf, w, h);
        break;
    case 16:
        nSubrects = subrectEncode16((CARD16 *)rreBeforeBuf, w, h);
        break;
    case 32:
        nSubrects = subrectEncode32((CARD32 *)rreBeforeBuf, w, h);
        break;
    default:
        rfbLog("getBgColour: bpp %d?\n",cl->format.bitsPerPixel);
        exit(1);
    }
        
    if (nSubrects < 0) {

        /* RRE encoding was too large, use raw */

        return rfbSendRectEncodingRaw(cl, x, y, w, h);
    }

    cl->rfbRectanglesSent[rfbEncodingCoRRE]++;
    cl->rfbBytesSent[rfbEncodingCoRRE] += (sz_rfbFramebufferUpdateRectHeader
                                           + sz_rfbRREHeader + rreAfterBufLen);

    if (cl->ublen + sz_rfbFramebufferUpdateRectHeader + sz_rfbRREHeader
        > UPDATE_BUF_SIZE)
    {
        if (!rfbSendUpdateBuf(cl))
            return FALSE;
    }

    rect.r.x = Swap16IfLE(x);
    rect.r.y = Swap16IfLE(y);
    rect.r.w = Swap16IfLE(w);
    rect.r.h = Swap16IfLE(h);
    rect.encoding = Swap32IfLE(rfbEncodingCoRRE);

    memcpy(&cl->updateBuf[cl->ublen], (char *)&rect,
           sz_rfbFramebufferUpdateRectHeader);
    cl->ublen += sz_rfbFramebufferUpdateRectHeader;

    hdr.nSubrects = Swap32IfLE(nSubrects);

    memcpy(&cl->updateBuf[cl->ublen], (char *)&hdr, sz_rfbRREHeader);
    cl->ublen += sz_rfbRREHeader;

    for (i = 0; i < rreAfterBufLen;) {

        int bytesToCopy = UPDATE_BUF_SIZE - cl->ublen;

        if (i + bytesToCopy > rreAfterBufLen) {
            bytesToCopy = rreAfterBufLen - i;
        }

        memcpy(&cl->updateBuf[cl->ublen], &rreAfterBuf[i], bytesToCopy);

        cl->ublen += bytesToCopy;
        i += bytesToCopy;

        if (cl->ublen == UPDATE_BUF_SIZE) {
            if (!rfbSendUpdateBuf(cl))
                return FALSE;
        }
    }

    return TRUE;
}



/*
 * subrectEncode() encodes the given multicoloured rectangle as a background 
 * colour overwritten by single-coloured rectangles.  It returns the number 
 * of subrectangles in the encoded buffer, or -1 if subrect encoding won't
 * fit in the buffer.  It puts the encoded rectangles in rreAfterBuf.  The
 * single-colour rectangle partition is not optimal, but does find the biggest
 * horizontal or vertical rectangle top-left anchored to each consecutive 
 * coordinate position.
 *
 * The coding scheme is simply [<bgcolour><subrect><subrect>...] where each 
 * <subrect> is [<colour><x><y><w><h>].
 */

#define DEFINE_SUBRECT_ENCODE(bpp)                                            \
static int                                                                    \
subrectEncode##bpp(data,w,h)                                                  \
    CARD##bpp *data;                                                          \
    int w;                                                                    \
    int h;                                                                    \
{                                                                             \
    CARD##bpp cl;                                                             \
    rfbCoRRERectangle subrect;                                                \
    int x,y;                                                                  \
    int i,j;                                                                  \
    int hx=0,hy,vx=0,vy;                                                      \
    int hyflag;                                                               \
    CARD##bpp *seg;                                                           \
    CARD##bpp *line;                                                          \
    int hw,hh,vw,vh;                                                          \
    int thex,they,thew,theh;                                                  \
    int numsubs = 0;                                                          \
    int newLen;                                                               \
    CARD##bpp bg = (CARD##bpp)getBgColour((char*)data,w*h,bpp);               \
                                                                              \
    *((CARD##bpp*)rreAfterBuf) = bg;                                          \
                                                                              \
    rreAfterBufLen = (bpp/8);                                                 \
                                                                              \
    for (y=0; y<h; y++) {                                                     \
      line = data+(y*w);                                                      \
      for (x=0; x<w; x++) {                                                   \
        if (line[x] != bg) {                                                  \
          cl = line[x];                                                       \
          hy = y-1;                                                           \
          hyflag = 1;                                                         \
          for (j=y; j<h; j++) {                                               \
            seg = data+(j*w);                                                 \
            if (seg[x] != cl) {break;}                                        \
            i = x;                                                            \
            while ((seg[i] == cl) && (i < w)) i += 1;                         \
            i -= 1;                                                           \
            if (j == y) vx = hx = i;                                          \
            if (i < vx) vx = i;                                               \
            if ((hyflag > 0) && (i >= hx)) {hy += 1;} else {hyflag = 0;}      \
          }                                                                   \
          vy = j-1;                                                           \
                                                                              \
          /*  We now have two possible subrects: (x,y,hx,hy) and (x,y,vx,vy)  \
           *  We'll choose the bigger of the two.                             \
           */                                                                 \
          hw = hx-x+1;                                                        \
          hh = hy-y+1;                                                        \
          vw = vx-x+1;                                                        \
          vh = vy-y+1;                                                        \
                                                                              \
          thex = x;                                                           \
          they = y;                                                           \
                                                                              \
          if ((hw*hh) > (vw*vh)) {                                            \
            thew = hw;                                                        \
            theh = hh;                                                        \
          } else {                                                            \
            thew = vw;                                                        \
            theh = vh;                                                        \
          }                                                                   \
                                                                              \
          subrect.x = thex;                                                   \
          subrect.y = they;                                                   \
          subrect.w = thew;                                                   \
          subrect.h = theh;                                                   \
                                                                              \
          newLen = rreAfterBufLen + (bpp/8) + sz_rfbCoRRERectangle;           \
          if ((newLen > (w * h * (bpp/8))) || (newLen > rreAfterBufSize))     \
            return -1;                                                        \
                                                                              \
          numsubs += 1;                                                       \
          *((CARD##bpp*)(rreAfterBuf + rreAfterBufLen)) = cl;                 \
          rreAfterBufLen += (bpp/8);                                          \
          memcpy(&rreAfterBuf[rreAfterBufLen],&subrect,sz_rfbCoRRERectangle); \
          rreAfterBufLen += sz_rfbCoRRERectangle;                             \
                                                                              \
          /*                                                                  \
           * Now mark the subrect as done.                                    \
           */                                                                 \
          for (j=they; j < (they+theh); j++) {                                \
            for (i=thex; i < (thex+thew); i++) {                              \
              data[j*w+i] = bg;                                               \
            }                                                                 \
          }                                                                   \
        }                                                                     \
      }                                                                       \
    }                                                                         \
                                                                              \
    return numsubs;                                                           \
}

DEFINE_SUBRECT_ENCODE(8)
DEFINE_SUBRECT_ENCODE(16)
DEFINE_SUBRECT_ENCODE(32)


/*
 * getBgColour() gets the most prevalent colour in a byte array.
 */
static CARD32
getBgColour(data,size,bpp)
    char *data;
    int size;
    int bpp;
{
    
#define NUMCLRS 256
  
  static int counts[NUMCLRS];
  int i,j,k;

  int maxcount = 0;
  CARD8 maxclr = 0;

  if (bpp != 8) {
    if (bpp == 16) {
      return ((CARD16 *)data)[0];
    } else if (bpp == 32) {
      return ((CARD32 *)data)[0];
    } else {
      rfbLog("getBgColour: bpp %d?\n",bpp);
      exit(1);
    }
  }

  for (i=0; i<NUMCLRS; i++) {
    counts[i] = 0;
  }

  for (j=0; j<size; j++) {
    k = (int)(((CARD8 *)data)[j]);
    if (k >= NUMCLRS) {
      rfbLog("getBgColour: unusual colour = %d\n", k);
      exit(1);
    }
    counts[k] += 1;
    if (counts[k] > maxcount) {
      maxcount = counts[k];
      maxclr = ((CARD8 *)data)[j];
    }
  }
  
  return maxclr;
}
