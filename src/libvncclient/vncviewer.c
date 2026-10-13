/*
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
 * vncviewer.c - the Xt-based VNC viewer.
 */

#ifdef WIN32
#include <winsock2.h>
#endif

#ifdef _MSC_VER
#define strdup _strdup /* Prevent POSIX deprecation warnings */
#endif

#ifdef __STRICT_ANSI__
#define _BSD_SOURCE
#define _POSIX_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <rfb/rfbclient.h>
#include "tls.h"
#if defined(LIBVNCSERVER_HAVE_LIBZ) && defined(LIBVNCSERVER_HAVE_LIBJPEG)
#include "turbojpeg.h"
#endif

static void Dummy(rfbClient* client) {
}
static rfbBool DummyPoint(rfbClient* client, int x, int y) {
  return TRUE;
}
static void DummyRect(rfbClient* client, int x, int y, int w, int h) {
}

#ifndef WIN32
#include <termios.h>
#endif

static char* ReadPassword(rfbClient* client) {
	int i;
	char* p=calloc(1,9);
	if (!p) return p;
#ifndef WIN32
	struct termios save,noecho;
	if(tcgetattr(fileno(stdin),&save)!=0) return p;
	noecho=save; noecho.c_lflag &= ~ECHO;
	if(tcsetattr(fileno(stdin),TCSAFLUSH,&noecho)!=0) return p;
#endif
	fprintf(stderr,"Password: ");
	fflush(stderr);
	i=0;
	while(1) {
		int c=fgetc(stdin);
		if(c=='\n')
			break;
		if(i<8) {
			p[i]=c;
			i++;
			p[i]=0;
		}
	}
#ifndef WIN32
	tcsetattr(fileno(stdin),TCSAFLUSH,&save);
#endif
	return p;
}
static int FrameBufferStridePixels(rfbClient* client)
{
  if (client->useFrameBufferViewport)
    return client->frameBufferViewportW;
  return client->width;
}

static rfbBool ClipToFrameBuffer(rfbClient* client, int x, int y, int w, int h,
                                 int* localX, int* localY,
                                 int* srcX, int* srcY,
                                 int* clippedW, int* clippedH)
{
  int fbX = 0;
  int fbY = 0;
  int fbW = client->width;
  int fbH = client->height;
  int x0, y0, x1, y1;

  if (w <= 0 || h <= 0)
    return FALSE;

  if (client->useFrameBufferViewport) {
    fbX = client->frameBufferViewportX;
    fbY = client->frameBufferViewportY;
    fbW = client->frameBufferViewportW;
    fbH = client->frameBufferViewportH;
  }

  x0 = x > fbX ? x : fbX;
  y0 = y > fbY ? y : fbY;
  x1 = (x + w) < (fbX + fbW) ? (x + w) : (fbX + fbW);
  y1 = (y + h) < (fbY + fbH) ? (y + h) : (fbY + fbH);

  if (x0 >= x1 || y0 >= y1)
    return FALSE;

  *localX = x0 - fbX;
  *localY = y0 - fbY;
  *srcX = x0 - x;
  *srcY = y0 - y;
  *clippedW = x1 - x0;
  *clippedH = y1 - y0;
  return TRUE;
}

static rfbBool RectFullyInsideFrameBuffer(rfbClient* client, int x, int y, int w, int h,
                                          int* localX, int* localY)
{
  int srcX, srcY, clippedW, clippedH;

  if (!ClipToFrameBuffer(client, x, y, w, h,
                         localX, localY, &srcX, &srcY, &clippedW, &clippedH))
    return FALSE;

  return srcX == 0 && srcY == 0 && clippedW == w && clippedH == h;
}

static rfbBool MallocFrameBuffer(rfbClient* client) {
  uint64_t allocSize;
  int width = client->width;
  int height = client->height;

  if(client->frameBuffer) {
    free(client->frameBuffer);
    client->frameBuffer = NULL;
  }

  if (client->useFrameBufferViewport) {
    width = client->frameBufferViewportW;
    height = client->frameBufferViewportH;
  }

  /* SECURITY: promote 'width' into uint64_t so that the multiplication does not overflow
     'width' and 'height' are 16-bit integers per RFB protocol design
     SIZE_MAX is the maximum value that can fit into size_t
  */
  allocSize = (uint64_t)width * height * client->format.bitsPerPixel/8;

  if (allocSize >= SIZE_MAX) {
    rfbClientErr("CRITICAL: cannot allocate frameBuffer, requested size is too large\n");
    return FALSE;
  }

  client->frameBuffer=malloc( (size_t)allocSize );

  if (client->frameBuffer == NULL)
    rfbClientErr("CRITICAL: frameBuffer allocation failed, requested size too large or not enough memory?\n");

  return client->frameBuffer?TRUE:FALSE;
}

/* messages */

static void FillRectangle(rfbClient* client, int x, int y, int w, int h, uint32_t colour) {
  int i,j;
  int localX, localY, srcX, srcY, clippedW, clippedH;
  int stride;

  if (client->frameBuffer == NULL) {
      return;
  }

  if (!ClipToFrameBuffer(client, x, y, w, h,
                         &localX, &localY, &srcX, &srcY, &clippedW, &clippedH)) {
    return;
  }

  stride = FrameBufferStridePixels(client);

#define FILL_RECT(BPP) \
    for(j=localY*stride;j<(localY+clippedH)*stride;j+=stride) \
      for(i=localX;i<localX+clippedW;i++) \
	((uint##BPP##_t*)client->frameBuffer)[j+i]=colour;

  switch(client->format.bitsPerPixel) {
  case  8: FILL_RECT(8);  break;
  case 16: FILL_RECT(16); break;
  case 32: FILL_RECT(32); break;
  default:
    rfbClientLog("Unsupported bitsPerPixel: %d\n",client->format.bitsPerPixel);
  }
}

static void CopyRectangle(rfbClient* client, const uint8_t* buffer, int x, int y, int w, int h) {
  int j;
  int localX, localY, srcX, srcY, clippedW, clippedH;
  int stride;

  if (client->frameBuffer == NULL) {
      return;
  }

  if (!ClipToFrameBuffer(client, x, y, w, h,
                         &localX, &localY, &srcX, &srcY, &clippedW, &clippedH)) {
    return;
  }

  stride = FrameBufferStridePixels(client);

#define COPY_RECT(BPP) \
  { \
    int srcStride = w * BPP / 8; \
    int dstStride = stride * BPP / 8; \
    int rowBytes = clippedW * BPP / 8; \
    const uint8_t* src = buffer + (srcY * srcStride) + (srcX * BPP / 8); \
    uint8_t* dst = client->frameBuffer + (localY * dstStride) + (localX * BPP / 8); \
    for (j = 0; j < clippedH; j++) { \
      memcpy(dst, src, rowBytes); \
      src += srcStride; \
      dst += dstStride; \
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
static void CopyRectangleFromRectangle(rfbClient* client, int src_x, int src_y, int w, int h, int dest_x, int dest_y) {
  int i,j;
  int localSrcX, localSrcY, localDestX, localDestY;
  int destClipLocalX, destClipLocalY, destClipSrcX, destClipSrcY, clippedW, clippedH;
  int clippedSrcX, clippedSrcY;
  int stride;

  if (client->frameBuffer == NULL) {
      return;
  }

  if (!ClipToFrameBuffer(client, dest_x, dest_y, w, h,
                         &destClipLocalX, &destClipLocalY,
                         &destClipSrcX, &destClipSrcY,
                         &clippedW, &clippedH)) {
    return;
  }

  clippedSrcX = src_x + destClipSrcX;
  clippedSrcY = src_y + destClipSrcY;

  if (!RectFullyInsideFrameBuffer(client, clippedSrcX, clippedSrcY, clippedW, clippedH,
                                  &localSrcX, &localSrcY) ||
      !RectFullyInsideFrameBuffer(client, dest_x + destClipSrcX, dest_y + destClipSrcY,
                                  clippedW, clippedH, &localDestX, &localDestY)) {
    rfbClientLog("CopyRect outside local framebuffer viewport: src=%dx%d at (%d, %d), dest=%dx%d at (%d, %d)\n",
                 w, h, src_x, src_y, w, h, dest_x, dest_y);
    return;
  }

  stride = FrameBufferStridePixels(client);

#define COPY_RECT_FROM_RECT(BPP) \
  { \
    uint##BPP##_t* _buffer=((uint##BPP##_t*)client->frameBuffer)+(localSrcY-localDestY)*stride+localSrcX-localDestX; \
    if (localDestY < localSrcY) { \
      for(j = localDestY*stride; j < (localDestY+clippedH)*stride; j += stride) { \
        if (localDestX < localSrcX) { \
          for(i = localDestX; i < localDestX+clippedW; i++) { \
            ((uint##BPP##_t*)client->frameBuffer)[j+i]=_buffer[j+i]; \
          } \
        } else { \
          for(i = localDestX+clippedW-1; i >= localDestX; i--) { \
            ((uint##BPP##_t*)client->frameBuffer)[j+i]=_buffer[j+i]; \
          } \
        } \
      } \
    } else { \
      for(j = (localDestY+clippedH-1)*stride; j >= localDestY*stride; j-=stride) { \
        if (localDestX < localSrcX) { \
          for(i = localDestX; i < localDestX+clippedW; i++) { \
            ((uint##BPP##_t*)client->frameBuffer)[j+i]=_buffer[j+i]; \
          } \
        } else { \
          for(i = localDestX+clippedW-1; i >= localDestX; i--) { \
            ((uint##BPP##_t*)client->frameBuffer)[j+i]=_buffer[j+i]; \
          } \
        } \
      } \
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

static void initAppData(AppData* data) {
	data->shareDesktop=TRUE;
	data->viewOnly=FALSE;
	data->encodingsString="tight zrle ultra copyrect hextile zlib corre rre raw";
	data->useBGR233=FALSE;
	data->nColours=0;
	data->forceOwnCmap=FALSE;
	data->forceTrueColour=FALSE;
	data->requestedDepth=0;
	data->compressLevel=3;
	data->qualityLevel=5;
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
	data->enableJPEG=TRUE;
#else
	data->enableJPEG=FALSE;
#endif
	data->useRemoteCursor=FALSE;
}

static void parse_host_and_port(const char *input, char **host, int *port) {
    char *open_bracket = strchr(input, '[');
    char *close_bracket = strchr(input, ']');

    if (open_bracket && close_bracket && close_bracket > open_bracket) {
        // IPv6 address with brackets, allocate and copy IP inside brackets
        size_t ip_len = close_bracket - open_bracket - 1;
        *host = malloc(ip_len + 1);
        strncpy(*host, open_bracket + 1, ip_len);
        (*host)[ip_len] = '\0';
        // check for port via last colon
        char *colon = strchr(close_bracket, ':');
        if (colon) {
            // Parse port as integer
            *port = atoi(colon + 1);
        }
    } else {
        // IPv4 or IPv6 w/o brackets, look for colons to decide which one
        char *first_colon = strchr(input, ':');
        char *last_colon = strrchr(input, ':');
        if (first_colon) {
            // check IPv4 vs IPv6
            if (first_colon == last_colon) {
                // one colon, i.e. IPv4 with port, allocate and copy IP (before last colon)
                size_t ip_len = first_colon - input;
                *host = malloc(ip_len + 1);
                strncpy(*host, input, ip_len);
                (*host)[ip_len] = '\0';
                // Parse port as integer
                *port = atoi(first_colon + 1);
            } else {
                // IPv6, just copy input
                *host = strdup(input);
            }
        } else {
            // No colon, i.e. IPv4 w/o port, just copy input
            *host = strdup(input);
        }
    }
}

rfbClient* rfbGetClient(int bitsPerSample,int samplesPerPixel,
			int bytesPerPixel) {
#ifdef WIN32
    WSADATA unused;
#endif
  rfbClient* client=(rfbClient*)calloc(sizeof(rfbClient),1);
  if(!client) {
    rfbClientErr("Couldn't allocate client structure!\n");
    return NULL;
  }
#ifdef WIN32
  if((errno = WSAStartup(MAKEWORD(2,0), &unused)) != 0) {
      rfbClientErr("Could not init Windows Sockets: %s\n", strerror(errno));
      return NULL;
  }
#endif
  initAppData(&client->appData);
  client->endianTest = 1;
  client->programName="";
  client->serverHost=strdup("");
  client->serverPort=5900;

  client->destHost = NULL;
  client->destPort = 5900;
  
  client->connectTimeout = DEFAULT_CONNECT_TIMEOUT;
  client->readTimeout = DEFAULT_READ_TIMEOUT;

  /* default: use complete frame buffer */ 
  client->updateRect.x = -1;
  client->useFrameBufferViewport = FALSE;
  client->frameBufferViewportX = 0;
  client->frameBufferViewportY = 0;
  client->frameBufferViewportW = 0;
  client->frameBufferViewportH = 0;
 
  client->frameBuffer = NULL;
  client->outputWindow = 0;
 
  client->format.bitsPerPixel = bytesPerPixel*8;
  client->format.depth = bitsPerSample*samplesPerPixel;
  client->appData.requestedDepth=client->format.depth;
  client->format.bigEndian = *(char *)&client->endianTest?FALSE:TRUE;
  client->format.trueColour = 1;

  if (client->format.bitsPerPixel == 8) {
    client->format.redMax = 7;
    client->format.greenMax = 7;
    client->format.blueMax = 3;
    client->format.redShift = 0;
    client->format.greenShift = 3;
    client->format.blueShift = 6;
  } else {
    client->format.redMax = (1 << bitsPerSample) - 1;
    client->format.greenMax = (1 << bitsPerSample) - 1;
    client->format.blueMax = (1 << bitsPerSample) - 1;
    if(!client->format.bigEndian) {
      client->format.redShift = 0;
      client->format.greenShift = bitsPerSample;
      client->format.blueShift = bitsPerSample * 2;
    } else {
      if(client->format.bitsPerPixel==8*3) {
	client->format.redShift = bitsPerSample*2;
	client->format.greenShift = bitsPerSample*1;
	client->format.blueShift = 0;
      } else {
	client->format.redShift = bitsPerSample*3;
	client->format.greenShift = bitsPerSample*2;
	client->format.blueShift = bitsPerSample;
      }
    }
  }

  client->bufoutptr=client->buf;
  client->buffered=0;

#ifdef LIBVNCSERVER_HAVE_LIBZ
  client->raw_buffer_size = -1;
  client->decompStreamInited = FALSE;

#ifdef LIBVNCSERVER_HAVE_LIBJPEG
  memset(client->zlibStreamActive,0,sizeof(rfbBool)*4);
#endif
#endif

  client->HandleCursorPos = DummyPoint;
  client->SoftCursorLockArea = DummyRect;
  client->SoftCursorUnlockScreen = Dummy;
  client->GotFrameBufferUpdate = DummyRect;
  client->GotCopyRect = CopyRectangleFromRectangle;
  client->GotFillRect = FillRectangle;
  client->GotBitmap = CopyRectangle;
  client->FinishedFrameBufferUpdate = NULL;
  client->GetPassword = ReadPassword;
  client->MallocFrameBuffer = MallocFrameBuffer;
  client->Bell = Dummy;
  client->CurrentKeyboardLedState = 0;
  client->HandleKeyboardLedState = (HandleKeyboardLedStateProc)DummyPoint;
  client->QoS_DSCP = 0;

  client->authScheme = 0;
  client->subAuthScheme = 0;
  client->GetCredential = NULL;
  client->tlsSession = NULL;
  client->LockWriteToTLS = NULL;
  client->UnlockWriteToTLS = NULL;
  client->sock = RFB_INVALID_SOCKET;
  client->listenSock = RFB_INVALID_SOCKET;
  client->listenAddress = NULL;
  client->listen6Sock = RFB_INVALID_SOCKET;
  client->listen6Address = NULL;
  client->clientAuthSchemes = NULL;

#ifdef LIBVNCSERVER_HAVE_SASL
  client->GetSASLMechanism = NULL;
  client->GetUser = NULL;
  client->saslSecret = NULL;
#endif /* LIBVNCSERVER_HAVE_SASL */

  client->requestedResize = FALSE;
  client->screen.width = 0;
  client->screen.height = 0;

  return client;
}

rfbBool rfbClientConnect(rfbClient* client) {
  /* Unless we accepted an incoming connection, make a TCP connection to the
     given VNC server */

  if (!client->listenSpecified) {
    if (!client->serverHost)
      return FALSE;
    if (client->destHost) {
      if (!ConnectToRFBRepeater(client,client->serverHost,client->serverPort,client->destHost,client->destPort))
        return FALSE;
    } else {
      if (!ConnectToRFBServer(client,client->serverHost,client->serverPort))
        return FALSE;
    }
  }
  return TRUE;
}


rfbBool rfbClientInitialise(rfbClient* client) {
  /* Initialise the VNC connection, including reading the password */

  if (!InitialiseRFBConnection(client))
    return FALSE;

  client->width=client->si.framebufferWidth;
  client->height=client->si.framebufferHeight;

  if (client->useFrameBufferViewport) {
    if (client->frameBufferViewportX + client->frameBufferViewportW > client->width ||
        client->frameBufferViewportY + client->frameBufferViewportH > client->height) {
      rfbClientErr("Framebuffer viewport %dx%d at (%d, %d) exceeds remote framebuffer %dx%d\n",
                   client->frameBufferViewportW, client->frameBufferViewportH,
                   client->frameBufferViewportX, client->frameBufferViewportY,
                   client->width, client->height);
      return FALSE;
    }
  }

  if (!client->MallocFrameBuffer(client))
    return FALSE;

  if (!SetFormatAndEncodings(client))
    return FALSE;

  if (client->updateRect.x < 0) {
    client->updateRect.x = client->updateRect.y = 0;
    client->updateRect.w = client->width;
    client->updateRect.h = client->height;
    client->isUpdateRectManagedByLib = TRUE;
  }

  if (client->appData.scaleSetting>1)
  {
      if (!SendScaleSetting(client, client->appData.scaleSetting))
          return FALSE;
      if (!SendFramebufferUpdateRequest(client,
			      client->updateRect.x / client->appData.scaleSetting,
			      client->updateRect.y / client->appData.scaleSetting,
			      client->updateRect.w / client->appData.scaleSetting,
			      client->updateRect.h / client->appData.scaleSetting,
			      FALSE))
	      return FALSE;
  }
  else
  {
      if (!SendFramebufferUpdateRequest(client,
			      client->updateRect.x, client->updateRect.y,
			      client->updateRect.w, client->updateRect.h,
			      FALSE))
      return FALSE;
  }

  return TRUE;
}

rfbBool rfbInitClient(rfbClient* client,int* argc,char** argv) {
  int i,j;

  if(argv && argc && *argc) {
    if(client->programName==0)
      client->programName=argv[0];

    for (i = 1; i < *argc; i++) {
      j = i;
      if (strcmp(argv[i], "-listen") == 0) {
	listenForIncomingConnections(client);
	break;
      } else if (strcmp(argv[i], "-listennofork") == 0) {
	listenForIncomingConnectionsNoFork(client, -1);
	break;
      } else if (strcmp(argv[i], "-play") == 0) {
	client->serverPort = -1;
	j++;
      } else if (i+1<*argc && strcmp(argv[i], "-encodings") == 0) {
	client->appData.encodingsString = argv[i+1];
	j+=2;
      } else if (i+1<*argc && strcmp(argv[i], "-compress") == 0) {
	client->appData.compressLevel = atoi(argv[i+1]);
	j+=2;
      } else if (i+1<*argc && strcmp(argv[i], "-quality") == 0) {
	client->appData.qualityLevel = atoi(argv[i+1]);
	j+=2;
      } else if (i+1<*argc && strcmp(argv[i], "-scale") == 0) {
        client->appData.scaleSetting = atoi(argv[i+1]);
        j+=2;
      } else if (i+1<*argc && strcmp(argv[i], "-qosdscp") == 0) {
        client->QoS_DSCP = atoi(argv[i+1]);
        j+=2;
      } else if (i+1<*argc && strcmp(argv[i], "-repeaterdest") == 0) {
	free(client->destHost);
	client->destPort = 5900;

        parse_host_and_port(argv[i], &client->destHost, &client->destPort);

        j+=2;
      } else {
	free(client->serverHost);

        parse_host_and_port(argv[i], &client->serverHost, &client->serverPort);

	if(client->serverPort >= 0 && client->serverPort < 5900)
	  client->serverPort += 5900;
      }
      /* purge arguments */
      if (j>i) {
	*argc-=j-i;
	memmove(argv+i,argv+j,(*argc-i)*sizeof(char*));
	i--;
      }
    }
  }

  if(!rfbClientConnect(client) || !rfbClientInitialise(client)) {
    rfbClientCleanup(client);
    return FALSE;
  }

  return TRUE;
}

void rfbClientCleanup(rfbClient* client) {
#ifdef LIBVNCSERVER_HAVE_LIBZ
  int i;

  for ( i = 0; i < 4; i++ ) {
    if (client->zlibStreamActive[i] == TRUE ) {
      if (inflateEnd (&client->zlibStream[i]) != Z_OK &&
	  client->zlibStream[i].msg != NULL)
	rfbClientLog("inflateEnd: %s\n", client->zlibStream[i].msg);
    }
  }

  if ( client->decompStreamInited == TRUE ) {
    if (inflateEnd (&client->decompStream) != Z_OK &&
	client->decompStream.msg != NULL)
      rfbClientLog("inflateEnd: %s\n", client->decompStream.msg );
  }

#ifdef LIBVNCSERVER_HAVE_LIBJPEG
  if(client->tjhnd){
    tjDestroy(client->tjhnd);
    client->tjhnd = NULL;
  }
#endif /* LIBVNCSERVER_HAVE_LIBJPEG */
#endif

  free(client->ultra_buffer);
  free(client->raw_buffer);

  FreeTLS(client);

  while (client->clientData) {
    rfbClientData* next = client->clientData->next;
    free(client->clientData);
    client->clientData = next;
  }

  free(client->vncRec);

  if (client->sock != RFB_INVALID_SOCKET)
    rfbCloseSocket(client->sock);
  if (client->listenSock != RFB_INVALID_SOCKET)
    rfbCloseSocket(client->listenSock);
  if (client->listen6Sock != RFB_INVALID_SOCKET)
    rfbCloseSocket(client->listen6Sock);
  free(client->desktopName);
  free(client->serverHost);
  free(client->destHost);
  free(client->clientAuthSchemes);
  free(client->rcSource);
  free(client->rcMask);

#ifdef LIBVNCSERVER_HAVE_SASL
  free(client->saslSecret);
  if (client->saslconn)
    sasl_dispose(&client->saslconn);
#endif /* LIBVNCSERVER_HAVE_SASL */

#ifdef WIN32
  if(WSACleanup() != 0) {
      errno=WSAGetLastError();
      rfbClientErr("Could not terminate Windows Sockets: %s\n", strerror(errno));
  }
#endif

  free(client);
}
