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
 * CVS tree and released in version 0.5.
 *
 * gcc should be used on all platforms.  To build a threaded version put
 * "-D_REENTRANT -DX11VNC_THREADED" in the environment variable CFLAGS
 * or CPPFLAGS (e.g. before running the libvncserver configure).  The
 * threaded mode is a bit more responsive, but can be unstable.
 *
 * Known shortcomings:
 *
 * The screen updates are good, but of course not perfect since the X
 * display must be continuously polled and read for changes (as opposed to
 * receiving a change callback from the X server, if that were generally
 * possible...).  So, e.g., opaque moves and similar window activity
 * can be very painful; one has to modify one's behavior a bit.
 *
 * General audio at the remote display is lost unless one separately
 * sets up some audio side-channel.
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
 * Windows using visuals other than the default X visual may have
 * their colors messed up.  When using 8bpp indexed color, the colormap
 * is attempted to be followed, but may become out of date.  Use the
 * -flashcmap option to have colormap flashing as the pointer moves
 * windows with private colormaps (slow).  Displays with mixed depth 8 and
 * 24 visuals will incorrect display the non-default one.
 *
 * Feature -id <windowid> can be picky: it can crash for things like the
 * window not sufficiently mapped into server memory, use of -mouse, etc.
 * SaveUnders menus, popups, etc will not be seen.
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
#include <X11/Xatom.h>

#include <sys/stat.h>
#include <fcntl.h>

#include <rfb/rfb.h>
#include <rfb/rfbregion.h>

#ifdef LIBVNCSERVER_HAVE_XKEYBOARD
#include <X11/XKBlib.h>
#endif

/* 
 * Temporary kludge: to run with -xinerama define the following macro
 * and be sure to link with * -lXinerama (e.g. LDFLAGS=-lXinerama before
 * configure).  Support for this is being added to libvncserver 'configure.ac'
 * so it will all be done automatically.

#define LIBVNCSERVER_HAVE_LIBXINERAMA
 */
#ifdef LIBVNCSERVER_HAVE_LIBXINERAMA
#include <X11/extensions/Xinerama.h>
#endif


/* X and rfb framebuffer */
Display *dpy = 0;
Visual *visual;
Window window, rootwin;
int scr;
int bpp, depth;
int button_mask = 0;
int dpy_x, dpy_y;
int off_x, off_y;
int subwin = 0;
int indexed_colour = 0;

XImage *tile;
XImage **tile_row;		/* for all possible row runs */
XImage *scanline;
XImage *fullscreen;
int fs_factor = 0;

#ifdef SINGLE_TILE_SHM
XShmSegmentInfo tile_shm;
#endif
XShmSegmentInfo *tile_row_shm;	/* for all possible row runs */
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

/* blacked-out region things */
typedef struct bout {
	int x1, y1, x2, y2;
} blackout_t;
typedef struct tbout {
	blackout_t bo[10];	/* hardwired max rectangles. */
	int cover;
	int count;
} tile_blackout_t;
blackout_t black[100];		/* hardwired max blackouts */
int blackouts = 0;
tile_blackout_t *tile_blackout;


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
char *allow_list = NULL;	/* for -allow and -localhost */
int view_only = 0;		/* clients can only watch. */
int inetd = 0;			/* spawned from inetd(1) */
int connect_once = 1;		/* disconnect after first connection session. */
int flash_cmap = 0;		/* follow installed colormaps */
int force_indexed_color = 0;	/* whether to force indexed color for 8bpp */

int use_modifier_tweak = 0;	/* use the altgr_keyboard modifier tweak */
char *remap_file = NULL;	/* user supplied remapping file or list */
int nofb = 0;			/* do not send any fb updates */

char *blackout_string = NULL;	/* -blackout */
int xinerama = 0;		/* -xinerama */

char *client_connect = NULL;	/* strings for -connect option */
char *client_connect_file = NULL;
int vnc_connect = 0;		/* -vncconnect option */

int local_cursor = 1;		/* whether the viewer draws a local cursor */
int cursor_pos = 0;		/* cursor position updates -cursorpos */
int show_mouse = 0;		/* display a cursor for the real mouse */
int use_xwarppointer = 0;	/* use XWarpPointer instead of XTestFake... */
int show_root_cursor = 0;	/* show X when on root background */
int show_dragging = 1;		/* process mouse movement events */
int watch_bell = 1;		/* watch for the bell using XKEYBOARD */

int old_pointer = 0;		/* use the old way of updating the pointer */
int single_copytile = 0;	/* use the old way copy_tiles() */

int using_shm = 1;		/* whether mit-shm is used */
int flip_byte_order = 0;	/* sometimes needed when using_shm = 0 */
/*
 * waitms is the msec to wait between screen polls.  Not too old h/w shows
 * poll times of 10-35ms, so maybe this value cuts the idle load by 2 or so.
 */
int waitms = 30;
int defer_update = 30;	/* rfbDeferUpdateTime ms to wait before sends. */
int defer_update_nofb = 6;	/* defer a shorter time under -nofb */

int screen_blank = 60;	/* number of seconds of no activity to throttle */
			/* down the screen polls.  zero to disable. */
int take_naps = 0;
int naptile = 3;	/* tile change threshold per poll to take a nap */
int napfac = 4;		/* time = napfac*waitms, cut load with extra waits */
int napmax = 1500;	/* longest nap in ms. */
int ui_skip = 10;	/* see watchloop.  negative means ignore input */

/* for -visual override */
VisualID visual_id = (VisualID) 0;
int visual_depth = 0;

int nap_ok = 0, nap_diff_count = 0;
time_t last_event, last_input, last_client = 0;

/* tile heuristics: */
double fs_frac = 0.75;	/* threshold tile fraction to do fullscreen updates. */
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
int got_pointer_input = 0;
int got_keyboard_input = 0;
int scan_in_progress = 0;	
int fb_copy_in_progress = 0;	
int client_count = 0;
int shut_down = 0;	
int sigpipe = 1;		/* 0=skip, 1=ignore, 2=exit */

int debug_pointer = 0;
int debug_keyboard = 0;

int quiet = 0;
double dtime(double *);

void zero_fb(int, int, int, int);

#if defined(LIBVNCSERVER_X11VNC_THREADED) && ! defined(X11VNC_THREADED)
#define X11VNC_THREADED
#endif

#if defined(LIBVNCSERVER_HAVE_LIBPTHREAD) && defined(X11VNC_THREADED)
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
	int i;
	exit_flag = 1;

	/* remove the shm areas: */
#ifdef SINGLE_TILE_SHM
	shm_clean(&tile_shm, tile);
#endif
	shm_clean(&scanline_shm, scanline);
	shm_clean(&fullscreen_shm, fullscreen);

	for(i=1; i<=ntiles_x; i++) {
		shm_clean(&tile_row_shm[i], tile_row[i]);
		if (single_copytile && i >= single_copytile) {
			break;
		}
	}

	X_LOCK;
	XTestDiscard(dpy);
	X_UNLOCK;

	exit(ret);
}

/*
 * General problem handler
 */
void interrupted (int sig) {
	int i;
	if (exit_flag) {
		exit_flag++;
		if (use_threads) {
			usleep2(250 * 1000);
		} else if (exit_flag <= 2) {
			return;
		}
		exit(4);
	}
	exit_flag++;
	if (sig == 0) {
		fprintf(stderr, "caught X11 error:\n");
	} else {
		fprintf(stderr, "caught signal: %d\n", sig);
	}
	/*
	 * to avoid deadlock, etc, just delete the shm areas and
	 * leave the X stuff hanging.
	 */
#ifdef SINGLE_TILE_SHM
	shm_delete(&tile_shm);
#endif
	shm_delete(&scanline_shm);
	shm_delete(&fullscreen_shm);

	/* 
	 * Here we have to clean up quite a few shm areas for all
	 * the possible tile row runs (40 for 1280), not as robust
	 * as one might like... sometimes need to run ipcrm(1). 
	 */
	for(i=1; i<=ntiles_x; i++) {
		shm_delete(&tile_row_shm[i]);
		if (single_copytile && i >= single_copytile) {
			break;
		}
	}
	if (sig) {
		exit(2);
	}
}

XErrorHandler   Xerror_def;
XIOErrorHandler XIOerr_def;
int Xerror(Display *d, XErrorEvent *error) {
	X_UNLOCK;
	interrupted(0);
	return (*Xerror_def)(d, error);
}
int XIOerr(Display *d) {
	X_UNLOCK;
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

	if (sigpipe == 1) {
#ifdef SIG_IGN
		rfbLog("set_signals: ignoring SIGPIPE\n");
		signal(SIGPIPE, SIG_IGN);
#endif
	} else if (sigpipe == 2) {
		rfbLog("set_signals: will exit on SIGPIPE\n");
		signal(SIGPIPE, interrupted);
	}

	X_LOCK;
	Xerror_def = XSetErrorHandler(Xerror);
	XIOerr_def = XSetIOErrorHandler(XIOerr);
	X_UNLOCK;
}

void client_gone(rfbClientPtr client) {
	client_count--;
	rfbLog("client_count: %d\n", client_count);
	if (connect_once) {
		rfbLog("viewer exited.\n");
		clean_up_exit(0);
	}
}

/*
 * Simple routine to limit access via string compare.  A power user will
 * want to compile libvncserver with libwrap support and use /etc/hosts.allow.
 */
int check_access(char *addr) {
	int allowed = 0;
	char *p, *list;

	if (allow_list == NULL || *allow_list == '\0') {
		return 1;
	}
	if (addr == NULL || *addr == '\0') {
		rfbLog("check_access: denying empty host IP address string.\n");
		return 0;
	}

	list = strdup(allow_list);
	p = strtok(list, ",");
	while (p) {
		char *q = strstr(addr, p);
		if (q == addr) {
			rfbLog("check_access: client %s matches pattern %s\n",
			    addr, p);
			allowed = 1;

		} else if(!strcmp(p,"localhost") && !strcmp(addr,"127.0.0.1")) {
			allowed = 1;
		}
		p = strtok(NULL, ",");
	}
	free(list);
	return allowed;
}

/*
 * For the -connect <file> option: periodically read the file looking for
 * a connect string.  If one is found set client_connect to it.
 */
void check_connect_file(char *file) {
	FILE *in;
	char line[512], host[512];
	static int first_warn = 1, truncate_ok = 1;
	static time_t last_time = 0; 
	time_t now = time(0);

	if (now - last_time < 1) {
		/* check only once a second */
		return;
	}
	last_time = now;

	if (! truncate_ok) {
		/* check if permissions changed */
		if (access(file, W_OK) == 0) {
			truncate_ok = 1;
		} else {
			return;
		}
	}

	in = fopen(file, "r");
	if (in == NULL) {
		if (first_warn) {
			rfbLog("check_connect_file: fopen failure: %s\n", file);
			perror("fopen");
			first_warn = 0;
		}
		return;
	}

	if (fgets(line, 512, in) != NULL) {
		if (sscanf(line, "%s", host) == 1) {
			if (strlen(host) > 0) {
				client_connect = strdup(host);
				rfbLog("read connect file: %s\n", host);
			}
		}
	}
	fclose(in);

	/* truncate file */
	in = fopen(file, "w");
	if (in != NULL) {
		fclose(in);
	} else {
		/* disable if we cannot truncate */
		rfbLog("check_connect_file: could not truncate %s, "
		   "disabling checking.\n", file);
		truncate_ok = 0;
	}
}

/* Do a reverse connect for a single "host" or "host:port" */
int do_reverse_connect(char *str) {
	rfbClientPtr cl;
	char *host, *p;
	int port = 5500, len = strlen(str);

	if (len < 1) {
		return;
	}
	if (len > 512) {
		rfbLog("reverse_connect: string too long: %d bytes\n", len);
		return;
	}

	/* copy in to host */
	host = (char *) malloc((size_t) len+1);
	if (! host) {
		rfbLog("reverse_connect: could not malloc string %d\n", len);
		return;
	}
	strncpy(host, str, len);
	host[len] = '\0';

	/* extract port, if any */
	if ((p = strchr(host, ':')) != NULL) {
		port = atoi(p+1);
		*p = '\0';
	}

	cl = rfbReverseConnection(screen, host, port);
	free(host);

	if (cl == NULL) {
		rfbLog("reverse_connect: %s failed\n", str);
		return 0;
	} else {
		rfbLog("reverse_connect: %s/%s OK\n", str, cl->host);
		return 1;
	}
}

void rfbPE(rfbScreenInfoPtr scr, long us) {
	if (! use_threads) {
		return rfbProcessEvents(scr, us);
	}
}

/* break up comma separated list of hosts and call do_reverse_connect() */

void reverse_connect(char *str) {
	char *p, *tmp = strdup(str);
	int sleep_between_host = 300;
	int sleep_min = 1500, sleep_max = 4500, n_max = 5;
	int n, tot, t, dt = 100, cnt = 0;

	p = strtok(tmp, ",");
	while (p) {
		if ((n = do_reverse_connect(p)) != 0) {
			rfbPE(screen, -1);
		}
		cnt += n;

		p = strtok(NULL, ",");
		if (p) {
			t = 0;
			while (t < sleep_between_host) {
				usleep(dt * 1000);
				rfbPE(screen, -1);
				t += dt;
			}
		}
	}
	free(tmp);

	if (cnt == 0) {
		return;
	}

	/*
	 * XXX: we need to process some of the initial handshaking
	 * events, otherwise the client can get messed up (why??) 
	 * so we send rfbProcessEvents() all over the place.
	 */

	n = cnt;
	if (n >= n_max) {
		n = n_max; 
	} 
	t = sleep_max - sleep_min;
	tot = sleep_min + ((n-1) * t) / (n_max-1);

	t = 0;
	while (t < tot) {
		rfbPE(screen, -1);
		usleep(dt * 1000);
		t += dt;
	}
}

/* check if client_connect has been set, if so make the reverse connections. */

void send_client_connect() {
	if (client_connect != NULL) {
		reverse_connect(client_connect);
		free(client_connect);
		client_connect = NULL;
	}
}

/* string for the VNC_CONNECT property */
#define VNC_CONNECT_MAX 512
char vnc_connect_str[VNC_CONNECT_MAX+1];

/* monitor the various input methods */
void check_connect_inputs() {

	/* flush any already set: */
	send_client_connect();

	/* connect file: */
	if (client_connect_file != NULL) {
		check_connect_file(client_connect_file);		
	}
	send_client_connect();

	/* VNC_CONNECT property (vncconnect program) */
	if (vnc_connect && *vnc_connect_str != '\0') {
		client_connect = strdup(vnc_connect_str);
		vnc_connect_str[0] = '\0';
	}
	send_client_connect();
}

/*
 * libvncserver callback for when a new client connects
 */
enum rfbNewClientAction new_client(rfbClientPtr client) {
	static accepted_client = 0;
	last_event = last_input = time(0);

	if (! check_access(client->host)) {
		rfbLog("denying client: %s does not match %s\n", client->host,
		    allow_list ? allow_list : "(null)" );
		return(RFB_CLIENT_REFUSE);
	}
	if (connect_once) {
		if (screen->rfbDontDisconnect && screen->rfbNeverShared) {
			if (! shared && accepted_client) {
				rfbLog("denying additional client: %s\n",
				     client->host);
				return(RFB_CLIENT_REFUSE);
			}
		}
	}
	client->clientGoneHook = client_gone;
	if (view_only)  {
		client->clientData = (void *) -1;
	} else {
		client->clientData = (void *) 0;
	}
	accepted_client = 1;
	last_client = time(0);
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

void initialize_modtweak() {
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

/*
 * The following is for an experimental -remap option to allow the user
 * to remap keystrokes.  It is currently confusing wrt modifiers...
 */
typedef struct keyremap {
	KeySym before;
	KeySym after;
	struct keyremap *next;
} keyremap_t;

keyremap_t *keyremaps = NULL;

void initialize_remap(char *infile) {
	FILE *in;
	char *p, line[256], str1[256], str2[256];
	int i;
	KeySym ksym1, ksym2;
	keyremap_t *remap, *current;

	in = fopen(infile, "r"); 
	if (in == NULL) {
		/* assume cmd line key1:key2,key3:key4 */
		if (! strchr(infile, ':') || (in = tmpfile()) == NULL) {
			rfbLog("remap: cannot open: %s\n", infile);
			perror("fopen");
			clean_up_exit(1);
		}
		p = infile;
		while (*p) {
			if (*p == ':') {
				fprintf(in, " ");
			} else if (*p == ',') {
				fprintf(in, "\n");
			} else {
				fprintf(in, "%c", *p);
			}
			p++;
		}
		fprintf(in, "\n");
		fflush(in);	
		rewind(in);
	}
	while (fgets(line, 256, in) != NULL) {
		int blank = 1;
		p = line;
		while (*p) {
			if (! isspace(*p)) {
				blank = 0;
				break;
			}
			p++;
		}
		if (blank) {
			continue;
		}
		if (strchr(line, '#')) {
			continue;
		}
		
		if (sscanf(line, "%s %s", str1, str2) != 2) {
			rfbLog("remap: bad line: %s\n", line);
			fclose(in);
			clean_up_exit(1);
		}
		if (sscanf(str1, "0x%x", &i) == 1) {
			ksym1 = (KeySym) i;
		} else {
			ksym1 = XStringToKeysym(str1);
		}
		if (sscanf(str2, "0x%x", &i) == 1) {
			ksym2 = (KeySym) i;
		} else {
			ksym2 = XStringToKeysym(str2);
		}
		if (ksym1 == NoSymbol || ksym2 == NoSymbol) {
			rfbLog("warning: skipping bad remap line: %s", line);
			continue;
		}
		remap = (keyremap_t *) malloc((size_t) sizeof(keyremap_t));
		remap->before = ksym1;
		remap->after  = ksym2;
		remap->next   = NULL;
		rfbLog("remapping: (%s, 0x%x) -> (%s, 0x%x)\n", str1, ksym1,
		    str2, ksym2);
		if (keyremaps == NULL) {
			keyremaps = remap;
		} else {
			current->next = remap;
			
		}
		current = remap;
	}
	fclose(in);
}

void DebugXTestFakeKeyEvent(Display* dpy, KeyCode key, Bool down, time_t cur_time)
{
	if (debug_keyboard) {
		rfbLog("XTestFakeKeyEvent(dpy, keycode=0x%x \"%s\", %s)\n",
		    key, XKeysymToString(XKeycodeToKeysym(dpy, key, 0)),
		    down ? "down":"up");
	}
	XTestFakeKeyEvent(dpy, key, down, cur_time);
}

/*
 * This is to allow debug_keyboard option trap everything:
 */
#define XTestFakeKeyEvent DebugXTestFakeKeyEvent

void tweak_mod(signed char mod, rfbBool down) {
	rfbBool is_shift = mod_state & (LEFTSHIFT|RIGHTSHIFT);
	Bool dn = (Bool) down;

	if (mod < 0) {
		if (debug_keyboard) {
			rfbLog("tweak_mod: Skip:  down=%d mod=0x%x\n", down,
			    (int) mod);
		}
		return;
	}
	if (debug_keyboard) {
		rfbLog("tweak_mod: Start:  down=%d mod=0x%x mod_state=0x%x"
		    " is_shift=%d\n", down, (int) mod, (int) mod_state,
		    is_shift);
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
	if (debug_keyboard) {
		rfbLog("tweak_mod: Finish: down=%d mod=0x%x mod_state=0x%x"
		    " is_shift=%d\n", down, (int) mod, (int) mod_state,
		    is_shift);
	}
}

static void modifier_tweak_keyboard(rfbBool down, rfbKeySym keysym, rfbClientPtr client) {
	KeyCode k;
	int tweak = 0;
	if (debug_keyboard) {
		rfbLog("modifier_tweak_keyboard: %s keysym=0x%x\n",
		    down ? "down" : "up", (int) keysym);
	}

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
	if (debug_keyboard) {
		rfbLog("modifier_tweak_keyboard: KeySym 0x%x \"%s\" -> "
		    "KeyCode 0x%x%s\n", (int) keysym, XKeysymToString(keysym),
		    (int) k, k ? "" : " *ignored*");
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
 * key event handler.  See the above functions for contortions for
 * running under -modtweak.
 */
rfbClientPtr last_keyboard_client = NULL;

static void keyboard(rfbBool down, rfbKeySym keysym, rfbClientPtr client) {
	KeyCode k;

	if (debug_keyboard) {
		X_LOCK;
		rfbLog("keyboard(%s, 0x%x \"%s\")\n", down ? "down":"up",
		    (int) keysym, XKeysymToString(keysym));
		X_UNLOCK;
	}

	if (view_only) {
		return;
	}
	last_keyboard_client = client;
	
	if (keyremaps) {
		keyremap_t *remap = keyremaps;
		while (remap != NULL) {
			if (remap->before == keysym) {
				keysym = remap->after;
				if (debug_keyboard) {
					rfbLog("keyboard(): remapping keysym: "
					    "0x%x \"%s\" -> 0x%x \"%s\"\n",
					    (int) remap->before,
					    XKeysymToString(remap->before),
					    (int) remap->after,
					    XKeysymToString(remap->after));
				}
				break;
			}
			remap = remap->next;
		}
	}

	if (use_modifier_tweak) {
		modifier_tweak_keyboard(down, keysym, client);
		X_LOCK;
		XFlush(dpy);
		X_UNLOCK;
		return;
	}

	X_LOCK;

	k = XKeysymToKeycode(dpy, (KeySym) keysym);

	if (debug_keyboard) {
		rfbLog("keyboard(): KeySym 0x%x \"%s\" -> KeyCode 0x%x%s\n",
		    (int) keysym, XKeysymToString(keysym), (int) k,
		    k ? "" : " *ignored*");
	}

	if ( k != NoSymbol ) {
		XTestFakeKeyEvent(dpy, k, (Bool) down, CurrentTime);
		XFlush(dpy);

		last_event = last_input = time(0);
		got_user_input++;
		got_keyboard_input++;
	}

	X_UNLOCK;
}

/*
 * pointer event handling routines.
 */
MUTEX(pointerMutex);
#define MAX_BUTTONS 5
int pointer_map[MAX_BUTTONS+1];
int num_buttons = -1;
char *pointer_remap = NULL;
void update_pointer(int, int, int);

void init_pointer(void) {
	unsigned char map[MAX_BUTTONS];
	int i;
	/*
	 * This routine counts the number of pointer buttons on the X
	 * server (to avoid problems, even crashes, if a client has more
	 * buttons).  And also initializes any pointer button remapping
	 * from -buttonmap option.
	 */
	
	X_LOCK;
	num_buttons = XGetPointerMapping(dpy, map, MAX_BUTTONS);
	X_UNLOCK;
	if (num_buttons < 0) {
		num_buttons = 0;
	}

	/* FIXME: should use info in map[] */
	for (i=1; i<= MAX_BUTTONS; i++) {
		pointer_map[i] = i;
	}

	if (pointer_remap) {
		/* -buttonmap, format is like: 12-21:2 */
		char *p, *q;	
		int n;
		if ((p = strchr(pointer_remap, ':')) != NULL) {
			/* undocumented max button number */
			n = atoi(p+1);	
			if (n < num_buttons || num_buttons == 0) {
				num_buttons = n;
			} else {
				rfbLog("warning: increasing number of mouse "
				    "buttons from %d to %d\n", num_buttons, n);
				num_buttons = n;
			}
		}
		if ((q = strchr(pointer_remap, '-')) != NULL) {
			/*
			 * The '-' separates the 'from' and 'to' lists,
			 * then it is kind of like tr(1).  
			 */
			char str[2];
			int from, to;
			p = pointer_remap;
			q++;
			i = 0;
			str[1] = '\0';
			while (*p != '-') {
				str[0] = *p;
				from = atoi(str);
				str[0] = *(q+i);
				to   = atoi(str);
				rfbLog("button remap: %d -> %d using: "
				    "%s\n", from, to, pointer_remap);
				pointer_map[from] = to;
				p++;
				i++;
			}
		}
	}
}

/*
 * Actual callback from libvncserver when it gets a pointer event.
 */
rfbClientPtr last_pointer_client = NULL;

static void pointer(int mask, int x, int y, rfbClientPtr client) {

	if (debug_pointer && mask >= 0) {
		rfbLog("pointer(mask: 0x%x, x:%4d, y:%4d)\n", mask, x, y);
	}

	if (view_only) {
		return;
	}

	if (num_buttons < 0) {
		init_pointer();
	}

	if (mask >= 0) {
		/*
		 * mask = -1 is a special case call from scan_for_updates()
		 * to flush the event queue; there is no real pointer event.
		 */
		got_user_input++;
		got_pointer_input++;
		last_pointer_client = client;
	}

	/*
	 * The following is hopefully an improvement wrt response during
	 * pointer user input (window drags) for the threaded case.
	 * See check_user_input() for the more complicated things we do
	 * in the non-threaded case.
	 */
	if (use_threads && ! old_pointer) {
#		define NEV 32
		/* storage for the event queue */
		static int mutex_init = 0;
		static int nevents = 0;
		static int ev[NEV][3];
		int i;
		/* timer things */
		static double dt = 0.0, tmr = 0.0, maxwait = 0.4;

		if (! mutex_init) {
			INIT_MUTEX(pointerMutex);
			mutex_init = 1;
		}

		LOCK(pointerMutex);

		/* 
		 * If the framebuffer is being copied in another thread
		 * (scan_for_updates()), we will queue up to 32 pointer
		 * events for later.  The idea is by delaying these input
		 * events, the screen is less likely to change during the
		 * copying period, and so will give rise to less window
		 * "tearing".
		 *
		 * Tearing is not completely eliminated because we do
		 * not suspend work in the other libvncserver threads.
		 * Maybe that is a possibility with a mutex...
		 */
		if (fb_copy_in_progress && mask >= 0) {
			/* 
			 * mask = -1 is an all-clear signal from
			 * scan_for_updates().
			 *
			 * dt is a timer in seconds; we only queue for so long.
			 */
			dt += dtime(&tmr);

			if (nevents < NEV && dt < maxwait) {
				i = nevents++;
				ev[i][0] = mask;
				ev[i][1] = x;
				ev[i][2] = y;
				UNLOCK(pointerMutex);
				if (debug_pointer) {
					rfbLog("pointer(): deferring event "
					    "%d\n", i);
				}
				return;
			}
		}

		/* time to send the queue */
		for (i=0; i<nevents; i++) {
			if (debug_pointer) {
				rfbLog("pointer(): sending event %d\n", i+1);
			}
			update_pointer(ev[i][0], ev[i][1], ev[i][2]);
		}
		if (nevents && dt > maxwait) {
			X_LOCK;
			XFlush(dpy);	
			X_UNLOCK;
		}
		nevents = 0;	/* reset everything */
		tmr = 0.0;
		dt = 0.0;
		dtime(&tmr);

		UNLOCK(pointerMutex);
	}
	if (mask < 0) {		/* -1 just means flush the event queue */
		if (debug_pointer > 1) {
			rfbLog("pointer(): flush only.\n");
		}
		return;
	}

	/* update the X display with the event: */
	update_pointer(mask, x, y);
}

/*
 * Send a pointer event to the X server.
 */

void update_pointer(int mask, int x, int y) {
	int i, mb;

	X_LOCK;

	if (! use_xwarppointer) {
		XTestFakeMotionEvent(dpy, scr, x+off_x, y+off_y, CurrentTime);
	} else {
		XWarpPointer(dpy, None, window, 0, 0, 0, 0,  x+off_x, y+off_y);
	}

	cursor_x = x;
	cursor_y = y;

	last_event = last_input = time(0);

	for (i=0; i < MAX_BUTTONS; i++) {
		/* look for buttons that have be clicked or released: */
		if ( (button_mask & (1<<i)) != (mask & (1<<i)) ) {
			if (debug_pointer) {
				rfbLog("pointer(): mask change: mask: 0x%x -> "
				    "0x%x button: %d\n", button_mask, mask,i+1);
			}
			mb = pointer_map[i+1];
			if (num_buttons && mb > num_buttons) {
				rfbLog("ignoring mouse button out of bounds: %d"
				    ">%d mask: 0x%x -> 0x%x\n", mb, num_buttons,
				    button_mask, mask);
				continue;
			}
			XTestFakeButtonEvent(dpy, mb, 
			    (mask & (1<<i)) ? True : False, CurrentTime);
		}
	}

	if (nofb) {
		/* 
		 * nofb is for, e.g. Win2VNC, where fastest pointer
		 * updates are desired.
		 */
		XFlush(dpy);
	}

	X_UNLOCK;

	/*
	 * Remember the button state for next time and also for the
	 * -nodragging case:
	 */
	button_mask = mask;
}

/*
 * Bell event handling.  Requires XKEYBOARD extension.
 */
#ifdef LIBVNCSERVER_HAVE_XKEYBOARD

int xkb_base_event_type;

void initialize_watch_bell() {
	int ir, reason;
	if (! XkbSelectEvents(dpy, XkbUseCoreKbd, XkbBellNotifyMask,
	    XkbBellNotifyMask) ) {
		fprintf(stderr, "warning: disabling bell.\n");
		watch_bell = 0;
		return;
	}
	if (! XkbOpenDisplay(DisplayString(dpy), &xkb_base_event_type, &ir,
	    NULL, NULL, &reason) ) {
		fprintf(stderr, "warning: disabling bell.\n");
		watch_bell = 0;
	}
}

/*
 * We call this periodically to process any bell events that have 
 * taken place.
 */
void watch_bell_event() {
	XEvent xev;
	XkbAnyEvent *xkb_ev;
	int got_bell = 0;

	if (! watch_bell) {
		return;
	}

	X_LOCK;
	if (! XCheckTypedEvent(dpy, xkb_base_event_type , &xev)) {
		X_UNLOCK;
		return;
	}
	xkb_ev = (XkbAnyEvent *) &xev;
	if (xkb_ev->xkb_type == XkbBellNotify) {
		got_bell = 1;
	}
	X_UNLOCK;

	if (got_bell) {
		if (! all_clients_initialized()) {
			rfbLog("watch_bell_event: not sending bell: "
			    "uninitialized clients\n");
		} else {
			rfbSendBell(screen);
		}
	}
}
#else
void watch_bell_event() {}
#endif


/*
 * Selection/Cutbuffer/Clipboard handlers.
 */

int watch_selection = 1;	/* normal selection/cutbuffer maintenance */
int watch_primary = 1;		/* more dicey, poll for changes in PRIMARY */
int own_selection = 0;		/* whether we currently own PRIMARY or not */
int set_cutbuffer = 0;		/* to avoid bouncing the CutText right back */
int sel_waittime = 15;		/* some seconds to skip before first send */
Window selwin;			/* special window for our selection */

/*
 * This is where we keep our selection: the string sent TO us from VNC
 * clients, and the string sent BY us to requesting X11 clients.
 */
char *xcut_string = NULL;

/*
 * Our callbacks instruct us to check for changes in the cutbuffer
 * and PRIMARY selection on the local X11 display.
 *
 * We store the new cutbuffer and/or PRIMARY selection data in this
 * constant sized array selection_str[].
 * TODO: check if malloc does not cause performance issues (esp. WRT
 * SelectionNotify handling).
 */
#define PROP_MAX (131072L)
char selection_str[PROP_MAX+1];

/*
 * An X11 (not VNC) client on the local display has requested the selection
 * from us (because we are the current owner).
 *
 * n.b.: our caller already has the X_LOCK.
 */
void selection_request(XEvent *ev) {
	XSelectionEvent notify_event;
	XSelectionRequestEvent *req_event;
	unsigned int length;
	unsigned char *data;
#ifndef XA_LENGTH
	unsigned long XA_LENGTH = XInternAtom(dpy, "LENGTH", True);
#endif

	req_event = &(ev->xselectionrequest);
	notify_event.type 	= SelectionNotify;
	notify_event.display	= req_event->display;
	notify_event.requestor	= req_event->requestor;
	notify_event.selection	= req_event->selection;
	notify_event.target	= req_event->target;
	notify_event.time	= req_event->time;

	if (req_event->property == None) {
		notify_event.property = req_event->target;
	} else {
		notify_event.property = req_event->property;
	}
	if (xcut_string) {
		length = strlen(xcut_string);
	} else {
		length = 0;
	}


	if (ev->xselectionrequest.target == XA_LENGTH) {
		/* length request */

		XChangeProperty(ev->xselectionrequest.display,
		    ev->xselectionrequest.requestor,
		    ev->xselectionrequest.property,
		    ev->xselectionrequest.target, 32, PropModeReplace,
		    (unsigned char *) &length, sizeof(unsigned int));

	} else {
		/* data request */

		data = (unsigned char *)xcut_string;

		XChangeProperty(ev->xselectionrequest.display,
		    ev->xselectionrequest.requestor,
		    ev->xselectionrequest.property,
		    ev->xselectionrequest.target, 8, PropModeReplace,
		    data, length);
	}

	XSendEvent(req_event->display, req_event->requestor, False, 0,
	    (XEvent *)&notify_event);

	XFlush(dpy);
}

int all_clients_initialized() {
	rfbClientIteratorPtr iter;
	rfbClientPtr cl;
	int ok = 1;

	iter = rfbGetClientIterator(screen);
	while( (cl = rfbClientIteratorNext(iter)) ) {
		if (cl->state != RFB_NORMAL) {
			ok = 0;
			break;
		}
	}
	rfbReleaseClientIterator(iter);

	return ok;
}

/*
 * CUT_BUFFER0 property on the local display has changed, we read and
 * store it and send it out to any connected VNC clients.
 *
 * n.b.: our caller already has the X_LOCK.
 */
void cutbuffer_send() {
	Atom type;
	int format, slen, dlen;
	unsigned long nitems = 0, bytes_after = 0;
	unsigned char* data = NULL;

	selection_str[0] = '\0';
	slen = 0;

	/* read the property value into selection_str: */
	do {
		if (XGetWindowProperty(dpy, DefaultRootWindow(dpy),
		    XA_CUT_BUFFER0, nitems/4, PROP_MAX/16, False,
		    AnyPropertyType, &type, &format, &nitems, &bytes_after,
		    &data) == Success) {

			dlen = nitems * (format/8);
			if (slen + dlen > PROP_MAX) {
				/* too big */
				rfbLog("warning: truncating large CUT_BUFFER0"
				   " selection > %d bytes.\n", PROP_MAX);
				XFree(data);
				break;
			}
			memcpy(selection_str+slen, data, dlen);
			slen += dlen;
			selection_str[slen] = '\0';
			XFree(data);
		}
	} while (bytes_after > 0);

	selection_str[PROP_MAX] = '\0';

	if (! all_clients_initialized()) {
		rfbLog("cutbuffer_send: no send: uninitialized clients\n");
		return; /* some clients initializing, cannot send */ 
	}

	/* now send it to any connected VNC clients (rfbServerCutText) */
	rfbSendServerCutText(screen, selection_str, strlen(selection_str));
}

/* 
 * "callback" for our SelectionNotify polling.  We try to determine if
 * the PRIMARY selection has changed (checking length and first CHKSZ bytes)
 * and if it has we store it and send it off to any connected VNC clients.
 *
 * n.b.: our caller already has the X_LOCK.
 *
 * TODO: if we were willing to use libXt, we could perhaps get selection
 * timestamps to speed up the checking... XtGetSelectionValue().
 */
#define CHKSZ 32
void selection_send(XEvent *ev) {
	Atom type;
	int format, slen, dlen, oldlen, newlen, toobig = 0;
	static int skip_count = 2, err = 0, sent_one = 0;
	char before[CHKSZ], after[CHKSZ];
	unsigned long nitems = 0, bytes_after = 0;
	unsigned char* data = NULL;

	/*
	 * remember info about our last value of PRIMARY (or CUT_BUFFER0)
	 * so we can check for any changes below.
	 */
	oldlen = strlen(selection_str);
	strncpy(before, selection_str, CHKSZ);

	selection_str[0] = '\0';
	slen = 0;

	/* read in the current value of PRIMARY: */
	do {
		if (XGetWindowProperty(dpy, ev->xselection.requestor,
		    ev->xselection.property, nitems/4, PROP_MAX/16, True,
		    AnyPropertyType, &type, &format, &nitems, &bytes_after,
		    &data) == Success) {

			dlen = nitems * (format/8);
			if (slen + dlen > PROP_MAX) {
				/* too big */
				toobig = 1;
				XFree(data);
				if (err) {	/* cut down on messages */
					break;
				} else {
					err = 5;
				}
				rfbLog("warning: truncating large PRIMARY"
				   " selection > %d bytes.\n", PROP_MAX);
				break;
			}
			memcpy(selection_str+slen, data, dlen);
			slen += dlen;
			selection_str[slen] = '\0';
			XFree(data);
		}
	} while (bytes_after > 0);

	if (! toobig) {
		err = 0;
	} else if (err) {
		err--;
	}

	if (! sent_one) {
		/* try to force a send first time in */
		oldlen = -1;
		sent_one = 1;
	}

	/* look for changes in the new value */
	newlen = strlen(selection_str);
	strncpy(after, selection_str, CHKSZ);

	if (oldlen == newlen && strncmp(before, after, CHKSZ) == 0) {
		/* evidently no change */
		return;
	}
	if (newlen == 0) {
		/* do not bother sending a null string out */
		return;
	}

	if (! all_clients_initialized()) {
		rfbLog("selection_send: no send: uninitialized clients\n");
		return; /* some clients initializing, cannot send */ 
	}

	/* now send it to any connected VNC clients (rfbServerCutText) */
	rfbSendServerCutText(screen, selection_str, newlen);
}


/*
 * Routines for monitoring the VNC_CONNECT property for changes.
 * The vncconnect(1) will set it on our X display.
 */

Atom vnc_connect_prop = None;

void read_vnc_connect_prop() {
	Atom type;
	int format, slen, dlen;
	unsigned long nitems = 0, bytes_after = 0;
	unsigned char* data = NULL;

	vnc_connect_str[0] = '\0';
	slen = 0;

	if (! vnc_connect || vnc_connect_prop == None) {
		/* not active or problem with VNC_CONNECT atom */
		return;
	}

	/* read the property value into vnc_connect_str: */
	do {
		if (XGetWindowProperty(dpy, DefaultRootWindow(dpy),
		    vnc_connect_prop, nitems/4, VNC_CONNECT_MAX/16, False,
		    AnyPropertyType, &type, &format, &nitems, &bytes_after,
		    &data) == Success) {

			dlen = nitems * (format/8);
			if (slen + dlen > VNC_CONNECT_MAX) {
				/* too big */
				rfbLog("warning: truncating large VNC_CONNECT"
				   " string > %d bytes.\n", VNC_CONNECT_MAX);
				XFree(data);
				break;
			}
			memcpy(vnc_connect_str+slen, data, dlen);
			slen += dlen;
			vnc_connect_str[slen] = '\0';
			XFree(data);
		}
	} while (bytes_after > 0);

	vnc_connect_str[VNC_CONNECT_MAX] = '\0';
	rfbLog("read property VNC_CONNECT: %s\n", vnc_connect_str);
}

/*
 * This routine is periodically called to check for selection related
 * and other X11 events and respond to them as needed.
 */
void watch_xevents() {
	XEvent xev;
	static int first = 1, sent_sel = 0;
	int have_clients = screen->rfbClientHead ? 1 : 0;
	time_t last_request = 0, now = time(0);

	X_LOCK;
	if (first && (watch_selection || vnc_connect)) {
		/*
		 * register desired event(s) for notification.
		 * PropertyChangeMask is for CUT_BUFFER0 changes.
		 * TODO: does this cause a flood of other stuff?
		 */
		XSelectInput(dpy, rootwin, PropertyChangeMask);
	}
	if (first && watch_selection) {
		/* create fake window for our selection ownership, etc */
		selwin = XCreateSimpleWindow(dpy, rootwin, 0, 0, 1, 1, 0, 0, 0);
	}
	if (first && vnc_connect) {
		vnc_connect_str[0] = '\0';
		vnc_connect_prop = XInternAtom(dpy, "VNC_CONNECT", False);
	}
	first = 0;

	/*
	 * There is a bug where we have to wait before sending text to
	 * the client... so instead of sending right away we wait a
	 * the few seconds.
	 */
	if (have_clients && watch_selection && ! sent_sel
	    && now > last_client + sel_waittime) {
		if (XGetSelectionOwner(dpy, XA_PRIMARY) == None) {
			cutbuffer_send();
		}
		sent_sel = 1;
	}

	/* check for CUT_BUFFER0 and VNC_CONNECT changes: */
	if (XCheckTypedEvent(dpy, PropertyNotify, &xev)) {
		if (xev.type == PropertyNotify) {
			if (xev.xproperty.atom == XA_CUT_BUFFER0) {
				/*
				 * Go retrieve CUT_BUFFER0 and send it.
				 *
				 * set_cutbuffer is a flag to try to avoid
				 * processing our own cutbuffer changes.
				 */
				if (have_clients && watch_selection
				    && ! set_cutbuffer) {
					cutbuffer_send();
					sent_sel = 1;
				}
				set_cutbuffer = 0;
			} else if (vnc_connect && vnc_connect_prop != None
		    	    && xev.xproperty.atom == vnc_connect_prop) {
	
				/*
				 * Go retrieve VNC_CONNECT string.
				 */
				read_vnc_connect_prop();
			}
		}
	}

	if (! have_clients || ! watch_selection) {
		/*
		 * no need to monitor selections if no current clients
		 * or -nosel.
		 */
		X_UNLOCK;
		return;
	}

	/* check for our PRIMARY request notification: */
	if (watch_primary) {
		if (XCheckTypedEvent(dpy, SelectionNotify, &xev)) {
			if (xev.type == SelectionNotify &&
			    xev.xselection.requestor == selwin &&
			    xev.xselection.selection == XA_PRIMARY &&
			    xev.xselection.property != None &&
			    xev.xselection.target == XA_STRING) {

				/* go retrieve PRIMARY and check it */
				if (now > last_client + sel_waittime
				    || sent_sel) {
					selection_send(&xev);
				}
			}
		}
		if (now > last_request + 1) {
			/*
			 * Every second or two, request PRIMARY, unless we
			 * already own it or there is no owner.
			 * TODO: even at this low rate we should look into
			 * and performance problems in odds cases, etc.
			 */
			last_request = now;
			if (! own_selection &&
			    XGetSelectionOwner(dpy, XA_PRIMARY) != None) {
				XConvertSelection(dpy, XA_PRIMARY, XA_STRING,
				    XA_STRING, selwin, CurrentTime);
			}
		}
	}

	if (! own_selection) {
		/*
		 * no need to do the PRIMARY maintenance tasks below if
		 * no we do not own it (right?).
		 */
		X_UNLOCK;
		return;
	}

	/* we own PRIMARY, see if someone requested it: */
	if (XCheckTypedEvent(dpy, SelectionRequest, &xev)) {
		if (xev.type == SelectionRequest &&
		    xev.xselectionrequest.selection == XA_PRIMARY) {
			selection_request(&xev);
		}
	}

	/* we own PRIMARY, see if we no longer own it: */
	if (XCheckTypedEvent(dpy, SelectionClear, &xev)) {
		if (xev.type == SelectionClear &&
		    xev.xselectionclear.selection == XA_PRIMARY) {

			own_selection = 0;
			if (xcut_string) {
				free(xcut_string);
				xcut_string = NULL;
			}
		}
	}
	X_UNLOCK;
}

/*
 * hook called when a VNC client sends us some "XCut" text (rfbClientCutText).
 */
void xcut_receive(char *text, int len, rfbClientPtr cl) {
	static int first = 1;

	if (text == NULL || len == 0) {
		return;
	}

	X_LOCK;

	/* associate this text with PRIMARY (and SECONDARY...) */
	if (! own_selection) {
		own_selection = 1;
		/* we need to grab the PRIMARY selection */
		XSetSelectionOwner(dpy, XA_PRIMARY, selwin, CurrentTime);
		XFlush(dpy);
	}

	/* duplicate the text string for our own use. */
	if (xcut_string != NULL) {
		free(xcut_string);
	}
	xcut_string = (unsigned char *)
	    malloc((size_t) (len+1) * sizeof(unsigned char));
	strncpy(xcut_string, text, len);
	xcut_string[len] = '\0';	/* make sure null terminated */

	/* copy this text to CUT_BUFFER0 as well: */
	XChangeProperty(dpy, rootwin, XA_CUT_BUFFER0, XA_STRING, 8,
	    PropModeReplace, text, len);
	XFlush(dpy);

	X_UNLOCK;

	set_cutbuffer = 1;
}

void mark_hint(hint_t);

/*
 * Here begins a bit of a mess to experiment with multiple cursors ...
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
 * save current cursor info and the patch of non-cursor data it covers
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
 * Descends window tree at pointer until the window cursor matches the current 
 * cursor.  So far only used to detect if mouse is on root background or not.
 * (returns 0 in that case, 1 otherwise).
 *
 * It seems impossible to do, but if the actual cursor could ever be
 * determined we might want to hash that info on window ID or something...
 */
int tree_descend_cursor(void) {
	Window r, c;
	int rx, ry, wx, wy;
	unsigned int mask;
	int descend = 0, tries = 0, maxtries = 1;

	X_LOCK;
	c = window;
	while (c) {
		if (++tries > maxtries) {
			descend = maxtries;
			break;
		}
		if ( XTestCompareCurrentCursorWithWindow(dpy, c) ) {
			break;
		}
		XQueryPointer(dpy, c, &r, &c, &rx, &ry, &wx, &wy, &mask);
		descend++;
	}
	X_UNLOCK;
	return descend;
}

void blackout_nearby_tiles(x, y, dt) {
	int sx, sy, n, b;
	int tx = x/tile_x;
	int ty = y/tile_y;
	
	if (! blackouts) {
		return;
	}
	if (dt < 1) {
		dt = 1;
	}
	/* loop over a range of tiles, blacking out as needed */

	for (sx = tx - dt; sx <= tx + dt; sx++) {
		if (sx < 0 || sx >= tile_x) {
			continue;
		}
		for (sy = ty - dt; sy <= ty + dt; sy++) {
			if (sy < 0 || sy >= tile_y) {
				continue;
			}
			n = sx + sy * ntiles_x;
			if (tile_blackout[n].cover == 0) {
				continue;
			}
			for (b=0; b <= tile_blackout[n].count; b++) {
				int x1, y1, x2, y2;
				x1 = tile_blackout[n].bo[b].x1;
				y1 = tile_blackout[n].bo[b].y1;
				x2 = tile_blackout[n].bo[b].x2;
				y2 = tile_blackout[n].bo[b].y2;
				zero_fb(x1, y1, x2, y2);
			}
		}
	}
}

/*
 * This is a special workaround to quickly send rfbCursorPosUpdates
 * to client(s) when in -nofb mode, e.g. Win2VNC.  It sends the updates
 * immediately (by-passing libvncserver).  Requires a customized Win2VNC
 * to interpret the rfbCursorPosUpdates.  Currently no big reason to
 * do this for non-nofb mode (i.e. normal viewing) and so the normal
 * libvncserver mechanism is used.  Thanks to Edoardo Tirtarahardja.
 */
void update_client_pointer(rfbClientPtr cl, rfbBool send) {
	rfbFramebufferUpdateMsg *fu;

	if (! nofb) {
		/* normal libvncserver mechanism: */
		cl->cursorWasMoved = send;
		return;
	}
	if (! send) {
		return;
	}
	if (cl->state != RFB_NORMAL) {
		/* bad idea to force this data to initializing clients */
		return;
	}

	fu = (rfbFramebufferUpdateMsg*) cl->updateBuf;
	cl->rfbFramebufferUpdateMessagesSent++;
	fu->type = rfbFramebufferUpdate;
	fu->nRects = Swap16IfLE(1);
	memcpy(&cl->updateBuf[cl->ublen], (char*) fu,
	    sz_rfbFramebufferUpdateMsg);
	cl->ublen = sz_rfbFramebufferUpdateMsg;

	rfbSendCursorPos(cl);

	if (debug_pointer) {
		rfbLog("Sending rfbEncodingPointerPos message to host %s with "
		    "x=%d,y=%d\n", cl->host, screen->cursorX, screen->cursorY);
	}
}

/*
 * Send rfbCursorPosUpdates back to clients that understand them.  This
 * seems to be TightVNC specific.
 */
void cursor_pos_updates(int x, int y) {
	rfbClientIteratorPtr iter;
	rfbClientPtr cl;
	int cnt = 0;

	if (! cursor_pos) {
		return;
	}
	/* x and y are current positions of X11 pointer on the X11 display */
	if (x == screen->cursorX && y == screen->cursorY) {
		return;
	}

	if (screen->cursorIsDrawn) {
		rfbUndrawCursor(screen);
	}
	LOCK(screen->cursorMutex);
	if (! screen->cursorIsDrawn) {
		screen->cursorX = x;
		screen->cursorY = y;
	}
	UNLOCK(screen->cursorMutex);

	iter = rfbGetClientIterator(screen);
	while( (cl = rfbClientIteratorNext(iter)) ) {
		if (! cl->enableCursorPosUpdates) {
			continue;
		}
		if (cl == last_pointer_client) {
			/*
			 * special case if this client was the last one to
			 * send a pointer position.
			 */
			if (x == cursor_x && y == cursor_y) {
				update_client_pointer(cl, FALSE);
			} else {
				/* an X11 app evidently warped the pointer */
				if (debug_pointer) {
					rfbLog("cursor_pos_updates: warp "
					    "detected dx=%3d dy=%3d\n",
					    cursor_x - x, cursor_y - y);
				}
				update_client_pointer(cl, TRUE);
				cnt++;
			}
		} else {
			update_client_pointer(cl, TRUE);
			cnt++;
		}
	}
	rfbReleaseClientIterator(iter);

	if (debug_pointer && cnt) {
		rfbLog("cursor_pos_updates: sent position x=%3d y=%3d to %d"
		    " clients\n", x, y, cnt);
	}
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

	if (! show_mouse) {
		return;
	}
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

	if (indexed_colour) {
		black = BlackPixel(dpy, scr) % 256;
		white = WhitePixel(dpy, scr) % 256;
	}
	if (reverse) {
		int tmp = black;
		black = white;
		white = tmp;
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

	if (blackouts) {
		/*
		 * loop over a range of tiles, blacking out as needed
		 * note we currently disable mouse drawing under blackouts.
		 */
		static int mx = -1, my = -1;
		int skip = 0;
		if (mx < 0) {
			mx = x;
			my = y;
		} else if (mx == x && my == y) {
			skip = 1;
		}
		mx = x;
		my = y;

		if (! skip) {
			blackout_nearby_tiles(x, y, 2);
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
		int descend;
		if ( (descend = tree_descend_cursor()) ) {
			which = 0;
		} else {
			which = 1;
		}
	}

	cursor_pos_updates(root_x - off_x, root_y - off_y);
	draw_mouse(root_x - off_x, root_y - off_y, which, 1);
}

/*
 * For the subwin case follows the window if it is moved.
 */
void set_offset(void) {
	Window w;
	if (! subwin) {
		return;
	}
	X_LOCK;
	XTranslateCoordinates(dpy, window, rootwin, 0, 0, &off_x, &off_y, &w);
	X_UNLOCK;
}

/*
 * Some handling of 8bpp PseudoColor colormaps.  Called for initializing
 * the clients and dynamically if -flashcmap is specified.
 */
#define NCOLOR 256
void set_colormap(void) {
	static int first = 1;
	static XColor color[NCOLOR], prev[NCOLOR];
	Colormap cmap;
	Visual *vis;
	int i, ncells, diffs = 0;

	if (first) {
		screen->colourMap.count = NCOLOR;
		screen->rfbServerFormat.trueColour = FALSE;
		screen->colourMap.is16 = TRUE;
		screen->colourMap.data.shorts = (unsigned short*)
			malloc(3*sizeof(short) * NCOLOR);
	}

	for (i=0; i < NCOLOR; i++) {
		prev[i].red   = color[i].red;
		prev[i].green = color[i].green;
		prev[i].blue  = color[i].blue;
	}

	X_LOCK;

	cmap = DefaultColormap(dpy, scr);
	ncells = CellsOfScreen(ScreenOfDisplay(dpy, scr));
	vis = visual;

	if (subwin) {
		XWindowAttributes attr;

		if (XGetWindowAttributes(dpy, window, &attr)) {
			cmap = attr.colormap;
			vis = attr.visual;
			ncells = vis->map_entries;
		}
	}

	if (first && ncells != NCOLOR) {
		fprintf(stderr, "set_colormap: number of cells is %d"
		    " instead of %d.\n", ncells, NCOLOR);
		screen->colourMap.count = ncells;
	}

	if (flash_cmap && ! first) {
		XWindowAttributes attr;
		Window r, c;
		int rx, ry, wx, wy, tries = 0;
		unsigned int m;

		c = window;
		while (c && tries++ < 16) {
			/* XXX XQueryTree somehow? */
			XQueryPointer(dpy, c, &r, &c, &rx, &ry, &wx, &wy, &m);
			if (c && XGetWindowAttributes(dpy, c, &attr)) {
				if (attr.colormap && attr.map_installed) {
					cmap = attr.colormap;
					vis = attr.visual;
					ncells = vis->map_entries;
					break;
				}
			} else {
				break;
			}
		}
	}
	if (ncells > NCOLOR) {
		fprintf(stderr, "set_colormap: big problem: ncells=%d > %d\n",
		    ncells, NCOLOR);
	}

	if (vis->class == TrueColor || vis->class == DirectColor) {
		/*
		 * Kludge to make 8bpp TrueColor & DirectColor be like
		 * the StaticColor map.  The ncells = 8 is "8 per subfield"
		 * mentioned in xdpyinfo.  Looks OK... likely fortuitously.
		 */
		if (ncells == 8) {
			ncells = NCOLOR;
		}
	}

	for (i=0; i < ncells; i++) {
		color[i].pixel = i;
		color[i].pad = 0;
	}

	XQueryColors(dpy, cmap, color, ncells);

	X_UNLOCK;

	for(i=0; i < ncells; i++) {
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
		if (! all_clients_initialized()) {
			rfbLog("set_colormap: warning: sending cmap "
			    "with uninitialized clients.\n");
		}
		rfbSetClientColourMaps(screen, 0, ncells);
	}

	first = 0;
}

/*
 * Experimental mode to force the visual of the window instead of querying
 * it.  Currently just used for testing or overriding some rare cases.
 * Input string can be a decimal or 0x hex or something like TrueColor
 * or TrueColor:24 to force a depth as well.
 */
void set_visual(char *vstring) {
	int vis, defdepth = DefaultDepth(dpy, scr);
	XVisualInfo vinfo;
	char *p;

	fprintf(stderr, "set_visual: %s\n", vstring);

	if ((p = strchr(vstring, ':')) != NULL) {
		visual_depth = atoi(p+1);
		*p = '\0';
	} else {
		visual_depth = defdepth;
	}
	if (strcmp(vstring, "StaticGray") == 0) {
		vis = StaticGray;
	} else if (strcmp(vstring, "GrayScale") == 0) {
		vis = GrayScale;
	} else if (strcmp(vstring, "StaticColor") == 0) {
		vis = StaticColor;
	} else if (strcmp(vstring, "PseudoColor") == 0) {
		vis = PseudoColor;
	} else if (strcmp(vstring, "TrueColor") == 0) {
		vis = TrueColor;
	} else if (strcmp(vstring, "DirectColor") == 0) {
		vis = DirectColor;
	} else {
		if (sscanf(vstring, "0x%x", &visual_id) != 1) {
			if (sscanf(vstring, "%d", &visual_id) == 1) {
				return;
			}
			fprintf(stderr, "bad -visual arg: %s\n", vstring);
			exit(1);
		}
		return;
	}
	if (XMatchVisualInfo(dpy, scr, visual_depth, vis, &vinfo)) {
		;
	} else if (XMatchVisualInfo(dpy, scr, defdepth, vis, &vinfo)) {
		;
	} else {
		fprintf(stderr, "could not find visual: %s\n", vstring);
		exit(1);
	}
	visual_id = vinfo.visualid;
}

/*
 * Presumably under -nofb the clients will never request the framebuffer.
 * But we have gotten such a request... so let's just give them
 * the current view on the display.  n.b. x2vnc and perhaps win2vnc
 * requests a 1x1 pixel for some workaround so sadly this evidently
 * nearly always happens.
 */
void nofb_hook(rfbClientPtr cl) {
	static int loaded_fb = 0;
	XImage *fb;
	if (loaded_fb) {
		return;
	}
	rfbLog("framebuffer requested in -nofb mode by client %s\n", cl->host);
	fb = XGetImage(dpy, window, 0, 0, dpy_x, dpy_y, AllPlanes, ZPixmap);
	screen->frameBuffer = fb->data;
	loaded_fb = 1;
	screen->displayHook = NULL;
}

int got_rfbport = 0;
int got_alwaysshared = 0;
int got_nevershared = 0;
/*
 * initialize the rfb framebuffer/screen
 */
void initialize_screen(int *argc, char **argv, XImage *fb) {
	int have_masks = 0;
	int argc_orig = *argc;

	screen = rfbGetScreen(argc, argv, fb->width, fb->height,
	    fb->bits_per_pixel, 8, fb->bits_per_pixel/8);

	fprintf(stderr, "\n");

	if (! screen) {
		int i;
		rfbLog("\n");
		rfbLog("failed to create rfb screen.\n");
		for (i=0; i< *argc; i++)  {
			rfbLog("\t[%d]  %s\n", i, argv[i]);
		}
		clean_up_exit(1);
	}

/*
 * This ifdef is a transient for source compatibility for people who download
 * the x11vnc.c file by itself and plop it down into their libvncserver tree.
 * Remove at some point.  BTW, this assumes no usage of earlier "0.7pre".
 */
#ifdef LIBVNCSERVER_VERSION
if (strcmp(LIBVNCSERVER_VERSION, "0.5") && strcmp(LIBVNCSERVER_VERSION, "0.6")) {
	if (*argc != 1) {
		int i;
		rfbLog("*** unrecognized option(s) ***\n");
		for (i=1; i< *argc; i++)  {
			rfbLog("\t[%d]  %s\n", i, argv[i]);
		}
		rfbLog("for a list of options run: x11vnc -help\n");
		clean_up_exit(1);
	}
}
#endif

	screen->paddedWidthInBytes = fb->bytes_per_line;
	screen->rfbServerFormat.bitsPerPixel = fb->bits_per_pixel;
	screen->rfbServerFormat.depth = fb->depth;
	screen->rfbServerFormat.trueColour = (uint8_t) TRUE;
	have_masks = (fb->red_mask|fb->green_mask|fb->blue_mask != 0);
	if (force_indexed_color) {
		have_masks = 0;
	}

	if ( ! have_masks && screen->rfbServerFormat.bitsPerPixel == 8
	    && CellsOfScreen(ScreenOfDisplay(dpy,scr)) ) {
		/* indexed colour */
		if (!quiet) rfbLog("Using X display with 8bpp indexed color\n");
		indexed_colour = 1;
		set_colormap();
	} else {
		/* general case ... */
		if (! quiet) {
			rfbLog("Using X display with %dbpp depth=%d true "
			    "color\n", fb->bits_per_pixel, fb->depth);
		}

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

	/* nofb is for pointer/keyboard only handling.  */
	if (nofb) {
		screen->frameBuffer = NULL;
		screen->displayHook = nofb_hook;
	} else {
		screen->frameBuffer = fb->data; 
	}

	/* called from inetd, we need to treat stdio as our socket */
	if (inetd) {
		int fd = dup(0);
		if (fd < 3) {
			rfbErr("dup(0) = %d failed.\n", fd);
			perror("dup");
			clean_up_exit(1);
		}
		fclose(stdin);
		fclose(stdout);
		/* we keep stderr for logging */
		screen->inetdSock = fd;
		screen->rfbPort = 0;

	} else if (! got_rfbport) {
		screen->autoPort = TRUE;
	}

	if (! got_nevershared && ! got_alwaysshared) {
		if (shared) {
			screen->rfbAlwaysShared = TRUE;
		} else {
			screen->rfbDontDisconnect = TRUE;
			screen->rfbNeverShared = TRUE;
		}
	}
	/* XXX the following is based on libvncserver defaults. */
	if (screen->rfbDeferUpdateTime == 5) {
		/* XXX will be fixed someday */
		screen->rfbDeferUpdateTime = defer_update;
	}

	/* event callbacks: */
	screen->newClientHook = new_client;
	screen->kbdAddEvent = keyboard;
	screen->ptrAddEvent = pointer;
	if (watch_selection) {
		screen->setXCutText = xcut_receive;
	}

	if (local_cursor) {
		cursor = rfbMakeXCursor(CUR_SIZE, CUR_SIZE, CUR_DATA, CUR_MASK);
		screen->cursor = cursor;
	} else {
		screen->cursor = NULL;
	}

	rfbInitServer(screen);

	bytes_per_line = screen->paddedWidthInBytes;
	bpp = screen->rfbServerFormat.bitsPerPixel;
	depth = screen->rfbServerFormat.depth;
}

/*
 * Take a comma separated list of geometries: WxH+X+Y and register them as
 * rectangles to black out from the screen.
 */
void initialize_blackout (char *list) {
	char *p, *blist = strdup(list);
	int x, y, X, Y, h, w;
	int tx, ty;

	p = strtok(blist, ",");
	while (p) {
		/* handle +/-x and +/-y */
		if (sscanf(p, "%dx%d+%d+%d", &w, &h, &x, &y) == 4) {
			;
		} else if (sscanf(p, "%dx%d-%d+%d", &w, &h, &x, &y) == 4) {
			x = dpy_x - x - w;
		} else if (sscanf(p, "%dx%d+%d-%d", &w, &h, &x, &y) == 4) {
			y = dpy_y - y - h;
		} else if (sscanf(p, "%dx%d-%d-%d", &w, &h, &x, &y) == 4) {
			x = dpy_x - x - w;
			y = dpy_y - y - h;
		} else {
			if (*p != '\0') {
				rfbLog("skipping invalid geometry: %s\n", p);
			}
			p = strtok(NULL, ",");
			continue;
		}
		X = x + w;
		Y = y + h;
		if (x < 0 || x > dpy_x || y < 0 || y > dpy_y ||
		    X < 0 || X > dpy_x || Y < 0 || Y > dpy_y) {
			rfbLog("skipping invalid blackout geometry: %s x="
			    "%d-%d,y=%d-%d,w=%d,h=%d\n", p, x, X, y, Y, w, h);
		} else {
			rfbLog("blackout rect: %s: x=%d-%d y=%d-%d\n", p,
			    x, X, y, Y);

			/*
			 * note that the black out is x1 <= x but x < x2
			 * for the region. i.e. the x2, y2 are outside
			 * by 1 pixel. 
			 */
			black[blackouts].x1 = x;
			black[blackouts].y1 = y;
			black[blackouts].x2 = X;
			black[blackouts].y2 = Y;
			blackouts++;
			if (blackouts >= 100) {
				rfbLog("too many blackouts: %d\n", blackouts);
				break;
			}
		}
		p = strtok(NULL, ",");
	}
	free(blist);
}

/*
 * Now that all blackout rectangles have been constructed, see what overlap
 * they have with the tiles in the system.  If a tile is touched by a
 * blackout, record information.
 */
void blackout_tiles() {
	int tx, ty;
	if (! blackouts) {
		return;
	}

	/* 
	 * to simplify things drop down to single copy mode, no vcr, etc...
	 */
	single_copytile = 1;
	if (show_mouse) {
		rfbLog("disabling remote mouse drawing due to blackouts\n");
		show_mouse = 0;
	}

	/* loop over all tiles. */
	for (ty=0; ty < ntiles_y; ty++) {
		for (tx=0; tx < ntiles_x; tx++) {
			sraRegionPtr tile_reg, black_reg;
			sraRect rect;
			sraRectangleIterator *iter;
			int n, b, x1, y1, x2, y2, cnt;

			/* tile number and coordinates: */
			n = tx + ty * ntiles_x;
			x1 = tx * tile_x;
			y1 = ty * tile_y;
			x2 = x1 + tile_x;
			y2 = y1 + tile_y;
			if (x2 > dpy_x) {
				x2 = dpy_x;
			}
			if (y2 > dpy_y) {
				y2 = dpy_y;
			}

			/* make regions for the tile and the blackouts: */
			black_reg = (sraRegionPtr) sraRgnCreate();
			tile_reg  = (sraRegionPtr) sraRgnCreateRect(x1, y1,
			    x2, y2);

			tile_blackout[n].cover = 0;
			tile_blackout[n].count = 0;

			/* union of blackouts */
			for (b=0; b < blackouts; b++) {
				sraRegionPtr tmp_reg = (sraRegionPtr)
				    sraRgnCreateRect(black[b].x1, black[b].y1,
				    black[b].x2, black[b].y2);

				sraRgnOr(black_reg, tmp_reg);
				sraRgnDestroy(tmp_reg);
			}

			if (! sraRgnAnd(black_reg, tile_reg)) {
				/*
				 * no intersection for this tile, so we
				 * are done.
				 */
				sraRgnDestroy(black_reg);
				sraRgnDestroy(tile_reg);
				continue;
			}

			/*
			 * loop over rectangles that make up the blackout
			 * region:
			 */
			cnt = 0;
			iter = sraRgnGetIterator(black_reg);
			while (sraRgnIteratorNext(iter, &rect)) {

				/* make sure x1 < x2 and y1 < y2 */
				if (rect.x1 > rect.x2) {
					int tmp = rect.x2;
					rect.x2 = rect.x1;
					rect.x1 = tmp;
				}
				if (rect.y1 > rect.y2) {
					int tmp = rect.y2;
					rect.y2 = rect.y1;
					rect.y1 = tmp;
				}

				/* store coordinates */
				tile_blackout[n].bo[cnt].x1 = rect.x1;
				tile_blackout[n].bo[cnt].y1 = rect.y1;
				tile_blackout[n].bo[cnt].x2 = rect.x2;
				tile_blackout[n].bo[cnt].y2 = rect.y2;

				/* note if the tile is completely obscured */
				if (rect.x1 == x1 && rect.y1 == y1 &&
				    rect.x2 == x2 && rect.y2 == y2) {
					tile_blackout[n].cover = 2;
				} else {
					tile_blackout[n].cover = 1;
				}

				if (++cnt >= 10) {
					rfbLog("too many blackout rectangles "
					    "for tile %d=%d,%d.\n", n, tx, ty);
					break;
				}
			}

			sraRgnReleaseIterator(iter);
			sraRgnDestroy(black_reg);
			sraRgnDestroy(tile_reg);

			tile_blackout[n].count = cnt;
		}
	}
}

void initialize_xinerama () {
#ifdef LIBVNCSERVER_HAVE_LIBXINERAMA
	XineramaScreenInfo *sc, *xineramas;
#endif
	sraRegionPtr black_region, tmp_region;
	sraRectangleIterator *iter;
	sraRect rect;
	char *bstr, *tstr;
	int ev, er, i, n, rcnt;

#ifndef LIBVNCSERVER_HAVE_LIBXINERAMA
	rfbLog("Xinerama: Library libXinerama is not available to determine\n");
	rfbLog("Xinerama: the head geometries, consider using -blackout\n");
	rfbLog("Xinerama: if the screen is non-rectangular.\n");
#else
	if (! XineramaQueryExtension(dpy, &ev, &er)) {
		rfbLog("Xinerama: disabling: display does not support it.\n");
		xinerama = 0;
		return;
	}
	if (! XineramaIsActive(dpy)) {
		/* n.b. change to XineramaActive(dpy, window) someday */
		rfbLog("Xinerama: disabling: not active on display.\n");
		xinerama = 0;
		return;
	}

	/* n.b. change to XineramaGetData() someday */
	xineramas = XineramaQueryScreens(dpy, &n);
	rfbLog("Xinerama: number of sub-screens: %d\n", n);

	if (n == 1) {
		rfbLog("Xinerama: no blackouts needed (only one"
		    " sub-screen)\n");
		XFree(xineramas);
		return;		/* must be OK w/o change */
	}

	black_region = sraRgnCreateRect(0, 0, dpy_x, dpy_y);

	sc = xineramas;
	for (i=0; i<n; i++) {
		int x, y, w, h;
		
		x = sc->x_org;
		y = sc->y_org;
		w = sc->width;
		h = sc->height;

		tmp_region = sraRgnCreateRect(x, y, x + w, y + h);

		sraRgnSubtract(black_region, tmp_region);
		sraRgnDestroy(tmp_region);
		sc++;
	}
	XFree(xineramas);

	if (sraRgnEmpty(black_region)) {
		rfbLog("Xinerama: no blackouts needed (screen fills"
		    " rectangle)\n");
		sraRgnDestroy(black_region);
		return;
	}

	/* max len is 10000x10000+10000+10000 (23 chars) per geometry */
	rcnt = (int) sraRgnCountRects(black_region);
	bstr = (char *) malloc(30 * rcnt * sizeof(char));
	tstr = (char *) malloc(30 * sizeof(char));
	bstr[0] = '\0';

	iter = sraRgnGetIterator(black_region);
	while (sraRgnIteratorNext(iter, &rect)) {
		int x, y, w, h;

		/* make sure x1 < x2 and y1 < y2 */
		if (rect.x1 > rect.x2) {
			int tmp = rect.x2;
			rect.x2 = rect.x1;
			rect.x1 = tmp;
		}
		if (rect.y1 > rect.y2) {
			int tmp = rect.y2;
			rect.y2 = rect.y1;
			rect.y1 = tmp;
		}
		x = rect.x1;
		y = rect.y1;
		w = rect.x2 - x;
		h = rect.y2 - y;
		sprintf(tstr, "%dx%d+%d+%d,", w, h, x, y);
		strcat(bstr, tstr);
	}
	initialize_blackout(bstr);

	free(bstr);
	free(tstr);
#endif

}

/*
 * Fill the framebuffer with zero for the prescribed rectangle
 */
void zero_fb(x1, y1, x2, y2) {
	int pixelsize = bpp >> 3;
	int line, fill = 0;
	char *dst;
	
	if (x1 < 0 || x2 <= x1 || x2 > dpy_x) {
		return;
	}
	if (y1 < 0 || y2 <= y1 || y2 > dpy_y) {
		return;
	}

	dst = screen->frameBuffer + y1 * bytes_per_line + x1 * pixelsize;
	line = y1;
	while (line++ < y2) {
		memset(dst, fill, (size_t) (x2 - x1) * pixelsize);
		dst += bytes_per_line;
	}
}

/*
 * Fill the framebuffer with zeros for each blackout region
 */
void blackout_regions() {
	int i;
	for (i=0; i < blackouts; i++) {
		zero_fb(black[i].x1, black[i].y1, black[i].x2, black[i].y2);
	}
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
	tile_blackout    = (tile_blackout_t *)
		malloc((size_t) (ntiles * sizeof(tile_blackout_t)));
	tile_region = (region_t *) malloc((size_t) (ntiles * sizeof(region_t)));

	tile_row = (XImage **)
		malloc((size_t) ((ntiles_x + 1) * sizeof(XImage *)));
	tile_row_shm = (XShmSegmentInfo *)
		malloc((size_t) ((ntiles_x + 1) * sizeof(XShmSegmentInfo)));

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
int shm_create(XShmSegmentInfo *shm, XImage **ximg_ptr, int w, int h,
    char *name) {

	XImage *xim;

	shm->shmid = -1;
	shm->shmaddr = (char *) -1;
	*ximg_ptr = NULL;

	if (nofb) {
		return 1;
	}

	X_LOCK;

	if (! using_shm) {
		/* we only need the XImage created */
		xim = XCreateImage(dpy, visual, depth, ZPixmap, 0, NULL, w, h,
		    BitmapPad(dpy), 0);

		X_UNLOCK;

		if (xim == NULL) {
			rfbErr("XCreateImage(%s) failed.\n", name);
			return 0;
		}
		xim->data = (char *) malloc(xim->bytes_per_line * xim->height);
		if (xim->data == NULL) {
			rfbErr("XCreateImage(%s) data malloc failed.\n", name);
			return 0;
		}
		if (flip_byte_order) {
			static int reported = 0;
			char *bo;
			if (xim->byte_order == LSBFirst) {
				bo = "MSBFirst";
				xim->byte_order = MSBFirst;
				xim->bitmap_bit_order = MSBFirst;
			} else {
				bo = "LSBFirst";
				xim->byte_order = LSBFirst;
				xim->bitmap_bit_order = LSBFirst;
			}
			if (! reported && ! quiet) {
				rfbLog("changing XImage byte order"
				    " to %s\n", bo);
				reported = 1;
			}
		}

		*ximg_ptr = xim;
		return 1;
	}

	xim = XShmCreateImage(dpy, visual, depth, ZPixmap, NULL, shm, w, h);

	if (xim == NULL) {
		rfbErr("XShmCreateImage(%s) failed.\n", name);
		X_UNLOCK;
		return 0;
	}

	*ximg_ptr = xim;

	shm->shmid = shmget(IPC_PRIVATE,
	    xim->bytes_per_line * xim->height, IPC_CREAT | 0777);

	if (shm->shmid == -1) {
		rfbErr("shmget(%s) failed.\n", name);
		perror("shmget");

		XDestroyImage(xim);
		*ximg_ptr = NULL;

		X_UNLOCK;
		return 0;
	}

	shm->shmaddr = xim->data = (char *) shmat(shm->shmid, 0, 0);

	if (shm->shmaddr == (char *)-1) {
		rfbErr("shmat(%s) failed.\n", name);
		perror("shmat");

		XDestroyImage(xim);
		*ximg_ptr = NULL;

		shmctl(shm->shmid, IPC_RMID, 0);
		shm->shmid = -1;

		X_UNLOCK;
		return 0;
	}

	shm->readOnly = False;

	if (! XShmAttach(dpy, shm)) {
		rfbErr("XShmAttach(%s) failed.\n", name);
		XDestroyImage(xim);
		*ximg_ptr = NULL;

		shmdt(shm->shmaddr);
		shm->shmaddr = (char *) -1;

		shmctl(shm->shmid, IPC_RMID, 0);
		shm->shmid = -1;

		X_UNLOCK;
		return 0;
	}

	X_UNLOCK;
	return 1;
}

void shm_delete(XShmSegmentInfo *shm) {
	if (! using_shm) {
		return;
	}
	if (shm->shmaddr != (char *) -1) {
		shmdt(shm->shmaddr);
	}
	if (shm->shmid != -1) {
		shmctl(shm->shmid, IPC_RMID, 0);
	}
}

void shm_clean(XShmSegmentInfo *shm, XImage *xim) {
	if (! using_shm || nofb) {
		return;
	}
	X_LOCK;
	if (shm->shmid != -1) {
		XShmDetach(dpy, shm);
	}
	if (xim != NULL) {
		XDestroyImage(xim);
	}
	X_UNLOCK;

	shm_delete(shm);
}

void initialize_shm() {
	int i;

	/* set all shm areas to "none" before trying to create any */
#ifdef SINGLE_TILE_SHM
	tile_shm.shmid		= -1;
	tile_shm.shmaddr	= (char *) -1;
	tile			= NULL;
#endif
	scanline_shm.shmid	= -1;
	scanline_shm.shmaddr	= (char *) -1;
	scanline		= NULL;
	fullscreen_shm.shmid	= -1;
	fullscreen_shm.shmaddr	= (char *) -1;
	fullscreen		= NULL;
	for (i=1; i<=ntiles_x; i++) {
		tile_row_shm[i].shmid	= -1;
		tile_row_shm[i].shmaddr	= (char *) -1;
		tile_row[i]		= NULL;
	}

#ifdef SINGLE_TILE_SHM
	/* the tile (e.g. 32x32) shared memory area image: */

	if (! shm_create(&tile_shm, &tile, tile_x, tile_y, "tile")) {
		clean_up_exit(1);
	}
#endif

	/* the scanline (e.g. 1280x1) shared memory area image: */

	if (! shm_create(&scanline_shm, &scanline, dpy_x, 1, "scanline")) {
		clean_up_exit(1);
	}

	/*
	 * the fullscreen (e.g. 1280x1024/fs_factor) shared memory area image:
	 * (we cut down the size of the shm area to try avoid and shm segment
	 * limits, e.g. the default 1MB on Solaris)
	 */
	set_fs_factor(1024 * 1024);
	if (fs_frac >= 1.0) {
		fs_frac = 1.1;
		fs_factor = 0;
	}
	if (! fs_factor) {
		rfbLog("warning: fullscreen updates are disabled.\n");
	} else {
		if (! shm_create(&fullscreen_shm, &fullscreen, dpy_x,
		    dpy_y/fs_factor, "fullscreen")) {
			clean_up_exit(1);
		} 
	}

	/*
	 * for copy_tiles we need a lot of shared memory areas, one for
	 * each possible run length of changed tiles.  32 for 1024x768
	 * and 40 for 1280x1024, etc. 
	 */

	for (i=1; i<=ntiles_x; i++) {
		if (! shm_create(&tile_row_shm[i], &tile_row[i], tile_x * i,
		    tile_y, "tile_row")) {
			int j;
			if (i == 1) {
				clean_up_exit(1);
			}
			rfbLog("shm: Error creating shared memory tile-row for"
			    " len=%d,\n", i);
			rfbLog("shm: reverting to -onetile mode. If this"
			    " problem persists\n");
			rfbLog("shm: try using the -onetile or -noshm options"
			    " to limit\n");
			rfbLog("shm: shared memory usage, or run ipcrm(1)"
			    " to manually\n");
			rfbLog("shm: delete unattached shm segments.\n");
			/* n.b.: "i" not "1", a kludge for cleanup */
			single_copytile = i;
		}
		if (single_copytile && i >= 1) {
			/* only need 1x1 tiles */
			break;
		}
	}
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
	/* simply copy it to the global array for later use. */
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
 * copy_tiles() gives a slight improvement over copy_tile() since
 * adjacent runs of tiles are done all at once there is some savings
 * due to contiguous memory access.  Not a great speedup, but in
 * some cases it can be up to 2X.  Even more on a SunRay where no
 * graphics hardware is involved in the read.  Generally, graphics
 * devices are optimized for write, not read, so we are limited by
 * the read bandwidth, sometimes only 5 MB/sec on otherwise fast
 * hardware.
 */

int *first_line = NULL, *last_line;
unsigned short *left_diff, *right_diff;

void copy_tiles(int tx, int ty, int nt) {
	int x, y, line;
	int size_x, size_y, width, width1, width2;
	int off, len, n, dw, dx, i, t;
	int w1, w2, dx1, dx2;	/* tmps for normal and short tiles */
	int pixelsize = bpp >> 3;
	int first_min, last_max;

	int restored_patch = 0; /* for show_mouse */

	char *src, *dst, *s_src, *s_dst, *m_src, *m_dst;
	char *h_src, *h_dst;
	if (! first_line) {
		/* allocate arrays first time in. */
		int n = ntiles_x + 1;
		first_line = (int *) malloc((size_t) (n * sizeof(int)));
		last_line  = (int *) malloc((size_t) (n * sizeof(int)));
		left_diff  = (unsigned short *)
			malloc((size_t) (n * sizeof(unsigned short)));
		right_diff = (unsigned short *)
			malloc((size_t) (n * sizeof(unsigned short)));
	}

	x = tx * tile_x;
	y = ty * tile_y;

	size_x = dpy_x - x;
	if ( size_x > tile_x * nt ) {
		size_x = tile_x * nt;
		width1 = tile_x;
		width2 = tile_x;
	} else {
		/* short tile */
		width1 = tile_x;	/* internal tile */
		width2 = size_x - (nt - 1) * tile_x;	/* right hand tile */
	}

	size_y = dpy_y - y;
	if ( size_y > tile_y ) {
		size_y = tile_y;
	}

	n = tx + ty * ntiles_x;		/* number of the first tile */

	if (blackouts && tile_blackout[n].cover == 2) {
		/*
		 * If there are blackouts and this tile is completely covered
		 * no need to poll screen or do anything else..
		 * n.b. we are int single copy_tile mode: nt=1
		 */
		tile_has_diff[n] = 0;
		return;
	}

	X_LOCK;
	/* read in the whole tile run at once: */
	if ( using_shm && size_x == tile_x * nt && size_y == tile_y ) {
		/* general case: */
		XShmGetImage(dpy, window, tile_row[nt], x, y, AllPlanes);
	} else {
		/*
		 * No shm or near bottom/rhs edge case:
		 * (but only if tile size does not divide screen size)
		 */
		XGetSubImage(dpy, window, x, y, size_x, size_y, AllPlanes,
		    ZPixmap, tile_row[nt], 0, 0);
	}
	X_UNLOCK;

	if (blackouts && tile_blackout[n].cover == 1) {
		/*
		 * If there are blackouts and this tile is partially covered
		 * we should re-black-out the portion.
		 * n.b. we are int single copy_tile mode: nt=1
		 */
		int x1, x2, y1, y2, b;
		int w, s, fill = 0;

		for (b=0; b < tile_blackout[n].count; b++) {
			char *b_dst = tile_row[nt]->data;
			
			x1 = tile_blackout[n].bo[b].x1 - x;
			y1 = tile_blackout[n].bo[b].y1 - y;
			x2 = tile_blackout[n].bo[b].x2 - x;
			y2 = tile_blackout[n].bo[b].y2 - y;

			w = (x2 - x1) * pixelsize;
			s = x1 * pixelsize;

			for (line = 0; line < size_y; line++) {
				if (y1 <= line && line < y2) {
					memset(b_dst + s, fill, (size_t) w);
				}
				b_dst += tile_row[nt]->bytes_per_line;
			}
		}
	}

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

	src = tile_row[nt]->data;
	dst = screen->frameBuffer + y * bytes_per_line + x * pixelsize;

	s_src = src;
	s_dst = dst;

	for (t=1; t <= nt; t++) {
		first_line[t] = -1;
	}
	/* find the first line with difference: */
	w1 = width1 * pixelsize;
	w2 = width2 * pixelsize;


	/* foreach line: */
	for (line = 0; line < size_y; line++) {
		/* foreach horizontal tile: */
		for (t=1; t <= nt; t++) {
			if (first_line[t] != -1) {
				continue;
			}

			off = (t-1) * w1;
			if (t == nt) {
				len = w2;	/* possible short tile */
			} else {
				len = w1;
			}
			
			if (memcmp(s_dst + off, s_src + off, len)) {
				first_line[t] = line;
			}
		}
		s_src += tile_row[nt]->bytes_per_line;
		s_dst += bytes_per_line;
	}

	/* see if there were any differences for any tile: */
	first_min = -1;
	for (t=1; t <= nt; t++) {
		tile_tried[n+(t-1)] = 1;
		if (first_line[t] != -1) {
			if (first_min == -1 || first_line[t] < first_min) {
				first_min = first_line[t];
			}
		}
	}
	if (first_min == -1) {
		/* no tile has a difference, note this and get out: */
		for (t=1; t <= nt; t++) {
			tile_has_diff[n+(t-1)] = 0;
		}
		if (restored_patch) {
			redraw_mouse(); 
		}
		return;
	} else {
		/*
		 * at least one tile has a difference.  make sure info
		 * is recorded (e.g. sometimes we guess tiles and they
		 * came in with tile_has_diff 0)
		 */
		for (t=1; t <= nt; t++) {
			if (first_line[t] == -1) {
				tile_has_diff[n+(t-1)] = 0;
			} else {
				tile_has_diff[n+(t-1)] = 1;
			}
		}
	}

	m_src = src + (tile_row[nt]->bytes_per_line * size_y);
	m_dst = dst + (bytes_per_line * size_y);

	for (t=1; t <= nt; t++) {
		last_line[t] = first_line[t];
	}

	/* find the last line with difference: */
	w1 = width1 * pixelsize;
	w2 = width2 * pixelsize;

	/* foreach line: */
	for (line = size_y - 1; line > first_min; line--) {

		m_src -= tile_row[nt]->bytes_per_line;
		m_dst -= bytes_per_line;

		/* foreach tile: */
		for (t=1; t <= nt; t++) {
			if (first_line[t] == -1
			    || last_line[t] != first_line[t]) {
				/* tile has no changes or already done */
				continue;
			}

			off = (t-1) * w1;
			if (t == nt) {
				len = w2;	/* possible short tile */
			} else {
				len = w1;
			}
			if (memcmp(m_dst + off, m_src + off, len)) {
				last_line[t] = line;
			}
		}
	}
	
	/*
	 * determine the farthest down last changed line
	 * will be used below to limit our memcpy() to the framebuffer.
	 */
	last_max = -1;
	for (t=1; t <= nt; t++) {
		if (first_line[t] == -1) {
			continue;
		}
		if (last_max == -1 || last_line[t] > last_max) {
			last_max = last_line[t];
		}
	}

	/* look for differences on left and right hand edges: */
	for (t=1; t <= nt; t++) {
		left_diff[t] = 0;
		right_diff[t] = 0;
	}

	h_src = src;
	h_dst = dst;

	w1 = width1 * pixelsize;
	w2 = width2 * pixelsize;

	dx1 = (width1 - tile_fuzz) * pixelsize;
	dx2 = (width2 - tile_fuzz) * pixelsize;
	dw = tile_fuzz * pixelsize; 

	/* foreach line: */
	for (line = 0; line < size_y; line++) {
		/* foreach tile: */
		for (t=1; t <= nt; t++) {
			if (first_line[t] == -1) {
				/* tile has no changes at all */
				continue;
			}

			off = (t-1) * w1;
			if (t == nt) {
				dx = dx2;	/* possible short tile */
				if (dx <= 0) {
					break;
				}
			} else {
				dx = dx1;
			}

			if (! left_diff[t] && memcmp(h_dst + off,
			    h_src + off, dw)) {
				left_diff[t] = 1;
			}
			if (! right_diff[t] && memcmp(h_dst + off + dx,
			    h_src + off + dx, dw) ) {
				right_diff[t] = 1;
			}
		}
		h_src += tile_row[nt]->bytes_per_line;
		h_dst += bytes_per_line;
	}

	/* now finally copy the difference to the rfb framebuffer: */
	s_src = src + tile_row[nt]->bytes_per_line * first_min;
	s_dst = dst + bytes_per_line * first_min;

	for (line = first_min; line <= last_max; line++) {
		/* for I/O speed we do not do this tile by tile */
		memcpy(s_dst, s_src, size_x * pixelsize);
		s_src += tile_row[nt]->bytes_per_line;
		s_dst += bytes_per_line;
	}

	if (restored_patch) {
		redraw_mouse(); 
	}

	/* record all the info in the region array for this tile: */
	for (t=1; t <= nt; t++) {
		int s = t - 1;

		if (first_line[t] == -1) {
			/* tile unchanged */
			continue;
		}
		tile_region[n+s].first_line = first_line[t];
		tile_region[n+s].last_line  = last_line[t];

		tile_region[n+s].top_diff = 0;
		tile_region[n+s].bot_diff = 0;
		if ( first_line[t] < tile_fuzz ) {
			tile_region[n+s].top_diff = 1;
		}
		if ( last_line[t] > (size_y - 1) - tile_fuzz ) {
			tile_region[n+s].bot_diff = 1;
		}

		tile_region[n+s].left_diff  = left_diff[t];
		tile_region[n+s].right_diff = right_diff[t];
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
				copy_tiles(x, y, 1);
			}
			if (! tile_has_diff[n]) {
				/*
				 * n.b. copy_tiles() may have detected
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
 * Routine analogous to copy_all_tiles() above, but for horizontal runs
 * of adjacent changed tiles.
 */
int copy_all_tile_runs() {
	int x, y, n, m, i;
	int diffs = 0;
	int in_run = 0, run = 0;
	int ntave = 0, ntcnt = 0;

	for (y=0; y < ntiles_y; y++) {
		for (x=0; x < ntiles_x + 1; x++) {
			n = x + y * ntiles_x;

			if (x != ntiles_x && tile_has_diff[n]) {
				in_run = 1;
				run++;
			} else {
				if (! in_run) {
					in_run = 0;
					run = 0;
					continue;
				}
				copy_tiles(x - run, y, run);

				ntcnt++;
				ntave += run;
				diffs += run;

				/* neighboring tile downward: */
				for (i=1; i <= run; i++) {
					if ((y+1) < ntiles_y
					    && tile_region[n-i].bot_diff) {
						m = (x-i) + (y+1) * ntiles_x;
						if (! tile_has_diff[m]) {
							tile_has_diff[m] = 2;
						}
					}
				}

				/* neighboring tile to right: */
				if (((x-1)+1) < ntiles_x
				    && tile_region[n-1].right_diff) {
					m = ((x-1)+1) + y * ntiles_x;
					if (! tile_has_diff[m]) {
						tile_has_diff[m] = 2;
					}
					
					/* note that this starts a new run */
					in_run = 1;
					run = 1;
				} else {
					in_run = 0;
					run = 0;
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
				copy_tiles(x, y-1, 1);
			}
		}

		m = (x-1) + y * ntiles_x;	/* neighboring tile to left */

		if (x >= 1 && ! tile_has_diff[m] && tile_region[n].left_diff) {
			if (! tile_tried[m]) {
				tile_has_diff[m] = 2;
				copy_tiles(x-1, y, 1);
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

		copy_tiles(xt, yt, 1);
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
 * BTW, grow_islands() is actually pretty successful at doing this too...
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

		copy_tiles(u, v, 1);
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
	int i, y, block_size;

	block_size = (dpy_x * (dpy_y/fs_factor) * pixelsize);

	rfb_fb = screen->frameBuffer;
	y = 0;

	X_LOCK;

	/* screen may be too big for 1 shm area, so broken into fs_factor */
	for (i=0; i < fs_factor; i++) {
		if (using_shm) {
			XShmGetImage(dpy, window, fullscreen, 0, y, AllPlanes);
		} else {
			XGetSubImage(dpy, window, 0, y, fullscreen->width,
			    fullscreen->height, AllPlanes, ZPixmap, fullscreen,
			    0, 0);
		}
		memcpy(rfb_fb, fullscreen->data, (size_t) block_size);

		y += dpy_y / fs_factor;
		rfb_fb += block_size;
	}

	X_UNLOCK;

	if (blackouts) {
		blackout_regions();
	}

	rfbMarkRectAsModified(screen, 0, 0, dpy_x, dpy_y);
}

/* profiling routines */

double dtime(double *t_old) {
	/* 
	 * usage: call with 0.0 to initialize, subsequent calls give
	 * the time differences.
	 */
	double t_now, dt;
	struct timeval now;

	gettimeofday(&now, NULL);
	t_now = now.tv_sec + ( (double) now.tv_usec/1000000. );
	if (*t_old == 0) {
		*t_old = t_now;
		return t_now;
	}
	dt = t_now - *t_old;
	*t_old = t_now;
	return(dt);
}


/*
 * Utilities for managing the "naps" to cut down on amount of polling.
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

	/* split up a long nap to improve the wakeup time */
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

/*
 * This is called to avoid a ~20 second timeout in libvncserver.
 * May no longer be needed.
 */
void ping_clients(int tile_cnt) {
	static time_t last_send = 0;
	time_t now = time(0);

	if (rfbMaxClientWait < 20000) {
		rfbMaxClientWait = 20000;
		rfbLog("reset rfbMaxClientWait to %d ms.\n",
		    rfbMaxClientWait);
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
 * scan_display() wants to know if this tile can be skipped due to
 * blackout regions: (no data compare is done, just a quick geometric test)
 */
int blackout_line_skip(int n, int x, int y, int rescan, int *tile_count) {
	
	if (tile_blackout[n].cover == 2) {
		tile_has_diff[n] = 0;
		return 1;	/* skip it */

	} else if (tile_blackout[n].cover == 1) {
		int w, x1, y1, x2, y2, b, hit = 0;
		if (x + NSCAN > dpy_x) {
			w = dpy_x - x;
		} else {
			w = NSCAN;
		}

		for (b=0; b < tile_blackout[n].count; b++) {
			
			/* n.b. these coords are in full display space: */
			x1 = tile_blackout[n].bo[b].x1;
			x2 = tile_blackout[n].bo[b].x2;
			y1 = tile_blackout[n].bo[b].y1;
			y2 = tile_blackout[n].bo[b].y2;

			if (x2 - x1 < w) {
				/* need to cover full width */
				continue;
			}
			if (y1 <= y && y < y2) {
				hit = 1;
				break;
			}
		}
		if (hit) {
			if (! rescan) {
				tile_has_diff[n] = 0;
			} else {
				*tile_count += tile_has_diff[n];
			}
			return 1;	/* skip */
		}
	}
	return 0;	/* do not skip */
}

/*
 * scan_display() wants to know if this changed tile can be skipped due
 * to blackout regions (we do an actual compare to find the changed region).
 */
int blackout_line_cmpskip(int n, int x, int y, char *dst, char *src,
    int w, int pixelsize) {

	int i, x1, y1, x2, y2, b, hit = 0;
	int beg = -1, end = -1; 

	if (tile_blackout[n].cover == 0) {
		return 0;	/* 0 means do not skip it. */
	} else if (tile_blackout[n].cover == 2) {
		return 1;	/* 1 means skip it. */
	}

	/* tile has partial coverage: */

	for (i=0; i < w * pixelsize; i++)  {
		if (*(dst+i) != *(src+i)) {
			beg = i/pixelsize;	/* beginning difference */
			break;
		}
	}
	for (i = w * pixelsize - 1; i >= 0; i--)  {
		if (*(dst+i) != *(src+i)) {
			end = i/pixelsize;	/* ending difference */
			break;
		}
	}
	if (beg < 0 || end < 0) {
		/* problem finding range... */
		return 0;
	}

	/* loop over blackout rectangles: */
	for (b=0; b < tile_blackout[n].count; b++) {
		
		/* y in full display space: */
		y1 = tile_blackout[n].bo[b].y1;
		y2 = tile_blackout[n].bo[b].y2;

		/* x relative to tile origin: */
		x1 = tile_blackout[n].bo[b].x1 - x;
		x2 = tile_blackout[n].bo[b].x2 - x;

		if (y1 > y || y >= y2) {
			continue;
		}
		if (x1 <= beg && end <= x2) {
			hit = 1;
			break;
		}
	}
	if (hit) {
		return 1;
	} else {
		return 0;
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
	int whole_line = 1, nodiffs = 0;

	y = ystart;

	while (y < dpy_y) {

		/* grab the horizontal scanline from the display: */
		X_LOCK;
		if (using_shm) {
			XShmGetImage(dpy, window, scanline, 0, y, AllPlanes);
		} else {
			XGetSubImage(dpy, window, 0, y, scanline->width,
			    scanline->height, AllPlanes, ZPixmap, scanline,
			    0, 0);
		}
		X_UNLOCK;

		/* for better memory i/o try the whole line at once */
		src = scanline->data;
		dst = screen->frameBuffer + y * bytes_per_line;

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

			if (blackouts) {
				if (blackout_line_skip(n, x, y, rescan,
				    &tile_count)) {
					x += NSCAN;
					continue;
				}
			}

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
				if (! blackouts) {
					tile_has_diff[n] = 1;
					tile_count++;		
				} else {
					if (blackout_line_cmpskip(n, x, y,
					    dst, src, w, pixelsize)) {
						tile_has_diff[n] = 0;
					} else {
						tile_has_diff[n] = 1;
						tile_count++;		
					}
				}
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
	double frac2 = 0.35;  /* or 3rd */
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
		/* some periodic maintenance */

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
	scan_in_progress = 1;
	tile_count = scan_display(scanlines[count], 0);

	nap_set(tile_count);

	if (fs_factor && frac1 >= fs_frac) {
		/* make frac1 < fs_frac if fullscreen updates are enabled */
		frac1 = fs_frac/2.0;
	}

	if (tile_count > frac1 * ntiles) {
		/*
		 * many tiles have changed, so try a rescan (since it should
		 * be short compared to the many upcoming copy_tiles() calls)
		 */

		/* this check is done to skip the extra scan_display() call */
		if (! fs_factor || tile_count <= fs_frac * ntiles) {
			int cp, tile_count_old = tile_count;
			
			/* choose a different y shift for the 2nd scan: */
			cp = (NSCAN - count) % NSCAN;

			tile_count = scan_display(scanlines[cp], 1);

			if (tile_count >= (1 + frac2) * tile_count_old) {
				/* on a roll... do a 3rd scan */
				cp = (NSCAN - count + 7) % NSCAN;
				tile_count = scan_display(scanlines[cp], 1);
			}
		}
		scan_in_progress = 0;

		/*
		 * At some number of changed tiles it is better to just
		 * copy the full screen at once.  I.e. time = c1 + m * r1
		 * where m is number of tiles, r1 is the copy_tiles()
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
			fb_copy_in_progress = 1;
			copy_screen();
			if (show_mouse || cursor_pos) {
				if (show_mouse && ! use_threads) {
					redraw_mouse();
				}
				update_mouse();
			}
			fb_copy_in_progress = 0;
			if (use_threads && ! old_pointer) {
				pointer(-1, 0, 0, NULL);
			}
			nap_check(tile_count);
			return;
		}
	}
	scan_in_progress = 0;

	/* copy all tiles with differences from display to rfb framebuffer: */
	fb_copy_in_progress = 1;

	if (single_copytile) {
		/*
		 * Old way, copy I/O one tile at a time.
		 */
		tile_diffs = copy_all_tiles();
	} else {
		/* 
		 * New way, does runs of horizontal tiles at once.
		 * Note that below, for simplicity, the extra tile finding
		 * (e.g. copy_tiles_backward_pass) is done the old way.
		 */
		tile_diffs = copy_all_tile_runs();
	}

	/*
	 * This backward pass for upward and left tiles complements what
	 * was done in copy_all_tiles() for downward and right tiles.
	 */
	tile_diffs = copy_tiles_backward_pass();

	/* Given enough tile diffs, try the islands: */
	if (grow_fill && tile_diffs > 4) {
		tile_diffs = grow_islands();
	}

	/* Given enough tile diffs, try the gaps: */
	if (gaps_fill && tile_diffs > 4) {
		tile_diffs = fill_tile_gaps();
	}

	fb_copy_in_progress = 0;
	if (use_threads && ! old_pointer) {
		/*
		 * tell the pointer handler it can process any queued
		 * pointer events:
		 */
		pointer(-1, 0, 0, NULL);
	}

	if (blackouts) {
		/* ignore any diffs in completely covered tiles */
		int x, y, n;
		for (y=0; y < ntiles_y; y++) {
			for (x=0; x < ntiles_x; x++) {
				n = x + y * ntiles_x;
				if (tile_blackout[n].cover == 2) {
					tile_has_diff[n] = 0;
				}
			}
		}
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
	if (show_mouse || cursor_pos) {
		if (show_mouse && ! use_threads) {
			redraw_mouse();
		}
		update_mouse();
	}

	nap_check(tile_diffs);
}

void watch_loop(void) {
	int cnt = 0;
	double dt = 0.0;

	if (use_threads) {
		rfbRunEventLoop(screen, -1, TRUE);
	}

	while (1) {

		got_user_input = 0;
		got_pointer_input = 0;
		got_keyboard_input = 0;

		if (! use_threads) {
			rfbProcessEvents(screen, -1);
			if (! nofb && check_user_input(dt, &cnt)) {
				/* true means loop back for more input */
				continue;
			}
		}

		if (shut_down) {
			clean_up_exit(0);
		}

		watch_xevents();
		check_connect_inputs();		

		if (! screen->rfbClientHead) {	/* waiting for a client */
			usleep(200 * 1000);
			continue;
		}

		if (nofb) {	/* no framebuffer polling needed */
			if (cursor_pos) {
				update_mouse();
			}
			continue;
		}

		if (watch_bell) {
			/*
			 * check for any bell events.
			 * n.b. assumes -nofb folks do not want bell...
			 */
			watch_bell_event();
		}
		if (! show_dragging && button_mask) {
			/* if any button is pressed do not update screen */
			/* XXX consider: use_threads || got_pointer_input */
			X_LOCK;
			XFlush(dpy);
			X_UNLOCK;
		} else {
			/* for timing the scan to try to detect thrashing */
			double tm = 0.0;
			dtime(&tm);

			rfbUndrawCursor(screen);
			scan_for_updates();

			dt = dtime(&tm);
		}

		/* sleep a bit to lessen load */
		usleep(waitms * 1000);
		cnt++;
	}
}

/*
 * We need to handle user input, particularly pointer input, carefully.
 * This function is only called when non-threaded.  Note that
 * rfbProcessEvents() only processes *one* pointer event per call,
 * so if we interlace it with scan_for_updates(), we can get swamped
 * with queued up pointer inputs.  And if the pointer inputs are inducing
 * large changes on the screen (e.g. window drags), the whole thing
 * bogs down miserably and only comes back to life at some time after
 * one stops moving the mouse.  So, to first approximation, we are trying
 * to eat as much user input here as we can using some hints from the
 * duration of the previous scan_for_updates() call (in dt).
 *
 * return of 1 means watch_loop should short-circuit and reloop,
 * return of 0 means watch_loop should proceed to scan_for_updates().
 */

int check_user_input(double dt, int *cnt) {
	
	if (old_pointer) {
		/* every n-th drops thru to scan */
		if ((got_user_input || ui_skip < 0) && *cnt % ui_skip != 0) {
			*cnt++;
			XFlush(dpy);
			return 1;	/* short circuit watch_loop */
		} else {
			return 0;
		}
	}

	if (got_keyboard_input) {
		if (*cnt % ui_skip != 0) {
			*cnt++;
			return 1;	/* short circuit watch_loop */
		}
		/* otherwise continue with pointer input */
	}

	if (got_pointer_input) {
		int eaten = 0, miss = 0, max_eat = 50;
		int g, g_in;
		double spin = 0.0, tm = 0.0;
		double quick_spin_fac  = 0.40;
		double grind_spin_time = 0.175;


		dtime(&tm);
		g = g_in = got_pointer_input;

		/*
		 * Try for some "quick" pointer input processing.
		 *
		 * About as fast as we can, we try to process user input
		 * calling rfbProcessEvents or rfbCheckFds.  We do this
		 * for a time on order of the last scan_for_updates() time,
		 * dt, but if we stop getting user input we break out.
		 * We will also break out if we have processed max_eat
		 * inputs.
		 *
		 * Note that rfbCheckFds() does not send any framebuffer
		 * updates, so is more what we want here, although it is
		 * likely they have all be sent already.
		 */
		while (1) {
			rfbCheckFds(screen, 1000);
			XFlush(dpy);

			spin += dtime(&tm);

			if (spin > quick_spin_fac * dt) {
				/* get out if spin time comparable to last scan time */
				break;
			}
			if (got_pointer_input > g) {
				g = got_pointer_input;
				if (eaten++ < max_eat) {
					continue;
				}
			} else {
				miss++;
			}
			if (miss > 1) {	/* 1 means out on 2nd miss */
				break;
			}
		}


		/*
		 * Probably grinding with a lot of fb I/O if dt is
		 * this large.  (need to do this more elegantly) 
		 *
		 * Current idea is to spin our wheels here *not* processing
		 * any fb I/O, but still processing the user input.
		 * This user input goes to the X display and changes it,
		 * but we don't poll it while we "rest" here for a time
		 * on order of dt, the previous scan_for_updates() time.
		 * We also break out if we miss enough user input.
		 */
		if (dt > grind_spin_time) {
			int i, ms, split = 30;
			double shim;

			/*
			 * Break up our pause into 'split' steps.
			 * We get at most one input per step.
			 */
			shim = 0.75 * dt / split;

			ms = (int) (1000 * shim);

			/* cutoff how long the pause can be */
			if (split * ms > 300) {
				ms = 300 / split;
			}

			spin = 0.0;
			tm = 0.0;
			dtime(&tm);

			g = got_pointer_input;
			miss = 0;
			for (i=0; i<split; i++) {
				usleep(ms * 1000);
				rfbCheckFds(screen, 1000);
				spin += dtime(&tm);
				if (got_pointer_input > g) {
					XFlush(dpy);
					miss = 0;
				} else {
					miss++;
				}
				g = got_pointer_input;
				if (miss > 2) {
					break;
				}
				if (1000 * spin > ms * split)  {
					break;
				}
			}
		}
	}
	return 0;
}

void print_help() {
	char help[] = 
"\n"
"x11vnc: allow VNC connections to real X11 displays.\n"
"\n"
"Typical usage is:\n"
"\n"
"   Run this command in a shell on the remote machine \"far-host\":\n"
"\n"
"       x11vnc -display :0\n"
"\n"
"   Then run this in another window on the machine you are sitting at:\n"
"\n"
"       vncviewer far-host:0\n"
"\n"
"Once x11vnc establishes connections with the X11 server and starts\n"
"listening as a VNC server it will print out a string: PORT=XXXX where\n"
"XXXX is typically 5900 (the default VNC port).  One would next run something\n"
"like this on the local machine: \"vncviewer host:N\" where N is XXXX - 5900.\n" 
"\n"
"By default x11vnc will not allow the screen to be shared and it will\n"
"exit as soon as a client disconnects.  See -shared and -forever below\n"
"to override these protections.\n"
"\n"
"Options:\n"
"\n"
"-display disp          X11 server display to connect to, the X server process\n"
"                       must be running on same machine and support MIT-SHM.\n"
"-id windowid           Show the window corresponding to <windowid> not the\n"
"                       entire display. Warning: bugs! new toplevels missed!...\n"
"-flashcmap             In 8bpp indexed color, let the installed colormap flash\n"
"                       as the pointer moves from window to window (slow).\n"
"-notruecolor           Force 8bpp indexed color even if it looks like TrueColor.\n"
"\n"
"-visual n              Experimental option: probably does not do what you think.\n"
"                       It simply *forces* the visual used for the framebuffer;\n"
"                       this may be a bad thing... It is useful for testing and\n"
"                       for some workarounds.  n may be a decimal number, or 0x\n"
"                       hex.  Run xdpyinfo(1) for the values. One may also use\n"
"                       \"TrueColor\", etc. see <X11/X.h> for a list.  If the\n"
"                       string ends in \":m\" the visual depth is forced to be m.\n"
"\n"
"-viewonly              Clients can only watch (default %s).\n"
"-shared                VNC display is shared (default %s).\n"
"-forever               Keep listening for more connections rather than exiting\n"
"                       as soon as the first client(s) disconnect. Same as -many\n"
"-connect string        For use with \"vncviewer -listen\" reverse connections. If\n"
"                       string has the form \"host\" or \"host:port\" the connection\n"
"                       is made once at startup. Use commas for a list. If string\n"
"                       contains \"/\" it is a file to periodically check for new\n"
"                       hosts. The first line is read and then file is truncated.\n"
"-vncconnect            Monitor the VNC_CONNECT X property set by vncconnect(1).\n"
"-auth file             Set the X authority file to be \"file\", equivalent to\n"
"                       setting the XAUTHORITY env. var to \"file\" before startup.\n"
"-allow addr1[,addr2..] Only allow client connections from IP addresses matching\n"
"                       the comma separated list of numerical addresses. Can be\n"
"                       a prefix, e.g. \"192.168.100.\" to match a simple subnet,\n"
"                       for more control build libvncserver with libwrap support.\n"
"-localhost             Same as -allow 127.0.0.1\n"
"-inetd                 Launched by inetd(1): stdio instead of listening socket.\n"
"\n"
"-noshm                 Do not use the MIT-SHM extension for the polling.\n"
"                       remote displays can be polled this way: be careful\n"
"                       this can use large amounts of network bandwidth.\n"
"-flipbyteorder         Sometimes needed if remotely polled host has different\n"
"                       endianness.  Ignored unless -noshm is set.\n"
"-blackout string       Black out rectangles on the screen. string is a comma\n"
"                       separated list of WxH+X+Y type geometries for each rect.\n"
"-xinerama              If your screen is composed of multiple monitors glued\n"
"                       together via XINERAMA, and that screen is non-rectangular\n"
"                       this option will try to guess the areas to black out.\n"
"\n"
"-q                     Be quiet by printing less informational output.\n" 
"-bg                    Go into the background after screen setup.\n" 
"                       Something like this could be useful in a script:\n"
"                         port=`ssh $host \"x11vnc -display :0 -bg\" | grep PORT`\n"
"                         port=`echo \"$port\" | sed -e 's/PORT=//'`\n"
"                         port=`expr $port - 5900`\n"
"                         vncviewer $host:$port\n"
"\n"
"-modtweak              Handle AltGr/Shift modifiers for differing languages\n"
"                       between client and host (default %s).\n"
"-nomodtweak            Send the keysym directly to the X server.\n"
"-remap string          Read keysym remappings from file \"string\".  Format is\n"
"                       one pair of keysyms per line (can be name or hex value).\n"
"                       \"string\" can also be of form: key1:key2,key3:key4...\n"
"-nobell                Do not watch for XBell events.\n"
"-nofb                  Ignore framebuffer: only process keyboard and pointer.\n"
"-nosel                 Do not manage exchange of X selection/cutbuffer.\n"
"-noprimary             Do not poll the PRIMARY selection for changes and send\n"
"                       back to clients.  PRIMARY is set for received changes.\n"
"\n"
"-nocursor              Do not have the viewer show a local cursor.\n"
"-mouse                 Draw a 2nd cursor at the current X pointer position.\n"
"-mouseX                As -mouse, but also draw an X on root background.\n"
"-X                     Shorthand for -mouseX -nocursor.\n"
"-xwarppointer          Move the pointer with XWarpPointer instead of XTEST\n"
"                       (try as a workaround if pointer behaves poorly, e.g.\n"
"                       on touchscreens or other non-standard setups).\n"
"-cursorpos             Send the X cursor position back to all vnc clients that\n"
"                       support the TightVNC CursorPosUpdates extension.\n"
"-buttonmap str         String to remap mouse buttons.  Format: IJK-LMN, this\n"
"                       maps buttons I -> L, etc., e.g.  -buttonmap 13-31\n"
"-nodragging            Do not update the display during mouse dragging events\n"
"                       (mouse motion with a button held down).  Greatly\n"
"                       improves response on slow setups, but you lose all\n"
"                       visual feedback for drags, text selection, and some\n"
"                       menu traversals.\n"
"-old_pointer           Do not use the new pointer input handling mechanisms.\n"
"                       See check_input() and pointer() for details.\n"
"-input_skip n          For the old pointer handling when non-threaded: try to\n"
"                       read n user input events before scanning display. n < 0\n"
"                       means to act as though there is always user input.\n"
"\n"
"-debug_pointer         Print debugging output for every pointer event.\n"
"-debug_keyboard        Print debugging output for every keyboard event.\n"
"\n"
"-defer time            Time in ms to wait for updates before sending to\n"
"                       client [rfbDeferUpdateTime]  (default %d).\n"
"-wait time             Time in ms to pause between screen polls.  Used\n"
"                       to cut down on load (default %d).\n"
"-nap                   Monitor activity and if low take longer naps between\n" 
"                       polls to really cut down load when idle (default %s).\n"
"-sigpipe string        Broken pipe (SIGPIPE) handling. string can be \"ignore\"\n"
"                       or \"exit\", for the 1st libvncserver will handle the\n"
"                       abrupt loss of a client and continue, for the 2nd x11vnc\n"
"                       will cleanup and exit at the 1st broken connection.\n"
"                       Default is \"ignore\".\n"
#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
"-threads               Whether or not to use the threaded libvncserver\n"
"-nothreads             algorithm [rfbRunEventLoop] (default %s).\n"
#endif
"\n"
"-fs f                  If the fraction of changed tiles in a poll is greater\n"
"                       than f, the whole screen is updated (default %.2f).\n"
"-onetile               Do not use the new copy_tiles() framebuffer mechanism,\n"
"                       just use 1 shm tile for polling.  Same as -old_copytile.\n"
"-gaps n                Heuristic to fill in gaps in rows or cols of n or less\n"
"                       tiles.  Used to improve text paging (default %d).\n"
"-grow n                Heuristic to grow islands of changed tiles n or wider\n"
"                       by checking the tile near the boundary (default %d).\n"
"-fuzz n                Tolerance in pixels to mark a tiles edges as changed\n"
"                       (default %d).\n"
"-hints                 Use krfb/x0rfbserver hints (glue changed adjacent\n"
"                       horizontal tiles into one big rectangle)  (default %s).\n"
"-nohints               Do not use hints; send each tile separately.\n"
"%s\n"
"\n"
"These options are passed to libvncserver:\n"
"\n"
;
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
#ifdef LIBVNCSERVER_HAVE_GETHOSTNAME
		if (gethostname(host, MAXN) == 0) {
			strncpy(title, host, MAXN - strlen(title));
		}
#endif
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
	int i, op, ev, er, maj, min;
	char *use_dpy = NULL;
	char *auth_file = NULL;
	char *arg, *visual_str = NULL;
	int pw_loc = -1;
	int dt = 0;
	int bg = 0;
	int got_waitms = 0, got_rfbwait = 0;
	int got_deferupdate = 0, got_defer = 0;

	/* used to pass args we do not know about to rfbGetScreen(): */
	int argc2 = 1; char *argv2[100];

	argv2[0] = strdup(argv[0]);
	
	for (i=1; i < argc; i++) {
		/* quick-n-dirty --option handling. */
		arg = argv[i];
		if (strstr(arg, "--") == arg) {
			arg++;
		}

		if (!strcmp(arg, "-display")) {
			use_dpy = argv[++i];
		} else if (!strcmp(arg, "-id")) {
			if (sscanf(argv[++i], "0x%x", &subwin) != 1) {
				if (sscanf(argv[i], "%d", &subwin) != 1) {
					fprintf(stderr, "bad -id arg: %s\n",
					    argv[i]);
					exit(1);
				}
			}
		} else if (!strcmp(arg, "-visual")) {
			visual_str = argv[++i];
		} else if (!strcmp(arg, "-flashcmap")) {
			flash_cmap = 1;
		} else if (!strcmp(arg, "-notruecolor")) {
			force_indexed_color = 1;
		} else if (!strcmp(arg, "-viewonly")) {
			view_only = 1;
		} else if (!strcmp(arg, "-shared")) {
			shared = 1;
		} else if (!strcmp(arg, "-auth")) {
			auth_file = argv[++i];
		} else if (!strcmp(arg, "-allow")) {
			allow_list = argv[++i];
		} else if (!strcmp(arg, "-localhost")) {
			allow_list = "127.0.0.1";
		} else if (!strcmp(arg, "-many")
			|| !strcmp(arg, "-forever")) {
			connect_once = 0;
		} else if (!strcmp(arg, "-connect")) {
			i++;
			if (strchr(arg, '/')) {
				client_connect_file = argv[i];
			} else {
				client_connect = strdup(argv[i]);
			}
		} else if (!strcmp(arg, "-vncconnect")) {
			vnc_connect = 1;
		} else if (!strcmp(arg, "-inetd")) {
			inetd = 1;
		} else if (!strcmp(arg, "-noshm")) {
			using_shm = 0;
		} else if (!strcmp(arg, "-flipbyteorder")) {
			flip_byte_order = 1;
		} else if (!strcmp(arg, "-modtweak")) {
			use_modifier_tweak = 1;
		} else if (!strcmp(arg, "-nomodtweak")) {
			use_modifier_tweak = 0;
		} else if (!strcmp(arg, "-remap")) {
			remap_file = argv[++i];
		} else if (!strcmp(arg, "-blackout")) {
			blackout_string = argv[++i];
		} else if (!strcmp(arg, "-xinerama")) {
			xinerama = 1;
		} else if (!strcmp(arg, "-nobell")) {
			watch_bell = 0;
		} else if (!strcmp(arg, "-nofb")) {
			nofb = 1;
		} else if (!strcmp(arg, "-nosel")) {
			watch_selection = 0;
		} else if (!strcmp(arg, "-noprimary")) {
			watch_primary = 0;
		} else if (!strcmp(arg, "-nocursor")) {
			local_cursor = 0;
		} else if (!strcmp(arg, "-mouse")) {
			show_mouse = 1;
		} else if (!strcmp(arg, "-mouseX")) {
			show_mouse = 1;
			show_root_cursor = 1;
		} else if (!strcmp(arg, "-X")) {
			show_mouse = 1;
			show_root_cursor = 1;
			local_cursor = 0;
		} else if (!strcmp(arg, "-xwarppointer")) {
			use_xwarppointer = 1;
		} else if (!strcmp(arg, "-cursorpos")) {
			cursor_pos = 1;
		} else if (!strcmp(arg, "-buttonmap")) {
			pointer_remap = argv[++i];
		} else if (!strcmp(arg, "-nodragging")) {
			show_dragging = 0;
		} else if (!strcmp(arg, "-input_skip")) {
			ui_skip = atoi(argv[++i]);
			if (! ui_skip) ui_skip = 1;
		} else if (!strcmp(arg, "-old_pointer")) {
			old_pointer = 1;
		} else if (!strcmp(arg, "-onetile")
			|| !strcmp(arg, "-old_copytile")) {
			single_copytile = 1;
		} else if (!strcmp(arg, "-debug_pointer")) {
			debug_pointer++;
		} else if (!strcmp(arg, "-debug_keyboard")) {
			debug_keyboard++;
		} else if (!strcmp(arg, "-defer")) {
			defer_update = atoi(argv[++i]);
			got_defer = 1;
		} else if (!strcmp(arg, "-wait")) {
			waitms = atoi(argv[++i]);
			got_waitms = 1;
		} else if (!strcmp(arg, "-nap")) {
			take_naps = 1;
#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
		} else if (!strcmp(arg, "-threads")) {
			use_threads = 1;
		} else if (!strcmp(arg, "-nothreads")) {
			use_threads = 0;
#endif
		} else if (!strcmp(arg, "-sigpipe")) {
			if (!strcmp(argv[++i], "ignore"))  {
				sigpipe = 1;
			} else if (!strcmp(argv[i], "exit")) {
				sigpipe = 2;
			} else if (!strcmp(argv[i], "skip")) {
				sigpipe = 0;
			} else {
				fprintf(stderr, "bad -sigpipe arg: %s, must "
				    "be \"ignore\" or \"exit\"\n", argv[i]);
				exit(1);
			}
		} else if (!strcmp(arg, "-fs")) {
			fs_frac = atof(argv[++i]);
		} else if (!strcmp(arg, "-gaps")) {
			gaps_fill = atoi(argv[++i]);
		} else if (!strcmp(arg, "-grow")) {
			grow_fill = atoi(argv[++i]);
		} else if (!strcmp(arg, "-fuzz")) {
			tile_fuzz = atoi(argv[++i]);
		} else if (!strcmp(arg, "-hints")) {
			use_hints = 1;
		} else if (!strcmp(arg, "-nohints")) {
			use_hints = 0;
		} else if (!strcmp(arg, "-h") || !strcmp(arg, "-help")
			|| !strcmp(arg, "-?")) {
			print_help();
		} else if (!strcmp(arg, "-q") || !strcmp(arg, "-quiet")) {
			quiet = 1;
#ifdef LIBVNCSERVER_HAVE_SETSID
		} else if (!strcmp(arg, "-bg") || !strcmp(arg, "-background")) {
			bg = 1;
#endif
		} else {
			if (!strcmp(arg, "-desktop")) {
				dt = 1;
			}
			if (!strcmp(arg, "-passwd")) {
				pw_loc = i;
			}
			if (!strcmp(arg, "-rfbwait")) {
				got_rfbwait = 1;
			}
			if (!strcmp(arg, "-deferupdate")) {
				got_deferupdate = 1;
			}
			if (!strcmp(arg, "-rfbport")) {
				got_rfbport = 1;
			}
			if (!strcmp(arg, "-alwaysshared ")) {
				got_alwaysshared = 1;
			}
			if (!strcmp(arg, "-nevershared")) {
				got_nevershared = 1;
			}
			/* otherwise copy it for use below. */
			if (! quiet && i != pw_loc && i != pw_loc+1) {
			    fprintf(stderr, "passing arg to libvncserver: %s\n",
				arg);
			}
			if (argc2 < 100) {
				argv2[argc2++] = strdup(arg);
			}
		}
	}

	/*
	 * If -passwd was used, clear it out of argv.  This does not
	 * work on all UNIX, have to use execvp() in general...
	 */
	if (pw_loc > 0) {
		char *p = argv[pw_loc];		
		while (*p != '\0') {
			*p++ = '\0';
		}
		if (pw_loc+1 < argc) {
			p = argv[pw_loc+1];		
			while (*p != '\0') {
				*p++ = '\0';
			}
		}
	}

	/* fixup settings that do not make sense */
		
	if (use_threads && nofb && cursor_pos) {
		fprintf(stderr, "disabling -threads under -nofb -cursorpos\n");
		use_threads = 0;
	}
	if (tile_fuzz < 1) {
		tile_fuzz = 1;
	}
	if (waitms < 0) {
		waitms = 0;
	}
	if (! using_shm && ! got_waitms) {
		/* try to cut down on polling over network... */
		waitms *= 2;
	}
	if (inetd) {
		shared = 0;
		connect_once = 1;
		bg = 0;
	}

	/* increase rfbwait if threaded */
	if (use_threads && ! got_rfbwait) {
		argv2[argc2++] = "-rfbwait";
		argv2[argc2++] = "604800000"; /* one week... */
	}

	if (nofb && ! got_deferupdate && ! got_defer) {
		/* reduce defer time under -nofb */
		defer_update = defer_update_nofb;
	}
	if (! got_deferupdate) {
		char tmp[40];
		/* XXX not working yet in libvncserver */
		sprintf(tmp, "%d", defer_update);
		argv2[argc2++] = "-deferupdate";
		argv2[argc2++] = strdup(tmp);
	}

	if (! quiet) {
		fprintf(stderr, "\n");
		fprintf(stderr, "display:    %s\n", use_dpy ? use_dpy
                    : "null");
		fprintf(stderr, "subwin:     0x%x\n", subwin);
		fprintf(stderr, "visual:     %s\n", visual_str ? visual_str
                    : "null");
		fprintf(stderr, "flashcmap:  %d\n", flash_cmap);
		fprintf(stderr, "force_idx:  %d\n", force_indexed_color);
		fprintf(stderr, "viewonly:   %d\n", view_only);
		fprintf(stderr, "shared:     %d\n", shared);
		fprintf(stderr, "authfile:   %s\n", auth_file ? auth_file
                    : "null");
		fprintf(stderr, "allow:      %s\n", allow_list ? allow_list
                    : "null");
		fprintf(stderr, "conn_once:  %d\n", connect_once);
		fprintf(stderr, "connect:    %s\n", client_connect
		    ? client_connect : "null");
		fprintf(stderr, "connectfile %s\n", client_connect_file
		    ? client_connect_file : "null");
		fprintf(stderr, "vnc_conn:   %d\n", vnc_connect);
		fprintf(stderr, "inetd:      %d\n", inetd);
		fprintf(stderr, "using_shm:  %d\n", using_shm);
		fprintf(stderr, "flipbytes:  %d\n", flip_byte_order);
		fprintf(stderr, "mod_tweak:  %d\n", use_modifier_tweak);
		fprintf(stderr, "remap:      %s\n", remap_file ? remap_file
                    : "null");
		fprintf(stderr, "blackout:   %s\n", blackout_string
		    ? blackout_string : "null");
		fprintf(stderr, "xinerama:   %d\n", xinerama);
		fprintf(stderr, "watchbell:  %d\n", watch_bell);
		fprintf(stderr, "nofb:       %d\n", nofb);
		fprintf(stderr, "watchsel:   %d\n", watch_selection);
		fprintf(stderr, "watchprim:  %d\n", watch_primary);
		fprintf(stderr, "loc_curs:   %d\n", local_cursor);
		fprintf(stderr, "mouse:      %d\n", show_mouse);
		fprintf(stderr, "root_curs:  %d\n", show_root_cursor);
		fprintf(stderr, "xwarpptr:   %d\n", use_xwarppointer);
		fprintf(stderr, "cursorpos:  %d\n", cursor_pos);
		fprintf(stderr, "buttonmap:  %s\n", pointer_remap
		    ? pointer_remap : "null");
		fprintf(stderr, "dragging:   %d\n", show_dragging);
		fprintf(stderr, "inputskip:  %d\n", ui_skip);
		fprintf(stderr, "old_ptr:    %d\n", old_pointer);
		fprintf(stderr, "onetile:    %d\n", single_copytile);
		fprintf(stderr, "debug_ptr:  %d\n", debug_pointer);
		fprintf(stderr, "debug_key:  %d\n", debug_keyboard);
		fprintf(stderr, "defer:      %d\n", defer_update);
		fprintf(stderr, "waitms:     %d\n", waitms);
		fprintf(stderr, "take_naps:  %d\n", take_naps);
		fprintf(stderr, "sigpipe:    %d\n", sigpipe);
		fprintf(stderr, "threads:    %d\n", use_threads);
		fprintf(stderr, "fs_frac:    %.2f\n", fs_frac);
		fprintf(stderr, "gaps_fill:  %d\n", gaps_fill);
		fprintf(stderr, "grow_fill:  %d\n", grow_fill);
		fprintf(stderr, "tile_fuzz:  %d\n", tile_fuzz);
		fprintf(stderr, "use_hints:  %d\n", use_hints);
		fprintf(stderr, "bg:         %d\n", bg);
	} else {
		rfbLogEnable(0);
	}

	/* open the X display: */
	X_INIT;
	if (auth_file) {
		char *tmp;
		int len = strlen("XAUTHORITY=") + strlen(auth_file) + 1;
		tmp = (char *) malloc((size_t) len);
		sprintf(tmp, "XAUTHORITY=%s", auth_file);
		putenv(tmp);
	}
	if (use_dpy) {
		dpy = XOpenDisplay(use_dpy);
	} else if ( (use_dpy = getenv("DISPLAY")) ) {
		dpy = XOpenDisplay(use_dpy);
	} else {
		dpy = XOpenDisplay("");
	}

	if (! dpy) {
		fprintf(stderr, "XOpenDisplay failed (%s)\n",
		    use_dpy ? use_dpy:"null");
		exit(1);
	} else if (use_dpy) {
		if (! quiet) fprintf(stderr, "Using X display %s\n", use_dpy);
	} else {
		if (! quiet) fprintf(stderr, "Using default X display.\n");
	}

	/* check for XTEST */
	if (! XTestQueryExtension(dpy, &ev, &er, &maj, &min)) {
		fprintf(stderr, "Display does not support XTest extension.\n");
		exit(1);
	}

	/* check for MIT-SHM */
	if (! nofb && ! XShmQueryExtension(dpy)) {
		if (! using_shm) {
			fprintf(stderr, "warning: display does not support "
			    "XShm.\n");
		} else {
			fprintf(stderr, "Display does not support XShm "
			    "extension (must be local).\n");
			exit(1);
		}
	}

	if (visual_str != NULL) {
		set_visual(visual_str);
	}
#ifdef LIBVNCSERVER_HAVE_XKEYBOARD
	/* check for XKEYBOARD */
	if (watch_bell) {
		if (! XkbQueryExtension(dpy, &op, &ev, &er, &maj, &min)) {
			fprintf(stderr, "warning: disabling bell.\n");
			watch_bell = 0;
		} else {
			initialize_watch_bell();
		}
	}
#endif

	/*
	 * Window managers will often grab the display during resize, etc.
	 * To avoid deadlock (our user resize input is not processed)
	 * we tell the server to process our requests during all grabs:
	 */
	XTestGrabControl(dpy, True);

	scr = DefaultScreen(dpy);
	rootwin = RootWindow(dpy, scr);

	/* set up parameters for subwin or non-subwin cases: */
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
			fprintf(stderr, "bad window: 0x%x\n", window);
			exit(1);
		}
		dpy_x = attr.width;
		dpy_y = attr.height;
		visual = attr.visual;

		/* show_mouse has some segv crashes as well */
		if (show_root_cursor) {
			show_root_cursor = 0;
			fprintf(stderr, "disabling root cursor drawing for "
			    "subwindow\n");
		}

		set_offset();
	}

	/* initialize depth to reasonable value */
	depth = DefaultDepth(dpy, scr);

	/*
	 * User asked for non-default visual, this is not working well but it
	 * does some useful things...  What should it do in general?
	 */
	if (visual_id) {
		XVisualInfo vinfo_tmpl, *vinfo;
		int n;
		
		vinfo_tmpl.visualid = visual_id; 
		vinfo = XGetVisualInfo(dpy, VisualIDMask, &vinfo_tmpl, &n);
		if (vinfo == NULL || n == 0) {
			fprintf(stderr, "could not match visual_id: 0x%x\n",
			    visual_id);
			exit(1);
		}
		visual = vinfo->visual;
		depth = vinfo->depth;
		if (visual_depth) {
			depth = visual_depth;	/* force it */
		}
		if (! quiet) {
			fprintf(stderr, "vis id:     0x%x\n", vinfo->visualid);
			fprintf(stderr, "vis scr:      %d\n", vinfo->screen);
			fprintf(stderr, "vis depth     %d\n", vinfo->depth);
			fprintf(stderr, "vis class     %d\n", vinfo->class);
			fprintf(stderr, "vis rmask   0x%x\n", vinfo->red_mask);
			fprintf(stderr, "vis gmask   0x%x\n", vinfo->green_mask);
			fprintf(stderr, "vis bmask   0x%x\n", vinfo->blue_mask);
			fprintf(stderr, "vis cmap_sz   %d\n", vinfo->colormap_size);
			fprintf(stderr, "vis b/rgb     %d\n", vinfo->bits_per_rgb);
		}

		XFree(vinfo);
	}


	if (nofb || visual_id) {
		fb = XCreateImage(dpy, visual, depth, ZPixmap, 0, NULL,
		    dpy_x, dpy_y, BitmapPad(dpy), 0);
		/* 
		 * For -nofb we do not allocate the framebuffer, so we
		 * can save a few MB of memory. 
		 */
		if (! nofb) {
			fb->data = (char *) malloc(fb->bytes_per_line *
			    fb->height);
		}
	} else {
		fb = XGetImage(dpy, window, 0, 0, dpy_x, dpy_y, AllPlanes,
		    ZPixmap);
		if (! quiet) {
			fprintf(stderr, "Read initial data from X display into"
			    " framebuffer.\n");
		}
	}
	if (fb->bits_per_pixel == 24 && ! quiet) {
		fprintf(stderr, "warning: 24 bpp may have poor"
		    " performance.\n");
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

	/* rectangular blackout regions */
	if (blackout_string != NULL) {
		initialize_blackout(blackout_string);
	}
	if (xinerama) {
		initialize_xinerama();
	}
	if (blackouts) {
		blackout_tiles();
	}

	initialize_shm();	/* also creates XImages when using_shm = 0 */

	set_signals();

	if (blackouts) {	/* blackout fb as needed. */
		copy_screen();
	}

	if (use_modifier_tweak) {
		initialize_modtweak();
	}
	if (remap_file != NULL) {
		initialize_remap(remap_file);
	}

	if (screen->rfbPort) {
		fprintf(stdout, "PORT=%d\n", screen->rfbPort);
		fflush(stdout);	
	}
	if (! quiet) {
		rfbLog("screen setup finished.\n");
	}

#if defined(LIBVNCSERVER_HAVE_FORK) && defined(LIBVNCSERVER_HAVE_SETSID)
	if (bg) {
		/* fork into the background now */
		int p, n;
		if ((p = fork()) > 0)  {
			exit(0);
		} else if (p == -1) {
			fprintf(stderr, "could not fork\n");
			perror("fork");
			clean_up_exit(1);
		}
		if (setsid() == -1) {
			fprintf(stderr, "setsid failed\n");
			perror("setsid");
			clean_up_exit(1);
		}
		/* adjust our stdio */
		n = open("/dev/null", O_RDONLY);
		dup2(n, 0);
		dup2(n, 1);
		dup2(n, 2);
		if (n > 2) {
			close(n);
		}
	}
#endif

	watch_loop();

	return(0);
}
