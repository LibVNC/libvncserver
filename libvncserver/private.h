#ifndef RFB_PRIVATE_H
#define RFB_PRIVATE_H

/* from cursor.c */

void rfbShowCursor(rfbClientPtr cl);
void rfbHideCursor(rfbClientPtr cl);
void rfbRedrawAfterHideCursor(rfbClientPtr cl,sraRegionPtr updateRegion);

/* from main.c */

rfbClientPtr rfbClientIteratorHead(rfbClientIteratorPtr i);

#ifdef LIBVNCSERVER_WITH_WEBSOCKETS
/* from websockets.c */

rfbBool webSocketsHandshake(rfbClientPtr cl, char *scheme, const char *httpHeaders);
#endif

/* from tight.c */

#ifdef LIBVNCSERVER_HAVE_LIBZ
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
extern void rfbTightCleanup(rfbScreenInfoPtr screen);
#endif

/* from zlib.c */
extern void rfbZlibCleanup(rfbScreenInfoPtr screen);

/* from zrle.c */
void rfbFreeZrleData(rfbClientPtr cl);

#endif


/* from ultra.c */

extern void rfbFreeUltraData(rfbClientPtr cl);

#endif

