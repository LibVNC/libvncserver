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
static Bool DummyPoint(rfbClient* client, int x, int y) {
  return TRUE;
}
static void DummyRect(rfbClient* client, int x, int y, int w, int h) {
}
static char* NoPassword(rfbClient* client) {
  return "";
}
static Bool MallocFrameBuffer(rfbClient* client) {
  if(client->frameBuffer)
    free(client->frameBuffer);
  client->frameBuffer=malloc(client->width*client->height*client->format.bitsPerPixel/8);
  return client->frameBuffer?TRUE:FALSE;
}

rfbClient* rfbGetClient(int* argc,char** argv,
			int bitsPerSample,int samplesPerPixel,
			int bytesPerPixel) {
  rfbClient* client=(rfbClient*)calloc(sizeof(rfbClient),1);
  client->programName = argv[0];
  client->endianTest = 1;

  client->format.bitsPerPixel = bytesPerPixel*8;
  client->format.depth = bitsPerSample*samplesPerPixel;
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
  client->FramebufferUpdateReceived = DummyRect;
  client->GetPassword = NoPassword;
  client->MallocFrameBuffer = MallocFrameBuffer;
  client->Bell = Dummy;

  return client;
}

void PrintRect(rfbClient* client, int x, int y, int w, int h) {
  fprintf(stderr,"Received an update for %d,%d,%d,%d.\n",x,y,w,h);
}

void SaveFramebufferAsPGM(rfbClient* client, int x, int y, int w, int h) {
  static time_t t=0,t1;
  FILE* f;
  int i,j;
  int bpp=client->format.bitsPerPixel/8;
  int row_stride=client->width*bpp;

  /* save one picture only if the last is older than 2 seconds */
  t1=time(0);
  if(t1-t>2)
    t=t1;
  else
    return;

  /* assert bpp=4 */
  if(bpp!=4) {
    fprintf(stderr,"bpp = %d (!=4)\n",bpp);
    return;
  }

  f=fopen("/tmp/framebuffer.ppm","wb");

  fprintf(f,"P6\n# %s\n%d %d\n255\n",client->desktopName,client->width,client->height);
  for(j=0;j<client->height*row_stride;j+=row_stride)
    for(i=0;i<client->width*bpp;i+=bpp) {
      if(client->format.bigEndian) {
	fputc(client->frameBuffer[j+i+bpp-1],f);
	fputc(client->frameBuffer[j+i+bpp-2],f);
	fputc(client->frameBuffer[j+i+bpp-3],f);
      } else {
	fputc(client->frameBuffer[j+i+bpp+0],f);
	fputc(client->frameBuffer[j+i+bpp+1],f);
	fputc(client->frameBuffer[j+i+bpp+2],f);
      }
    }
  fclose(f);
}

void
vncEncryptBytes(unsigned char *bytes, char *passwd);

int
main(int argc, char **argv)
{
  int i;
  rfbClient* client = rfbGetClient(&argc,argv,8,3,4);
  const char* vncServerHost="";
  int vncServerPort=5900;

  char buf1[]="pass",buf2[]="pass";
  vncEncryptBytes(buf1,buf2);

  client->FramebufferUpdateReceived = PrintRect;
  client->FramebufferUpdateReceived = SaveFramebufferAsPGM;

  /* The -listen option is used to make us a daemon process which listens for
     incoming connections from servers, rather than actively connecting to a
     given server. The -tunnel and -via options are useful to create
     connections tunneled via SSH port forwarding. We must test for the
     -listen option before invoking any Xt functions - this is because we use
     forking, and Xt doesn't seem to cope with forking very well. For -listen
     option, when a successful incoming connection has been accepted,
     listenForIncomingConnections() returns, setting the listenSpecified
     flag. */

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-listen") == 0) {
      listenForIncomingConnections(client);
      break;
    } else {
      char* colon=strchr(argv[i],':');

      vncServerHost=argv[i];
      if(colon) {
	*colon=0;
	vncServerPort=atoi(colon+1);
      } else
	vncServerPort=0;
      vncServerPort+=5900;
    }

    /* TODO:
    if (strcmp(argv[i], "-tunnel") == 0 || strcmp(argv[i], "-via") == 0) {
      if (!createTunnel(&argc, argv, i))
	exit(1);
      break;
    }
    */
  }

  /* Call the main Xt initialisation function.  It parses command-line options,
     generating appropriate resource specs, and makes a connection to the X
     display. */

  /* TODO: cmdline args
  toplevel = XtVaAppInitialize(&appContext, "Vncviewer",
			       cmdLineOptions, numCmdLineOptions,
			       &argc, argv, fallback_resources,
			       XtNborderWidth, 0, NULL);

  dpy = XtDisplay(toplevel);
  */

  /* Interpret resource specs and process any remaining command-line arguments
     (i.e. the VNC server name).  If the server name isn't specified on the
     command line, getArgsAndResources() will pop up a dialog box and wait
     for one to be entered. */

  /*
  GetArgsAndResources(argc, argv);
  */

  /* Unless we accepted an incoming connection, make a TCP connection to the
     given VNC server */

  if (!client->listenSpecified) {
    if (!ConnectToRFBServer(client,vncServerHost, vncServerPort)) exit(1);
  }

  /* Initialise the VNC connection, including reading the password */

  if (!InitialiseRFBConnection(client)) exit(1);

  SetFormatAndEncodings(client);

  client->width=client->si.framebufferWidth;
  client->height=client->si.framebufferHeight;
  client->MallocFrameBuffer(client);
  SendFramebufferUpdateRequest(client,0,0,client->width,client->height,FALSE);

  /* Now enter the main loop, processing VNC messages.  X events will
     automatically be processed whenever the VNC connection is idle. */

  while (1) {
    if (!HandleRFBServerMessage(client))
      break;
  }

  return 0;
}
