
/*
 *  OSXvnc Copyright (C) 2001 Dan McGuirk <mcguirk@incompleteness.net>.
 *  Original Xvnc code Copyright (C) 1999 AT&T Laboratories Cambridge.  
 *  All Rights Reserved.
 * 
 * Cut in two parts by Johannes Schindelin (2001): libvncserver and OSXvnc.
 * 
 * 
 * This file implements every system specific function for Mac OS X.
 * 
 *  It includes the init function:
 * 
     void rfbScreenInit(void)
 * 
 * the function to get the address of the framebuffer:
 * 
     char *rfbGetFramebuffer(void)
 *
 *  keyboard functions:
 * 
     void KbdAddEvent(down, keySym, cl)
        Bool down;
        KeySym keySym;
        rfbClientPtr cl;
     void KbdReleaseAllKeys()
 * 
 *  the mouse functions:
 * 
     void PtrAddEvent(buttonMask, x, y, cl)
        int buttonMask;
        int x;
        int y;
        rfbClientPtr cl;
 * 
 *  and the EventLoop:
 * 
     void runEventLoop()
 * 
 * These six functions have to be defined by every VNC server using
 * libvncserver.
 *
 * The main function just calls runVNCServer. This is different
 * for more complex users of libvncserver.
 * 
 */

#include "rfb.h"
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <X11/keysym.h>

void 
rfbScreenInit(void)
{
    int bitsPerSample;
    int samplesPerPixel;

    rfbScreen.width = CGDisplayPixelsWide(kCGDirectMainDisplay);
    rfbScreen.height = CGDisplayPixelsHigh(kCGDirectMainDisplay);
    rfbScreen.bitsPerPixel = rfbScreen.depth = 
        CGDisplayBitsPerPixel(kCGDirectMainDisplay);
    gethostname(rfbThisHost, 255);
    rfbScreen.paddedWidthInBytes = CGDisplayBytesPerRow(kCGDirectMainDisplay);
    rfbServerFormat.bitsPerPixel = rfbScreen.bitsPerPixel;
    rfbServerFormat.depth = rfbScreen.depth;
    rfbServerFormat.bigEndian = !(*(char *)&rfbEndianTest);
    rfbServerFormat.trueColour = TRUE;

    bitsPerSample = CGDisplayBitsPerSample(kCGDirectMainDisplay);
    samplesPerPixel = CGDisplaySamplesPerPixel(kCGDirectMainDisplay);
    if (samplesPerPixel != 3) {
        rfbLog("screen format not supported.  exiting.\n");
        exit(1);
    }

    /* This works for 16 and 32-bit, but not for 8-bit.
       What should it be for 8-bit?  (Shouldn't 8-bit use a colormap?) */
    rfbServerFormat.redMax = (1 << bitsPerSample) - 1;
    rfbServerFormat.greenMax = (1 << bitsPerSample) - 1;
    rfbServerFormat.blueMax = (1 << bitsPerSample) - 1;
    rfbServerFormat.redShift = bitsPerSample * 2;
    rfbServerFormat.greenShift = bitsPerSample;
    rfbServerFormat.blueShift = 0;

    /* We want to use the X11 REGION_* macros without having an actual
       X11 ScreenPtr, so we do this.  Pretty ugly, but at least it lets us
       avoid hacking up regionstr.h, or changing every call to REGION_*
       (which actually I should probably do eventually). */
    hackScreen.RegionCreate = miRegionCreate;
    hackScreen.RegionInit = miRegionInit;
    hackScreen.RegionCopy = miRegionCopy;
    hackScreen.RegionDestroy = miRegionDestroy;
    hackScreen.RegionUninit = miRegionUninit;
    hackScreen.Intersect = miIntersect;
    hackScreen.Union = miUnion;
    hackScreen.Subtract = miSubtract;
    hackScreen.Inverse = miInverse;
    hackScreen.RegionReset = miRegionReset;
    hackScreen.TranslateRegion = miTranslateRegion;
    hackScreen.RectIn = miRectIn;
    hackScreen.PointInRegion = miPointInRegion;
    hackScreen.RegionNotEmpty = miRegionNotEmpty;
    hackScreen.RegionEmpty = miRegionEmpty;
    hackScreen.RegionExtents = miRegionExtents;
    hackScreen.RegionAppend = miRegionAppend;
    hackScreen.RegionValidate = miRegionValidate;
}

char *
rfbGetFramebuffer(void)
{
    return (char *)CGDisplayBaseAddress(kCGDirectMainDisplay);
}

/* Where do I get the "official" list of Mac key codes?
   Ripped these out of a Mac II emulator called Basilisk II
   that I found on the net. */
static int keyTable[] = {
    /* The alphabet */
    XK_A,                  0,      /* A */
    XK_B,                 11,      /* B */
    XK_C,                  8,      /* C */
    XK_D,                  2,      /* D */
    XK_E,                 14,      /* E */
    XK_F,                  3,      /* F */
    XK_G,                  5,      /* G */
    XK_H,                  4,      /* H */
    XK_I,                 34,      /* I */
    XK_J,                 38,      /* J */
    XK_K,                 40,      /* K */
    XK_L,                 37,      /* L */
    XK_M,                 46,      /* M */
    XK_N,                 45,      /* N */
    XK_O,                 31,      /* O */
    XK_P,                 35,      /* P */
    XK_Q,                 12,      /* Q */
    XK_R,                 15,      /* R */
    XK_S,                  1,      /* S */
    XK_T,                 17,      /* T */
    XK_U,                 32,      /* U */
    XK_V,                  9,      /* V */
    XK_W,                 13,      /* W */
    XK_X,                  7,      /* X */
    XK_Y,                 16,      /* Y */
    XK_Z,                  6,      /* Z */
    XK_a,                  0,      /* a */
    XK_b,                 11,      /* b */
    XK_c,                  8,      /* c */
    XK_d,                  2,      /* d */
    XK_e,                 14,      /* e */
    XK_f,                  3,      /* f */
    XK_g,                  5,      /* g */
    XK_h,                  4,      /* h */
    XK_i,                 34,      /* i */
    XK_j,                 38,      /* j */
    XK_k,                 40,      /* k */
    XK_l,                 37,      /* l */
    XK_m,                 46,      /* m */
    XK_n,                 45,      /* n */
    XK_o,                 31,      /* o */
    XK_p,                 35,      /* p */
    XK_q,                 12,      /* q */
    XK_r,                 15,      /* r */
    XK_s,                  1,      /* s */
    XK_t,                 17,      /* t */
    XK_u,                 32,      /* u */
    XK_v,                  9,      /* v */
    XK_w,                 13,      /* w */
    XK_x,                  7,      /* x */
    XK_y,                 16,      /* y */
    XK_z,                  6,      /* z */

    /* Numbers */
    XK_0,                 29,      /* 0 */
    XK_1,                 18,      /* 1 */
    XK_2,                 19,      /* 2 */
    XK_3,                 20,      /* 3 */
    XK_4,                 21,      /* 4 */
    XK_5,                 23,      /* 5 */
    XK_6,                 22,      /* 6 */
    XK_7,                 26,      /* 7 */
    XK_8,                 28,      /* 8 */
    XK_9,                 25,      /* 9 */

    /* Symbols */
    XK_exclam,            18,      /* ! */
    XK_at,                19,      /* @ */
    XK_numbersign,        20,      /* # */
    XK_dollar,            21,      /* $ */
    XK_percent,           23,      /* % */
    XK_asciicircum,       22,      /* ^ */
    XK_ampersand,         26,      /* & */
    XK_asterisk,          28,      /* * */
    XK_parenleft,         25,      /* ( */
    XK_parenright,        29,      /* ) */
    XK_minus,             27,      /* - */
    XK_underscore,        27,      /* _ */
    XK_equal,             24,      /* = */
    XK_plus,              24,      /* + */
    XK_grave,             10,      /* ` */  /* XXX ? */
    XK_asciitilde,        10,      /* ~ */
    XK_bracketleft,       33,      /* [ */
    XK_braceleft,         33,      /* { */
    XK_bracketright,      30,      /* ] */
    XK_braceright,        30,      /* } */
    XK_semicolon,         41,      /* ; */
    XK_colon,             41,      /* : */
    XK_apostrophe,        39,      /* ' */
    XK_quotedbl,          39,      /* " */
    XK_comma,             43,      /* , */
    XK_less,              43,      /* < */
    XK_period,            47,      /* . */
    XK_greater,           47,      /* > */
    XK_slash,             44,      /* / */
    XK_question,          44,      /* ? */
    XK_backslash,         42,      /* \ */
    XK_bar,               42,      /* | */

    /* "Special" keys */
    XK_space,             49,      /* Space */
    XK_Return,            36,      /* Return */
    XK_Delete,           117,      /* Delete */
    XK_Tab,               48,      /* Tab */
    XK_Escape,            53,      /* Esc */
    XK_Caps_Lock,         57,      /* Caps Lock */
    XK_Num_Lock,          71,      /* Num Lock */
    XK_Scroll_Lock,      107,      /* Scroll Lock */
    XK_Pause,            113,      /* Pause */
    XK_BackSpace,         51,      /* Backspace */
    XK_Insert,           114,      /* Insert */

    /* Cursor movement */
    XK_Up,               126,      /* Cursor Up */
    XK_Down,             125,      /* Cursor Down */
    XK_Left,             123,      /* Cursor Left */
    XK_Right,            124,      /* Cursor Right */
    XK_Page_Up,          116,      /* Page Up */
    XK_Page_Down,        121,      /* Page Down */
    XK_Home,             115,      /* Home */
    XK_End,              119,      /* End */

    /* Numeric keypad */
    XK_KP_0,              82,      /* KP 0 */
    XK_KP_1,              83,      /* KP 1 */
    XK_KP_2,              84,      /* KP 2 */
    XK_KP_3,              85,      /* KP 3 */
    XK_KP_4,              86,      /* KP 4 */
    XK_KP_5,              87,      /* KP 5 */
    XK_KP_6,              88,      /* KP 6 */
    XK_KP_7,              89,      /* KP 7 */
    XK_KP_8,              91,      /* KP 8 */
    XK_KP_9,              92,      /* KP 9 */
    XK_KP_Enter,          76,      /* KP Enter */
    XK_KP_Decimal,        65,      /* KP . */
    XK_KP_Add,            69,      /* KP + */
    XK_KP_Subtract,       78,      /* KP - */
    XK_KP_Multiply,       67,      /* KP * */
    XK_KP_Divide,         75,      /* KP / */

    /* Function keys */
    XK_F1,               122,      /* F1 */
    XK_F2,               120,      /* F2 */
    XK_F3,                99,      /* F3 */
    XK_F4,               118,      /* F4 */
    XK_F5,                96,      /* F5 */
    XK_F6,                97,      /* F6 */
    XK_F7,                98,      /* F7 */
    XK_F8,               100,      /* F8 */
    XK_F9,               101,      /* F9 */
    XK_F10,              109,      /* F10 */
    XK_F11,              103,      /* F11 */
    XK_F12,              111,      /* F12 */

    /* Modifier keys */
    XK_Shift_L,           56,      /* Shift Left */
    XK_Shift_R,           56,      /* Shift Right */
    XK_Control_L,         59,      /* Ctrl Left */
    XK_Control_R,         59,      /* Ctrl Right */
    XK_Meta_L,            58,      /* Logo Left (-> Option) */
    XK_Meta_R,            58,      /* Logo Right (-> Option) */
    XK_Alt_L,             55,      /* Alt Left (-> Command) */
    XK_Alt_R,             55,      /* Alt Right (-> Command) */

    /* Weirdness I can't figure out */
    /*    XK_3270_PrintScreen,     105,     /* PrintScrn */  /* XXX ? */
    /*  ???  94,          50,      /* International */
    XK_Menu,              50,      /* Menu (-> International) */
};

void
KbdAddEvent(down, keySym, cl)
    Bool down;
    KeySym keySym;
    rfbClientPtr cl;
{
    int i;
    CGKeyCode keyCode = -1;
    int found = 0;

    for (i = 0; i < (sizeof(keyTable) / sizeof(int)); i += 2) {
        if (keyTable[i] == keySym) {
            keyCode = keyTable[i+1];
            found = 1;
            break;
        }
    }

    if (!found) {
        rfbLog("warning: couldn't figure out keycode for X keysym %d (0x%x)\n", 
               (int)keySym, (int)keySym);
    } else {
        /* Hopefully I can get away with not specifying a CGCharCode.
           (Why would you need both?) */
        CGPostKeyboardEvent((CGCharCode)0, keyCode, down);
    }
}

void
PtrAddEvent(buttonMask, x, y, cl)
    int buttonMask;
    int x;
    int y;
    rfbClientPtr cl;
{
    CGPoint position;

    position.x = x;
    position.y = y;

    CGPostMouseEvent(position, TRUE, 8,
                     (buttonMask & (1 << 0)) ? TRUE : FALSE,
                     (buttonMask & (1 << 1)) ? TRUE : FALSE,
                     (buttonMask & (1 << 2)) ? TRUE : FALSE,
                     (buttonMask & (1 << 3)) ? TRUE : FALSE,
                     (buttonMask & (1 << 4)) ? TRUE : FALSE,
                     (buttonMask & (1 << 5)) ? TRUE : FALSE,
                     (buttonMask & (1 << 6)) ? TRUE : FALSE,
                     (buttonMask & (1 << 7)) ? TRUE : FALSE);
}

void
KbdReleaseAllKeys()
{
    /* Doesn't seem to be possible. */
}

static void 
refreshCallback(CGRectCount count, const CGRect *rectArray, void *ignore)
{
    BoxRec box;
    RegionRec region;
    rfbClientIteratorPtr iterator;
    rfbClientPtr cl;                                                          
    int i;

    for (i = 0; i < count; i++) {
        box.x1 = rectArray[i].origin.x;
        box.y1 = rectArray[i].origin.y;
        box.x2 = box.x1 + rectArray[i].size.width;
        box.y2 = box.y1 + rectArray[i].size.height;

        SAFE_REGION_INIT(&hackScreen, &region, &box, 0);

        iterator = rfbGetClientIterator();
        while ((cl = rfbClientIteratorNext(iterator)) != NULL) {
            pthread_mutex_lock(&cl->updateMutex);
            REGION_UNION(&hackScreen,&cl->modifiedRegion,&cl->modifiedRegion,&region);
            pthread_cond_signal(&cl->updateCond);
            pthread_mutex_unlock(&cl->updateMutex);
        }
        rfbReleaseClientIterator(iterator);

        REGION_UNINIT(&hackScreen, &region);
    }
}

void runEventLoop()
{
    CGRegisterScreenRefreshCallback(refreshCallback, NULL);

    RunApplicationEventLoop();
}

int main(int argc,char *argv[])
{
  runVNCServer(argc,argv);
}
