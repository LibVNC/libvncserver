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
#ifndef WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif
#include <signal.h>
#include <time.h>

#include "rfb.h"
#include "sraRegion.h"

MUTEX(logMutex);

/* we cannot compare to _LITTLE_ENDIAN, because some systems
   (as Solaris) assume little endian if _LITTLE_ENDIAN is
   defined, even if _BYTE_ORDER is not _LITTLE_ENDIAN */
char rfbEndianTest = (_BYTE_ORDER == 1234);

/*
 * rfbLog prints a time-stamped message to the log file (stderr).
 */

void
rfbLog(const char *format, ...)
{
    va_list args;
    char buf[256];
    time_t log_clock;

    LOCK(logMutex);
    va_start(args, format);

    time(&log_clock);
    strftime(buf, 255, "%d/%m/%Y %T ", localtime(&log_clock));
    fprintf(stderr,buf);

    vfprintf(stderr, format, args);
    fflush(stderr);

    va_end(args);
    UNLOCK(logMutex);
}

void rfbLogPerror(const char *str)
{
    rfbLog("%s: %s\n", str, strerror(errno));
}

void rfbScheduleCopyRegion(rfbScreenInfoPtr rfbScreen,sraRegionPtr copyRegion,int dx,int dy)
{  
   rfbClientIteratorPtr iterator;
   rfbClientPtr cl;

   rfbUndrawCursor(rfbScreen);
  
   iterator=rfbGetClientIterator(rfbScreen);
   while((cl=rfbClientIteratorNext(iterator))) {
     LOCK(cl->updateMutex);
     if(cl->useCopyRect) {
       sraRegionPtr modifiedRegionBackup;
       if(!sraRgnEmpty(cl->copyRegion)) {
	  if(cl->copyDX!=dx || cl->copyDY!=dy) {
	     /* if a copyRegion was not yet executed, treat it as a
	      * modifiedRegion. The idea: in this case it could be
	      * source of the new copyRect or modified anyway. */
	     sraRgnOr(cl->modifiedRegion,cl->copyRegion);
	     sraRgnMakeEmpty(cl->copyRegion);
	  } else {
	     /* we have to set the intersection of the source of the copy
	      * and the old copy to modified. */
	     modifiedRegionBackup=sraRgnCreateRgn(copyRegion);
	     sraRgnOffset(modifiedRegionBackup,-dx,-dy);
	     sraRgnAnd(modifiedRegionBackup,cl->copyRegion);
	     sraRgnOr(cl->modifiedRegion,modifiedRegionBackup);
	     sraRgnDestroy(modifiedRegionBackup);
	  }
       }
	  
       sraRgnOr(cl->copyRegion,copyRegion);
       cl->copyDX = dx;
       cl->copyDY = dy;

       /* if there were modified regions, which are now copied,
	* mark them as modified, because the source of these can be overlapped
	* either by new modified or now copied regions. */
       modifiedRegionBackup=sraRgnCreateRgn(cl->modifiedRegion);
       sraRgnOffset(modifiedRegionBackup,dx,dy);
       sraRgnAnd(modifiedRegionBackup,cl->copyRegion);
       sraRgnOr(cl->modifiedRegion,modifiedRegionBackup);
       sraRgnDestroy(modifiedRegionBackup);

#if 0
//TODO: is this needed? Or does it mess up deferring?
       /* while(!sraRgnEmpty(cl->copyRegion)) */ {
#ifdef HAVE_PTHREADS
	 if(!cl->screen->backgroundLoop)
#endif
	   {
	     sraRegionPtr updateRegion = sraRgnCreateRgn(cl->modifiedRegion);
	     sraRgnOr(updateRegion,cl->copyRegion);
	     UNLOCK(cl->updateMutex);
	     rfbSendFramebufferUpdate(cl,updateRegion);
	     sraRgnDestroy(updateRegion);
	     continue;
	   }
       }
#endif
     } else {
       sraRgnOr(cl->modifiedRegion,copyRegion);
     }
     TSIGNAL(cl->updateCond);
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

   rfbUndrawCursor(rfbScreen);
  
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
     TSIGNAL(cl->updateCond);
     UNLOCK(cl->updateMutex);
   }

   rfbReleaseClientIterator(iterator);
}

void rfbMarkRectAsModified(rfbScreenInfoPtr rfbScreen,int x1,int y1,int x2,int y2)
{
   sraRegionPtr region;
   int i;

   if(x1>x2) { i=x1; x1=x2; x2=i; }
   if(x1<0) x1=0;
   if(x2>=rfbScreen->width) x2=rfbScreen->width-1;
   if(x1==x2) return;
   
   if(y1>y2) { i=y1; y1=y2; y2=i; }
   if(y1<0) y1=0;
   if(y2>=rfbScreen->height) y2=rfbScreen->height-1;
   if(y1==y2) return;
   
   region = sraRgnCreateRect(x1,y1,x2,y2);
   rfbMarkRegionAsModified(rfbScreen,region);
   sraRgnDestroy(region);
}

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
        usleep(cl->screen->rfbDeferUpdateTime * 1000);

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
    TSIGNAL(cl->updateCond);
    UNLOCK(cl->updateMutex);
    IF_PTHREADS(pthread_join(output_thread, NULL));

    rfbClientConnectionGone(cl);

    return NULL;
}

static void*
listenerRun(void *data)
{
    rfbScreenInfoPtr rfbScreen=(rfbScreenInfoPtr)data;
    int client_fd;
    struct sockaddr_in peer;
    rfbClientPtr cl;
    int len;

    len = sizeof(peer);

    /* TODO: this thread wont die by restarting the server */
    while ((client_fd = accept(rfbScreen->rfbListenSock, 
                               (struct sockaddr*)&peer, &len)) >= 0) {
        cl = rfbNewClient(rfbScreen,client_fd);
        len = sizeof(peer);

	if (cl && !cl->onHold )
		rfbStartOnHoldClient(cl);
    }
}

void 
rfbStartOnHoldClient(rfbClientPtr cl)
{
    pthread_create(&cl->client_thread, NULL, clientInput, (void *)cl);
}

#else

void 
rfbStartOnHoldClient(rfbClientPtr cl)
{
	cl->onHold = FALSE;
}

#endif

void 
rfbRefuseOnHoldClient(rfbClientPtr cl)
{
    rfbCloseClient(cl);
    rfbClientConnectionGone(cl);
}

static void
defaultKbdAddEvent(Bool down, KeySym keySym, rfbClientPtr cl)
{
}

void
defaultPtrAddEvent(int buttonMask, int x, int y, rfbClientPtr cl)
{
   if(x!=cl->screen->cursorX || y!=cl->screen->cursorY) {
      if(cl->screen->cursorIsDrawn)
	rfbUndrawCursor(cl->screen);
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

#if defined(WIN32) || defined(sparc)
static rfbCursor myCursor = 
{
   "\000\102\044\030\044\102\000",
   "\347\347\176\074\176\347\347",
   8, 7, 3, 3,
   0, 0, 0,
   0xffff, 0xffff, 0xffff,
   0
};
#else
static rfbCursor myCursor = 
{
   source: "\000\102\044\030\044\102\000",
   mask:   "\347\347\176\074\176\347\347",
   width: 8, height: 7, xhot: 3, yhot: 3,
   /*
     width: 8, height: 7, xhot: 0, yhot: 0,
     source: "\000\074\176\146\176\074\000",
     mask:   "\176\377\377\377\377\377\176",
   */
   foreRed: 0, foreGreen: 0, foreBlue: 0,
   backRed: 0xffff, backGreen: 0xffff, backBlue: 0xffff,
   richSource: 0
};
#endif

rfbCursorPtr defaultGetCursorPtr(rfbClientPtr cl)
{
   return(cl->screen->cursor);
}

/* response is cl->authChallenge vncEncrypted with passwd */
Bool defaultPasswordCheck(rfbClientPtr cl,char* response,int len)
{
  int i;
  char *passwd=vncDecryptPasswdFromFile(cl->screen->rfbAuthPasswdData);

  if(!passwd) {
    rfbLog("Couldn't read password file: %s\n",cl->screen->rfbAuthPasswdData);
    return(FALSE);
  }

  vncEncryptBytes(cl->authChallenge, passwd);

  /* Lose the password from memory */
  for (i = strlen(passwd); i >= 0; i--) {
    passwd[i] = '\0';
  }

  free(passwd);

  if (memcmp(cl->authChallenge, response, len) != 0) {
    rfbLog("rfbAuthProcessClientMessage: authentication failed from %s\n",
	   cl->host);
    return(FALSE);
  }

  return(TRUE);
}

/* for this method, rfbAuthPasswdData is really a pointer to an array
   of char*'s, where the last pointer is 0. */
Bool rfbCheckPasswordByList(rfbClientPtr cl,char* response,int len)
{
  char **passwds;

  for(passwds=(char**)cl->screen->rfbAuthPasswdData;*passwds;passwds++) {
    vncEncryptBytes(cl->authChallenge, *passwds);

    if (memcmp(cl->authChallenge, response, len) == 0)
      return(TRUE);
  }

  rfbLog("rfbAuthProcessClientMessage: authentication failed from %s\n",
	 cl->host);
  return(FALSE);
}

void doNothingWithClient(rfbClientPtr cl)
{
}

enum rfbNewClientAction defaultNewClientHook(rfbClientPtr cl)
{
	return RFB_CLIENT_ACCEPT;
}

rfbScreenInfoPtr rfbGetScreen(int* argc,char** argv,
 int width,int height,int bitsPerSample,int samplesPerPixel,
 int bytesPerPixel)
{
   rfbScreenInfoPtr rfbScreen=malloc(sizeof(rfbScreenInfo));
   rfbPixelFormat* format=&rfbScreen->rfbServerFormat;

   INIT_MUTEX(logMutex);

   if(width&3)
     fprintf(stderr,"WARNING: Width (%d) is not a multiple of 4. VncViewer has problems with that.\n",width);

   rfbScreen->autoPort=FALSE;
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
   rfbScreen->rfbAuthPasswdData = 0;
   
   rfbScreen->width = width;
   rfbScreen->height = height;
   rfbScreen->bitsPerPixel = rfbScreen->depth = 8*bytesPerPixel;

   rfbScreen->passwordCheck = defaultPasswordCheck;

   rfbProcessArguments(rfbScreen,argc,argv);

#ifdef WIN32
   {
	   DWORD dummy=255;
	   GetComputerName(rfbScreen->rfbThisHost,&dummy);
   }
#else
   gethostname(rfbScreen->rfbThisHost, 255);
#endif

   rfbScreen->paddedWidthInBytes = width*bytesPerPixel;

   /* format */

   format->bitsPerPixel = rfbScreen->bitsPerPixel;
   format->depth = rfbScreen->depth;
   format->bigEndian = rfbEndianTest?FALSE:TRUE;
   format->trueColour = TRUE;
   rfbScreen->colourMap.count = 0;
   rfbScreen->colourMap.is16 = 0;
   rfbScreen->colourMap.data.bytes = NULL;

   if(bytesPerPixel == 1) {
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
       if(bytesPerPixel==3) {
	 format->redShift = bitsPerSample*2;
	 format->greenShift = bitsPerSample*1;
	 format->blueShift = 0;
       } else {
	 format->redShift = bitsPerSample*3;
	 format->greenShift = bitsPerSample*2;
	 format->blueShift = bitsPerSample;
       }
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

   rfbScreen->rfbDeferUpdateTime=5;

   /* proc's and hook's */

   rfbScreen->kbdAddEvent = defaultKbdAddEvent;
   rfbScreen->kbdReleaseAllKeys = doNothingWithClient;
   rfbScreen->ptrAddEvent = defaultPtrAddEvent;
   rfbScreen->setXCutText = defaultSetXCutText;
   rfbScreen->getCursorPtr = defaultGetCursorPtr;
   rfbScreen->setTranslateFunction = rfbSetTranslateFunction;
   rfbScreen->newClientHook = defaultNewClientHook;
   rfbScreen->displayHook = 0;

   /* initialize client list and iterator mutex */
   rfbClientListInit(rfbScreen);

   return(rfbScreen);
}

void rfbScreenCleanup(rfbScreenInfoPtr rfbScreen)
{
  rfbClientIteratorPtr i=rfbGetClientIterator(rfbScreen);
  rfbClientPtr cl,cl1=rfbClientIteratorNext(i);
  while(cl1) {
    cl=rfbClientIteratorNext(i);
    rfbClientConnectionGone(cl1);
    cl1=cl;
  }
  rfbReleaseClientIterator(i);
    
  /* TODO: hang up on all clients and free all reserved memory */
#define FREE_IF(x) if(rfbScreen->x) free(rfbScreen->x)
  FREE_IF(colourMap.data.bytes);
  FREE_IF(underCursorBuffer);
  TINI_MUTEX(rfbScreen->cursorMutex);
  free(rfbScreen);
}

void rfbInitServer(rfbScreenInfoPtr rfbScreen)
{
#ifdef WIN32
  WSADATA trash;
  int i=WSAStartup(MAKEWORD(2,2),&trash);
#endif
  rfbInitSockets(rfbScreen);
  httpInitSockets(rfbScreen);
}

#ifdef WIN32
#include <fcntl.h>
#include <conio.h>
#include <sys/timeb.h>

void gettimeofday(struct timeval* tv,char* dummy)
{
   SYSTEMTIME t;
   GetSystemTime(&t);
   tv->tv_sec=t.wHour*3600+t.wMinute*60+t.wSecond;
   tv->tv_usec=t.wMilliseconds*1000;
}
#endif

void
rfbProcessEvents(rfbScreenInfoPtr rfbScreen,long usec)
{
  rfbClientIteratorPtr i;
  rfbClientPtr cl,clPrev;
  struct timeval tv;

  if(usec<0)
    usec=rfbScreen->rfbDeferUpdateTime*1000;

  rfbCheckFds(rfbScreen,usec);
  httpCheckFds(rfbScreen);
#ifdef CORBA
  corbaCheckFds(rfbScreen);
#endif

  i = rfbGetClientIterator(rfbScreen);
  cl=rfbClientIteratorNext(i);
  while(cl) {
    if(cl->sock>=0 && (!cl->onHold) && FB_UPDATE_PENDING(cl)) {
      if(cl->screen->rfbDeferUpdateTime == 0) {
	  rfbSendFramebufferUpdate(cl,cl->modifiedRegion);
      } else if(cl->startDeferring.tv_usec == 0) {
	gettimeofday(&cl->startDeferring,NULL);
	if(cl->startDeferring.tv_usec == 0)
	  cl->startDeferring.tv_usec++;
      } else {
	gettimeofday(&tv,NULL);
	if(tv.tv_sec < cl->startDeferring.tv_sec /* at midnight */
	   || ((tv.tv_sec-cl->startDeferring.tv_sec)*1000
	       +(tv.tv_usec-cl->startDeferring.tv_usec)/1000)
	     > cl->screen->rfbDeferUpdateTime) {
	  cl->startDeferring.tv_usec = 0;
	  rfbSendFramebufferUpdate(cl,cl->modifiedRegion);
	}
      }
    }
    clPrev=cl;
    cl=rfbClientIteratorNext(i);
    if(clPrev->sock==-1)
      rfbClientConnectionGone(clPrev);
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

  if(usec<0)
    usec=rfbScreen->rfbDeferUpdateTime*1000;

  while(1)
    rfbProcessEvents(rfbScreen,usec);
}
