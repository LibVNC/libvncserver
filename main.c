/*
 *  This file is called main.c, because it contains most of the new functions
 *  for use with LibVNCServer.
 *
 *  LibVNCServer (C) 2001 Johannes E. Schindelin <Johannes.Schindelin@gmx.de>
 *  Original OSXvnc (C) 2001 Dan McGuirk <mcguirk@incompleteness.net>.
 *  Original Xvnc (C) 1999 AT&T Laboratories Cambridge.  
 *  All Rights Reserved.
 *
 *  see GPL (latest version) for full details
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#ifndef false
#define false 0
#define true -1
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include "rfb.h"
#include "sraRegion.h"

MUTEX(logMutex);

/*
 * rfbLog prints a time-stamped message to the log file (stderr).
 */

void
rfbLog(char *format, ...)
{
    va_list args;
    char buf[256];
    time_t log_clock;

    LOCK(logMutex);
    va_start(args, format);

    time(&log_clock);
    strftime(buf, 255, "%d/%m/%Y %T ", localtime(&log_clock));
    fprintf(stderr, buf);

    vfprintf(stderr, format, args);
    fflush(stderr);

    va_end(args);
    UNLOCK(logMutex);
}

void rfbLogPerror(char *str)
{
    rfbLog("%s: %s\n", str, strerror(errno));
}

void rfbScheduleCopyRegion(rfbScreenInfoPtr rfbScreen,sraRegionPtr copyRegion,int dx,int dy)
{  
   rfbClientIteratorPtr iterator;
   rfbClientPtr cl;

  iterator=rfbGetClientIterator(rfbScreen);
   while((cl=rfbClientIteratorNext(iterator))) {
     LOCK(cl->updateMutex);
     if(cl->useCopyRect) {
       while(!sraRgnEmpty(cl->copyRegion) && (cl->copyDX!=dx || cl->copyDY!=dy)) {
#ifdef HAVE_PTHREADS
	 if(cl->screen->backgroundLoop) {
	   SIGNAL(cl->updateCond);
	   UNLOCK(cl->updateMutex);
	   LOCK(cl->updateMutex);
	 } else
#endif
	   rfbSendFramebufferUpdate(cl,cl->copyRegion);
       }
       sraRgnOr(cl->copyRegion,copyRegion);
       cl->copyDX = dx;
       cl->copyDY = dy;
     } else {
       sraRgnOr(cl->modifiedRegion,copyRegion);
     }
     SIGNAL(cl->updateCond);
     UNLOCK(cl->updateMutex);
   }

   rfbReleaseClientIterator(iterator);
}

void rfbDoCopyRegion(rfbScreenInfoPtr rfbScreen,sraRegionPtr copyRegion,int dx,int dy)
{
   sraRectangleIterator* i;
   sraRect rect;
   int j,widthInBytes,bpp=rfbScreen->rfbServerFormat.bitsPerPixel/8,
    rowstride=rfbScreen->paddedWidthInBytes;
   char *in,*out;

   /* copy it, really */
   i = sraRgnGetReverseIterator(copyRegion,dx<0,dy<0);
   while(sraRgnIteratorNext(i,&rect)) {
     widthInBytes = (rect.x2-rect.x1)*bpp;
     out = rfbScreen->frameBuffer+rect.x1*bpp+rect.y1*rowstride;
     in = rfbScreen->frameBuffer+(rect.x1-dx)*bpp+(rect.y1-dy)*rowstride;
     if(dy<0)
       for(j=rect.y1;j<rect.y2;j++,out+=rowstride,in+=rowstride)
	 memmove(out,in,widthInBytes);
     else {
       out += rowstride*(rect.y2-rect.y1-1);
       in += rowstride*(rect.y2-rect.y1-1);
       for(j=rect.y2-1;j>=rect.y1;j--,out-=rowstride,in-=rowstride)
	 memmove(out,in,widthInBytes);
     }
   }
  
   rfbScheduleCopyRegion(rfbScreen,copyRegion,dx,dy);
}

void rfbDoCopyRect(rfbScreenInfoPtr rfbScreen,int x1,int y1,int x2,int y2,int dx,int dy)
{
  sraRegionPtr region = sraRgnCreateRect(x1,y1,x2,y2);
  rfbDoCopyRegion(rfbScreen,region,dx,dy);
}

void rfbScheduleCopyRect(rfbScreenInfoPtr rfbScreen,int x1,int y1,int x2,int y2,int dx,int dy)
{
  sraRegionPtr region = sraRgnCreateRect(x1,y1,x2,y2);
  rfbScheduleCopyRegion(rfbScreen,region,dx,dy);
}

void rfbMarkRegionAsModified(rfbScreenInfoPtr rfbScreen,sraRegionPtr modRegion)
{
   rfbClientIteratorPtr iterator;
   rfbClientPtr cl;

   iterator=rfbGetClientIterator(rfbScreen);
   while((cl=rfbClientIteratorNext(iterator))) {
     LOCK(cl->updateMutex);
     sraRgnOr(cl->modifiedRegion,modRegion);
     SIGNAL(cl->updateCond);
     UNLOCK(cl->updateMutex);
   }

   rfbReleaseClientIterator(iterator);
}

void rfbMarkRectAsModified(rfbScreenInfoPtr rfbScreen,int x1,int y1,int x2,int y2)
{
   sraRegionPtr region;
   int i;

   if(x1>x2) { i=x1; x1=x2; x2=i; }
   x2++;
   if(x1<0) { x1=0; if(x2==x1) x2++; }
   if(x2>=rfbScreen->width) { x2=rfbScreen->width-1; if(x1==x2) x1--; }
   
   if(y1>y2) { i=y1; y1=y2; y2=i; }
   y2++;
   if(y1<0) { y1=0; if(y2==y1) y2++; }
   if(y2>=rfbScreen->height) { y2=rfbScreen->height-1; if(y1==y2) y1--; }
   
   region = sraRgnCreateRect(x1,y1,x2,y2);
   rfbMarkRegionAsModified(rfbScreen,region);
   sraRgnDestroy(region);
}

int rfbDeferUpdateTime = 40; /* ms */

#ifdef HAVE_PTHREADS
static void *
clientOutput(void *data)
{
    rfbClientPtr cl = (rfbClientPtr)data;
    Bool haveUpdate;
    sraRegion* updateRegion;

    while (1) {
        haveUpdate = false;
        while (!haveUpdate) {
            if (cl->sock == -1) {
                /* Client has disconnected. */
                return NULL;
            }
	    LOCK(cl->updateMutex);
	    haveUpdate = FB_UPDATE_PENDING(cl);
	    if(!haveUpdate) {
		updateRegion = sraRgnCreateRgn(cl->modifiedRegion);
		haveUpdate = sraRgnAnd(updateRegion,cl->requestedRegion);
		sraRgnDestroy(updateRegion);
	    }
	    UNLOCK(cl->updateMutex);

            if (!haveUpdate) {
                WAIT(cl->updateCond, cl->updateMutex);
		UNLOCK(cl->updateMutex); /* we really needn't lock now. */
            }
        }
        
        /* OK, now, to save bandwidth, wait a little while for more
           updates to come along. */
        usleep(rfbDeferUpdateTime * 1000);

        /* Now, get the region we're going to update, and remove
           it from cl->modifiedRegion _before_ we send the update.
           That way, if anything that overlaps the region we're sending
           is updated, we'll be sure to do another update later. */
        LOCK(cl->updateMutex);
	updateRegion = sraRgnCreateRgn(cl->modifiedRegion);
        UNLOCK(cl->updateMutex);

        /* Now actually send the update. */
        rfbSendFramebufferUpdate(cl, updateRegion);

	sraRgnDestroy(updateRegion);
    }

    return NULL;
}

static void *
clientInput(void *data)
{
    rfbClientPtr cl = (rfbClientPtr)data;
    pthread_t output_thread;
    pthread_create(&output_thread, NULL, clientOutput, (void *)cl);

    while (1) {
        rfbProcessClientMessage(cl);
        if (cl->sock == -1) {
            /* Client has disconnected. */
            break;
        }
    }

    /* Get rid of the output thread. */
    LOCK(cl->updateMutex);
    SIGNAL(cl->updateCond);
    UNLOCK(cl->updateMutex);
    IF_PTHREADS(pthread_join(output_thread, NULL));

    rfbClientConnectionGone(cl);

    return NULL;
}

void*
listenerRun(void *data)
{
    rfbScreenInfoPtr rfbScreen=(rfbScreenInfoPtr)data;
    int client_fd;
    struct sockaddr_in peer;
    pthread_t client_thread;
    rfbClientPtr cl;
    int len;

    len = sizeof(peer);
    while ((client_fd = accept(rfbScreen->rfbListenSock, 
                               (struct sockaddr *)&peer, &len)) >= 0) {
        cl = rfbNewClient(rfbScreen,client_fd);

        pthread_create(&client_thread, NULL, clientInput, (void *)cl);
        len = sizeof(peer);
    }

    rfbLog("accept failed\n");
    exit(1);
}
#endif

static void
usage(void)
{
    fprintf(stderr, "-rfbport port          TCP port for RFB protocol\n");
    fprintf(stderr, "-rfbwait time          max time in ms to wait for RFB client\n");
    fprintf(stderr, "-rfbauth passwd-file   use authentication on RFB protocol\n"
                    "                       (use 'storepasswd' to create a password file)\n");
    fprintf(stderr, "-deferupdate time      time in ms to defer updates "
                                                             "(default 40)\n");
    fprintf(stderr, "-desktop name          VNC desktop name (default \"LibVNCServer\")\n");
    fprintf(stderr, "-alwaysshared          always treat new clients as shared\n");
    fprintf(stderr, "-nevershared           never treat new clients as shared\n");
    fprintf(stderr, "-dontdisconnect        don't disconnect existing clients when a "
                                                             "new non-shared\n"
                    "                       connection comes in (refuse new connection "
                                                                "instead)\n");
    exit(1);
}

static void 
processArguments(rfbScreenInfoPtr rfbScreen,int argc, char *argv[])
{
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-rfbport") == 0) { /* -rfbport port */
            if (i + 1 >= argc) usage();
	   rfbScreen->rfbPort = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-rfbwait") == 0) {  /* -rfbwait ms */
            if (i + 1 >= argc) usage();
	   rfbScreen->rfbMaxClientWait = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-rfbauth") == 0) {  /* -rfbauth passwd-file */
            if (i + 1 >= argc) usage();
            rfbScreen->rfbAuthPasswdFile = argv[++i];
        } else if (strcmp(argv[i], "-desktop") == 0) {  /* -desktop desktop-name */
            if (i + 1 >= argc) usage();
            rfbScreen->desktopName = argv[++i];
        } else if (strcmp(argv[i], "-alwaysshared") == 0) {
	    rfbScreen->rfbAlwaysShared = TRUE;
        } else if (strcmp(argv[i], "-nevershared") == 0) {
            rfbScreen->rfbNeverShared = TRUE;
        } else if (strcmp(argv[i], "-dontdisconnect") == 0) {
            rfbScreen->rfbDontDisconnect = TRUE;
        } else {
	  /* usage(); we no longer exit for unknown arguments */
        }
    }
}

void
defaultKbdAddEvent(Bool down, KeySym keySym, rfbClientPtr cl)
{
}

void
defaultPtrAddEvent(int buttonMask, int x, int y, rfbClientPtr cl)
{
   if(x!=cl->screen->cursorX || y!=cl->screen->cursorY) {
      if(cl->screen->cursorIsDrawn)
	rfbUndrawCursor(cl);
      LOCK(cl->screen->cursorMutex);
      if(!cl->screen->cursorIsDrawn) {
	  cl->screen->cursorX = x;
	  cl->screen->cursorY = y;
      }
      UNLOCK(cl->screen->cursorMutex);
   }
}

void defaultSetXCutText(char* text, int len, rfbClientPtr cl)
{
}

/* TODO: add a nice VNC or RFB cursor */

static rfbCursor myCursor = 
{
   width: 8, height: 7, xhot: 3, yhot: 3,
   source: "\000\102\044\030\044\102\000",
   mask:   "\347\347\176\074\176\347\347",
   /*
     width: 8, height: 7, xhot: 0, yhot: 0,
     source: "\000\074\176\146\176\074\000",
     mask:   "\176\377\377\377\377\377\176",
   */
   foreRed: 0, foreGreen: 0, foreBlue: 0,
   backRed: 0xffff, backGreen: 0xffff, backBlue: 0xffff,
   richSource: 0
};

rfbCursorPtr defaultGetCursorPtr(rfbClientPtr cl)
{
   return(cl->screen->cursor);
}

void doNothingWithClient(rfbClientPtr cl)
{
}

rfbScreenInfoPtr rfbGetScreen(int argc,char** argv,
 int width,int height,int bitsPerSample,int samplesPerPixel,
 int bytesPerPixel)
{
   rfbScreenInfoPtr rfbScreen=malloc(sizeof(rfbScreenInfo));
   rfbPixelFormat* format=&rfbScreen->rfbServerFormat;

   INIT_MUTEX(logMutex);

   if(width&3)
     fprintf(stderr,"WARNING: Width (%d) is not a multiple of 4. VncViewer has problems with that.\n",width);

   rfbScreen->rfbClientHead=0;
   rfbScreen->rfbPort=5900;
   rfbScreen->socketInitDone=FALSE;

   rfbScreen->inetdInitDone = FALSE;
   rfbScreen->inetdSock=-1;

   rfbScreen->udpSock=-1;
   rfbScreen->udpSockConnected=FALSE;
   rfbScreen->udpPort=0;
   rfbScreen->udpClient=0;

   rfbScreen->maxFd=0;
   rfbScreen->rfbListenSock=-1;

   rfbScreen->httpInitDone=FALSE;
   rfbScreen->httpPort=0;
   rfbScreen->httpDir=NULL;
   rfbScreen->httpListenSock=-1;
   rfbScreen->httpSock=-1;
   rfbScreen->httpFP=NULL;

   rfbScreen->desktopName = "LibVNCServer";
   rfbScreen->rfbAlwaysShared = FALSE;
   rfbScreen->rfbNeverShared = FALSE;
   rfbScreen->rfbDontDisconnect = FALSE;
   
   processArguments(rfbScreen,argc,argv);

   rfbScreen->width = width;
   rfbScreen->height = height;
   rfbScreen->bitsPerPixel = rfbScreen->depth = 8*bytesPerPixel;
   gethostname(rfbScreen->rfbThisHost, 255);
   rfbScreen->paddedWidthInBytes = width*bytesPerPixel;

   /* format */

   format->bitsPerPixel = rfbScreen->bitsPerPixel;
   format->depth = rfbScreen->depth;
   format->bigEndian = rfbEndianTest?FALSE:TRUE;
   format->trueColour = TRUE;
   rfbScreen->colourMap.count = 0;
   rfbScreen->colourMap.is16 = 0;
   rfbScreen->colourMap.data.bytes = NULL;

   if(bytesPerPixel == 8) {
     format->redMax = 7;
     format->greenMax = 7;
     format->blueMax = 3;
     format->redShift = 0;
     format->greenShift = 3;
     format->blueShift = 6;
   } else {
     format->redMax = (1 << bitsPerSample) - 1;
     format->greenMax = (1 << bitsPerSample) - 1;
     format->blueMax = (1 << bitsPerSample) - 1;
     if(rfbEndianTest) {
       format->redShift = 0;
       format->greenShift = bitsPerSample;
       format->blueShift = bitsPerSample * 2;
     } else {
       format->redShift = bitsPerSample*3;
       format->greenShift = bitsPerSample*2;
       format->blueShift = bitsPerSample;
     }
   }

   /* cursor */

   rfbScreen->cursorIsDrawn = FALSE;
   rfbScreen->dontSendFramebufferUpdate = FALSE;
   rfbScreen->cursorX=rfbScreen->cursorY=rfbScreen->underCursorBufferLen=0;
   rfbScreen->underCursorBuffer=NULL;
   rfbScreen->dontConvertRichCursorToXCursor = FALSE;
   rfbScreen->cursor = &myCursor;
   INIT_MUTEX(rfbScreen->cursorMutex);

   IF_PTHREADS(rfbScreen->backgroundLoop = FALSE);

   /* proc's and hook's */

   rfbScreen->kbdAddEvent = defaultKbdAddEvent;
   rfbScreen->kbdReleaseAllKeys = doNothingWithClient;
   rfbScreen->ptrAddEvent = defaultPtrAddEvent;
   rfbScreen->setXCutText = defaultSetXCutText;
   rfbScreen->getCursorPtr = defaultGetCursorPtr;
   rfbScreen->setTranslateFunction = rfbSetTranslateFunction;
   rfbScreen->newClientHook = doNothingWithClient;

   /* initialize client list and iterator mutex */
   rfbClientListInit(rfbScreen);

   return(rfbScreen);
}

void rfbScreenCleanup(rfbScreenInfoPtr rfbScreen)
{
  /* TODO */
  if(rfbScreen->frameBuffer)
    free(rfbScreen->frameBuffer);
  if(rfbScreen->colourMap.data.bytes)
    free(rfbScreen->colourMap.data.bytes);
  TINI_MUTEX(rfbScreen->cursorMutex);
  free(rfbScreen);
}

void rfbInitServer(rfbScreenInfoPtr rfbScreen)
{
  rfbInitSockets(rfbScreen);
  httpInitSockets(rfbScreen);
}

void
rfbProcessEvents(rfbScreenInfoPtr rfbScreen,long usec)
{
  rfbClientIteratorPtr i;
  rfbClientPtr cl;

  rfbCheckFds(rfbScreen,usec);
  httpCheckFds(rfbScreen);
#ifdef CORBA
  corbaCheckFds(rfbScreen);
#endif

  i = rfbGetClientIterator(rfbScreen);
  while((cl=rfbClientIteratorNext(i))) {
    if(cl->sock>=0 && FB_UPDATE_PENDING(cl))
      rfbSendFramebufferUpdate(cl,cl->modifiedRegion);
    if(cl->sock==-1)
      rfbClientConnectionGone(cl);
  }
  rfbReleaseClientIterator(i);
}

void rfbRunEventLoop(rfbScreenInfoPtr rfbScreen, long usec, Bool runInBackground)
{
  if(runInBackground) {
#ifdef HAVE_PTHREADS
       pthread_t listener_thread;

       rfbScreen->backgroundLoop = TRUE;

       pthread_create(&listener_thread, NULL, listenerRun, rfbScreen);
    return;
#else
    fprintf(stderr,"Can't run in background, because I don't have PThreads!\n");
    exit(-1);
#endif
  }

  while(1)
    rfbProcessEvents(rfbScreen,usec);
}
