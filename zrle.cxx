//
// Copyright (C) 2002 RealVNC Ltd.  All Rights Reserved.
//
// This is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this software; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
// USA.
//

//
// zrle.cc
//
// Routines to implement Zlib Run-length Encoding (ZRLE).
//

extern "C" {
#include "rfb.h"
}
#include <rdr/MemOutStream.h>
#include <rdr/ZlibOutStream.h>


#define GET_IMAGE_INTO_BUF(tx,ty,tw,th,buf)                                \
  char *fbptr = (cl->screen->frameBuffer                                   \
		 + (cl->screen->paddedWidthInBytes * ty)                   \
                 + (tx * (cl->screen->bitsPerPixel / 8)));                 \
                                                                           \
  (*cl->translateFn)(cl->translateLookupTable, &cl->screen->rfbServerFormat,\
                     &cl->format, fbptr, (char*)buf,                       \
                     cl->screen->paddedWidthInBytes, tw, th);

#define EXTRA_ARGS , rfbClientPtr cl

#define BPP 8
#include <zrleEncode.h>
#undef BPP
#define BPP 16
#include <zrleEncode.h>
#undef BPP
#define BPP 32
#include <zrleEncode.h>
#define CPIXEL 24A
#include <zrleEncode.h>
#undef CPIXEL
#define CPIXEL 24B
#include <zrleEncode.h>
#undef CPIXEL
#undef BPP


/*
 * zrleBeforeBuf contains pixel data in the client's format.  It must be at
 * least one pixel bigger than the largest tile of pixel data, since the
 * ZRLE encoding algorithm writes to the position one past the end of the pixel
 * data.
 */

static char zrleBeforeBuf[rfbZRLETileWidth * rfbZRLETileHeight * 4 + 4];

static rdr::MemOutStream mos;


/*
 * rfbSendRectEncodingZRLE - send a given rectangle using ZRLE encoding.
 */


Bool rfbSendRectEncodingZRLE(rfbClientPtr cl, int x, int y, int w, int h)
{
  if (!cl->zrleData) cl->zrleData = new rdr::ZlibOutStream;
  rdr::ZlibOutStream* zos = (rdr::ZlibOutStream*)cl->zrleData;
  mos.clear();

  switch (cl->format.bitsPerPixel) {

  case 8:
    zrleEncode8( x, y, w, h, &mos, zos, zrleBeforeBuf, cl);
    break;

  case 16:
    zrleEncode16(x, y, w, h, &mos, zos, zrleBeforeBuf, cl);
    break;

  case 32:
    bool fitsInLS3Bytes
      = ((cl->format.redMax   << cl->format.redShift)   < (1<<24) &&
         (cl->format.greenMax << cl->format.greenShift) < (1<<24) &&
         (cl->format.blueMax  << cl->format.blueShift)  < (1<<24));

    bool fitsInMS3Bytes = (cl->format.redShift   > 7  &&
                           cl->format.greenShift > 7  &&
                           cl->format.blueShift  > 7);

    if ((fitsInLS3Bytes && !cl->format.bigEndian) ||
        (fitsInMS3Bytes && cl->format.bigEndian))
    {
      zrleEncode24A(x, y, w, h, &mos, zos, zrleBeforeBuf, cl);
    }
    else if ((fitsInLS3Bytes && cl->format.bigEndian) ||
             (fitsInMS3Bytes && !cl->format.bigEndian))
    {
      zrleEncode24B(x, y, w, h, &mos, zos, zrleBeforeBuf, cl);
    }
    else
    {
      zrleEncode32(x, y, w, h, &mos, zos, zrleBeforeBuf, cl);
    }
    break;
  }

  cl->rfbRectanglesSent[rfbEncodingZRLE]++;
  cl->rfbBytesSent[rfbEncodingZRLE] += (sz_rfbFramebufferUpdateRectHeader
                                        + sz_rfbZRLEHeader + mos.length());

  if (cl->ublen + sz_rfbFramebufferUpdateRectHeader + sz_rfbZRLEHeader
      > UPDATE_BUF_SIZE)
    {
      if (!rfbSendUpdateBuf(cl))
        return FALSE;
    }

  rfbFramebufferUpdateRectHeader rect;
  rect.r.x = Swap16IfLE(x);
  rect.r.y = Swap16IfLE(y);
  rect.r.w = Swap16IfLE(w);
  rect.r.h = Swap16IfLE(h);
  rect.encoding = Swap32IfLE(rfbEncodingZRLE);

  memcpy(cl->updateBuf+cl->ublen, (char *)&rect,
         sz_rfbFramebufferUpdateRectHeader);
  cl->ublen += sz_rfbFramebufferUpdateRectHeader;

  rfbZRLEHeader hdr;

  hdr.length = Swap32IfLE(mos.length());

  memcpy(cl->updateBuf+cl->ublen, (char *)&hdr, sz_rfbZRLEHeader);
  cl->ublen += sz_rfbZRLEHeader;

  // copy into updateBuf and send from there.  Maybe should send directly?

  for (int i = 0; i < mos.length();) {

    int bytesToCopy = UPDATE_BUF_SIZE - cl->ublen;

    if (i + bytesToCopy > mos.length()) {
      bytesToCopy = mos.length() - i;
    }

    memcpy(cl->updateBuf+cl->ublen, (uint8_t*)mos.data() + i, bytesToCopy);

    cl->ublen += bytesToCopy;
    i += bytesToCopy;

    if (cl->ublen == UPDATE_BUF_SIZE) {
      if (!rfbSendUpdateBuf(cl))
        return FALSE;
    }
  }

  return TRUE;
}


void FreeZrleData(rfbClientPtr cl)
{
  delete (rdr::ZlibOutStream*)cl->zrleData;
}

