/*
 *  OSXvnc Copyright (C) 2001 Dan McGuirk <mcguirk@incompleteness.net>.
 *  Original Xvnc code Copyright (C) 1999 AT&T Laboratories Cambridge.  
 *  All Rights Reserved.
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
#ifdef HAVE_PTHREADS
#include <pthread.h>
#endif
#include <unistd.h>
#include <signal.h>

#include "rfb.h"

#ifdef HAVE_PTHREADS
pthread_mutex_t logMutex;
#endif

/*
 * rfbLog prints a time-stamped message to the log file (stderr).
 */

void
rfbLog(char *format, ...)
{
    va_list args;
    char buf[256];
    time_t clock;

    IF_PTHREADS(pthread_mutex_lock(&logMutex));
    va_start(args, format);

    time(&clock);
    strftime(buf, 255, "%d/%m/%Y %T ", localtime(&clock));
    fprintf(stderr, buf);

    vfprintf(stderr, format, args);
    fflush(stderr);

    va_end(args);
    IF_PTHREADS(pthread_mutex_unlock(&logMutex));
}

void rfbLogPerror(char *str)
{
    rfbLog("%s: %s\n", str, strerror(errno));
}


void rfbMarkRegionAsModified(rfbScreenInfoPtr rfbScreen,RegionPtr modRegion)
{
   rfbClientIteratorPtr iterator;
   rfbClientPtr cl;
   iterator=rfbGetClientIterator(rfbScreen);
   while((cl=rfbClientIteratorNext(iterator))) {
     REGION_UNION(cl->screen,&cl->modifiedRegion,&cl->modifiedRegion,modRegion);
   }
  
   rfbReleaseClientIterator(iterator);
}

void rfbMarkRectAsModified(rfbScreenInfoPtr rfbScreen,int x1,int y1,int x2,int y2)
{
   BoxRec box;
   RegionRec region;
   box.x1=x1; box.y1=y1; box.x2=x2; box.y2=y2;
   REGION_INIT(cl->screen,&region,&box,0);
   rfbMarkRegionAsModified(rfbScreen,&region);
}

int rfbDeferUpdateTime = 40; /* ms */

#ifdef HAVE_PTHREADS
static void *
clientOutput(void *data)
{
    rfbClientPtr cl = (rfbClientPtr)data;
    Bool haveUpdate;
    RegionRec updateRegion;

    while (1) {
        haveUpdate = false;
        pthread_mutex_lock(&cl->updateMutex);
        while (!haveUpdate) {
            if (cl->sock == -1) {
                /* Client has disconnected. */
	        pthread_mutex_unlock(&cl->updateMutex);
                return NULL;
            }

            REGION_INIT(&hackScreen, &updateRegion, NullBox, 0);
            REGION_INTERSECT(&hackScreen, &updateRegion, 
                             &cl->modifiedRegion, &cl->requestedRegion);
            haveUpdate = REGION_NOTEMPTY(&hackScreen, &updateRegion);
            REGION_UNINIT(&hackScreen, &updateRegion);

            if (!haveUpdate) {
                pthread_cond_wait(&cl->updateCond, &cl->updateMutex);
            }
        }
        
        /* OK, now, to save bandwidth, wait a little while for more
           updates to come along. */
        pthread_mutex_unlock(&cl->updateMutex);
        usleep(rfbDeferUpdateTime * 1000);

        /* Now, get the region we're going to update, and remove
           it from cl->modifiedRegion _before_ we send the update.
           That way, if anything that overlaps the region we're sending
           is updated, we'll be sure to do another update later. */
        pthread_mutex_lock(&cl->updateMutex);
        REGION_INIT(&hackScreen, &updateRegion, NullBox, 0);
        REGION_INTERSECT(&hackScreen, &updateRegion, 
                         &cl->modifiedRegion, &cl->requestedRegion);
        REGION_SUBTRACT(&hackScreen, &cl->modifiedRegion,
                        &cl->modifiedRegion, &updateRegion);
        pthread_mutex_unlock(&cl->updateMutex);

        /* Now actually send the update. */
        rfbSendFramebufferUpdate(cl, updateRegion);

        REGION_UNINIT(&hackScreen, &updateRegion);
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
    pthread_mutex_lock(&cl->updateMutex);
    pthread_cond_signal(&cl->updateCond);
    pthread_mutex_unlock(&cl->updateMutex);
    pthread_join(output_thread, NULL);

    rfbClientConnectionGone(cl);

    return NULL;
}

void*
listenerRun(void *data)
{
    rfbScreenInfoPtr rfbScreen=(rfbScreenInfoPtr)data;
    int listen_fd, client_fd;
    struct sockaddr_in sin, peer;
    pthread_t client_thread;
    rfbClientPtr cl;
    int len, value;

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(rfbScreen->rfbPort ? rfbScreen->rfbPort : 5901);

    if ((listen_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        return NULL;
    }
    value = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, 
                   &value, sizeof(value)) < 0) {
        rfbLog("setsockopt SO_REUSEADDR failed\n");
    }
                                                                   
    if (bind(listen_fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        rfbLog("failed to bind socket\n");
        exit(1);
    }

    if (listen(listen_fd, 5) < 0) {
        rfbLog("listen failed\n");
        exit(1);
    }

    len = sizeof(peer);
    while ((client_fd = accept(listen_fd, 
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
        } else if (strcmp(argv[i], "-deferupdate") == 0) {      /* -deferupdate ms */
            if (i + 1 >= argc) usage();
            rfbScreen->rfbDeferUpdateTime = atoi(argv[++i]);
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
            usage();
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
}

void defaultSetXCutText(char* text, int len, rfbClientPtr cl)
{
}

void doNothingWithClient(rfbClientPtr cl)
{
}

rfbScreenInfoPtr rfbDefaultScreenInit(int argc,char** argv)
{
   int bitsPerSample,samplesPerPixel;
   
   rfbScreenInfoPtr rfbScreen=malloc(sizeof(rfbScreenInfo));
   rfbScreen->rfbPort=5900;
   rfbScreen->socketInitDone=FALSE;
   rfbScreen->inetdSock=-1;
   rfbScreen->udpSock=-1;
   rfbScreen->udpSockConnected=FALSE;
   rfbScreen->maxFd=0;
   rfbScreen->rfbListenSock=-1;
   rfbScreen->udpPort=0;
   rfbScreen->inetdInitDone = FALSE;
   rfbScreen->desktopName = "LibVNCServer";
   rfbScreen->rfbAlwaysShared = FALSE;
   rfbScreen->rfbNeverShared = FALSE;
   rfbScreen->rfbDontDisconnect = FALSE;
   
   processArguments(rfbScreen,argc,argv);

   rfbScreen->width = 640;
   rfbScreen->height = 480;
   rfbScreen->bitsPerPixel = rfbScreen->depth = 32;
   gethostname(rfbScreen->rfbThisHost, 255);
   rfbScreen->paddedWidthInBytes = 640*4;
   rfbScreen->rfbServerFormat.bitsPerPixel = rfbScreen->bitsPerPixel;
   rfbScreen->rfbServerFormat.depth = rfbScreen->depth;
   rfbScreen->rfbServerFormat.bigEndian = !(*(char *)&rfbEndianTest);
   rfbScreen->rfbServerFormat.trueColour = TRUE;

   bitsPerSample = 8;
   samplesPerPixel = 3;
   if (samplesPerPixel != 3) {
      rfbLog("screen format not supported.  exiting.\n");
      exit(1);
   }

   /* This works for 16 and 32-bit, but not for 8-bit.
    What should it be for 8-bit?  (Shouldn't 8-bit use a colormap?) */
   rfbScreen->rfbServerFormat.redMax = (1 << bitsPerSample) - 1;
   rfbScreen->rfbServerFormat.greenMax = (1 << bitsPerSample) - 1;
   rfbScreen->rfbServerFormat.blueMax = (1 << bitsPerSample) - 1;
   rfbScreen->rfbServerFormat.redShift = bitsPerSample * 2;
   rfbScreen->rfbServerFormat.greenShift = bitsPerSample;
   rfbScreen->rfbServerFormat.blueShift = 0;

   /* We want to use the X11 REGION_* macros without having an actual
    X11 ScreenPtr, so we do this.  Pretty ugly, but at least it lets us
    avoid hacking up regionstr.h, or changing every call to REGION_*
    (which actually I should probably do eventually). */
   rfbScreen->screen.RegionCreate = miRegionCreate;
   rfbScreen->screen.RegionInit = miRegionInit;
   rfbScreen->screen.RegionCopy = miRegionCopy;
   rfbScreen->screen.RegionDestroy = miRegionDestroy;
   rfbScreen->screen.RegionUninit = miRegionUninit;
   rfbScreen->screen.Intersect = miIntersect;
   rfbScreen->screen.Union = miUnion;
   rfbScreen->screen.Subtract = miSubtract;
   rfbScreen->screen.Inverse = miInverse;
   rfbScreen->screen.RegionReset = miRegionReset;
   rfbScreen->screen.TranslateRegion = miTranslateRegion;
   rfbScreen->screen.RectIn = miRectIn;
   rfbScreen->screen.PointInRegion = miPointInRegion;
   rfbScreen->screen.RegionNotEmpty = miRegionNotEmpty;
   rfbScreen->screen.RegionEmpty = miRegionEmpty;
   rfbScreen->screen.RegionExtents = miRegionExtents;
   rfbScreen->screen.RegionAppend = miRegionAppend;
   rfbScreen->screen.RegionValidate = miRegionValidate;

   rfbScreen->kbdAddEvent = defaultKbdAddEvent;
   rfbScreen->kbdReleaseAllKeys = doNothingWithClient;
   rfbScreen->ptrAddEvent = defaultPtrAddEvent;
   rfbScreen->setXCutText = defaultSetXCutText;
   rfbScreen->newClientHook = doNothingWithClient;

   return(rfbScreen);
}

void
processEvents(rfbScreenInfoPtr rfbScreen,long usec)
{
    rfbCheckFds(rfbScreen,usec);
    //httpCheckFds(rfbScreen);
#ifdef CORBA
    corbaCheckFds(rfbScreen);
#endif
    {
       rfbClientPtr cl,cl_next;
       cl=rfbScreen->rfbClientHead;
       while(cl) {
	 cl_next=cl->next;
	 if(cl->sock>=0 && FB_UPDATE_PENDING(cl)) {
	    rfbSendFramebufferUpdate(cl,cl->modifiedRegion);
	 }
	 cl=cl_next;
       }
    }
}

void runEventLoop(rfbScreenInfoPtr rfbScreen, long usec, Bool runInBackground)
{
  if(runInBackground) {
#ifdef HAVE_PTHREADS
       pthread_t listener_thread;

       rfbClientListInit(rfbScreen);
       //pthread_mutex_init(&logMutex, NULL);
       pthread_create(&listener_thread, NULL, listenerRun, rfbScreen);
    return;
#else
    fprintf(stderr,"Can't run in background, because I don't have PThreads!\n");
#endif
  }

  rfbInitSockets(rfbScreen);
  while(1)
    processEvents(rfbScreen,usec);
}
