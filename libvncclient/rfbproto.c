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
#ifdef LIBVNCSERVER_HAVE_LIBZ
#include <zlib.h>
#endif
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
#include <jpeglib.h>
#endif
#include <stdarg.h>
#include <time.h>

/*
 * rfbClientLog prints a time-stamped message to the log file (stderr).
 */

rfbBool rfbEnableClientLogging=TRUE;

void
rfbDefaultClientLog(const char *format, ...)
{
    va_list args;
    char buf[256];
    time_t log_clock;

    if(!rfbEnableClientLogging)
      return;

    va_start(args, format);

    time(&log_clock);
    strftime(buf, 255, "%d/%m/%Y %X ", localtime(&log_clock));
    fprintf(stderr,buf);

    vfprintf(stderr, format, args);
    fflush(stderr);

    va_end(args);
}

rfbClientLogProc rfbClientLog=rfbDefaultClientLog;
rfbClientLogProc rfbClientErr=rfbDefaultClientLog;

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
    rfbClientLog("Unsupported bitsPerPixel: %d\n",client->format.bitsPerPixel);
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
    rfbClientLog("Unsupported bitsPerPixel: %d\n",client->format.bitsPerPixel);
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
    rfbClientLog("Unsupported bitsPerPixel: %d\n",client->format.bitsPerPixel);
  }
}

static rfbBool HandleRRE8(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleRRE16(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleRRE32(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleCoRRE8(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleCoRRE16(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleCoRRE32(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleHextile8(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleHextile16(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleHextile32(rfbClient* client, int rx, int ry, int rw, int rh);
#ifdef LIBVNCSERVER_HAVE_LIBZ
static rfbBool HandleZlib8(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleZlib16(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleZlib32(rfbClient* client, int rx, int ry, int rw, int rh);
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
static rfbBool HandleTight8(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleTight16(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleTight32(rfbClient* client, int rx, int ry, int rw, int rh);

static long ReadCompactLen (rfbClient* client);

static void JpegInitSource(j_decompress_ptr cinfo);
static boolean JpegFillInputBuffer(j_decompress_ptr cinfo);
static void JpegSkipInputData(j_decompress_ptr cinfo, long num_bytes);
static void JpegTermSource(j_decompress_ptr cinfo);
static void JpegSetSrcManager(j_decompress_ptr cinfo, uint8_t *compressedData,
                              int compressedLen);
#endif
#endif

/* The zlib encoding requires expansion/decompression/deflation of the
   compressed data in the "buffer" above into another, result buffer.
   However, the size of the result buffer can be determined precisely
   based on the bitsPerPixel, height and width of the rectangle.  We
   allocate this buffer one time to be the full size of the buffer. */

#ifdef LIBVNCSERVER_HAVE_LIBZ
static int raw_buffer_size = -1;
static char *raw_buffer;

static z_stream decompStream;
static rfbBool decompStreamInited = FALSE;

#endif


#ifdef LIBVNCSERVER_HAVE_LIBJPEG
/*
 * Variables for the ``tight'' encoding implementation.
 */

/* Separate buffer for compressed data. */
/* TODO: threading issues */
#define ZLIB_BUFFER_SIZE 30000
static char zlib_buffer[ZLIB_BUFFER_SIZE];

/* Four independent compression streams for zlib library. */
static z_stream zlibStream[4];
static rfbBool zlibStreamActive[4] = {
  FALSE, FALSE, FALSE, FALSE
};

/* Filter stuff. Should be initialized by filter initialization code. */
static rfbBool cutZeros;
static int rectWidth, rectColors;
static char tightPalette[256*4];
static uint8_t tightPrevRow[2048*3*sizeof(uint16_t)];

/* JPEG decoder state. */
static rfbBool jpegError;
#endif

/*
 * ConnectToRFBServer.
 */

rfbBool
ConnectToRFBServer(rfbClient* client,const char *hostname, int port)
{
  unsigned int host;

  if (client->serverPort==-1) {
    /* serverHost is a file recorded by vncrec. */
    const char* magic="vncLog0.0";
    char buffer[10];
    rfbVNCRec* rec = (rfbVNCRec*)malloc(sizeof(rfbVNCRec));
    client->vncRec = rec;

    rec->file = fopen(client->serverHost,"rb");
    rec->tv.tv_sec = 0;
    rec->readTimestamp = FALSE;
    rec->doNotSleep = FALSE;
    
    if (!rec->file) {
      rfbClientLog("Could not open %s.\n",client->serverHost);
      return FALSE;
    }
    setbuf(rec->file,0);
    fread(buffer,1,strlen(magic),rec->file);
    if (strncmp(buffer,magic,strlen(magic))) {
      rfbClientLog("File %s was not recorded by vncrec.\n",client->serverHost);
      fclose(rec->file);
      return FALSE;
    }
    client->sock = 0;
    return TRUE;
  }

  if (!StringToIPAddr(hostname, &host)) {
    rfbClientLog("Couldn't convert '%s' to host address\n", hostname);
    return FALSE;
  }

  client->sock = ConnectClientToTcpAddr(host, port);

  if (client->sock < 0) {
    rfbClientLog("Unable to connect to VNC server\n");
    return FALSE;
  }

  return SetNonBlocking(client->sock);
}

extern void rfbClientEncryptBytes(unsigned char* bytes, char* passwd);

/*
 * InitialiseRFBConnection.
 */

rfbBool
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
  pv[sz_rfbProtocolVersionMsg]=0;

  errorMessageOnReadFailure = TRUE;

  pv[sz_rfbProtocolVersionMsg] = 0;

  if (sscanf(pv,rfbProtocolVersionFormat,&major,&minor) != 2) {
    rfbClientLog("Not a valid VNC server (%s)\n",pv);
    return FALSE;
  }

  rfbClientLog("VNC server supports protocol version %d.%d (viewer %d.%d)\n",
	  major, minor, rfbProtocolMajorVersion, rfbProtocolMinorVersion);

  major = rfbProtocolMajorVersion;
  minor = rfbProtocolMinorVersion;

  sprintf(pv,rfbProtocolVersionFormat,major,minor);

  if (!WriteToRFBServer(client, pv, sz_rfbProtocolVersionMsg)) return FALSE;

  if (!ReadFromRFBServer(client, (char *)&authScheme, 4)) return FALSE;

  authScheme = rfbClientSwap32IfLE(authScheme);

  switch (authScheme) {

  case rfbConnFailed:
    if (!ReadFromRFBServer(client, (char *)&reasonLen, 4)) return FALSE;
    reasonLen = rfbClientSwap32IfLE(reasonLen);

    reason = malloc(reasonLen);

    if (!ReadFromRFBServer(client, reason, reasonLen)) return FALSE;

    rfbClientLog("VNC connection failed: %.*s\n",(int)reasonLen, reason);
    return FALSE;

  case rfbNoAuth:
    rfbClientLog("No authentication needed\n");
    break;

  case rfbVncAuth:
    if (!ReadFromRFBServer(client, (char *)challenge, CHALLENGESIZE)) return FALSE;

    if (client->serverPort!=-1) { /* if not playing a vncrec file */
      if (client->GetPassword)
        passwd = client->GetPassword(client);

      if ((!passwd) || (strlen(passwd) == 0)) {
        rfbClientLog("Reading password failed\n");
        return FALSE;
      }
      if (strlen(passwd) > 8) {
        passwd[8] = '\0';
      }

      rfbClientEncryptBytes(challenge, passwd);

      /* Lose the password from memory */
      for (i = strlen(passwd); i >= 0; i--) {
        passwd[i] = '\0';
      }
      free(passwd);

      if (!WriteToRFBServer(client, (char *)challenge, CHALLENGESIZE)) return FALSE;
    }

    if (!ReadFromRFBServer(client, (char *)&authResult, 4)) return FALSE;

    authResult = rfbClientSwap32IfLE(authResult);

    switch (authResult) {
    case rfbVncAuthOK:
      rfbClientLog("VNC authentication succeeded\n");
      break;
    case rfbVncAuthFailed:
      rfbClientLog("VNC authentication failed\n");
      return FALSE;
    case rfbVncAuthTooMany:
      rfbClientLog("VNC authentication failed - too many tries\n");
      return FALSE;
    default:
      rfbClientLog("Unknown VNC authentication result: %d\n",
	      (int)authResult);
      return FALSE;
    }
    break;

  default:
    rfbClientLog("Unknown authentication scheme from VNC server: %d\n",
	    (int)authScheme);
    return FALSE;
  }

  ci.shared = (client->appData.shareDesktop ? 1 : 0);

  if (!WriteToRFBServer(client,  (char *)&ci, sz_rfbClientInitMsg)) return FALSE;

  if (!ReadFromRFBServer(client, (char *)&client->si, sz_rfbServerInitMsg)) return FALSE;

  client->si.framebufferWidth = rfbClientSwap16IfLE(client->si.framebufferWidth);
  client->si.framebufferHeight = rfbClientSwap16IfLE(client->si.framebufferHeight);
  client->si.format.redMax = rfbClientSwap16IfLE(client->si.format.redMax);
  client->si.format.greenMax = rfbClientSwap16IfLE(client->si.format.greenMax);
  client->si.format.blueMax = rfbClientSwap16IfLE(client->si.format.blueMax);
  client->si.nameLength = rfbClientSwap32IfLE(client->si.nameLength);

  client->desktopName = malloc(client->si.nameLength + 1);
  if (!client->desktopName) {
    rfbClientLog("Error allocating memory for desktop name, %lu bytes\n",
            (unsigned long)client->si.nameLength);
    return FALSE;
  }

  if (!ReadFromRFBServer(client, client->desktopName, client->si.nameLength)) return FALSE;

  client->desktopName[client->si.nameLength] = 0;

  rfbClientLog("Desktop name \"%s\"\n",client->desktopName);

  rfbClientLog("Connected to VNC server, using protocol version %d.%d\n",
	  rfbProtocolMajorVersion, rfbProtocolMinorVersion);

  rfbClientLog("VNC server default format:\n");
  PrintPixelFormat(&client->si.format);

  return TRUE;
}


/*
 * SetFormatAndEncodings.
 */

rfbBool
SetFormatAndEncodings(rfbClient* client)
{
  rfbSetPixelFormatMsg spf;
  char buf[sz_rfbSetEncodingsMsg + MAX_ENCODINGS * 4];
  rfbSetEncodingsMsg *se = (rfbSetEncodingsMsg *)buf;
  uint32_t *encs = (uint32_t *)(&buf[sz_rfbSetEncodingsMsg]);
  int len = 0;
  rfbBool requestCompressLevel = FALSE;
  rfbBool requestQualityLevel = FALSE;
  rfbBool requestLastRectEncoding = FALSE;

  spf.type = rfbSetPixelFormat;
  spf.format = client->format;
  spf.format.redMax = rfbClientSwap16IfLE(spf.format.redMax);
  spf.format.greenMax = rfbClientSwap16IfLE(spf.format.greenMax);
  spf.format.blueMax = rfbClientSwap16IfLE(spf.format.blueMax);

  if (!WriteToRFBServer(client, (char *)&spf, sz_rfbSetPixelFormatMsg))
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
	encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingRaw);
      } else if (strncasecmp(encStr,"copyrect",encStrLen) == 0) {
	encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingCopyRect);
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
      } else if (strncasecmp(encStr,"tight",encStrLen) == 0) {
	encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingTight);
	requestLastRectEncoding = TRUE;
	if (client->appData.compressLevel >= 0 && client->appData.compressLevel <= 9)
	  requestCompressLevel = TRUE;
	if (client->appData.enableJPEG)
	  requestQualityLevel = TRUE;
#endif
      } else if (strncasecmp(encStr,"hextile",encStrLen) == 0) {
	encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingHextile);
#ifdef LIBVNCSERVER_HAVE_LIBZ
      } else if (strncasecmp(encStr,"zlib",encStrLen) == 0) {
	encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingZlib);
	if (client->appData.compressLevel >= 0 && client->appData.compressLevel <= 9)
	  requestCompressLevel = TRUE;
      } else if (strncasecmp(encStr,"zlibhex",encStrLen) == 0) {
	encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingZlibHex);
	if (client->appData.compressLevel >= 0 && client->appData.compressLevel <= 9)
	  requestCompressLevel = TRUE;
      } else if (strncasecmp(encStr,"zrle",encStrLen) == 0) {
	encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingZRLE);
#endif
      } else if (strncasecmp(encStr,"corre",encStrLen) == 0) {
	encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingCoRRE);
      } else if (strncasecmp(encStr,"rre",encStrLen) == 0) {
	encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingRRE);
      } else {
	rfbClientLog("Unknown encoding '%.*s'\n",encStrLen,encStr);
      }

      encStr = nextEncStr;
    } while (encStr && se->nEncodings < MAX_ENCODINGS);

    if (se->nEncodings < MAX_ENCODINGS && requestCompressLevel) {
      encs[se->nEncodings++] = rfbClientSwap32IfLE(client->appData.compressLevel +
					  rfbEncodingCompressLevel0);
    }

    if (se->nEncodings < MAX_ENCODINGS && requestQualityLevel) {
      if (client->appData.qualityLevel < 0 || client->appData.qualityLevel > 9)
        client->appData.qualityLevel = 5;
      encs[se->nEncodings++] = rfbClientSwap32IfLE(client->appData.qualityLevel +
					  rfbEncodingQualityLevel0);
    }

    if (client->appData.useRemoteCursor) {
      if (se->nEncodings < MAX_ENCODINGS)
	encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingXCursor);
      if (se->nEncodings < MAX_ENCODINGS)
	encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingRichCursor);
      if (se->nEncodings < MAX_ENCODINGS)
	encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingPointerPos);
    }

    if (se->nEncodings < MAX_ENCODINGS && requestLastRectEncoding) {
      encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingLastRect);
    }
  }
  else {
    if (SameMachine(client->sock)) {
      /* TODO:
      if (!tunnelSpecified) {
      */
      rfbClientLog("Same machine: preferring raw encoding\n");
      encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingRaw);
      /*
      } else {
	rfbClientLog("Tunneling active: preferring tight encoding\n");
      }
      */
    }

    encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingCopyRect);
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
    encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingTight);
#endif
    encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingHextile);
#ifdef LIBVNCSERVER_HAVE_LIBZ
    encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingZlib);
#endif
    encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingCoRRE);
    encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingRRE);

    if (client->appData.compressLevel >= 0 && client->appData.compressLevel <= 9) {
      encs[se->nEncodings++] = rfbClientSwap32IfLE(client->appData.compressLevel +
					  rfbEncodingCompressLevel0);
    } else /* if (!tunnelSpecified) */ {
      /* If -tunnel option was provided, we assume that server machine is
	 not in the local network so we use default compression level for
	 tight encoding instead of fast compression. Thus we are
	 requesting level 1 compression only if tunneling is not used. */
      encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingCompressLevel1);
    }

    if (client->appData.enableJPEG) {
      if (client->appData.qualityLevel < 0 || client->appData.qualityLevel > 9)
	client->appData.qualityLevel = 5;
      encs[se->nEncodings++] = rfbClientSwap32IfLE(client->appData.qualityLevel +
					  rfbEncodingQualityLevel0);
    }

    if (client->appData.useRemoteCursor) {
      encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingXCursor);
      encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingRichCursor);
      encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingPointerPos);
    }

    encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingLastRect);
  }

  len = sz_rfbSetEncodingsMsg + se->nEncodings * 4;

  se->nEncodings = rfbClientSwap16IfLE(se->nEncodings);

  if (!WriteToRFBServer(client, buf, len)) return FALSE;

  return TRUE;
}


/*
 * SendIncrementalFramebufferUpdateRequest.
 */

rfbBool
SendIncrementalFramebufferUpdateRequest(rfbClient* client)
{
  return SendFramebufferUpdateRequest(client, 0, 0, client->si.framebufferWidth,
				      client->si.framebufferHeight, TRUE);
}


/*
 * SendFramebufferUpdateRequest.
 */

rfbBool
SendFramebufferUpdateRequest(rfbClient* client, int x, int y, int w, int h, rfbBool incremental)
{
  rfbFramebufferUpdateRequestMsg fur;

  fur.type = rfbFramebufferUpdateRequest;
  fur.incremental = incremental ? 1 : 0;
  fur.x = rfbClientSwap16IfLE(x);
  fur.y = rfbClientSwap16IfLE(y);
  fur.w = rfbClientSwap16IfLE(w);
  fur.h = rfbClientSwap16IfLE(h);

  if (!WriteToRFBServer(client, (char *)&fur, sz_rfbFramebufferUpdateRequestMsg))
    return FALSE;

  return TRUE;
}


/*
 * SendPointerEvent.
 */

rfbBool
SendPointerEvent(rfbClient* client,int x, int y, int buttonMask)
{
  rfbPointerEventMsg pe;

  pe.type = rfbPointerEvent;
  pe.buttonMask = buttonMask;
  if (x < 0) x = 0;
  if (y < 0) y = 0;

  pe.x = rfbClientSwap16IfLE(x);
  pe.y = rfbClientSwap16IfLE(y);
  return WriteToRFBServer(client, (char *)&pe, sz_rfbPointerEventMsg);
}


/*
 * SendKeyEvent.
 */

rfbBool
SendKeyEvent(rfbClient* client, uint32_t key, rfbBool down)
{
  rfbKeyEventMsg ke;

  ke.type = rfbKeyEvent;
  ke.down = down ? 1 : 0;
  ke.key = rfbClientSwap32IfLE(key);
  return WriteToRFBServer(client, (char *)&ke, sz_rfbKeyEventMsg);
}


/*
 * SendClientCutText.
 */

rfbBool
SendClientCutText(rfbClient* client, char *str, int len)
{
  rfbClientCutTextMsg cct;

  if (client->serverCutText)
    free(client->serverCutText);
  client->serverCutText = NULL;

  cct.type = rfbClientCutText;
  cct.length = rfbClientSwap32IfLE(len);
  return  (WriteToRFBServer(client, (char *)&cct, sz_rfbClientCutTextMsg) &&
	   WriteToRFBServer(client, str, len));
}



/*
 * HandleRFBServerMessage.
 */

rfbBool
HandleRFBServerMessage(rfbClient* client)
{
  rfbServerToClientMsg msg;

  if (client->serverPort==-1)
    client->vncRec->readTimestamp = TRUE;
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

    msg.scme.firstColour = rfbClientSwap16IfLE(msg.scme.firstColour);
    msg.scme.nColours = rfbClientSwap16IfLE(msg.scme.nColours);

    for (i = 0; i < msg.scme.nColours; i++) {
      if (!ReadFromRFBServer(client, (char *)rgb, 6))
	return FALSE;
      xc.pixel = msg.scme.firstColour + i;
      xc.red = rfbClientSwap16IfLE(rgb[0]);
      xc.green = rfbClientSwap16IfLE(rgb[1]);
      xc.blue = rfbClientSwap16IfLE(rgb[2]);
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

    msg.fu.nRects = rfbClientSwap16IfLE(msg.fu.nRects);

    for (i = 0; i < msg.fu.nRects; i++) {
      if (!ReadFromRFBServer(client, (char *)&rect, sz_rfbFramebufferUpdateRectHeader))
	return FALSE;

      rect.encoding = rfbClientSwap32IfLE(rect.encoding);
      if (rect.encoding == rfbEncodingLastRect)
	break;

      rect.r.x = rfbClientSwap16IfLE(rect.r.x);
      rect.r.y = rfbClientSwap16IfLE(rect.r.y);
      rect.r.w = rfbClientSwap16IfLE(rect.r.w);
      rect.r.h = rfbClientSwap16IfLE(rect.r.h);

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
	  rfbClientLog("Rect too large: %dx%d at (%d, %d)\n",
		  rect.r.w, rect.r.h, rect.r.x, rect.r.y);
	  return FALSE;
	}

      if (rect.r.h * rect.r.w == 0) {
	rfbClientLog("Zero size rect - ignoring\n");
	continue;
      }

      /* If RichCursor encoding is used, we should prevent collisions
	 between framebuffer updates and cursor drawing operations. */
      client->SoftCursorLockArea(client, rect.r.x, rect.r.y, rect.r.w, rect.r.h);

      switch (rect.encoding) {

      case rfbEncodingRaw: {
	int y=rect.r.y, h=rect.r.h;

	bytesPerLine = rect.r.w * client->format.bitsPerPixel / 8;
	linesToRead = RFB_BUFFER_SIZE / bytesPerLine;

	while (h > 0) {
	  if (linesToRead > h)
	    linesToRead = h;

	  if (!ReadFromRFBServer(client, client->buffer,bytesPerLine * linesToRead))
	    return FALSE;

	  CopyRectangle(client, client->buffer,
			   rect.r.x, y, rect.r.w,linesToRead);

	  h -= linesToRead;
	  y += linesToRead;

	}
      } break;

      case rfbEncodingCopyRect:
      {
	rfbCopyRect cr;

	if (!ReadFromRFBServer(client, (char *)&cr, sz_rfbCopyRect))
	  return FALSE;

	cr.srcX = rfbClientSwap16IfLE(cr.srcX);
	cr.srcY = rfbClientSwap16IfLE(cr.srcY);

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

#ifdef LIBVNCSERVER_HAVE_LIBZ
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

#ifdef LIBVNCSERVER_HAVE_LIBJPEG
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
#endif
#endif

      default:
	rfbClientLog("Unknown rect encoding %d\n",
		(int)rect.encoding);
	return FALSE;
      }

      /* Now we may discard "soft cursor locks". */
      client->SoftCursorUnlockScreen(client);

      client->GotFrameBufferUpdate(client, rect.r.x, rect.r.y, rect.r.w, rect.r.h);
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

    msg.sct.length = rfbClientSwap32IfLE(msg.sct.length);

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
    rfbClientLog("Unknown message type %d from VNC server\n",msg.type);
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
    rfbClientLog("  Single bit per pixel.\n");
    rfbClientLog(
	    "  %s significant bit in each byte is leftmost on the screen.\n",
	    (format->bigEndian ? "Most" : "Least"));
  } else {
    rfbClientLog("  %d bits per pixel.\n",format->bitsPerPixel);
    if (format->bitsPerPixel != 8) {
      rfbClientLog("  %s significant byte first in each pixel.\n",
	      (format->bigEndian ? "Most" : "Least"));
    }
    if (format->trueColour) {
      rfbClientLog("  TRUE colour: max red %d green %d blue %d"
		   ", shift red %d green %d blue %d\n",
		   format->redMax, format->greenMax, format->blueMax,
		   format->redShift, format->greenShift, format->blueShift);
    } else {
      rfbClientLog("  Colour map (not true colour).\n");
    }
  }
}

#ifdef LIBVNCSERVER_HAVE_LIBJPEG

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

#endif

/* avoid name clashes with LibVNCServer */

#define rfbEncryptBytes rfbClientEncryptBytes
#define rfbEncryptAndStorePasswd rfbClientEncryptAndStorePasswdUnused
#define rfbDecryptPasswdFromFile rfbClientDecryptPasswdFromFileUnused
#define rfbRandomBytes rfbClientRandomBytesUnused
#define rfbDes rfbClientDes
#define rfbDesKey rfbClientDesKey
#define rfbUseKey rfbClientUseKey
#define rfbCPKey rfbClientCPKey

#include "../libvncserver/vncauth.c"
#include "../libvncserver/d3des.c"
