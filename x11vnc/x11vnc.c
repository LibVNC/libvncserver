/*
 * x11vnc.c: a VNC server for X displays.
 *
 * Copyright (c) 2002-2004 Karl J. Runge <runge@karlrunge.com>
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
 * The algorithm of x0rfbserver was used as a base.  Additional heuristics
 * are also applied (currently there are a bit too many of these...)
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
 * sets up some audio side-channel such as esd.
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
 * 24 visuals will incorrectly display windows using the non-default one.
 *
 * Feature -id <windowid> can be picky: it can crash for things like the
 * window not sufficiently mapped into server memory, use of -mouse, etc.
 * SaveUnders menus, popups, etc will not be seen.
 *
 * Occasionally, a few tile updates can be missed leaving a patch of
 * color that needs to be refreshed.  This may only be when threaded,
 * which is no longer the default.
 *
 * There seems to be a serious bug with simultaneous clients when
 * threaded, currently the only workaround in this case is -nothreads.
 *
 */

/* 
 * These ' -- filename -- ' comments represent a partial cleanup:
 * they are an odd way to indicate how this huge file would be split up
 * someday into multiple files.  Not finished, externs and other things
 * would need to be done, but it indicates the breakup, including static
 * keyword for local items.
 *
 * The primary reason we do not break up this file is for user
 * convenience: those wanting to use the latest version download a single
 * file, x11vnc.c, and off they go...
 */

/* -- x11vnc.h -- */

#include <unistd.h>
#include <signal.h>
#include <sys/utsname.h>

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
#include <ctype.h>

#include <rfb/rfb.h>
#include <rfb/rfbregion.h>

#ifdef LIBVNCSERVER_HAVE_XKEYBOARD
#include <X11/XKBlib.h>
#endif

#ifdef LIBVNCSERVER_HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef LIBVNCSERVER_HAVE_NETINET_IN_H
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#endif

/* 
 * Temporary kludge: to run with -xinerama define the following
 * macro (uncomment) and be sure to link with -lXinerama
 * (e.g. LDFLAGS=-lXinerama before configure).  Support for this is
 * being added to libvncserver 'configure.ac' so it will all be done
 * automatically, but it won't be in users' build trees for a while,
 * so one can do it manually here.

#define LIBVNCSERVER_HAVE_LIBXINERAMA
 */
#ifdef LIBVNCSERVER_HAVE_LIBXINERAMA
#include <X11/extensions/Xinerama.h>
#endif

/*        date +'"lastmod:    %Y-%m-%d";' */
char lastmod[] = "lastmod:    2004-07-26";

/* X display info */
Display *dpy = 0;
Visual *visual;
Window window, rootwin;
int scr;
int bpp, depth;
int button_mask = 0;
int dpy_x, dpy_y;
int off_x, off_y;
int indexed_colour = 0;
int num_buttons = -1;

/* image structures */
XImage *scanline;
XImage *fullscreen;
XImage **tile_row;		/* for all possible row runs */

/* corresponding shm structures */
XShmSegmentInfo scanline_shm;
XShmSegmentInfo fullscreen_shm;
XShmSegmentInfo *tile_row_shm;	/* for all possible row runs */

/* rfb info */
rfbScreenInfoPtr screen;
rfbCursorPtr cursor;
char *main_fb;			/* our copy of the X11 fb */
char *rfb_fb;			/* same as main_fb unless transformation */
int main_bytes_per_line;
unsigned long  main_red_mask,  main_green_mask,  main_blue_mask;
unsigned short main_red_max,   main_green_max,   main_blue_max;
unsigned short main_red_shift, main_green_shift, main_blue_shift;
int rfb_bytes_per_line;

/* scaling info */
int scaling = 0;
int scaling_noblend = 0;
int scaling_nomult4 = 0;
int scaling_pad = 0;
int scaling_interpolate = 0;
double scale_fac = 1.0;
int scaled_x = 0, scaled_y = 0;
int scale_numer = 0, scale_denom = 0;

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
	blackout_t bo[16];	/* hardwired max rectangles. */
	int cover;
	int count;
} tile_blackout_t;

blackout_t blackr[100];		/* hardwired max blackouts */
int blackouts = 0;
tile_blackout_t *tile_blackout;

/* saved cursor */
int cur_save_x, cur_save_y, cur_save_w, cur_save_h, cur_saved = 0;

/* times of recent events */
time_t last_event, last_input, last_client = 0;

/* last client to move pointer */
rfbClientPtr last_pointer_client = NULL;

int cursor_x, cursor_y;		/* x and y from the viewer(s) */
int got_user_input = 0;
int got_pointer_input = 0;
int got_keyboard_input = 0;
int last_keyboard_input = 0;
int fb_copy_in_progress = 0;	
int shut_down = 0;	

/* string for the VNC_CONNECT property */
#define VNC_CONNECT_MAX 512
char vnc_connect_str[VNC_CONNECT_MAX+1];
Atom vnc_connect_prop = None;

/* function prototypes (see filename comment above) */

int all_clients_initialized(void);
void autorepeat(int restore);
char *bitprint(unsigned int);
void blackout_tiles(void);
void check_connect_inputs(void);
void clean_up_exit(int);
void clear_modifiers(int init);
void clear_keys(void);
void copy_screen(void);

double dtime(double *);

void initialize_blackout(char *);
void initialize_modtweak(void);
void initialize_pointer_map(char *);
void initialize_remap(char *);
void initialize_screen(int *argc, char **argv, XImage *fb);
void initialize_shm(void);
void initialize_signals(void);
void initialize_tiles(void);
void initialize_watch_bell(void);
void initialize_xinerama(void);

void keyboard(rfbBool down, rfbKeySym keysym, rfbClientPtr client);

void myXTestFakeKeyEvent(Display*, KeyCode, Bool, time_t);

typedef struct hint {
	/* location x, y, height, and width of a change-rectangle  */
	/* (grows as adjacent horizontal tiles are glued together) */
	int x, y, w, h;
} hint_t;
void mark_hint(hint_t);
void mark_rect_as_modified(int x1, int y1, int x2, int y2, int force);

enum rfbNewClientAction new_client(rfbClientPtr client);
void nofb_hook(rfbClientPtr client);
void pointer(int mask, int x, int y, rfbClientPtr client);

void read_vnc_connect_prop(void);
void redraw_mouse(void);
void restore_mouse_patch(void);
void rfbPE(rfbScreenInfoPtr, long);
void scan_for_updates(void);
void set_colormap(void);
void set_offset(void);
void set_visual(char *vstring);

void shm_clean(XShmSegmentInfo *, XImage *);
void shm_delete(XShmSegmentInfo *);

void update_mouse(void);
void watch_bell_event(void);
void watch_xevents(void);

void xcut_receive(char *text, int len, rfbClientPtr client);

void zero_fb(int, int, int, int);

/* -- options.h -- */
/* 
 * variables for the command line options
 */

int shared = 0;			/* share vnc display. */
char *allow_list = NULL;	/* for -allow and -localhost */
char *accept_cmd = NULL;	/* for -accept */
char *gone_cmd = NULL;		/* for -gone */
int view_only = 0;		/* clients can only watch. */
char *viewonly_passwd = NULL;	/* view only passwd. */
int inetd = 0;			/* spawned from inetd(1) */
int connect_once = 1;		/* disconnect after first connection session. */
int flash_cmap = 0;		/* follow installed colormaps */
int force_indexed_color = 0;	/* whether to force indexed color for 8bpp */

int use_modifier_tweak = 1;	/* use the shift/altgr modifier tweak */
int use_iso_level3 = 0;		/* ISO_Level3_Shift instead of Mode_switch */
int clear_mods = 0;		/* -clear_mods (1) and -clear_keys (2) */
int nofb = 0;			/* do not send any fb updates */

int subwin = 0;			/* -id */

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
int no_autorepeat = 0;		/* turn off autorepeat with clients */
int watch_bell = 1;		/* watch for the bell using XKEYBOARD */
int xkbcompat = 0;		/* ignore XKEYBOARD extension */
int use_xkb = 0;		/* try to open Xkb connection (for bell or other) */
int use_xkb_modtweak = 0;	/* -xkb */
char *skip_keycodes = NULL;

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

int screen_blank = 60;	/* number of seconds of no activity to throttle */
			/* down the screen polls.  zero to disable. */
int take_naps = 0;
int naptile = 3;	/* tile change threshold per poll to take a nap */
int napfac = 4;		/* time = napfac*waitms, cut load with extra waits */
int napmax = 1500;	/* longest nap in ms. */
int ui_skip = 10;	/* see watchloop.  negative means ignore input */

int watch_selection = 1;	/* normal selection/cutbuffer maintenance */
int watch_primary = 1;		/* more dicey, poll for changes in PRIMARY */

int sigpipe = 1;		/* 0=skip, 1=ignore, 2=exit */

/* for -visual override */
VisualID visual_id = (VisualID) 0;
int visual_depth = 0;

/* tile heuristics: */
double fs_frac = 0.75;	/* threshold tile fraction to do fullscreen updates. */
int tile_fuzz = 2;	/* tolerance for suspecting changed tiles touching */
			/* a known changed tile. */
int grow_fill = 3;	/* do the grow islands heuristic with this width. */
int gaps_fill = 4;	/* do a final pass to try to fill gaps between tiles. */

int debug_pointer = 0;
int debug_keyboard = 0;

int quiet = 0;

int got_rfbport = 0;
int got_alwaysshared = 0;
int got_nevershared = 0;

/* threaded vs. non-threaded (default) */
#if defined(LIBVNCSERVER_X11VNC_THREADED) && ! defined(X11VNC_THREADED)
#define X11VNC_THREADED
#endif

#if defined(LIBVNCSERVER_HAVE_LIBPTHREAD) && defined(X11VNC_THREADED)
	int use_threads = 1;
#else
	int use_threads = 0;
#endif


/* -- util.h -- */


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
 * following is based on IsModifierKey in Xutil.h
*/
#define ismodkey(keysym) \
  ((((KeySym)(keysym) >= XK_Shift_L) && ((KeySym)(keysym) <= XK_Hyper_R) && \
  ((KeySym)(keysym) != XK_Caps_Lock) && ((KeySym)(keysym) != XK_Shift_Lock)))

/*
 * Not sure why... but when threaded we have to mutex our X11 calls to
 * avoid XIO crashes.
 */
MUTEX(x11Mutex);
#define X_LOCK       LOCK(x11Mutex)
#define X_UNLOCK   UNLOCK(x11Mutex)
#define X_INIT INIT_MUTEX(x11Mutex)

/* -- util.c -- ? */

/*
 * routine to keep 0 <= i < n, should use in more places...
 */
int nfix(int i, int n) {
	if (i < 0) {
		i = 0;
	} else if (i >= n) {
		i = n - 1;
	}
	return i;
}


/* -- cleanup.c -- */
/*
 * Exiting and error handling routines
 */

static int exit_flag = 0;
int exit_sig = 0;

/*
 * Normal exiting
 */
void clean_up_exit (int ret) {
	int i;
	exit_flag = 1;

	/* remove the shm areas: */
	shm_clean(&scanline_shm, scanline);
	shm_clean(&fullscreen_shm, fullscreen);

	for(i=1; i<=ntiles_x; i++) {
		shm_clean(&tile_row_shm[i], tile_row[i]);
		if (single_copytile && i >= single_copytile) {
			break;
		}
	}

	if (clear_mods == 1) {
		clear_modifiers(0);
	} else if (clear_mods == 2) {
		clear_keys();
	}

	if (no_autorepeat) {
		autorepeat(1);
	}
	X_LOCK;
	XTestDiscard(dpy);
	XCloseDisplay(dpy);
	X_UNLOCK;

	fflush(stderr);
	exit(ret);
}

/*
 * General problem handler
 */
static void interrupted (int sig) {
	int i;
	exit_sig = sig;
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
	if (sig == SIGINT) {
		shut_down = 1;
		return;
	}
	/*
	 * to avoid deadlock, etc, just delete the shm areas and
	 * leave the X stuff hanging.
	 */
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
	if (clear_mods == 1) {
		clear_modifiers(0);
	} else if (clear_mods == 2) {
		clear_keys();
	}
	if (no_autorepeat) {
		autorepeat(1);
	}
	if (sig) {
		exit(2);
	}
}

/* X11 error handlers */

static XErrorHandler   Xerror_def;
static XIOErrorHandler XIOerr_def;
int trapped_xerror = 0;

int trap_xerror(Display *d, XErrorEvent *error) {
	trapped_xerror = 1;
	return 0;
}

static int Xerror(Display *d, XErrorEvent *error) {
	X_UNLOCK;
	interrupted(0);
	return (*Xerror_def)(d, error);
}

static int XIOerr(Display *d) {
	X_UNLOCK;
	interrupted(0);
	return (*XIOerr_def)(d);
}

/* signal handlers */
void initialize_signals(void) {
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
		signal(SIGPIPE, SIG_IGN);
#endif
	} else if (sigpipe == 2) {
		rfbLog("initialize_signals: will exit on SIGPIPE\n");
		signal(SIGPIPE, interrupted);
	}

	X_LOCK;
	Xerror_def = XSetErrorHandler(Xerror);
	XIOerr_def = XSetIOErrorHandler(XIOerr);
	X_UNLOCK;
}

/* -- connections.c -- */
/*
 * routines for handling incoming, outgoing, etc connections
 */

static int accepted_client = 0;
static int client_count = 0;

/*
 * check that all clients are in RFB_NORMAL state
 */
int all_clients_initialized(void) {
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
 * utility to run a user supplied command setting some RFB_ env vars.
 * used by, e.g., accept_client() and client_gone()
 */
static int run_user_command(char *cmd, rfbClientPtr client) {
	char *dpystr = DisplayString(dpy);
	static char *display_env = NULL;
	static char env_rfb_client_id[100];
	static char env_rfb_client_ip[100];
	static char env_rfb_client_port[100];
	static char env_rfb_server_ip[100];
	static char env_rfb_server_port[100];
	static char env_rfb_x11vnc_pid[100];
	static char env_rfb_client_count[100];
	char *addr = client->host;
	int rc;
	char *saddr_ip_str = NULL;
	int saddr_len, saddr_port;
	struct sockaddr_in saddr;

	if (addr == NULL || addr[0] == '\0') {
		addr = "unknown-host";
	}

	/* set RFB_CLIENT_ID to semi unique id for command to use */
	sprintf(env_rfb_client_id, "RFB_CLIENT_ID=%p", (void *) client);
	putenv(env_rfb_client_id);

	/* set RFB_CLIENT_IP to IP addr for command to use */
	sprintf(env_rfb_client_ip, "RFB_CLIENT_IP=%s", addr);
	putenv(env_rfb_client_ip);

	/* set RFB_X11VNC_PID to our pid for command to use */
	sprintf(env_rfb_x11vnc_pid, "RFB_X11VNC_PID=%d", (int) getpid());
	putenv(env_rfb_x11vnc_pid);

	/* set RFB_CLIENT_PORT to peer port for command to use */
	saddr_len = sizeof(saddr);
	memset(&saddr, 0, sizeof(saddr));
	saddr_port = -1;
	if (!getpeername(client->sock, (struct sockaddr *)&saddr, &saddr_len)) {
		saddr_port = ntohs(saddr.sin_port);
	}
	sprintf(env_rfb_client_port, "RFB_CLIENT_PORT=%d", saddr_port);
	putenv(env_rfb_client_port);

	/* 
	 * now do RFB_SERVER_IP and RFB_SERVER_PORT (i.e. us!)
	 * This will establish a 5-tuple (including tcp) the external
	 * program can potentially use to work out the virtual circuit
	 * for this connection.
	 */
	saddr_len = sizeof(saddr);
	memset(&saddr, 0, sizeof(saddr));
	saddr_port = -1;
	saddr_ip_str = "unknown";
	if (!getsockname(client->sock, (struct sockaddr *)&saddr, &saddr_len)) {
		saddr_port = ntohs(saddr.sin_port);
#ifdef LIBVNCSERVER_HAVE_NETINET_IN_H
		saddr_ip_str = inet_ntoa(saddr.sin_addr);
#endif
	}
	sprintf(env_rfb_server_ip, "RFB_SERVER_IP=%s", saddr_ip_str);
	putenv(env_rfb_server_ip);
	sprintf(env_rfb_server_port, "RFB_SERVER_PORT=%d", saddr_port);
	putenv(env_rfb_server_port);


	/* 
	 * Better set DISPLAY to the one we are polling, if they
	 * want something trickier, they can handle on their own
	 * via environment, etc.  XXX really should save/restore old.
	 */
	if (display_env == NULL) {
		display_env = (char *) malloc(strlen(dpystr)+10);
	}
	sprintf(display_env, "DISPLAY=%s", dpystr);
	putenv(display_env);

	/*
	 * work out the number of clients (have to use client_count
	 * since there is deadlock in rfbGetClientIterator) 
	 */
	sprintf(env_rfb_client_count, "RFB_CLIENT_COUNT=%d", client_count);
	putenv(env_rfb_client_count);

	rfbLog("running command:\n");
	rfbLog("  %s\n", cmd);

	rc = system(cmd);

	if (rc >= 256) {
		rc = rc/256;
	}
	rfbLog("command returned: %d\n", rc);

	return rc;
}

/*
 * Kludge for -norepeat option: we turn off keystroke autorepeat in
 * the X server when clients are connected.  This may annoy people at
 * the physical display.  We do this because 'key down' and 'key up'
 * user input events may be separated by 100s of ms due to screen fb
 * processing or link latency, thereby inducing the X server to apply
 * autorepeat when it should not.  Since the *client* is likely doing
 * keystroke autorepeating as well, it kind of makes sense to shut it
 * off if no one is at the physical display...
 */
void autorepeat(int restore) {
	XKeyboardState kstate;
	XKeyboardControl kctrl;
	static int save_auto_repeat = -1;

	if (restore) {
		if (save_auto_repeat < 0) {
			return;		/* nothing to restore */
		}
		X_LOCK;
		/* read state and skip restore if equal (e.g. no clients) */
		XGetKeyboardControl(dpy, &kstate);
		if (kstate.global_auto_repeat == save_auto_repeat) {
			X_UNLOCK;
			return;
		}

		kctrl.auto_repeat_mode = save_auto_repeat;
		XChangeKeyboardControl(dpy, KBAutoRepeatMode, &kctrl);
		XFlush(dpy);
		X_UNLOCK;

		rfbLog("Restored X server key autorepeat to: %d\n",
		    save_auto_repeat);
	} else {
		X_LOCK;
		XGetKeyboardControl(dpy, &kstate);
		save_auto_repeat = kstate.global_auto_repeat;

		kctrl.auto_repeat_mode = AutoRepeatModeOff;
		XChangeKeyboardControl(dpy, KBAutoRepeatMode, &kctrl);
		XFlush(dpy);
		X_UNLOCK;

		rfbLog("Disabled X server key autorepeat. (you can run the\n");
		rfbLog("command: 'xset r on' to force it back on)\n");
	}
}

/*
 * callback for when a client disconnects
 */
static void client_gone(rfbClientPtr client) {

	client_count--;
	rfbLog("client_count: %d\n", client_count);

	if (no_autorepeat && client_count == 0) {
		autorepeat(1);
	}
	if (gone_cmd) {
		rfbLog("client_gone: using cmd for: %s\n", client->host);
		run_user_command(gone_cmd, client);
	}

	if (inetd) {
		rfbLog("viewer exited.\n");
		clean_up_exit(0);
	}
	if (connect_once) {
		/*
		 * This non-exit is done for a bad passwd to be consistent
		 * with our RFB_CLIENT_REFUSE behavior in new_client()  (i.e.
		 * we disconnect after 1 successful connection).
		 */
		if ((client->state == RFB_PROTOCOL_VERSION ||
		     client->state == RFB_AUTHENTICATION) && accepted_client) {
			rfbLog("connect_once: bad password or early "
			   "disconnect.\n");
			rfbLog("connect_once: waiting for next connection.\n"); 
			accepted_client = 0;
			return;
		}

		rfbLog("viewer exited.\n");
		clean_up_exit(0);
	}
}

/*
 * Simple routine to limit access via string compare.  A power user will
 * want to compile libvncserver with libwrap support and use /etc/hosts.allow.
 */
static int check_access(char *addr) {
	int allowed = 0;
	char *p, *list;

	if (allow_list == NULL || *allow_list == '\0') {
		return 1;
	}
	if (addr == NULL || *addr == '\0') {
		rfbLog("check_access: denying empty host IP address string.\n");
		return 0;
	}

	if (strchr(allow_list, '/')) {
		/* a file of IP addresess or prefixes */
		int len;
		struct stat sbuf;
		FILE *in;
		char line[1024], *q;

		if (stat(allow_list, &sbuf) != 0) {
			rfbLog("check_access: failure stating file: %s\n",
			    allow_list);
			rfbLogPerror("stat");
			clean_up_exit(1);
		}
		len = sbuf.st_size + 1;	/* 1 more for '\0' at end */
		list = malloc(len);
		list[0] = '\0';
		
		in = fopen(allow_list, "r");
		if (in == NULL) {
			rfbLog("check_access: cannot open: %s\n", allow_list);
			rfbLogPerror("fopen");
			clean_up_exit(1);
		}
		while (fgets(line, 1024, in) != NULL) {
			if ( (q = strchr(line, '#')) != NULL) {
				*q = '\0';
			}
			if (strlen(list) + strlen(line) >= len) {
				break;
			}
			strcat(list, line);
		}
		fclose(in);
	} else {
		list = strdup(allow_list);
	}

	
	p = strtok(list, ", \t\n\r");
	while (p) {
		char *q;
		if (*p == '\0') {
			continue;	
		}
		q = strstr(addr, p);
		if (q == addr) {
			rfbLog("check_access: client %s matches pattern %s\n",
			    addr, p);
			allowed = 1;

		} else if(!strcmp(p,"localhost") && !strcmp(addr,"127.0.0.1")) {
			allowed = 1;
		}
		p = strtok(NULL, ", \t\n\r");
	}
	free(list);
	return allowed;
}

/*
 * x11vnc's first (and only) visible widget: accept/reject dialog window.
 * We go through this pain to avoid dependency on libXt.
 */
static int ugly_accept_window(char *addr, int X, int Y, int timeout,
    char *mode) {

#define t2x2_width 16
#define t2x2_height 16
static char t2x2_bits[] = {
   0xff, 0xff, 0xff, 0xff, 0x33, 0x33, 0x33, 0x33, 0xff, 0xff, 0xff, 0xff,
   0x33, 0x33, 0x33, 0x33, 0xff, 0xff, 0xff, 0xff, 0x33, 0x33, 0x33, 0x33,
   0xff, 0xff, 0xff, 0xff, 0x33, 0x33, 0x33, 0x33};

	Window awin;
	GC gc;
	XSizeHints hints;
	XGCValues values;
	static XFontStruct *font_info = NULL;
	static Pixmap ico = 0;
	unsigned long valuemask = 0;
	static char dash_list[] = {20, 40};
	int list_length = sizeof(dash_list);

	Atom wm_protocols;
	Atom wm_delete_window;

	XEvent ev;
	long evmask = ExposureMask | KeyPressMask | ButtonPressMask
	    | StructureNotifyMask;
	double waited = 0.0;

	/* strings and geometries y/n */
	KeyCode key_y, key_n, key_v;
	char strh[100];
	char str1_b[] = "To accept: press \"y\" or click the \"Yes\" button";
	char str2_b[] = "To reject: press \"n\" or click the \"No\" button";
	char str3_b[] = "View only: press \"v\" or click the \"View\" button";
	char str1_m[] = "To accept: click the \"Yes\" button";
	char str2_m[] = "To reject: click the \"No\" button";
	char str3_m[] = "View only: click the \"View\" button";
	char str1_k[] = "To accept: press \"y\"";
	char str2_k[] = "To reject: press \"n\"";
	char str3_k[] = "View only: press \"v\"";
	char *str1, *str2, *str3;
	char str_y[] = "Yes";
	char str_n[] = "No";
	char str_v[] = "View";
	int x, y, w = 345, h = 150, ret = 0;
	int X_sh = 20, Y_sh = 30, dY = 20;
	int Ye_x = 20,  Ye_y = 0, Ye_w = 45, Ye_h = 20;
	int No_x = 75,  No_y = 0, No_w = 45, No_h = 20; 
	int Vi_x = 130, Vi_y = 0, Vi_w = 45, Vi_h = 20; 

	if (!strcmp(mode, "mouse_only")) {
		str1 = str1_m;
		str2 = str2_m;
		str3 = str3_m;
	} else if (!strcmp(mode, "key_only")) {
		str1 = str1_k;
		str2 = str2_k;
		str3 = str3_k;
		h -= dY;
	} else {
		str1 = str1_b;
		str2 = str2_b;
		str3 = str3_b;
	}
	if (view_only) {
		h -= dY;
	}

	if (X < -dpy_x) {
		x = (dpy_x - w)/2;	/* large negative: center */
		if (x < 0) x = 0;
	} else if (X < 0) {
		x = dpy_x + X - w;	/* from lower right */
	} else {
		x = X;			/* from upper left */
	}
	
	if (Y < -dpy_y) {
		y = (dpy_y - h)/2;
		if (y < 0) y = 0;
	} else if (Y < 0) {
		y = dpy_y + Y - h;
	} else {
		y = Y;
	}

	X_LOCK;

	awin = XCreateSimpleWindow(dpy, window, x, y, w, h, 4,
	    BlackPixel(dpy, scr), WhitePixel(dpy, scr));

	wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wm_delete_window = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(dpy, awin, &wm_delete_window, 1);

	if (! ico) {
		ico = XCreateBitmapFromData(dpy, awin, t2x2_bits, t2x2_width,
		    t2x2_height);
	}

	hints.flags = PPosition | PSize | PMinSize;
	hints.x = x;
	hints.y = y;
	hints.width = w;
	hints.height = h;
	hints.min_width = w;
	hints.min_height = h;

	XSetStandardProperties(dpy, awin, "new x11vnc client", "x11vnc query",
	    ico, NULL, 0, &hints);

	XSelectInput(dpy, awin, evmask);

	if (! font_info && (font_info = XLoadQueryFont(dpy, "fixed")) == NULL) {
		rfbLog("ugly_accept_window: cannot locate font fixed.\n");
		X_UNLOCK;
		clean_up_exit(1);
	}

	gc = XCreateGC(dpy, awin, valuemask, &values);
	XSetFont(dpy, gc, font_info->fid);
	XSetForeground(dpy, gc, BlackPixel(dpy, scr));
	XSetLineAttributes(dpy, gc, 1, LineSolid, CapButt, JoinMiter);
	XSetDashes(dpy, gc, 0, dash_list, list_length);

	XMapWindow(dpy, awin);
	XFlush(dpy);

	sprintf(strh, "x11vnc: accept connection from %s?", addr);
	key_y = XKeysymToKeycode(dpy, XStringToKeysym("y"));
	key_n = XKeysymToKeycode(dpy, XStringToKeysym("n"));
	key_v = XKeysymToKeycode(dpy, XStringToKeysym("v"));

	while (1) {
		int out = -1, x, y, tw, k;

		if (XCheckWindowEvent(dpy, awin, evmask, &ev)) {
			;	/* proceed to handling */
		} else if (XCheckTypedEvent(dpy, ClientMessage, &ev)) {
			;	/* proceed to handling */
		} else {
			int ms = 100;	/* sleep a bit */
			usleep(ms * 1000);
			waited += ((double) ms)/1000.;
			if (timeout && (int) waited >= timeout) {
				rfbLog("accept_client: popup timed out after "
				    "%d seconds.\n", timeout);
				out = 0;
				ev.type = 0;
			} else {
				continue;
			}
		}

		switch(ev.type) {
		case Expose:
			while (XCheckTypedEvent(dpy, Expose, &ev)) {
				;
			}
			k=0;

			/* instructions */
			XDrawString(dpy, awin, gc, X_sh, Y_sh+(k++)*dY,
			    strh, strlen(strh));
			XDrawString(dpy, awin, gc, X_sh, Y_sh+(k++)*dY,
			    str1, strlen(str1));
			XDrawString(dpy, awin, gc, X_sh, Y_sh+(k++)*dY,
			    str2, strlen(str2));
			if (! view_only) {
				XDrawString(dpy, awin, gc, X_sh, Y_sh+(k++)*dY,
				    str3, strlen(str3));
			}

			if (!strcmp(mode, "key_only")) {
				break;
			}

			/* buttons */
			Ye_y = Y_sh+k*dY;
			No_y = Y_sh+k*dY;
			Vi_y = Y_sh+k*dY;
			XDrawRectangle(dpy, awin, gc, Ye_x, Ye_y, Ye_w, Ye_h);
			XDrawRectangle(dpy, awin, gc, No_x, No_y, No_w, No_h);
			if (! view_only) {
				XDrawRectangle(dpy, awin, gc, Vi_x, Vi_y,
				    Vi_w, Vi_h);
			}

			tw = XTextWidth(font_info, str_y, strlen(str_y));
			tw = (Ye_w - tw)/2;
			if (tw < 0) tw = 1;
			XDrawString(dpy, awin, gc, Ye_x+tw, Ye_y+Ye_h-5,
			    str_y, strlen(str_y));

			tw = XTextWidth(font_info, str_n, strlen(str_n));
			tw = (No_w - tw)/2;
			if (tw < 0) tw = 1;
			XDrawString(dpy, awin, gc, No_x+tw, No_y+No_h-5,
			    str_n, strlen(str_n));

			if (! view_only) {
				tw = XTextWidth(font_info, str_v,
				    strlen(str_v));
				tw = (Vi_w - tw)/2;
				if (tw < 0) tw = 1;
				XDrawString(dpy, awin, gc, Vi_x+tw, Vi_y+Vi_h-5,
				    str_v, strlen(str_v));
			}

			break;

		case ClientMessage:
			if (ev.xclient.message_type == wm_protocols &&
			    ev.xclient.data.l[0] == wm_delete_window) {
				out = 0;
			}
			break;

		case ButtonPress:
			x = ev.xbutton.x;
			y = ev.xbutton.y;
			if (!strcmp(mode, "key_only")) {
				;
			} else if (x > No_x && x < No_x+No_w && y > No_y
			    && y < No_y+No_h) {
				out = 0;
			} else if (x > Ye_x && x < Ye_x+Ye_w && y > Ye_y
			    && y < Ye_y+Ye_h) {
				out = 1;
			} else if (! view_only && x > Vi_x && x < Vi_x+Vi_w
			    && y > Vi_y && y < Vi_y+Ye_h) {
				out = 2;
			}
			break;

		case KeyPress:
			if (!strcmp(mode, "mouse_only")) {
				;
			} else if (ev.xkey.keycode == key_y) {
				out = 1;
			} else if (ev.xkey.keycode == key_n) {
				out = 0;
			} else if (! view_only && ev.xkey.keycode == key_v) {
				out = 2;
			}
			break;
		default:
			break;
		}
		if (out != -1) {
			ret = out;
			XUnmapWindow(dpy, awin);
			XFreeGC(dpy, gc);
			XDestroyWindow(dpy, awin);
			XFlush(dpy);
			break;
		}
	}
	X_UNLOCK;

	return ret;
}

/*
 * process a "yes:0,no:*,view:3" type action list comparing to command
 * return code rc.  * means the default action with no other match.
 */
static int action_match(char *action, int rc) {
	char *p, *q, *s = strdup(action);
	int cases[4], i, result;
	char *labels[4];

	labels[1] = "yes";
	labels[2] = "no";
	labels[3] = "view";

	rfbLog("accept_client: process action line: %s\n",
	    action);

	for (i=1; i <= 3; i++) {
		cases[i] = -2;
	}

	p = strtok(s, ",");
	while (p) {
		if ((q = strchr(p, ':')) != NULL) {
			int in, k;
			*q = '\0';
			q++;
			if (strstr(p, "yes") == p) {
				k = 1;
			} else if (strstr(p, "no") == p) {
				k = 2;
			} else if (strstr(p, "view") == p) {
				k = 3;
			} else {
				rfbLog("bad action line: %s\n", action);
				clean_up_exit(1);
			}
			if (*q == '*') {
				cases[k] = -1;
			} else if (sscanf(q, "%d", &in) == 1) {
				if (in < 0) {
					rfbLog("bad action line: %s\n", action);
					clean_up_exit(1);
				}
				cases[k] = in;
			} else {
				rfbLog("bad action line: %s\n", action);
				clean_up_exit(1);
			}
		} else {
			rfbLog("bad action line: %s\n", action);
			clean_up_exit(1);
		}
		p = strtok(NULL, ",");
	}
	free(s);

	result = -1;
	for (i=1; i <= 3; i++) {
		if (cases[i] == -1) {
			rfbLog("accept_client: default action is case=%d %s\n",
			    i, labels[i]);
			result = i;
			break;
		}
	}
	if (result == -1) {
		rfbLog("accept_client: no default action\n");
	}
	for (i=1; i <= 3; i++) {
		if (cases[i] >= 0 && cases[i] == rc) {
			rfbLog("accept_client: matched action is case=%d %s\n",
			    i, labels[i]);
			result = i;
			break;
		}
	}
	if (result < 0) {
		rfbLog("no action match: %s rc=%d set to no\n", action, rc);
		result = 2;
	}
	return result;
}

/*
 * Simple routine to prompt the user on the X display whether an incoming
 * client should be allowed to connect or not.  If a gui is involved it
 * will be running in the environment/context of the X11 DISPLAY.
 *
 * The command supplied via -accept is run as is (i.e. no string
 * substitution) with the RFB_CLIENT_IP environment variable set to the
 * incoming client's numerical IP address.
 * 
 * If the external command exits with 0 the client is accepted, otherwise
 * the client is rejected.
 * 
 * Some builtins are provided:
 *
 *	xmessage:  use homebrew xmessage(1) for the external command.  
 *	popup:     use internal X widgets for prompting.
 * 
 */
static int accept_client(rfbClientPtr client) {

	char xmessage[200], *cmd = NULL;
	char *addr = client->host;
	char *action = NULL;

	if (accept_cmd == NULL) {
		return 1;	/* no command specified, so we accept */
	}

	if (addr == NULL || addr[0] == '\0') {
		addr = "unknown-host";
	}

	if (strstr(accept_cmd, "popup") == accept_cmd) {
		/* use our builtin popup button */

		/* (popup|popupkey|popupmouse)[+-X+-Y][:timeout] */

		int ret, timeout = 120;
		int x = -64000, y = -64000;
		char *p, *mode;

		/* extract timeout */
		if ((p = strchr(accept_cmd, ':')) != NULL) {
			int in;
			if (sscanf(p+1, "%d", &in) == 1) {
				timeout = in;
			}
		}
		/* extract geometry */
		if ((p = strpbrk(accept_cmd, "+-")) != NULL) {
			int x1, y1;
			if (sscanf(p, "+%d+%d", &x1, &y1) == 2) {
				x = x1;
				y = y1;
			} else if (sscanf(p, "+%d-%d", &x1, &y1) == 2) {
				x = x1;
				y = -y1;
			} else if (sscanf(p, "-%d+%d", &x1, &y1) == 2) {
				x = -x1;
				y = y1;
			} else if (sscanf(p, "-%d-%d", &x1, &y1) == 2) {
				x = -x1;
				y = -y1;
			}
		}

		/* find mode: mouse, key, or both */
		if (strstr(accept_cmd, "popupmouse") == accept_cmd) {
			mode = "mouse_only";
		} else if (strstr(accept_cmd, "popupkey") == accept_cmd) {
			mode = "key_only";
		} else {
			mode = "both";
		}

		rfbLog("accept_client: using builtin popup for: %s\n", addr);
		if ((ret = ugly_accept_window(addr, x, y, timeout, mode))) {
			if (ret == 2) {
				rfbLog("accept_client: viewonly: %s\n", addr);
				client->viewOnly = TRUE;
			}
			rfbLog("accept_client: popup accepted: %s\n", addr);
			return 1;
		} else {
			rfbLog("accept_client: popup rejected: %s\n", addr);
			return 0;
		}

	} else if (!strcmp(accept_cmd, "xmessage")) {
		/* make our own command using xmessage(1) */

		if (view_only) {
			sprintf(xmessage, "xmessage -buttons yes:0,no:2 -center"
			    " 'x11vnc: accept connection from %s?'", addr);
		} else {
			sprintf(xmessage, "xmessage -buttons yes:0,no:2,"
			    "view-only:3 -center" " 'x11vnc: accept connection"
			    " from %s?'", addr);
			action = "yes:0,no:*,view:3";
		}
		cmd = xmessage;
		
	} else {
		/* use the user supplied command: */

		cmd = accept_cmd;

		/* extract any action prefix:  yes:N,no:M,view:K */
		if (strstr(accept_cmd, "yes:") == accept_cmd) {
			char *p;
			if ((p = strpbrk(accept_cmd, " \t")) != NULL) {
				int i;
				cmd = p;
				p = accept_cmd;
				for (i=0; i<200; i++) {
					if (*p == ' ' || *p == '\t') {
						xmessage[i] = '\0';
						break;
					}
					xmessage[i] = *p;
					p++;
				}
				xmessage[200-1] = '\0';
				action = xmessage;
			}
		}
	}

	if (cmd) {
		int rc;

		rfbLog("accept_client: using cmd for: %s\n", addr);
		rc = run_user_command(cmd, client);

		if (action) {
			int result;

			if (rc < 0) {
				rfbLog("accept_client: cannot use negative "
				    "rc: %d, action %s\n", rc, action);
				result = 2;
			} else {
				result = action_match(action, rc);
			}

			if (result == 1) {
				rc = 0;
			} else if (result == 2) {
				rc = 1;
			} else if (result == 3) {
				rc = 0;
				rfbLog("accept_client: viewonly: %s\n", addr);
				client->viewOnly = TRUE;
			} else {
				rc = 1;	/* NOTREACHED */
			}
		}

		if (rc == 0) {
			rfbLog("accept_client: accepted: %s\n", addr);
			return 1;
		} else {
			rfbLog("accept_client: rejected: %s\n", addr);
			return 0;
		}
	} else {
		rfbLog("accept_client: no command, rejecting %s\n", addr);
		return 0;
	}

	return 0;	/* NOTREACHED */
}

/*
 * For the -connect <file> option: periodically read the file looking for
 * a connect string.  If one is found set client_connect to it.
 */
static void check_connect_file(char *file) {
	FILE *in;
	char line[1024], host[1024];
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
			rfbLogPerror("fopen");
			first_warn = 0;
		}
		return;
	}

	if (fgets(line, 1024, in) != NULL) {
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

/*
 * Do a reverse connect for a single "host" or "host:port"
 */
static int do_reverse_connect(char *str) {
	rfbClientPtr cl;
	char *host, *p;
	int port = 5500, len = strlen(str);

	if (len < 1) {
		return 0;
	}
	if (len > 1024) {
		rfbLog("reverse_connect: string too long: %d bytes\n", len);
		return 0;
	}

	/* copy in to host */
	host = (char *) malloc((size_t) len+1);
	if (! host) {
		rfbLog("reverse_connect: could not malloc string %d\n", len);
		return 0;
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

/*
 * Break up comma separated list of hosts and call do_reverse_connect()
 */
static void reverse_connect(char *str) {
	char *p, *tmp = strdup(str);
	int sleep_between_host = 300;
	int sleep_min = 1500, sleep_max = 4500, n_max = 5;
	int n, tot, t, dt = 100, cnt = 0;

	p = strtok(tmp, ", \t\r\n");
	while (p) {
		if ((n = do_reverse_connect(p)) != 0) {
			rfbPE(screen, -1);
		}
		cnt += n;

		p = strtok(NULL, ", \t\r\n");
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

/*
 * Routines for monitoring the VNC_CONNECT property for changes.
 * The vncconnect(1) will set it on our X display.
 */
void read_vnc_connect_prop(void) {
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
 * check if client_connect has been set, if so make the reverse connections.
 */
static void send_client_connect(void) {
	if (client_connect != NULL) {
		reverse_connect(client_connect);
		free(client_connect);
		client_connect = NULL;
	}
}

/*
 * monitor the various input methods
 */
void check_connect_inputs(void) {

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
	last_event = last_input = time(0);

	if (inetd) {
		/* 
		 * Set this so we exit as soon as connection closes,
		 * otherwise client_gone is only called after RFB_CLIENT_ACCEPT
		 */
		client->clientGoneHook = client_gone;
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
	if (! check_access(client->host)) {
		rfbLog("denying client: %s does not match %s\n", client->host,
		    allow_list ? allow_list : "(null)" );
		return(RFB_CLIENT_REFUSE);
	}
	if (! accept_client(client)) {
		rfbLog("denying client: %s local user rejected connection.\n",
		    client->host);
		rfbLog("denying client: accept_cmd=\"%s\"\n",
		    accept_cmd ? accept_cmd : "(null)" );
		return(RFB_CLIENT_REFUSE);
	}

	if (view_only)  {
		client->clientData = (void *) -1;
		client->viewOnly = TRUE;
	} else {
		client->clientData = (void *) 0;
	}

	client->clientGoneHook = client_gone;
	client_count++;

	if (no_autorepeat && client_count == 1) {
		/* first client, turn off X server autorepeat */
		autorepeat(0);
	}

	accepted_client = 1;
	last_client = time(0);

	return(RFB_CLIENT_ACCEPT);
}

/* -- keyboard.c -- */
/*
 * Routine to retreive current state keyboard.  1 means down, 0 up.
 */
static void get_keystate(int *keystate) {
	int i, k;
	char keys[32];
	
	/* n.b. caller decides to X_LOCK or not. */
	XQueryKeymap(dpy, keys);
	for (i=0; i<32; i++) {
		char c = keys[i];

		for (k=0; k < 8; k++) {
			if (c & 0x1) {
				keystate[8*i + k] = 1;
			} else {
				keystate[8*i + k] = 0;
			}
			c = c >> 1;
		}
	}
}

/*
 * Try to KeyRelease any non-Lock modifiers that are down.
 */
void clear_modifiers(int init) {
	static KeyCode keycodes[256];
	static KeySym  keysyms[256];
	static char *keystrs[256];
	static int kcount = 0, first = 1;
	int keystate[256];
	int i, j, minkey, maxkey, syms_per_keycode;
	KeySym *keymap;
	KeySym keysym;
	KeyCode keycode;

	/* n.b. caller decides to X_LOCK or not. */
	if (first) {
		/*
		 * we store results in static arrays, to aid interrupted
		 * case, but modifiers could have changed during session...
		 */
		XDisplayKeycodes(dpy, &minkey, &maxkey);

		keymap = XGetKeyboardMapping(dpy, minkey, (maxkey - minkey + 1),
		    &syms_per_keycode);

		for (i = minkey; i <= maxkey; i++) {
		    for (j = 0; j < syms_per_keycode; j++) {
			keysym = keymap[ (i - minkey) * syms_per_keycode + j ];
			if (keysym == NoSymbol || ! ismodkey(keysym)) {
				continue;
			}
			keycode = XKeysymToKeycode(dpy, keysym);
			if (keycode == NoSymbol) {
				continue;
			}
			keycodes[kcount] = keycode;
			keysyms[kcount]  = keysym;
			keystrs[kcount]  = strdup(XKeysymToString(keysym));
			kcount++;
		    }
		}
		XFree((void *) keymap);
		first = 0;
	}
	if (init) {
		return;
	}
	
	get_keystate(keystate);
	for (i=0; i < kcount; i++) {
		keysym  = keysyms[i];
		keycode = keycodes[i];

		if (! keystate[(int) keycode]) {
			continue;
		}

		if (clear_mods) {
			rfbLog("clear_modifiers: up: %-10s (0x%x) "
			    "keycode=0x%x\n", keystrs[i], keysym, keycode);
		}
		myXTestFakeKeyEvent(dpy, keycode, False, CurrentTime);
	}
	XFlush(dpy);
}

/*
 * Attempt to set all keys to Up position.  Can mess up typing at the
 * physical keyboard so use with caution.
 */
void clear_keys(void) {
	int k, keystate[256];
	
	/* n.b. caller decides to X_LOCK or not. */
	get_keystate(keystate);
	for (k=0; k<256; k++) {
		if (keystate[k]) {
			KeyCode keycode = (KeyCode) k;
			rfbLog("clear_keys: keycode=%d\n", keycode);
			myXTestFakeKeyEvent(dpy, keycode, False, CurrentTime);
		}
	}
	XFlush(dpy);
}
		
/*
 * The following is for an experimental -remap option to allow the user
 * to remap keystrokes.  It is currently confusing wrt modifiers...
 */
typedef struct keyremap {
	KeySym before;
	KeySym after;
	int isbutton;
	struct keyremap *next;
} keyremap_t;

static keyremap_t *keyremaps = NULL;

/*
 * process the -remap string (file or mapping string)
 */
void initialize_remap(char *infile) {
	FILE *in;
	char *p, *q, line[256], str1[256], str2[256];
	int i;
	KeySym ksym1, ksym2;
	keyremap_t *remap, *current;

	in = fopen(infile, "r"); 
	if (in == NULL) {
		/* assume cmd line key1-key2,key3-key4 */
		if (! strchr(infile, '-') || (in = tmpfile()) == NULL) {
			rfbLog("remap: cannot open: %s\n", infile);
			rfbLogPerror("fopen");
			clean_up_exit(1);
		}
		p = infile;
		while (*p) {
			if (*p == '-') {
				fprintf(in, " ");
			} else if (*p == ',' || *p == ' ' ||  *p == '\t') {
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
		int blank = 1, isbtn = 0;
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
		if ( (q = strchr(line, '-')) != NULL)   {
			/* allow Keysym1-Keysym2 notation */
			*q = ' ';	
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
		if (ksym2 == NoSymbol) {
			int i;
			if (sscanf(str2, "Button%d", &i) == 1) {
				ksym2 = (KeySym) i;
				isbtn = 1; 
			}
		}
		if (ksym1 == NoSymbol || ksym2 == NoSymbol) {
			rfbLog("warning: skipping bad remap line: %s", line);
			continue;
		}
		remap = (keyremap_t *) malloc((size_t) sizeof(keyremap_t));
		remap->before = ksym1;
		remap->after  = ksym2;
		remap->isbutton = isbtn;
		remap->next   = NULL;
		rfbLog("remapping: (%s, 0x%x) -> (%s, 0x%x) isbtn=%d\n", str1,
		    ksym1, str2, ksym2, isbtn);
		if (keyremaps == NULL) {
			keyremaps = remap;
		} else {
			current->next = remap;
		}
		current = remap;
	}
	fclose(in);
}

/*
 * debugging wrapper for XTestFakeKeyEvent()
 */
void myXTestFakeKeyEvent(Display* dpy, KeyCode key, Bool down,
    time_t cur_time) {
	if (debug_keyboard) {
		rfbLog("XTestFakeKeyEvent(dpy, keycode=0x%x \"%s\", %s)\n",
		    key, XKeysymToString(XKeycodeToKeysym(dpy, key, 0)),
		    down ? "down":"up");
	}
	if (down) {
		last_keyboard_input = -key;
	} else {
		last_keyboard_input = key;
	}
	XTestFakeKeyEvent(dpy, key, down, cur_time);
}


/*
 * preliminary support for using the Xkb (XKEYBOARD) extension for handling
 * user input.  inelegant, slow, and incomplete currently... but initial
 * tests show it is useful for some setups.
 */
typedef struct keychar {
	KeyCode code;
	int group;
	int level;
} keychar_t;

/* max number of key groups and shift levels we consider */
#define GRP 4
#define LVL 8
static int lvl_max, grp_max, kc_min, kc_max;
static KeySym xkbkeysyms[0x100][GRP][LVL];
static unsigned int xkbstate[0x100][GRP][LVL];
static unsigned int xkbignore[0x100][GRP][LVL];
static unsigned int xkbmodifiers[0x100][GRP][LVL];
static int multi_key[0x100], mode_switch[0x100], skipkeycode[0x100];
static int shift_keys[0x100];

#ifndef LIBVNCSERVER_HAVE_XKEYBOARD

/* empty functions for no xkb */
static void initialize_xkb_modtweak(void) {}
static void xkb_tweak_keyboard(rfbBool down, rfbKeySym keysym,
    rfbClientPtr client) {
}

#else

/* sets up all the keymapping info via Xkb API */

static void initialize_xkb_modtweak(void) {
	KeySym ks;
	int kc, grp, lvl, k;
	unsigned int state;

/*
 * Here is a guide:

Workarounds arrays:

multi_key[]     indicates which keycodes have Multi_key (Compose)
                bound to them.
mode_switch[]   indicates which keycodes have Mode_switch (AltGr)
                bound to them.
shift_keys[]    indicates which keycodes have Shift bound to them.
skipkeycode[]   indicates which keycodes are to be skipped
                for any lookups from -skip_keycodes option. 

Groups and Levels, here is an example:
                                                                  
      ^          --------                                      
      |      L2 | A   AE |                                      
    shift       |        |                                      
    level    L1 | a   ae |                                      
                 --------                                      
                  G1  G2                                        
                                                                
                  group ->                                      

Traditionally this it all a key could do.  L1 vs. L2 selected via Shift
and G1 vs. G2 selected via Mode_switch.  Up to 4 Keysyms could be bound
to a key.  See initialize_modtweak() for an example of using that type
of keymap from XGetKeyboardMapping().

Xkb gives us up to 4 groups and 63 shift levels per key, with the
situation being potentially different for each key.  This is complicated,
and I don't claim to understand it all, but in the following we just think
of ranging over the group and level indices as covering all of the cases.
This gives us an accurate view of the keymap.  The main tricky part
is mapping between group+level and modifier state.

On current linux/XFree86 setups (Xkb is enabled by default) the
information from XGetKeyboardMapping() (evidently the compat map)
is incomplete and inaccurate, so we are really forced to use the
Xkb API.

xkbkeysyms[]      For a (keycode,group,level) holds the KeySym (0 for none)
xkbstate[]        For a (keycode,group,level) holds the corresponding
                  modifier state needed to get that KeySym
xkbignore[]       For a (keycode,group,level) which modifiers can be
                  ignored (the 0 bits can be ignored).
xkbmodifiers[]    For the KeySym bound to this (keycode,group,level) store
                  the modifier mask.   
 *
 */

	/* initialize all the arrays: */
	for (kc = 0; kc < 0x100; kc++) {
		multi_key[kc] = 0;
		mode_switch[kc] = 0;
		skipkeycode[kc] = 0;
		shift_keys[kc] = 0;

		for (grp = 0; grp < GRP; grp++) {
			for (lvl = 0; lvl < LVL; lvl++) {
				xkbkeysyms[kc][grp][lvl] = NoSymbol;
				xkbmodifiers[kc][grp][lvl] = -1;
				xkbstate[kc][grp][lvl] = -1;
			}
		}
	}

	/*
	 * the array is 256*LVL*GRP, but we can make the searched region
	 * smaller by computing the actual ranges.
	 */
	lvl_max = 0;
	grp_max = 0;
	kc_max = 0;
	kc_min = 0x100;

	/*
	 * loop over all possible (keycode, group, level) triples
	 * and record what we find for it:
	 */
	if (debug_keyboard > 1) {
		rfbLog("initialize_xkb_modtweak: XKB keycode -> keysyms "
		    "mapping info:\n");
	}
	for (kc = 0; kc < 0x100; kc++) {
	    for (grp = 0; grp < GRP; grp++) {
		for (lvl = 0; lvl < LVL; lvl++) {
			unsigned int ms, mods;
			int state_save = -1, mods_save;
			KeySym ks2;

			/* look up the Keysym, if any */
			ks = XkbKeycodeToKeysym(dpy, kc, grp, lvl);
			xkbkeysyms[kc][grp][lvl] = ks;

			/* if no Keysym, on to next */
			if (ks == NoSymbol) {
				continue;
			}
			/*
			 * for various workarounds, note where these special
			 * keys are bound to.
			 */
			if (ks == XK_Multi_key) {
				multi_key[kc] = lvl+1;
			}
			if (ks == XK_Mode_switch) {
				mode_switch[kc] = lvl+1;
			}
			if (ks == XK_Shift_L || ks == XK_Shift_R) {
				shift_keys[kc] = lvl+1;
			}

			/*
			 * record maximum extent for group/level indices
			 * and keycode range:
			 */
			if (grp > grp_max) {
				grp_max = grp;
			}
			if (lvl > lvl_max) {
				lvl_max = lvl;
			}
			if (kc > kc_max) {
				kc_max = kc;
			}
			if (kc < kc_min) {
				kc_min = kc;
			}

			/*
			 * lookup on *keysym* (i.e. not kc, grp, lvl)
			 * and get the modifier mask.  this is 0 for
			 * most keysyms, only non zero for modifiers.
			 */
			ms = XkbKeysymToModifiers(dpy, ks);
			xkbmodifiers[kc][grp][lvl] = ms;

			/*
			 * Amusing heuristic (may have bugs).  There are
			 * 8 modifier bits, so 256 possible modifier
			 * states.  We loop over all of them for this
			 * keycode (simulating Key "events") and ask
			 * XkbLookupKeySym to tell us the Keysym.  Once it
			 * matches the Keysym we have for this (keycode,
			 * group, level), gotten via XkbKeycodeToKeysym()
			 * above, we then (hopefully...) know that state
			 * of modifiers needed to generate this keysym.
			 *
			 * Yes... keep your fingers crossed.
			 *
			 * Note that many of the 256 states give the
			 * Keysym, we just need one, and we take the
			 * first one found.
			 */
			state = 0;
			while(state < 256) {
				if (XkbLookupKeySym(dpy, kc, state, &mods,
				    &ks2)) {

					/* save these for workaround below */
					if (state_save == -1) {
						state_save = state;
						mods_save = mods;
					}
					if (ks2 == ks) {
						/*
						 * zero the irrelevant bits
						 * by anding with mods.
						 */
						xkbstate[kc][grp][lvl]
						    = state & mods;
						/*
						 * also remember the irrelevant
						 * bits since it is handy.
						 */
						xkbignore[kc][grp][lvl] = mods;

						break;
					}
				}
				state++;
			}
			if (xkbstate[kc][grp][lvl] == -1 && grp == 1) {
				/*
				 * Hack on Solaris 9 for Mode_switch
				 * for Group2 characters.  We force the 
				 * Mode_switch modifier bit on.
				 * XXX Need to figure out better what is
				 * happening here.  Is compat on somehow??
				 */
				unsigned int ms2;
				ms2 = XkbKeysymToModifiers(dpy, XK_Mode_switch);

				xkbstate[kc][grp][lvl]
				    = (state_save & mods_save) | ms2;

				xkbignore[kc][grp][lvl] = mods_save | ms2;
			}

			if (debug_keyboard > 1) {
				fprintf(stderr, "  %03d  G%d L%d  mod=%s ",
				    kc, grp+1, lvl+1, bitprint(ms));
				fprintf(stderr, "state=%s ",
				    bitprint(xkbstate[kc][grp][lvl]));
				fprintf(stderr, "ignore=%s ",
				    bitprint(xkbignore[kc][grp][lvl]));
				fprintf(stderr, " ks=0x%08lx \"%s\"\n",
				    ks, XKeysymToString(ks));
			}
		}
	    }
	}

	/*
	 * process the user supplied -skip_keycodes string.
	 * This is presumably a list if "ghost" keycodes, the X server
	 * thinks they exist, but they do not.  ghosts can lead to
	 * ambiguities in the reverse map: Keysym -> KeyCode + Modstate,
	 * so if we can ignore them so much the better.  Presumably the
	 * user can never generate them from the physical keyboard.
	 * There may be other reasons to deaden some keys.
	 */
	if (skip_keycodes != NULL) {
		char *p, *str = strdup(skip_keycodes);
		p = strtok(str, ", \t\n\r");
		while (p) {
			k = 1;
			if (sscanf(p, "%d", &k) != 1 || k < 0 || k >= 0x100) {
				rfbLog("bad skip_keycodes: %s %s\n",
				    skip_keycodes, p);
				clean_up_exit(1);
			}
			skipkeycode[k] = 1;
			p = strtok(NULL, ", \t\n\r");
		}
		free(str);
	}
	if (debug_keyboard > 1) {
		fprintf(stderr, "grp_max=%d lvl_max=%d\n", grp_max, lvl_max);
	}
}

/*
 * Called on user keyboard input.  Try to solve the reverse mapping
 * problem: KeySym (from VNC client) => KeyCode(s) to press to generate
 * it.  The one-to-many KeySym => KeyCode mapping makes it difficult, as
 * does working out what changes to the modifier keypresses are needed.
 */
static void xkb_tweak_keyboard(rfbBool down, rfbKeySym keysym,
    rfbClientPtr client) {

	int kc, grp, lvl, i;
	int kc_f[0x100], grp_f[0x100], lvl_f[0x100], state_f[0x100], found;
	unsigned int state;

	/* these are used for finding modifiers, etc */
	XkbStateRec kbstate;
	int got_kbstate = 0;
	int Kc_f, Grp_f, Lvl_f;

	X_LOCK;

	if (debug_keyboard) {
		char *str = XKeysymToString(keysym);

		if (debug_keyboard > 1) fprintf(stderr, "\n");

		rfbLog("xkb_tweak_keyboard: %s keysym=0x%x \"%s\"\n",
		    down ? "down" : "up", (int) keysym, str ? str : "null");
	}

	/*
	 * set everything to not-yet-found.
	 * these "found" arrays (*_f) let us dyanamically consider the
	 * one-to-many Keysym -> Keycode issue.  we set the size at 256,
	 * but of course only very few will be found.
	 */
	for (i = 0; i < 0x100; i++) {
		kc_f[i]    = -1;
		grp_f[i]   = -1;
		lvl_f[i]   = -1;
		state_f[i] = -1;
	}
	found = 0;

	/*
	 * loop over all (keycode, group, level) triples looking for
	 * matching keysyms.  Amazingly this isn't slow (but maybe if
	 * you type really fast...).  Hash lookup into a linked list of
	 * (keycode,grp,lvl) triples would be the way to improve this
	 * in the future if needed.
	 */
	for (kc = kc_min; kc <= kc_max; kc++) {
	    for (grp = 0; grp < grp_max+1; grp++) {
		for (lvl = 0; lvl < lvl_max+1; lvl++) {
			if (keysym != xkbkeysyms[kc][grp][lvl]) {
				continue;
			}
			/* got a keysym match */
			state = xkbstate[kc][grp][lvl];

			if (debug_keyboard > 1) {
				fprintf(stderr, "  got match kc=%03d=0x%02x G%d"
				    " L%d  ks=0x%x \"%s\"  (basesym: \"%s\")\n",
				    kc, kc, grp+1, lvl+1, keysym,
				    XKeysymToString(keysym), XKeysymToString(
				    XKeycodeToKeysym(dpy, kc, 0)));
				fprintf(stderr, "    need state: %s\n",
				    bitprint(state));
				fprintf(stderr, "    ignorable : %s\n",
				    bitprint(xkbignore[kc][grp][lvl]));
			}

			/* save it if state is OK and not told to skip */
			if (state == -1) {
				continue;
			}
			if (skipkeycode[kc] && debug_keyboard) {
				fprintf(stderr, "    xxx skipping keycode: %d "
				   "G%d/L%d\n", kc, grp+1, lvl+1);
			}
			if (skipkeycode[kc]) {
				continue;
			}
			if (found > 0 && kc == kc_f[found-1]) {
				/* ignore repeats for same keycode */
				continue;
			}
			kc_f[found] = kc;
			grp_f[found] = grp;
			lvl_f[found] = lvl;
			state_f[found] = state;
			found++;
		}
	    }
	}

#define PKBSTATE  \
	fprintf(stderr, "    --- current mod state:\n"); \
	fprintf(stderr, "    mods      : %s\n", bitprint(kbstate.mods)); \
	fprintf(stderr, "    base_mods : %s\n", bitprint(kbstate.base_mods)); \
	fprintf(stderr, "    latch_mods: %s\n", bitprint(kbstate.latched_mods)); \
	fprintf(stderr, "    lock_mods : %s\n", bitprint(kbstate.locked_mods)); \
	fprintf(stderr, "    compat    : %s\n", bitprint(kbstate.compat_state));

	/*
	 * Now get the current state of the keyboard from the X server.
	 * This seems to be the safest way to go as opposed to our
	 * keeping track of the modifier state on our own.  Again,
	 * this is fortunately not too slow.
	 */

	if (debug_keyboard > 1) {
		/* get state early for debug output */
		XkbGetState(dpy, XkbUseCoreKbd, &kbstate);
		got_kbstate = 1;
		PKBSTATE
	}
	if (!found && debug_keyboard) {
		char *str = XKeysymToString(keysym);
		fprintf(stderr, "    *** NO key found for: 0x%x %s  "
		    "*keystroke ignored*\n", keysym, str ? str : "null");
	}
	if (!found) {
		X_UNLOCK;
		return;
	}

	/* 
	 * we could optimize here if found > 1
	 * e.g. minimize lvl or grp, or other things to give
	 * "safest" scenario to simulate the keystrokes.
	 * but for now we just take the first one we found.
	 */
	Kc_f = kc_f[0];
	Grp_f = grp_f[0];
	Lvl_f = lvl_f[0];
	state = state_f[0];

	if (debug_keyboard && found > 1) {
		int l;
		char *str;
		fprintf(stderr, "    *** found more than one keycode: ");
		for (l = 0; l < found; l++) {
			fprintf(stderr, "%03d ", kc_f[l]);
		}
		for (l = 0; l < found; l++) {
			str = XKeysymToString(XKeycodeToKeysym(dpy,kc_f[l],0));
			fprintf(stderr, " \"%s\"", str ? str : "null");
		}
		fprintf(stderr, ", using first one: %03d\n", Kc_f);
	}

	if (down) {
		/*
		 * need to set up the mods for tweaking and other workarounds
		 */
		int needmods[8], sentmods[8], Ilist[8], keystate[256];
		int involves_multi_key, shift_is_down;
		int i, j, b, curr, need;
		unsigned int ms;
		KeySym ks;
		Bool dn;

		if (! got_kbstate) {
			/* get the current modifier state if we haven't yet */
			XkbGetState(dpy, XkbUseCoreKbd, &kbstate);
		}

		/*
		 * needmods[] whether or not that modifier bit needs
		 *            something done to it. 
		 *            < 0 means no,
		 *            0   means needs to go up.
		 *            1   means needs to go down.
		 *
		 * -1, -2, -3 are used for debugging info to indicate
		 * why nothing needs to be done with the modifier, see below.
		 *
		 * sentmods[] is the corresponding keycode to use
		 * to acheive the needmods[] requirement for the bit.
		 */

		for (i=0; i<8; i++) {
			needmods[i] = -1;
			sentmods[i] = 0;
		}

		/*
		 * Loop over the 8 modifier bits and check if the current
		 * setting is what we need it to be or whether it should
		 * be changed (by us sending some keycode event)
		 *
		 * If nothing needs to be done to it record why:
		 *   -1  the modifier bit is ignored.
		 *   -2  the modifier bit is ignored, but is correct anyway.
		 *   -3  the modifier bit is correct.
		 */

		b = 0x1;
		for (i=0; i<8; i++) {
			curr = b & kbstate.mods;
			need = b & state;

			if (! (b & xkbignore[Kc_f][Grp_f][Lvl_f])) {
				/* irrelevant modifier bit */
				needmods[i] = -1;
				if (curr == need) needmods[i] = -2;
			} else if (curr == need) {
				/* already correct */
				needmods[i] = -3;
			} else if (! curr && need) {
				/* need it down */
				needmods[i] = 1;
			} else if (curr && ! need) {
				/* need it up */
				needmods[i] = 0;
			}

			b = b << 1;
		}

		/*
		 * Again we dynamically probe the X server for information,
		 * this time for the state of all the keycodes.  Useful
		 * info, and evidently is not too slow...
		 */
		get_keystate(keystate);

		/*
		 * We try to determine if Shift is down (since that can
		 * screw up ISO_Level3_Shift manipulations).
		 */
		shift_is_down = 0;

		for (kc = kc_min; kc <= kc_max; kc++) {
			if (skipkeycode[kc] && debug_keyboard) {
				fprintf(stderr, "    xxx skipping keycode: "
				    "%d\n", kc);
			}
			if (skipkeycode[kc]) {
				continue;
			}
			if (shift_keys[kc] && keystate[kc]) {
				shift_is_down = kc;
				break;
			}
		}

		/*
		 * Now loop over the modifier bits and try to deduce the
		 * keycode presses/release require to match the desired
		 * state.
		 */
		for (i=0; i<8; i++) {
			if (needmods[i] < 0 && debug_keyboard > 1) {
				int k = -needmods[i] - 1;
				char *words[] = {"ignorable",
				    "bitset+ignorable", "bitset"};
				fprintf(stderr, "    +++ needmods: mod=%d is "
				    "OK  (%s)\n", i, words[k]);
			}
			if (needmods[i] < 0) {
				continue;
			}

			b = 1 << i;

			if (debug_keyboard > 1) {
				fprintf(stderr, "    +++ needmods: mod=%d %s "
				    "need it to be: %d %s\n", i, bitprint(b),
				    needmods[i], needmods[i] ? "down" : "up");
			}

			/*
			 * Again, an inefficient loop, this time just
			 * looking for modifiers...
			 */
			for (kc = kc_min; kc <= kc_max; kc++) {
			    for (grp = 0; grp < grp_max+1; grp++) {
				for (lvl = 0; lvl < lvl_max+1; lvl++) {
					int skip = 1, dbmsg = 0;

					ms = xkbmodifiers[kc][grp][lvl];
					if (! ms || ms != b) {
						continue;
					}

					if (skipkeycode[kc] && debug_keyboard) {
					    fprintf(stderr, "    xxx skipping "
						"keycode: %d G%d/L%d\n",
						kc, grp+1, lvl+1);
					}
					if (skipkeycode[kc]) {
						continue;
					}

					ks = xkbkeysyms[kc][grp][lvl];
					if (! ks) {
						continue;
					}

					if (ks == XK_Shift_L) {
						skip = 0;
					} else if (ks == XK_Shift_R) {
						skip = 0;
					} else if (ks == XK_Mode_switch) {
						skip = 0;
					} else if (ks == XK_ISO_Level3_Shift) {
						skip = 0;
					}
					/*
					 * Alt, Meta, Control, Super,
					 * Hyper, Num, Caps are skipped.
					 *
					 * XXX need more work on Locks,
					 * and non-standard modifiers.
					 * (e.g. XF86_Next_VMode using
					 * Ctrl+Alt)
					 */
					if (debug_keyboard > 1) {
						char *str = XKeysymToString(ks);
						int kt = keystate[kc];
						fprintf(stderr, "    === for "
						    "mod=%s found kc=%03d/G%d"
						    "/L%d it is %d %s skip=%d "
						    "(%s)\n", bitprint(b), kc,
						    grp+1, lvl+1, kt, kt ?
						    "down" : "up  ", skip,
						    str ? str : "null");
					}

					if (! skip && needmods[i] !=
					    keystate[kc] && sentmods[i] == 0) {
						sentmods[i] = kc;
						dbmsg = 1;
					}

					if (debug_keyboard > 1 && dbmsg) {
						int nm = needmods[i];
						fprintf(stderr, "    >>> we "
						    "choose kc=%03d=0x%02x to "
						    "change it to: %d %s\n", kc,
						    kc, nm, nm ? "down" : "up");
					}
						
				}
			    }
			}
		}
		for (i=0; i<8; i++) {
			/*
			 * reverse order is useful for tweaking
			 * ISO_Level3_Shift before Shift, but assumes they
			 * are in that order (i.e. Shift is first bit).
			 */
			int reverse = 1;
			if (reverse) {
				Ilist[i] = 7 - i;
			} else {
				Ilist[i] = i;
			}
		}

		/*
		 * check to see if Multi_key is bound to one of the Mods
		 * we have to tweak
		 */
		involves_multi_key = 0;
		for (j=0; j<8; j++) {
			i = Ilist[j];
			if (sentmods[i] == 0) continue;
			dn = (Bool) needmods[i];
			if (!dn) continue;
			if (multi_key[sentmods[i]]) {
				involves_multi_key = i+1;
			}
		}

		if (involves_multi_key && shift_is_down && needmods[0] < 0) {
			/*
			 * Workaround for Multi_key and shift.
			 * Assumes Shift is bit 1 (needmods[0])
			 */
			if (debug_keyboard) {
				fprintf(stderr, "    ^^^ trying to avoid "
				    "inadvertent Multi_key from Shift "
				    "(doing %03d up now)\n", shift_is_down);
			}
			myXTestFakeKeyEvent(dpy, shift_is_down, False,
			    CurrentTime);
		} else {
			involves_multi_key = 0;
		}

		for (j=0; j<8; j++) {
			/* do the Mod ups */
			i = Ilist[j];
			if (sentmods[i] == 0) continue;
			dn = (Bool) needmods[i];
			if (dn) continue;
			myXTestFakeKeyEvent(dpy, sentmods[i], dn, CurrentTime);
		}
		for (j=0; j<8; j++) {
			/* next, do the Mod downs */
			i = Ilist[j];
			if (sentmods[i] == 0) continue;
			dn = (Bool) needmods[i];
			if (!dn) continue;
			myXTestFakeKeyEvent(dpy, sentmods[i], dn, CurrentTime);
		}

		if (involves_multi_key) {
			/*
			 * Reverse workaround for Multi_key and shift.
			 */
			if (debug_keyboard) {
				fprintf(stderr, "    vvv trying to avoid "
				    "inadvertent Multi_key from Shift "
				    "(doing %03d down now)\n", shift_is_down);
			}
			myXTestFakeKeyEvent(dpy, shift_is_down, True,
			    CurrentTime);
		}

		/*
		 * With the above modifier work done, send the actual keycode:
		 */
		myXTestFakeKeyEvent(dpy, Kc_f, (Bool) down, CurrentTime);

		/*
		 * Now undo the modifier work:
		 */
		for (j=7; j>=0; j--) {
			/* reverse Mod downs we did */
			i = Ilist[j];
			if (sentmods[i] == 0) continue;
			dn = (Bool) needmods[i];
			if (!dn) continue;
			myXTestFakeKeyEvent(dpy, sentmods[i], !dn, CurrentTime);
		}
		for (j=7; j>=0; j--) {
			/* finally reverse the Mod ups we did */
			i = Ilist[j];
			if (sentmods[i] == 0) continue;
			dn = (Bool) needmods[i];
			if (dn) continue;
			myXTestFakeKeyEvent(dpy, sentmods[i], !dn, CurrentTime);
		}

	} else { /* for up case, hopefully just need to pop it up: */

		myXTestFakeKeyEvent(dpy, Kc_f, (Bool) down, CurrentTime);
	}
	X_UNLOCK;
}
#endif

/*
 * For tweaking modifiers wrt the Alt-Graph key, etc.
 */
#define LEFTSHIFT 1
#define RIGHTSHIFT 2
#define ALTGR 4
static char mod_state = 0;

static char modifiers[0x100];
static KeyCode keycodes[0x100];
static KeyCode left_shift_code, right_shift_code, altgr_code, iso_level3_code;

char *bitprint(unsigned int st) {
	static char str[9];
	int i, mask;
	for (i=0; i<8; i++) {
		str[i] = '0';
	}
	str[8] = '\0';
	mask = 1;
	for (i=7; i>=0; i--) {
		if (st & mask) {
			str[i] = '1';
		}
		mask = mask << 1;
	}
	return str;
}

void initialize_modtweak(void) {
	KeySym keysym, *keymap;
	int i, j, minkey, maxkey, syms_per_keycode;

	if (use_xkb_modtweak) {
		initialize_xkb_modtweak();
		return;
	}
	memset(modifiers, -1, sizeof(modifiers));
	for (i=0; i<0x100; i++) {
		keycodes[i] = NoSymbol;
	}

	X_LOCK;
	XDisplayKeycodes(dpy, &minkey, &maxkey);

	keymap = XGetKeyboardMapping(dpy, minkey, (maxkey - minkey + 1),
	    &syms_per_keycode);

	/* handle alphabetic char with only one keysym (no upper + lower) */
	for (i = minkey; i <= maxkey; i++) {
		KeySym lower, upper;
		/* 2nd one */
		keysym = keymap[(i - minkey) * syms_per_keycode + 1];
		if (keysym != NoSymbol) {
			continue;
		}
		/* 1st one */
		keysym = keymap[(i - minkey) * syms_per_keycode + 0];
		if (keysym == NoSymbol) {
			continue;
		}
		XConvertCase(keysym, &lower, &upper);
		if (lower != upper) {
			keymap[(i - minkey) * syms_per_keycode + 0] = lower;
			keymap[(i - minkey) * syms_per_keycode + 1] = upper;
		}
	}
	for (i = minkey; i <= maxkey; i++) {
		if (debug_keyboard) {
			if (i == minkey) {
				rfbLog("initialize_modtweak: keycode -> "
				    "keysyms mapping info:\n");
			}
			fprintf(stderr, "  %03d  ", i);
		}
		for (j = 0; j < syms_per_keycode; j++) {
			if (debug_keyboard) {
				char *sym;
				sym = XKeysymToString(XKeycodeToKeysym(dpy,
				    i, j));
				fprintf(stderr, "%-18s ", sym ? sym : "null");
				if (j == syms_per_keycode - 1) {
					fprintf(stderr, "\n");
				}
			}
			if (j >= 4) {
				/*
				 * Something wacky in the keymapping.
				 * Ignore these non Shift/AltGr chords
				 * for now...
				 */
				continue;
			}
			keysym = keymap[ (i - minkey) * syms_per_keycode + j ];
			if ( keysym >= ' ' && keysym < 0x100
			    && i == XKeysymToKeycode(dpy, keysym) ) {
				keycodes[keysym] = i;
				modifiers[keysym] = j;
			}
		}
	}

	left_shift_code = XKeysymToKeycode(dpy, XK_Shift_L);
	right_shift_code = XKeysymToKeycode(dpy, XK_Shift_R);
	altgr_code = XKeysymToKeycode(dpy, XK_Mode_switch);
	iso_level3_code = NoSymbol;
#ifdef XK_ISO_Level3_Shift
	iso_level3_code = XKeysymToKeycode(dpy, XK_ISO_Level3_Shift);
#endif

	XFree ((void *) keymap);

	X_UNLOCK;
}

/*
 * does the actual tweak:
 */
static void tweak_mod(signed char mod, rfbBool down) {
	rfbBool is_shift = mod_state & (LEFTSHIFT|RIGHTSHIFT);
	Bool dn = (Bool) down;
	KeyCode altgr = altgr_code;

	if (mod < 0) {
		if (debug_keyboard) {
			rfbLog("tweak_mod: Skip:  down=%d index=%d\n", down,
			    (int) mod);
		}
		return;
	}
	if (debug_keyboard) {
		rfbLog("tweak_mod: Start:  down=%d index=%d mod_state=0x%x"
		    " is_shift=%d\n", down, (int) mod, (int) mod_state,
		    is_shift);
	}

	if (use_iso_level3 && iso_level3_code) {
		altgr = iso_level3_code;
	}

	X_LOCK;
	if (is_shift && mod != 1) {
	    if (mod_state & LEFTSHIFT) {
		myXTestFakeKeyEvent(dpy, left_shift_code, !dn, CurrentTime);
	    }
	    if (mod_state & RIGHTSHIFT) {
		myXTestFakeKeyEvent(dpy, right_shift_code, !dn, CurrentTime);
	    }
	}
	if ( ! is_shift && mod == 1 ) {
	    myXTestFakeKeyEvent(dpy, left_shift_code, dn, CurrentTime);
	}
	if ( altgr && (mod_state & ALTGR) && mod != 2 ) {
	    myXTestFakeKeyEvent(dpy, altgr, !dn, CurrentTime);
	}
	if ( altgr && ! (mod_state & ALTGR) && mod == 2 ) {
	    myXTestFakeKeyEvent(dpy, altgr, dn, CurrentTime);
	}
	X_UNLOCK;
	if (debug_keyboard) {
		rfbLog("tweak_mod: Finish: down=%d index=%d mod_state=0x%x"
		    " is_shift=%d\n", down, (int) mod, (int) mod_state,
		    is_shift);
	}
}

/*
 * tweak the modifier under -modtweak
 */
static void modifier_tweak_keyboard(rfbBool down, rfbKeySym keysym,
    rfbClientPtr client) {
	KeyCode k;
	int tweak = 0;

	if (use_xkb_modtweak) {
		xkb_tweak_keyboard(down, keysym, client);
		return;
	}
	if (debug_keyboard) {
		rfbLog("modifier_tweak_keyboard: %s keysym=0x%x\n",
		    down ? "down" : "up", (int) keysym);
	}

	if (view_only) {
		return;
	}
	if (client->viewOnly) {
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
		myXTestFakeKeyEvent(dpy, k, (Bool) down, CurrentTime);
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
static rfbClientPtr last_keyboard_client = NULL;

void keyboard(rfbBool down, rfbKeySym keysym, rfbClientPtr client) {
	KeyCode k;
	int isbutton = 0;

	if (debug_keyboard) {
		char *str;
		X_LOCK;
		str = XKeysymToString(keysym);
		rfbLog("keyboard(%s, 0x%x \"%s\")\n", down ? "down":"up",
		    (int) keysym, str ? str : "null");
		X_UNLOCK;
	}

	if (view_only) {
		return;
	}
	if (client->viewOnly) {
		return;
	}

	last_keyboard_client = client;
	last_event = last_input = time(0);
	got_user_input++;
	got_keyboard_input++;
	
	if (keyremaps) {
		keyremap_t *remap = keyremaps;
		while (remap != NULL) {
			if (remap->before == keysym) {
				keysym = remap->after;
				isbutton = remap->isbutton;
				if (debug_keyboard) {
					X_LOCK;
					rfbLog("keyboard(): remapping keysym: "
					    "0x%x \"%s\" -> 0x%x \"%s\"\n",
					    (int) remap->before,
					    XKeysymToString(remap->before),
					    (int) remap->after,
					    remap->isbutton ? "button" :
					    XKeysymToString(remap->after));
					X_UNLOCK;
				}
				break;
			}
			remap = remap->next;
		}
	}

	if (isbutton) {
		int button = (int) keysym;
		if (! down) {
			return;	/* nothing to send */
		}
		if (debug_keyboard) {
			rfbLog("keyboard(): remapping keystroke to button %d"
			    " click\n", button);
		}
		if (button < 1 || button > num_buttons) {
			rfbLog("keyboard(): ignoring mouse button out of "
			    "bounds: %d\n", button);
			return;
		}
		X_LOCK;
		XTestFakeButtonEvent(dpy, button, True, CurrentTime);
		XTestFakeButtonEvent(dpy, button, False, CurrentTime);
		XFlush(dpy);
		X_UNLOCK;
		return;
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
		char *str = XKeysymToString(keysym);
		rfbLog("keyboard(): KeySym 0x%x \"%s\" -> KeyCode 0x%x%s\n",
		    (int) keysym, str ? str : "null", (int) k,
		    k ? "" : " *ignored*");
	}

	if ( k != NoSymbol ) {
		myXTestFakeKeyEvent(dpy, k, (Bool) down, CurrentTime);
		XFlush(dpy);
	}

	X_UNLOCK;
}

/* -- pointer.c -- */
/*
 * pointer event handling routines.
 */
typedef struct ptrremap {
	KeySym keysym;
	KeyCode keycode;
	int end;
	int button;
	int down;
	int up;
} prtremap_t;

MUTEX(pointerMutex);
#define MAX_BUTTONS 5
#define MAX_BUTTON_EVENTS 50
static prtremap_t pointer_map[MAX_BUTTONS+1][MAX_BUTTON_EVENTS];

/*
 * For parsing the -buttonmap sections, e.g. "4" or ":Up+Up+Up:"
 */
static void buttonparse(int from, char **s) {
	char *q;
	int to, i;
	int modisdown[256];

	q = *s;

	for (i=0; i<256; i++) {
		modisdown[i] = 0;
	}

	if (*q == ':') {
		/* :sym1+sym2+...+symN: format */
		int l = 0, n = 0;
		char list[1000];
		char *t, *kp = q + 1;
		KeyCode kcode;

		while (*(kp+l) != ':' && *(kp+l) != '\0') {
			/* loop to the matching ':' */
			l++;
			if (l >= 1000) {
				rfbLog("buttonparse: keysym list too long: "
				    "%s\n", q);
				break;
			}
		}
		*(kp+l) = '\0';
		strncpy(list, kp, l);
		list[l] = '\0';
		rfbLog("remap button %d using \"%s\"\n", from, list);

		/* loop over tokens separated by '+' */
		t = strtok(list, "+");
		while (t) {
			KeySym ksym;
			int i;
			if (n >= MAX_BUTTON_EVENTS - 20) {
				rfbLog("buttonparse: too many button map "
					"events: %s\n", list);
				break;
			}
			if (sscanf(t, "0x%x", &i) == 1) {
				ksym = (KeySym) i;	/* hex value */
			} else {
				ksym = XStringToKeysym(t); /* string value */
			}
			if (ksym == NoSymbol) {
				/* see if Button<N> "keysym" was used: */
				if (sscanf(t, "Button%d", &i) == 1) {
					rfbLog("   event %d: button %d\n",
					    from, n+1, i);
					if (i == 0) i = -1; /* bah */
					pointer_map[from][n].keysym  = NoSymbol;
					pointer_map[from][n].keycode = NoSymbol;
					pointer_map[from][n].button = i;
					pointer_map[from][n].end    = 0;
					pointer_map[from][n].down   = 0;
					pointer_map[from][n].up     = 0;
				} else {
					rfbLog("buttonparse: ignoring unknown "
					    "keysym: %s\n", t);
					n--;
				}
			} else {
				kcode = XKeysymToKeycode(dpy, ksym);

				pointer_map[from][n].keysym  = ksym;
				pointer_map[from][n].keycode = kcode;
				pointer_map[from][n].button = 0;
				pointer_map[from][n].end    = 0;
				if (! ismodkey(ksym) ) {
					/* do both down then up */
					pointer_map[from][n].down = 1;
					pointer_map[from][n].up   = 1;
				} else {
					if (modisdown[kcode]) {
						pointer_map[from][n].down = 0;
						pointer_map[from][n].up   = 1;
						modisdown[kcode] = 0;
					} else {
						pointer_map[from][n].down = 1;
						pointer_map[from][n].up   = 0;
						modisdown[kcode] = 1;
					}
				}
				rfbLog("   event %d: keysym %s (0x%x) -> "
				    "keycode 0x%x down=%d up=%d\n", n+1,
				    XKeysymToString(ksym), ksym, kcode,
				    pointer_map[from][n].down,
				    pointer_map[from][n].up);
			}
			t = strtok(NULL, "+");
			n++;
		}

		/* we must release any modifiers that are still down: */
		for (i=0; i<256; i++) {
			kcode = (KeyCode) i;
			if (n >= MAX_BUTTON_EVENTS) {
				rfbLog("buttonparse: too many button map "
					"events: %s\n", list);
				break;
			}
			if (modisdown[kcode]) {
				pointer_map[from][n].keysym  = NoSymbol;
				pointer_map[from][n].keycode = kcode;
				pointer_map[from][n].button = 0;
				pointer_map[from][n].end    = 0;
				pointer_map[from][n].down = 0;
				pointer_map[from][n].up   = 1;
				modisdown[kcode] = 0;
				n++;
			}
		}

		/* advance the source pointer position */
		(*s) += l+2;
	} else {
		/* single digit format */
		char str[2];
		str[0] = *q;
		str[1] = '\0';

		to = atoi(str);
		if (to < 1) {
			rfbLog("skipping invalid remap button \"%d\" for button"
			    " %d from string \"%s\"\n",
			    to, from, str);
		} else {
			rfbLog("remap button %d using \"%s\"\n", from, str);
			rfbLog("   button: %d -> %d\n", from, to);
			pointer_map[from][0].keysym  = NoSymbol;
			pointer_map[from][0].keycode = NoSymbol;
			pointer_map[from][0].button = to;
			pointer_map[from][0].end    = 0;
			pointer_map[from][0].down   = 0;
			pointer_map[from][0].up     = 0;
		}
		/* advance the source pointer position */
		(*s)++;
	}
}

/*
 * process the -buttonmap string
 */
void initialize_pointer_map(char *pointer_remap) {
	unsigned char map[MAX_BUTTONS];
	int i, k;
	/*
	 * This routine counts the number of pointer buttons on the X
	 * server (to avoid problems, even crashes, if a client has more
	 * buttons).  And also initializes any pointer button remapping
	 * from -buttonmap option.
	 */
	
	X_LOCK;
	num_buttons = XGetPointerMapping(dpy, map, MAX_BUTTONS);

	if (num_buttons < 0) {
		num_buttons = 0;
	}

	/* FIXME: should use info in map[] */
	for (i=1; i<= MAX_BUTTONS; i++) {
		for (k=0; k < MAX_BUTTON_EVENTS; k++) {
			pointer_map[i][k].end = 1;
		}
		pointer_map[i][0].keysym  = NoSymbol;
		pointer_map[i][0].keycode = NoSymbol;
		pointer_map[i][0].button = i;
		pointer_map[i][0].end    = 0;
		pointer_map[i][0].down   = 0;
		pointer_map[i][0].up     = 0;
	}

	if (pointer_remap) {
		/* -buttonmap, format is like: 12-21=2 */
		char *p, *q, *remap = pointer_remap;	
		int n;

		if ((p = strchr(remap, '=')) != NULL) {
			/* undocumented max button number */
			n = atoi(p+1);	
			*p = '\0';
			if (n < num_buttons || num_buttons == 0) {
				num_buttons = n;
			} else {
				rfbLog("warning: increasing number of mouse "
				    "buttons from %d to %d\n", num_buttons, n);
				num_buttons = n;
			}
		}
		if ((q = strchr(remap, '-')) != NULL) {
			/*
			 * The '-' separates the 'from' and 'to' lists,
			 * then it is kind of like tr(1).  
			 */
			char str[2];
			int from;

			rfbLog("remapping pointer buttons using string:\n");
			rfbLog("   \"%s\"\n", remap);

			p = remap;
			q++;
			i = 0;
			str[1] = '\0';
			while (*p != '-') {
				str[0] = *p;
				from = atoi(str);
				buttonparse(from, &q);
				p++;
			}
		}
	}
	X_UNLOCK;
}

/*
 * Send a pointer event to the X server.
 */
static void update_pointer(int mask, int x, int y) {
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
		int k;
		if (debug_pointer) {
			rfbLog("pointer(): mask change: mask: 0x%x -> "
			    "0x%x button: %d\n", button_mask, mask,i+1);
		}
		for (k=0; k < MAX_BUTTON_EVENTS; k++) {
			int bmask = (mask & (1<<i));

			if (pointer_map[i+1][k].end) {
				break;
			}

			if (pointer_map[i+1][k].button) {
				/* sent button up or down */
				mb = pointer_map[i+1][k].button;
				if ((num_buttons && mb > num_buttons)
				    || mb < 1) {
					rfbLog("ignoring mouse button out of "
					    "bounds: %d>%d mask: 0x%x -> 0x%x\n",
					    mb, num_buttons, button_mask, mask);
					continue;
				}
				if (debug_pointer) {
					rfbLog("pointer(): sending button %d"
					    " %s (event %d)\n", mb, bmask
					    ? "down" : "up", k+1);
				}
				XTestFakeButtonEvent(dpy, mb, (mask & (1<<i))
				    ? True : False, CurrentTime);
			} else {
				/* sent keysym up or down */
				KeyCode key = pointer_map[i+1][k].keycode;
				int up   = pointer_map[i+1][k].up;
				int down = pointer_map[i+1][k].down;

				if (! bmask) {
					/* do not send keysym on button up */
					continue; 
				}
				if (debug_pointer) {
					rfbLog("pointer(): sending button %d "
					    "down as keycode 0x%x (event %d)\n",
					    i+1, key, k+1);
					rfbLog("           down=%d up=%d "
					    "keysym: %s\n", down, up,
					    XKeysymToString(XKeycodeToKeysym(
					    dpy, key, 0)));
				}
				if (down) {
					myXTestFakeKeyEvent(dpy, key, True,
					    CurrentTime);
				}
				if (up) {
					myXTestFakeKeyEvent(dpy, key, False,
					    CurrentTime);
				}
			}
		}
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
 * Actual callback from libvncserver when it gets a pointer event.
 */
void pointer(int mask, int x, int y, rfbClientPtr client) {

	if (debug_pointer && mask >= 0) {
		rfbLog("pointer(mask: 0x%x, x:%4d, y:%4d)\n", mask, x, y);
	}

	if (view_only) {
		return;
	}
	if (client->viewOnly) {
		return;
	}
	if (scaling) {
		x = ((double) x / scaled_x) * dpy_x;
		if (x >= dpy_x) x = dpy_x - 1;
		y = ((double) y / scaled_y) * dpy_y;
		if (y >= dpy_y) y = dpy_y - 1;
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

/* -- bell.c -- */
/*
 * Bell event handling.  Requires XKEYBOARD extension.
 */
#ifdef LIBVNCSERVER_HAVE_XKEYBOARD

static int xkb_base_event_type;

/*
 * check for XKEYBOARD, set up xkb_base_event_type
 */
void initialize_xkb(void) {
	int ir, reason;
	int op, ev, er, maj, min;
	
	if (! use_xkb) {
		return;
	}
	if (! XkbQueryExtension(dpy, &op, &ev, &er, &maj, &min)) {
		if (! quiet) {
			fprintf(stderr, "warning: XKEYBOARD"
			    " extension not present.\n");
		}
		use_xkb = 0;
		return;
	}
	if (! XkbOpenDisplay(DisplayString(dpy), &xkb_base_event_type, &ir,
	    NULL, NULL, &reason) ) {
		if (! quiet) {
			fprintf(stderr, "warning: disabling XKEYBOARD."
			    " XkbOpenDisplay failed.\n");
		}
		use_xkb = 0;
	}
}

void initialize_watch_bell(void) {
	if (! use_xkb) {
		if (! quiet) {
			fprintf(stderr, "warning: disabling bell."
			    " XKEYBOARD ext. not present.\n");
		}
		watch_bell = 0;
		return;
	}
	if (! XkbSelectEvents(dpy, XkbUseCoreKbd, XkbBellNotifyMask,
	    XkbBellNotifyMask) ) {
		if (! quiet) {
			fprintf(stderr, "warning: disabling bell."
			    " XkbSelectEvents failed.\n");
		}
		watch_bell = 0;
	}
}

/*
 * We call this periodically to process any bell events that have 
 * taken place.
 */
void watch_bell_event(void) {
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
void watch_bell_event(void) {}
#endif

/* -- selection.c -- */
/*
 * Selection/Cutbuffer/Clipboard handlers.
 */

static int own_selection = 0;	/* whether we currently own PRIMARY or not */
static int set_cutbuffer = 0;	/* to avoid bouncing the CutText right back */
static int sel_waittime = 15;	/* some seconds to skip before first send */
static Window selwin;		/* special window for our selection */

/*
 * This is where we keep our selection: the string sent TO us from VNC
 * clients, and the string sent BY us to requesting X11 clients.
 */
static char *xcut_string = NULL;

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
static char selection_str[PROP_MAX+1];

/*
 * An X11 (not VNC) client on the local display has requested the selection
 * from us (because we are the current owner).
 *
 * n.b.: our caller already has the X_LOCK.
 */
static void selection_request(XEvent *ev) {
	XSelectionEvent notify_event;
	XSelectionRequestEvent *req_event;
	XErrorHandler old_handler;
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

	/* the window may have gone away, so trap errors */
	trapped_xerror = 0;
	old_handler = XSetErrorHandler(trap_xerror);

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

	if (! trapped_xerror) {
		XSendEvent(req_event->display, req_event->requestor, False, 0,
		    (XEvent *)&notify_event);
	} 
	if (trapped_xerror) {
		rfbLog("selection_request: ignored XError while sending "
		    "PRIMARY selection to 0x%x.\n", req_event->requestor);
	}
	XSetErrorHandler(old_handler);
	trapped_xerror = 0;

	XFlush(dpy);
}

/*
 * CUT_BUFFER0 property on the local display has changed, we read and
 * store it and send it out to any connected VNC clients.
 *
 * n.b.: our caller already has the X_LOCK.
 */
static void cutbuffer_send(void) {
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
static void selection_send(XEvent *ev) {
	Atom type;
	int format, slen, dlen, oldlen, newlen, toobig = 0;
	static int err = 0, sent_one = 0;
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
 * This routine is periodically called to check for selection related
 * and other X11 events and respond to them as needed.
 */
void watch_xevents(void) {
	XEvent xev;
	static int first = 1, sent_some_sel = 0;
	static time_t last_request = 0;
	time_t now = time(0);
	int have_clients = screen->rfbClientHead ? 1 : 0;

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
	if (have_clients && watch_selection && ! sent_some_sel
	    && now > last_client + sel_waittime) {
		if (XGetSelectionOwner(dpy, XA_PRIMARY) == None) {
			cutbuffer_send();
		}
		sent_some_sel = 1;
	}

	if (XCheckTypedEvent(dpy, MappingNotify, &xev)) {
		if (use_modifier_tweak) {
			X_UNLOCK;
			initialize_modtweak();
			X_LOCK;
		}
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
					sent_some_sel = 1;
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
				    || sent_some_sel) {
					selection_send(&xev);
				}
			}
		}
		if (now > last_request) {
			/*
			 * Every second or two, request PRIMARY, unless we
			 * already own it or there is no owner or we have
			 * no clients.
			 * TODO: even at this low rate we should look into
			 * and performance problems in odds cases, etc.
			 */
			last_request = now;
			if (! own_selection && have_clients &&
			    XGetSelectionOwner(dpy, XA_PRIMARY) != None) {
				XConvertSelection(dpy, XA_PRIMARY, XA_STRING,
				    XA_STRING, selwin, CurrentTime);
			}
		}
	}

	if (own_selection) {
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
	}
	X_UNLOCK;
}

/*
 * hook called when a VNC client sends us some "XCut" text (rfbClientCutText).
 */
void xcut_receive(char *text, int len, rfbClientPtr cl) {

	if (cl->viewOnly) {
		return;
	}
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
	    PropModeReplace, (unsigned char *) text, len);
	XFlush(dpy);

	X_UNLOCK;

	set_cutbuffer = 1;
}

/* -- cursor.c -- */
/*
 * Here begins a bit of a mess to experiment with multiple cursors 
 * drawn on the remote background ...
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
static cursor_info_t cur0 = {NULL, NULL, CUR_SIZE, CUR_SIZE, 0, 0, 0};

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
static cursor_info_t cur1 = {NULL, NULL, 18, 18, 8, 8, 1};

static cursor_info_t *cursors[2];

static void setup_cursors(void) {
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
static char cur_save[(4 * CUR_SIZE * CUR_SIZE)];
static int cur_save_cx, cur_save_cy, cur_save_which;

/*
 * save current cursor info and the patch of non-cursor data it covers
 */
static void save_mouse_patch(int x, int y, int w, int h, int cx, int cy,
    int which) {
	int pixelsize = bpp >> 3;
	char *fbp = main_fb;
	int ly, i = 0;

	for (ly = y; ly < y + h; ly++) {
		memcpy(cur_save+i, fbp + ly * main_bytes_per_line
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
void restore_mouse_patch(void) {
	int pixelsize = bpp >> 3;
	char *fbp = main_fb;
	int ly, i = 0;

	if (! cur_saved) {
		return;		/* not yet saved */
	}

	for (ly = cur_save_y; ly < cur_save_y + cur_save_h; ly++) {
		memcpy(fbp + ly * main_bytes_per_line + cur_save_x * pixelsize,
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
static int tree_descend_cursor(void) {
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

/*
 * This is for mouse patch drawing under -xinerama or -blackout
 */
static void blackout_nearby_tiles(x, y, dt) {
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
 * Send rfbCursorPosUpdates back to clients that understand them.  This
 * seems to be TightVNC specific.
 */
static void cursor_pos_updates(int x, int y) {
	rfbClientIteratorPtr iter;
	rfbClientPtr cl;
	static time_t last_warp = 0;
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
			time_t now = time(0);
			/*
			 * special case if this client was the last one to
			 * send a pointer position.
			 */
			if (x == cursor_x && y == cursor_y && now>last_warp+5) {
				cl->cursorWasMoved = FALSE;
			} else {
				/* an X11 app evidently warped the pointer */
				if (debug_pointer) {
					rfbLog("cursor_pos_updates: warp "
					    "detected dx=%3d dy=%3d\n",
					    cursor_x - x, cursor_y - y);
				}
				cl->cursorWasMoved = TRUE;
				last_warp = now;
				cnt++;
			}
		} else {
			cl->cursorWasMoved = TRUE;
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
static void draw_mouse(int x, int y, int which, int update) {
	int px, py, i, offset;
	int pixelsize = bpp >> 3;
	char *fbp = main_fb;
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

			offset = (y0 + py)*main_bytes_per_line + (x0 + px)*pixelsize;

			/* fill in each color byte in the fb */
			for (i=0; i < pixelsize; i++) {
				fbp[offset+i] = (char) shade;
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

/*
 * wrapper to redraw the mouse patch
 */
void redraw_mouse(void) {
	if (cur_saved) {
		/* redraw saved mouse from info (save_mouse_patch) */
		draw_mouse(cur_save_cx, cur_save_cy, cur_save_which, 0);
	}
}

/*
 * routine called periodically to update the mouse aspects (drawn & 
 * cursorpos updates)
 */
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

/* -- screen.c -- */
/*
 * X11 and rfb display/screen related routines
 */

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
		if (! quiet) {
			fprintf(stderr, "set_colormap: number of cells is %d "
			    "instead of %d.\n", ncells, NCOLOR);
		}
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
	if (ncells > NCOLOR && ! quiet) {
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

	if (! quiet) {
		fprintf(stderr, "set_visual: %s\n", vstring);
	}

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
		int v_in;
		if (sscanf(vstring, "0x%x", &v_in) != 1) {
			if (sscanf(vstring, "%d", &v_in) == 1) {
				visual_id = (VisualID) v_in;
				return;
			}
			fprintf(stderr, "bad -visual arg: %s\n", vstring);
			exit(1);
		}
		visual_id = (VisualID) v_in;
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
	main_fb = fb->data;
	rfb_fb = main_fb;
	screen->frameBuffer = rfb_fb;
	loaded_fb = 1;
	screen->displayHook = NULL;
}

/*
 * initialize the rfb framebuffer/screen
 */
void initialize_screen(int *argc, char **argv, XImage *fb) {
	int have_masks = 0;
	int width  = fb->width;
	int height = fb->height;
	
	main_bytes_per_line = fb->bytes_per_line;

	main_red_mask   = fb->red_mask;
	main_green_mask = fb->green_mask;
	main_blue_mask  = fb->blue_mask;

	if (scaling) {
		double eps = 0.000001;
		width  = (int) (width  * scale_fac + eps); 
		height = (int) (height * scale_fac + eps); 
		if (scale_denom && scaling_pad) {
			/* it is not clear this padding is useful anymore */
			rfbLog("width  %% denom: %d %% %d = %d\n", width,
			    scale_denom, width  % scale_denom);
			rfbLog("height %% denom: %d %% %d = %d\n", height,
			    scale_denom, height % scale_denom);
			if (width % scale_denom != 0) {
				int w = width;
				w += scale_denom - (w % scale_denom);
				if (!scaling_nomult4 && w % 4 != 0) {
					/* need to make mult of 4 as well */
					int c = 0;	
					while (w % 4 != 0 && c++ <= 5) {
						w += scale_denom;
					}
				}
				width = w;
				rfbLog("padded width  to: %d (mult of %d%s\n",
				    width, scale_denom, !scaling_nomult4 ?
				    " and 4)" : ")");
			}
			if (height % scale_denom != 0) {
				height += scale_denom - (height % scale_denom);
				rfbLog("padded height to: %d (mult of %d)\n",
				    height, scale_denom);
			}
		}
		if (!scaling_nomult4 && width % 4 != 0 && width > 2) {
			/* reset width to be multiple of 4 */
			int width0 = width;
			if ((width+1) % 4 == 0) {
				width = width+1;
			} else if ((width-1) % 4 == 0) {
				width = width-1;
			} else if ((width+2) % 4 == 0) {
				width = width+2;
			}
			rfbLog("reset scaled width %d -> %d to be a multiple of"
			    " 4 (to\n", width0, width);
			rfbLog("make vncviewers happy). use -scale m/n:n4 to "
			    "disable.\n");
		}
		scaled_x = width;
		scaled_y = height;
		rfb_bytes_per_line = (main_bytes_per_line / fb->width) * width;
		rfbLog("scaling screen: %dx%d -> %dx%d  scale_fac=%.5f\n",
		    fb->width, fb->height, scaled_x, scaled_y, scale_fac);
	} else {
		rfb_bytes_per_line = main_bytes_per_line;
	}

	screen = rfbGetScreen(argc, argv, width, height, fb->bits_per_pixel,
	    8, fb->bits_per_pixel/8);

	if (! quiet) {
		fprintf(stderr, "\n");
	}

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
	if (strcmp(LIBVNCSERVER_VERSION, "0.5") 
	    && strcmp(LIBVNCSERVER_VERSION, "0.6")) {
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

	screen->paddedWidthInBytes = rfb_bytes_per_line;
	screen->rfbServerFormat.bitsPerPixel = fb->bits_per_pixel;
	screen->rfbServerFormat.depth = fb->depth;
	screen->rfbServerFormat.trueColour = (uint8_t) TRUE;
	have_masks = ((fb->red_mask|fb->green_mask|fb->blue_mask) != 0);
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

		main_red_max   = screen->rfbServerFormat.redMax;
		main_green_max = screen->rfbServerFormat.greenMax;
		main_blue_max  = screen->rfbServerFormat.blueMax;

		main_red_shift   = screen->rfbServerFormat.redShift;
		main_green_shift = screen->rfbServerFormat.greenShift;
		main_blue_shift  = screen->rfbServerFormat.blueShift;
	}

	/* nofb is for pointer/keyboard only handling.  */
	if (nofb) {
		main_fb = NULL;
		rfb_fb = main_fb;
		screen->displayHook = nofb_hook;
	} else {
		main_fb = fb->data;
		if (scaling) {
			rfb_fb = (char *) malloc(rfb_bytes_per_line * height);
			memset(rfb_fb, 0, rfb_bytes_per_line * height);
		} else {
			rfb_fb = main_fb;
		}
	}
	screen->frameBuffer = rfb_fb;

	/* called from inetd, we need to treat stdio as our socket */
	if (inetd) {
		int fd = dup(0);
		if (fd < 3) {
			rfbErr("dup(0) = %d failed.\n", fd);
			rfbLogPerror("dup");
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


	bpp = screen->rfbServerFormat.bitsPerPixel;
	depth = screen->rfbServerFormat.depth;

	if (scaling) {
		mark_rect_as_modified(0, 0, dpy_x, dpy_y, 0);
	}

	if (viewonly_passwd) {
		/* append the view only passwd after the normal passwd */
		char **passwds_new = malloc(3*sizeof(char**));
		char **passwds_old = (char **) screen->rfbAuthPasswdData;
		passwds_new[0] = passwds_old[0];
		passwds_new[1] = viewonly_passwd;
		passwds_new[2] = NULL;
		screen->rfbAuthPasswdData = (void*) passwds_new;
	}
}

/* -- xinerama.c -- */
/*
 * routines related to xinerama and blacking out rectangles
 */

/*
 * Take a comma separated list of geometries: WxH+X+Y and register them as
 * rectangles to black out from the screen.
 */
void initialize_blackout (char *list) {
	char *p, *blist = strdup(list);
	int x, y, X, Y, h, w;

	p = strtok(blist, ", \t");
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
			p = strtok(NULL, ", \t");
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
			blackr[blackouts].x1 = x;
			blackr[blackouts].y1 = y;
			blackr[blackouts].x2 = X;
			blackr[blackouts].y2 = Y;
			blackouts++;
			if (blackouts >= 100) {
				rfbLog("too many blackouts: %d\n", blackouts);
				break;
			}
		}
		p = strtok(NULL, ", \t");
	}
	free(blist);
}

/*
 * Now that all blackout rectangles have been constructed, see what overlap
 * they have with the tiles in the system.  If a tile is touched by a
 * blackout, record information.
 */
void blackout_tiles(void) {
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
				    sraRgnCreateRect(blackr[b].x1, blackr[b].y1,
				    blackr[b].x2, blackr[b].y2);

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

void initialize_xinerama (void) {
#ifndef LIBVNCSERVER_HAVE_LIBXINERAMA
	rfbLog("Xinerama: Library libXinerama is not available to determine\n");
	rfbLog("Xinerama: the head geometries, consider using -blackout\n");
	rfbLog("Xinerama: if the screen is non-rectangular.\n");
#else
	XineramaScreenInfo *sc, *xineramas;
	sraRegionPtr black_region, tmp_region;
	sraRectangleIterator *iter;
	sraRect rect;
	char *bstr, *tstr;
	int ev, er, i, n, rcnt;

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

	dst = main_fb + y1 * main_bytes_per_line + x1 * pixelsize;
	line = y1;
	while (line++ < y2) {
		memset(dst, fill, (size_t) (x2 - x1) * pixelsize);
		dst += main_bytes_per_line;
	}
}

/* -- scan.c -- */
/*
 * routines for scanning and reading the X11 display for changes, and
 * for doing all the tile work (shm, etc).
 */

/* array to hold the hints: */
static hint_t *hint_list;

/* nap state */
static int nap_ok = 0, nap_diff_count = 0;

static int scan_count = 0;	/* indicates which scan pattern we are on  */
static int scan_in_progress = 0;	

/* scan pattern jitter from x0rfbserver */
#define NSCAN 32
static int scanlines[NSCAN] = {
	 0, 16,  8, 24,  4, 20, 12, 28,
	10, 26, 18,  2, 22,  6, 30, 14,
	 1, 17,  9, 25,  7, 23, 15, 31,
	19,  3, 27, 11, 29, 13,  5, 21
};

typedef struct tile_change_region {
	/* start and end lines, along y, of the changed area inside a tile. */
	unsigned short first_line, last_line;
	/* info about differences along edges. */
	unsigned short left_diff, right_diff;
	unsigned short top_diff,  bot_diff;
} region_t;

/* array to hold the tiles region_t-s. */
static region_t *tile_region;


/*
 * setup tile numbers and allocate the tile and hint arrays:
 */
void initialize_tiles(void) {

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
static int fs_factor = 0;

static void set_fs_factor(int max) {
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
static int shm_create(XShmSegmentInfo *shm, XImage **ximg_ptr, int w, int h,
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
		rfbLogPerror("shmget");

		XDestroyImage(xim);
		*ximg_ptr = NULL;

		X_UNLOCK;
		return 0;
	}

	shm->shmaddr = xim->data = (char *) shmat(shm->shmid, 0, 0);

	if (shm->shmaddr == (char *)-1) {
		rfbErr("shmat(%s) failed.\n", name);
		rfbLogPerror("shmat");

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

void initialize_shm(void) {
	int i;

	/* set all shm areas to "none" before trying to create any */
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
static void create_tile_hint(int x, int y, int th, hint_t *hint) {
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

static void extend_tile_hint(int x, int y, int th, hint_t *hint) {
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

static void save_hint(hint_t hint, int loc) {
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
static void hint_updates(void) {
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
 * kludge, simple ceil+floor for non-negative doubles:
 */
#define CEIL(x)  ( (double) ((int) (x)) == (x) ? \
	(double) ((int) (x)) : (double) ((int) (x) + 1) )
#define FLOOR(x) ( (double) ((int) (x)) )

/*
 * Scaling:
 *
 * For shrinking, a destination (scaled) pixel will correspond to more
 * than one source (i.e. main fb) pixel.  Think of an x-y plane made with
 * graph paper.  Each unit square in the graph paper (i.e. collection of
 * points (x,y) such that N < x < N+1 and M < y < M+1, N and M integers)
 * corresponds to one pixel in the unscaled fb.  There is a solid
 * color filling the inside of such a square.  A scaled pixel has width
 * 1/scale_fac, e.g. for "-scale 3/4" the width of the scaled pixel
 * is 1.333.  The area of this scaled pixel is 1.333 * 1.333 (so it
 * obviously overlaps more than one source pixel, each which have area 1).
 *
 * We take the weight an unscaled pixel (source) contributes to a
 * scaled pixel (destination) as simply proportional to the overlap area
 * between the two pixels.  One can then think of the value of the scaled
 * pixel as an integral over the portion of the graph paper it covers.
 * The thing being integrated is the color value of the unscaled source.
 * That color value is constant over a graph paper square (source pixel),
 * and changes discontinuously from one unit square to the next.
 *

Here is an example for -scale 3/4, the solid lines are the source pixels
(graph paper unit squares), while the dotted lines denote the scaled
pixels (destination pixels):

            0         1 4/3     2     8/3 3         4=12/3
            |---------|--.------|------.--|---------|.                
            |         |  .      |      .  |         |.                
            |    A    |  . B    |      .  |         |.                
            |         |  .      |      .  |         |.                
            |         |  .      |      .  |         |.                
          1 |---------|--.------|------.--|---------|.                
         4/3|.........|.........|.........|.........|.                
            |         |  .      |      .  |         |.                
            |    C    |  . D    |      .  |         |.                
            |         |  .      |      .  |         |.                
          2 |---------|--.------|------.--|---------|.                
            |         |  .      |      .  |         |.                
            |         |  .      |      .  |         |.                
         8/3|.........|.........|.........|.........|.                
            |         |  .      |      .  |         |.                
          3 |---------|--.------|------.--|---------|.                

So we see the first scaled pixel (0 < x < 4/3 and 0 < y < 4/3) mostly
overlaps with unscaled source pixel "A".  The integration (averaging)
weights for this scaled pixel are:

			A	 1
			B	1/3
			C	1/3
			D	1/9

 *
 * The Red, Green, and Blue color values must be averaged over separately
 * otherwise you can get a complete mess (except in solid regions),
 * because high order bits are averaged differently from the low order bits.
 *
 * So the algorithm is roughly:
 *
 *   - Given as input a rectangle in the unscaled source fb with changes,
 *     find the rectangle of pixels this affects in the scaled destination fb.
 *
 *   - For each of the affected scaled (dest) pixels, determine all of the
 *     unscaled (source) pixels it overlaps with.
 *  
 *   - Average those unscaled source values together, weighted by the area
 *     overlap with the destination pixel.  Average R, G, B separately.
 *
 *   - Take this average value and convert to a valid pixel value if
 *     necessary (e.g. rounding, shifting), and then insert it into the
 *     destination framebuffer as the pixel value.
 *
 *   - On to the next destination pixel...
 *
 * ========================================================================
 *
 * For expanding, e.g. -scale 1.1 (which we don't think people will do
 * very often... or at least so we hope, the framebuffer can become huge)
 * the situation is reversed and the destination pixel is smaller than a
 * "graph paper" unit square (source pixel).  Some destination pixels
 * will be completely within a single unscaled source pixel.
 *
 * What we do here is a simple 4 point interpolation scheme:
 * 
 * Let P00 be the source pixel closest to the destination pixel but with
 * x and y values less than or equal to those of the destination pixel.
 * (for simplicity, think of the upper left corner of a pixel defining the
 * x,y location of the pixel, the center would work just as well).  So it
 * is the source pixel immediately to the upper left of the destination
 * pixel.  Let P10 be the source pixel one to the right of P00.  Let P01
 * be one down from P00.  And let P11 be one down and one to the right
 * of P00.  They form a 2x2 square we will interpolate inside of.
 * 
 * Let V00, V10, V01, and V11 be the color values of those 4 source
 * pixels.  Let dx be the displacement along x the destination pixel is
 * from P00.  Note: 0 <= dx < 1 by definition of P00.  Similarly let
 * dy be the displacement along y.  The weighted average for the
 * interpolation is:
 * 
 * 	V_ave = V00 * (1 - dx) * (1 - dy)
 * 	      + V10 *      dx  * (1 - dy)
 * 	      + V01 * (1 - dx) *      dy
 * 	      + V11 *      dx  *      dy
 * 
 * Note that the weights (1-dx)*(1-dy) + dx*(1-dy) + (1-dx)*dy + dx*dy
 * automatically add up to 1.  It is also nice that all the weights are
 * positive (unsigned char stays unsigned char).  The above formula can
 * be motivated by doing two 1D interpolations along x:
 * 
 * 	VA = V00 * (1 - dx) + V10 * dx
 * 	VB = V01 * (1 - dx) + V11 * dx
 * 
 * and then interpolating VA and VB along y:
 * 
 * 	V_ave = VA * (1 - dy) + VB * dy
 * 
 *                      VA 
 *           v   |<-dx->|
 *           -- V00 ------ V10
 *           dy  |          |  
 *           --  |      o...|...    "o" denotes the position of the desired
 *           ^   |      .   |  .    destination pixel relative to the P00
 *               |      .   |  .    source pixel.
 *              V10 ----.- V11 .
 *                      ........
 *                      |  
 *                      VB 
 *
 * 
 * Of course R, G, B averages are done separately as in the shrinking
 * case.  This gives reasonable results, and the implementation for
 * shrinking can simply be used with different choices for weights for
 * the loop over the 4 pixels.
 */

static void scale_and_mark_rect(int X1, int Y1, int X2, int Y2) {
/*
 * Notation:
 * "i" an x pixel index in the destination (scaled) framebuffer
 * "j" a  y pixel index in the destination (scaled) framebuffer
 * "I" an x pixel index in the source (un-scaled, i.e. main) framebuffer
 * "J" a  y pixel index in the source (un-scaled, i.e. main) framebuffer
 *
 *  Similarly for nx, ny, Nx, Ny, etc.  Lowercase: dest, Uppercase: source.
 */
	int Nx, Ny, nx, ny, Bpp, b;

	int i, j, i1, i2, j1, j2;	/* indices for scaled fb (dest) */
	int I, J, I1, I2, J1, J2;	/* indices for main fb   (source) */

	double w, wx, wy, wtot;	/* pixel weights */

	double x1, y1, x2, y2;	/* x-y coords for destination pixels edges */
	double dx, dy;		/* size of destination pixel */

	double ddx, ddy;	/* for interpolation expansion */

	char *src, *dest;	/* pointers to the two framebuffers */

	double pixave[4];	/* for averaging pixel values */

	unsigned short us;

	int shrink;		/* whether shrinking or expanding */
	static int constant_weights = -1, cnt = 0;

	if (scale_fac <= 1.0) {
		shrink = 1;
	} else {
		shrink = 0;
	}
	if (shrink && scaling_interpolate) {
		/*
		 * User asked for interpolation scheme, presumably for
		 * small shrink. 
		 */
		shrink = 0;
	}

	if (! screen->rfbServerFormat.trueColour) {
		/*
		 * PseudoColor colormap... blending leads to random colors.
		 */
		scaling_noblend = 1;
	}

	Bpp = bpp/8;	/* Bytes per pixel */

	Nx = dpy_x;	/* extent of source (the whole main fb) */
	Ny = dpy_y;

	nx = scaled_x;	/* extent of dest (the whole scaled rfb fb) */
	ny = scaled_y;

	/*
	 * width and height (real numbers) of a scaled pixel.
	 * both are > 1   (e.g. 1.333 for -scale 3/4)
	 * they should also be equal but we don't assume it.
	 */

	/*
	 * This original way is probably incorrect, giving rise to dx and
	 * dy that will not exactly line up with the grid for 2/3, etc.
	 * This gives rise to a whole spectrum of weights, leading to poor
	 * tightvnc (and other encoding) compression. 
	 */
#if 0
	dx = (double) Nx / nx;
	dy = (double) Ny / ny;
#else
	
	/*
	 * This new way is probably the best we can do, take the inverse
	 * of the scaling factor to double precision.
	 */
	dx = 1.0/scale_fac;
	dy = 1.0/scale_fac;
#endif

	/*
	 * find the extent of the change the input rectangle induces in
	 * the scaled framebuffer.
	 */

	/* Left edges: find largest i such that i * dx <= X1  */
	i1 = FLOOR(X1/dx);

	/* Right edges: find smallest i such that (i+1) * dx >= X2+1  */
	i2 = CEIL( (X2+1)/dx ) - 1;

	/* To be safe, correct any overflows: */
	i1 = nfix(i1, nx);
	i2 = nfix(i2, nx) + 1;	/* add 1 to make a rectangle upper boundary */

	/* Repeat above for y direction: */
	j1 = FLOOR(Y1/dy);
	j2 = CEIL( (Y2+1)/dy ) - 1;

	j1 = nfix(j1, ny);
	j2 = nfix(j2, ny) + 1;

	/*
	 * There is some speedup if the pixel weights are constant, so
	 * let's special case these.
	 *
	 * If scale = 1/n and n divides Nx and Ny, the pixel weights
	 * are constant (e.g. 1/2 => equal on 2x2 square).
	 */
	if (constant_weights < 0) {
		int n = 0;
		constant_weights = 0;

		for (i = 2; i<=128; i++) {
			double test = ((double) 1)/ i;
			double diff, eps = 1.0e-7;
			diff = scale_fac - test;
			if (-eps < diff && diff < eps) {
				n = i;
				break;
			}
		}
		if (scaling_noblend || ! shrink) {
			;
		} else if (n != 0) {
			if (Nx % n == 0 && Ny % n == 0) {
				rfbLog("scale_and_mark_rect: using constant "
				    "pixel weight speedup for 1/%d\n", n);
				constant_weights = 1;
			}
		}
	}
	/* set these all to 1.0 to begin with */
	wx = 1.0;
	wy = 1.0;
	w  = 1.0;

	/*
	 * Loop over destination pixels in scaled fb:
	 */
	for (j=j1; j<j2; j++) {
		y1 =  j * dy;	/* top edge */
		if (y1 > Ny - 1) {
			/* can go over with dy = 1/scale_fac */
			y1 = Ny - 1;
		}
		y2 = y1 + dy;	/* bottom edge */

		/* Find main fb indices covered by this dest pixel: */
		J1 = (int) FLOOR(y1);
		J1 = nfix(J1, Ny);

		if (shrink) {
			J2 = (int) CEIL(y2) - 1;
			J2 = nfix(J2, Ny);
		} else {
			J2 = J1 + 1;	/* simple interpolation */
			ddy = y1 - J1;
		}

		/* destination char* pointer: */
		dest = rfb_fb + j*rfb_bytes_per_line + i1*Bpp;
		
		for (i=i1; i<i2; i++) {
			x1 =  i * dx;	/* left edge */
			if (x1 > Nx - 1) {
				/* can go over with dx = 1/scale_fac */
				x1 = Nx - 1;
			}
			x2 = x1 + dx;	/* right edge */

			cnt++;

			/* Find main fb indices covered by this dest pixel: */
			I1 = (int) FLOOR(x1);
			if (I1 >= Nx) I1 = Nx - 1;

			if (shrink) {
				I2 = (int) CEIL(x2) - 1;
				if (I2 >= Nx) I2 = Nx - 1;
			} else {
				I2 = I1 + 1;	/* simple interpolation */
				ddx = x1 - I1;
			}
			
			/* Zero out accumulators for next pixel average: */
			for (b=0; b<4; b++) {
				pixave[b] = 0.0; /* for RGB weighted sums */
			}

			/*
			 * wtot is for accumulating the total weight.
			 * It should always sum to 1/(scale_fac * scale_fac).
			 */
			wtot = 0.0;

			/*
			 * Loop over source pixels covered by this dest pixel.
			 * 
			 * These "extra" loops over "J" and "I" make
			 * the cache/cacheline performance unclear.
			 * For example, will the data brought in from
			 * src for j, i, and J=0 still be in the cache
			 * after the J > 0 data have been accessed and
			 * we are at j, i+1, J=0?  The stride in J is
			 * main_bytes_per_line, and so ~4 KB.
			 */
			for (J=J1; J<=J2; J++) {
			    /* see comments for I, x1, x2, etc. below */
			    if (constant_weights) {
				;
			    } else if (scaling_noblend) {
				if (J != J1) {
					continue;
				}
				wy = 1.0;

				/* interpolation scheme: */
			    } else if (!shrink) {
				if (J >= Ny) {
					continue;
				} else if (J == J1) {
					wy = 1.0 - ddy;
				} else if (J != J1) {
					wy = ddy;
				}

				/* integration scheme: */
			    } else if (J < y1) {
				wy = J+1 - y1;
			    } else if (J+1 > y2) {
				wy = y2 - J;
			    } else {
				wy = 1.0;
			    }

			    src = main_fb + J*main_bytes_per_line + I1*Bpp;

			    for (I=I1; I<=I2; I++) {

				/* Work out the weight: */

				if (constant_weights) {
					;
				} else if (scaling_noblend) {
					/*
					 * Ugh, PseudoColor colormap is
					 * bad news, to avoid random
					 * colors just take the first
					 * pixel.  Or user may have
					 * specified :nb to fraction.
					 */
					if (I != I1) {
						continue;
					}
					wx = 1.0;

					/* interpolation scheme: */
				} else if (!shrink) {
					if (I >= Nx) {
						continue;	/* off edge */
					} else if (I == I1) {
						wx = 1.0 - ddx;
					} else if (I != I1) {
						wx = ddx;
					}

					/* integration scheme: */
				} else if (I < x1) {
					/* 
					 * source left edge (I) to the
					 * left of dest left edge (x1):
					 * fractional weight
					 */
					wx = I+1 - x1;
				} else if (I+1 > x2) {
					/* 
					 * source right edge (I+1) to the
					 * right of dest right edge (x2):
					 * fractional weight
					 */
					wx = x2 - I;
				} else {
					/* 
					 * source edges (I and I+1) completely
					 * inside dest edges (x1 and x2):
					 * full weight
					 */
					wx = 1.0;
				}

				w = wx * wy;
				wtot += w;


				/* 
				 * We average the unsigned char value
				 * instead of char value: otherwise
				 * the minimum (char 0) is right next
				 * to the maximum (char -1)!  This way
				 * they are spread between 0 and 255.
				 */
				if (Bpp != 2) {
					for (b=0; b<Bpp; b++) {
						pixave[b] += w *
						    ((unsigned char) *(src+b));
					}
				} else {
					/*
					 * 16bpp: trickier with green
					 * split over two bytes, so we
					 * use the masks:
					 */
					us = *( (unsigned short *) src );
					pixave[0] += w*(us & main_red_mask);
					pixave[1] += w*(us & main_green_mask);
					pixave[2] += w*(us & main_blue_mask);
				}
				src += Bpp;
			    }
			}

			if (wtot <= 0.0) {
				wtot = 1.0;
			}
			wtot = 1.0/wtot;	/* normalization factor */

			/* place weighted average pixel in the scaled fb: */
			if (Bpp != 2) {
				for (b=0; b<Bpp; b++) {
					*(dest + b) = (char) (wtot * pixave[b]);
				}
			} else {
				/* 16bpp / 565 case: */
				pixave[0] *= wtot;
				pixave[1] *= wtot;
				pixave[2] *= wtot;
				us =  (main_red_mask   & (int) pixave[0])
				    | (main_green_mask & (int) pixave[1])
				    | (main_blue_mask  & (int) pixave[2]);
				*( (unsigned short *) dest ) = us;
			}
			dest += Bpp;
		}
	}

	mark_rect_as_modified(i1, j1, i2, j2, 1);
}

void mark_rect_as_modified(int x1, int y1, int x2, int y2, int force) {
	
	if (rfb_fb == main_fb || force) {
		rfbMarkRectAsModified(screen, x1, y1, x2, y2);
	} else if (scaling) {
		scale_and_mark_rect(x1, y1, x2, y2);
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

	mark_rect_as_modified(x, y, x + w, y + h, 0);
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
static int *first_line = NULL, *last_line;
static unsigned short *left_diff, *right_diff;

static void copy_tiles(int tx, int ty, int nt) {
	int x, y, line;
	int size_x, size_y, width1, width2;
	int off, len, n, dw, dx, t;
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
	dst = main_fb + y * main_bytes_per_line + x * pixelsize;

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
		s_dst += main_bytes_per_line;
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
	m_dst = dst + (main_bytes_per_line * size_y);

	for (t=1; t <= nt; t++) {
		last_line[t] = first_line[t];
	}

	/* find the last line with difference: */
	w1 = width1 * pixelsize;
	w2 = width2 * pixelsize;

	/* foreach line: */
	for (line = size_y - 1; line > first_min; line--) {

		m_src -= tile_row[nt]->bytes_per_line;
		m_dst -= main_bytes_per_line;

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
		h_dst += main_bytes_per_line;
	}

	/* now finally copy the difference to the rfb framebuffer: */
	s_src = src + tile_row[nt]->bytes_per_line * first_min;
	s_dst = dst + main_bytes_per_line * first_min;

	for (line = first_min; line <= last_max; line++) {
		/* for I/O speed we do not do this tile by tile */
		memcpy(s_dst, s_src, size_x * pixelsize);
		s_src += tile_row[nt]->bytes_per_line;
		s_dst += main_bytes_per_line;
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
static int copy_all_tiles(void) {
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
static int copy_all_tile_runs(void) {
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
static int copy_tiles_backward_pass(void) {
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

static void gap_try(int x, int y, int *run, int *saw, int along_x) {
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
static int fill_tile_gaps(void) {
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

static void island_try(int x, int y, int u, int v, int *run) {
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
static int grow_islands(void) {
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
 * Fill the framebuffer with zeros for each blackout region
 */
static void blackout_regions(void) {
	int i;
	for (i=0; i < blackouts; i++) {
		zero_fb(blackr[i].x1, blackr[i].y1, blackr[i].x2, blackr[i].y2);
	}
}

/*
 * copy the whole X screen to the rfb framebuffer.  For a large enough
 * number of changed tiles, this is faster than tiles scheme at retrieving
 * the info from the X server.  Bandwidth to client and compression time
 * are other issues...  use -fs 1.0 to disable.
 */
void copy_screen(void) {
	int pixelsize = bpp >> 3;
	char *fbp;
	int i, y, block_size;

	block_size = (dpy_x * (dpy_y/fs_factor) * pixelsize);

	fbp = main_fb;
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
		memcpy(fbp, fullscreen->data, (size_t) block_size);

		y += dpy_y / fs_factor;
		fbp += block_size;
	}

	X_UNLOCK;

	if (blackouts) {
		blackout_regions();
	}

	mark_rect_as_modified(0, 0, dpy_x, dpy_y, 0);
}


/*
 * Utilities for managing the "naps" to cut down on amount of polling.
 */
static void nap_set(int tile_cnt) {

	if (scan_count == 0) {
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

/*
 * split up a long nap to improve the wakeup time
 */
static void nap_sleep(int ms, int split) {
	int i, input = got_user_input;

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

/*
 * see if we should take a nap of some sort between polls
 */
static void nap_check(int tile_cnt) {
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
static void ping_clients(int tile_cnt) {
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
		mark_rect_as_modified(0, 0, 1, 1, 1);
		last_send = now;
	}
}

/*
 * scan_display() wants to know if this tile can be skipped due to
 * blackout regions: (no data compare is done, just a quick geometric test)
 */
static int blackout_line_skip(int n, int x, int y, int rescan,
    int *tile_count) {
	
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
static int blackout_line_cmpskip(int n, int x, int y, char *dst, char *src,
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
 * Loop over 1-pixel tall horizontal scanlines looking for changes.  
 * Record the changes in tile_has_diff[].  Scanlines in the loop are
 * equally spaced along y by NSCAN pixels, but have a slightly random
 * starting offset ystart ( < NSCAN ) from scanlines[].
 */
static int scan_display(int ystart, int rescan) {
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
		dst = main_fb + y * main_bytes_per_line;

		if (whole_line && ! memcmp(dst, src, main_bytes_per_line)) {
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
			dst = main_fb + y * main_bytes_per_line + x * pixelsize;

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
void scan_for_updates(void) {
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

	scan_count++;
	scan_count %= NSCAN;

	if (scan_count % (NSCAN/4) == 0)  {
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
	tile_count = scan_display(scanlines[scan_count], 0);

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
			cp = (NSCAN - scan_count) % NSCAN;

			tile_count = scan_display(scanlines[cp], 1);

			if (tile_count >= (1 + frac2) * tile_count_old) {
				/* on a roll... do a 3rd scan */
				cp = (NSCAN - scan_count + 7) % NSCAN;
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

	hint_updates();	/* use krfb/x0rfbserver hints algorithm */

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

/* -- x11vnc.c -- */
/*
 * main routine for the x11vnc program
 */

static int defer_update_nofb = 6;	/* defer a shorter time under -nofb */

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
 * note: we do this even under -nofb
 *
 * return of 1 means watch_loop should short-circuit and reloop,
 * return of 0 means watch_loop should proceed to scan_for_updates().
 */
static int check_user_input(double dt, int *cnt) {

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

/*
 * simple function for measuring sub-second time differences, using
 * a double to hold the value.
 */
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
 * utility wrapper to call rfbProcessEvents
 */
void rfbPE(rfbScreenInfoPtr scr, long us) {
	if (! use_threads) {
		rfbProcessEvents(scr, us);
	}
}

/*
 * main x11vnc loop: polls, checks for events, iterate libvncserver, etc.
 */
static void watch_loop(void) {
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
			if (check_user_input(dt, &cnt)) {
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
 * text printed out under -help option
 */
static void print_help(void) {
	char help[] = 
"\n"
"x11vnc: allow VNC connections to real X11 displays.  %s\n"
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
"For additional info see: http://www.karlrunge.com/x11vnc/\n"
"                    and  http://www.karlrunge.com/x11vnc/#faq\n"
"\n"
"\n"
"Rudimentary config file support: if the file $HOME/.x11vncrc exists then each\n"
"line in it is treated as a single command line option.  Disable with -norc.\n"
"For each option name, the leading character \"-\" is not required.  E.g. a\n"
"line that is either \"nap\" or \"-nap\" may be used and are equivalent.\n"
"Likewise \"wait 100\" or \"-wait 100\" are acceptable lines.  The \"#\"\n"
"character comments out to the end of the line in the usual way.  Leading and\n"
"trailing whitespace is trimmed off.  Lines may be continued with a \"\\\"\n"
"as the last character of a line (it becomes a space character).\n"
"\n"
"Options:\n"
"\n"
"-display disp          X11 server display to connect to, usually :0.  The X\n"
"                       server process must be running on same machine and\n"
"                       support MIT-SHM.  Equivalent to setting the DISPLAY\n"
"                       environment variable to disp.\n"
"-id windowid           Show the window corresponding to <windowid> not the\n"
"                       entire display. Warning: bugs! new toplevels missed!...\n"
"-flashcmap             In 8bpp indexed color, let the installed colormap flash\n"
"                       as the pointer moves from window to window (slow).\n"
"-notruecolor           Force 8bpp indexed color even if it looks like TrueColor.\n"
"\n"
"-scale fraction        Scale the framebuffer by factor \"fraction\".  Values\n"
"                       less than 1 shrink the fb.  Note: image may not be sharp\n"
"                       and response may be slower.  If \"fraction\" contains\n"
"                       a decimal point \".\" it is taken as a floating point\n"
"                       number, alternatively the notation \"m/n\" may be used\n"
"                       to denote fractions, e.g. -scale 2/3.\n"
"\n"
"                       Scaling Options: can be added after fraction via \":\",\n"
"                       to supply multiple \":\" options use commas.\n"
"                       If you just want a quick, rough scaling without\n"
"                       blending, append \":nb\" to \"fraction\" (e.g. -scale\n"
"                       1/3:nb).  For compatibility with vncviewers the scaled\n"
"                       width is adjusted to be a multiple of 4, to disable\n"
"                       this use \":n4\".  More esoteric options: \":in\" use\n"
"                       interpolation scheme even when shrinking, \":pad\",\n"
"                       pad scaled width and height to be multiples of scaling\n"
"                       denominator (e.g. 3 for 2/3).\n"
"-visual n              Experimental option: probably does not do what you\n"
"                       think.  It simply *forces* the visual used for the\n"
"                       framebuffer; this may be a bad thing... It is useful for\n"
"                       testing and for some workarounds.  n may be a decimal\n"
"                       number, or 0x hex.  Run xdpyinfo(1) for the values.\n"
"                       One may also use \"TrueColor\", etc. see <X11/X.h>\n"
"                       for a list.  If the string ends in \":m\" for better\n"
"                       or for worse the visual depth is forced to be m.\n"
"\n"
"-viewonly              All clients can only watch (default %s).\n"
"-shared                VNC display is shared (default %s).\n"
"-once                  Exit after the first successfully connected viewer\n"
"                       disconnects.  This is the Default behavior.\n"
"-forever               Keep listening for more connections rather than exiting\n"
"                       as soon as the first client(s) disconnect. Same as -many\n"
"-connect string        For use with \"vncviewer -listen\" reverse connections.\n"
"                       If string has the form \"host\" or \"host:port\"\n"
"                       the connection is made once at startup.  Use commas\n"
"                       for a list of host's and host:port's.  If string\n"
"                       contains \"/\" it is instead interpreted as a file to\n"
"                       periodically check for new hosts.  The first line is\n"
"                       read and then the file is truncated.\n"
"-vncconnect            Monitor the VNC_CONNECT X property set by the standard\n"
"                       VNC program vncconnect(1).  When the property is set\n"
"                       to host or host:port establish a reverse connection.\n"
"                       Using xprop(1) instead of vncconnect may work, see FAQ.\n"
"-auth file             Set the X authority file to be \"file\", equivalent\n"
"                       to setting the XAUTHORITY environment var to \"file\"\n"
"                       before startup. See Xsecurity(7), xauth(1) man pages.\n"
"-allow addr1[,addr2..] Only allow client connections from IP addresses matching\n"
"                       the comma separated list of numerical addresses.\n"
"                       Can be a prefix, e.g. \"192.168.100.\" to match a\n"
"                       simple subnet, for more control build libvncserver\n"
"                       with libwrap support.  If the list contains a \"/\"\n"
"                       it instead is a interpreted as a file containing\n"
"                       addresses or prefixes that is re-read each time a new\n"
"                       client connects.  Lines can be commented out with the\n"
"                       \"#\" character in the usual way.\n"
"-localhost             Same as -allow 127.0.0.1\n"
"-viewpasswd string     Supply a 2nd password for view-only logins.  The -passwd\n"
"                       (full-access) password must also be supplied.\n"
"-passwdfile filename   Specify libvncserver -passwd via the first line of\n"
"                       the file \"filename\" instead of via command line.\n"
"                       If a second non blank line exists in the file it is\n"
"                       taken as a view-only password (i.e. -viewpasswd) Note:\n"
"                       this is a simple plaintext passwd, see also -rfbauth\n"
"                       and -storepasswd below.\n"
"-storepasswd pass file Store password \"pass\" as the VNC password in the\n"
"                       file \"file\".  Once the password is stored the\n"
"                       program exits.  Use the password via \"-rfbauth file\"\n"
"-accept string         Run a command (possibly to prompt the user at the X11\n"
"                       display) to decide whether an incoming client should be\n"
"                       allowed to connect or not.  \"string\" is an external\n"
"                       command run via system(3) (see below for special cases).\n"
"                       Be sure to quote \"string\" if it contains spaces,\n"
"                       etc.  If the external command returns 0 the client is\n"
"                       accepted, otherwise the client is rejected.  See below\n"
"                       for an extension to accept a client view-only.\n"
"\n"
"                       Environment: The RFB_CLIENT_IP environment variable will\n"
"                       be set to the incoming client IP number and the port\n"
"                       in RFB_CLIENT_PORT (or -1 if unavailable).  Similarly,\n"
"                       RFB_SERVER_IP and RFB_SERVER_PORT (the x11vnc side\n"
"                       of the connection), are set to allow identification\n"
"                       of the tcp virtual circuit.  The x11vnc process\n"
"                       id will be in RFB_X11VNC_PID, a client id number in\n"
"                       RFB_CLIENT_ID, and the number of other connected clients\n"
"                       in RFB_CLIENT_COUNT.\n"
"\n"
"                       If \"string\" is \"popup\" then a builtin popup window\n"
"                       is used.  The popup will time out after 120 seconds,\n"
"                       use \"popup:N\" to modify the timeout to N seconds\n"
"                       (use 0 for no timeout)\n"
"\n"
"                       If \"string\" is \"xmessage\" then an xmessage(1)\n"
"                       invocation is used for the command.\n"
"\n"
"                       Both \"popup\" and \"xmessage\" will present an option\n"
"                       for accepting the client \"View-Only\" (the client\n"
"                       can only watch).  This option will not be presented if\n"
"                       -viewonly has been specified, in which case the entire\n"
"                       display is view only.\n"
"\n"
"                       If the user supplied command is prefixed with something\n"
"                       like \"yes:0,no:*,view:3 mycommand ...\" then this\n"
"                       associates the numerical command return code with\n"
"                       the actions: accept, reject, and accept-view-only,\n"
"                       respectively.  Use \"*\" instead of a number to indicate\n"
"                       the default action (in case the command returns an\n"
"                       unexpected value).  E.g. \"no:*\" is a good choice.\n"
"\n"
"                       Note that x11vnc blocks while the external command or\n"
"                       or popup is running (other clients may see no updates\n"
"                       during this period).\n"
"\n"
"                       More -accept tricks: use \"popupmouse\" to only allow\n"
"                       mouse clicks in the builtin popup to be recognized.\n"
"                       Similarly use \"popupkey\" to only recognize keystroke\n"
"                       responses.  All 3 of the popup keywords can be followed\n"
"                       by +N+M to supply a position for the popup window.\n"
"                       The default is to center the popup window.\n"
"\n"
"-gone string           As -accept string, except to run a user supplied command\n"
"                       when a client goes away (disconnects).  Unlike -accept,\n"
"                       the command return code is not interpreted by x11vnc.\n"
"\n"
"-inetd                 Launched by inetd(1): stdio instead of listening socket.\n"
"                       Note: if you are not redirecting stderr to a log file\n"
"                       (via shell 2> or -o option) you must also specify the\n"
"                       -q option.\n"
"\n"
"-noshm                 Do not use the MIT-SHM extension for the polling.\n"
"                       Remote displays can be polled this way: be careful this\n"
"                       can use large amounts of network bandwidth.  This is\n"
"                       also of use if the local machine has a limited number\n"
"                       of shm segments and -onetile is not sufficient.\n"
"-flipbyteorder         Sometimes needed if remotely polled host has different\n"
"                       endianness.  Ignored unless -noshm is set.\n"
"-blackout string       Black out rectangles on the screen. string is a comma\n"
"                       separated list of WxH+X+Y type geometries for each rect.\n"
"-xinerama              If your screen is composed of multiple monitors\n"
"                       glued together via XINERAMA, and that screen is\n"
"                       non-rectangular this option will try to guess the areas\n"
"                       to black out (if your system has libXinerama).\n"
"\n"
"-o logfile             Write stderr messages to file \"logfile\" instead of\n"
"                       to the terminal.  Same as -logfile.\n"
"-rc filename           Use \"filename\" instead of $HOME/.x11vncrc for rc file.\n"
"-norc                  Do not process any .x11vncrc file for options.\n"
"-h, -help              Print this help text.\n"
"-V, -version           Print program version (last modification date).\n"
"\n"
"-q                     Be quiet by printing less informational output to\n"
"                       stderr.  Same as -quiet.\n"
"-bg                    Go into the background after screen setup.  Messages to\n"
"                       stderr are lost unless -o logfile is used.  Something\n"
"                       like this could be useful in a script:\n"
"                         port=`ssh $host \"x11vnc -display :0 -bg\" | grep PORT`\n"
"                         port=`echo \"$port\" | sed -e 's/PORT=//'`\n"
"                         port=`expr $port - 5900`\n"
"                         vncviewer $host:$port\n"
"\n"
"-modtweak              Handle AltGr/Shift modifiers for differing languages\n"
"                       between client and host (Default %s).  Also helps\n"
"                       resolve cases with a keysym bound to multiple keys.\n"
"-nomodtweak            Try to send the keysym directly to the X server.\n"
"                       This may cause problems if a keysym is bound to multiple\n"
"                       keys, e.g. when typing \"<\" if the Xserver defines a\n"
"                       \"< and >\" key in addition to a \"< and comma\" key.\n"
#if 0
"-isolevel3             When in modtweak mode, send ISO_Level3_Shift to the X\n"
"                       server instead of Mode_switch (AltGr).\n"
#endif
"-xkb                   Use XKEYBOARD extension (if it exists) to do the modifier\n"
"                       tweaking.\n"
"-skip_keycodes string  Skip keycodes not on your keyboard but your X server\n"
"                       thinks exist.  Currently only applies to -xkb mode.\n"
"                       \"string\" is a comma separated list of decimal\n"
"                       keycodes.  Use this option to help x11vnc in the reverse\n"
"                       problem it tries to solve: Keysym -> Keycode(s) when\n"
"                       ambiguities exist.  E.g. -skip_keycodes 94,114\n"
#if 0
"-xkbcompat             Ignore the XKEYBOARD extension.  Use as a workaround for\n"
"                       some keyboard mapping problems.  E.g. if you are using\n"
"                       an international keyboard (AltGr or ISO_Level3_Shift),\n"
"                       and the OS or keyboard where the VNC viewer is run\n"
"                       is not identical to that of the X server, and you are\n"
"                       having problems typing some keys.  Implies -nobell.\n"
#endif
"-clear_mods            At startup and exit clear the modifier keys by sending\n"
"                       KeyRelease for each one. The Lock modifiers are skipped.\n"
"                       Used to clear the state if the display was accidentally\n"
"                       left with any pressed down.\n"
"-clear_keys            As -clear_mods, except try to release any pressed key.\n"
"                       Mostly for testing.  This option and -clear_mods can\n"
"                       interfere with typing at the physical keyboard.\n"
"-remap string          Read keysym remappings from file named \"string\".\n"
"                       Format is one pair of keysyms per line (can be name\n"
"                       or hex value) separated by a space.  If no file named\n"
"                       \"string\" exists, it is instead interpreted as this\n"
"                       form: key1-key2,key3-key4,...  To map a key to a\n"
"                       button click, use the fake keysyms \"Button1\", ...,\n"
"                       etc. E.g. -remap Super_R-Button2\n"
"\n"
"-nofb                  Ignore framebuffer: only process keyboard and pointer.\n"
"                       Intended for use with Win2VNC and x2vnc dual displays.\n"
"-nobell                Do not watch for XBell events. (no beeps will be heard)\n"
"-nosel                 Do not manage exchange of X selection/cutbuffer.\n"
"-noprimary             Do not poll the PRIMARY selection for changes to send\n"
"                       back to clients.  (PRIMARY is still set on received\n"
"                       changes, however).\n"
"\n"
"-nocursor              Do not have the viewer show a local cursor.\n"
"-mouse                 Draw a 2nd cursor at the current X pointer position.\n"
"-mouseX                As -mouse, but also draw an X on root background.\n"
"-X                     Shorthand for -mouseX -nocursor.\n"
"-xwarppointer          Move the pointer with XWarpPointer() instead of XTEST\n"
"                       (try as a workaround if pointer behaves poorly, e.g.\n"
"                       on touchscreens or other non-standard setups).\n"
"-cursorpos             Send the X cursor position back to all vnc clients that\n"
"                       support the TightVNC CursorPosUpdates extension.\n"
"-buttonmap string      String to remap mouse buttons.  Format: IJK-LMN, this\n"
"                       maps buttons I -> L, etc., e.g.  -buttonmap 13-31\n"
"\n"
"                       Button presses can also be mapped to keystrokes: replace\n"
"                       a button digit on the right of the dash with :<sym>:\n"
"                       or :<sym1>+<sym2>: etc. for multiple keys. For example,\n"
"                       if the viewing machine has a mouse-wheel (buttons 4 5)\n"
"                       but the x11vnc side does not, these will do scrolls:\n"
"                              -buttonmap 12345-123:Prior::Next:\n"
"                              -buttonmap 12345-123:Up+Up+Up::Down+Down+Down:\n"
"\n"
"                       If you include a modifier like \"Shift_L\" the\n"
"                       modifier's up/down state is toggled, e.g. to send\n"
"                       \"The\" use :Shift_L+t+Shift_L+h+e: (the 1st one is\n"
"                       shift down and the 2nd one is shift up). (note: the\n"
"                       initial state of the modifier is ignored and not reset)\n"
"                       To include button events use \"Button1\", ... etc.\n"
"\n"
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
"-norepeat              Disable X server key auto repeat when clients are\n"
"                       connected.  This works around a repeating keystrokes\n"
"                       bug (triggered by long processing delays between key\n"
"                       down and key up client events: either from large screen\n"
"                       changes or high latency).  Note: your VNC viewer side\n"
"                       will likely do autorepeating, so this is no loss unless\n"
"                       someone is simultaneously at the real X display.\n"
"\n"
"-debug_pointer         Print debugging output for every pointer event.\n"
"-debug_keyboard        Print debugging output for every keyboard event.\n"
"                       Same as -dp and -dk, respectively.  Use multiple\n"
"                       times for more output.\n"
"\n"
"-defer time            Time in ms to wait for updates before sending to client\n"
"                       [rfbDeferUpdateTime]  (default %d).\n"
"-wait time             Time in ms to pause between screen polls.  Used to cut\n"
"                       down on load (default %d).\n"
"-nap                   Monitor activity and if low take longer naps between\n"
"                       polls to really cut down load when idle (default %s).\n"
"-sigpipe string        Broken pipe (SIGPIPE) handling.  \"string\" can be\n"
"                       \"ignore\" or \"exit\".  For \"ignore\" libvncserver\n"
"                       will handle the abrupt loss of a client and continue,\n"
"                       for \"exit\" x11vnc will cleanup and exit at the 1st\n"
"                       broken connection.  Default is \"ignore\".\n"
#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
"-threads               Whether or not to use the threaded libvncserver\n"
"-nothreads             algorithm [rfbRunEventLoop] (default %s).\n"
#endif
"\n"
"-fs f                  If the fraction of changed tiles in a poll is greater\n"
"                       than f, the whole screen is updated (default %.2f).\n"
"-onetile               Do not use the new copy_tiles() framebuffer mechanism,\n"
"                       just use 1 shm tile for polling.  Same as -old_copytile.\n"
"                       Limits shm segments used to 3.\n"
"-gaps n                Heuristic to fill in gaps in rows or cols of n or\n"
"                       less tiles.  Used to improve text paging (default %d).\n"
"-grow n                Heuristic to grow islands of changed tiles n or wider\n"
"                       by checking the tile near the boundary (default %d).\n"
"-fuzz n                Tolerance in pixels to mark a tiles edges as changed\n"
"                       (default %d).\n"
"%s\n"
"\n"
"These options are passed to libvncserver:\n"
"\n"
;
	/* have both our help and rfbUsage to stdout for more(1), etc. */
	dup2(1, 2);
	fprintf(stderr, help, lastmod,
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
		""
	);

	rfbUsage();
	exit(1);
}

/*
 * utility to get the current host name
 */
#define MAXN 256

static char *this_host(void) {
	char host[MAXN];
#ifdef LIBVNCSERVER_HAVE_GETHOSTNAME
	if (gethostname(host, MAXN) == 0) {
		return strdup(host);
	}
#endif
	return NULL;
}

/*
 * choose a desktop name
 */
static char *choose_title(char *display) {
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
		if (this_host() != NULL) {
			strncpy(title, this_host(), MAXN - strlen(title));
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

/* 
 * check blacklist for OSs with tight shm limits.
 */
static int limit_shm(void) {
	struct utsname ut;
	int limit = 0;

	if (uname(&ut) == -1) {
		return 0;
	}
	if (!strcmp(ut.sysname, "SunOS")) {
		char *r = ut.release;
		if (*r == '5' && *(r+1) == '.') {
			if (strchr("2345678", *(r+2)) != NULL) {
				limit = 1;
			}
		}
	}
	if (limit && ! quiet) {
		fprintf(stderr, "reducing shm usage on %s %s (adding "
		    "-onetile)\n", ut.sysname, ut.release);
	}
	return limit;
}

/*
 * quick-n-dirty ~/.x11vncrc: each line (except # comments) is a cmdline option.
 */
static int argc2 = 0;
static char **argv2;

static void check_rcfile(int argc, char **argv) {
	int i, norc = 0, argmax = 1024;
	char *infile = NULL;
	char rcfile[1024];
	FILE *rc; 

	for (i=1; i < argc; i++) {
		if (!strcmp(argv[i], "-norc")) {
			norc = 1;
		}
		if (!strcmp(argv[i], "-rc")) {
			if (i+1 >= argc) {
				fprintf(stderr, "-rc option requires a "
				    "filename\n");
				exit(1);
			} else {
				infile = argv[i+1];
			}
		}
	}
	if (norc) {
		;
	} else if (infile != NULL) {
		rc = fopen(infile, "r");
		if (rc == NULL) {
			fprintf(stderr, "could not open rcfile: %s\n", infile);
			perror("fopen");
			exit(1);
		}
	} else if (getenv("HOME") == NULL) {
		norc = 1;
	} else {
		strncpy(rcfile, getenv("HOME"), 500);
		strcat(rcfile, "/.x11vncrc");
		infile = rcfile;
		rc = fopen(rcfile, "r");
		if (rc == NULL) {
			norc = 1;
		}
	}

	argv2 = (char **) malloc(argmax * sizeof(char *));
	argv2[argc2++] = strdup(argv[0]);

	if (! norc) {
		char line[4096], parm[100], tmp[101];
		char *buf;
		struct stat sbuf;
		int sz;

		if (fstat(fileno(rc), &sbuf) != 0) {
			fprintf(stderr, "problem with %s\n", infile);
			perror("fstat");
			exit(1);
		}
		sz = sbuf.st_size+1;
		if (sz < 1024) {
			sz = 1024;
		}

		buf  = (char *) malloc(sz);

		while (fgets(line, 4096, rc) != NULL) {
			char *q, *p = line;
			char c = '\0';
			int cont = 0;

			q = p;
			while (*q) {
				if (*q == '\n') {
					if (c == '\\') {
						cont = 1;
						*q = '\0';
						*(q-1) = ' ';
						break;
					}
					while (isspace(*q)) {
						*q = '\0';
						if (q == p) {
							break;
						}
						q--;
					}
					break;
				}
				c = *q;
				q++;
			}
			if ( (q = strchr(p, '#')) != NULL) {
				*q = '\0';
			}
			while (*p) {
				if (! isspace(*p)) {
					break;
				}
				p++;
			}

			strncat(buf, p, sz - strlen(buf) - 1);
			if (cont) {
				continue;
			}
			if (buf[0] == '\0') {
				continue;
			}

			if (sscanf(buf, "%s", parm) != 1) {
				fprintf(stderr, "invalid rcfile line: %s\n", p);
				exit(1);
			}
			if (parm[0] == '-') {
				strncpy(tmp, parm, 100); 
			} else {
				tmp[0] = '-';
				strncpy(tmp+1, parm, 100); 
			}

			argv2[argc2++] = strdup(tmp);
			if (argc2 >= argmax) {
				fprintf(stderr, "too many rcfile options\n");
				exit(1);
			}
			
			p = buf;
			p += strlen(parm);
			while (*p) {
				if (! isspace(*p)) {
					break;
				}
				p++;
			}
			if (*p == '\0') {
				buf[0] = '\0';
				continue;
			}

			argv2[argc2++] = strdup(p);
			if (argc2 >= argmax) {
				fprintf(stderr, "too many rcfile options\n");
				exit(1);
			}
			buf[0] = '\0';
		}
		fclose(rc);
		free(buf);
	}
	for (i=1; i < argc; i++) {
		argv2[argc2++] = strdup(argv[i]);
		if (argc2 >= argmax) {
			fprintf(stderr, "too many rcfile options\n");
			exit(1);
		}
	}
}

int main(int argc, char* argv[]) {

	XImage *fb;
	int i, ev, er, maj, min;
	char *use_dpy = NULL;
	char *auth_file = NULL;
	char *arg, *visual_str = NULL;
	char *logfile = NULL;
	char *passwdfile = NULL;
	char *blackout_string = NULL;
	char *remap_file = NULL;
	char *pointer_remap = NULL;
	int pw_loc = -1;
	int vpw_loc = -1;
	int dt = 0;
	int bg = 0;
	int got_rfbwait = 0;
	int got_deferupdate = 0, got_defer = 0;

	/* used to pass args we do not know about to rfbGetScreen(): */
	int argc_vnc = 1; char *argv_vnc[128];

	argv_vnc[0] = strdup(argv[0]);

	check_rcfile(argc, argv);
	/* kludge for the new array argv2 set in check_rcfile() */
#	define argc argc2
#	define argv argv2

#	define CHECK_ARGC if (i >= argc-1) { \
		fprintf(stderr, "not enough arguments for: %s\n", arg); \
		exit(1); \
	}

	for (i=1; i < argc; i++) {
		/* quick-n-dirty --option handling. */
		arg = argv[i];
		if (strstr(arg, "--") == arg) {
			arg++;
		}

		if (!strcmp(arg, "-display")) {
			CHECK_ARGC
			use_dpy = argv[++i];
		} else if (!strcmp(arg, "-id")) {
			CHECK_ARGC
			if (sscanf(argv[++i], "0x%x", &subwin) != 1) {
				if (sscanf(argv[i], "%d", &subwin) != 1) {
					fprintf(stderr, "bad -id arg: %s\n",
					    argv[i]);
					exit(1);
				}
			}
		} else if (!strcmp(arg, "-scale")) {
			int m, n;
			char *p;
			double f;
			CHECK_ARGC
			if ( (p = strchr(argv[++i], ':')) != NULL) {
				/* options */
				if (strstr(p+1, "nb") != NULL) {
					scaling_noblend = 1;
				}
				if (strstr(p+1, "n4") != NULL) {
					scaling_nomult4 = 1;
				}
				if (strstr(p+1, "in") != NULL) {
					scaling_interpolate = 1;
				}
				if (strstr(p+1, "pad") != NULL) {
					scaling_pad = 1;
				}
				*p = '\0';
			}
			if (strchr(argv[i], '.') != NULL) {
				double test, diff, eps = 1.0e-7;
				if (sscanf(argv[i], "%lf", &f) != 1) {
					fprintf(stderr, "bad -scale arg: %s\n",
					    argv[i]);
					exit(1);
				}
				scale_fac = (double) f;
				/* look for common fractions from small ints: */
				for (n=2; n<=10; n++) {
					for (m=1; m<n; m++) {
						test = ((double) m)/ n;
						diff = scale_fac - test;
						if (-eps < diff && diff < eps) {
							scale_numer = m;
							scale_denom = n;
							break;
						
						}
					}
					if (scale_denom) {
						break;
					}
				}
			} else {
				if (sscanf(argv[i], "%d/%d", &m, &n) != 2) {
					fprintf(stderr, "bad -scale arg: %s\n",
					    argv[i]);
					exit(1);
				}
				scale_fac = ((double) m)/ n;
				scale_numer = m;
				scale_denom = n;
			}
			if (scale_fac == 1.0) {
				fprintf(stderr, "scaling disabled for factor "
				    "%f\n", scale_fac);
			} else {
				scaling = 1;
			}
		} else if (!strcmp(arg, "-visual")) {
			CHECK_ARGC
			visual_str = argv[++i];
		} else if (!strcmp(arg, "-flashcmap")) {
			flash_cmap = 1;
		} else if (!strcmp(arg, "-notruecolor")) {
			force_indexed_color = 1;
		} else if (!strcmp(arg, "-viewonly")) {
			view_only = 1;
		} else if (!strcmp(arg, "-viewpasswd")) {
			vpw_loc = i;
			CHECK_ARGC
			viewonly_passwd = strdup(argv[++i]);
		} else if (!strcmp(arg, "-passwdfile")) {
			CHECK_ARGC
			passwdfile = argv[++i];
		} else if (!strcmp(arg, "-storepasswd")) {
			if (i+2 >= argc || vncEncryptAndStorePasswd(argv[i+1],
			    argv[i+2]) != 0) {
				fprintf(stderr, "-storepasswd failed\n");
				exit(1);
			} else {
				fprintf(stderr, "stored passwd in file %s\n",
				    argv[i+2]);
				exit(0);
			}
		} else if (!strcmp(arg, "-shared")) {
			shared = 1;
		} else if (!strcmp(arg, "-auth")) {
			CHECK_ARGC
			auth_file = argv[++i];
		} else if (!strcmp(arg, "-allow")) {
			CHECK_ARGC
			allow_list = argv[++i];
		} else if (!strcmp(arg, "-localhost")) {
			allow_list = "127.0.0.1";
		} else if (!strcmp(arg, "-accept")) {
			CHECK_ARGC
			accept_cmd = argv[++i];
		} else if (!strcmp(arg, "-gone")) {
			CHECK_ARGC
			gone_cmd = argv[++i];
		} else if (!strcmp(arg, "-once")) {
			connect_once = 1;
		} else if (!strcmp(arg, "-many")
			|| !strcmp(arg, "-forever")) {
			connect_once = 0;
		} else if (!strcmp(arg, "-connect")) {
			CHECK_ARGC
			if (strchr(argv[++i], '/')) {
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
		} else if (!strcmp(arg, "-isolevel3")) {
			use_iso_level3 = 1;
		} else if (!strcmp(arg, "-xkb")) {
			use_xkb_modtweak = 1;
		} else if (!strcmp(arg, "-skip_keycodes")) {
			CHECK_ARGC
			skip_keycodes = argv[++i];
		} else if (!strcmp(arg, "-xkbcompat")) {
			xkbcompat = 1;
		} else if (!strcmp(arg, "-clear_mods")) {
			clear_mods = 1;
		} else if (!strcmp(arg, "-clear_keys")) {
			clear_mods = 2;
		} else if (!strcmp(arg, "-remap")) {
			CHECK_ARGC
			remap_file = argv[++i];
		} else if (!strcmp(arg, "-blackout")) {
			CHECK_ARGC
			blackout_string = argv[++i];
		} else if (!strcmp(arg, "-xinerama")) {
			xinerama = 1;
		} else if (!strcmp(arg, "-norc")) {
			;	/* done above */
		} else if (!strcmp(arg, "-rc")) {
			i++;	/* done above */
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
			CHECK_ARGC
			pointer_remap = argv[++i];
		} else if (!strcmp(arg, "-nodragging")) {
			show_dragging = 0;
		} else if (!strcmp(arg, "-input_skip")) {
			CHECK_ARGC
			ui_skip = atoi(argv[++i]);
			if (! ui_skip) ui_skip = 1;
		} else if (!strcmp(arg, "-old_pointer")) {
			old_pointer = 1;
		} else if (!strcmp(arg, "-norepeat")) {
			no_autorepeat = 1;
		} else if (!strcmp(arg, "-onetile")
			|| !strcmp(arg, "-old_copytile")) {
			single_copytile = 1;
		} else if (!strcmp(arg, "-debug_pointer")) {
		} else if (!strcmp(arg, "-debug_pointer")
		    || !strcmp(arg, "-dp")) {
			debug_pointer++;
		} else if (!strcmp(arg, "-debug_keyboard")
		    || !strcmp(arg, "-dk")) {
			debug_keyboard++;
		} else if (!strcmp(arg, "-defer")) {
			CHECK_ARGC
			defer_update = atoi(argv[++i]);
			got_defer = 1;
		} else if (!strcmp(arg, "-wait")) {
			CHECK_ARGC
			waitms = atoi(argv[++i]);
		} else if (!strcmp(arg, "-nap")) {
			take_naps = 1;
#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
		} else if (!strcmp(arg, "-threads")) {
			use_threads = 1;
		} else if (!strcmp(arg, "-nothreads")) {
			use_threads = 0;
#endif
		} else if (!strcmp(arg, "-sigpipe")) {
			CHECK_ARGC
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
			CHECK_ARGC
			fs_frac = atof(argv[++i]);
		} else if (!strcmp(arg, "-gaps")) {
			CHECK_ARGC
			gaps_fill = atoi(argv[++i]);
		} else if (!strcmp(arg, "-grow")) {
			CHECK_ARGC
			grow_fill = atoi(argv[++i]);
		} else if (!strcmp(arg, "-fuzz")) {
			CHECK_ARGC
			tile_fuzz = atoi(argv[++i]);
		} else if (!strcmp(arg, "-hints") || !strcmp(arg, "-nohints")) {
			fprintf(stderr, "warning: -hints/-nohints option "
			    "has been removed.\n");
		} else if (!strcmp(arg, "-h") || !strcmp(arg, "-help")
			|| !strcmp(arg, "-?")) {
			print_help();
		} else if (!strcmp(arg, "-V") || !strcmp(arg, "-version")) {
			fprintf(stderr, "x11vnc: %s\n", lastmod);
			exit(0);
		} else if (!strcmp(arg, "-o") || !strcmp(arg, "-logfile")) {
			CHECK_ARGC
			logfile = argv[++i];
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
			/* otherwise copy it for libvncserver use below. */
			if (argc_vnc < 100) {
				argv_vnc[argc_vnc++] = strdup(arg);
			}
		}
	}
	if (logfile) {
		int n;
		if ((n = open(logfile, O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0) {
			fprintf(stderr, "error opening logfile: %s\n", logfile);
			perror("open");
			exit(1);
		}
		if (dup2(n, 2) < 0) {
			fprintf(stderr, "dup2 failed\n");
			perror("dup2");
			exit(1);
		}
		if (n > 2) {
			close(n);
		}
	}
	if (! quiet && ! inetd) {
		int i;
		for (i=1; i < argc_vnc; i++) {
			fprintf(stderr, "passing arg to libvncserver: %s\n",
			    argv_vnc[i]);
			if (!strcmp(argv_vnc[i], "-passwd")) {
				i++;
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
	} else if (passwdfile) {
		/* read passwd from file */
		char line[1024];
		FILE *in;
		in = fopen(passwdfile, "r");
		if (in == NULL) {
			fprintf(stderr, "cannot open passwdfile: %s\n",
			    passwdfile);
			perror("fopen");
			exit(1);
		}
		if (fgets(line, 1024, in) != NULL) {
			int len = strlen(line); 
			if (len > 0 && line[len-1] == '\n') {
				line[len-1] = '\0';
			}
			argv_vnc[argc_vnc++] = "-passwd";
			argv_vnc[argc_vnc++] = strdup(line);
			pw_loc = 100;	/* just for pw_loc check below */
			if (fgets(line, 1024, in) != NULL) {
				/* try to read viewonly passwd from file */
				int ok = 0;
				len = strlen(line); 
				if (len > 0 && line[len-1] == '\n') {
					line[len-1] = '\0';
				}
				if (strlen(line) > 0) {
					char *p = line;
					/* check for non-blank line */
					while (*p != '\0') {
						if (! isspace(*p)) {
							ok = 1;
						}
						p++;
					}
				}
				if (ok) {
					viewonly_passwd = strdup(line);
				} else {
					fprintf(stderr, "*** not setting"
					    " viewonly password to the 2nd"
					    " line of %s. (blank or other"
					    " problem)\n", passwdfile);
				}
			}
		} else {
			fprintf(stderr, "cannot read a line from "
			    "passwdfile: %s\n", passwdfile);
			exit(1);
		}
		fclose(in);
	}
	if (vpw_loc > 0) {
		char *p = argv[vpw_loc];		
		while (*p != '\0') {
			*p++ = '\0';
		}
		if (vpw_loc+1 < argc) {
			p = argv[vpw_loc+1];		
			while (*p != '\0') {
				*p++ = '\0';
			}
		}
	} 
	if (viewonly_passwd && pw_loc < 0) {
		fprintf(stderr, "-passwd must be supplied when using "
		    "-viewpasswd\n");
		exit(1);
	}

	/* fixup settings that do not make sense */
		
	if (use_threads && nofb && cursor_pos) {
		if (! quiet) {
			fprintf(stderr, "disabling -threads under -nofb "
			    "-cursorpos\n");
		}
		use_threads = 0;
	}
	if (tile_fuzz < 1) {
		tile_fuzz = 1;
	}
	if (waitms < 0) {
		waitms = 0;
	}
	if (inetd) {
		shared = 0;
		connect_once = 1;
		bg = 0;
	}

	/* increase rfbwait if threaded */
	if (use_threads && ! got_rfbwait) {
		argv_vnc[argc_vnc++] = "-rfbwait";
		argv_vnc[argc_vnc++] = "604800000"; /* one week... */
	}

	/* check for OS with small shm limits */
	if (using_shm && ! single_copytile) {
		if (limit_shm()) {
			single_copytile = 1;
		}
	}

	if (nofb && ! got_deferupdate && ! got_defer) {
		/* reduce defer time under -nofb */
		defer_update = defer_update_nofb;
	}
	if (! got_deferupdate) {
		char tmp[40];
		/* XXX not working yet in libvncserver */
		sprintf(tmp, "%d", defer_update);
		argv_vnc[argc_vnc++] = "-deferupdate";
		argv_vnc[argc_vnc++] = strdup(tmp);
	}
	if (debug_pointer || debug_keyboard) {
		if (bg || quiet) {
			fprintf(stderr, "disabling -bg/-q under -debug_pointer"
			    "/-debug_keyboard\n");
			bg = 0;
			quiet = 0;
		}
	}

	if (! quiet) {
		fprintf(stderr, "\n");
		fprintf(stderr, "display:    %s\n", use_dpy ? use_dpy
                    : "null");
		fprintf(stderr, "subwin:     0x%x\n", subwin);
		fprintf(stderr, "flashcmap:  %d\n", flash_cmap);
		fprintf(stderr, "force_idx:  %d\n", force_indexed_color);
		fprintf(stderr, "scaling:    %d %.5f\n", scaling, scale_fac);
		fprintf(stderr, "visual:     %s\n", visual_str ? visual_str
                    : "null");
		fprintf(stderr, "viewonly:   %d\n", view_only);
		fprintf(stderr, "shared:     %d\n", shared);
		fprintf(stderr, "conn_once:  %d\n", connect_once);
		fprintf(stderr, "connect:    %s\n", client_connect
		    ? client_connect : "null");
		fprintf(stderr, "connectfile %s\n", client_connect_file
		    ? client_connect_file : "null");
		fprintf(stderr, "vnc_conn:   %d\n", vnc_connect);
		fprintf(stderr, "authfile:   %s\n", auth_file ? auth_file
                    : "null");
		fprintf(stderr, "allow:      %s\n", allow_list ? allow_list
                    : "null");
		fprintf(stderr, "passfile:   %s\n", passwdfile ? passwdfile
                    : "null");
		fprintf(stderr, "accept:     %s\n", accept_cmd ? accept_cmd
                    : "null");
		fprintf(stderr, "gone:       %s\n", gone_cmd ? gone_cmd
                    : "null");
		fprintf(stderr, "inetd:      %d\n", inetd);
		fprintf(stderr, "using_shm:  %d\n", using_shm);
		fprintf(stderr, "flipbytes:  %d\n", flip_byte_order);
		fprintf(stderr, "blackout:   %s\n", blackout_string
		    ? blackout_string : "null");
		fprintf(stderr, "xinerama:   %d\n", xinerama);
		fprintf(stderr, "logfile:    %s\n", logfile ? logfile
                    : "null");
		fprintf(stderr, "bg:         %d\n", bg);
		fprintf(stderr, "mod_tweak:  %d\n", use_modifier_tweak);
		fprintf(stderr, "isolevel3:  %d\n", use_iso_level3);
		fprintf(stderr, "xkb:        %d\n", use_xkb_modtweak);
		fprintf(stderr, "skipkeys:   %s\n", skip_keycodes ? skip_keycodes
                    : "null");
		fprintf(stderr, "xkbcompat:  %d\n", xkbcompat);
		fprintf(stderr, "clearmods:  %d\n", clear_mods);
		fprintf(stderr, "remap:      %s\n", remap_file ? remap_file
                    : "null");
		fprintf(stderr, "nofb:       %d\n", nofb);
		fprintf(stderr, "watchbell:  %d\n", watch_bell);
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
		fprintf(stderr, "old_ptr:    %d\n", old_pointer);
		fprintf(stderr, "inputskip:  %d\n", ui_skip);
		fprintf(stderr, "norepeat:   %d\n", no_autorepeat);
		fprintf(stderr, "debug_ptr:  %d\n", debug_pointer);
		fprintf(stderr, "debug_key:  %d\n", debug_keyboard);
		fprintf(stderr, "defer:      %d\n", defer_update);
		fprintf(stderr, "waitms:     %d\n", waitms);
		fprintf(stderr, "take_naps:  %d\n", take_naps);
		fprintf(stderr, "sigpipe:    %d\n", sigpipe);
		fprintf(stderr, "threads:    %d\n", use_threads);
		fprintf(stderr, "fs_frac:    %.2f\n", fs_frac);
		fprintf(stderr, "onetile:    %d\n", single_copytile);
		fprintf(stderr, "gaps_fill:  %d\n", gaps_fill);
		fprintf(stderr, "grow_fill:  %d\n", grow_fill);
		fprintf(stderr, "tile_fuzz:  %d\n", tile_fuzz);
		fprintf(stderr, "%s\n", lastmod);
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
	if (watch_bell || use_xkb_modtweak) {
		/* we need XKEYBOARD for these: */
		use_xkb = 1;
	}
	if (xkbcompat) {
		use_xkb = 0;
	}
#ifdef LIBVNCSERVER_HAVE_XKEYBOARD
	/*
	 * Disable XKEYBOARD before calling XOpenDisplay()
	 * this should be used if there is ambiguity in the keymapping. 
	 */
	if (xkbcompat) {
		Bool rc = XkbIgnoreExtension(True);
		if (! quiet) {
			fprintf(stderr, "disabling xkb extension. rc=%d\n", rc);
			if (watch_bell) {
				watch_bell = 0;
				fprintf(stderr, "disabling bell.\n");
			}
		}
	}
#else
	use_xkb = 0;
#endif
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
	if (! dt) {
		static char str[] = "-desktop";
		argv_vnc[argc_vnc++] = str;
		argv_vnc[argc_vnc++] = choose_title(use_dpy);
	}
	scr = DefaultScreen(dpy);
	rootwin = RootWindow(dpy, scr);


	/* check for XTEST */
	if (! XTestQueryExtension(dpy, &ev, &er, &maj, &min)) {
		fprintf(stderr, "Display does not support XTest extension.\n");
		exit(1);
	}
	/*
	 * Window managers will often grab the display during resize, etc.
	 * To avoid deadlock (our user resize input is not processed)
	 * we tell the server to process our requests during all grabs:
	 */
	XTestGrabControl(dpy, True);

	/* check for MIT-SHM */
	if (! nofb && ! XShmQueryExtension(dpy)) {
		if (! using_shm) {
			if (! quiet) {
				fprintf(stderr, "info: display does not "
				    "support XShm.\n");
			}
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
	if (use_xkb) {
		initialize_xkb();
	}
	initialize_watch_bell();
	if (!use_xkb && use_xkb_modtweak) {
		fprintf(stderr, "warning: disabling xkb modtweak."
		    " XKEYBOARD ext. not present.\n");
		use_xkb_modtweak = 0;
	}
#endif

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
			fprintf(stderr, "bad window: 0x%lx\n", window);
			exit(1);
		}
		dpy_x = attr.width;
		dpy_y = attr.height;
		visual = attr.visual;

		/* show_mouse has some segv crashes as well */
		if (show_root_cursor) {
			show_root_cursor = 0;
			if (! quiet) {
				fprintf(stderr, "disabling root cursor drawing"
				    " for subwindow\n");
			}
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
			    (int) visual_id);
			exit(1);
		}
		visual = vinfo->visual;
		depth = vinfo->depth;
		if (visual_depth) {
			depth = visual_depth;	/* force it */
		}
		if (! quiet) {
			fprintf(stderr, "vis id:     0x%x\n",
			    (int) vinfo->visualid);
			fprintf(stderr, "vis scr:      %d\n", vinfo->screen);
			fprintf(stderr, "vis depth     %d\n", vinfo->depth);
			fprintf(stderr, "vis class     %d\n", vinfo->class);
			fprintf(stderr, "vis rmask   0x%lx\n", vinfo->red_mask);
			fprintf(stderr, "vis gmask   0x%lx\n", vinfo->green_mask);
			fprintf(stderr, "vis bmask   0x%lx\n", vinfo->blue_mask);
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

	/*
	 * n.b. we do not have to X_LOCK any X11 calls until watch_loop()
	 * is called since we are single-threaded until then.
	 */

	initialize_screen(&argc_vnc, argv_vnc, fb);

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

	initialize_signals();


	if (blackouts) {	/* blackout fb as needed. */
		copy_screen();
	}

	if (use_modifier_tweak) {
		initialize_modtweak();
	}
	if (remap_file != NULL) {
		initialize_remap(remap_file);
	}
	initialize_pointer_map(pointer_remap);

	clear_modifiers(1);
	if (clear_mods == 1) {
		clear_modifiers(0);
	}

	if (! inetd) {
		if (! screen->rfbPort || screen->rfbListenSock < 0) {
			rfbLog("Error: could not obtain listening port.\n");
			clean_up_exit(1);
		}
	}
	if (! quiet) {
		rfbLog("screen setup finished.\n");
	}
	if (screen->rfbPort) {
		char *host = this_host();
		int port = screen->rfbPort;
		if (host != NULL) {
			/* note that vncviewer special cases 5900-5999 */
			if (inetd) {
				;	/* should not occur */
			} else if (quiet) {
				if (port >= 5900) {
					fprintf(stderr, "The VNC desktop is "
					    "%s:%d\n", host, port - 5900);
				} else {
					fprintf(stderr, "The VNC desktop is "
					    "%s:%d\n", host, port);
				}
			} else if (port >= 5900) {
				rfbLog("The VNC desktop is %s:%d\n", host,
				    port - 5900);
				if (port >= 6000) {
					rfbLog("possible aliases:  %s:%d, "
					    "%s::%d\n", host, port, host, port);
				}
			} else {
				rfbLog("The VNC desktop is %s:%d\n", host,
				    port);
				rfbLog("possible alias:    %s::%d\n",
				    host, port);
			}
		}
		fflush(stderr);	
		fprintf(stdout, "PORT=%d\n", screen->rfbPort);
		fflush(stdout);	
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
		if (! logfile) {
			dup2(n, 2);
		}
		if (n > 2) {
			close(n);
		}
	}
#endif

	watch_loop();

	return(0);

#undef argc
#undef argv
}

