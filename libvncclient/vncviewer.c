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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <rfb/rfbclient.h>

static void Dummy(rfbClient* client) {
}
static rfbBool DummyPoint(rfbClient* client, int x, int y) {
  return TRUE;
}
static void DummyRect(rfbClient* client, int x, int y, int w, int h) {
}
static char* NoPassword(rfbClient* client) {
  return strdup("");
}
#include <stdio.h>
#include <termios.h>
static char* ReadPassword(rfbClient* client) {
	int i=8;
	char* p=malloc(9);
	struct termios save,noecho;
	p[0]=0;
	if(tcgetattr(fileno(stdin),&save)!=0) return p;
	noecho=save; noecho.c_lflag &= ~ECHO;
	if(tcsetattr(fileno(stdin),TCSAFLUSH,&noecho)!=0) return p;
	fprintf(stderr,"Password: ");
	getline(&p,&i,stdin);
	if(i>0 && p[i-2]=='\n') p[i-2]=0;
	tcsetattr(fileno(stdin),TCSAFLUSH,&save);
	return p;
}
static rfbBool MallocFrameBuffer(rfbClient* client) {
  if(client->frameBuffer)
    free(client->frameBuffer);
  client->frameBuffer=malloc(client->width*client->height*client->format.bitsPerPixel/8);
  return client->frameBuffer?TRUE:FALSE;
}

static void initAppData(AppData* data) {
	data->shareDesktop=TRUE;
	data->viewOnly=FALSE;
	data->encodingsString="tight hextile zlib corre rre raw";
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

rfbClient* rfbGetClient(int bitsPerSample,int samplesPerPixel,
			int bytesPerPixel) {
  rfbClient* client=(rfbClient*)calloc(sizeof(rfbClient),1);
  initAppData(&client->appData);
  client->programName = 0;
  client->endianTest = 1;

  client->format.bitsPerPixel = bytesPerPixel*8;
  client->format.depth = bitsPerSample*samplesPerPixel;
  client->appData.requestedDepth=client->format.depth;
  client->format.bigEndian = *(char *)&client->endianTest?FALSE:TRUE;
  client->format.trueColour = TRUE;

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

  client->HandleCursorPos = DummyPoint;
  client->SoftCursorLockArea = DummyRect;
  client->SoftCursorUnlockScreen = Dummy;
  client->GotFrameBufferUpdate = DummyRect;
  client->GetPassword = ReadPassword;
  client->MallocFrameBuffer = MallocFrameBuffer;
  client->Bell = Dummy;

  return client;
}

static rfbBool rfbInitConnection(rfbClient* client)
{
  /* Unless we accepted an incoming connection, make a TCP connection to the
     given VNC server */

  if (!client->listenSpecified) {
    if (!client->serverHost || !ConnectToRFBServer(client,client->serverHost,client->serverPort))
      return FALSE;
  }

  /* Initialise the VNC connection, including reading the password */

  if (!InitialiseRFBConnection(client))
    return FALSE;

  if (!SetFormatAndEncodings(client))
    return FALSE;

  client->width=client->si.framebufferWidth;
  client->height=client->si.framebufferHeight;
  client->MallocFrameBuffer(client);
  if (!SendFramebufferUpdateRequest(client,
				    0,0,client->width,client->height,FALSE))
    return FALSE;

  return TRUE;
}

rfbBool rfbInitClient(rfbClient* client,int* argc,char** argv) {
  int i,j;

  if(client->programName==0)
    client->programName=argv[0];

  for (i = 1; i < *argc; i++) {
    j = i;
    if (strcmp(argv[i], "-listen") == 0) {
      listenForIncomingConnections(client);
      break;
    } else if (strcmp(argv[i], "-play") == 0) {
      client->serverPort = -1;
      j++;
    } else if (i+1<*argc && strcmp(argv[i], "-encodings") == 0) {
      client->appData.encodingsString = argv[i+1];
      j+=2;
    } else {
      char* colon=strchr(argv[i],':');

      client->serverHost=argv[i];
      if(colon) {
	*colon=0;
	client->serverPort=atoi(colon+1);
      }
      if(client->serverPort>=0 && client->serverPort<5900)
	client->serverPort+=5900;
    }
    /* purge arguments */
    if (j>i) {
      *argc-=j-i;
      memmove(argv+i,argv+j,(*argc-i)*sizeof(char*));
      i--;
    }
  }

  if(!rfbInitConnection(client)) {
    rfbClientCleanup(client);
    return FALSE;
  }

  return TRUE;
}

void rfbClientCleanup(rfbClient* client) {
  free(client);
}
