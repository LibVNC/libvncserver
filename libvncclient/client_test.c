/* A simple example of an RFB client */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <rfb/rfbclient.h>

void PrintRect(rfbClient* client, int x, int y, int w, int h) {
  rfbClientLog("Received an update for %d,%d,%d,%d.\n",x,y,w,h);
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
    rfbClientLog("bpp = %d (!=4)\n",bpp);
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
	fputc(client->frameBuffer[j+i+0],f);
	fputc(client->frameBuffer[j+i+1],f);
	fputc(client->frameBuffer[j+i+2],f);
      }
    }
  fclose(f);
}

int WaitForMessage(rfbClient* client,unsigned int usecs)
{
  fd_set fds;
  struct timeval timeout;
  int num;

  timeout.tv_sec=(usecs/1000000);
  timeout.tv_usec=(usecs%1000000);

  FD_ZERO(&fds);
  FD_SET(client->sock,&fds);

  num=select(client->sock+1, &fds, NULL, NULL, &timeout);
  if(num<0)
    rfbClientLog("Waiting for message failed: %d (%s)\n",errno,strerror(errno));

  return num;
}

int
main(int argc, char **argv)
{
  int i;
  rfbClient* client = rfbGetClient(&argc,argv,8,3,4);
  const char* vncServerHost="";
  int vncServerPort=5900;
  time_t t=time(0);

  client->GotFrameBufferUpdate = PrintRect;
  //client->GotFrameBufferUpdate = SaveFramebufferAsPGM;

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
  }

  client->appData.encodingsString="tight";
  if(!rfbInitClient(client,vncServerHost,vncServerPort)) {
    rfbClientCleanup(client);
    return 1;
  }

  while (time(0)-t<5) {
    static int i=0;
    fprintf(stderr,"\r%d",i++);
    if(WaitForMessage(client,500)<0)
      break;
    if(!HandleRFBServerMessage(client))
      break;
  }

  rfbClientCleanup(client);

  return 0;
}

