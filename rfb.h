/*
 * rfb.h - header file for RFB DDX implementation.
 */

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
#include <string.h>
#include "keysym.h"

/* TODO: this stuff has to go into autoconf */
typedef unsigned char CARD8;
typedef unsigned short CARD16;
typedef unsigned int CARD32;
typedef CARD32 Pixel;
typedef CARD32 KeySym;
/* for some strange reason, "typedef signed char Bool;" yields a four byte
   signed int on an SGI, but only for rfbserver.o!!! */
#define Bool signed char
#define FALSE 0
#define TRUE -1

#define xalloc malloc
#define xrealloc realloc
#define xfree free

int max(int,int);

#include <zlib.h>

#include <rfbproto.h>
#include <netinet/in.h>
#ifdef HAVE_PTHREADS
#include <pthread.h>
#endif
#ifdef __linux__
#include <endian.h>
#else
#ifdef __APPLE__
#include <machine/endian.h>
#define _BYTE_ORDER BYTE_ORDER
#define _LITTLE_ENDIAN LITTLE_ENDIAN
#else
#include <sys/endian.h>
#endif
#endif

#ifndef _BYTE_ORDER
#define _BYTE_ORDER __BYTE_ORDER
#endif

#ifndef _LITTLE_ENDIAN
#define _LITTLE_ENDIAN __LITTLE_ENDIAN
#endif

#define MAX_ENCODINGS 10

#ifdef HAVE_PTHREADS
#define IF_PTHREADS(x) (x)
#else
#define IF_PTHREADS(x)
#endif


struct rfbClientRec;
struct rfbScreenInfo;
struct rfbCursor;

typedef void (*KbdAddEventProcPtr) (Bool down, KeySym keySym, struct rfbClientRec* cl);
typedef void (*KbdReleaseAllKeysProcPtr) (struct rfbClientRec* cl);
typedef void (*PtrAddEventProcPtr) (int buttonMask, int x, int y, struct rfbClientRec* cl);
typedef void (*SetXCutTextProcPtr) (char* str,int len, struct rfbClientRec* cl);
typedef struct rfbCursor* (*GetCursorProcPtr) (struct rfbClientRec* pScreen);
typedef void (*NewClientHookPtr)(struct rfbClientRec* cl);

/*
 * Per-screen (framebuffer) structure.  There is only one of these, since we
 * don't allow the X server to have multiple screens.
 */

typedef struct
{
    int width;
    int paddedWidthInBytes;
    int height;
    int depth;
    int bitsPerPixel;
    int sizeInBytes;

    Pixel blackPixel;
    Pixel whitePixel;

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

       Removing the cursor (rfbSpriteRemoveCursor) is eventually achieved by
       doing a CopyArea from a pixmap to the screen, where the pixmap contains
       the saved contents of the screen under the cursor.  Before doing this,
       however, we set cursorIsDrawn to FALSE.  Then, when CopyArea is called,
       it sees that cursorIsDrawn is FALSE and so doesn't feel the need to
       (recursively!) remove the cursor before doing it.

       Putting up the cursor (rfbSpriteRestoreCursor) involves a call to
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

    Bool cursorIsDrawn;		    /* TRUE if the cursor is currently drawn */
    Bool dontSendFramebufferUpdate; /* TRUE while removing or drawing the
				       cursor */
   
    /* these variables are needed to save the area under the cursor */
    int cursorX, cursorY,underCursorBufferLen;
    char* underCursorBuffer;
   
    /* wrapped screen functions */

  /*
    CloseScreenProcPtr			CloseScreen;
    CreateGCProcPtr			CreateGC;
    PaintWindowBackgroundProcPtr	PaintWindowBackground;
    PaintWindowBorderProcPtr		PaintWindowBorder;
    CopyWindowProcPtr			CopyWindow;
    ClearToBackgroundProcPtr		ClearToBackground;
    RestoreAreasProcPtr			RestoreAreas;
  */

    /* additions by libvncserver */

  /*
    ScreenRec screen;
  */
    rfbPixelFormat rfbServerFormat;
    char* desktopName;
    char rfbThisHost[255];
    int rfbPort;
    Bool socketInitDone;
    int inetdSock;
    int maxSock;
    int maxFd;
    int rfbListenSock;
    int udpPort;
    int udpSock;
    Bool udpSockConnected;
    struct sockaddr_in udpRemoteAddr;
    Bool inetdInitDone;
    fd_set allFds;
    int rfbMaxClientWait;
  /* http stuff */
    Bool httpInitDone;
    int httpPort;
    char* httpDir;
    int httpListenSock;
    int httpSock;
    FILE* httpFP;

    char* rfbAuthPasswdFile;
    int rfbDeferUpdateTime;
    char* rfbScreen;
    Bool rfbAlwaysShared;
    Bool rfbNeverShared;
    Bool rfbDontDisconnect;
    struct rfbClientRec* rfbClientHead;
    struct rfbCursor* cursor;
   
    /* the following members have to be supplied by the serving process */
    char* frameBuffer;
    KbdAddEventProcPtr kbdAddEvent;
    KbdReleaseAllKeysProcPtr kbdReleaseAllKeys;
    PtrAddEventProcPtr ptrAddEvent;
    SetXCutTextProcPtr setXCutText;
    GetCursorProcPtr getCursorPtr;
   
    /* the following members are hooks, i.e. they are called if set,
       but not overriding original functionality */
    /* newClientHook is called just after a new client is created */
    NewClientHookPtr newClientHook;

} rfbScreenInfo, *rfbScreenInfoPtr;


/*
 * rfbTranslateFnType is the type of translation functions.
 */

typedef void (*rfbTranslateFnType)(char *table, rfbPixelFormat *in,
                                   rfbPixelFormat *out,
                                   char *iptr, char *optr,
                                   int bytesBetweenInputLines,
                                   int width, int height);


/* 
 * vncauth.h - describes the functions provided by the vncauth library.
 */

#define MAXPWLEN 8
#define CHALLENGESIZE 16

extern int vncEncryptAndStorePasswd(char *passwd, char *fname);
extern char *vncDecryptPasswdFromFile(char *fname);
extern void vncRandomBytes(unsigned char *bytes);
extern void vncEncryptBytes(unsigned char *bytes, char *passwd);

/* region stuff */

typedef struct BoxRec {
    short x1, y1, x2, y2;
} BoxRec, *BoxPtr;

typedef struct RegDataRec* RegDataPtr;

typedef struct RegionRec {
    BoxRec 	extents;
    RegDataPtr	data;
} RegionRec, *RegionPtr;

/*
 * Per-client structure.
 */

typedef void (*ClientGoneHookPtr)(struct rfbClientRec* cl);

typedef struct rfbClientRec {
    rfbScreenInfoPtr screen;
    void* clientData;
    ClientGoneHookPtr clientGoneHook;
   
    int sock;
    char *host;
#ifdef HAVE_PTHREADS
    pthread_mutex_t outputMutex;
#endif
                                /* Possible client states: */
    enum {
        RFB_PROTOCOL_VERSION,   /* establishing protocol version */
        RFB_AUTHENTICATION,     /* authenticating */
        RFB_INITIALISATION,     /* sending initialisation messages */
        RFB_NORMAL              /* normal protocol messages */
    } state;

    Bool reverseConnection;

    Bool readyForSetColourMapEntries;
   
    Bool useCopyRect;
    int preferredEncoding;
    int correMaxWidth, correMaxHeight;

    /* The following member is only used during VNC authentication */

    CARD8 authChallenge[CHALLENGESIZE];

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

    RegionRec copyRegion;	/* the destination region of the copy */
    int copyDX, copyDY;		/* the translation by which the copy happens */


#ifdef HAVE_PTHREADS
    pthread_mutex_t updateMutex;
    pthread_cond_t updateCond;
#endif

    RegionRec modifiedRegion;

    /* As part of the FramebufferUpdateRequest, a client can express interest
       in a subrectangle of the whole framebuffer.  This is stored in the
       requestedRegion member.  In the normal case this is the whole
       framebuffer if the client is ready, empty if it's not. */

    RegionRec requestedRegion;

    /* The following members represent the state of the "deferred update" timer
       - when the framebuffer is modified and the client is ready, in most
       cases it is more efficient to defer sending the update by a few
       milliseconds so that several changes to the framebuffer can be combined
       into a single update. */

  /* no deferred timer here; server has to do it alone */

  /* Bool deferredUpdateScheduled;
     OsTimerPtr deferredUpdateTimer; */

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

    int rfbBytesSent[MAX_ENCODINGS];
    int rfbRectanglesSent[MAX_ENCODINGS];
    int rfbLastRectMarkersSent;
    int rfbLastRectBytesSent;
    int rfbCursorBytesSent;
    int rfbCursorUpdatesSent;
    int rfbFramebufferUpdateMessagesSent;
    int rfbRawBytesEquivalent;
    int rfbKeyEventsRcvd;
    int rfbPointerEventsRcvd;

    /* zlib encoding -- necessary compression state info per client */

    struct z_stream_s compStream;
    Bool compStreamInited;

    CARD32 zlibCompressLevel;

    /* tight encoding -- preserve zlib streams' state for each client */

    z_stream zsStruct[4];
    Bool zsActive[4];
    int zsLevel[4];
    int tightCompressLevel;
    int tightQualityLevel;

    Bool enableLastRectEncoding;   /* client supports LastRect encoding */
    Bool enableCursorShapeUpdates; /* client supports cursor shape updates */
    Bool useRichCursorEncoding;    /* rfbEncodingRichCursor is preferred */
    Bool cursorWasChanged;         /* cursor shape update should be sent */

    struct rfbClientRec *prev;
    struct rfbClientRec *next;

} rfbClientRec, *rfbClientPtr;


/*
 * This macro is used to test whether there is a framebuffer update needing to
 * be sent to the client.
 */

#define FB_UPDATE_PENDING(cl)                           \
     ((!(cl)->enableCursorShapeUpdates && !(cl)->screen->cursorIsDrawn) ||   \
     ((cl)->enableCursorShapeUpdates && (cl)->cursorWasChanged) ||      \
     REGION_NOTEMPTY(&((cl)->screenInfo->screen),&(cl)->copyRegion) ||  \
     REGION_NOTEMPTY(&((cl)->screenInfo->screen),&(cl)->modifiedRegion))

/*
 * This macro creates an empty region (ie. a region with no areas) if it is
 * given a rectangle with a width or height of zero. It appears that 
 * REGION_INTERSECT does not quite do the right thing with zero-width
 * rectangles, but it should with completely empty regions.
 */

#define SAFE_REGION_INIT(pscreen, preg, rect, size)          \
{                                                            \
      if ( ( (rect) ) &&                                     \
           ( ( (rect)->x2 == (rect)->x1 ) ||                 \
             ( (rect)->y2 == (rect)->y1 ) ) ) {              \
          REGION_INIT( (pscreen), (preg), NullBox, 0 );      \
      } else {                                               \
          REGION_INIT( (pscreen), (preg), (rect), (size) );  \
      }                                                      \
}


/*
 * Macros for endian swapping.
 */

#define Swap16(s) ((((s) & 0xff) << 8) | (((s) >> 8) & 0xff))

#define Swap32(l) (((l) >> 24) | \
                   (((l) & 0x00ff0000) >> 8)  | \
                   (((l) & 0x0000ff00) << 8)  | \
                   ((l) << 24))


static const int rfbEndianTest = (_BYTE_ORDER == _LITTLE_ENDIAN);

#define Swap16IfLE(s) (rfbEndianTest ? Swap16(s) : (s))

#define Swap32IfLE(l) (rfbEndianTest ? Swap32(l) : (l))

/* main.c */

extern void rfbLog(char *format, ...);
extern void rfbLogPerror(char *str);
extern int runVNCServer(int argc, char *argv[]);


/* sockets.c */

extern int rfbMaxClientWait;

extern void rfbCloseClient(rfbClientPtr cl);
extern int ReadExact(rfbClientPtr cl, char *buf, int len);
extern int WriteExact(rfbClientPtr cl, char *buf, int len);
extern void rfbCheckFds(rfbScreenInfoPtr rfbScreen,long usec);
extern int ListenOnTCPPort(int port);
extern int ListenOnUDPPort(int port);

/* rfbserver.c */

extern rfbClientPtr pointerClient;


/* Routines to iterate over the client list in a thread-safe way.
   Only a single iterator can be in use at a time process-wide. */
typedef struct rfbClientIterator *rfbClientIteratorPtr;

extern void rfbClientListInit(rfbScreenInfoPtr rfbScreen);
extern rfbClientIteratorPtr rfbGetClientIterator(rfbScreenInfoPtr rfbScreen);
extern rfbClientPtr rfbClientIteratorNext(rfbClientIteratorPtr iterator);
extern void rfbReleaseClientIterator(rfbClientIteratorPtr iterator);

extern void rfbNewClientConnection(rfbScreenInfoPtr rfbScreen,int sock);
extern rfbClientPtr rfbNewClient(rfbScreenInfoPtr rfbScreen,int sock);
extern rfbClientPtr rfbReverseConnection(char *host, int port);
extern void rfbClientConnectionGone(rfbClientPtr cl);
extern void rfbProcessClientMessage(rfbClientPtr cl);
extern void rfbClientConnFailed(rfbClientPtr cl, char *reason);
extern void rfbNewUDPConnection(rfbScreenInfoPtr rfbScreen,int sock);
extern void rfbProcessUDPInput(rfbClientPtr cl);
extern Bool rfbSendFramebufferUpdate(rfbClientPtr cl, RegionRec updateRegion);
extern Bool rfbSendRectEncodingRaw(rfbClientPtr cl, int x,int y,int w,int h);
extern Bool rfbSendUpdateBuf(rfbClientPtr cl);
extern void rfbSendServerCutText(rfbScreenInfoPtr rfbScreen,char *str, int len);
extern Bool rfbSendCopyRegion(rfbClientPtr cl,RegionPtr reg,int dx,int dy);
extern Bool rfbSendLastRectMarker(rfbClientPtr cl);

void rfbGotXCutText(rfbScreenInfoPtr rfbScreen, char *str, int len);

/* translate.c */

extern Bool rfbEconomicTranslate;

extern void rfbTranslateNone(char *table, rfbPixelFormat *in,
                             rfbPixelFormat *out,
                             char *iptr, char *optr,
                             int bytesBetweenInputLines,
                             int width, int height);
extern Bool rfbSetTranslateFunction(rfbClientPtr cl);


/* httpd.c */

extern int httpPort;
extern char *httpDir;

extern void httpInitSockets();
extern void httpCheckFds();



/* auth.c */

extern char *rfbAuthPasswdFile;
extern Bool rfbAuthenticating;

extern void rfbAuthNewClient(rfbClientPtr cl);
extern void rfbAuthProcessClientMessage(rfbClientPtr cl);


/* rre.c */

extern Bool rfbSendRectEncodingRRE(rfbClientPtr cl, int x,int y,int w,int h);


/* corre.c */

extern Bool rfbSendRectEncodingCoRRE(rfbClientPtr cl, int x,int y,int w,int h);


/* hextile.c */

extern Bool rfbSendRectEncodingHextile(rfbClientPtr cl, int x, int y, int w,
                                       int h);


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

extern Bool rfbSendRectEncodingZlib(rfbClientPtr cl, int x, int y, int w,
				    int h);


/* tight.c */

#define TIGHT_DEFAULT_COMPRESSION  6

extern Bool rfbTightDisableGradient;

extern int rfbNumCodedRectsTight(rfbClientPtr cl, int x,int y,int w,int h);
extern Bool rfbSendRectEncodingTight(rfbClientPtr cl, int x,int y,int w,int h);


/* cursor.c */

typedef struct rfbCursor {
    unsigned char *source;			/* points to bits */
    unsigned char *mask;			/* points to bits */
    unsigned short width, height, xhot, yhot;	/* metrics */
    unsigned short foreRed, foreGreen, foreBlue; /* device-independent color */
    unsigned short backRed, backGreen, backBlue; /* device-independent color */
    unsigned char *richSource; /* source bytes for a rich cursor */
} rfbCursor, *rfbCursorPtr;

extern Bool rfbSendCursorShape(rfbClientPtr cl/*, rfbScreenInfoPtr pScreen*/);
extern void rfbConvertLSBCursorBitmapOrMask(int width,int height,unsigned char* bitmap);
extern rfbCursorPtr rfbMakeXCursor(int width,int height,char* cursorString,char* maskString);
extern char* rfbMakeMaskForXCursor(int width,int height,char* cursorString);
extern void MakeXCursorFromRichCursor(rfbScreenInfoPtr rfbScreen,rfbCursorPtr cursor);
extern void MakeRichCursorFromXCursor(rfbScreenInfoPtr rfbScreen,rfbCursorPtr cursor);
extern void rfbFreeCursor(rfbCursorPtr cursor);
extern void rfbDrawCursor(rfbClientPtr cl);
extern void rfbUndrawCursor(rfbClientPtr cl);

/* stats.c */

extern void rfbResetStats(rfbClientPtr cl);
extern void rfbPrintStats(rfbClientPtr cl);

extern void rfbInitSockets(rfbScreenInfoPtr rfbScreen);
extern void rfbDisconnectUDPSock(rfbScreenInfoPtr cl);

void rfbMarkRectAsModified(rfbScreenInfoPtr rfbScreen,int x1,int y1,int x2,int y2);
void rfbMarkRegionAsModified(rfbScreenInfoPtr rfbScreen,RegionPtr modRegion);
void doNothingWithClient(rfbClientPtr cl);

/* functions to make a vnc server */
extern rfbScreenInfoPtr rfbGetScreen(int argc,char** argv,
 int width,int height,int bitsPerSample,int samplesPerPixel,
 int bytesPerPixel);
extern void rfbInitServer(rfbScreenInfoPtr rfbScreen);
extern void rfbScreenCleanup(rfbScreenInfoPtr screenInfo);

/* call one of these two functions to service the vnc clients.
 usec are the microseconds the select on the fds waits.
 if you are using the event loop, set this to some value > 0, so the
 server doesn't get a high load just by listening. */

extern void rfbRunEventLoop(rfbScreenInfoPtr screenInfo, long usec, Bool runInBackground);
extern void rfbProcessEvents(rfbScreenInfoPtr screenInfo,long usec);
