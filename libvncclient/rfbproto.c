/*
 *  Copyright (C) 2000-2002 Constantin Kaplinsky.  All Rights Reserved.
 *  Copyright (C) 2000 Tridia Corporation.  All Rights Reserved.
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

/*
 * rfbproto.c - functions to deal with client side of RFB protocol.
 */

#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <rfb/rfbclient.h>
#ifdef HAVE_LIBZ
#include <zlib.h>
#endif
#ifdef HAVE_LIBJPEG
#include <jpeglib.h>
#endif

void FillRectangle(rfbClient* client, int x, int y, int w, int h, uint32_t colour) {
  int i,j;

#define FILL_RECT(BPP) \
    for(j=y*client->width;j<(y+h)*client->width;j+=client->width) \
      for(i=x;i<x+w;i++) \
	((uint##BPP##_t*)client->frameBuffer)[j+i]=colour;

  switch(client->format.bitsPerPixel) {
  case  8: FILL_RECT(8);  break;
  case 16: FILL_RECT(16); break;
  case 32: FILL_RECT(32); break;
  default:
    fprintf(stderr,"Unsupported bitsPerPixel: %d\n",client->format.bitsPerPixel);
  }
}

void CopyRectangle(rfbClient* client, uint8_t* buffer, int x, int y, int w, int h) {
  int i,j;

#define COPY_RECT(BPP) \
  { \
    uint##BPP##_t* _buffer=(uint##BPP##_t*)buffer; \
    for(j=y*client->width;j<(y+h)*client->width;j+=client->width) { \
      for(i=x;i<x+w;i++,_buffer++) \
	((uint##BPP##_t*)client->frameBuffer)[j+i]=*_buffer; \
    } \
  }

  switch(client->format.bitsPerPixel) {
  case  8: COPY_RECT(8);  break;
  case 16: COPY_RECT(16); break;
  case 32: COPY_RECT(32); break;
  default:
    fprintf(stderr,"Unsupported bitsPerPixel: %d\n",client->format.bitsPerPixel);
  }
}

/* TODO: test */
void CopyRectangleFromRectangle(rfbClient* client, int src_x, int src_y, int w, int h, int dest_x, int dest_y) {
  int i,j;

#define COPY_RECT_FROM_RECT(BPP) \
  { \
    uint##BPP##_t* _buffer=((uint##BPP##_t*)client->frameBuffer)+(src_y-dest_y)*client->width+src_x-dest_x; \
    for(j=dest_y*client->width;j<(dest_y+h)*client->width;j+=client->width) { \
      for(i=dest_x;i<dest_x+w;i++) \
	((uint##BPP##_t*)client->frameBuffer)[j+i]=_buffer[j+i]; \
    } \
  }

  switch(client->format.bitsPerPixel) {
  case  8: COPY_RECT_FROM_RECT(8);  break;
  case 16: COPY_RECT_FROM_RECT(16); break;
  case 32: COPY_RECT_FROM_RECT(32); break;
  default:
    fprintf(stderr,"Unsupported bitsPerPixel: %d\n",client->format.bitsPerPixel);
  }
}

static Bool HandleRRE8(rfbClient* client, int rx, int ry, int rw, int rh);
static Bool HandleRRE16(rfbClient* client, int rx, int ry, int rw, int rh);
static Bool HandleRRE32(rfbClient* client, int rx, int ry, int rw, int rh);
static Bool HandleCoRRE8(rfbClient* client, int rx, int ry, int rw, int rh);
static Bool HandleCoRRE16(rfbClient* client, int rx, int ry, int rw, int rh);
static Bool HandleCoRRE32(rfbClient* client, int rx, int ry, int rw, int rh);
static Bool HandleHextile8(rfbClient* client, int rx, int ry, int rw, int rh);
static Bool HandleHextile16(rfbClient* client, int rx, int ry, int rw, int rh);
static Bool HandleHextile32(rfbClient* client, int rx, int ry, int rw, int rh);
static Bool HandleZlib8(rfbClient* client, int rx, int ry, int rw, int rh);
static Bool HandleZlib16(rfbClient* client, int rx, int ry, int rw, int rh);
static Bool HandleZlib32(rfbClient* client, int rx, int ry, int rw, int rh);
static Bool HandleTight8(rfbClient* client, int rx, int ry, int rw, int rh);
static Bool HandleTight16(rfbClient* client, int rx, int ry, int rw, int rh);
static Bool HandleTight32(rfbClient* client, int rx, int ry, int rw, int rh);

static long ReadCompactLen (rfbClient* client);

static void JpegInitSource(j_decompress_ptr cinfo);
static boolean JpegFillInputBuffer(j_decompress_ptr cinfo);
static void JpegSkipInputData(j_decompress_ptr cinfo, long num_bytes);
static void JpegTermSource(j_decompress_ptr cinfo);
static void JpegSetSrcManager(j_decompress_ptr cinfo, uint8_t *compressedData,
                              int compressedLen);

/* The zlib encoding requires expansion/decompression/deflation of the
   compressed data in the "buffer" above into another, result buffer.
   However, the size of the result buffer can be determined precisely
   based on the bitsPerPixel, height and width of the rectangle.  We
   allocate this buffer one time to be the full size of the buffer. */

static int raw_buffer_size = -1;
static char *raw_buffer;

static z_stream decompStream;
static Bool decompStreamInited = FALSE;


/*
 * Variables for the ``tight'' encoding implementation.
 */

/* Separate buffer for compressed data. */
#define ZLIB_BUFFER_SIZE 512
static char zlib_buffer[ZLIB_BUFFER_SIZE];

/* Four independent compression streams for zlib library. */
static z_stream zlibStream[4];
static Bool zlibStreamActive[4] = {
  FALSE, FALSE, FALSE, FALSE
};

/* Filter stuff. Should be initialized by filter initialization code. */
static Bool cutZeros;
static int rectWidth, rectColors;
static char tightPalette[256*4];
static uint8_t tightPrevRow[2048*3*sizeof(uint16_t)];

/* JPEG decoder state. */
static Bool jpegError;


/*
 * ConnectToRFBServer.
 */

Bool
ConnectToRFBServer(rfbClient* client,const char *hostname, int port)
{
  unsigned int host;

  if (!StringToIPAddr(hostname, &host)) {
    fprintf(stderr,"Couldn't convert '%s' to host address\n", hostname);
    return FALSE;
  }

  client->sock = ConnectToTcpAddr(host, port);

  if (client->sock < 0) {
    fprintf(stderr,"Unable to connect to VNC server\n");
    return FALSE;
  }

  return SetNonBlocking(client->sock);
}

static void rfbEncryptBytes(unsigned char *bytes, char *passwd);

/*
 * InitialiseRFBConnection.
 */

Bool
InitialiseRFBConnection(rfbClient* client)
{
  rfbProtocolVersionMsg pv;
  int major,minor;
  uint32_t authScheme, reasonLen, authResult;
  char *reason;
  uint8_t challenge[CHALLENGESIZE];
  char *passwd;
  int i;
  rfbClientInitMsg ci;

  /* if the connection is immediately closed, don't report anything, so
       that pmw's monitor can make test connections */

  if (client->listenSpecified)
    errorMessageOnReadFailure = FALSE;

  if (!ReadFromRFBServer(client, pv, sz_rfbProtocolVersionMsg)) return FALSE;

  errorMessageOnReadFailure = TRUE;

  pv[sz_rfbProtocolVersionMsg] = 0;

  if (sscanf(pv,rfbProtocolVersionFormat,&major,&minor) != 2) {
    fprintf(stderr,"Not a valid VNC server\n");
    return FALSE;
  }

  fprintf(stderr,"VNC server supports protocol version %d.%d (viewer %d.%d)\n",
	  major, minor, rfbProtocolMajorVersion, rfbProtocolMinorVersion);

  major = rfbProtocolMajorVersion;
  minor = rfbProtocolMinorVersion;

  sprintf(pv,rfbProtocolVersionFormat,major,minor);

  if (!WriteExact(client, pv, sz_rfbProtocolVersionMsg)) return FALSE;

  if (!ReadFromRFBServer(client, (char *)&authScheme, 4)) return FALSE;

  authScheme = Swap32IfLE(authScheme);

  switch (authScheme) {

  case rfbConnFailed:
    if (!ReadFromRFBServer(client, (char *)&reasonLen, 4)) return FALSE;
    reasonLen = Swap32IfLE(reasonLen);

    reason = malloc(reasonLen);

    if (!ReadFromRFBServer(client, reason, reasonLen)) return FALSE;

    fprintf(stderr,"VNC connection failed: %.*s\n",(int)reasonLen, reason);
    return FALSE;

  case rfbNoAuth:
    fprintf(stderr,"No authentication needed\n");
    break;

  case rfbVncAuth:
    if (!ReadFromRFBServer(client, (char *)challenge, CHALLENGESIZE)) return FALSE;

    if (client->GetPassword)
      passwd = client->GetPassword(client);

    if ((!passwd) || (strlen(passwd) == 0)) {
      fprintf(stderr,"Reading password failed\n");
      return FALSE;
    }
    if (strlen(passwd) > 8) {
      passwd[8] = '\0';
    }

    rfbEncryptBytes(challenge, passwd);

	/* Lose the password from memory */
    for (i = strlen(passwd); i >= 0; i--) {
      passwd[i] = '\0';
    }

    if (!WriteExact(client, (char *)challenge, CHALLENGESIZE)) return FALSE;

    if (!ReadFromRFBServer(client, (char *)&authResult, 4)) return FALSE;

    authResult = Swap32IfLE(authResult);

    switch (authResult) {
    case rfbVncAuthOK:
      fprintf(stderr,"VNC authentication succeeded\n");
      break;
    case rfbVncAuthFailed:
      fprintf(stderr,"VNC authentication failed\n");
      return FALSE;
    case rfbVncAuthTooMany:
      fprintf(stderr,"VNC authentication failed - too many tries\n");
      return FALSE;
    default:
      fprintf(stderr,"Unknown VNC authentication result: %d\n",
	      (int)authResult);
      return FALSE;
    }
    break;

  default:
    fprintf(stderr,"Unknown authentication scheme from VNC server: %d\n",
	    (int)authScheme);
    return FALSE;
  }

  ci.shared = (client->appData.shareDesktop ? 1 : 0);

  if (!WriteExact(client,  (char *)&ci, sz_rfbClientInitMsg)) return FALSE;

  if (!ReadFromRFBServer(client, (char *)&client->si, sz_rfbServerInitMsg)) return FALSE;

  client->si.framebufferWidth = Swap16IfLE(client->si.framebufferWidth);
  client->si.framebufferHeight = Swap16IfLE(client->si.framebufferHeight);
  client->si.format.redMax = Swap16IfLE(client->si.format.redMax);
  client->si.format.greenMax = Swap16IfLE(client->si.format.greenMax);
  client->si.format.blueMax = Swap16IfLE(client->si.format.blueMax);
  client->si.nameLength = Swap32IfLE(client->si.nameLength);

  client->desktopName = malloc(client->si.nameLength + 1);
  if (!client->desktopName) {
    fprintf(stderr, "Error allocating memory for desktop name, %lu bytes\n",
            (unsigned long)client->si.nameLength);
    return FALSE;
  }

  if (!ReadFromRFBServer(client, client->desktopName, client->si.nameLength)) return FALSE;

  client->desktopName[client->si.nameLength] = 0;

  fprintf(stderr,"Desktop name \"%s\"\n",client->desktopName);

  fprintf(stderr,"Connected to VNC server, using protocol version %d.%d\n",
	  rfbProtocolMajorVersion, rfbProtocolMinorVersion);

  fprintf(stderr,"VNC server default format:\n");
  PrintPixelFormat(&client->si.format);

  return TRUE;
}


/*
 * SetFormatAndEncodings.
 */

Bool
SetFormatAndEncodings(rfbClient* client)
{
  rfbSetPixelFormatMsg spf;
  char buf[sz_rfbSetEncodingsMsg + MAX_ENCODINGS * 4];
  rfbSetEncodingsMsg *se = (rfbSetEncodingsMsg *)buf;
  uint32_t *encs = (uint32_t *)(&buf[sz_rfbSetEncodingsMsg]);
  int len = 0;
  Bool requestCompressLevel = FALSE;
  Bool requestQualityLevel = FALSE;
  Bool requestLastRectEncoding = FALSE;

  spf.type = rfbSetPixelFormat;
  spf.format = client->format;
  spf.format.redMax = Swap16IfLE(spf.format.redMax);
  spf.format.greenMax = Swap16IfLE(spf.format.greenMax);
  spf.format.blueMax = Swap16IfLE(spf.format.blueMax);

  if (!WriteExact(client, (char *)&spf, sz_rfbSetPixelFormatMsg))
    return FALSE;

  se->type = rfbSetEncodings;
  se->nEncodings = 0;

  if (client->appData.encodingsString) {
    const char *encStr = client->appData.encodingsString;
    int encStrLen;
    do {
      const char *nextEncStr = strchr(encStr, ' ');
      if (nextEncStr) {
	encStrLen = nextEncStr - encStr;
	nextEncStr++;
      } else {
	encStrLen = strlen(encStr);
      }

      if (strncasecmp(encStr,"raw",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingRaw);
      } else if (strncasecmp(encStr,"copyrect",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingCopyRect);
      } else if (strncasecmp(encStr,"tight",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingTight);
	requestLastRectEncoding = TRUE;
	if (client->appData.compressLevel >= 0 && client->appData.compressLevel <= 9)
	  requestCompressLevel = TRUE;
	if (client->appData.enableJPEG)
	  requestQualityLevel = TRUE;
      } else if (strncasecmp(encStr,"hextile",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingHextile);
      } else if (strncasecmp(encStr,"zlib",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingZlib);
	if (client->appData.compressLevel >= 0 && client->appData.compressLevel <= 9)
	  requestCompressLevel = TRUE;
      } else if (strncasecmp(encStr,"corre",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingCoRRE);
      } else if (strncasecmp(encStr,"rre",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingRRE);
      } else {
	fprintf(stderr,"Unknown encoding '%.*s'\n",encStrLen,encStr);
      }

      encStr = nextEncStr;
    } while (encStr && se->nEncodings < MAX_ENCODINGS);

    if (se->nEncodings < MAX_ENCODINGS && requestCompressLevel) {
      encs[se->nEncodings++] = Swap32IfLE(client->appData.compressLevel +
					  rfbEncodingCompressLevel0);
    }

    if (se->nEncodings < MAX_ENCODINGS && requestQualityLevel) {
      if (client->appData.qualityLevel < 0 || client->appData.qualityLevel > 9)
        client->appData.qualityLevel = 5;
      encs[se->nEncodings++] = Swap32IfLE(client->appData.qualityLevel +
					  rfbEncodingQualityLevel0);
    }

    if (client->appData.useRemoteCursor) {
      if (se->nEncodings < MAX_ENCODINGS)
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingXCursor);
      if (se->nEncodings < MAX_ENCODINGS)
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingRichCursor);
      if (se->nEncodings < MAX_ENCODINGS)
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingPointerPos);
    }

    if (se->nEncodings < MAX_ENCODINGS && requestLastRectEncoding) {
      encs[se->nEncodings++] = Swap32IfLE(rfbEncodingLastRect);
    }
  }
  else {
    if (SameMachine(client->sock)) {
      /* TODO:
      if (!tunnelSpecified) {
      */
	fprintf(stderr,"Same machine: preferring raw encoding\n");
	/* TODO: */
	//encs[se->nEncodings++] = Swap32IfLE(rfbEncodingRaw);
      /*
      } else {
	fprintf(stderr,"Tunneling active: preferring tight encoding\n");
      }
      */
    }

    encs[se->nEncodings++] = Swap32IfLE(rfbEncodingCopyRect);
    encs[se->nEncodings++] = Swap32IfLE(rfbEncodingTight);
    encs[se->nEncodings++] = Swap32IfLE(rfbEncodingHextile);
    encs[se->nEncodings++] = Swap32IfLE(rfbEncodingZlib);
    encs[se->nEncodings++] = Swap32IfLE(rfbEncodingCoRRE);
    encs[se->nEncodings++] = Swap32IfLE(rfbEncodingRRE);

    if (client->appData.compressLevel >= 0 && client->appData.compressLevel <= 9) {
      encs[se->nEncodings++] = Swap32IfLE(client->appData.compressLevel +
					  rfbEncodingCompressLevel0);
    } else /* if (!tunnelSpecified) */ {
      /* If -tunnel option was provided, we assume that server machine is
	 not in the local network so we use default compression level for
	 tight encoding instead of fast compression. Thus we are
	 requesting level 1 compression only if tunneling is not used. */
      encs[se->nEncodings++] = Swap32IfLE(rfbEncodingCompressLevel1);
    }

    if (client->appData.enableJPEG) {
      if (client->appData.qualityLevel < 0 || client->appData.qualityLevel > 9)
	client->appData.qualityLevel = 5;
      encs[se->nEncodings++] = Swap32IfLE(client->appData.qualityLevel +
					  rfbEncodingQualityLevel0);
    }

    if (client->appData.useRemoteCursor) {
      encs[se->nEncodings++] = Swap32IfLE(rfbEncodingXCursor);
      encs[se->nEncodings++] = Swap32IfLE(rfbEncodingRichCursor);
      encs[se->nEncodings++] = Swap32IfLE(rfbEncodingPointerPos);
    }

    encs[se->nEncodings++] = Swap32IfLE(rfbEncodingLastRect);
  }

  len = sz_rfbSetEncodingsMsg + se->nEncodings * 4;

  se->nEncodings = Swap16IfLE(se->nEncodings);

  if (!WriteExact(client, buf, len)) return FALSE;

  return TRUE;
}


/*
 * SendIncrementalFramebufferUpdateRequest.
 */

Bool
SendIncrementalFramebufferUpdateRequest(rfbClient* client)
{
  return SendFramebufferUpdateRequest(client, 0, 0, client->si.framebufferWidth,
				      client->si.framebufferHeight, TRUE);
}


/*
 * SendFramebufferUpdateRequest.
 */

Bool
SendFramebufferUpdateRequest(rfbClient* client, int x, int y, int w, int h, Bool incremental)
{
  rfbFramebufferUpdateRequestMsg fur;

  fur.type = rfbFramebufferUpdateRequest;
  fur.incremental = incremental ? 1 : 0;
  fur.x = Swap16IfLE(x);
  fur.y = Swap16IfLE(y);
  fur.w = Swap16IfLE(w);
  fur.h = Swap16IfLE(h);

  if (!WriteExact(client, (char *)&fur, sz_rfbFramebufferUpdateRequestMsg))
    return FALSE;

  return TRUE;
}


/*
 * SendPointerEvent.
 */

Bool
SendPointerEvent(rfbClient* client,int x, int y, int buttonMask)
{
  rfbPointerEventMsg pe;

  pe.type = rfbPointerEvent;
  pe.buttonMask = buttonMask;
  if (x < 0) x = 0;
  if (y < 0) y = 0;

  pe.x = Swap16IfLE(x);
  pe.y = Swap16IfLE(y);
  return WriteExact(client, (char *)&pe, sz_rfbPointerEventMsg);
}


/*
 * SendKeyEvent.
 */

Bool
SendKeyEvent(rfbClient* client, uint32_t key, Bool down)
{
  rfbKeyEventMsg ke;

  ke.type = rfbKeyEvent;
  ke.down = down ? 1 : 0;
  ke.key = Swap32IfLE(key);
  return WriteExact(client, (char *)&ke, sz_rfbKeyEventMsg);
}


/*
 * SendClientCutText.
 */

Bool
SendClientCutText(rfbClient* client, char *str, int len)
{
  rfbClientCutTextMsg cct;

  if (client->serverCutText)
    free(client->serverCutText);
  client->serverCutText = NULL;

  cct.type = rfbClientCutText;
  cct.length = Swap32IfLE(len);
  return  (WriteExact(client, (char *)&cct, sz_rfbClientCutTextMsg) &&
	   WriteExact(client, str, len));
}



/*
 * HandleRFBServerMessage.
 */

Bool
HandleRFBServerMessage(rfbClient* client)
{
  rfbServerToClientMsg msg;

  if (!ReadFromRFBServer(client, (char *)&msg, 1))
    return FALSE;

  switch (msg.type) {

  case rfbSetColourMapEntries:
  {
    /* TODO:
    int i;
    uint16_t rgb[3];
    XColor xc;

    if (!ReadFromRFBServer(client, ((char *)&msg) + 1,
			   sz_rfbSetColourMapEntriesMsg - 1))
      return FALSE;

    msg.scme.firstColour = Swap16IfLE(msg.scme.firstColour);
    msg.scme.nColours = Swap16IfLE(msg.scme.nColours);

    for (i = 0; i < msg.scme.nColours; i++) {
      if (!ReadFromRFBServer(client, (char *)rgb, 6))
	return FALSE;
      xc.pixel = msg.scme.firstColour + i;
      xc.red = Swap16IfLE(rgb[0]);
      xc.green = Swap16IfLE(rgb[1]);
      xc.blue = Swap16IfLE(rgb[2]);
      xc.flags = DoRed|DoGreen|DoBlue;
      XStoreColor(dpy, cmap, &xc);
    }
    */

    break;
  }

  case rfbFramebufferUpdate:
  {
    rfbFramebufferUpdateRectHeader rect;
    int linesToRead;
    int bytesPerLine;
    int i;

    if (!ReadFromRFBServer(client, ((char *)&msg.fu) + 1,
			   sz_rfbFramebufferUpdateMsg - 1))
      return FALSE;

    msg.fu.nRects = Swap16IfLE(msg.fu.nRects);

    for (i = 0; i < msg.fu.nRects; i++) {
      if (!ReadFromRFBServer(client, (char *)&rect, sz_rfbFramebufferUpdateRectHeader))
	return FALSE;

      rect.encoding = Swap32IfLE(rect.encoding);
      if (rect.encoding == rfbEncodingLastRect)
	break;

      rect.r.x = Swap16IfLE(rect.r.x);
      rect.r.y = Swap16IfLE(rect.r.y);
      rect.r.w = Swap16IfLE(rect.r.w);
      rect.r.h = Swap16IfLE(rect.r.h);

      if (rect.encoding == rfbEncodingXCursor ||
	  rect.encoding == rfbEncodingRichCursor) {
	if (!HandleCursorShape(client,
			       rect.r.x, rect.r.y, rect.r.w, rect.r.h,
			       rect.encoding)) {
	  return FALSE;
	}
	continue;
      }

      if (rect.encoding == rfbEncodingPointerPos) {
	if (!client->HandleCursorPos(client,rect.r.x, rect.r.y)) {
	  return FALSE;
	}
	continue;
      }

      if ((rect.r.x + rect.r.w > client->si.framebufferWidth) ||
	  (rect.r.y + rect.r.h > client->si.framebufferHeight))
	{
	  fprintf(stderr,"Rect too large: %dx%d at (%d, %d)\n",
		  rect.r.w, rect.r.h, rect.r.x, rect.r.y);
	  return FALSE;
	}

      if (rect.r.h * rect.r.w == 0) {
	fprintf(stderr,"Zero size rect - ignoring\n");
	continue;
      }

      /* If RichCursor encoding is used, we should prevent collisions
	 between framebuffer updates and cursor drawing operations. */
      client->SoftCursorLockArea(client, rect.r.x, rect.r.y, rect.r.w, rect.r.h);

      switch (rect.encoding) {

      case rfbEncodingRaw:

	bytesPerLine = rect.r.w * client->format.bitsPerPixel / 8;
	linesToRead = BUFFER_SIZE / bytesPerLine;

	while (rect.r.h > 0) {
	  if (linesToRead > rect.r.h)
	    linesToRead = rect.r.h;

	  if (!ReadFromRFBServer(client, client->buffer,bytesPerLine * linesToRead))
	    return FALSE;

	  CopyRectangle(client, client->buffer,
			   rect.r.x, rect.r.y, rect.r.w,linesToRead);

	  rect.r.h -= linesToRead;
	  rect.r.y += linesToRead;

	}
	break;

      case rfbEncodingCopyRect:
      {
	rfbCopyRect cr;

	if (!ReadFromRFBServer(client, (char *)&cr, sz_rfbCopyRect))
	  return FALSE;

	cr.srcX = Swap16IfLE(cr.srcX);
	cr.srcY = Swap16IfLE(cr.srcY);

	/* If RichCursor encoding is used, we should extend our
	   "cursor lock area" (previously set to destination
	   rectangle) to the source rectangle as well. */
	client->SoftCursorLockArea(client,
				   cr.srcX, cr.srcY, rect.r.w, rect.r.h);

	CopyRectangleFromRectangle(client,
				   cr.srcX, cr.srcY, rect.r.w, rect.r.h,
				   rect.r.x, rect.r.y);

	break;
      }

      case rfbEncodingRRE:
      {
	switch (client->format.bitsPerPixel) {
	case 8:
	  if (!HandleRRE8(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 16:
	  if (!HandleRRE16(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 32:
	  if (!HandleRRE32(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	}
	break;
      }

      case rfbEncodingCoRRE:
      {
	switch (client->format.bitsPerPixel) {
	case 8:
	  if (!HandleCoRRE8(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 16:
	  if (!HandleCoRRE16(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 32:
	  if (!HandleCoRRE32(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	}
	break;
      }

      case rfbEncodingHextile:
      {
	switch (client->format.bitsPerPixel) {
	case 8:
	  if (!HandleHextile8(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 16:
	  if (!HandleHextile16(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 32:
	  if (!HandleHextile32(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	}
	break;
      }

      case rfbEncodingZlib:
      {
	switch (client->format.bitsPerPixel) {
	case 8:
	  if (!HandleZlib8(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 16:
	  if (!HandleZlib16(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 32:
	  if (!HandleZlib32(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	}
	break;
     }

      case rfbEncodingTight:
      {
	switch (client->format.bitsPerPixel) {
	case 8:
	  if (!HandleTight8(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 16:
	  if (!HandleTight16(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 32:
	  if (!HandleTight32(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	}
	break;
      }

      default:
	fprintf(stderr,"Unknown rect encoding %d\n",
		(int)rect.encoding);
	return FALSE;
      }

      /* Now we may discard "soft cursor locks". */
      client->SoftCursorUnlockScreen(client);

      client->FramebufferUpdateReceived(client, rect.r.x, rect.r.y, rect.r.w, rect.r.h);
    }

#ifdef MITSHM
    /* if using shared memory PutImage, make sure that the X server has
       updated its framebuffer before we reuse the shared memory.  This is
       mainly to avoid copyrect using invalid screen contents - not sure
       if we'd need it otherwise. */

    if (client->appData.useShm)
      XSync(dpy, FALSE);
#endif

    if (!SendIncrementalFramebufferUpdateRequest(client))
      return FALSE;

    break;
  }

  case rfbBell:
  {
    client->Bell(client);

    break;
  }

  case rfbServerCutText:
  {
    if (!ReadFromRFBServer(client, ((char *)&msg) + 1,
			   sz_rfbServerCutTextMsg - 1))
      return FALSE;

    msg.sct.length = Swap32IfLE(msg.sct.length);

    if (client->serverCutText)
      free(client->serverCutText);

    client->serverCutText = malloc(msg.sct.length+1);

    if (!ReadFromRFBServer(client, client->serverCutText, msg.sct.length))
      return FALSE;

    client->serverCutText[msg.sct.length] = 0;

    client->newServerCutText = TRUE;

    break;
  }

  default:
    fprintf(stderr,"Unknown message type %d from VNC server\n",msg.type);
    return FALSE;
  }

  return TRUE;
}


#define GET_PIXEL8(pix, ptr) ((pix) = *(ptr)++)

#define GET_PIXEL16(pix, ptr) (((uint8_t*)&(pix))[0] = *(ptr)++, \
			       ((uint8_t*)&(pix))[1] = *(ptr)++)

#define GET_PIXEL32(pix, ptr) (((uint8_t*)&(pix))[0] = *(ptr)++, \
			       ((uint8_t*)&(pix))[1] = *(ptr)++, \
			       ((uint8_t*)&(pix))[2] = *(ptr)++, \
			       ((uint8_t*)&(pix))[3] = *(ptr)++)

/* CONCAT2 concatenates its two arguments.  CONCAT2E does the same but also
   expands its arguments if they are macros */

#define CONCAT2(a,b) a##b
#define CONCAT2E(a,b) CONCAT2(a,b)
#define CONCAT3(a,b,c) a##b##c
#define CONCAT3E(a,b,c) CONCAT3(a,b,c)

#define BPP 8
#include "rre.c"
#include "corre.c"
#include "hextile.c"
#include "zlib.c"
#include "tight.c"
#undef BPP
#define BPP 16
#include "rre.c"
#include "corre.c"
#include "hextile.c"
#include "zlib.c"
#include "tight.c"
#undef BPP
#define BPP 32
#include "rre.c"
#include "corre.c"
#include "hextile.c"
#include "zlib.c"
#include "tight.c"
#undef BPP


/*
 * PrintPixelFormat.
 */

void
PrintPixelFormat(format)
    rfbPixelFormat *format;
{
  if (format->bitsPerPixel == 1) {
    fprintf(stderr,"  Single bit per pixel.\n");
    fprintf(stderr,
	    "  %s significant bit in each byte is leftmost on the screen.\n",
	    (format->bigEndian ? "Most" : "Least"));
  } else {
    fprintf(stderr,"  %d bits per pixel.\n",format->bitsPerPixel);
    if (format->bitsPerPixel != 8) {
      fprintf(stderr,"  %s significant byte first in each pixel.\n",
	      (format->bigEndian ? "Most" : "Least"));
    }
    if (format->trueColour) {
      fprintf(stderr,"  TRUE colour: max red %d green %d blue %d",
	      format->redMax, format->greenMax, format->blueMax);
      fprintf(stderr,", shift red %d green %d blue %d\n",
	      format->redShift, format->greenShift, format->blueShift);
    } else {
      fprintf(stderr,"  Colour map (not true colour).\n");
    }
  }
}

static long
ReadCompactLen (rfbClient* client)
{
  long len;
  uint8_t b;

  if (!ReadFromRFBServer(client, (char *)&b, 1))
    return -1;
  len = (int)b & 0x7F;
  if (b & 0x80) {
    if (!ReadFromRFBServer(client, (char *)&b, 1))
      return -1;
    len |= ((int)b & 0x7F) << 7;
    if (b & 0x80) {
      if (!ReadFromRFBServer(client, (char *)&b, 1))
	return -1;
      len |= ((int)b & 0xFF) << 14;
    }
  }
  return len;
}

/*
 * JPEG source manager functions for JPEG decompression in Tight decoder.
 */

static struct jpeg_source_mgr jpegSrcManager;
static JOCTET *jpegBufferPtr;
static size_t jpegBufferLen;

static void
JpegInitSource(j_decompress_ptr cinfo)
{
  jpegError = FALSE;
}

static boolean
JpegFillInputBuffer(j_decompress_ptr cinfo)
{
  jpegError = TRUE;
  jpegSrcManager.bytes_in_buffer = jpegBufferLen;
  jpegSrcManager.next_input_byte = (JOCTET *)jpegBufferPtr;

  return TRUE;
}

static void
JpegSkipInputData(j_decompress_ptr cinfo, long num_bytes)
{
  if (num_bytes < 0 || num_bytes > jpegSrcManager.bytes_in_buffer) {
    jpegError = TRUE;
    jpegSrcManager.bytes_in_buffer = jpegBufferLen;
    jpegSrcManager.next_input_byte = (JOCTET *)jpegBufferPtr;
  } else {
    jpegSrcManager.next_input_byte += (size_t) num_bytes;
    jpegSrcManager.bytes_in_buffer -= (size_t) num_bytes;
  }
}

static void
JpegTermSource(j_decompress_ptr cinfo)
{
  /* No work necessary here. */
}

static void
JpegSetSrcManager(j_decompress_ptr cinfo, uint8_t *compressedData,
		  int compressedLen)
{
  jpegBufferPtr = (JOCTET *)compressedData;
  jpegBufferLen = (size_t)compressedLen;

  jpegSrcManager.init_source = JpegInitSource;
  jpegSrcManager.fill_input_buffer = JpegFillInputBuffer;
  jpegSrcManager.skip_input_data = JpegSkipInputData;
  jpegSrcManager.resync_to_restart = jpeg_resync_to_restart;
  jpegSrcManager.term_source = JpegTermSource;
  jpegSrcManager.next_input_byte = jpegBufferPtr;
  jpegSrcManager.bytes_in_buffer = jpegBufferLen;

  cinfo->src = &jpegSrcManager;
}

/* avoid name clashes with LibVNCServer */

#define vncEncryptBytes rfbEncryptBytes
#define vncEncryptAndStorePasswd rfbEncryptAndStorePasswdUnused
#define vncDecryptPasswdFromFile rfbDecryptPasswdFromFileUnused
#define vncRandomBytes rfbRandomBytesUnused

#include "../vncauth.c"
#include "../d3des.c"
