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

#define Swap16IfLE(s) \
    (*(char *)&client->endianTest ? ((((s) & 0xff) << 8) | (((s) >> 8) & 0xff)) : (s))

#define Swap32IfLE(l) \
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
  Bool shareDesktop;
  Bool viewOnly;
  Bool fullScreen;
  Bool grabKeyboard;
  Bool raiseOnBeep;

  const char* encodingsString;

  Bool useBGR233;
  int nColours;
  Bool useSharedColours;
  Bool forceOwnCmap;
  Bool forceTrueColour;
  int requestedDepth;

  Bool useShm;

  int wmDecorationWidth;
  int wmDecorationHeight;

  Bool debug;

  int popupButtonCount;

  int bumpScrollTime;
  int bumpScrollPixels;

  int compressLevel;
  int qualityLevel;
  Bool enableJPEG;
  Bool useRemoteCursor;
} AppData;


struct _rfbClient;

typedef Bool (*HandleCursorPosProc)(struct _rfbClient* client, int x, int y);
typedef void (*SoftCursorLockAreaProc)(struct _rfbClient* client, int x, int y, int w, int h);
typedef void (*SoftCursorUnlockScreenProc)(struct _rfbClient* client);
typedef void (*FramebufferUpdateReceivedProc)(struct _rfbClient* client, int x, int y, int w, int h);
typedef char* (*GetPasswordProc)(struct _rfbClient* client);
typedef Bool (*MallocFrameBufferProc)(struct _rfbClient* client);
typedef void (*BellProc)(struct _rfbClient* client);

typedef struct _rfbClient {
	uint8_t* frameBuffer;
	int width, height;

	int endianTest;

	AppData appData;

	const char* programName;
	const char* serverHost;
	int serverPort;
	Bool listenSpecified;
	int listenPort, flashPort;

	/* Note that the CoRRE encoding uses this buffer and assumes it is big enough
	   to hold 255 * 255 * 32 bits -> 260100 bytes.  640*480 = 307200 bytes.
	   Hextile also assumes it is big enough to hold 16 * 16 * 32 bits.
	   Tight encoding assumes BUFFER_SIZE is at least 16384 bytes. */

#define BUFFER_SIZE (640*480)
	char buffer[BUFFER_SIZE];

	/* rfbproto.c */

	int sock;
	Bool canUseCoRRE;
	Bool canUseHextile;
	char *desktopName;
	rfbPixelFormat format;
	rfbServerInitMsg si;
	char *serverCutText;
	Bool newServerCutText;

	/* cursor.c */
	uint8_t *rcSource, *rcMask;

	/* hooks */
	HandleCursorPosProc HandleCursorPos;
	SoftCursorLockAreaProc SoftCursorLockArea;
	SoftCursorUnlockScreenProc SoftCursorUnlockScreen;
	FramebufferUpdateReceivedProc FramebufferUpdateReceived;
	GetPasswordProc GetPassword;
	MallocFrameBufferProc MallocFrameBuffer;
	BellProc Bell;
} rfbClient;

/* cursor.c */

// TODO: make callback

extern Bool HandleCursorShape(rfbClient* client,int xhot, int yhot, int width, int height, uint32_t enc);

/* listen.c */

extern void listenForIncomingConnections(rfbClient* viewer);

/* rfbproto.c */

extern Bool ConnectToRFBServer(rfbClient* client,const char *hostname, int port);
extern Bool InitialiseRFBConnection(rfbClient* client);
extern Bool SetFormatAndEncodings(rfbClient* client);
extern Bool SendIncrementalFramebufferUpdateRequest(rfbClient* client);
extern Bool SendFramebufferUpdateRequest(rfbClient* client,
					 int x, int y, int w, int h,
					 Bool incremental);
extern Bool SendPointerEvent(rfbClient* client,int x, int y, int buttonMask);
extern Bool SendKeyEvent(rfbClient* client,uint32_t key, Bool down);
extern Bool SendClientCutText(rfbClient* client,char *str, int len);
extern Bool HandleRFBServerMessage(rfbClient* client);

extern void PrintPixelFormat(rfbPixelFormat *format);

/* sockets.c */

extern Bool errorMessageOnReadFailure;

extern Bool ReadFromRFBServer(rfbClient* client, char *out, unsigned int n);
extern Bool WriteExact(rfbClient* client, char *buf, int n);
extern int FindFreeTcpPort(void);
extern int ListenAtTcpPort(int port);
extern int ConnectToTcpAddr(unsigned int host, int port);
extern int AcceptTcpConnection(int listenSock);
extern Bool SetNonBlocking(int sock);

extern Bool StringToIPAddr(const char *str, unsigned int *addr);
extern Bool SameMachine(int sock);

