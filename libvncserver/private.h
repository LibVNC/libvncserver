#ifndef RFB_PRIVATE_H
#define RFB_PRIVATE_H

/* from cursor.c */

void rfbShowCursor(rfbClientPtr cl);
void rfbHideCursor(rfbClientPtr cl);
void rfbRedrawAfterHideCursor(rfbClientPtr cl,sraRegionPtr updateRegion);

/* from main.c */

rfbClientPtr rfbClientIteratorHead(rfbClientIteratorPtr i);

/* from tight.c */

extern void rfbTightCleanup(rfbScreenInfoPtr screen);

/* from zrle.c */

extern void rfbFreeZrleData(rfbClientPtr cl);

#endif

