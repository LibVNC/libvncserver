/*
 * x11vnc.c: a VNC server for X displays.
 *
 * Copyright (c) 2002-2003 Karl J. Runge <runge@karlrunge.com>
 * All rights reserved.
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
 *
 * Obtain the libvncserver package (http://libvncserver.sourceforge.net).
 * As of 12/2002 this version of x11vnc.c is contained in the libvncserver
 * CVS tree and released in version 0.5.  For earlier releases (say
 * libvncserver-0.4) this file may be inserted in place of the original
 * x11vnc.c file.
 *
 * gcc should be used on all platforms.  To build a threaded version put
 * "-D_REENTRANT -DX11VNC_THREADED" in the environment variable CFLAGS
 * or CPPFLAGS (e.g. before running configure).  The threaded mode is a 
 * bit more responsive, but can be unstable.
 *
 * Known shortcomings:
 *
 * The screen updates are good, but of course not perfect since the X
 * display must be continuously polled and read for changes (as opposed to
 * receiving a change callback from the X server, if that were generally
 * possible...).  So, e.g., opaque moves and similar window activity
 * can be very painful; one has to modify one's behavior a bit.
 *
 * It currently cannot capture XBell beeps (impossible?)  And, of course,
 * general audio at the remote display is lost as well unless one separately
 * sets up some audio side-channel.
 *
 * Windows using visuals other than the default X visual may have their
 * colors messed up.  When using 8bpp indexed color, the colormap may
 * become out of date (as the colormap is added to) or incorrect.
 *
 * It does not appear possible to query the X server for the current
 * cursor shape.  We can use XTest to compare cursor to current window's
 * cursor, but we cannot extract what the cursor is...  
 * 
 * Nevertheless, the current *position* of the remote X mouse pointer
 * is shown with the -mouse option.  Further, if -mouseX or -X is used, a
 * trick is done to at least show the root window cursor vs non-root cursor.
 * (perhaps some heuristic can be done to further distinguish cases...)
 *
 * With -mouse there are occasionally some repainting errors involving
 * big areas near the cursor.  The mouse painting is in general a bit
 * ragged and not very pleasant.
 *
 * Occasionally, a few tile updates can be missed leaving a patch of
 * color that needs to be refreshed.
 *
 * There seems to be a serious bug with simultaneous clients when
 * threaded, currently the only workaround in this case is -nothreads.
 *
 */

#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

#include <rfb/rfb.h>

/* X and rfb framebuffer */
Display *dpy = 0;
Visual *visual;
Window window, rootwin;
int subwin = 0;
int scr;
int bpp;
int button_mask = 0;
int dpy_x, dpy_y;
int off_x, off_y;
int indexed_colour = 0;

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
unsigned char *tile_has_diff, *tile_tried;

typedef struct tile_change_region {
	/* start and end lines, along y, of the changed area inside a tile. */
	unsigned short first_line, last_line;
	/* info about differences along edges. */
	unsigned short left_diff, right_diff;
	unsigned short top_diff,  bot_diff;
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

/* various command line options */

int shared = 0;			/* share vnc display. */
int view_only = 0;		/* clients can only watch. */
int connect_once = 1;		/* disconnect after first connection session. */
int flash_cmap = 0;		/* follow installed colormaps */

int use_modifier_tweak = 0;	/* use the altgr_keyboard modifier tweak */

int local_cursor = 1;		/* whether the viewer draws a local cursor */
int show_mouse = 0;		/* display a cursor for the real mouse */
int show_root_cursor = 0;	/* show X when on root background */

/*
 * waitms is the msec to wait between screen polls.  Not too old h/w shows
 * poll times of 10-35ms, so maybe this value cuts the idle load by 2 or so.
 */
int waitms = 30;
int defer_update = 30;	/* rfbDeferUpdateTime ms to wait before sends. */

int screen_blank = 60;	/* number of seconds of no activity to throttle */
			/* down the screen polls.  zero to disable. */
int take_naps = 0;
int naptile = 3;	/* tile change threshold per poll to take a nap */
int napfac = 4;		/* time = napfac*waitms, cut load with extra waits */
int napmax = 1500;	/* longest nap in ms. */

int nap_ok = 0, nap_diff_count = 0;
time_t last_event, last_input;

/* tile heuristics: */
double fs_frac = 0.6;	/* threshold tile fraction to do fullscreen updates. */
int use_hints = 1;	/* use the krfb scheme of gluing tiles together. */
int tile_fuzz = 2;	/* tolerance for suspecting changed tiles touching */
			/* a known changed tile. */
int grow_fill = 3;	/* do the grow islands heuristic with this width. */
int gaps_fill = 4;	/* do a final pass to try to fill gaps between tiles. */

/* scan pattern jitter from x0rfbserver */
#define NSCAN 32
int scanlines[NSCAN] = {
	 0, 16,  8, 24,  4, 20, 12, 28,
	10, 26, 18,  2, 22,  6, 30, 14,
	 1, 17,  9, 25,  7, 23, 15, 31,
	19,  3, 27, 11, 29, 13,  5, 21
};
int count = 0;			/* indicates which scan pattern we are on  */

int cursor_x, cursor_y;		/* x and y from the viewer(s) */
int got_user_input = 0;
int shut_down = 0;	

#if defined(LIBVNCSERVER_HAVE_LIBPTHREAD) && defined(LIBVNCSERVER_X11VNC_THREADED)
	int use_threads = 1;
#else
	int use_threads = 0;
#endif

/* XXX usleep(3) is not thread safe on some older systems... */
struct timeval _mysleep;
#define usleep2(x) \
	_mysleep.tv_sec  = (x) / 1000000; \
	_mysleep.tv_usec = (x) % 1000000; \
	select(0, NULL, NULL, NULL, &_mysleep); 
#if !defined(X11VNC_USLEEP)
#undef usleep
#define usleep usleep2
#endif

/*
 * Not sure why... but when threaded we have to mutex our X11 calls to
 * avoid XIO crashes.
 */
MUTEX(x11Mutex);
#define X_LOCK       LOCK(x11Mutex)
#define X_UNLOCK   UNLOCK(x11Mutex)
#define X_INIT INIT_MUTEX(x11Mutex)

/*
 * Exiting and error handling:
 */
void shm_clean(XShmSegmentInfo *, XImage *);
void shm_delete(XShmSegmentInfo *);

int exit_flag = 0;
void clean_up_exit (int ret) {
	exit_flag = 1;

	/* remove the shm areas: */
	shm_clean(&tile_shm, tile);
	shm_clean(&scanline_shm, scanline);
	shm_clean(&fullscreen_shm, fullscreen);

	X_LOCK;
	XTestDiscard(dpy);
	X_UNLOCK;

	exit(ret);
}

void interrupted (int sig) {
	if (exit_flag) {
		if (use_threads) {
			usleep2(250 * 1000);
		}
		exit(4);
	}
	exit_flag++;
	if (sig == 0) {
		printf("caught X11 error:\n");
	} else {
		printf("caught signal: %d\n", sig);
	}
	/*
	 * to avoid deadlock, etc, just delete the shm areas and
	 * leave the X stuff hanging.
	 */
	shm_delete(&tile_shm);
	shm_delete(&scanline_shm);
	shm_delete(&fullscreen_shm);
	if (sig) {
		exit(2);
	}
}

XErrorHandler   Xerror_def;
XIOErrorHandler XIOerr_def;
int Xerror(Display *d, XErrorEvent *error) {
	interrupted(0);
	return (*Xerror_def)(d, error);
}
int XIOerr(Display *d) {
	interrupted(0);
	return (*XIOerr_def)(d);
}

void set_signals(void) {
	signal(SIGHUP,  interrupted);
	signal(SIGINT,  interrupted);
	signal(SIGQUIT, interrupted);
	signal(SIGABRT, interrupted);
	signal(SIGTERM, interrupted);
	signal(SIGBUS,  interrupted);
	signal(SIGSEGV, interrupted);
	signal(SIGFPE,  interrupted);

	X_LOCK;
	Xerror_def = XSetErrorHandler(Xerror);
	XIOerr_def = XSetIOErrorHandler(XIOerr);
	X_UNLOCK;
}

void client_gone(rfbClientPtr client) {
	if (connect_once) {
		printf("viewer exited.\n");
		clean_up_exit(0);
	}
}

enum rfbNewClientAction new_client(rfbClientPtr client) {
	static client_count = 0;
	last_event = last_input = time(0);
	if (connect_once) {
		if (screen->rfbDontDisconnect && screen->rfbNeverShared) {
			if (! shared && client_count) {
				printf("denying additional client: %s\n",
				    client->host);
				return(RFB_CLIENT_REFUSE);
			}
		}
		client->clientGoneHook = client_gone;
	}
	if (view_only)  {
		client->clientData = (void *) -1;
	} else {
		client->clientData = (void *) 0;
	}
	client_count++;
	return(RFB_CLIENT_ACCEPT);
}

/*
 * For tweaking modifiers wrt the Alt-Graph key, etc.
 */
#define LEFTSHIFT 1
#define RIGHTSHIFT 2
#define ALTGR 4
char mod_state = 0;

char modifiers[0x100];
KeyCode keycodes[0x100], left_shift_code, right_shift_code, altgr_code;

void initialize_keycodes() {
	KeySym key, *keymap;
	int i, j, minkey, maxkey, syms_per_keycode;

	memset(modifiers, -1, sizeof(modifiers));

	XDisplayKeycodes(dpy, &minkey, &maxkey);

	keymap = XGetKeyboardMapping(dpy, minkey, (maxkey - minkey + 1),
	    &syms_per_keycode);

	/* handle alphabetic char with only one keysym (no upper + lower) */
	for (i = minkey; i <= maxkey; i++) {
		KeySym lower, upper;
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
	rfbLog("XTestFakeKeyEvent(dpy,%s(0x%x),%s,CurrentTime)\n",
	    XKeysymToString(XKeycodeToKeysym(dpy,keysym,0)),keysym,
	    down?"down":"up");
	XTestFakeKeyEvent(dpy,keysym,down,cur_time);
}

/* #define XTestFakeKeyEvent DebugXTestFakeKeyEvent */

void tweak_mod(signed char mod, rfbBool down) {
	rfbBool is_shift = mod_state & (LEFTSHIFT|RIGHTSHIFT);
	Bool dn = (Bool) down;

	if (mod < 0) {
		return;
	}

	X_LOCK;
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
	X_UNLOCK;
}

static void modifier_tweak_keyboard(rfbBool down, rfbKeySym keysym, rfbClientPtr client) {
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
		X_LOCK;
		k = XKeysymToKeycode(dpy, (KeySym) keysym);
		X_UNLOCK;
	}
	if ( k != NoSymbol ) {
		X_LOCK;
		XTestFakeKeyEvent(dpy, k, (Bool) down, CurrentTime);
		X_UNLOCK;
	}

	if ( tweak ) {
		tweak_mod(modifiers[keysym], False);
	}
}

/*
 * key event handler
 */
static void keyboard(rfbBool down, rfbKeySym keysym, rfbClientPtr client) {
	KeyCode k;

	if (0) {
		X_LOCK;
		rfbLog("keyboard(%s,%s(0x%x),client)\n",
		    down?"down":"up",XKeysymToString(keysym),(int)keysym);
		X_UNLOCK;
	}

	if (view_only) {
		return;
	}
	
	if (use_modifier_tweak) {
		modifier_tweak_keyboard(down, keysym, client);
		return;
	}

	X_LOCK;

	k = XKeysymToKeycode(dpy, (KeySym) keysym);

	if ( k != NoSymbol ) {
		XTestFakeKeyEvent(dpy, k, (Bool) down, CurrentTime);
		XFlush(dpy);

		last_event = last_input = time(0);
		got_user_input++;
	}

	X_UNLOCK;
}

/*
 * pointer event handler
 */
static void pointer(int mask, int x, int y, rfbClientPtr client) {
	int i;

	if (view_only) {
		return;
	}

	X_LOCK;

	XTestFakeMotionEvent(dpy, scr, x + off_x, y + off_y, CurrentTime);

	cursor_x = x;
	cursor_y = y;

	last_event = last_input = time(0);
	got_user_input++;

	for (i=0; i < 5; i++) {
		if ( (button_mask & (1<<i)) != (mask & (1<<i)) ) {
			XTestFakeButtonEvent(dpy, i+1, 
			    (mask & (1<<i)) ? True : False, CurrentTime);
		}
	}

	X_UNLOCK;

	/* remember the button state for next time: */
	button_mask = mask;
}

void mark_hint(hint_t);

/*
 * here begins a bit of a mess to experiment with multiple cursors ...
 */
typedef struct cursor_info {
	char *data;	/* data and mask pointers */
	char *mask;
	int wx, wy;	/* size of cursor */
	int sx, sy;	/* shift to its centering point */
	int reverse;	/* swap black and white */
} cursor_info_t;

/* main cursor */
static char* cur_data =
"                  "
" x                "
" xx               "
" xxx              "
" xxxx             "
" xxxxx            "
" xxxxxx           "
" xxxxxxx          "
" xxxxxxxx         "
" xxxxx            "
" xx xx            "
" x   xx           "
"     xx           "
"      xx          "
"      xx          "
"                  "
"                  "
"                  ";

static char* cur_mask =
"xx                "
"xxx               "
"xxxx              "
"xxxxx             "
"xxxxxx            "
"xxxxxxx           "
"xxxxxxxx          "
"xxxxxxxxx         "
"xxxxxxxxxx        "
"xxxxxxxxxx        "
"xxxxxxx           "
"xxx xxxx          "
"xx  xxxx          "
"     xxxx         "
"     xxxx         "
"      xx          "
"                  "
"                  ";
#define CUR_SIZE 18
#define CUR_DATA cur_data
#define CUR_MASK cur_mask
cursor_info_t cur0 = {NULL, NULL, CUR_SIZE, CUR_SIZE, 0, 0, 0};

/*
 * It turns out we can at least detect mouse is on the root window so 
 * show it (under -mouseX or -X) with this familiar cursor... 
 */
static char* root_data =
"                  "
"                  "
"  xxx        xxx  "
"  xxxx      xxxx  "
"  xxxxx    xxxxx  "
"   xxxxx  xxxxx   "
"    xxxxxxxxxx    "
"     xxxxxxxx     "
"      xxxxxx      "
"      xxxxxx      "
"     xxxxxxxx     "
"    xxxxxxxxxx    "
"   xxxxx  xxxxx   "
"  xxxxx    xxxxx  "
"  xxxx      xxxx  "
"  xxx        xxx  "
"                  "
"                  ";

static char* root_mask =
"                  "
" xxxx        xxxx "
" xxxxx      xxxxx "
" xxxxxx    xxxxxx "
" xxxxxxx  xxxxxxx "
"  xxxxxxxxxxxxxx  "
"   xxxxxxxxxxxx   "
"    xxxxxxxxxx    "
"     xxxxxxxx     "
"     xxxxxxxx     "
"    xxxxxxxxxx    "
"   xxxxxxxxxxxx   "
"  xxxxxxxxxxxxxx  "
" xxxxxxx  xxxxxxx "
" xxxxxx    xxxxxx "
" xxxxx      xxxxx "
" xxxx        xxxx "
"                  ";
cursor_info_t cur1 = {NULL, NULL, 18, 18, 8, 8, 1};

cursor_info_t *cursors[2];
void setup_cursors(void) {
	/* TODO clean this up if we ever do more cursors... */

	cur0.data = cur_data;
	cur0.mask = cur_mask;

	cur1.data = root_data;
	cur1.mask = root_mask;

	cursors[0] = &cur0;
	cursors[1] = &cur1;
}

/*
 * data and functions for -mouse real pointer position updates
 */
char cur_save[(4 * CUR_SIZE * CUR_SIZE)];
int cur_save_x, cur_save_y, cur_save_w, cur_save_h;
int cur_save_cx, cur_save_cy, cur_save_which, cur_saved = 0;

/*
 * save current cursor info and the patch of data it covers
 */
void save_mouse_patch(int x, int y, int w, int h, int cx, int cy, int which) {
	int pixelsize = bpp >> 3;
	char *rfb_fb = screen->frameBuffer;
	int ly, i = 0;

	for (ly = y; ly < y + h; ly++) {
		memcpy(cur_save+i, rfb_fb + ly * bytes_per_line
		    + x * pixelsize, w * pixelsize);

		i += w * pixelsize;
	}
	cur_save_x = x;		/* patch geometry */
	cur_save_y = y;
	cur_save_w = w;
	cur_save_h = h;

	cur_save_which = which;	/* which cursor and its position  */
	cur_save_cx = cx;
	cur_save_cy = cy;

	cur_saved = 1;
}

/*
 * put the non-cursor patch back in the rfb fb
 */
void restore_mouse_patch() {
	int pixelsize = bpp >> 3;
	char *rfb_fb = screen->frameBuffer;
	int ly, i = 0;

	if (! cur_saved) {
		return;		/* not yet saved */
	}

	for (ly = cur_save_y; ly < cur_save_y + cur_save_h; ly++) {
		memcpy(rfb_fb + ly * bytes_per_line + cur_save_x * pixelsize,
		    cur_save+i, cur_save_w * pixelsize);
		i += cur_save_w * pixelsize;
	}
}

/*
 * Descends windows at pointer until the window cursor matches the current 
 * cursor.  So far only used to detect if mouse is on root background or not.
 * (returns 0 in that case, 1 otherwise).
 *
 * It seems impossible to do, but if the actual cursor could ever be
 * determined we might want to hash that info on window ID or something...
 */
int tree_depth_cursor(void) {
	Window r, c;
	int rx, ry, wx, wy;
	unsigned int mask;
	int depth = 0, tries = 0, maxtries = 1;

	X_LOCK;
	c = window;
	while (c) {
		if (++tries > maxtries) {
			depth = maxtries;
			break;
		}
		if ( XTestCompareCurrentCursorWithWindow(dpy, c) ) {
			break;
		}
		XQueryPointer(dpy, c, &r, &c, &rx, &ry, &wx, &wy, &mask);
		depth++;
	}
	X_UNLOCK;
	return depth;
}

/*
 * draw one of the mouse cursors into the rfb fb
 */
void draw_mouse(int x, int y, int which, int update) {
	int px, py, i, offset;
	int pixelsize = bpp >> 3;
	char *rfb_fb = screen->frameBuffer;
	char cdata, cmask;
	char *data, *mask;
	int white = 255, black = 0, shade;
	int x0, x1, x2, y0, y1, y2;
	int cur_x, cur_y, cur_sx, cur_sy, reverse;
	static int first = 1;

	if (first) {
		first = 0;
		setup_cursors();
	}

	data	= cursors[which]->data;		/* pattern data */
	mask	= cursors[which]->mask;
	cur_x	= cursors[which]->wx;		/* widths */
	cur_y	= cursors[which]->wy;
	cur_sx	= cursors[which]->sx;		/* shifts */
	cur_sy	= cursors[which]->sy;
	reverse	= cursors[which]->reverse;	/* reverse video */

	if (reverse) {
		black = white;
		white = 0;
	}

	/*
	 * notation:
	 *   x0, y0: position after cursor shift (no edge corrections)
	 *   x1, y1: corrected for lower boundary < 0
	 *   x2, y2: position + cursor width and corrected for upper boundary
	 */

	x0 = x1 = x - cur_sx;		/* apply shift */
	if (x1 < 0) x1 = 0;

	y0 = y1 = y - cur_sy;
	if (y1 < 0) y1 = 0;

	x2 = x0 + cur_x;		/* apply width for upper endpoints */
	if (x2 >= dpy_x) x2 = dpy_x - 1;

	y2 = y0 + cur_y;
	if (y2 >= dpy_y) y2 = dpy_y - 1;

	/* save the patch and info about which cursor will overwrite it */
	save_mouse_patch(x1, y1, x2 - x1, y2 - y1, x, y, which);

	for (py = 0; py < cur_y; py++) {
		if (y0 + py < 0 || y0 + py >= dpy_y) {
			continue;		/* off screen */
		}
		for (px = 0; px < cur_x; px++) {
			if (x0 + px < 0 || x0 + px >= dpy_x){
				continue;	/* off screen */
			}
			cdata = data[px + py * cur_x];
			cmask = mask[px + py * cur_x];

			if (cmask != 'x') {
				continue;	/* transparent */
			}

			shade = white;
			if (cdata != cmask)  {
				shade = black;
			}

			offset = (y0 + py)*bytes_per_line + (x0 + px)*pixelsize;

			/* fill in each color byte in the fb */
			for (i=0; i < pixelsize; i++) {
				rfb_fb[offset+i] = (char) shade;
			}
		}
	}

	if (update) {
		/* x and y of the real (X server) mouse */
		static int mouse_x = -1;
		static int mouse_y = -1;

		if (x != mouse_x || y != mouse_y) { 
			hint_t hint;

			hint.x = x1;
			hint.y = y2;
			hint.w = x2 - x1;
			hint.h = y2 - y1;

			mark_hint(hint);
			
			if (mouse_x < 0) {
				mouse_x = 0;
			}
			if (mouse_y < 0) {
				mouse_y = 0;
			}

			/* XXX this ignores change of shift... */
			x1 = mouse_x - cur_sx;
			if (x1 < 0) x1 = 0;

			y1 = mouse_y - cur_sy;
			if (y1 < 0) y1 = 0;

			x2 = mouse_x - cur_sx + cur_x;
			if (x2 >= dpy_x) x2 = dpy_x - 1;

			y2 = mouse_y - cur_sy + cur_y;
			if (y2 >= dpy_y) y2 = dpy_y - 1;

			hint.x = x1;
			hint.y = y2;
			hint.w = x2 - x1;
			hint.h = y2 - y1;

			mark_hint(hint);

			mouse_x = x;
			mouse_y = y;
		}
	}
}

void redraw_mouse(void) {
	if (cur_saved) {
		/* redraw saved mouse from info (save_mouse_patch) */
		draw_mouse(cur_save_cx, cur_save_cy, cur_save_which, 0);
	}
}

void update_mouse(void) {
	Window root_w, child_w;
	rfbBool ret;
	int root_x, root_y, win_x, win_y, which = 0;
	unsigned int mask;

	X_LOCK;
	ret = XQueryPointer(dpy, rootwin, &root_w, &child_w, &root_x, &root_y,
	    &win_x, &win_y, &mask);
	X_UNLOCK;

	if (! ret) {
		return;
	}

	if (show_root_cursor) {
		int depth;
		if ( (depth = tree_depth_cursor()) ) {
			which = 0;
		} else {
			which = 1;
		}
	}

	draw_mouse(root_x - off_x, root_y - off_y, which, 1);
}

void set_offset(void) {
	Window w;
	if (! subwin) {
		return;
	}
	X_LOCK;
	XTranslateCoordinates(dpy, window, rootwin, 0, 0, &off_x, &off_y, &w);
	X_UNLOCK;
}

#define NCOLOR 256
void set_colormap(void) {
	static int first = 1;
	static XColor color[NCOLOR], prev[NCOLOR];
	Colormap cmap;
	int i, diffs = 0;

	if (first) {
		screen->colourMap.count = NCOLOR;
		screen->rfbServerFormat.trueColour = FALSE;
		screen->colourMap.is16 = TRUE;
		screen->colourMap.data.shorts = (unsigned short*)
			malloc(3*sizeof(short) * NCOLOR);
	}

	for (i=0; i < NCOLOR; i++) {
		color[i].pixel = i;
		prev[i].red   = color[i].red;
		prev[i].green = color[i].green;
		prev[i].blue  = color[i].blue;
	}

	X_LOCK;

	cmap = DefaultColormap(dpy, scr);

	if (flash_cmap && ! first) {
		XWindowAttributes attr;
		Window r, c;
		int rx, ry, wx, wy, tries = 0;
		unsigned int m;

		c = window;
		while (c && tries++ < 16) {
			/* XXX this is a hack, XQueryTree probably better. */
			XQueryPointer(dpy, c, &r, &c, &rx, &ry, &wx, &wy, &m);
			if (c && XGetWindowAttributes(dpy, c, &attr)) {
				if (attr.colormap && attr.map_installed) {
					cmap = attr.colormap;
					break;
				}
			} else {
				break;
			}
		}
	}

	XQueryColors(dpy, cmap, color, NCOLOR);

	X_UNLOCK;

	for(i=0; i < NCOLOR; i++) {
		screen->colourMap.data.shorts[i*3+0] = color[i].red;
		screen->colourMap.data.shorts[i*3+1] = color[i].green;
		screen->colourMap.data.shorts[i*3+2] = color[i].blue;

		if (prev[i].red   != color[i].red ||
		    prev[i].green != color[i].green || 
		    prev[i].blue  != color[i].blue ) {
			diffs++;
		}
	}

	if (diffs && ! first) {
		rfbSetClientColourMaps(screen, 0, NCOLOR);
	}

	first = 0;
}

/*
 * initialize the rfb framebuffer/screen
 */
void initialize_screen(int *argc, char **argv, XImage *fb) {

	screen = rfbGetScreen(argc, argv, fb->width, fb->height,
	    fb->bits_per_pixel, 8, fb->bits_per_pixel/8);

	screen->paddedWidthInBytes = fb->bytes_per_line;
	screen->rfbServerFormat.bitsPerPixel = fb->bits_per_pixel;
	screen->rfbServerFormat.depth = fb->depth;
	screen->rfbServerFormat.trueColour = (uint8_t) TRUE;

	if ( screen->rfbServerFormat.bitsPerPixel == 8
	    && CellsOfScreen(ScreenOfDisplay(dpy,scr)) ) {
		/* indexed colour */
		printf("using 8bpp indexed colour\n");
		indexed_colour = 1;
		set_colormap();
	} else {
		/* general case ... */
		printf("using %dbpp depth=%d true colour\n", fb->bits_per_pixel,
		    fb->depth);

		/* convert masks to bit shifts and max # colors */
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
	if (! screen->rfbNeverShared && ! screen->rfbAlwaysShared) {
		if (shared) {
			screen->rfbAlwaysShared = TRUE;
		} else {
			screen->rfbDontDisconnect = TRUE;
			screen->rfbNeverShared = TRUE;
		}
	}

	/* event callbacks: */
	screen->newClientHook = new_client;
	screen->kbdAddEvent = keyboard;
	screen->ptrAddEvent = pointer;

	if (local_cursor) {
		cursor = rfbMakeXCursor(CUR_SIZE, CUR_SIZE, CUR_DATA, CUR_MASK);
		screen->cursor = cursor;
	} else {
		screen->cursor = NULL;
	}

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

	tile_has_diff = (unsigned char *)
		malloc((size_t) (ntiles * sizeof(unsigned char)));
	tile_tried    = (unsigned char *)
		malloc((size_t) (ntiles * sizeof(unsigned char)));
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

/*
 * set up an XShm image
 */
void shm_create(XShmSegmentInfo *shm, XImage **ximg_ptr, int w, int h,
    char *name) {

	XImage *xim;

	X_LOCK;

	xim = XShmCreateImage(dpy, visual, bpp, ZPixmap, NULL, shm, w, h);

	if (xim == NULL) {
		rfbLog( "XShmCreateImage(%s) failed.\n", name);
		exit(1);
	}

	*ximg_ptr = xim;

	shm->shmid = shmget(IPC_PRIVATE,
	    xim->bytes_per_line * xim->height, IPC_CREAT | 0777);

	if (shm->shmid == -1) {
		rfbLog("shmget(%s) failed.\n", name);
		perror("shmget");
		exit(1);
	}

	shm->shmaddr = xim->data = (char *) shmat(shm->shmid, 0, 0);

	if (shm->shmaddr == (char *)-1) {
		rfbLog("shmat(%s) failed.\n", name);
		perror("shmat");
		exit(1);
	}

	shm->readOnly = False;

	if (! XShmAttach(dpy, shm)) {
		rfbLog("XShmAttach(%s) failed.\n", name);
		exit(1);
	}

	X_UNLOCK;
}

void shm_delete(XShmSegmentInfo *shm) {
	shmdt(shm->shmaddr);
	shmctl(shm->shmid, IPC_RMID, 0);
}

void shm_clean(XShmSegmentInfo *shm, XImage *xim) {
	X_LOCK;
	XShmDetach(dpy, shm);
	XDestroyImage(xim);
	X_UNLOCK;

	shm_delete(shm);
}

void initialize_shm() {

	/* the tile (e.g. 32x32) shared memory area image: */

	shm_create(&tile_shm, &tile, tile_x, tile_y, "tile"); 

	/* the scanline (e.g. 1280x1) shared memory area image: */

	shm_create(&scanline_shm, &scanline, dpy_x, 1, "scanline"); 

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

	shm_create(&fullscreen_shm, &fullscreen, dpy_x, dpy_y/fs_factor,
	    "fullscreen"); 

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
	hint_list[loc].x = hint.x;		/* copy to the global array */
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
 * image difference to the rfb framebuffer, and do bookkeepping wrt
 * the y-range and edge differences.
 *
 * This call is somewhat costly, maybe 1-2 ms.  Primarily the XShmGetImage
 * and then the memcpy/memcmp.
 */

void copy_tile(int tx, int ty) {
	int x, y, line, first_line, last_line;
	int size_x, size_y, n, dw, dx;
	int pixelsize = bpp >> 3;
	unsigned short l_diff = 0, r_diff = 0;

	int restored_patch = 0; /* for show_mouse */

	char *src, *dst, *s_src, *s_dst, *m_src, *m_dst;
	char *h_src, *h_dst;

	x = tx * tile_x;
	y = ty * tile_y;

	size_x = dpy_x - x;
	if ( size_x > tile_x ) size_x = tile_x;

	size_y = dpy_y - y;
	if ( size_y > tile_y ) size_y = tile_y;

	n = tx + ty * ntiles_x;		/* number of the tile */

	X_LOCK;
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
	X_UNLOCK;

	/*
	 * Some awkwardness wrt the little remote mouse patch we display.
	 * When threaded we want to have as small a window of time
	 * as possible when the mouse image is not in the fb, otherwise
	 * a libvncserver thread may send the uncorrected patch to the
	 * clients.
	 */
	if (show_mouse && use_threads && cur_saved) {
		/* check for overlap */
		if (cur_save_x + cur_save_w > x && x + size_x > cur_save_x &&
		    cur_save_y + cur_save_h > y && y + size_y > cur_save_y) {

			/* restore the real data to the rfb fb */
			restore_mouse_patch();
			restored_patch = 1;
		}
	}

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
		if (restored_patch) {
			redraw_mouse(); 
		}
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

	/* now copy the difference to the rfb framebuffer: */
	for (line = first_line; line <= last_line; line++) {
		memcpy(s_dst, s_src, size_x * pixelsize);
		s_src += tile->bytes_per_line;
		s_dst += bytes_per_line;
	}

	if (restored_patch) {
		redraw_mouse(); 
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
 * the rfb framebuffer.  Note that copy_tile() sets the tile_region
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
int copy_all_tiles() {
	int x, y, n, m;
	int diffs = 0;

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
			diffs++;

			/* neighboring tile downward: */
			if ( (y+1) < ntiles_y && tile_region[n].bot_diff) {
				m = x + (y+1) * ntiles_x;
				if (! tile_has_diff[m]) {
					tile_has_diff[m] = 2;
				}
			}
			/* neighboring tile to right: */
			if ( (x+1) < ntiles_x && tile_region[n].right_diff) {
				m = (x+1) + y * ntiles_x;
				if (! tile_has_diff[m]) {
					tile_has_diff[m] = 2;
				}
			}
		}
	}
	return diffs;
}

/*
 * Here starts a bunch of heuristics to guess/detect changed tiles.
 * They are:
 *   copy_tiles_backward_pass, fill_tile_gaps/gap_try, grow_islands/island_try
 */

/*
 * Try to predict whether the upward and/or leftward tile has been modified.
 * copy_all_tiles() has already done downward and rightward tiles.
 */
int copy_tiles_backward_pass() {
	int x, y, n, m;
	int diffs = 0;

	for (y = ntiles_y - 1; y >= 0; y--) {
	    for (x = ntiles_x - 1; x >= 0; x--) {
		n = x + y * ntiles_x;		/* number of this tile */

		if (! tile_has_diff[n]) {
			continue;
		}

		m = x + (y-1) * ntiles_x;	/* neighboring tile upward */

		if (y >= 1 && ! tile_has_diff[m] && tile_region[n].top_diff) {
			if (! tile_tried[m]) {
				tile_has_diff[m] = 2;
				copy_tile(x, y-1);
			}
		}

		m = (x-1) + y * ntiles_x;	/* neighboring tile to left */

		if (x >= 1 && ! tile_has_diff[m] && tile_region[n].left_diff) {
			if (! tile_tried[m]) {
				tile_has_diff[m] = 2;
				copy_tile(x-1, y);
			}
		}
	    }
	}
	for (n=0; n < ntiles; n++) {
		if (tile_has_diff[n]) {
			diffs++;
		}
	}
	return diffs;
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
 * BTW, grow_islands() is actually pretty successful at doing this too.
 */
int fill_tile_gaps() {
	int x, y, run, saw;
	int n, diffs = 0;

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

	for (n=0; n < ntiles; n++) {
		if (tile_has_diff[n]) {
			diffs++;
		}
	}
	return diffs;
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
		/* found a discontinuity */

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
int grow_islands() {
	int x, y, n, run;
	int diffs = 0;

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
	for (n=0; n < ntiles; n++) {
		if (tile_has_diff[n]) {
			diffs++;
		}
	}
	return diffs;
}

/*
 * copy the whole X screen to the rfb framebuffer.  For a large enough
 * number of changed tiles, this is faster than tiles scheme at retrieving
 * the info from the X server.  Bandwidth to client and compression time
 * are other issues...  use -fs 1.0 to disable.
 */
void copy_screen() {
	int pixelsize = bpp >> 3;
	char *rfb_fb;
	int i, y, block_size, xi;

	block_size = (dpy_x * (dpy_y/fs_factor) * pixelsize);

	rfb_fb = screen->frameBuffer;
	y = 0;

	X_LOCK;

	for (i=0; i < fs_factor; i++) {
		xi = XShmGetImage(dpy, window, fullscreen, 0, y, AllPlanes);
		memcpy(rfb_fb, fullscreen->data, (size_t) block_size);

		y += dpy_y / fs_factor;
		rfb_fb += block_size;
	}

	X_UNLOCK;

	rfbMarkRectAsModified(screen, 0, 0, dpy_x, dpy_y);
}

/*
 * Utilities for managing the "naps" to cut down on amount of polling
 */
void nap_set(int tile_cnt) {

	if (count == 0) {
		/* roll up check for all NSCAN scans */
		nap_ok = 0;
		if (naptile && nap_diff_count < 2 * NSCAN * naptile) {
			/* "2" is a fudge to permit a bit of bg drawing */
			nap_ok = 1;
		}
		nap_diff_count = 0;
	}

	if (show_mouse) {
		/* kludge for the up to 4 tiles the mouse patch could occupy */
		if ( tile_cnt > 4) {
			last_event = time(0);
		}
	} else if (tile_cnt != 0) {
		last_event = time(0);
	}
}

void nap_sleep(int ms, int split) {
	int i, input = got_user_input;

	/* split it up to improve the wakeup time */
	for (i=0; i<split; i++) {
		usleep(ms * 1000 / split);
		if (! use_threads && i != split - 1) {
			rfbProcessEvents(screen, -1);
		}
		if (input != got_user_input) {
			break;
		}
	}
}

void nap_check(int tile_cnt) {
	time_t now;

	nap_diff_count += tile_cnt;

	if (! take_naps) {
		return;
	}

	now = time(0);

	if (screen_blank > 0) {
		int dt = (int) (now - last_event);
		int ms = 1500;

		/* if no activity, pause here for a second or so. */
		if (dt > screen_blank) {
			nap_sleep(ms, 8);
			return;
		}
	}
	if (naptile && nap_ok && tile_cnt < naptile) {
		int ms = napfac * waitms;
		ms = ms > napmax ? napmax : ms;
		if (now - last_input <= 2) {
			nap_ok = 0;
		} else {
			nap_sleep(ms, 1);
		}
	}
}

void ping_clients(int tile_cnt) {
	static time_t last_send = 0;
	time_t now = time(0);

	if (rfbMaxClientWait <= 3000) {
		rfbMaxClientWait = 3000;
		printf("reset rfbMaxClientWait to %d ms.\n", rfbMaxClientWait);
	}
	if (tile_cnt) {
		last_send = now;
	} else if (now - last_send > 1) {
		/* Send small heartbeat to client */
		rfbMarkRectAsModified(screen, 0, 0, 1, 1);
		last_send = now;
	}
}

/*
 * Loop over 1-pixel tall horizontal scanlines looking for changes.  
 * Record the changes in tile_has_diff[].  Scanlines in the loop are
 * equally spaced along y by NSCAN pixels, but have a slightly random
 * starting offset ystart ( < NSCAN ) from scanlines[].
 */
int scan_display(int ystart, int rescan) {
	char *src, *dst;
	int pixelsize = bpp >> 3;
	int x, y, w, n;
	int tile_count = 0;
	int whole_line = 1, nodiffs;

	y = ystart;

	while (y < dpy_y) {

		/* grab the horizontal scanline from the display: */
		X_LOCK;
		XShmGetImage(dpy, window, scanline, 0, y, AllPlanes);
		X_UNLOCK;

		/* for better memory i/o try the whole line at once */
		src = scanline->data;
		dst = screen->frameBuffer + y * bytes_per_line;

		nodiffs = 0;
		if (whole_line && ! memcmp(dst, src, bytes_per_line)) {
			/* no changes anywhere in scan line */
			nodiffs = 1;
			if (! rescan) {
				y += NSCAN;
				continue;
			}
		}

		x = 0;
		while (x < dpy_x) {
			n = (x/tile_x) + (y/tile_y) * ntiles_x;

			if (rescan) {
				if (nodiffs || tile_has_diff[n]) {
					tile_count += tile_has_diff[n];
					x += NSCAN;
					continue;
				}
			}

			/* set ptrs to correspond to the x offset: */
			src = scanline->data + x * pixelsize;
			dst = screen->frameBuffer + y * bytes_per_line
			    + x * pixelsize;

			/* compute the width of data to be compared: */
			if (x + NSCAN > dpy_x) {
				w = dpy_x - x;
			} else {
				w = NSCAN;
			}

			if (memcmp(dst, src, w * pixelsize)) {
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
	int i, tile_count, tile_diffs;
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

	if (count % (NSCAN/4) == 0)  {
		if (subwin) {
			set_offset();	/* follow the subwindow */
		}
		if (indexed_colour) {	/* check for changed colormap */
			set_colormap();
		}
	}

	if (show_mouse && ! use_threads) {
		/* single-thread is safe to do it here for all scanning */
		restore_mouse_patch();
	}

	/* scan with the initial y to the jitter value from scanlines: */
	tile_count = scan_display( scanlines[count], 0 );

	nap_set(tile_count);

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
		 * where m is number of tiles, r1 is the copy_tile()
		 * time, and c1 is the scan_display() time: for some m
		 * it crosses the full screen update time.
		 *
		 * We try to predict that crossover with the fs_frac
		 * fudge factor... seems to be about 1/2 the total number
		 * of tiles.  n.b. this ignores network bandwidth,
		 * compression time etc...
		 *
		 * Use -fs 1.0 to disable on slow links.
		 */
		if (fs_factor && tile_count > fs_frac * ntiles) {
			copy_screen();
			if (show_mouse) {
				if (! use_threads) {
					redraw_mouse();
				}
				update_mouse();
			}
			nap_check(tile_count);
			return;
		}
	}

	/* copy all tiles with differences from display to rfb framebuffer: */
	tile_diffs = copy_all_tiles();

	/*
	 * This backward pass for upward and left tiles complements what
	 * was done in copy_all_tiles() for downward and right tiles.
	 */
	tile_diffs = copy_tiles_backward_pass();

	if (grow_fill && tile_diffs > 4) {
		tile_diffs = grow_islands();
	}

	if (gaps_fill && tile_diffs > 4) {
		tile_diffs = fill_tile_gaps();
	}

	if (use_hints) {
		hint_updates();	/* use krfb/x0rfbserver hints algorithm */
	} else {
		tile_updates();	/* send each tile change individually */
	}

	/* Work around threaded rfbProcessClientMessage() calls timeouts */
	if (use_threads) {
		ping_clients(tile_diffs);
	}

	/* Handle the remote mouse pointer */
	if (show_mouse) {
		if (! use_threads) {
			redraw_mouse();
		}
		update_mouse();
	}

	nap_check(tile_diffs);
}

void watch_loop(void) {
	int cnt = 0;

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
			clean_up_exit(0);
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
"\n"
"x11vnc options:\n"
"\n"
"-display disp          X11 server display to connect to, the X server process\n"
"                       must be running on same machine and support MIT-SHM.\n"
"-id windowid           show the window corresponding to <windowid> not the\n"
"                       entire display. Warning: bugs! new toplevels missed!...\n"
"-flashcmap             in 8bpp indexed color, let the installed colormap flash\n"
"                       as the pointer moves from window to window (slow).\n"
"\n"
"-viewonly              clients can only watch (default %s).\n"
"-shared                VNC display is shared (default %s).\n"
"-many                  keep listening for more connections rather than exiting\n"
"                       as soon as the first clients disconnect.\n"
"\n"
"-modtweak              handle AltGr/Shift modifiers for differing languages\n"
"                       between client and host (default %s).\n"
"-nomodtweak            send the keysym directly to the X server.\n"
"\n"
"-nocursor              do not have the viewer show a local cursor.\n"
"-mouse                 draw a 2nd cursor at the current X pointer position.\n"
"-mouseX                as -mouse, but also draw an X on root background.\n"
"-X                     shorthand for -mouseX -nocursor.\n"
"\n"
"-defer time            time in ms to wait for updates before sending to\n"
"                       client [rfbDeferUpdateTime]  (default %d).\n"
"-wait time             time in ms to pause between screen polls.  used\n"
"                       to cut down on load (default %d).\n"
"-nap                   monitor activity and if low take longer naps between\n" 
"                       polls to really cut down load when idle (default %s).\n"
#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
"-threads               whether or not to use the threaded libvncserver\n"
"-nothreads             algorithm [rfbRunEventLoop] (default %s).\n"
#endif
"\n"
"-fs f                  if the fraction of changed tiles in a poll is greater\n"
"                       than f, the whole screen is updated (default %.2f).\n"
"-gaps n                heuristic to fill in gaps in rows or cols of n or less\n"
"                       tiles.  used to improve text paging (default %d).\n"
"-grow n                heuristic to grow islands of changed tiles n or wider\n"
"                       by checking the tile near the boundary (default %d).\n"
"-fuzz n                tolerance in pixels to mark a tiles edges as changed\n"
"                       (default %d).\n"
"-hints                 use krfb/x0rfbserver hints (glue changed adjacent\n"
"                       horizontal tiles into one big rectangle)  (default %s).\n"
"-nohints               do not use hints; send each tile separately.\n"
"%s\n"
"\n"
"These options are passed to libvncserver:\n"
"\n"
;
	fprintf(stderr, help,
		view_only ? "on":"off",
		shared ? "on":"off",
		use_modifier_tweak ? "on":"off",
		defer_update,
		waitms,
		take_naps ? "on":"off",
#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
		use_threads ? "on":"off",
#endif
		fs_frac,
		gaps_fill,
		grow_fill,
		tile_fuzz,
		use_hints ? "on":"off",
		""
	);

	rfbUsage();
	exit(1);
}

/*
 * choose a desktop name
 */
#define MAXN 256
char *choose_title(char *display) {
	static char title[(MAXN+10)];	
	strcpy(title, "x11vnc");

	if (display == NULL) {
		display = getenv("DISPLAY");
	}
	if (display == NULL) {
		return title;
	}
	title[0] = '\0';
	if (display[0] == ':') {
		char host[MAXN];
		if (gethostname(host, MAXN) == 0) {
			strncpy(title, host, MAXN - strlen(title));
		}
	}
	strncat(title, display, MAXN - strlen(title));
	if (subwin) {
		char *name;
		if (XFetchName(dpy, window, &name)) {
			strncat(title, " ",  MAXN - strlen(title));
			strncat(title, name, MAXN - strlen(title));
		}
	}
	return title;
}

int main(int argc, char** argv) {

	XImage *fb;
	int i, ev, er, maj, min;
	char *use_dpy = NULL;
	int dt = 0;

	/* used to pass args we do not know about to rfbGetScreen(): */
	int argc2 = 1; char *argv2[100];

	argv2[0] = argv[0];
	
	for (i=1; i < argc; i++) {
		if (!strcmp(argv[i], "-display")) {
			use_dpy = argv[++i];
		} else if (!strcmp(argv[i], "-id")) {
			/* expt to just show one window. XXX not finished. */
			if (sscanf(argv[++i], "0x%x", &subwin) != 1) {
				if (sscanf(argv[i], "%d", &subwin) != 1) {
					printf("bad -id arg: %s\n", argv[i]);
					exit(1);
				}
			}
		} else if (!strcmp(argv[i], "-flashcmap")) {
			flash_cmap = 1;
		} else if (!strcmp(argv[i], "-viewonly")) {
			view_only = 1;
		} else if (!strcmp(argv[i], "-shared")) {
			shared = 1;
		} else if (!strcmp(argv[i], "-many")) {
			connect_once = 0;
		} else if (!strcmp(argv[i], "-modtweak")) {
			use_modifier_tweak = 1;
		} else if (!strcmp(argv[i], "-nomodtweak")) {
			use_modifier_tweak = 0;
		} else if (!strcmp(argv[i], "-nocursor")) {
			local_cursor = 0;
		} else if (!strcmp(argv[i], "-mouse")) {
			show_mouse = 1;
		} else if (!strcmp(argv[i], "-mouseX")) {
			show_mouse = 1;
			show_root_cursor = 1;
		} else if (!strcmp(argv[i], "-X")) {
			show_mouse = 1;
			show_root_cursor = 1;
			local_cursor = 0;
		} else if (!strcmp(argv[i], "-defer")) {
			defer_update = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "-wait")) {
			waitms = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "-nap")) {
			take_naps = 1;
#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
		} else if (!strcmp(argv[i], "-threads")) {
			use_threads = 1;
		} else if (!strcmp(argv[i], "-nothreads")) {
			use_threads = 0;
#endif
		} else if (!strcmp(argv[i], "-fs")) {
			fs_frac = atof(argv[++i]);
		} else if (!strcmp(argv[i], "-gaps")) {
			gaps_fill = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "-grow")) {
			grow_fill = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "-fuzz")) {
			tile_fuzz = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "-hints")) {
			use_hints = 1;
		} else if (!strcmp(argv[i], "-nohints")) {
			use_hints = 0;
		} else if (!strcmp(argv[i], "-h")
		    || !strcmp(argv[i], "-help")) {
			print_help();
		} else {
			if (!strcmp(argv[i], "-desktop")) {
				dt = 1;
			}
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
	printf("viewonly:   %d\n", view_only);
	printf("shared:     %d\n", shared);
	printf("conn_once:  %d\n", connect_once);
	printf("mod_tweak:  %d\n", use_modifier_tweak);
	printf("loc_curs:   %d\n", local_cursor);
	printf("mouse:      %d\n", show_mouse);
	printf("root_curs:  %d\n", show_root_cursor);
	printf("defer:      %d\n", defer_update);
	printf("waitms:     %d\n", waitms);
	printf("take_naps:  %d\n", take_naps);
	printf("threads:    %d\n", use_threads);
	printf("fs_frac:    %.2f\n", fs_frac);
	printf("gaps_fill:  %d\n", gaps_fill);
	printf("grow_fill:  %d\n", grow_fill);
	printf("tile_fuzz:  %d\n", tile_fuzz);
	printf("use_hints:  %d\n", use_hints);

	X_INIT;
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
	rootwin = RootWindow(dpy, scr);

	if (! subwin) {
		window = rootwin;
		dpy_x = DisplayWidth(dpy, scr);
		dpy_y = DisplayHeight(dpy, scr);
		off_x = 0;
		off_y = 0;
		visual = DefaultVisual(dpy, scr);
	} else {
		/* experiment to share just one window */
		XWindowAttributes attr;

		window = (Window) subwin;
		if ( ! XGetWindowAttributes(dpy, window, &attr) ) {
			printf("bad window: 0x%x\n", window);
			exit(1);
		}
		dpy_x = attr.width;
		dpy_y = attr.height;
		visual = attr.visual;

		/* show_mouse has some segv crashes as well */
		if (show_root_cursor) {
			show_root_cursor = 0;
			printf("disabling root cursor drawing for subwindow\n");
		}

		set_offset();
	}

	fb = XGetImage(dpy, window, 0, 0, dpy_x, dpy_y, AllPlanes, ZPixmap);
	printf("Read initial data from display into framebuffer.\n");

	if (fb->bits_per_pixel == 24) {
		printf("warning: 24 bpp may have poor performance.\n");
	}

	if (! dt) {
		static char str[] = "-desktop";
		argv2[argc2++] = str;
		argv2[argc2++] = choose_title(use_dpy);
	}

	/*
	 * n.b. we do not have to X_LOCK any X11 calls until watch_loop()
	 * is called since we are single-threaded until then.
	 */

	initialize_screen(&argc2, argv2, fb);

	initialize_tiles();

	initialize_shm();

	set_signals();

	if (use_modifier_tweak) {
		initialize_keycodes();
	}

	printf("screen setup finished.\n");

	watch_loop();

	return(0);
}
