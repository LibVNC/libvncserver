/*
 * rfbserver.c - deal with server-side of the RFB protocol.
 */

/*
 *  Copyright (C) 2002 RealVNC Ltd.
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

#include <string.h>
#include <rfb/rfb.h>
#include <rfb/rfbregion.h>

#ifdef LIBVNCSERVER_HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef WIN32
#define write(sock,buf,len) send(sock,buf,len,0)
#else
#ifdef LIBVNCSERVER_HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <pwd.h>
#ifdef LIBVNCSERVER_HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef LIBVNCSERVER_HAVE_NETINET_IN_H
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#endif
#endif

#ifdef CORBA
#include <vncserverctrl.h>
#endif

#ifdef DEBUGPROTO
#undef DEBUGPROTO
#define DEBUGPROTO(x) x
#else
#define DEBUGPROTO(x)
#endif

rfbClientPtr pointerClient = NULL;  /* Mutex for pointer events */

static void rfbProcessClientProtocolVersion(rfbClientPtr cl);
static void rfbProcessClientNormalMessage(rfbClientPtr cl);
static void rfbProcessClientInitMessage(rfbClientPtr cl);

#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
void rfbIncrClientRef(rfbClientPtr cl)
{
  LOCK(cl->refCountMutex);
  cl->refCount++;
  UNLOCK(cl->refCountMutex);
}

void rfbDecrClientRef(rfbClientPtr cl)
{
  LOCK(cl->refCountMutex);
  cl->refCount--;
  if(cl->refCount<=0) /* just to be sure also < 0 */
    TSIGNAL(cl->deleteCond);
  UNLOCK(cl->refCountMutex);
}
#else
void rfbIncrClientRef(rfbClientPtr cl) {}
void rfbDecrClientRef(rfbClientPtr cl) {}
#endif

#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
MUTEX(rfbClientListMutex);
#endif

struct rfbClientIterator {
  rfbClientPtr next;
  rfbScreenInfoPtr screen;
};

void
rfbClientListInit(rfbScreenInfoPtr rfbScreen)
{
    if(sizeof(rfbBool)!=1) {
        /* a sanity check */
        fprintf(stderr,"rfbBool's size is not 1 (%d)!\n",sizeof(rfbBool));
	/* we cannot continue, because rfbBool is supposed to be char everywhere */
	exit(1);
    }
    rfbScreen->rfbClientHead = NULL;
    INIT_MUTEX(rfbClientListMutex);
}

rfbClientIteratorPtr
rfbGetClientIterator(rfbScreenInfoPtr rfbScreen)
{
  rfbClientIteratorPtr i =
    (rfbClientIteratorPtr)malloc(sizeof(struct rfbClientIterator));
  i->next = 0;
  i->screen = rfbScreen;
  return i;
}

rfbClientPtr
rfbClientIteratorHead(rfbClientIteratorPtr i)
{
#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
  if(i->next != 0) {
    rfbDecrClientRef(i->next);
    rfbIncrClientRef(i->screen->rfbClientHead);
  }
#endif
  LOCK(rfbClientListMutex);
  i->next = i->screen->rfbClientHead;
  UNLOCK(rfbClientListMutex);
  return i->next;
}

rfbClientPtr
rfbClientIteratorNext(rfbClientIteratorPtr i)
{
  if(i->next == 0) {
    LOCK(rfbClientListMutex);
    i->next = i->screen->rfbClientHead;
    UNLOCK(rfbClientListMutex);
  } else {
    IF_PTHREADS(rfbClientPtr cl = i->next);
    i->next = i->next->next;
    IF_PTHREADS(rfbDecrClientRef(cl));
  }

#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
    while(i->next && i->next->sock<0)
      i->next = i->next->next;
    if(i->next)
      rfbIncrClientRef(i->next);
#endif

    return i->next;
}

void
rfbReleaseClientIterator(rfbClientIteratorPtr iterator)
{
  IF_PTHREADS(if(iterator->next) rfbDecrClientRef(iterator->next));
  free(iterator);
}


/*
 * rfbNewClientConnection is called from sockets.c when a new connection
 * comes in.
 */

void
rfbNewClientConnection(rfbScreen,sock)
    rfbScreenInfoPtr rfbScreen;
    int sock;
{
    rfbClientPtr cl;

    cl = rfbNewClient(rfbScreen,sock);
#ifdef CORBA
    if(cl!=NULL)
      newConnection(cl, (KEYBOARD_DEVICE|POINTER_DEVICE),1,1,1);
#endif
}


/*
 * rfbReverseConnection is called by the CORBA stuff to make an outward
 * connection to a "listening" RFB client.
 */

rfbClientPtr
rfbReverseConnection(rfbScreen,host, port)
    rfbScreenInfoPtr rfbScreen;
    char *host;
    int port;
{
    int sock;
    rfbClientPtr cl;

    if ((sock = rfbConnect(rfbScreen, host, port)) < 0)
        return (rfbClientPtr)NULL;

    cl = rfbNewClient(rfbScreen, sock);

    if (cl) {
        cl->reverseConnection = TRUE;
    }

    return cl;
}


/*
 * rfbNewClient is called when a new connection has been made by whatever
 * means.
 */

rfbClientPtr
rfbNewTCPOrUDPClient(rfbScreen,sock,isUDP)
    rfbScreenInfoPtr rfbScreen;
    int sock;
    rfbBool isUDP;
{
    rfbProtocolVersionMsg pv;
    rfbClientIteratorPtr iterator;
    rfbClientPtr cl,cl_;
    struct sockaddr_in addr;
    size_t addrlen = sizeof(struct sockaddr_in);

    cl = (rfbClientPtr)calloc(sizeof(rfbClientRec),1);

    cl->screen = rfbScreen;
    cl->sock = sock;
    cl->viewOnly = FALSE;

    rfbResetStats(cl);

    if(isUDP) {
      rfbLog(" accepted UDP client\n");
    } else {
      int one=1;

      getpeername(sock, (struct sockaddr *)&addr, &addrlen);
      cl->host = strdup(inet_ntoa(addr.sin_addr));

      rfbLog("  other clients:\n");
      iterator = rfbGetClientIterator(rfbScreen);
      while ((cl_ = rfbClientIteratorNext(iterator)) != NULL) {
        rfbLog("     %s\n",cl_->host);
      }
      rfbReleaseClientIterator(iterator);

#ifndef WIN32
      if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0) {
	rfbLogPerror("fcntl failed");
	close(sock);
	return NULL;
      }
#endif

      if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
		     (char *)&one, sizeof(one)) < 0) {
	rfbLogPerror("setsockopt failed");
	close(sock);
	return NULL;
      }

      FD_SET(sock,&(rfbScreen->allFds));
		rfbScreen->maxFd = max(sock,rfbScreen->maxFd);

      INIT_MUTEX(cl->outputMutex);
      INIT_MUTEX(cl->refCountMutex);
      INIT_COND(cl->deleteCond);

      cl->state = RFB_PROTOCOL_VERSION;

      cl->reverseConnection = FALSE;
      cl->readyForSetColourMapEntries = FALSE;
      cl->useCopyRect = FALSE;
      cl->preferredEncoding = rfbEncodingRaw;
      cl->correMaxWidth = 48;
      cl->correMaxHeight = 48;
#ifdef LIBVNCSERVER_HAVE_LIBZ
      cl->zrleData = 0;
#endif

      cl->copyRegion = sraRgnCreate();
      cl->copyDX = 0;
      cl->copyDY = 0;
   
      cl->modifiedRegion =
	sraRgnCreateRect(0,0,rfbScreen->width,rfbScreen->height);

      INIT_MUTEX(cl->updateMutex);
      INIT_COND(cl->updateCond);

      cl->requestedRegion = sraRgnCreate();

      cl->format = cl->screen->rfbServerFormat;
      cl->translateFn = rfbTranslateNone;
      cl->translateLookupTable = NULL;

      LOCK(rfbClientListMutex);

      IF_PTHREADS(cl->refCount = 0);
      cl->next = rfbScreen->rfbClientHead;
      cl->prev = NULL;
      if (rfbScreen->rfbClientHead)
        rfbScreen->rfbClientHead->prev = cl;

      rfbScreen->rfbClientHead = cl;
      UNLOCK(rfbClientListMutex);

#ifdef LIBVNCSERVER_HAVE_LIBJPEG
      cl->tightCompressLevel = TIGHT_DEFAULT_COMPRESSION;
      cl->tightQualityLevel = -1;
      {
	int i;
	for (i = 0; i < 4; i++)
          cl->zsActive[i] = FALSE;
      }
#endif

      cl->enableCursorShapeUpdates = FALSE;
      cl->enableCursorPosUpdates = FALSE;
      cl->useRichCursorEncoding = FALSE;
      cl->enableLastRectEncoding = FALSE;
      cl->useNewFBSize = FALSE;

#ifdef LIBVNCSERVER_HAVE_LIBZ
      cl->compStreamInited = FALSE;
      cl->compStream.total_in = 0;
      cl->compStream.total_out = 0;
      cl->compStream.zalloc = Z_NULL;
      cl->compStream.zfree = Z_NULL;
      cl->compStream.opaque = Z_NULL;

      cl->zlibCompressLevel = 5;
#endif

      cl->progressiveSliceY = 0;

      sprintf(pv,rfbProtocolVersionFormat,rfbProtocolMajorVersion,
	      rfbProtocolMinorVersion);

      if (WriteExact(cl, pv, sz_rfbProtocolVersionMsg) < 0) {
        rfbLogPerror("rfbNewClient: write");
        rfbCloseClient(cl);
	/* TODO: memory leak here (cl is never freed)
	 * can rfbClientConnectionGone called at this time?
	 * tim@tjansen.de
	 */
        return NULL;
      }
    }

    cl->clientData = NULL;
    cl->clientGoneHook = doNothingWithClient;
    switch (cl->screen->newClientHook(cl)) {
    case RFB_CLIENT_ON_HOLD:
	    cl->onHold = TRUE;
	    break;
    case RFB_CLIENT_ACCEPT:
	    cl->onHold = FALSE;
	    break;
    case RFB_CLIENT_REFUSE:
	    rfbCloseClient(cl);
	    rfbClientConnectionGone(cl);
	    cl = NULL;
	    break;
    }
    return cl;
}

rfbClientPtr
rfbNewClient(rfbScreen,sock)
    rfbScreenInfoPtr rfbScreen;
    int sock;
{
  return(rfbNewTCPOrUDPClient(rfbScreen,sock,FALSE));
}

rfbClientPtr
rfbNewUDPClient(rfbScreen)
     rfbScreenInfoPtr rfbScreen;
{
  return((rfbScreen->udpClient=
	  rfbNewTCPOrUDPClient(rfbScreen,rfbScreen->udpSock,TRUE)));
}

/*
 * rfbClientConnectionGone is called from sockets.c just after a connection
 * has gone away.
 */

void
rfbClientConnectionGone(cl)
     rfbClientPtr cl;
{
    int i;

    LOCK(rfbClientListMutex);

    if (cl->prev)
        cl->prev->next = cl->next;
    else
        cl->screen->rfbClientHead = cl->next;
    if (cl->next)
        cl->next->prev = cl->prev;

#ifdef LIBVNCSERVER_HAVE_LIBZ
    FreeZrleData(cl);
#endif

#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
    if(cl->screen->backgroundLoop != FALSE)
      do {
	LOCK(cl->refCountMutex);
	i=cl->refCount;
	UNLOCK(cl->refCountMutex);
	if(i>0)
	  WAIT(cl->deleteCond,cl->refCountMutex);
      } while(i>0);
#endif

    if(cl->sock>=0)
       FD_CLR(cl->sock,&(cl->screen->allFds));

    cl->clientGoneHook(cl);

    rfbLog("Client %s gone\n",cl->host);
    free(cl->host);

#ifdef LIBVNCSERVER_HAVE_LIBZ
    /* Release the compression state structures if any. */
    if ( cl->compStreamInited ) {
	deflateEnd( &(cl->compStream) );
    }

#ifdef LIBVNCSERVER_HAVE_LIBJPEG
    for (i = 0; i < 4; i++) {
	if (cl->zsActive[i])
	    deflateEnd(&cl->zsStruct[i]);
    }
#endif
#endif

    if (pointerClient == cl)
        pointerClient = NULL;

    sraRgnDestroy(cl->modifiedRegion);
    sraRgnDestroy(cl->requestedRegion);
    sraRgnDestroy(cl->copyRegion);

    UNLOCK(rfbClientListMutex);

    if (cl->translateLookupTable) free(cl->translateLookupTable);

    TINI_COND(cl->updateCond);
    TINI_MUTEX(cl->updateMutex);

    LOCK(cl->outputMutex);
    TINI_MUTEX(cl->outputMutex);

#ifdef CORBA
    destroyConnection(cl);
#endif

    rfbPrintStats(cl);

    free(cl);
}


/*
 * rfbProcessClientMessage is called when there is data to read from a client.
 */

void
rfbProcessClientMessage(cl)
     rfbClientPtr cl;
{
    switch (cl->state) {
    case RFB_PROTOCOL_VERSION:
        rfbProcessClientProtocolVersion(cl);
        return;
    case RFB_AUTHENTICATION:
        rfbAuthProcessClientMessage(cl);
        return;
    case RFB_INITIALISATION:
        rfbProcessClientInitMessage(cl);
        return;
    default:
        rfbProcessClientNormalMessage(cl);
        return;
    }
}


/*
 * rfbProcessClientProtocolVersion is called when the client sends its
 * protocol version.
 */

static void
rfbProcessClientProtocolVersion(cl)
    rfbClientPtr cl;
{
    rfbProtocolVersionMsg pv;
    int n, major_, minor_;
    char failureReason[256];

    if ((n = ReadExact(cl, pv, sz_rfbProtocolVersionMsg)) <= 0) {
        if (n == 0)
            rfbLog("rfbProcessClientProtocolVersion: client gone\n");
        else
            rfbLogPerror("rfbProcessClientProtocolVersion: read");
        rfbCloseClient(cl);
        return;
    }

    pv[sz_rfbProtocolVersionMsg] = 0;
    if (sscanf(pv,rfbProtocolVersionFormat,&major_,&minor_) != 2) {
        char name[1024]; 
	if(sscanf(pv,"RFB %03d.%03d %1024s\n",&major_,&minor_,name) != 3) {
	    rfbErr("rfbProcessClientProtocolVersion: not a valid RFB client\n");
	    rfbCloseClient(cl);
	    return;
	}
	free(cl->host);
	cl->host=strdup(name);
    }
    rfbLog("Protocol version %d.%d\n", major_, minor_);

    if (major_ != rfbProtocolMajorVersion) {
        /* Major version mismatch - send a ConnFailed message */

        rfbErr("Major version mismatch\n");
        sprintf(failureReason,
                "RFB protocol version mismatch - server %d.%d, client %d.%d",
                rfbProtocolMajorVersion,rfbProtocolMinorVersion,major_,minor_);
        rfbClientConnFailed(cl, failureReason);
        return;
    }

    if (minor_ != rfbProtocolMinorVersion) {
        /* Minor version mismatch - warn but try to continue */
        rfbLog("Ignoring minor version mismatch\n");
    }

    rfbAuthNewClient(cl);
}


/*
 * rfbClientConnFailed is called when a client connection has failed either
 * because it talks the wrong protocol or it has failed authentication.
 */

void
rfbClientConnFailed(cl, reason)
    rfbClientPtr cl;
    char *reason;
{
    char *buf;
    int len = strlen(reason);

    buf = (char *)malloc(8 + len);
    ((uint32_t *)buf)[0] = Swap32IfLE(rfbConnFailed);
    ((uint32_t *)buf)[1] = Swap32IfLE(len);
    memcpy(buf + 8, reason, len);

    if (WriteExact(cl, buf, 8 + len) < 0)
        rfbLogPerror("rfbClientConnFailed: write");
    free(buf);
    rfbCloseClient(cl);
}


/*
 * rfbProcessClientInitMessage is called when the client sends its
 * initialisation message.
 */

static void
rfbProcessClientInitMessage(cl)
    rfbClientPtr cl;
{
    rfbClientInitMsg ci;
    char buf[256];
    rfbServerInitMsg *si = (rfbServerInitMsg *)buf;
    int len, n;
    rfbClientIteratorPtr iterator;
    rfbClientPtr otherCl;

    if ((n = ReadExact(cl, (char *)&ci,sz_rfbClientInitMsg)) <= 0) {
        if (n == 0)
            rfbLog("rfbProcessClientInitMessage: client gone\n");
        else
            rfbLogPerror("rfbProcessClientInitMessage: read");
        rfbCloseClient(cl);
        return;
    }

    si->framebufferWidth = Swap16IfLE(cl->screen->width);
    si->framebufferHeight = Swap16IfLE(cl->screen->height);
    si->format = cl->screen->rfbServerFormat;
    si->format.redMax = Swap16IfLE(si->format.redMax);
    si->format.greenMax = Swap16IfLE(si->format.greenMax);
    si->format.blueMax = Swap16IfLE(si->format.blueMax);

    if (strlen(cl->screen->desktopName) > 128)      /* sanity check on desktop name len */
        ((char*)cl->screen->desktopName)[128] = 0;

    strcpy(buf + sz_rfbServerInitMsg, cl->screen->desktopName);
    len = strlen(buf + sz_rfbServerInitMsg);
    si->nameLength = Swap32IfLE(len);

    if (WriteExact(cl, buf, sz_rfbServerInitMsg + len) < 0) {
        rfbLogPerror("rfbProcessClientInitMessage: write");
        rfbCloseClient(cl);
        return;
    }

    cl->state = RFB_NORMAL;

    if (!cl->reverseConnection &&
                        (cl->screen->rfbNeverShared || (!cl->screen->rfbAlwaysShared && !ci.shared))) {

        if (cl->screen->rfbDontDisconnect) {
            iterator = rfbGetClientIterator(cl->screen);
            while ((otherCl = rfbClientIteratorNext(iterator)) != NULL) {
                if ((otherCl != cl) && (otherCl->state == RFB_NORMAL)) {
                    rfbLog("-dontdisconnect: Not shared & existing client\n");
                    rfbLog("  refusing new client %s\n", cl->host);
                    rfbCloseClient(cl);
                    rfbReleaseClientIterator(iterator);
                    return;
                }
            }
            rfbReleaseClientIterator(iterator);
        } else {
            iterator = rfbGetClientIterator(cl->screen);
            while ((otherCl = rfbClientIteratorNext(iterator)) != NULL) {
                if ((otherCl != cl) && (otherCl->state == RFB_NORMAL)) {
                    rfbLog("Not shared - closing connection to client %s\n",
                           otherCl->host);
                    rfbCloseClient(otherCl);
                }
            }
            rfbReleaseClientIterator(iterator);
        }
    }
}

static rfbBool rectSwapIfLEAndClip(uint16_t* x,uint16_t* y,uint16_t* w,uint16_t* h,
		rfbScreenInfoPtr screen)
{
	*x=Swap16IfLE(*x);
	*y=Swap16IfLE(*y);
	*w=Swap16IfLE(*w);
	*h=Swap16IfLE(*h);
	if(*w>screen->width-*x)
		*w=screen->width-*x;
	/* possible underflow */
	if(*w>screen->width-*x)
		return FALSE;
	if(*h>screen->height-*y)
		*h=screen->height-*y;
	if(*h>screen->height-*y)
		return FALSE;

	return TRUE;
}

/*
 * rfbProcessClientNormalMessage is called when the client has sent a normal
 * protocol message.
 */

static void
rfbProcessClientNormalMessage(cl)
    rfbClientPtr cl;
{
    int n=0;
    rfbClientToServerMsg msg;
    char *str;

    if ((n = ReadExact(cl, (char *)&msg, 1)) <= 0) {
        if (n != 0)
            rfbLogPerror("rfbProcessClientNormalMessage: read");
        rfbCloseClient(cl);
        return;
    }

    switch (msg.type) {

    case rfbSetPixelFormat:

        if ((n = ReadExact(cl, ((char *)&msg) + 1,
                           sz_rfbSetPixelFormatMsg - 1)) <= 0) {
            if (n != 0)
                rfbLogPerror("rfbProcessClientNormalMessage: read");
            rfbCloseClient(cl);
            return;
        }

        cl->format.bitsPerPixel = msg.spf.format.bitsPerPixel;
        cl->format.depth = msg.spf.format.depth;
        cl->format.bigEndian = (msg.spf.format.bigEndian ? TRUE : FALSE);
        cl->format.trueColour = (msg.spf.format.trueColour ? TRUE : FALSE);
        cl->format.redMax = Swap16IfLE(msg.spf.format.redMax);
        cl->format.greenMax = Swap16IfLE(msg.spf.format.greenMax);
        cl->format.blueMax = Swap16IfLE(msg.spf.format.blueMax);
        cl->format.redShift = msg.spf.format.redShift;
        cl->format.greenShift = msg.spf.format.greenShift;
        cl->format.blueShift = msg.spf.format.blueShift;

	cl->readyForSetColourMapEntries = TRUE;
        cl->screen->setTranslateFunction(cl);

        return;


    case rfbFixColourMapEntries:
        if ((n = ReadExact(cl, ((char *)&msg) + 1,
                           sz_rfbFixColourMapEntriesMsg - 1)) <= 0) {
            if (n != 0)
                rfbLogPerror("rfbProcessClientNormalMessage: read");
            rfbCloseClient(cl);
            return;
        }
        rfbLog("rfbProcessClientNormalMessage: %s",
                "FixColourMapEntries unsupported\n");
        rfbCloseClient(cl);
        return;


    case rfbSetEncodings:
    {
        int i;
        uint32_t enc;

        if ((n = ReadExact(cl, ((char *)&msg) + 1,
                           sz_rfbSetEncodingsMsg - 1)) <= 0) {
            if (n != 0)
                rfbLogPerror("rfbProcessClientNormalMessage: read");
            rfbCloseClient(cl);
            return;
        }

        msg.se.nEncodings = Swap16IfLE(msg.se.nEncodings);

        cl->preferredEncoding = -1;
	cl->useCopyRect = FALSE;
	cl->enableCursorShapeUpdates = FALSE;
	cl->enableCursorPosUpdates = FALSE;
	cl->enableLastRectEncoding = FALSE;
        cl->useNewFBSize = FALSE;

        for (i = 0; i < msg.se.nEncodings; i++) {
            if ((n = ReadExact(cl, (char *)&enc, 4)) <= 0) {
                if (n != 0)
                    rfbLogPerror("rfbProcessClientNormalMessage: read");
                rfbCloseClient(cl);
                return;
            }
            enc = Swap32IfLE(enc);

            switch (enc) {

            case rfbEncodingCopyRect:
		cl->useCopyRect = TRUE;
                break;
            case rfbEncodingRaw:
                if (cl->preferredEncoding == -1) {
                    cl->preferredEncoding = enc;
                    rfbLog("Using raw encoding for client %s\n",
                           cl->host);
                }
                break;
            case rfbEncodingRRE:
                if (cl->preferredEncoding == -1) {
                    cl->preferredEncoding = enc;
                    rfbLog("Using rre encoding for client %s\n",
                           cl->host);
                }
                break;
            case rfbEncodingCoRRE:
                if (cl->preferredEncoding == -1) {
                    cl->preferredEncoding = enc;
                    rfbLog("Using CoRRE encoding for client %s\n",
                           cl->host);
                }
                break;
            case rfbEncodingHextile:
                if (cl->preferredEncoding == -1) {
                    cl->preferredEncoding = enc;
                    rfbLog("Using hextile encoding for client %s\n",
                           cl->host);
                }
                break;
#ifdef LIBVNCSERVER_HAVE_LIBZ
	    case rfbEncodingZlib:
		if (cl->preferredEncoding == -1) {
		    cl->preferredEncoding = enc;
		    rfbLog("Using zlib encoding for client %s\n",
			   cl->host);
		}
              break;
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
	    case rfbEncodingTight:
		if (cl->preferredEncoding == -1) {
		    cl->preferredEncoding = enc;
		    rfbLog("Using tight encoding for client %s\n",
			   cl->host);
		}
		break;
#endif
#endif
	    case rfbEncodingXCursor:
		if(!cl->screen->dontConvertRichCursorToXCursor) {
		    rfbLog("Enabling X-style cursor updates for client %s\n",
			   cl->host);
		    cl->enableCursorShapeUpdates = TRUE;
		    cl->cursorWasChanged = TRUE;
		}
		break;
	    case rfbEncodingRichCursor:
	        rfbLog("Enabling full-color cursor updates for client %s\n",
		       cl->host);
	        cl->enableCursorShapeUpdates = TRUE;
	        cl->useRichCursorEncoding = TRUE;
	        cl->cursorWasChanged = TRUE;
	        break;
	    case rfbEncodingPointerPos:
		if (!cl->enableCursorPosUpdates) {
		    rfbLog("Enabling cursor position updates for client %s\n",
			   cl->host);
		    cl->enableCursorPosUpdates = TRUE;
		    cl->cursorWasMoved = TRUE;
		}
	        break;
	    case rfbEncodingLastRect:
		if (!cl->enableLastRectEncoding) {
		    rfbLog("Enabling LastRect protocol extension for client "
			   "%s\n", cl->host);
		    cl->enableLastRectEncoding = TRUE;
		}
		break;
	    case rfbEncodingNewFBSize:
		if (!cl->useNewFBSize) {
		    rfbLog("Enabling NewFBSize protocol extension for client "
			   "%s\n", cl->host);
		    cl->useNewFBSize = TRUE;
		}
		break;
#ifdef LIBVNCSERVER_BACKCHANNEL
	    case rfbEncodingBackChannel:
	        if (!cl->enableBackChannel) {
		    rfbLog("Enabling BackChannel protocol extension for "
			   "client %s\n", cl->host);
		    cl->enableBackChannel = TRUE;
		}
		break;
#endif
#ifdef LIBVNCSERVER_HAVE_LIBZ
           case rfbEncodingZRLE:
               if (cl->preferredEncoding == -1) {
                   cl->preferredEncoding = enc;
                   rfbLog("Using ZRLE encoding for client %s\n",
                          cl->host);
               }
               break;
#endif
            default:
#ifdef LIBVNCSERVER_HAVE_LIBZ
		if ( enc >= (uint32_t)rfbEncodingCompressLevel0 &&
		     enc <= (uint32_t)rfbEncodingCompressLevel9 ) {
		    cl->zlibCompressLevel = enc & 0x0F;
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
		    cl->tightCompressLevel = enc & 0x0F;
		    rfbLog("Using compression level %d for client %s\n",
			   cl->tightCompressLevel, cl->host);
		} else if ( enc >= (uint32_t)rfbEncodingQualityLevel0 &&
			    enc <= (uint32_t)rfbEncodingQualityLevel9 ) {
		    cl->tightQualityLevel = enc & 0x0F;
		    rfbLog("Using image quality level %d for client %s\n",
			   cl->tightQualityLevel, cl->host);
#endif
		} else
#endif
		 rfbLog("rfbProcessClientNormalMessage: ignoring unknown "
                       "encoding type %d\n", (int)enc);
            }
        }

        if (cl->preferredEncoding == -1) {
            cl->preferredEncoding = rfbEncodingRaw;
        }

	if (cl->enableCursorPosUpdates && !cl->enableCursorShapeUpdates) {
	  rfbLog("Disabling cursor position updates for client %s\n",
		 cl->host);
	  cl->enableCursorPosUpdates = FALSE;
	}

        return;
    }


    case rfbFramebufferUpdateRequest:
    {
        sraRegionPtr tmpRegion;

        if ((n = ReadExact(cl, ((char *)&msg) + 1,
                           sz_rfbFramebufferUpdateRequestMsg-1)) <= 0) {
            if (n != 0)
                rfbLogPerror("rfbProcessClientNormalMessage: read");
            rfbCloseClient(cl);
            return;
        }

	if(!rectSwapIfLEAndClip(&msg.fur.x,&msg.fur.y,&msg.fur.w,&msg.fur.h,
			cl->screen))
		return;

	tmpRegion =
	  sraRgnCreateRect(msg.fur.x,
			   msg.fur.y,
			   msg.fur.x+msg.fur.w,
			   msg.fur.y+msg.fur.h);

        LOCK(cl->updateMutex);
	sraRgnOr(cl->requestedRegion,tmpRegion);

	if (!cl->readyForSetColourMapEntries) {
	    /* client hasn't sent a SetPixelFormat so is using server's */
	    cl->readyForSetColourMapEntries = TRUE;
	    if (!cl->format.trueColour) {
		if (!rfbSetClientColourMap(cl, 0, 0)) {
		    sraRgnDestroy(tmpRegion);
		    UNLOCK(cl->updateMutex);
		    return;
		}
	    }
	}

       if (!msg.fur.incremental) {
	    sraRgnOr(cl->modifiedRegion,tmpRegion);
	    sraRgnSubtract(cl->copyRegion,tmpRegion);
       }
       TSIGNAL(cl->updateCond);
       UNLOCK(cl->updateMutex);

       sraRgnDestroy(tmpRegion);

       return;
    }

    case rfbKeyEvent:

	cl->rfbKeyEventsRcvd++;

	if ((n = ReadExact(cl, ((char *)&msg) + 1,
			   sz_rfbKeyEventMsg - 1)) <= 0) {
	    if (n != 0)
		rfbLogPerror("rfbProcessClientNormalMessage: read");
	    rfbCloseClient(cl);
	    return;
	}

	if(!cl->viewOnly) {
	    cl->screen->kbdAddEvent(msg.ke.down, (rfbKeySym)Swap32IfLE(msg.ke.key), cl);
	}

        return;


    case rfbPointerEvent:

	cl->rfbPointerEventsRcvd++;

	if ((n = ReadExact(cl, ((char *)&msg) + 1,
			   sz_rfbPointerEventMsg - 1)) <= 0) {
	    if (n != 0)
		rfbLogPerror("rfbProcessClientNormalMessage: read");
	    rfbCloseClient(cl);
	    return;
	}

	if (pointerClient && (pointerClient != cl))
	    return;

	if (msg.pe.buttonMask == 0)
	    pointerClient = NULL;
	else
	    pointerClient = cl;

	if(!cl->viewOnly) {
	    cl->screen->ptrAddEvent(msg.pe.buttonMask,
				    Swap16IfLE(msg.pe.x), Swap16IfLE(msg.pe.y), cl);
	}

        return;


    case rfbClientCutText:

	if ((n = ReadExact(cl, ((char *)&msg) + 1,
			   sz_rfbClientCutTextMsg - 1)) <= 0) {
	    if (n != 0)
		rfbLogPerror("rfbProcessClientNormalMessage: read");
	    rfbCloseClient(cl);
	    return;
	}

	msg.cct.length = Swap32IfLE(msg.cct.length);

	str = (char *)malloc(msg.cct.length);

	if ((n = ReadExact(cl, str, msg.cct.length)) <= 0) {
	    if (n != 0)
	        rfbLogPerror("rfbProcessClientNormalMessage: read");
	    free(str);
	    rfbCloseClient(cl);
	    return;
	}

	if(!cl->viewOnly) {
	    cl->screen->setXCutText(str, msg.cct.length, cl);
	}
	free(str);

        return;


    default:

        rfbLog("rfbProcessClientNormalMessage: unknown message type %d\n",
                msg.type);
        rfbLog(" ... closing connection\n");
        rfbCloseClient(cl);
        return;
    }
}



/*
 * rfbSendFramebufferUpdate - send the currently pending framebuffer update to
 * the RFB client.
 * givenUpdateRegion is not changed.
 */

rfbBool
rfbSendFramebufferUpdate(cl, givenUpdateRegion)
     rfbClientPtr cl;
     sraRegionPtr givenUpdateRegion;
{
    sraRectangleIterator* i;
    sraRect rect;
    int nUpdateRegionRects;
    rfbFramebufferUpdateMsg *fu = (rfbFramebufferUpdateMsg *)cl->updateBuf;
    sraRegionPtr updateRegion,updateCopyRegion,tmpRegion;
    int dx, dy;
    rfbBool sendCursorShape = FALSE;
    rfbBool sendCursorPos = FALSE;

    if(cl->screen->displayHook)
      cl->screen->displayHook(cl);

    /*
     * If framebuffer size was changed and the client supports NewFBSize
     * encoding, just send NewFBSize marker and return.
     */

    if (cl->useNewFBSize && cl->newFBSizePending) {
      LOCK(cl->updateMutex);
      cl->newFBSizePending = FALSE;
      UNLOCK(cl->updateMutex);
      cl->rfbFramebufferUpdateMessagesSent++;
      fu->type = rfbFramebufferUpdate;
      fu->nRects = Swap16IfLE(1);
      cl->ublen = sz_rfbFramebufferUpdateMsg;
      if (!rfbSendNewFBSize(cl, cl->screen->width, cl->screen->height)) {
        return FALSE;
      }
      return rfbSendUpdateBuf(cl);
    }
    
    /*
     * If this client understands cursor shape updates, cursor should be
     * removed from the framebuffer. Otherwise, make sure it's put up.
     */

    if (cl->enableCursorShapeUpdates) {
      if (cl->screen->cursorIsDrawn) {
	rfbUndrawCursor(cl->screen);
      }
      if (!cl->screen->cursorIsDrawn && cl->cursorWasChanged &&
	  cl->readyForSetColourMapEntries)
	  sendCursorShape = TRUE;
    } else {
      if (!cl->screen->cursorIsDrawn) {
	rfbDrawCursor(cl->screen);
      }
    }

    /*
     * Do we plan to send cursor position update?
     */

    if (cl->enableCursorPosUpdates && cl->cursorWasMoved)
      sendCursorPos = TRUE;

    LOCK(cl->updateMutex);

    /*
     * The modifiedRegion may overlap the destination copyRegion.  We remove
     * any overlapping bits from the copyRegion (since they'd only be
     * overwritten anyway).
     */
    
    sraRgnSubtract(cl->copyRegion,cl->modifiedRegion);

    /*
     * The client is interested in the region requestedRegion.  The region
     * which should be updated now is the intersection of requestedRegion
     * and the union of modifiedRegion and copyRegion.  If it's empty then
     * no update is needed.
     */

    updateRegion = sraRgnCreateRgn(givenUpdateRegion);
    if(cl->screen->progressiveSliceHeight>0) {
	    int height=cl->screen->progressiveSliceHeight,
	    	y=cl->progressiveSliceY;
	    sraRegionPtr bbox=sraRgnBBox(updateRegion);
	    sraRect rect;
	    if(sraRgnPopRect(bbox,&rect,0)) {
		sraRegionPtr slice;
		if(y<rect.y1 || y>=rect.y2)
		    y=rect.y1;
	    	slice=sraRgnCreateRect(0,y,cl->screen->width,y+height);
		sraRgnAnd(updateRegion,slice);
		sraRgnDestroy(slice);
	    }
	    sraRgnDestroy(bbox);
	    y+=height;
	    if(y>=cl->screen->height)
		    y=0;
	    cl->progressiveSliceY=y;
    }

    sraRgnOr(updateRegion,cl->copyRegion);
    if(!sraRgnAnd(updateRegion,cl->requestedRegion) &&
       !sendCursorShape && !sendCursorPos) {
      sraRgnDestroy(updateRegion);
      UNLOCK(cl->updateMutex);
      return TRUE;
    }

    /*
     * We assume that the client doesn't have any pixel data outside the
     * requestedRegion.  In other words, both the source and destination of a
     * copy must lie within requestedRegion.  So the region we can send as a
     * copy is the intersection of the copyRegion with both the requestedRegion
     * and the requestedRegion translated by the amount of the copy.  We set
     * updateCopyRegion to this.
     */

    updateCopyRegion = sraRgnCreateRgn(cl->copyRegion);
    sraRgnAnd(updateCopyRegion,cl->requestedRegion);
    tmpRegion = sraRgnCreateRgn(cl->requestedRegion);
    sraRgnOffset(tmpRegion,cl->copyDX,cl->copyDY);
    sraRgnAnd(updateCopyRegion,tmpRegion);
    sraRgnDestroy(tmpRegion);
    dx = cl->copyDX;
    dy = cl->copyDY;

    /*
     * Next we remove updateCopyRegion from updateRegion so that updateRegion
     * is the part of this update which is sent as ordinary pixel data (i.e not
     * a copy).
     */

    sraRgnSubtract(updateRegion,updateCopyRegion);

    /*
     * Finally we leave modifiedRegion to be the remainder (if any) of parts of
     * the screen which are modified but outside the requestedRegion.  We also
     * empty both the requestedRegion and the copyRegion - note that we never
     * carry over a copyRegion for a future update.
     */


     sraRgnOr(cl->modifiedRegion,cl->copyRegion);
     sraRgnSubtract(cl->modifiedRegion,updateRegion);
     sraRgnSubtract(cl->modifiedRegion,updateCopyRegion);

     sraRgnMakeEmpty(cl->requestedRegion);
     sraRgnMakeEmpty(cl->copyRegion);
     cl->copyDX = 0;
     cl->copyDY = 0;
   
     UNLOCK(cl->updateMutex);
   
   /*
     * Now send the update.
     */

    cl->rfbFramebufferUpdateMessagesSent++;

    if (cl->preferredEncoding == rfbEncodingCoRRE) {
        nUpdateRegionRects = 0;

        for(i = sraRgnGetIterator(updateRegion); sraRgnIteratorNext(i,&rect);){
            int x = rect.x1;
            int y = rect.y1;
            int w = rect.x2 - x;
            int h = rect.y2 - y;
            nUpdateRegionRects += (((w-1) / cl->correMaxWidth + 1)
                                     * ((h-1) / cl->correMaxHeight + 1));
        }
	sraRgnReleaseIterator(i);
#ifdef LIBVNCSERVER_HAVE_LIBZ
    } else if (cl->preferredEncoding == rfbEncodingZlib) {
	nUpdateRegionRects = 0;

        for(i = sraRgnGetIterator(updateRegion); sraRgnIteratorNext(i,&rect);){
            int x = rect.x1;
            int y = rect.y1;
            int w = rect.x2 - x;
            int h = rect.y2 - y;
	    nUpdateRegionRects += (((h-1) / (ZLIB_MAX_SIZE( w ) / w)) + 1);
	}
	sraRgnReleaseIterator(i);
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
    } else if (cl->preferredEncoding == rfbEncodingTight) {
	nUpdateRegionRects = 0;

        for(i = sraRgnGetIterator(updateRegion); sraRgnIteratorNext(i,&rect);){
            int x = rect.x1;
            int y = rect.y1;
            int w = rect.x2 - x;
            int h = rect.y2 - y;
	    int n = rfbNumCodedRectsTight(cl, x, y, w, h);
	    if (n == 0) {
		nUpdateRegionRects = 0xFFFF;
		break;
	    }
	    nUpdateRegionRects += n;
	}
	sraRgnReleaseIterator(i);
#endif
#endif
    } else {
        nUpdateRegionRects = sraRgnCountRects(updateRegion);
    }

    fu->type = rfbFramebufferUpdate;
    if (nUpdateRegionRects != 0xFFFF) {
	if(cl->screen->maxRectsPerUpdate>0
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
	   /* Tight encoding counts the rectangles differently */
	   && cl->preferredEncoding != rfbEncodingTight
#endif
	   && nUpdateRegionRects>cl->screen->maxRectsPerUpdate) {
	    sraRegion* newUpdateRegion = sraRgnBBox(updateRegion);
	    sraRgnDestroy(updateRegion);
	    updateRegion = newUpdateRegion;
	    nUpdateRegionRects = sraRgnCountRects(updateRegion);
	}
	fu->nRects = Swap16IfLE((uint16_t)(sraRgnCountRects(updateCopyRegion) +
					   nUpdateRegionRects +
					   !!sendCursorShape + !!sendCursorPos));
    } else {
	fu->nRects = 0xFFFF;
    }
    cl->ublen = sz_rfbFramebufferUpdateMsg;

   if (sendCursorShape) {
	cl->cursorWasChanged = FALSE;
	if (!rfbSendCursorShape(cl)) {
	    sraRgnDestroy(updateRegion);
	    return FALSE;
	}
    }
   
   if (sendCursorPos) {
	cl->cursorWasMoved = FALSE;
	if (!rfbSendCursorPos(cl)) {
	    sraRgnDestroy(updateRegion);
	    return FALSE;
	}
    }
   
    if (!sraRgnEmpty(updateCopyRegion)) {
	if (!rfbSendCopyRegion(cl,updateCopyRegion,dx,dy)) {
	    sraRgnDestroy(updateRegion);
	    sraRgnDestroy(updateCopyRegion);
	    return FALSE;
	}
    }

    sraRgnDestroy(updateCopyRegion);

    for(i = sraRgnGetIterator(updateRegion); sraRgnIteratorNext(i,&rect);){
        int x = rect.x1;
        int y = rect.y1;
        int w = rect.x2 - x;
        int h = rect.y2 - y;

        cl->rfbRawBytesEquivalent += (sz_rfbFramebufferUpdateRectHeader
                                      + w * (cl->format.bitsPerPixel / 8) * h);

        switch (cl->preferredEncoding) {
        case rfbEncodingRaw:
            if (!rfbSendRectEncodingRaw(cl, x, y, w, h)) {
	        sraRgnDestroy(updateRegion);
		sraRgnReleaseIterator(i);
                return FALSE;
            }
            break;
        case rfbEncodingRRE:
            if (!rfbSendRectEncodingRRE(cl, x, y, w, h)) {
	        sraRgnDestroy(updateRegion);
		sraRgnReleaseIterator(i);
                return FALSE;
            }
            break;
        case rfbEncodingCoRRE:
            if (!rfbSendRectEncodingCoRRE(cl, x, y, w, h)) {
	        sraRgnDestroy(updateRegion);
		sraRgnReleaseIterator(i);
                return FALSE;
            }
            break;
        case rfbEncodingHextile:
            if (!rfbSendRectEncodingHextile(cl, x, y, w, h)) {
	        sraRgnDestroy(updateRegion);
		sraRgnReleaseIterator(i);
                return FALSE;
            }
            break;
#ifdef LIBVNCSERVER_HAVE_LIBZ
	case rfbEncodingZlib:
	    if (!rfbSendRectEncodingZlib(cl, x, y, w, h)) {
	        sraRgnDestroy(updateRegion);
		sraRgnReleaseIterator(i);
		return FALSE;
	    }
	    break;
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
	case rfbEncodingTight:
	    if (!rfbSendRectEncodingTight(cl, x, y, w, h)) {
	        sraRgnDestroy(updateRegion);
		sraRgnReleaseIterator(i);
		return FALSE;
	    }
	    break;
#endif
#endif
#ifdef LIBVNCSERVER_HAVE_LIBZ
       case rfbEncodingZRLE:
           if (!rfbSendRectEncodingZRLE(cl, x, y, w, h)) {
	       sraRgnDestroy(updateRegion);
		sraRgnReleaseIterator(i);
               return FALSE;
           }
           break;
#endif
        }
    }
    sraRgnReleaseIterator(i);

    if ( nUpdateRegionRects == 0xFFFF &&
	 !rfbSendLastRectMarker(cl) ) {
        sraRgnDestroy(updateRegion);
	return FALSE;
    }

    if (!rfbSendUpdateBuf(cl)) {
        sraRgnDestroy(updateRegion);
        return FALSE;
    }

    sraRgnDestroy(updateRegion);
    return TRUE;
}


/*
 * Send the copy region as a string of CopyRect encoded rectangles.
 * The only slightly tricky thing is that we should send the messages in
 * the correct order so that an earlier CopyRect will not corrupt the source
 * of a later one.
 */

rfbBool
rfbSendCopyRegion(cl, reg, dx, dy)
    rfbClientPtr cl;
    sraRegionPtr reg;
    int dx, dy;
{
    int x, y, w, h;
    rfbFramebufferUpdateRectHeader rect;
    rfbCopyRect cr;
    sraRectangleIterator* i;
    sraRect rect1;

    /* printf("copyrect: "); sraRgnPrint(reg); putchar('\n');fflush(stdout); */
    i = sraRgnGetReverseIterator(reg,dx>0,dy>0);

    while(sraRgnIteratorNext(i,&rect1)) {
      x = rect1.x1;
      y = rect1.y1;
      w = rect1.x2 - x;
      h = rect1.y2 - y;

      rect.r.x = Swap16IfLE(x);
      rect.r.y = Swap16IfLE(y);
      rect.r.w = Swap16IfLE(w);
      rect.r.h = Swap16IfLE(h);
      rect.encoding = Swap32IfLE(rfbEncodingCopyRect);

      memcpy(&cl->updateBuf[cl->ublen], (char *)&rect,
	     sz_rfbFramebufferUpdateRectHeader);
      cl->ublen += sz_rfbFramebufferUpdateRectHeader;

      cr.srcX = Swap16IfLE(x - dx);
      cr.srcY = Swap16IfLE(y - dy);

      memcpy(&cl->updateBuf[cl->ublen], (char *)&cr, sz_rfbCopyRect);
      cl->ublen += sz_rfbCopyRect;

      cl->rfbRectanglesSent[rfbEncodingCopyRect]++;
      cl->rfbBytesSent[rfbEncodingCopyRect]
	+= sz_rfbFramebufferUpdateRectHeader + sz_rfbCopyRect;

    }

    return TRUE;
}

/*
 * Send a given rectangle in raw encoding (rfbEncodingRaw).
 */

rfbBool
rfbSendRectEncodingRaw(cl, x, y, w, h)
    rfbClientPtr cl;
    int x, y, w, h;
{
    rfbFramebufferUpdateRectHeader rect;
    int nlines;
    int bytesPerLine = w * (cl->format.bitsPerPixel / 8);
    char *fbptr = (cl->screen->frameBuffer + (cl->screen->paddedWidthInBytes * y)
                   + (x * (cl->screen->bitsPerPixel / 8)));

    /* Flush the buffer to guarantee correct alignment for translateFn(). */
    if (cl->ublen > 0) {
        if (!rfbSendUpdateBuf(cl))
            return FALSE;
    }

    rect.r.x = Swap16IfLE(x);
    rect.r.y = Swap16IfLE(y);
    rect.r.w = Swap16IfLE(w);
    rect.r.h = Swap16IfLE(h);
    rect.encoding = Swap32IfLE(rfbEncodingRaw);

    memcpy(&cl->updateBuf[cl->ublen], (char *)&rect,sz_rfbFramebufferUpdateRectHeader);
    cl->ublen += sz_rfbFramebufferUpdateRectHeader;

    cl->rfbRectanglesSent[rfbEncodingRaw]++;
    cl->rfbBytesSent[rfbEncodingRaw]
        += sz_rfbFramebufferUpdateRectHeader + bytesPerLine * h;

    nlines = (UPDATE_BUF_SIZE - cl->ublen) / bytesPerLine;

    while (TRUE) {
        if (nlines > h)
            nlines = h;

        (*cl->translateFn)(cl->translateLookupTable,
			   &(cl->screen->rfbServerFormat),
                           &cl->format, fbptr, &cl->updateBuf[cl->ublen],
                           cl->screen->paddedWidthInBytes, w, nlines);

        cl->ublen += nlines * bytesPerLine;
        h -= nlines;

        if (h == 0)     /* rect fitted in buffer, do next one */
            return TRUE;

        /* buffer full - flush partial rect and do another nlines */

        if (!rfbSendUpdateBuf(cl))
            return FALSE;

        fbptr += (cl->screen->paddedWidthInBytes * nlines);

        nlines = (UPDATE_BUF_SIZE - cl->ublen) / bytesPerLine;
        if (nlines == 0) {
            rfbErr("rfbSendRectEncodingRaw: send buffer too small for %d "
                   "bytes per line\n", bytesPerLine);
            rfbCloseClient(cl);
            return FALSE;
        }
    }
}



/*
 * Send an empty rectangle with encoding field set to value of
 * rfbEncodingLastRect to notify client that this is the last
 * rectangle in framebuffer update ("LastRect" extension of RFB
 * protocol).
 */

rfbBool
rfbSendLastRectMarker(cl)
    rfbClientPtr cl;
{
    rfbFramebufferUpdateRectHeader rect;

    if (cl->ublen + sz_rfbFramebufferUpdateRectHeader > UPDATE_BUF_SIZE) {
	if (!rfbSendUpdateBuf(cl))
	    return FALSE;
    }

    rect.encoding = Swap32IfLE(rfbEncodingLastRect);
    rect.r.x = 0;
    rect.r.y = 0;
    rect.r.w = 0;
    rect.r.h = 0;

    memcpy(&cl->updateBuf[cl->ublen], (char *)&rect,sz_rfbFramebufferUpdateRectHeader);
    cl->ublen += sz_rfbFramebufferUpdateRectHeader;

    cl->rfbLastRectMarkersSent++;
    cl->rfbLastRectBytesSent += sz_rfbFramebufferUpdateRectHeader;

    return TRUE;
}


/*
 * Send NewFBSize pseudo-rectangle. This tells the client to change
 * its framebuffer size.
 */

rfbBool
rfbSendNewFBSize(cl, w, h)
    rfbClientPtr cl;
    int w, h;
{
    rfbFramebufferUpdateRectHeader rect;

    if (cl->ublen + sz_rfbFramebufferUpdateRectHeader > UPDATE_BUF_SIZE) {
	if (!rfbSendUpdateBuf(cl))
	    return FALSE;
    }

    rect.encoding = Swap32IfLE(rfbEncodingNewFBSize);
    rect.r.x = 0;
    rect.r.y = 0;
    rect.r.w = Swap16IfLE(w);
    rect.r.h = Swap16IfLE(h);

    memcpy(&cl->updateBuf[cl->ublen], (char *)&rect,
           sz_rfbFramebufferUpdateRectHeader);
    cl->ublen += sz_rfbFramebufferUpdateRectHeader;

    cl->rfbLastRectMarkersSent++;
    cl->rfbLastRectBytesSent += sz_rfbFramebufferUpdateRectHeader;

    return TRUE;
}


/*
 * Send the contents of cl->updateBuf.  Returns 1 if successful, -1 if
 * not (errno should be set).
 */

rfbBool
rfbSendUpdateBuf(cl)
    rfbClientPtr cl;
{
    if(cl->sock<0)
      return FALSE;

    if (WriteExact(cl, cl->updateBuf, cl->ublen) < 0) {
        rfbLogPerror("rfbSendUpdateBuf: write");
        rfbCloseClient(cl);
        return FALSE;
    }

    cl->ublen = 0;
    return TRUE;
}

/*
 * rfbSendSetColourMapEntries sends a SetColourMapEntries message to the
 * client, using values from the currently installed colormap.
 */

rfbBool
rfbSendSetColourMapEntries(cl, firstColour, nColours)
    rfbClientPtr cl;
    int firstColour;
    int nColours;
{
    char buf[sz_rfbSetColourMapEntriesMsg + 256 * 3 * 2];
    rfbSetColourMapEntriesMsg *scme = (rfbSetColourMapEntriesMsg *)buf;
    uint16_t *rgb = (uint16_t *)(&buf[sz_rfbSetColourMapEntriesMsg]);
    rfbColourMap* cm = &cl->screen->colourMap;
    
    int i, len;

    scme->type = rfbSetColourMapEntries;

    scme->firstColour = Swap16IfLE(firstColour);
    scme->nColours = Swap16IfLE(nColours);

    len = sz_rfbSetColourMapEntriesMsg;

    for (i = 0; i < nColours; i++) {
      if(i<(int)cm->count) {
	if(cm->is16) {
	  rgb[i*3] = Swap16IfLE(cm->data.shorts[i*3]);
	  rgb[i*3+1] = Swap16IfLE(cm->data.shorts[i*3+1]);
	  rgb[i*3+2] = Swap16IfLE(cm->data.shorts[i*3+2]);
	} else {
	  rgb[i*3] = Swap16IfLE(cm->data.bytes[i*3]);
	  rgb[i*3+1] = Swap16IfLE(cm->data.bytes[i*3+1]);
	  rgb[i*3+2] = Swap16IfLE(cm->data.bytes[i*3+2]);
	}
      }
    }

    len += nColours * 3 * 2;

    if (WriteExact(cl, buf, len) < 0) {
	rfbLogPerror("rfbSendSetColourMapEntries: write");
	rfbCloseClient(cl);
	return FALSE;
    }
    return TRUE;
}

/*
 * rfbSendBell sends a Bell message to all the clients.
 */

void
rfbSendBell(rfbScreenInfoPtr rfbScreen)
{
    rfbClientIteratorPtr i;
    rfbClientPtr cl;
    rfbBellMsg b;

    i = rfbGetClientIterator(rfbScreen);
    while((cl=rfbClientIteratorNext(i))) {
	b.type = rfbBell;
	if (WriteExact(cl, (char *)&b, sz_rfbBellMsg) < 0) {
	    rfbLogPerror("rfbSendBell: write");
	    rfbCloseClient(cl);
	}
    }
    rfbReleaseClientIterator(i);
}


/*
 * rfbSendServerCutText sends a ServerCutText message to all the clients.
 */

void
rfbSendServerCutText(rfbScreenInfoPtr rfbScreen,char *str, int len)
{
    rfbClientPtr cl;
    rfbServerCutTextMsg sct;
    rfbClientIteratorPtr iterator;

    iterator = rfbGetClientIterator(rfbScreen);
    while ((cl = rfbClientIteratorNext(iterator)) != NULL) {
        sct.type = rfbServerCutText;
        sct.length = Swap32IfLE(len);
        if (WriteExact(cl, (char *)&sct,
                       sz_rfbServerCutTextMsg) < 0) {
            rfbLogPerror("rfbSendServerCutText: write");
            rfbCloseClient(cl);
            continue;
        }
        if (WriteExact(cl, str, len) < 0) {
            rfbLogPerror("rfbSendServerCutText: write");
            rfbCloseClient(cl);
        }
    }
    rfbReleaseClientIterator(iterator);
}

/*****************************************************************************
 *
 * UDP can be used for keyboard and pointer events when the underlying
 * network is highly reliable.  This is really here to support ORL's
 * videotile, whose TCP implementation doesn't like sending lots of small
 * packets (such as 100s of pen readings per second!).
 */

unsigned char ptrAcceleration = 50;

void
rfbNewUDPConnection(rfbScreen,sock)
    rfbScreenInfoPtr rfbScreen;
    int sock;
{
    if (write(sock, &ptrAcceleration, 1) < 0) {
	rfbLogPerror("rfbNewUDPConnection: write");
    }
}

/*
 * Because UDP is a message based service, we can't read the first byte and
 * then the rest of the packet separately like we do with TCP.  We will always
 * get a whole packet delivered in one go, so we ask read() for the maximum
 * number of bytes we can possibly get.
 */

void
rfbProcessUDPInput(rfbScreenInfoPtr rfbScreen)
{
    int n;
    rfbClientPtr cl=rfbScreen->udpClient;
    rfbClientToServerMsg msg;

    if((!cl) || cl->onHold)
      return;

    if ((n = read(rfbScreen->udpSock, (char *)&msg, sizeof(msg))) <= 0) {
	if (n < 0) {
	    rfbLogPerror("rfbProcessUDPInput: read");
	}
	rfbDisconnectUDPSock(rfbScreen);
	return;
    }

    switch (msg.type) {

    case rfbKeyEvent:
	if (n != sz_rfbKeyEventMsg) {
	    rfbErr("rfbProcessUDPInput: key event incorrect length\n");
	    rfbDisconnectUDPSock(rfbScreen);
	    return;
	}
	cl->screen->kbdAddEvent(msg.ke.down, (rfbKeySym)Swap32IfLE(msg.ke.key), cl);
	break;

    case rfbPointerEvent:
	if (n != sz_rfbPointerEventMsg) {
	    rfbErr("rfbProcessUDPInput: ptr event incorrect length\n");
	    rfbDisconnectUDPSock(rfbScreen);
	    return;
	}
	cl->screen->ptrAddEvent(msg.pe.buttonMask,
		    Swap16IfLE(msg.pe.x), Swap16IfLE(msg.pe.y), cl);
	break;

    default:
	rfbErr("rfbProcessUDPInput: unknown message type %d\n",
	       msg.type);
	rfbDisconnectUDPSock(rfbScreen);
    }
}

#ifdef LIBVNCSERVER_BACKCHANNEL
void rfbSendBackChannel(rfbScreenInfoPtr rfbScreen,char* str,int len)
{
    rfbClientPtr cl;
    rfbBackChannelMsg sct;
    rfbClientIteratorPtr iterator;

    iterator = rfbGetClientIterator(rfbScreen);
    while ((cl = rfbClientIteratorNext(iterator)) != NULL) {
        if (cl->enableBackChannel) {
	    sct.type = rfbBackChannel;
	    sct.length = Swap32IfLE(len);
	    if (WriteExact(cl, (char *)&sct,
			   sz_rfbBackChannelMsg) < 0) {
	        rfbLogPerror("rfbSendBackChannel: write");
		rfbCloseClient(cl);
		continue;
	    }
	    if (WriteExact(cl, str, len) < 0) {
	        rfbLogPerror("rfbSendBackChannel: write");
		rfbCloseClient(cl);
	    }
	}
    }
    rfbReleaseClientIterator(iterator);
}
#endif
