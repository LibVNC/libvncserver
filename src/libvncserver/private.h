#ifndef RFB_PRIVATE_H
#define RFB_PRIVATE_H

/* from cursor.c */

void rfbShowCursor(rfbClientPtr cl);
void rfbHideCursor(rfbClientPtr cl);
void rfbRedrawAfterHideCursor(rfbClientPtr cl,sraRegionPtr updateRegion);

/* from main.c */

enum rfbProtocolExtensionHookTypeReserved {
    RFB_PROTOCOL_EXTENSION_HOOK_EXTENSION1 = 1,
};

/** pointer to the type 1 extension */
typedef rfbProtocolExtension* rfbProtocolExtensionHookExtension1;
_Static_assert(sizeof(rfbProtocolExtensionHookGeneric) == sizeof(rfbProtocolExtensionHookExtension1), "extension hook size doesn't match");

typedef struct _rfbExtension2Data {
    struct _rfbProtocolExtension2* extension2;
    void* data;
    struct _rfbExtension2Data* next;
} rfbExtension2Data;

rfbClientPtr rfbClientIteratorHead(rfbClientIteratorPtr i);

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

