/*
 * x11vnc.c: a VNC server for X displays.
 *
 * Copyright (c) 2002 Karl J. Runge <runge@karlrunge.com>  All rights reserved.
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
 *
 *
 * This program is based heavily on the following programs:
 *
 *       the originial x11vnc.c in libvncserver (Johannes E. Schindelin)
 *       krfb, the KDE desktopsharing project (Tim Jansen)
 *	 x0rfbserver, the original native X vnc server (Jens Wagner)
 *
 * The primary goal of this program is to create a portable and simple
 * command-line server utility that allows a VNC viewer to connect to an
 * actual X display (as the above do).  The only non-standard dependency
 * of this program is the static library libvncserver.a (although in
 * some environments libjpeg.so may not be readily available and needs
 * to be installed, it may be found at ftp://ftp.uu.net/graphics/jpeg/).
 * To increase portability it is written in plain C.
 *
 * The next goal is to improve performance and interactive response.
 * The algorithm currently used here to achieve this is that of krfb
 * (based on x0rfbserver algorithm).  Additional heuristics are also
 * applied (currently there are a bit too many of these...)
 *
 * To build:
 * Obtain the libvncserver package (http://libvncserver.sourceforge.net).
 * As of 12/2002 this version of x11vnc.c is contained in the libvncserver
 * CVS tree.  Parameters in the Makefile should be adjusted to the build
 * system and then "make x11vnc" should create it.  For earlier releases
 * (say libvncserver-0.4) this file may be inserted in place of the
 * original x11vnc.c file.
 */

#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

/*
 * Work around Bool and KeySym same names in X and rfb.
 * Bool is #define int in <X11/Xlib.h>
 * KeySym is typedef XID in <X11/X.h>
 * (note that X and rfb KeySym types are the same so a bit silly to worry...
 * the Bool types are different though)
 */
typedef Bool X_Bool;
typedef KeySym X_KeySym;

/* the #define Bool can be removed: */
#ifdef Bool
#undef Bool
#endif

/* the KeySym typedef cannot be removed, so use an alias for rest of file: */
#define KeySym RFBKeySym

#include "rfb.h"

Display *dpy = 0;
int scr;
int dpy_x, dpy_y;
int bpp;
int window;
int button_mask = 0;

XImage *tile;
XImage *scanline;
XImage *fullscreen;
int fs_factor = 0;

XShmSegmentInfo tile_shm;
XShmSegmentInfo scanline_shm;
XShmSegmentInfo fullscreen_shm;

rfbScreenInfoPtr screen;
rfbCursorPtr cursor;
int bytes_per_line;

/* size of the basic tile unit that is polled for changes: */
int tile_x = 32;
int tile_y = 32;
int ntiles, ntiles_x, ntiles_y;

/* arrays that indicate changed or checked tiles. */
char *tile_has_diff, *tile_tried;

typedef struct tile_change_region {
	short first_line, last_line;  /* start and end lines, along y,      */
				      /* of the changed area inside a tile. */
	short left_diff, right_diff;  /* info about differences along edges. */
	short top_diff,  bot_diff;
} region_t;

/* array to hold the tiles region_t-s. */
region_t *tile_region;

typedef struct hint {
	/* location x, y, height, and width of a change-rectangle  */
	/* (grows as adjacent horizontal tiles are glued together) */
	int x, y, w, h;
} hint_t;

/* array to hold the hints: */
hint_t *hint_list;

int shared = 0;		/* share vnc display. */
int view_only = 0;	/* client can only watch. */
int connect_once = 1;	/* allow only one client connection. */

int use_modifier_tweak = 0;	/* use the altgr_keyboard modifier tweak */

/*
 * waitms is the msec to wait between screen polls.  Not too old h/w shows
 * poll times of 15-35ms, so maybe this value cuts the rest load by 2 or so.
 */
int waitms = 30;
int defer_update = 30;	/* rfbDeferUpdateTime ms to wait before sends. */
int use_threads = 1;	/* but only if compiled with HAVE_PTHREADS */

/* tile heuristics: */
int use_hints = 1;	/* use the krfb scheme of gluing tiles together. */
int tile_fuzz = 2;	/* tolerance for suspecting changed tiles touching */
			/* a known changed tile. */
int grow_fill = 3;	/* do the grow islands heuristic with this width. */
int gaps_fill = 4;	/* do a final pass to try to fill gaps between tiles. */
double fs_frac = 0.6;	/* threshold tile fraction to do fullscreen updates. */

#define NSCAN 32
int scanlines[NSCAN] = {	 /* scan pattern jitter from x0rfbserver */
	 0, 16,  8, 24,  4, 20, 12, 28,
	10, 26, 18,  2, 22,  6, 30, 14,
	 1, 17,  9, 25,  7, 23, 15, 31,
	19,  3, 27, 11, 29, 13,  5, 21
};

int got_user_input;
int count = 0;
int shut_down = 0;	

/*
 * Not sure why... but when threaded we have to mutex our X11 calls to
 * avoid XIO crashes.  This should not be too bad since keyboard and pointer
 * updates are infrequent compared to the scanning. (note: these lines are
 * noops unless HAVE_PTHREADS)  XXX: what is going on?
 */
MUTEX(x11Mutex);
#define X_LOCK     LOCK(x11Mutex);
#define X_UNLOCK UNLOCK(x11Mutex);

void mark_hint(hint_t);

void clean_up_exit (void) {

	X_LOCK

	XTestDiscard(dpy);

	/* remove the shm areas: */

	XShmDetach(dpy, &tile_shm);
	XDestroyImage(tile);
	shmdt(tile_shm.shmaddr);
	shmctl(tile_shm.shmid, IPC_RMID, 0);

	XShmDetach(dpy, &scanline_shm);
	XDestroyImage(scanline);
	shmdt(scanline_shm.shmaddr);
	shmctl(scanline_shm.shmid, IPC_RMID, 0);

	XShmDetach(dpy, &fullscreen_shm);
	XDestroyImage(fullscreen);
	shmdt(fullscreen_shm.shmaddr);
	shmctl(fullscreen_shm.shmid, IPC_RMID, 0);

	/* more cleanup? */

	X_UNLOCK
	exit(0);
}

void client_gone(rfbClientPtr client) {
	if (connect_once) {
		printf("only one connection allowed.\n");
		clean_up_exit();
	}
}

enum rfbNewClientAction new_client(rfbClientPtr client) {
	if (connect_once) {
		client->clientGoneHook = client_gone;
	}
	if (view_only)  {
		client->clientData = (void *) -1;
	} else {
		client->clientData = (void *) 0;
	}
	return(RFB_CLIENT_ACCEPT);
}

/* For tweaking modifiers wrt the Alt-Graph key */

#define LEFTSHIFT 1
#define RIGHTSHIFT 2
#define ALTGR 4
char mod_state = 0;

char modifiers[0x100];
KeyCode keycodes[0x100], left_shift_code, right_shift_code, altgr_code;

void initialize_keycodes() {
	X_KeySym key, *keymap;
	int i, j, minkey, maxkey, syms_per_keycode;

	memset(modifiers, -1, sizeof(modifiers));

	XDisplayKeycodes(dpy, &minkey, &maxkey);

	keymap = XGetKeyboardMapping(dpy, minkey, (maxkey - minkey + 1),
	    &syms_per_keycode);

	/* handle alphabetic char with only one keysym (no upper + lower) */
	for (i = minkey; i <= maxkey; i++) {
		X_KeySym lower, upper;
		/* 2nd one */
		key = keymap[(i - minkey) * syms_per_keycode + 1];
		if (key != NoSymbol) {
			continue;
		}
		/* 1st one */
		key = keymap[(i - minkey) * syms_per_keycode + 0];
		if (key == NoSymbol) {
			continue;
		}
		XConvertCase(key, &lower, &upper);
		if (lower != upper) {
			keymap[(i - minkey) * syms_per_keycode + 0] = lower;
			keymap[(i - minkey) * syms_per_keycode + 1] = upper;
		}
	}
	for (i = minkey; i <= maxkey; i++) {
		for (j = 0; j < syms_per_keycode; j++) {
			key = keymap[ (i - minkey) * syms_per_keycode + j ];
			if ( key >= ' ' && key < 0x100
			    && i == XKeysymToKeycode(dpy, key) ) {
				keycodes[key] = i;
				modifiers[key] = j;
			}
		}
	}

	left_shift_code = XKeysymToKeycode(dpy, XK_Shift_L);
	right_shift_code = XKeysymToKeycode(dpy, XK_Shift_R);
	altgr_code = XKeysymToKeycode(dpy, XK_Mode_switch);

	XFree ((void *) keymap);
}

void DebugXTestFakeKeyEvent(Display* dpy, KeyCode keysym, Bool down, time_t cur_time)
{
    fprintf(stderr,"XTestFakeKeyEvent(dpy,%s(0x%x),%s,CurrentTime)\n",
	    XKeysymToString(XKeycodeToKeysym(dpy,keysym,0)),keysym,down?"down":"up");
    XTestFakeKeyEvent(dpy,keysym,down,cur_time);
}

/* #define XTestFakeKeyEvent DebugXTestFakeKeyEvent */

void tweak_mod(signed char mod, Bool down) {
	Bool is_shift = mod_state & (LEFTSHIFT|RIGHTSHIFT);
	X_Bool dn = (X_Bool) down;

	if (mod < 0) {
		return;
	}

	X_LOCK
	if (is_shift && mod != 1) {
	    if (mod_state & LEFTSHIFT) {
		XTestFakeKeyEvent(dpy, left_shift_code, !dn, CurrentTime);
	    }
	    if (mod_state & RIGHTSHIFT) {
		XTestFakeKeyEvent(dpy, right_shift_code, !dn, CurrentTime);
	    }
	}
	if ( ! is_shift && mod == 1 ) {
	    XTestFakeKeyEvent(dpy, left_shift_code, dn, CurrentTime);
	}
	if ( altgr_code && (mod_state & ALTGR) && mod != 2 ) {
	    XTestFakeKeyEvent(dpy, altgr_code, !dn, CurrentTime);
	}
	if ( altgr_code && ! (mod_state & ALTGR) && mod == 2 ) {
	    XTestFakeKeyEvent(dpy, altgr_code, dn, CurrentTime);
	}
	X_UNLOCK
}

static void modifier_tweak_keyboard(Bool down, KeySym keysym, rfbClientPtr client) {
	KeyCode k;
	int tweak = 0;

	if (view_only) {
		return;
	}

#define ADJUSTMOD(sym, state) \
	if (keysym == sym) { \
		if (down) { \
			mod_state |= state; \
		} else { \
			mod_state &= ~state; \
		} \
	}

	ADJUSTMOD(XK_Shift_L, LEFTSHIFT)
	ADJUSTMOD(XK_Shift_R, RIGHTSHIFT)
	ADJUSTMOD(XK_Mode_switch, ALTGR)

	if ( down && keysym >= ' ' && keysym < 0x100 ) {
		tweak = 1;
		tweak_mod(modifiers[keysym], True);
		k = keycodes[keysym];
	} else {
		X_LOCK
		k = XKeysymToKeycode(dpy, (X_KeySym) keysym);
		X_UNLOCK
	}
	if ( k != NoSymbol ) {
		X_LOCK
		XTestFakeKeyEvent(dpy, k, (X_Bool) down, CurrentTime);
		X_UNLOCK
	}

	if ( tweak ) {
		tweak_mod(modifiers[keysym], False);
	}
}

/* key event handler */
static void keyboard(Bool down, KeySym keysym, rfbClientPtr client) {
	KeyCode k;

	/* fprintf(stderr,"keyboard(%s,%s(0x%x),client)\n",
	   down?"down":"up",XKeysymToString(keysym),(int)keysym); */

	if (view_only) {
		return;
	}
	
	if (use_modifier_tweak) {
		modifier_tweak_keyboard(down, keysym, client);
		return;
	}

	X_LOCK

	k = XKeysymToKeycode(dpy, (X_KeySym) keysym);

	if ( k != NoSymbol ) {
		XTestFakeKeyEvent(dpy, k, (X_Bool) down, CurrentTime);
		XFlush(dpy);

		got_user_input++;
	}

	X_UNLOCK
}

/* mouse event handler */
static void pointer(int mask, int x, int y, rfbClientPtr client) {
	int i;

	if (view_only) {
		return;
	}

	X_LOCK

	XTestFakeMotionEvent(dpy, 0, x, y, CurrentTime);

	got_user_input++;

	for (i=0; i < 5; i++) {
		if ( (button_mask & (1<<i)) != (mask & (1<<i)) ) {
			XTestFakeButtonEvent(dpy, i+1, 
			    (mask & (1<<i)) ? True : False, CurrentTime);
		}
	}
	X_UNLOCK

	/* remember the button state for next time: */
	button_mask = mask;
}

/* simple fixed cursor */
static char* cur_data =
"                  "
"                  "
"  x               "
"  xx              "
"  xxx             "
"  xxxx            "
"  xxxxx           "
"  xxxxxx          "
"  xxxxxxx         "
"  xxxxxxxx        "
"  xxxxx           "
"  xx xx           "
"  x   xx          "
"      xx          "
"       xx         "
"       xx         "
"                  "
"                  ";

static char* cur_mask =
"                  "
" xx               "
" xxx              "
" xxxx             "
" xxxxx            "
" xxxxxx           "
" xxxxxxx          "
" xxxxxxxx         "
" xxxxxxxxx        "
" xxxxxxxxxx       "
" xxxxxxxxxx       "
" xxxxxxx          "
" xxx xxxx         "
" xx  xxxx         "
"      xxxx        "
"      xxxx        "
"       xx         "
"                  ";
int cursor_x = 18;
int cursor_y = 18;

void initialize_screen(int *argc, char **argv, XImage *fb) {

	screen = rfbGetScreen(argc, argv, fb->width, fb->height,
	    fb->bits_per_pixel, 8, fb->bits_per_pixel/8);

	screen->paddedWidthInBytes = fb->bytes_per_line;
	screen->rfbServerFormat.bitsPerPixel = fb->bits_per_pixel;
	screen->rfbServerFormat.depth = fb->depth;
	screen->rfbServerFormat.trueColour = (CARD8) TRUE;

	if ( screen->rfbServerFormat.bitsPerPixel == 8 ) {
		/* 8 bpp */
		if(CellsOfScreen(ScreenOfDisplay(dpy,scr))) {
			/* indexed colour */
			XColor color[256];
			int i;
			screen->colourMap.count = 256;
			screen->rfbServerFormat.trueColour = FALSE;
			screen->colourMap.is16 = TRUE;
			for(i=0;i<256;i++)
				color[i].pixel=i;
			XQueryColors(dpy,DefaultColormap(dpy,scr),color,256);
			screen->colourMap.data.shorts = (unsigned short*)malloc(3*sizeof(short)*screen->colourMap.count);
			for(i=0;i<screen->colourMap.count;i++) {
				screen->colourMap.data.shorts[i*3+0] = color[i].red;
				screen->colourMap.data.shorts[i*3+1] = color[i].green;
				screen->colourMap.data.shorts[i*3+2] = color[i].blue;
			}
		} else {
			/* true colour */
			screen->rfbServerFormat.redShift   = 0;
			screen->rfbServerFormat.greenShift = 2;
			screen->rfbServerFormat.blueShift  = 5;
			screen->rfbServerFormat.redMax     = 3;
			screen->rfbServerFormat.greenMax   = 7;
			screen->rfbServerFormat.blueMax    = 3;
		}	
	} else {
		/* general case ... */
		screen->rfbServerFormat.redShift = 0;
		if ( fb->red_mask ) {
			while ( ! (fb->red_mask
			    & (1 << screen->rfbServerFormat.redShift) ) ) {
				    screen->rfbServerFormat.redShift++;
			}
		}
		screen->rfbServerFormat.greenShift = 0;
		if ( fb->green_mask ) {
			while ( ! (fb->green_mask
			    & (1 << screen->rfbServerFormat.greenShift) ) ) {
				    screen->rfbServerFormat.greenShift++;
			}
		}
		screen->rfbServerFormat.blueShift = 0;
		if ( fb->blue_mask ) {
			while ( ! (fb->blue_mask
			    & (1 << screen->rfbServerFormat.blueShift) ) ) {
				    screen->rfbServerFormat.blueShift++;
			}
		}
		screen->rfbServerFormat.redMax
		    = fb->red_mask >> screen->rfbServerFormat.redShift;
		screen->rfbServerFormat.greenMax
		    = fb->green_mask >> screen->rfbServerFormat.greenShift;
		screen->rfbServerFormat.blueMax
		    = fb->blue_mask >> screen->rfbServerFormat.blueShift;
	}

	screen->frameBuffer = fb->data; 

	/* XXX the following 3 settings are based on libvncserver defaults. */
	if (screen->rfbPort == 5900) {
		screen->autoPort = TRUE;
	}
	if (screen->rfbDeferUpdateTime == 5) {
		screen->rfbDeferUpdateTime = defer_update;
	}
	if (shared && ! screen->rfbNeverShared) {
		screen->rfbAlwaysShared = TRUE;
	}

	/* event callbacks: */
	screen->newClientHook = new_client;
	screen->kbdAddEvent = keyboard;
	screen->ptrAddEvent = pointer;

	cursor = rfbMakeXCursor(cursor_x, cursor_y, cur_data, cur_mask);
	screen->cursor = cursor;

	rfbInitServer(screen);

	bytes_per_line = screen->paddedWidthInBytes;
	bpp = screen->rfbServerFormat.bitsPerPixel;
}

/*
 * setup tile numbers and allocate the tile and hint arrays:
 */
void initialize_tiles() {

	ntiles_x = (dpy_x - 1)/tile_x + 1;
	ntiles_y = (dpy_y - 1)/tile_y + 1;
	ntiles = ntiles_x * ntiles_y;

	tile_has_diff = (char *) malloc((size_t) (ntiles * sizeof(char)));
	tile_tried    = (char *) malloc((size_t) (ntiles * sizeof(char)));
	tile_region = (region_t *) malloc((size_t) (ntiles * sizeof(region_t)));

	/* there will never be more hints than tiles: */
	hint_list = (hint_t *) malloc((size_t) (ntiles * sizeof(hint_t)));
}

/*
 * silly function to factor dpy_y until fullscreen shm is not bigger than max.
 * should always work unless dpy_y is a large prime or something... under
 * failure fs_factor remains 0 and no fullscreen updates will be tried.
 */
void set_fs_factor(int max) {
	int f, fac = 1, n = dpy_y;

	if ( (bpp/8) * dpy_x * dpy_y <= max )  {
		fs_factor = 1;
		return;
	}
	for (f=2; f <= 101; f++) {
		while (n % f == 0) {
			n = n / f;
			fac = fac * f;
			if ( (bpp/8) * dpy_x * (dpy_y/fac) <= max )  {
				fs_factor = fac;
				return;
			}
		}
	}
}

void initialize_shm() {

	/* the tile (e.g. 32x32) shared memory area image: */

	tile = XShmCreateImage(dpy, DefaultVisual(dpy, 0), bpp, ZPixmap,
	    NULL, &tile_shm, tile_x, tile_y);

	tile_shm.shmid = shmget(IPC_PRIVATE,
	    tile->bytes_per_line * tile->height, IPC_CREAT | 0777);

	tile_shm.shmaddr = tile->data = (char *) shmat(tile_shm.shmid, 0, 0);
	tile_shm.readOnly = False;

	XShmAttach(dpy, &tile_shm);


	/* the scanline (e.g. 1280x1) shared memory area image: */

	scanline = XShmCreateImage(dpy, DefaultVisual(dpy, 0), bpp, ZPixmap,
	    NULL, &scanline_shm, dpy_x, 1);

	scanline_shm.shmid = shmget(IPC_PRIVATE,
	    scanline->bytes_per_line * scanline->height, IPC_CREAT | 0777);

	scanline_shm.shmaddr = scanline->data
	    = (char *) shmat(scanline_shm.shmid, 0, 0);
	scanline_shm.readOnly = False;

	XShmAttach(dpy, &scanline_shm);

	/*
	 * the fullscreen (e.g. 1280x1024/fs_factor) shared memory area image:
	 * (we cut down the size of the shm area to try avoid and shm segment
	 * limits, e.g. the default 1MB on Solaris)
	 */
	set_fs_factor(1024 * 1024);
	if (! fs_factor) {
		printf("warning: fullscreen updates are disabled.\n");
		return;
	}

	fullscreen = XShmCreateImage(dpy, DefaultVisual(dpy, 0), bpp, ZPixmap,
	    NULL, &fullscreen_shm, dpy_x, dpy_y/fs_factor);

	fullscreen_shm.shmid = shmget(IPC_PRIVATE,
	    fullscreen->bytes_per_line * fullscreen->height, IPC_CREAT | 0777);

	fullscreen_shm.shmaddr = fullscreen->data
	    = (char *) shmat(fullscreen_shm.shmid, 0, 0);
	fullscreen_shm.readOnly = False;

	XShmAttach(dpy, &fullscreen_shm);
}

/*
 * A hint is a rectangular region built from 1 or more adjacent tiles
 * glued together.  Ultimately, this information in a single hint is sent
 * to libvncserver rather than sending each tile separately.
 */
void create_tile_hint(int x, int y, int th, hint_t *hint) {
	int w = dpy_x - x;
	int h = dpy_y - y;

	if (w > tile_x) {
		w = tile_x;
	}
	if (h > th) {
		h = th;
	}

	hint->x = x;
	hint->y = y;
	hint->w = w;
	hint->h = h;
}

void extend_tile_hint(int x, int y, int th, hint_t *hint) {
	int w = dpy_x - x;
	int h = dpy_y - y;

	if (w > tile_x) {
		w = tile_x;
	}
	if (h > th) {
		h = th;
	}

	if (hint->x > x) {			/* extend to the left */
		hint->w += hint->x - x;
		hint->x = x;
	}
	if (hint->y > y) {			/* extend upward */
		hint->h += hint->y - y;
		hint->y = y;
	}

	if (hint->x + hint->w < x + w) {	/* extend to the right */
		hint->w = x + w - hint->x;
	}
	if (hint->y + hint->h < y + h) {	/* extend downward */
		hint->h = y + h - hint->y;
	}
}

void save_hint(hint_t hint, int loc) {
	/* copy it to the global array: */

	hint_list[loc].x = hint.x;
	hint_list[loc].y = hint.y;
	hint_list[loc].w = hint.w;
	hint_list[loc].h = hint.h;
}


/*
 * Glue together horizontal "runs" of adjacent changed tiles into one big
 * rectangle change "hint" to be passed to the vnc machinery.
 */
void hint_updates() {
	hint_t hint;
	int x, y, i, n, ty, th;
	int hint_count = 0, in_run = 0;

	for (y=0; y < ntiles_y; y++) {
		for (x=0; x < ntiles_x; x++) {
			n = x + y * ntiles_x;

			if (tile_has_diff[n]) {
				ty = tile_region[n].first_line;
				th = tile_region[n].last_line - ty + 1;
				if (! in_run) {
					create_tile_hint( x * tile_x,
					    y * tile_y + ty, th, &hint);
					in_run = 1;
				} else {
					extend_tile_hint( x * tile_x,
					    y * tile_y + ty, th, &hint);
				}
			} else {
				if (in_run) {
					/* end of a row run of altered tiles: */
					save_hint(hint, hint_count++);
					in_run = 0;
				}
			}
		}
		if (in_run) {	/* save the last row run */
			save_hint(hint, hint_count++);
			in_run = 0;
		}
	}

	for (i=0; i < hint_count; i++) {
		/* pass update info to vnc: */
		mark_hint(hint_list[i]);
	}
}

/*
 * Notifies libvncserver of a changed hint rectangle.
 */
void mark_hint(hint_t hint) {
	int x = hint.x;	
	int y = hint.y;	
	int w = hint.w;	
	int h = hint.h;	

	rfbMarkRectAsModified(screen, x, y, x + w, y + h);
}

/*
 * Notifies libvncserver of a changed tile rectangle.
 */
void mark_tile(int x, int y, int height) {
	int w = dpy_x - x;
	int h = dpy_y - y;

	if (w > tile_x) {
		w = tile_x;
	}

	/* height is the height of the changed portion of the tile */
	if (h > height) {
		h = height;
	}

	rfbMarkRectAsModified(screen, x, y, x + w, y + h);
}

/*
 * Simply send each modified tile separately to the vnc machinery:
 * (i.e. no hints)
 */
void tile_updates() {
	int x, y, n, ty, th;

	for (y=0; y < ntiles_y; y++) {
		for (x=0; x < ntiles_x; x++) {
			n = x + y * ntiles_x;

			if (tile_has_diff[n]) {
				ty = tile_region[n].first_line;
				th = tile_region[n].last_line - ty + 1;

				mark_tile(x * tile_x, y * tile_y + ty, th);
			}
		}
	}
}

/*
 * copy_tile() is called on a tile with a known change (from a scanline
 * diff) or a suspected change (from our various heuristics).
 *
 * Examine the whole tile for the y-range of difference, copy that
 * image difference to the vnc framebuffer, and do bookkeepping wrt
 * the y-range and edge differences.
 *
 * This call is somewhat costly, maybe 1-2 ms.  Primarily the XShmGetImage
 * and then the memcpy/memcmp.
 */
void copy_tile(int tx, int ty) {
	int x, y, line, first_line, last_line;
	int size_x, size_y, n, dw, dx;
	int pixelsize = bpp >> 3;
	short l_diff = 0, r_diff = 0;

	char *src, *dst, *s_src, *s_dst, *m_src, *m_dst;
	char *h_src, *h_dst;

	x = tx * tile_x;
	y = ty * tile_y;

	size_x = dpy_x - x;
	if ( size_x > tile_x ) {
		size_x = tile_x;
	}
	size_y = dpy_y - y;
	if ( size_y > tile_y ) {
		size_y = tile_y;
	}
	n = tx + ty * ntiles_x;		/* number of the tile */

	X_LOCK
	if ( size_x == tile_x && size_y == tile_y ) {
		/* general case: */
		XShmGetImage(dpy, window, tile, x, y, AllPlanes);
	} else {
		/*
		 * near bottom or rhs edge case:
		 * (but only if tile size does not divide screen size)
		 */
		XGetSubImage(dpy, window, x, y, size_x, size_y, AllPlanes,
		    ZPixmap, tile, 0, 0);
	}
	X_UNLOCK

	src = tile->data;
	dst = screen->frameBuffer + y * bytes_per_line + x * pixelsize;

	s_src = src;
	s_dst = dst;

	first_line = -1;

	/* find the first line with difference: */
	for (line = 0; line < size_y; line++) {
		if ( memcmp(s_dst, s_src, size_x * pixelsize) ) {
			first_line = line;
			break;
		}
		s_src += tile->bytes_per_line;
		s_dst += bytes_per_line;
	}

	tile_tried[n] = 1;
	if (first_line == -1) {
		/* tile has no difference, note it and get out: */
		tile_has_diff[n] = 0;
		return;
	} else {
		/*
		 * make sure it is recorded (e.g. sometimes we guess tiles
		 * and they came in with tile_has_diff 0)
		 */
		tile_has_diff[n] = 1;
	}

	m_src = src + (tile->bytes_per_line * size_y);
	m_dst = dst + (bytes_per_line * size_y);
	last_line = first_line;

	/* find the last line with difference: */
	for (line = size_y - 1; line > first_line; line--) {
		m_src -= tile->bytes_per_line;
		m_dst -= bytes_per_line;
		if ( memcmp(m_dst, m_src, size_x * pixelsize) ) {
			last_line = line;
			break;
		}
	}

	/* look for differences on left and right hand edges: */
	dx = (size_x - tile_fuzz) * pixelsize;
	dw = tile_fuzz * pixelsize; 

	h_src = src;
	h_dst = dst;
	for (line = 0; line < size_y; line++) {
		if (! l_diff && memcmp(h_dst, h_src, dw) ) {
			l_diff = 1;
		}
		if (! r_diff && memcmp(h_dst + dx, h_src + dx, dw) ) {
			r_diff = 1;
		}
		if (l_diff && r_diff) {
			break;
		}
		h_src += tile->bytes_per_line;
		h_dst += bytes_per_line;
	}

	/* now copy the difference to the vnc framebuffer: */
	for (line = first_line; line <= last_line; line++) {
		memcpy(s_dst, s_src, size_x * pixelsize);
		s_src += tile->bytes_per_line;
		s_dst += bytes_per_line;
	}

	/* record all the info in the region array for this tile: */
	tile_region[n].first_line = first_line;
	tile_region[n].last_line  = last_line;
	tile_region[n].left_diff  = l_diff;
	tile_region[n].right_diff = r_diff;

	tile_region[n].top_diff = 0;
	tile_region[n].bot_diff = 0;
	if ( first_line < tile_fuzz ) {
		tile_region[n].top_diff = 1;
	}
	if ( last_line > (size_y - 1) - tile_fuzz ) {
		tile_region[n].bot_diff = 1;
	}
}

/*
 * The copy_tile() call in the loop below copies the changed tile into
 * the vnc framebuffer.  Note that copy_tile() sets the tile_region
 * struct to have info about the y-range of the changed region and also
 * whether the tile edges contain diffs (within distance tile_fuzz).
 *
 * We use this tile_region info to try to guess if the downward and right
 * tiles will have diffs.  These tiles will be checked later in the loop
 * (since y+1 > y and x+1 > x).
 *
 * See copy_tiles_backward_pass() for analogous checking upward and
 * left tiles.
 */
void copy_all_tiles() {
	int x, y, n, m;

	for (y=0; y < ntiles_y; y++) {
		for (x=0; x < ntiles_x; x++) {
			n = x + y * ntiles_x;

			if (tile_has_diff[n]) {
				copy_tile(x, y);
			}
			if (! tile_has_diff[n]) {
				/*
				 * n.b. copy_tile() may have detected
				 * no change and reset tile_has_diff to 0.
				 */
				continue;
			}

			/* neighboring tile downward: */
			if ( (y+1) < ntiles_y && tile_region[n].bot_diff) {
				m = x + (y+1) * ntiles_x;
				if (! tile_has_diff[m]) {
					tile_has_diff[m] = 1;
				}
			}
			/* neighboring tile to right: */
			if ( (x+1) < ntiles_x && tile_region[n].right_diff) {
				m = (x+1) + y * ntiles_x;
				if (! tile_has_diff[m]) {
					tile_has_diff[m] = 1;
				}
			}
		}
	}
}

/*
 * Here starts a bunch of heuristics to guess/detect changed tiles.
 * They are:
 *   copy_tiles_backward_pass, fill_tile_gaps/gap_try, grow_islands/island_try
 * They are of varying utility... and perhaps some should be dropped.
 */

/*
 * Try to predict whether the upward and/or leftward tile has been modified.
 * copy_all_tiles() has already done downward and rightward tiles.
 */
void copy_tiles_backward_pass() {
	int x, y, n, m;

	for (y = ntiles_y - 1; y >= 0; y--) {
	    for (x = ntiles_x - 1; x >= 0; x--) {
		n = x + y * ntiles_x;		/* number of this tile */

		if (! tile_has_diff[n]) {
			continue;
		}

		m = x + (y-1) * ntiles_x;	/* neighboring tile upward */

		if (y >= 1 && ! tile_has_diff[m] && tile_region[n].top_diff) {
			if (! tile_tried[m]) {
				copy_tile(x, y-1);
			}
		}

		m = (x-1) + y * ntiles_x;	/* neighboring tile to left */

		if (x >= 1 && ! tile_has_diff[m] && tile_region[n].left_diff) {
			if (! tile_tried[m]) {
				copy_tile(x-1, y);
			}
		}
	    }
	}
}

void gap_try(int x, int y, int *run, int *saw, int along_x) {
	int n, m, i, xt, yt;

	n = x + y * ntiles_x;

	if (! tile_has_diff[n]) {
		if (*saw) {
			(*run)++;	/* extend the gap run. */
		}
		return;
	}
	if (! *saw || *run == 0 || *run > gaps_fill) {
		*run = 0;		/* unacceptable run. */
		*saw = 1;
		return;
	}

	for (i=1; i <= *run; i++) {	/* iterate thru the run. */
		if (along_x) {
			xt = x - i;
			yt = y;
		} else {
			xt = x;
			yt = y - i;
		}

		m = xt + yt * ntiles_x;
		if (tile_tried[m]) {	/* do not repeat tiles */
			continue;
		}

		copy_tile(xt, yt);
	}
	*run = 0;
	*saw = 1;
}

/*
 * Look for small gaps of unchanged tiles that may actually contain changes.
 * E.g. when paging up and down in a web broswer or terminal there can
 * be a distracting delayed filling in of such gaps.  gaps_fill is the
 * tweak parameter that sets the width of the gaps that are checked.
 *
 * btw, grow_islands() is actually pretty successful at doing this too.
 */
void fill_tile_gaps() {
	int x, y, run, saw;

	/* horizontal: */
	for (y=0; y < ntiles_y; y++) {
		run = 0;
		saw = 0;
		for (x=0; x < ntiles_x; x++) {
			gap_try(x, y, &run, &saw, 1);
		}
	}

	/* vertical: */
	for (x=0; x < ntiles_x; x++) {
		run = 0;
		saw = 0;
		for (y=0; y < ntiles_y; y++) {
			gap_try(x, y, &run, &saw, 0);
		}
	}
}

void island_try(int x, int y, int u, int v, int *run) {
	int n, m;

	n = x + y * ntiles_x;
	m = u + v * ntiles_x;

	if (tile_has_diff[n]) {
		(*run)++;
	} else {
		*run = 0;
	}

	if (tile_has_diff[n] && ! tile_has_diff[m]) {
		/* found discontinuity */

		if (tile_tried[m]) {
			return;
		} else if (*run < grow_fill) {
			return;
		}

		copy_tile(u, v);
	}
}

/*
 * Scan looking for discontinuities in tile_has_diff[].  Try to extend
 * the boundary of the discontinuity (i.e. make the island larger).
 * Vertical scans are skipped since they do not seem to yield much...
 */
void grow_islands() {
	int x, y, run;

	/*
	 * n.b. the way we scan here should keep an extension going,
	 * and so also fill in gaps effectively...
	 */

	/* left to right: */
	for (y=0; y < ntiles_y; y++) {
		run = 0;
		for (x=0; x <= ntiles_x - 2; x++) {
			island_try(x, y, x+1, y, &run);
		}
	}
	/* right to left: */
	for (y=0; y < ntiles_y; y++) {
		run = 0;
		for (x = ntiles_x - 1; x >= 1; x--) {
			island_try(x, y, x-1, y, &run);
		}
	}
}

/*
 * copy the whole X screen to the vnc framebuffer.  For a large enough
 * number of changed tiles, this is faster than tiles scheme at retrieving
 * the info from the X server.  Bandwidth to client is another issue...
 * use -fs 1.0 to disable.
 */
void copy_screen() {
	int pixelsize = bpp >> 3;
	char *vnc_fb;
	int i, y, block_size, xi;

	block_size = (dpy_x * (dpy_y/fs_factor) * pixelsize);

	vnc_fb = screen->frameBuffer;
	y = 0;

	for (i=0; i < fs_factor; i++) {
		xi = XShmGetImage(dpy, window, fullscreen, 0, y, AllPlanes);
		memcpy(vnc_fb, fullscreen->data, (size_t) block_size);

		y += dpy_y / fs_factor;
		vnc_fb += block_size;
	}

	rfbMarkRectAsModified(screen, 0, 0, dpy_x, dpy_y);
}

/*
 * Loop over 1-pixel tall horizontal scanlines looking for changes.  
 * Record the changes in tile_has_diff[].  Scanlines in the loop are
 * equally spaced along y by NSCAN pixels, but have a slightly random
 * starting offset ystart ( < NSCAN ) from scanlines[].
 */
int scan_display(int ystart, int rescan) {
	int x, y, w, n;
	int tile_count = 0;
	int pixelsize = bpp >> 3;
	char *src, *dst;

	y = ystart;

	while (y < dpy_y) {

		/* grab the horizontal scanline from the display: */
		X_LOCK
		XShmGetImage(dpy, window, scanline, 0, y, AllPlanes);
		X_UNLOCK

		x = 0;
		while (x < dpy_x) {
			n = (x/tile_x) + (y/tile_y) * ntiles_x;

			if (rescan && tile_has_diff[n]) {
				tile_count++;
				x += NSCAN;
				continue;
			}

			/* set ptrs to correspond to the x offset: */
			src = scanline->data + x * pixelsize;
			dst = screen->frameBuffer + y * bytes_per_line
			    + x * pixelsize;

			/* compute the width of data to be compared: */
			if ( x + NSCAN > dpy_x ) {
				w = dpy_x - x;
			} else {
				w = NSCAN;
			}

			if (memcmp(dst, src, w * pixelsize) ) {
				/* found a difference, record it: */
				tile_has_diff[n] = 1;
				tile_count++;		
			}
			x += NSCAN;
		}
		y += NSCAN;
	}
	return tile_count;
}

/*
 * toplevel for the scanning, rescanning, and applying the heuristics.
 */
void scan_for_updates() {
	int i, tile_count;
	double frac1 = 0.1;   /* tweak parameter to try a 2nd scan_display() */

	for (i=0; i < ntiles; i++) {
		tile_has_diff[i] = 0;
		tile_tried[i] = 0;
	}

	/*
	 * n.b. this program has only been tested so far with
	 * tile_x = tile_y = NSCAN = 32!
	 */

	count++;
	count %= NSCAN;

	/* scan with the initial y to the jitter value from scanlines: */
	tile_count = scan_display( scanlines[count], 0 );

	if (fs_factor && frac1 >= fs_frac) {
		/* make frac1 < fs_frac if fullscreen updates are enabled */
		frac1 = fs_frac/2.0;
	}

	if ( tile_count > frac1 * ntiles) {
		/*
		 * many tiles have changed, so try a rescan (since it should
		 * be short compared to the many upcoming copy_tile() calls)
		 */

		/* this check is done to skip the extra scan_display() call */
		if (! fs_factor || tile_count <= fs_frac * ntiles) {
			int cp;
			
			/* choose a different y shift for the 2nd scan: */
			cp = (NSCAN - count) % NSCAN;

			tile_count = scan_display( scanlines[cp], 1 );
		}

		/*
		 * At some number of changed tiles it is better to just
		 * copy the full screen at once.  I.e. time = c1 + m * r1
		 * where m is number of tiles and c1 is the scan_display()
		 * time: for some m it crosses the full screen update time.
		 *
		 * We try to predict that crossover with the fs_frac fudge
		 * factor... seems to be about 1/2 the total number
		 * of tiles.  n.b. this ignores network bandwidth, etc.
		 * use -fs 1.0 to disable on slow links.
		 */
		if (fs_factor && tile_count > fs_frac * ntiles) {
			copy_screen();
			return;
		}
	}

	/* copy all tiles with differences from display to vnc framebuffer: */
	copy_all_tiles();

	/*
	 * This backward pass for upward and left tiles complements what
	 * was done in copy_all_tiles() for downward and right tiles.
	 */
	copy_tiles_backward_pass();

	if (grow_fill) {
		grow_islands();
	}

	if (gaps_fill) {
		fill_tile_gaps();
	}

	if (use_hints) {
		hint_updates();	/* use krfb/x0rfbserver hints algorithm */
	} else {
		tile_updates();	/* send each tile change individually */
	}
}

void watch_loop(void) {
	int cnt = 0;

#if !defined(HAVE_PTHREADS)
	use_threads = 0;
#endif

	if (use_threads) {
		rfbRunEventLoop(screen, -1, TRUE);
	}

	while (1) {
		got_user_input = 0;

		if (! use_threads) {
			rfbProcessEvents(screen, -1);

			if (got_user_input && cnt % 10 != 0) {
				/* every 10-th drops thru to code below... */
				XFlush(dpy);
				continue;
			}
		}

		if (shut_down) {
			clean_up_exit();
		}

		if (! screen->rfbClientHead) {	/* waiting for a client */
			usleep(200 * 1000);
			continue;
		}

		rfbUndrawCursor(screen);
		scan_for_updates();

		usleep(waitms * 1000);

		cnt++;
	}
}

void print_help() {
	char help[] = 
"x11vnc options:\n"
"\n"
"-defer time            time in ms to wait for updates before sending to\n"
"                       client [rfbDeferUpdateTime]  (default %d)\n"
"-wait time             time in ms to pause between screen polls.  used\n"
"                       to cut down on load (default %d)\n"
"\n"
"-gaps n                heuristic to fill in gaps in rows or cols of n or less\n"
"                       tiles.  used to improve text paging (default %d).\n"
"-grow n                heuristic to grow islands of changed tiles n or wider\n"
"                       by checking the tile near the boundary (default %d).\n"
"-fs f                  if the fraction of changed tiles in a poll is greater\n"
"                       than f, the whole screen is updated (default %.2f)\n"
"-fuzz n                tolerance in pixels to mark a tiles edges as changed.\n"
"                       (default %d).\n"
"-hints                 use krfb/x0rfbserver hints (glue changed adjacent\n"
"                       horizontal tiles into one big rectangle)  (default %s).\n"
"-nohints               do not use hints; send each tile separately.\n"
"\n"
"-modtweak              handle AltGr/Shift modifiers for differing languages\n"
"                       between client and host (default %d).\n"
"-nomodtweak            send the keysym directly to the X server.\n"
"-threads               use threaded algorithm [rfbRunEventLoop] if compiled\n"
"                       with threads (default %s).\n"
"-nothreads             do not use [rfbRunEventLoop].\n"
"-viewonly              clients can only watch (default %s).\n"
"-shared                VNC display is shared (default %s)\n"
"\n"
"These options are passed to libvncserver:\n"
"\n"
;
	fprintf(stderr, help, defer_update, waitms, gaps_fill, grow_fill,
	    fs_frac, tile_fuzz,
	    use_hints ? "on":"off", use_modifier_tweak ? "on":"off",
	    use_threads ? "on":"off", view_only ? "on":"off",
	    shared ? "on":"off");
	rfbUsage();
	exit(1);
}

int main(int argc, char** argv) {

	XImage *fb;
	int i, ev, er, maj, min;
	char *use_dpy = NULL;


	/* used to pass args we do not know about to rfbGetScreen(): */
	int argc2 = 1; char *argv2[100];
	argv2[0] = argv[0];
	
	for (i=1; i < argc; i++) {
		if (!strcmp(argv[i], "-display")) {
			use_dpy = argv[++i];
		} else if (!strcmp(argv[i], "-defer")) {
			defer_update = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "-wait")) {
			waitms = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "-gaps")) {
			gaps_fill = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "-grow")) {
			grow_fill = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "-fs")) {
			fs_frac = atof(argv[++i]);
		} else if (!strcmp(argv[i], "-fuzz")) {
			tile_fuzz = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "-hints")) {
			use_hints = 1;
		} else if (!strcmp(argv[i], "-nohints")) {
			use_hints = 0;
		} else if (!strcmp(argv[i], "-threads")) {
			use_threads = 1;
		} else if (!strcmp(argv[i], "-nothreads")) {
			use_threads = 0;
		} else if (!strcmp(argv[i], "-modtweak")) {
			use_modifier_tweak = 1;
		} else if (!strcmp(argv[i], "-nomodtweak")) {
			use_modifier_tweak = 0;
		} else if (!strcmp(argv[i], "-viewonly")) {
			view_only = 1;
		} else if (!strcmp(argv[i], "-shared")) {
			shared = 1;
		} else if (!strcmp(argv[i], "-h")
		    || !strcmp(argv[i], "-help")) {
			print_help();
		} else {
			/* otherwise copy it for use below. */
			printf("passing arg to libvncserver: %s\n", argv[i]);
			if (argc2 < 100) {
				argv2[argc2++] = argv[i];
			}
		}
	}
	if (tile_fuzz < 1) {
		tile_fuzz = 1;
	}
	if (waitms < 0) {
		waitms = 0;
	}
	printf("defer:      %d\n", defer_update);
	printf("waitms:     %d\n", waitms);
	printf("tile_fuzz:  %d\n", tile_fuzz);
	printf("gaps_fill:  %d\n", gaps_fill);
	printf("grow_fill:  %d\n", grow_fill);
	printf("fs_frac:    %.2f\n", fs_frac);
	printf("use_hints:  %d\n", use_hints);
	printf("viewonly:   %d\n", view_only);
	printf("shared:     %d\n", shared);

	if (use_dpy) {
		dpy = XOpenDisplay(use_dpy);
	} else if ( (use_dpy = getenv("DISPLAY")) ) {
		dpy = XOpenDisplay(use_dpy);
	} else {
		dpy = XOpenDisplay("");
	}
	if (! dpy) {
		printf("XOpenDisplay failed (%s)\n", use_dpy);
		exit(1);
	} else if (use_dpy) {
		printf("Using display %s\n", use_dpy);
	} else {
		printf("Using default display.\n");
	}

	if (! XTestQueryExtension(dpy, &ev, &er, &maj, &min)) {
		printf("Display does not support the XTest extension.\n");
		exit(1);
	}
	if (! XShmQueryExtension(dpy)) {
		printf("Display does not support XShm extension"
		    " (must be local).\n");
		exit(1);
	}

	/*
	 * Window managers will often grab the display during resize, etc.
	 * To avoid deadlock (our user resize input is not processed)
	 * we tell the server to process our requests during all grabs:
	 */
	XTestGrabControl(dpy, True);

	scr = DefaultScreen(dpy);
	window = RootWindow(dpy, scr);

	dpy_x = DisplayWidth(dpy, scr);
	dpy_y = DisplayHeight(dpy, scr);

	fb = XGetImage(dpy, window, 0, 0, dpy_x, dpy_y, AllPlanes, ZPixmap);
	printf("Read initial data from display into framebuffer.\n");

	if (fb->bits_per_pixel == 24) {
		printf("warning: 24 bpp may have poor performance.\n");
	}

	/*
	 * n.b. we do not have to X_LOCK X11 calls until watch_loop()
	 * is called since we are single-threaded until then.
	 */

	initialize_screen(&argc2, argv2, fb);

	initialize_tiles();

	initialize_shm();

	if (use_modifier_tweak) {
		initialize_keycodes();
	}

	printf("screen setup finished.\n");

	watch_loop();

	return(0);
}
