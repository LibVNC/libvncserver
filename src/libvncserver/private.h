#ifndef RFB_PRIVATE_H
#define RFB_PRIVATE_H

/* from cursor.c */

void rfbShowCursor(rfbClientPtr cl);
void rfbHideCursor(rfbClientPtr cl);
void rfbRedrawAfterHideCursor(rfbClientPtr cl,sraRegionPtr updateRegion);

/* from main.c */

rfbClientPtr rfbClientIteratorHead(rfbClientIteratorPtr i);

/* from sockets.c */

/** Return whether sock is still in TCP LISTEN state. A listener dropped out of
 * LISTEN state by the OS when its interface went down must not be select()ed
 * on: it reports readable forever while accept() always fails. */
rfbBool rfbListenSocketIsListening(rfbSocket sock);

/* from tight.c */

#ifdef LIBVNCSERVER_HAVE_LIBZ
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
extern void rfbFreeTightData(rfbClientPtr cl);
#endif

/* from zrle.c */
void rfbFreeZrleData(rfbClientPtr cl);

#endif


/* from ultra.c */

extern void rfbFreeUltraData(rfbClientPtr cl);

#endif

