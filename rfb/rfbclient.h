/*
 *  Copyright (C) 2000, 2001 Const Kaplinsky.  All Rights Reserved.
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
 * vncviewer.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <rfb/rfbproto.h>
#include <rfb/keysym.h>

#define rfbClientSwap16IfLE(s) \
    (*(char *)&client->endianTest ? ((((s) & 0xff) << 8) | (((s) >> 8) & 0xff)) : (s))

#define rfbClientSwap32IfLE(l) \
    (*(char *)&client->endianTest ? ((((l) & 0xff000000) >> 24) | \
			     (((l) & 0x00ff0000) >> 8)  | \
			     (((l) & 0x0000ff00) << 8)  | \
			     (((l) & 0x000000ff) << 24))  : (l))

#define FLASH_PORT_OFFSET 5400
#define LISTEN_PORT_OFFSET 5500
#define TUNNEL_PORT_OFFSET 5500
#define SERVER_PORT_OFFSET 5900

#define DEFAULT_SSH_CMD "/usr/bin/ssh"
#define DEFAULT_TUNNEL_CMD  \
  (DEFAULT_SSH_CMD " -f -L %L:localhost:%R %H sleep 20")
#define DEFAULT_VIA_CMD     \
  (DEFAULT_SSH_CMD " -f -L %L:%H:%R %G sleep 20")


typedef struct {
  rfbBool shareDesktop;
  rfbBool viewOnly;
  rfbBool fullScreen;
  rfbBool grabKeyboard;
  rfbBool raiseOnBeep;

  const char* encodingsString;

  rfbBool useBGR233;
  int nColours;
  rfbBool useSharedColours;
  rfbBool forceOwnCmap;
  rfbBool forceTrueColour;
  int requestedDepth;

  rfbBool useShm;

  int wmDecorationWidth;
  int wmDecorationHeight;

  rfbBool debug;

  int popupButtonCount;

  int bumpScrollTime;
  int bumpScrollPixels;

  int compressLevel;
  int qualityLevel;
  rfbBool enableJPEG;
  rfbBool useRemoteCursor;
} AppData;


struct _rfbClient;

typedef rfbBool (*HandleCursorPosProc)(struct _rfbClient* client, int x, int y);
typedef void (*SoftCursorLockAreaProc)(struct _rfbClient* client, int x, int y, int w, int h);
typedef void (*SoftCursorUnlockScreenProc)(struct _rfbClient* client);
typedef void (*GotFrameBufferUpdateProc)(struct _rfbClient* client, int x, int y, int w, int h);
typedef char* (*GetPasswordProc)(struct _rfbClient* client);
typedef rfbBool (*MallocFrameBufferProc)(struct _rfbClient* client);
typedef void (*BellProc)(struct _rfbClient* client);

typedef struct _rfbClient {
	uint8_t* frameBuffer;
	int width, height;

	int endianTest;

	AppData appData;

	const char* programName;
	const char* serverHost;
	int serverPort;
	rfbBool listenSpecified;
	int listenPort, flashPort;

	/* Note that the CoRRE encoding uses this buffer and assumes it is big enough
	   to hold 255 * 255 * 32 bits -> 260100 bytes.  640*480 = 307200 bytes.
	   Hextile also assumes it is big enough to hold 16 * 16 * 32 bits.
	   Tight encoding assumes BUFFER_SIZE is at least 16384 bytes. */

#define BUFFER_SIZE (640*480)
	char buffer[BUFFER_SIZE];

	/* rfbproto.c */

	int sock;
	rfbBool canUseCoRRE;
	rfbBool canUseHextile;
	char *desktopName;
	rfbPixelFormat format;
	rfbServerInitMsg si;
	char *serverCutText;
	rfbBool newServerCutText;

	/* cursor.c */
	uint8_t *rcSource, *rcMask;

	/* hooks */
	HandleCursorPosProc HandleCursorPos;
	SoftCursorLockAreaProc SoftCursorLockArea;
	SoftCursorUnlockScreenProc SoftCursorUnlockScreen;
	GotFrameBufferUpdateProc GotFrameBufferUpdate;
	GetPasswordProc GetPassword;
	MallocFrameBufferProc MallocFrameBuffer;
	BellProc Bell;
} rfbClient;

/* cursor.c */

extern rfbBool HandleCursorShape(rfbClient* client,int xhot, int yhot, int width, int height, uint32_t enc);

/* listen.c */

extern void listenForIncomingConnections(rfbClient* viewer);

/* rfbproto.c */

extern rfbBool rfbEnableClientLogging;
extern void rfbClientLog(const char *format, ...);
extern rfbBool ConnectToRFBServer(rfbClient* client,const char *hostname, int port);
extern rfbBool InitialiseRFBConnection(rfbClient* client);
extern rfbBool SetFormatAndEncodings(rfbClient* client);
extern rfbBool SendIncrementalFramebufferUpdateRequest(rfbClient* client);
extern rfbBool SendFramebufferUpdateRequest(rfbClient* client,
					 int x, int y, int w, int h,
					 rfbBool incremental);
extern rfbBool SendPointerEvent(rfbClient* client,int x, int y, int buttonMask);
extern rfbBool SendKeyEvent(rfbClient* client,uint32_t key, rfbBool down);
extern rfbBool SendClientCutText(rfbClient* client,char *str, int len);
extern rfbBool HandleRFBServerMessage(rfbClient* client);

extern void PrintPixelFormat(rfbPixelFormat *format);

/* sockets.c */

extern rfbBool errorMessageOnReadFailure;

extern rfbBool ReadFromRFBServer(rfbClient* client, char *out, unsigned int n);
extern rfbBool WriteToRFBServer(rfbClient* client, char *buf, int n);
extern int FindFreeTcpPort(void);
extern int ListenAtTcpPort(int port);
extern int ConnectClientToTcpAddr(unsigned int host, int port);
extern int AcceptTcpConnection(int listenSock);
extern rfbBool SetNonBlocking(int sock);

extern rfbBool StringToIPAddr(const char *str, unsigned int *addr);
extern rfbBool SameMachine(int sock);

/* vncviewer.c */
rfbClient* rfbGetClient(int* argc,char** argv,int bitsPerSample,int samplesPerPixel,int bytesPerPixel);
rfbBool rfbInitClient(rfbClient* client,const char* vncServerHost,int vncServerPort);
void rfbClientCleanup(rfbClient* client);
