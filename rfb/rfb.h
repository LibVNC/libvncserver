#ifndef RFB_H
#define RFB_H

/*
 * rfb.h - header file for RFB DDX implementation.
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

#if(defined __cplusplus)
extern "C"
{
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rfb/rfbproto.h>

#ifdef LIBVNCSERVER_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef __MINGW32__
#include <winsock2.h>
#endif

#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
#include <pthread.h>
#if 0 /* debugging */
#define LOCK(mutex) (rfbLog("%s:%d LOCK(%s,0x%x)\n",__FILE__,__LINE__,#mutex,&(mutex)), pthread_mutex_lock(&(mutex)))
#define UNLOCK(mutex) (rfbLog("%s:%d UNLOCK(%s,0x%x)\n",__FILE__,__LINE__,#mutex,&(mutex)), pthread_mutex_unlock(&(mutex)))
#define MUTEX(mutex) pthread_mutex_t (mutex)
#define INIT_MUTEX(mutex) (rfbLog("%s:%d INIT_MUTEX(%s,0x%x)\n",__FILE__,__LINE__,#mutex,&(mutex)), pthread_mutex_init(&(mutex),NULL))
#define TINI_MUTEX(mutex) (rfbLog("%s:%d TINI_MUTEX(%s)\n",__FILE__,__LINE__,#mutex), pthread_mutex_destroy(&(mutex)))
#define TSIGNAL(cond) (rfbLog("%s:%d TSIGNAL(%s)\n",__FILE__,__LINE__,#cond), pthread_cond_signal(&(cond)))
#define WAIT(cond,mutex) (rfbLog("%s:%d WAIT(%s,%s)\n",__FILE__,__LINE__,#cond,#mutex), pthread_cond_wait(&(cond),&(mutex)))
#define COND(cond) pthread_cond_t (cond)
#define INIT_COND(cond) (rfbLog("%s:%d INIT_COND(%s)\n",__FILE__,__LINE__,#cond), pthread_cond_init(&(cond),NULL))
#define TINI_COND(cond) (rfbLog("%s:%d TINI_COND(%s)\n",__FILE__,__LINE__,#cond), pthread_cond_destroy(&(cond)))
#define IF_PTHREADS(x) x
#else
#define LOCK(mutex) pthread_mutex_lock(&(mutex));
#define UNLOCK(mutex) pthread_mutex_unlock(&(mutex));
#define MUTEX(mutex) pthread_mutex_t (mutex)
#define INIT_MUTEX(mutex) pthread_mutex_init(&(mutex),NULL)
#define TINI_MUTEX(mutex) pthread_mutex_destroy(&(mutex))
#define TSIGNAL(cond) pthread_cond_signal(&(cond))
#define WAIT(cond,mutex) pthread_cond_wait(&(cond),&(mutex))
#define COND(cond) pthread_cond_t (cond)
#define INIT_COND(cond) pthread_cond_init(&(cond),NULL)
#define TINI_COND(cond) pthread_cond_destroy(&(cond))
#define IF_PTHREADS(x) x
#endif
#else
#define LOCK(mutex)
#define UNLOCK(mutex)
#define MUTEX(mutex)
#define INIT_MUTEX(mutex)
#define TINI_MUTEX(mutex)
#define TSIGNAL(cond)
#define WAIT(cond,mutex) this_is_unsupported
#define COND(cond)
#define INIT_COND(cond)
#define TINI_COND(cond)
#define IF_PTHREADS(x)
#endif

/* end of stuff for autoconf */

/* if you use pthreads, but don't define LIBVNCSERVER_HAVE_LIBPTHREAD, the structs
   get all mixed up. So this gives a linker error reminding you to compile
   the library and your application (at least the parts including rfb.h)
   with the same support for pthreads. */
#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
#ifdef LIBVNCSERVER_HAVE_LIBZ
#define rfbInitServer rfbInitServerWithPthreadsAndZRLE
#else
#define rfbInitServer rfbInitServerWithPthreadsButWithoutZRLE
#endif
#else
#ifdef LIBVNCSERVER_HAVE_LIBZ
#define rfbInitServer rfbInitServerWithoutPthreadsButWithZRLE
#else
#define rfbInitServer rfbInitServerWithoutPthreadsAndZRLE
#endif
#endif

struct _rfbClientRec;
struct _rfbScreenInfo;
struct rfbCursor;

enum rfbNewClientAction {
	RFB_CLIENT_ACCEPT,
	RFB_CLIENT_ON_HOLD,
	RFB_CLIENT_REFUSE
};

typedef void (*rfbKbdAddEventProcPtr) (rfbBool down, rfbKeySym keySym, struct _rfbClientRec* cl);
typedef void (*rfbKbdReleaseAllKeysProcPtr) (struct _rfbClientRec* cl);
typedef void (*rfbPtrAddEventProcPtr) (int buttonMask, int x, int y, struct _rfbClientRec* cl);
typedef void (*rfbSetXCutTextProcPtr) (char* str,int len, struct _rfbClientRec* cl);
typedef struct rfbCursor* (*rfbGetCursorProcPtr) (struct _rfbClientRec* pScreen);
typedef rfbBool (*rfbSetTranslateFunctionProcPtr)(struct _rfbClientRec* cl);
typedef rfbBool (*rfbPasswordCheckProcPtr)(struct _rfbClientRec* cl,const char* encryptedPassWord,int len);
typedef enum rfbNewClientAction (*rfbNewClientHookPtr)(struct _rfbClientRec* cl);
typedef void (*rfbDisplayHookPtr)(struct _rfbClientRec* cl);

typedef struct {
  uint32_t count;
  rfbBool is16; /* is the data format short? */
  union {
    uint8_t* bytes;
    uint16_t* shorts;
  } data; /* there have to be count*3 entries */
} rfbColourMap;

/*
 * Per-screen (framebuffer) structure.  There can be as many as you wish,
 * each serving different clients. However, you have to call
 * rfbProcessEvents for each of these.
 */

typedef struct _rfbScreenInfo
{
    int width;
    int paddedWidthInBytes;
    int height;
    int depth;
    int bitsPerPixel;
    int sizeInBytes;

    rfbPixel blackPixel;
    rfbPixel whitePixel;

    /* some screen specific data can be put into a struct where screenData
     * points to. You need this if you have more than one screen at the
     * same time while using the same functions.
     */
    void* screenData;
  
    /* The following two members are used to minimise the amount of unnecessary
       drawing caused by cursor movement.  Whenever any drawing affects the
       part of the screen where the cursor is, the cursor is removed first and
       then the drawing is done (this is what the sprite routines test for).
       Afterwards, however, we do not replace the cursor, even when the cursor
       is logically being moved across the screen.  We only draw the cursor
       again just as we are about to send the client a framebuffer update.

       We need to be careful when removing and drawing the cursor because of
       their relationship with the normal drawing routines.  The drawing
       routines can invoke the cursor routines, but also the cursor routines
       themselves end up invoking drawing routines.

       Removing the cursor (rfbUndrawCursor) is eventually achieved by
       doing a CopyArea from a pixmap to the screen, where the pixmap contains
       the saved contents of the screen under the cursor.  Before doing this,
       however, we set cursorIsDrawn to FALSE.  Then, when CopyArea is called,
       it sees that cursorIsDrawn is FALSE and so doesn't feel the need to
       (recursively!) remove the cursor before doing it.

       Putting up the cursor (rfbDrawCursor) involves a call to
       PushPixels.  While this is happening, cursorIsDrawn must be FALSE so
       that PushPixels doesn't think it has to remove the cursor first.
       Obviously cursorIsDrawn is set to TRUE afterwards.

       Another problem we face is that drawing routines sometimes cause a
       framebuffer update to be sent to the RFB client.  When the RFB client is
       already waiting for a framebuffer update and some drawing to the
       framebuffer then happens, the drawing routine sees that the client is
       ready, so it calls rfbSendFramebufferUpdate.  If the cursor is not drawn
       at this stage, it must be put up, and so rfbSpriteRestoreCursor is
       called.  However, if the original drawing routine was actually called
       from within rfbSpriteRestoreCursor or rfbSpriteRemoveCursor we don't
       want this to happen.  So both the cursor routines set
       dontSendFramebufferUpdate to TRUE, and all the drawing routines check
       this before calling rfbSendFramebufferUpdate. */

    rfbBool cursorIsDrawn;		    /* TRUE if the cursor is currently drawn */
    rfbBool dontSendFramebufferUpdate; /* TRUE while removing or drawing the
				       cursor */
   
    /* additions by libvncserver */

    rfbPixelFormat serverFormat;
    rfbColourMap colourMap; /* set this if rfbServerFormat.trueColour==FALSE */
    const char* desktopName;
    char thisHost[255];

    rfbBool autoPort;
    int port;
    SOCKET listenSock;
    int maxSock;
    int maxFd;
    struct fd_set allFds;

    rfbBool socketInitDone;
    SOCKET inetdSock;
    rfbBool inetdInitDone;

    int udpPort;
    SOCKET udpSock;
    struct _rfbClientRec* udpClient;
    rfbBool udpSockConnected;
    struct sockaddr_in udpRemoteAddr;

    int maxClientWait;

    /* http stuff */
    rfbBool httpInitDone;
    rfbBool httpEnableProxyConnect;
    int httpPort;
    char* httpDir;
    SOCKET httpListenSock;
    SOCKET httpSock;

    rfbPasswordCheckProcPtr passwordCheck;
    void* authPasswdData;
    /* If rfbAuthPasswdData is given a list, this is the first
       view only password. */
    int authPasswdFirstViewOnly;

    /* send only this many rectangles in one update */
    int maxRectsPerUpdate;
    /* this is the amount of milliseconds to wait at least before sending
     * an update. */
    int deferUpdateTime;
#ifdef TODELETE
    char* screen;
#endif
    rfbBool alwaysShared;
    rfbBool neverShared;
    rfbBool dontDisconnect;
    struct _rfbClientRec* clientHead;

    /* cursor */
    int cursorX, cursorY,oldCursorX,oldCursorY,underCursorBufferLen;
    char* underCursorBuffer;
    rfbBool dontConvertRichCursorToXCursor;
    struct rfbCursor* cursor;

    /* the frameBufferhas to be supplied by the serving process.
     * The buffer will not be freed by 
     */
    char* frameBuffer;
    rfbKbdAddEventProcPtr kbdAddEvent;
    rfbKbdReleaseAllKeysProcPtr kbdReleaseAllKeys;
    rfbPtrAddEventProcPtr ptrAddEvent;
    rfbSetXCutTextProcPtr setXCutText;
    rfbGetCursorProcPtr getCursorPtr;
    rfbSetTranslateFunctionProcPtr setTranslateFunction;
  
    /* newClientHook is called just after a new client is created */
    rfbNewClientHookPtr newClientHook;
    /* displayHook is called just before a frame buffer update */
    rfbDisplayHookPtr displayHook;

#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
    MUTEX(cursorMutex);
    rfbBool backgroundLoop;
#endif

    /* if TRUE, an ignoring signal handler is installed for SIGPIPE */
    rfbBool ignoreSIGPIPE;

    /* if not zero, only a slice of this height is processed every time
     * an update should be sent. This should make working on a slow
     * link more interactive. */
    int progressiveSliceHeight;
} rfbScreenInfo, *rfbScreenInfoPtr;


/*
 * rfbTranslateFnType is the type of translation functions.
 */

typedef void (*rfbTranslateFnType)(char *table, rfbPixelFormat *in,
                                   rfbPixelFormat *out,
                                   char *iptr, char *optr,
                                   int bytesBetweenInputLines,
                                   int width, int height);


/* region stuff */

struct sraRegion;
typedef struct sraRegion* sraRegionPtr;

/*
 * Per-client structure.
 */

typedef void (*ClientGoneHookPtr)(struct _rfbClientRec* cl);

typedef struct _rfbClientRec {
  
    /* back pointer to the screen */
    rfbScreenInfoPtr screen;
  
    /* private data. You should put any application client specific data
     * into a struct and let clientData point to it. Don't forget to
     * free the struct via clientGoneHook!
     *
     * This is useful if the IO functions have to behave client specific.
     */
    void* clientData;
    ClientGoneHookPtr clientGoneHook;

    SOCKET sock;
    char *host;

#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
    pthread_t client_thread;
#endif
                                /* Possible client states: */
    enum {
        RFB_PROTOCOL_VERSION,   /* establishing protocol version */
        RFB_AUTHENTICATION,     /* authenticating */
        RFB_INITIALISATION,     /* sending initialisation messages */
        RFB_NORMAL              /* normal protocol messages */
    } state;

    rfbBool reverseConnection;
    rfbBool onHold;
    rfbBool readyForSetColourMapEntries;
    rfbBool useCopyRect;
    int preferredEncoding;
    int correMaxWidth, correMaxHeight;

    rfbBool viewOnly;

    /* The following member is only used during VNC authentication */
    uint8_t authChallenge[CHALLENGESIZE];

    /* The following members represent the update needed to get the client's
       framebuffer from its present state to the current state of our
       framebuffer.

       If the client does not accept CopyRect encoding then the update is
       simply represented as the region of the screen which has been modified
       (modifiedRegion).

       If the client does accept CopyRect encoding, then the update consists of
       two parts.  First we have a single copy from one region of the screen to
       another (the destination of the copy is copyRegion), and second we have
       the region of the screen which has been modified in some other way
       (modifiedRegion).

       Although the copy is of a single region, this region may have many
       rectangles.  When sending an update, the copyRegion is always sent
       before the modifiedRegion.  This is because the modifiedRegion may
       overlap parts of the screen which are in the source of the copy.

       In fact during normal processing, the modifiedRegion may even overlap
       the destination copyRegion.  Just before an update is sent we remove
       from the copyRegion anything in the modifiedRegion. */

    sraRegionPtr copyRegion;	/* the destination region of the copy */
    int copyDX, copyDY;		/* the translation by which the copy happens */

    sraRegionPtr modifiedRegion;

    /* As part of the FramebufferUpdateRequest, a client can express interest
       in a subrectangle of the whole framebuffer.  This is stored in the
       requestedRegion member.  In the normal case this is the whole
       framebuffer if the client is ready, empty if it's not. */

    sraRegionPtr requestedRegion;

    /* The following member represents the state of the "deferred update" timer
       - when the framebuffer is modified and the client is ready, in most
       cases it is more efficient to defer sending the update by a few
       milliseconds so that several changes to the framebuffer can be combined
       into a single update. */

      struct timeval startDeferring;

    /* translateFn points to the translation function which is used to copy
       and translate a rectangle from the framebuffer to an output buffer. */

    rfbTranslateFnType translateFn;
    char *translateLookupTable;
    rfbPixelFormat format;

    /*
     * UPDATE_BUF_SIZE must be big enough to send at least one whole line of the
     * framebuffer.  So for a max screen width of say 2K with 32-bit pixels this
     * means 8K minimum.
     */

#define UPDATE_BUF_SIZE 30000

    char updateBuf[UPDATE_BUF_SIZE];
    int ublen;

    /* statistics */

    int bytesSent[MAX_ENCODINGS];
    int rectanglesSent[MAX_ENCODINGS];
    int lastRectMarkersSent;
    int lastRectBytesSent;
    int cursorShapeBytesSent;
    int cursorShapeUpdatesSent;
    int cursorPosBytesSent;
    int cursorPosUpdatesSent;
    int framebufferUpdateMessagesSent;
    int rawBytesEquivalent;
    int keyEventsRcvd;
    int pointerEventsRcvd;

#ifdef LIBVNCSERVER_HAVE_LIBZ
    /* zlib encoding -- necessary compression state info per client */

    struct z_stream_s compStream;
    rfbBool compStreamInited;
    uint32_t zlibCompressLevel;

#ifdef LIBVNCSERVER_HAVE_LIBJPEG
    /* tight encoding -- preserve zlib streams' state for each client */
    z_stream zsStruct[4];
    rfbBool zsActive[4];
    int zsLevel[4];
    int tightCompressLevel;
    int tightQualityLevel;
#endif
#endif

    rfbBool enableLastRectEncoding;   /* client supports LastRect encoding */
    rfbBool enableCursorShapeUpdates; /* client supports cursor shape updates */
    rfbBool enableCursorPosUpdates;   /* client supports cursor position updates */
    rfbBool useRichCursorEncoding;    /* rfbEncodingRichCursor is preferred */
    rfbBool cursorWasChanged;         /* cursor shape update should be sent */
    rfbBool cursorWasMoved;           /* cursor position update should be sent */

    rfbBool useNewFBSize;             /* client supports NewFBSize encoding */
    rfbBool newFBSizePending;         /* framebuffer size was changed */

#ifdef LIBVNCSERVER_BACKCHANNEL
    rfbBool enableBackChannel;        /* custom channel for special clients */
#endif

    struct _rfbClientRec *prev;
    struct _rfbClientRec *next;

#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
    /* whenever a client is referenced, the refCount has to be incremented
       and afterwards decremented, so that the client is not cleaned up
       while being referenced.
       Use the functions rfbIncrClientRef(cl) and rfbDecrClientRef(cl);
    */
    int refCount;
    MUTEX(refCountMutex);
    COND(deleteCond);

    MUTEX(outputMutex);
    MUTEX(updateMutex);
    COND(updateCond);
#endif

#ifdef LIBVNCSERVER_HAVE_LIBZ
    void* zrleData;
#endif

    /* if progressive updating is on, this variable holds the current
     * y coordinate of the progressive slice. */
    int progressiveSliceY;
} rfbClientRec, *rfbClientPtr;

/*
 * This macro is used to test whether there is a framebuffer update needing to
 * be sent to the client.
 */

#define FB_UPDATE_PENDING(cl)                                              \
     ((!(cl)->enableCursorShapeUpdates && !(cl)->screen->cursorIsDrawn) || \
     ((cl)->enableCursorShapeUpdates && (cl)->cursorWasChanged) ||         \
     ((cl)->useNewFBSize && (cl)->newFBSizePending) ||                     \
     ((cl)->enableCursorPosUpdates && (cl)->cursorWasMoved) ||             \
     !sraRgnEmpty((cl)->copyRegion) || !sraRgnEmpty((cl)->modifiedRegion))

/*
 * Macros for endian swapping.
 */

#define Swap16(s) ((((s) & 0xff) << 8) | (((s) >> 8) & 0xff))

#define Swap24(l) ((((l) & 0xff) << 16) | (((l) >> 16) & 0xff) | \
                   (((l) & 0x00ff00)))

#define Swap32(l) (((l) >> 24) | \
                   (((l) & 0x00ff0000) >> 8)  | \
                   (((l) & 0x0000ff00) << 8)  | \
                   ((l) << 24))


extern char rfbEndianTest;

#define Swap16IfLE(s) (rfbEndianTest ? Swap16(s) : (s))
#define Swap24IfLE(l) (rfbEndianTest ? Swap24(l) : (l))
#define Swap32IfLE(l) (rfbEndianTest ? Swap32(l) : (l))

/* sockets.c */

extern int rfbMaxClientWait;

extern void rfbInitSockets(rfbScreenInfoPtr rfbScreen);
extern void rfbDisconnectUDPSock(rfbScreenInfoPtr rfbScreen);
extern void rfbCloseClient(rfbClientPtr cl);
extern int rfbReadExact(rfbClientPtr cl, char *buf, int len);
extern int rfbReadExactTimeout(rfbClientPtr cl, char *buf, int len,int timeout);
extern int rfbWriteExact(rfbClientPtr cl, const char *buf, int len);
extern void rfbCheckFds(rfbScreenInfoPtr rfbScreen,long usec);
extern int rfbConnect(rfbScreenInfoPtr rfbScreen, char* host, int port);
extern int rfbConnectToTcpAddr(char* host, int port);
extern int rfbListenOnTCPPort(int port);
extern int rfbListenOnUDPPort(int port);

/* rfbserver.c */

/* Routines to iterate over the client list in a thread-safe way.
   Only a single iterator can be in use at a time process-wide. */
typedef struct rfbClientIterator *rfbClientIteratorPtr;

extern void rfbClientListInit(rfbScreenInfoPtr rfbScreen);
extern rfbClientIteratorPtr rfbGetClientIterator(rfbScreenInfoPtr rfbScreen);
extern rfbClientPtr rfbClientIteratorNext(rfbClientIteratorPtr iterator);
extern void rfbReleaseClientIterator(rfbClientIteratorPtr iterator);

extern void rfbNewClientConnection(rfbScreenInfoPtr rfbScreen,int sock);
extern rfbClientPtr rfbNewClient(rfbScreenInfoPtr rfbScreen,int sock);
extern rfbClientPtr rfbNewUDPClient(rfbScreenInfoPtr rfbScreen);
extern rfbClientPtr rfbReverseConnection(rfbScreenInfoPtr rfbScreen,char *host, int port);
extern void rfbClientConnectionGone(rfbClientPtr cl);
extern void rfbProcessClientMessage(rfbClientPtr cl);
extern void rfbClientConnFailed(rfbClientPtr cl, char *reason);
extern void rfbNewUDPConnection(rfbScreenInfoPtr rfbScreen,int sock);
extern void rfbProcessUDPInput(rfbScreenInfoPtr rfbScreen);
extern rfbBool rfbSendFramebufferUpdate(rfbClientPtr cl, sraRegionPtr updateRegion);
extern rfbBool rfbSendRectEncodingRaw(rfbClientPtr cl, int x,int y,int w,int h);
extern rfbBool rfbSendUpdateBuf(rfbClientPtr cl);
extern void rfbSendServerCutText(rfbScreenInfoPtr rfbScreen,char *str, int len);
extern rfbBool rfbSendCopyRegion(rfbClientPtr cl,sraRegionPtr reg,int dx,int dy);
extern rfbBool rfbSendLastRectMarker(rfbClientPtr cl);
extern rfbBool rfbSendNewFBSize(rfbClientPtr cl, int w, int h);
extern rfbBool rfbSendSetColourMapEntries(rfbClientPtr cl, int firstColour, int nColours);
extern void rfbSendBell(rfbScreenInfoPtr rfbScreen);

void rfbGotXCutText(rfbScreenInfoPtr rfbScreen, char *str, int len);

#ifdef LIBVNCSERVER_BACKCHANNEL
extern void rfbSendBackChannel(rfbScreenInfoPtr s,char* message,int len);
#endif

/* translate.c */

extern rfbBool rfbEconomicTranslate;

extern void rfbTranslateNone(char *table, rfbPixelFormat *in,
                             rfbPixelFormat *out,
                             char *iptr, char *optr,
                             int bytesBetweenInputLines,
                             int width, int height);
extern rfbBool rfbSetTranslateFunction(rfbClientPtr cl);
extern rfbBool rfbSetClientColourMap(rfbClientPtr cl, int firstColour, int nColours);
extern void rfbSetClientColourMaps(rfbScreenInfoPtr rfbScreen, int firstColour, int nColours);

/* httpd.c */

extern void rfbHttpInitSockets(rfbScreenInfoPtr rfbScreen);
extern void rfbHttpCheckFds(rfbScreenInfoPtr rfbScreen);



/* auth.c */

extern void rfbAuthNewClient(rfbClientPtr cl);
extern void rfbAuthProcessClientMessage(rfbClientPtr cl);


/* rre.c */

extern rfbBool rfbSendRectEncodingRRE(rfbClientPtr cl, int x,int y,int w,int h);


/* corre.c */

extern rfbBool rfbSendRectEncodingCoRRE(rfbClientPtr cl, int x,int y,int w,int h);


/* hextile.c */

extern rfbBool rfbSendRectEncodingHextile(rfbClientPtr cl, int x, int y, int w,
                                       int h);


#ifdef LIBVNCSERVER_HAVE_LIBZ
/* zlib.c */

/* Minimum zlib rectangle size in bytes.  Anything smaller will
 * not compress well due to overhead.
 */
#define VNC_ENCODE_ZLIB_MIN_COMP_SIZE (17)

/* Set maximum zlib rectangle size in pixels.  Always allow at least
 * two scan lines.
 */
#define ZLIB_MAX_RECT_SIZE (128*256)
#define ZLIB_MAX_SIZE(min) ((( min * 2 ) > ZLIB_MAX_RECT_SIZE ) ? \
			    ( min * 2 ) : ZLIB_MAX_RECT_SIZE )

extern rfbBool rfbSendRectEncodingZlib(rfbClientPtr cl, int x, int y, int w,
				    int h);

#ifdef LIBVNCSERVER_HAVE_LIBJPEG
/* tight.c */

#define TIGHT_DEFAULT_COMPRESSION  6

extern rfbBool rfbTightDisableGradient;

extern int rfbNumCodedRectsTight(rfbClientPtr cl, int x,int y,int w,int h);
extern rfbBool rfbSendRectEncodingTight(rfbClientPtr cl, int x,int y,int w,int h);
#endif
#endif


/* cursor.c */

typedef struct rfbCursor {
    /* set this to true if LibVNCServer has to free this cursor */
    rfbBool cleanup, cleanupSource, cleanupMask, cleanupRichSource;
    unsigned char *source;			/* points to bits */
    unsigned char *mask;			/* points to bits */
    unsigned short width, height, xhot, yhot;	/* metrics */
    unsigned short foreRed, foreGreen, foreBlue; /* device-independent colour */
    unsigned short backRed, backGreen, backBlue; /* device-independent colour */
    unsigned char *richSource; /* source bytes for a rich cursor */
} rfbCursor, *rfbCursorPtr;
extern unsigned char rfbReverseByte[0x100];

extern rfbBool rfbSendCursorShape(rfbClientPtr cl/*, rfbScreenInfoPtr pScreen*/);
extern rfbBool rfbSendCursorPos(rfbClientPtr cl);
extern void rfbConvertLSBCursorBitmapOrMask(int width,int height,unsigned char* bitmap);
extern rfbCursorPtr rfbMakeXCursor(int width,int height,char* cursorString,char* maskString);
extern char* rfbMakeMaskForXCursor(int width,int height,char* cursorString);
extern void rfbMakeXCursorFromRichCursor(rfbScreenInfoPtr rfbScreen,rfbCursorPtr cursor);
extern void rfbMakeRichCursorFromXCursor(rfbScreenInfoPtr rfbScreen,rfbCursorPtr cursor);
extern void rfbFreeCursor(rfbCursorPtr cursor);
extern void rfbDrawCursor(rfbScreenInfoPtr rfbScreen);
extern void rfbUndrawCursor(rfbScreenInfoPtr rfbScreen);
extern void rfbSetCursor(rfbScreenInfoPtr rfbScreen,rfbCursorPtr c,rfbBool freeOld);

/* cursor handling for the pointer */
extern void rfbDefaultPtrAddEvent(int buttonMask,int x,int y,rfbClientPtr cl);

/* zrle.c */
#ifdef LIBVNCSERVER_HAVE_LIBZ
extern rfbBool rfbSendRectEncodingZRLE(rfbClientPtr cl, int x, int y, int w,int h);
extern void rfbFreeZrleData(rfbClientPtr cl);
#endif

/* stats.c */

extern void rfbResetStats(rfbClientPtr cl);
extern void rfbPrintStats(rfbClientPtr cl);

/* font.c */

typedef struct rfbFontData {
  unsigned char* data;
  /*
    metaData is a 256*5 array:
    for each character
    (offset,width,height,x,y)
  */
  int* metaData;
} rfbFontData,* rfbFontDataPtr;

int rfbDrawChar(rfbScreenInfoPtr rfbScreen,rfbFontDataPtr font,int x,int y,unsigned char c,rfbPixel colour);
void rfbDrawString(rfbScreenInfoPtr rfbScreen,rfbFontDataPtr font,int x,int y,const char* string,rfbPixel colour);
/* if colour==backColour, background is transparent */
int rfbDrawCharWithClip(rfbScreenInfoPtr rfbScreen,rfbFontDataPtr font,int x,int y,unsigned char c,int x1,int y1,int x2,int y2,rfbPixel colour,rfbPixel backColour);
void rfbDrawStringWithClip(rfbScreenInfoPtr rfbScreen,rfbFontDataPtr font,int x,int y,const char* string,int x1,int y1,int x2,int y2,rfbPixel colour,rfbPixel backColour);
int rfbWidthOfString(rfbFontDataPtr font,const char* string);
int rfbWidthOfChar(rfbFontDataPtr font,unsigned char c);
void rfbFontBBox(rfbFontDataPtr font,unsigned char c,int* x1,int* y1,int* x2,int* y2);
/* this returns the smallest box enclosing any character of font. */
void rfbWholeFontBBox(rfbFontDataPtr font,int *x1, int *y1, int *x2, int *y2);

/* dynamically load a linux console font (4096 bytes, 256 glyphs a 8x16 */
rfbFontDataPtr rfbLoadConsoleFont(char *filename);
/* free a dynamically loaded font */
void rfbFreeFont(rfbFontDataPtr font);

/* draw.c */

/* You have to call rfbUndrawCursor before using these functions */
void rfbFillRect(rfbScreenInfoPtr s,int x1,int y1,int x2,int y2,rfbPixel col);
void rfbDrawPixel(rfbScreenInfoPtr s,int x,int y,rfbPixel col);
void rfbDrawLine(rfbScreenInfoPtr s,int x1,int y1,int x2,int y2,rfbPixel col);

/* selbox.c */

/* this opens a modal select box. list is an array of strings, the end marked
   with a NULL.
   It returns the index in the list or -1 if cancelled or something else
   wasn't kosher. */
typedef void (*SelectionChangedHookPtr)(int _index);
extern int rfbSelectBox(rfbScreenInfoPtr rfbScreen,
			rfbFontDataPtr font, char** list,
			int x1, int y1, int x2, int y2,
			rfbPixel foreColour, rfbPixel backColour,
			int border,SelectionChangedHookPtr selChangedHook);

/* cargs.c */

extern void rfbUsage(void);
extern void rfbPurgeArguments(int* argc,int* position,int count,char *argv[]);
extern rfbBool rfbProcessArguments(rfbScreenInfoPtr rfbScreen,int* argc, char *argv[]);
extern rfbBool rfbProcessSizeArguments(int* width,int* height,int* bpp,int* argc, char *argv[]);

/* main.c */

extern void rfbLogEnable(int enabled);
typedef void (*rfbLogProc)(const char *format, ...);
extern rfbLogProc rfbLog, rfbErr;
extern void rfbLogPerror(const char *str);

void rfbScheduleCopyRect(rfbScreenInfoPtr rfbScreen,int x1,int y1,int x2,int y2,int dx,int dy);
void rfbScheduleCopyRegion(rfbScreenInfoPtr rfbScreen,sraRegionPtr copyRegion,int dx,int dy);

void rfbDoCopyRect(rfbScreenInfoPtr rfbScreen,int x1,int y1,int x2,int y2,int dx,int dy);
void rfbDoCopyRegion(rfbScreenInfoPtr rfbScreen,sraRegionPtr copyRegion,int dx,int dy);

void rfbMarkRectAsModified(rfbScreenInfoPtr rfbScreen,int x1,int y1,int x2,int y2);
void rfbMarkRegionAsModified(rfbScreenInfoPtr rfbScreen,sraRegionPtr modRegion);
void rfbDoNothingWithClient(rfbClientPtr cl);
enum rfbNewClientAction defaultNewClientHook(rfbClientPtr cl);

/* to check against plain passwords */
rfbBool rfbCheckPasswordByList(rfbClientPtr cl,const char* response,int len);

/* functions to make a vnc server */
extern rfbScreenInfoPtr rfbGetScreen(int* argc,char** argv,
 int width,int height,int bitsPerSample,int samplesPerPixel,
 int bytesPerPixel);
extern void rfbInitServer(rfbScreenInfoPtr rfbScreen);
extern void rfbNewFramebuffer(rfbScreenInfoPtr rfbScreen,char *framebuffer,
 int width,int height, int bitsPerSample,int samplesPerPixel,
 int bytesPerPixel);

extern void rfbScreenCleanup(rfbScreenInfoPtr screenInfo);

/* functions to accept/refuse a client that has been put on hold
   by a NewClientHookPtr function. Must not be called in other
   situations. */
extern void rfbStartOnHoldClient(rfbClientPtr cl);
extern void rfbRefuseOnHoldClient(rfbClientPtr cl);

/* call one of these two functions to service the vnc clients.
 usec are the microseconds the select on the fds waits.
 if you are using the event loop, set this to some value > 0, so the
 server doesn't get a high load just by listening. */

extern void rfbRunEventLoop(rfbScreenInfoPtr screenInfo, long usec, rfbBool runInBackground);
extern void rfbProcessEvents(rfbScreenInfoPtr screenInfo,long usec);

#endif

#if(defined __cplusplus)
}
#endif
