/*
 * x11vnc.c: a VNC server for X displays.
 *
 * Copyright (c) 2002-2005 Karl J. Runge <runge@karlrunge.com>
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
 *	 x0rfbserver, the original native X vnc server (Jens Wagner)
 *       krfb, the KDE desktopsharing project (Tim Jansen)
 *
 * The primary goal of this program is to create a portable and simple
 * command-line server utility that allows a VNC viewer to connect
 * to an actual X display (as the above do).  The only non-standard
 * dependency of this program is the static library libvncserver.a.
 * Although in some environments libjpeg.so or libz.so may not be
 * readily available and needs to be installed, they may be found
 * at ftp://ftp.uu.net/graphics/jpeg/ and http://www.gzip.org/zlib/,
 * respectively.  To increase portability it is written in plain C.
 *
 * Another goal is to improve performance and interactive response.
 * The algorithm of x0rfbserver was used as a base.  Additional heuristics
 * are also applied.
 *
 * Another goal is to add many features that enable and incourage creative
 * usage and application of the tool.  Apologies for the large number
 * of options!
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
 * threaded mode is a bit more responsive, but can be unstable (e.g.
 * if more than one client the same tight or zrle encoding).
 *
 * Known shortcomings:
 *
 * The screen updates are good, but of course not perfect since the X
 * display must be continuously polled and read for changes and this is
 * slow for most hardware. This can be contrasted with receiving a change
 * callback from the X server, if that were generally possible... (UPDATE:
 * this is handled now with the X DAMAGE extension, but unfortunately
 * that doesn't seem to address the slow read from the video h/w).  So,
 * e.g., opaque moves and similar window activity can be very painful;
 * one has to modify one's behavior a bit.
 *
 * General audio at the remote display is lost unless one separately
 * sets up some audio side-channel such as esd.
 *
 * It does not appear possible to query the X server for the current
 * cursor shape.  We can use XTest to compare cursor to current window's
 * cursor, but we cannot extract what the cursor is... (UPDATE: we now
 * use XFIXES extension for this.  Also on Solaris and IRIX Overlay
 * extensions exists that allow drawing the mouse into the framebuffer)
 * 
 * The current *position* of the remote X mouse pointer is shown with
 * the -cursor option.  Further, if -cursorX or -X is used, a trick
 * is done to at least show the root window cursor vs non-root cursor.
 * (perhaps some heuristic can be done to further distinguish cases...,
 * currently "-cursor some" is a first hack at this)
 *
 * Under XFIXES mode for showing the cursor shape, the cursor may be
 * poorly approximated if it has transparency (alpha channel).
 *
 * Windows using visuals other than the default X visual may have
 * their colors messed up.  When using 8bpp indexed color, the colormap
 * is attempted to be followed, but may become out of date.  Use the
 * -flashcmap option to have colormap flashing as the pointer moves
 * windows with private colormaps (slow).  Displays with mixed depth 8 and
 * 24 visuals will incorrectly display windows using the non-default one.
 * On Sun and Sgi hardware we can to work around this with -overlay.
 *
 * Feature -id <windowid> can be picky: it can crash for things like
 * the window not sufficiently mapped into server memory, etc (UPDATE:
 * we now use the -xrandr mechanisms to trap errors more robustly for
 * this mode).  SaveUnders menus, popups, etc will not be seen.
 *
 * Under some situations the keysym unmapping is not correct, especially
 * if the two keyboards correspond to different languages.  The -modtweak
 * option is the default and corrects most problems. One can use the
 * -xkb option to try to use the XKEYBOARD extension to clear up any
 * remaining problems.
 *
 * Occasionally, a few tile updates can be missed leaving a patch of
 * color that needs to be refreshed.  This may only be when threaded,
 * which is no longer the default.
 *
 * There seems to be a serious bug with simultaneous clients when
 * threaded, currently the only workaround in this case is -nothreads
 * (which is now the default).
 *
 */

/* 
 * These ' -- filename.[ch] -- ' comments represent a partial cleanup:
 * they are an odd way to indicate how this huge file would be split up
 * someday into multiple files.  Not finished, externs and other things
 * would need to be done, but it indicates a breakup, including static
 * keyword for some items.
 *
 * The primary reason we do not break up this file is for user
 * convenience: those wanting to use the latest version download a single
 * file, x11vnc.c, and off they go...
 */

/* -- x11vnc.h -- */

/****************************************************************************/

/* Standard includes and libvncserver */

#include <unistd.h>
#include <signal.h>
#include <sys/utsname.h>
#include <errno.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <X11/keysym.h>
#include <X11/Xatom.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

#include <rfb/rfb.h>
#include <rfb/rfbregion.h>

/****************************************************************************/

/* Build-time customization via CPPFLAGS. */

/*
 * Summary of options to include in CPPFLAGS for custom builds:
 *
 * -DSHARED  to have the vnc display shared by default.
 * -DFOREVER  to have -forever on by default.
 * -DNOREPEAT=0  to have -repeat on by default.
 * -DADDKEYSYMS=0  to have -noadd_keysyms the default.
 *
 * -DREMOTE_DEFAULT=0  to disable remote-control on by default (-yesremote).
 * -DREMOTE_CONTROL=0  to disable remote-control mechanism completely.
 * -DEXTERNAL_COMMANDS=0  to disable the running of all external commands.
 *
 * -DHARDWIRE_PASSWD=...      hardwired passwords, quoting necessary.
 * -DHARDWIRE_VIEWPASSWD=...
 *
 * -DWIREFRAME=0  to have -nowireframe as the default.
 * -DWIREFRAME_COPYRECT=0  to have -nowirecopyrect as the default.
 * -DWIREFRAME_PARMS=...   set default -wirecopyrect parameters.
 * -DSCROLL_COPYRECT=0     to have -noscrollcopyrect as the default.
 * -DSCROLL_COPYRECT_PARMS=...  set default -scrollcopyrect parameters.
 * -DXDAMAGE=0    to have -noxdamage as the default.
 * -DSKIDPUPS=0   to have -noskip_dups as the default.
 *
 * -DPOINTER_MODE_DEFAULT={0,1,2,3,4}  set default -pointer_mode.
 * -DBOLDLY_CLOSE_DISPLAY=0  to not close X DISPLAY under -rawfb.
 * -DSMALL_FOOTPRINT=1  for smaller binary size (no help, no gui, etc) 
 *                      use 2 or 3 for even smaller footprint.
 *
 * Set these in CPPFLAGS before running configure. E.g.:
 *
 *   % env CPPFLAGS="-DFOREVER -DREMOTE_CONTROL=0" ./configure
 *   % make
 */

/*
 * This can be used to disable the remote control mechanism.
 */
#ifndef REMOTE_CONTROL
#define REMOTE_CONTROL 1
#endif

/*
 * Beginning of support for small binary footprint build for embedded
 * systems, PDA's etc.  It currently just cuts out the low-hanging
 * fruit (large text passages).  Set to 2, 3 to cut out some of the
 * more esoteric extensions.  More tedious is to modify LDFLAGS in the
 * Makefile to not link against the extension libraries... but that
 * should be done too (manually for now).
 *
 * If there is interest more of the bloat can be removed...  Currently
 * these shrink the binary from 500K to about 270K.
 */
#ifndef SMALL_FOOTPRINT
#define SMALL_FOOTPRINT 0
#endif

#if (SMALL_FOOTPRINT > 1)
#define LIBVNCSERVER_HAVE_XKEYBOARD 0
#define LIBVNCSERVER_HAVE_LIBXINERAMA 0
#define LIBVNCSERVER_HAVE_LIBXRANDR 0
#define LIBVNCSERVER_HAVE_LIBXFIXES 0
#define LIBVNCSERVER_HAVE_LIBXDAMAGE 0
#endif

#if (SMALL_FOOTPRINT > 2)
#define LIBVNCSERVER_HAVE_UTMPX_H 0
#define LIBVNCSERVER_HAVE_PWD_H 0
#define REMOTE_CONTROL 0
#endif

/*
 * Not recommended unless you know what you are getting into, but if you
 * define the HARDWIRE_PASSWD or HARDWIRE_VIEWPASSWD variables here or in
 * CPPFLAGS you can set a default -passwd and -viewpasswd string values,
 * perhaps this would be better than nothing on an embedded system, etc.
 * These default values will be overridden by the command line.
 * We don't even give an example ;-)
 */

/****************************************************************************/


/* Extensions and related includes: */

#if LIBVNCSERVER_HAVE_XSHM
#  if defined(__hpux) && defined(__ia64)  /* something weird on hp/itanic */
#    undef _INCLUDE_HPUX_SOURCE
#  endif
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif

#if LIBVNCSERVER_HAVE_XTEST
#include <X11/extensions/XTest.h>
#endif
int xtest_base_event_type = 0;

#if LIBVNCSERVER_HAVE_LIBXTRAP
#define NEED_EVENTS
#define NEED_REPLIES
#include <X11/extensions/xtraplib.h>
#include <X11/extensions/xtraplibp.h>
XETC *trap_ctx = NULL;
#endif
int xtrap_base_event_type = 0;

#if LIBVNCSERVER_HAVE_RECORD
#include <X11/Xproto.h>
#include <X11/extensions/record.h>
#endif

#if LIBVNCSERVER_HAVE_XKEYBOARD
#include <X11/XKBlib.h>
#endif

#if LIBVNCSERVER_HAVE_LIBXINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#if LIBVNCSERVER_HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include <netdb.h>
extern int h_errno;

#if LIBVNCSERVER_HAVE_NETINET_IN_H
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#endif

/* XXX autoconf */
#if LIBVNCSERVER_HAVE_PWD_H
#include <pwd.h>
#endif
#if LIBVNCSERVER_HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#if LIBVNCSERVER_HAVE_UTMPX_H
#include <utmpx.h>
#endif

#if LIBVNCSERVER_HAVE_MMAP
#include <sys/mman.h>
#endif

/*
 * overlay/multi-depth screen reading support
 * undef SOLARIS_OVERLAY or IRIX_OVERLAY if there are problems building.
 */

/* solaris/sun */
#if defined (__SVR4) && defined (__sun)
#define SOLARIS
#define SOLARIS_OVERLAY
#define OVERLAY_OS
#endif
#ifdef SOLARIS_OVERLAY
#include <X11/extensions/transovl.h>
#endif

/* irix/sgi */
#if defined(__sgi)
#define IRIX
#define IRIX_OVERLAY
#define OVERLAY_OS
#endif
#ifdef IRIX_OVERLAY
#include <X11/extensions/readdisplay.h>
#endif

int overlay_present = 0;

/* 
 * Ditto for librandr.
 * (e.g. LDFLAGS=-lXrandr before configure).
#define LIBVNCSERVER_HAVE_LIBXRANDR 1
 */
#if LIBVNCSERVER_HAVE_LIBXRANDR
#include <X11/extensions/Xrandr.h>
#endif
int xrandr_base_event_type = 0;


int xfixes_present = 0;
int use_xfixes = 1;
int got_xfixes_cursor_notify = 0;
int cursor_changes = 0;
int alpha_threshold = 240;
double alpha_frac = 0.33;
int alpha_remove = 0;
int alpha_blend = 1;
int alt_arrow = 1;

#if LIBVNCSERVER_HAVE_LIBXFIXES
#include <X11/extensions/Xfixes.h>
#endif
int xfixes_base_event_type = 0;


#ifndef XDAMAGE
#define XDAMAGE 1
#endif
int use_xdamage = XDAMAGE;	/* use the xdamage rects for scanline hints */
int xdamage_present = 0;
#if LIBVNCSERVER_HAVE_LIBXDAMAGE
#include <X11/extensions/Xdamage.h>
Damage xdamage = 0;
#endif
int xdamage_base_event_type = 0;
int xdamage_max_area = 20000;	/* pixels */
double xdamage_memory = 1.0;	/* in units of NSCAN */
int xdamage_tile_count, xdamage_direct_count;
double xdamage_scheduled_mark = 0.0;
sraRegionPtr xdamage_scheduled_mark_region = NULL;

/*               date +'lastmod: %Y-%m-%d' */
char lastmod[] = "0.7.2 lastmod: 2005-05-24";
int hack_val = 0;

/* X display info */

Display *dpy = NULL;		/* the single display screen we connect to */
int scr;
Window window, rootwin;		/* polled window, root window (usu. same) */
Visual *default_visual;		/* the default visual (unless -visual) */
int bpp, depth;
int indexed_color = 0;
int dpy_x, dpy_y;		/* size of display */
int off_x, off_y;		/* offsets for -sid */
int wdpy_x, wdpy_y;		/* for actual sizes in case of -clip */
int cdpy_x, cdpy_y, coff_x, coff_y;	/* the -clip params */
int button_mask = 0;		/* button state and info */
int button_mask_prev = 0;
int num_buttons = -1;

/* image structures */
XImage *scanline;
XImage *fullscreen;
XImage **tile_row;		/* for all possible row runs */
XImage *fb0;
XImage *snaprect = NULL;	/* for XShmGetImage (fs_factor) */
XImage *snap = NULL;		/* the full snap fb */
XImage *raw_fb_image = NULL;	/* the raw fb */

#if !LIBVNCSERVER_HAVE_XSHM
/*
 * for simplicity, define this struct since we'll never use them
 * under using_shm = 0.
 */
typedef struct {
	int shmid; char *shmaddr; Bool readOnly;
} XShmSegmentInfo;
#endif

/* corresponding shm structures */
XShmSegmentInfo scanline_shm;
XShmSegmentInfo fullscreen_shm;
XShmSegmentInfo *tile_row_shm;	/* for all possible row runs */
XShmSegmentInfo snaprect_shm;

/* rfb screen info */
rfbScreenInfoPtr screen = NULL;
char *rfb_desktop_name = NULL;
char *http_dir = NULL;
char vnc_desktop_name[256];
char *main_fb;			/* our copy of the X11 fb */
char *rfb_fb;			/* same as main_fb unless transformation */
char *fake_fb = NULL;		/* used under -padgeom */
char *snap_fb = NULL;		/* used under -snapfb */
char *raw_fb = NULL;
char *raw_fb_addr = NULL;
int raw_fb_offset = 0;
int raw_fb_shm = 0;
int raw_fb_mmap = 0;
int raw_fb_seek = 0;
int raw_fb_fd = -1;

int rfb_bytes_per_line;
int main_bytes_per_line;
unsigned long  main_red_mask,  main_green_mask,  main_blue_mask;
unsigned short main_red_max,   main_green_max,   main_blue_max;
unsigned short main_red_shift, main_green_shift, main_blue_shift;

/* struct with client specific data: */
#define CILEN 10
typedef struct _ClientData {
	int uid;
	char *hostname;
	char *username;
	int client_port;
	int server_port;
	char *server_ip;
	char input[CILEN];
	int login_viewonly;

	int had_cursor_shape_updates;
	int had_cursor_pos_updates;

	double timer;
	double send_cmp_rate;
	double send_raw_rate;
	double latency;
	int cmp_bytes_sent;
	int raw_bytes_sent;
} ClientData;

/* scaling parameters */
char *scale_str = NULL;
double scale_fac = 1.0;
int scaling = 0;
int scaling_blend = 1;		/* for no blending option (very course) */
int scaling_nomult4 = 0;	/* do not require width = n * 4 */
int scaling_pad = 0;		/* pad out scaled sizes to fit denominator */
int scaling_interpolate = 0;	/* use interpolation scheme when shrinking */
int scaled_x = 0, scaled_y = 0;	/* dimensions of scaled display */
int scale_numer = 0, scale_denom = 0;	/* n/m */

/* scale cursor */
char *scale_cursor_str = NULL;
double scale_cursor_fac = 1.0;
int scaling_cursor = 0;
int scaling_cursor_blend = 1;
int scaling_cursor_interpolate = 0;
int scale_cursor_numer = 0, scale_cursor_denom = 0;

/* size of the basic tile unit that is polled for changes: */
int tile_x = 32;
int tile_y = 32;
int ntiles, ntiles_x, ntiles_y;

/* arrays that indicate changed or checked tiles. */
unsigned char *tile_has_diff, *tile_tried, *tile_copied;
unsigned char *tile_has_xdamage_diff, *tile_row_has_xdamage_diff;

/* times of recent events */
time_t last_event, last_input = 0, last_client = 0;
time_t last_keyboard_input = 0, last_pointer_input = 0; 
double last_keyboard_time = 0.0;
double last_pointer_time = 0.0;
double last_pointer_click_time = 0.0;
double last_pointer_motion_time = 0.0;
double last_key_to_button_remap_time = 0.0;
double servertime_diff = 0.0;
double x11vnc_start = 0.0;

/* last client to move pointer */
rfbClientPtr last_pointer_client = NULL;

int accepted_client = 0;
int client_count = 0;
int clients_served = 0;

/* more transient kludge variables: */
int cursor_x, cursor_y;		/* x and y from the viewer(s) */
int button_change_x, button_change_y;
int got_user_input = 0;
int got_pointer_input = 0;
int got_keyboard_input = 0;
int urgent_update = 0;
int last_keyboard_keycode = 0;
rfbBool last_rfb_down = FALSE;
rfbBool last_rfb_key_accepted = FALSE;
rfbKeySym last_rfb_keysym = 0;
double last_rfb_keytime = 0.0;
int fb_copy_in_progress = 0;	
int drag_in_progress = 0;	
int shut_down = 0;	
int do_copy_screen = 0;	
time_t damage_time = 0;
int damage_delay = 0;

int program_pid = 0;
char *program_name = NULL;
char *program_cmdline = NULL;

/* string for the VNC_CONNECT property */
#define VNC_CONNECT_MAX 16384
char vnc_connect_str[VNC_CONNECT_MAX+1];
Atom vnc_connect_prop = None;

struct utsname UT;

/* scan pattern jitter from x0rfbserver */
#define NSCAN 32
int scanlines[NSCAN] = {
	 0, 16,  8, 24,  4, 20, 12, 28,
	10, 26, 18,  2, 22,  6, 30, 14,
	 1, 17,  9, 25,  7, 23, 15, 31,
	19,  3, 27, 11, 29, 13,  5, 21
};


/* function prototypes (see filename comment above) */
int all_clients_initialized(void);
void close_all_clients(void);
void close_clients(char *);
int get_autorepeat_state(void);
int get_initial_autorepeat_state(void);
void autorepeat(int restore, int quiet);
char *bitprint(unsigned int, int);
void blackout_tiles(void);
void solid_bg(int);
void check_connect_inputs(void);
void check_padded_fb(void);
void clean_up_exit(int);
void clear_modifiers(int init);
void clear_keys(void);
int copy_screen(void);
void check_black_fb(void);
void do_new_fb(int);
void install_padded_fb(char *);
void install_fake_fb(int, int, int);
void remove_fake_fb(void);

int add_keysym(KeySym);
void delete_keycode(KeyCode, int);
void delete_added_keycodes(int);
int count_added_keycodes(void);

double dtime(double *);
double dtime0(double *);
double dnow(void);

void initialize_blackouts(char *);
void initialize_blackouts_and_xinerama(void);
void initialize_clipshift(void);
void initialize_keyboard_and_pointer(void);
void initialize_allowed_input(void);
void initialize_modtweak(void);
void initialize_pointer_map(char *);
void initialize_cursors_mode(void);
void initialize_remap(char *);
void initialize_screen(int *argc, char **argv, XImage *fb);
void initialize_polling_images(void);
void initialize_signals(void);
void initialize_tiles(void);
void initialize_speeds(void);
void clean_shm(int);
void free_tiles(void);
void initialize_watch_bell(void);
void initialize_xinerama(void);
void initialize_xfixes(void);
void initialize_xdamage(void);
int valid_window(Window, XWindowAttributes *, int);
int xtranslate(Window, Window, int, int, int*, int*, Window*, int);
void create_xdamage_if_needed(void);
void destroy_xdamage_if_needed(void);
void mark_for_xdamage(int, int, int, int);
void mark_region_for_xdamage(sraRegionPtr);
void set_xdamage_mark(int, int, int, int);
void initialize_xrandr(void);
XImage *initialize_xdisplay_fb(void);

void initialize_pipeinput(void);
void keyboard(rfbBool down, rfbKeySym keysym, rfbClientPtr client);
void pipe_keyboard(rfbBool down, rfbKeySym keysym, rfbClientPtr client);

void XTestFakeKeyEvent_wr(Display*, KeyCode, Bool, unsigned long);
void XTestFakeButtonEvent_wr(Display*, unsigned int, Bool, unsigned long);
void XTestFakeMotionEvent_wr(Display*, int, int, int, unsigned long);
int XTestGrabControl_wr(Display*, Bool);
Bool XTestCompareCurrentCursorWithWindow_wr(Display*, Window);
Bool XTestCompareCursorWithWindow_wr(Display*, Window, Cursor);
Bool XTestQueryExtension_wr(Display*, int*, int*, int*, int*);
Bool XETrapQueryExtension_wr(Display*, int*, int*, int*);
void XTestDiscard_wr(Display*);

typedef struct hint {
	/* location x, y, height, and width of a change-rectangle  */
	/* (grows as adjacent horizontal tiles are glued together) */
	int x, y, w, h;
} hint_t;
void mark_hint(hint_t);
void mark_rect_as_modified(int x1, int y1, int x2, int y2, int force);

enum rfbNewClientAction new_client(rfbClientPtr client);
void set_nofb_params(void);
void set_raw_fb_params(int);
void nofb_hook(rfbClientPtr client);
void pointer(int mask, int x, int y, rfbClientPtr client);
void pipe_pointer(int mask, int x, int y, rfbClientPtr client);
int check_pipeinput(void);
void cursor_position(int, int);
void do_button_mask_change(int, int);

void parse_wireframe(void);
void parse_scroll_copyrect(void);
void set_wirecopyrect_mode(char *);
void set_scrollcopyrect_mode(char *);
void initialize_scroll_matches(void);
void initialize_scroll_term(void);
void initialize_max_keyrepeat(void);
void initialize_scroll_keys(void);
int try_copyrect(Window, int, int, int, int, int, int, int *, sraRegionPtr,
    double);
void do_copyregion(sraRegionPtr, int, int);
int direct_fb_copy(int, int, int, int, int);
int get_wm_frame_pos(int *, int *, int *, int *, int *, int *,
    Window *, Window *);
Window descend_pointer(int, Window, char *, int);
int near_wm_edge(int, int, int, int, int, int);
int near_scrollbar_edge(int, int, int, int, int, int);
void read_vnc_connect_prop(void);
void set_vnc_connect_prop(char *);
void fb_push(void);
#define FB_COPY 0x1
#define FB_MOD  0x2
#define FB_REQ  0x4
void fb_push_wait(double, int);
char *process_remote_cmd(char *, int);
void rfbPE(long);
void rfbCFD(long);
int scan_for_updates(int);
void set_colormap(int);
void set_offset(void);
void set_rfb_cursor(int);
void set_visual(char *vstring);
int set_cursor(int, int, int);
void setup_cursors(void);
void setup_cursors_and_push(void);
void first_cursor(void);
rfbCursorPtr pixels2curs(unsigned long *, int, int, int, int, int);
void set_no_cursor(void);
void set_cursor_was_changed(rfbScreenInfoPtr);
void set_cursor_was_moved(rfbScreenInfoPtr);
int get_which_cursor(void);
int get_xfixes_cursor(int);

void disable_cursor_shape_updates(rfbScreenInfoPtr);
void restore_cursor_shape_updates(rfbScreenInfoPtr);
int new_fb_size_clients(rfbScreenInfoPtr);
void get_client_regions(int *, int *, int *, int *);

void shm_clean(XShmSegmentInfo *, XImage *);
void shm_delete(XShmSegmentInfo *);

int check_x11_pointer(void);
void check_bell_event(void);
void check_xevents(void);
char *this_host(void);
void set_vnc_desktop_name(void);

char *short_kmb(char *);

char **create_str_list(char *);
int match_str_list(char *, char **);

int link_rate(int *, int *);
int get_cmp_rate(void);
int get_raw_rate(void);
int get_read_rate(void);
int get_net_rate(void);
int get_net_latency(void);
int get_latency(void);
void measure_send_rates(int);
int fb_update_sent(int *);
void snapshot_stack_list(int, double);

int get_remote_port(int sock);
int get_local_port(int sock);
char *get_remote_host(int sock);
char *get_local_host(int sock);

void xcut_receive(char *text, int len, rfbClientPtr client);

void parse_scale_string(char *, double *, int *, int *,
    int *, int *, int *, int *, int *);
void scale_rect(double, int, int, int,
    char *, int, char *, int,
    int, int, int, int, int, int, int, int, int);
int scale_round(int, double);

void zero_fb(int, int, int, int);
void push_black_screen(int);
void push_sleep(int);
void refresh_screen(int);


/* -- options.h -- */
/* 
 * variables for the command line options
 */
char *use_dpy = NULL;		/* -display */
char *auth_file = NULL;		/* -auth/-xauth */
char *visual_str = NULL;	/* -visual */
char *logfile = NULL;		/* -o, -logfile */
int logfile_append = 0;
char *flagfile = NULL;		/* -flag */
char *passwdfile = NULL;	/* -passwdfile */
char *blackout_str = NULL;	/* -blackout */
char *clip_str = NULL;		/* -clip */
int use_solid_bg = 0;		/* -solid */
char *solid_str = NULL;
char *solid_default = "cyan4";

#define LATENCY0 20	/* 20ms */
#define NETRATE0 20	/* 20KB/sec */
char *speeds_str = NULL;	/* -speeds TBD */
int measure_speeds = 1;
int speeds_net_rate = 0;
int speeds_net_rate_measured = 0;
int speeds_net_latency = 0;
int speeds_net_latency_measured = 0;
int speeds_read_rate = 0;
int speeds_read_rate_measured = 0;
enum {
	LR_UNSET = 0,
	LR_UNKNOWN,
	LR_DIALUP,
	LR_BROADBAND,
	LR_LAN
};


char *rc_rcfile = NULL;		/* -rc */
int rc_norc = 0;
int opts_bg = 0;

#ifndef SHARED
int shared = 0;			/* share vnc display. */
#else
int shared = 1;
#endif
#ifndef FOREVER
int connect_once = 1;		/* disconnect after first connection session. */
#else
int connect_once = 0;
#endif
int deny_all = 0;		/* global locking of new clients */
#ifndef REMOTE_DEFAULT
#define REMOTE_DEFAULT 1
#endif
int accept_remote_cmds = REMOTE_DEFAULT;	/* -noremote */
int safe_remote_only = 1;	/* -unsafe */
int priv_remote = 0;		/* -privremote */
int more_safe = 0;		/* -safer */
#ifndef EXTERNAL_COMMANDS
#define EXTERNAL_COMMANDS 1
#endif
#if EXTERNAL_COMMANDS
int no_external_cmds = 0;	/* -nocmds */
#else
int no_external_cmds = 1;	/* cannot be turned back on. */
#endif
int started_as_root = 0;
int host_lookup = 1;
char *users_list = NULL;	/* -users */
char *allow_list = NULL;	/* for -allow and -localhost */
char *listen_str = NULL;
char *allow_once = NULL;	/* one time -allow */
char *accept_cmd = NULL;	/* for -accept */
char *gone_cmd = NULL;		/* for -gone */
int view_only = 0;		/* clients can only watch. */
char *allowed_input_view_only = NULL;
char *allowed_input_normal = NULL;
char *allowed_input_str = NULL;
char *viewonly_passwd = NULL;	/* view only passwd. */
int inetd = 0;			/* spawned from inetd(1) */
int first_conn_timeout = 0;	/* -timeout */
int flash_cmap = 0;		/* follow installed colormaps */
int shift_cmap = 0;		/* ncells < 256 and needs shift of pixel values */
int force_indexed_color = 0;	/* whether to force indexed color for 8bpp */
int launch_gui = 0;		/* -gui */

int use_modifier_tweak = 1;	/* use the shift/altgr modifier tweak */
int use_iso_level3 = 0;		/* ISO_Level3_Shift instead of Mode_switch */
int clear_mods = 0;		/* -clear_mods (1) and -clear_keys (2) */
int nofb = 0;			/* do not send any fb updates */
char *raw_fb_str = NULL;	/* used under -rawfb */
char *pipeinput_str = NULL;	/* -pipeinput [tee,reopen,keycodes:]cmd */
char *pipeinput_opts = NULL;
FILE *pipeinput_fh = NULL;
int pipeinput_tee = 0;

unsigned long subwin = 0x0;	/* -id, -sid */
int subwin_wait_mapped = 0;

int debug_xevents = 0;		/* -R debug_xevents:1 */
int debug_xdamage = 0;		/* -R debug_xdamage:1 or 2 ... */
int debug_wireframe = 0;
int debug_tiles = 0;

int xtrap_input = 0;		/* -xtrap for user input insertion */
int xinerama = 0;		/* -xinerama */
int xrandr = 0;			/* -xrandr */
int xrandr_present = 0;
int xrandr_width  = -1;
int xrandr_height = -1;
int xrandr_rotation = -1;
Time xrandr_timestamp = 0;
Time xrandr_cfg_time = 0;
char *xrandr_mode = NULL;
char *pad_geometry = NULL;
time_t pad_geometry_time;
int use_snapfb = 0;

Display *rdpy_data = NULL;		/* Data connection for RECORD */
Display *rdpy_ctrl = NULL;		/* Control connection for RECORD */
int use_xrecord = 0;
int noxrecord = 0;

Display *gdpy_data = NULL;		/* Ditto for GrabServer watcher */
Display *gdpy_ctrl = NULL;
int xserver_grabbed = 0;

char *client_connect = NULL;	/* strings for -connect option */
char *client_connect_file = NULL;
int vnc_connect = 1;		/* -vncconnect option */

int show_cursor = 1;		/* show cursor shapes */
int show_multiple_cursors = 0;	/* show X when on root background, etc */
char *multiple_cursors_mode = NULL;
int cursor_pos_updates = 1;	/* cursor position updates -cursorpos */
int cursor_shape_updates = 1;	/* cursor shape updates -nocursorshape */
int use_xwarppointer = 0;	/* use XWarpPointer instead of XTestFake... */
int show_dragging = 1;		/* process mouse movement events */
#ifndef WIREFRAME
#define WIREFRAME 1
#endif
int wireframe = WIREFRAME;	/* try to emulate wireframe wm moves */
/* shade,linewidth,percent,T+B+L+R,t1+t2+t3+t4 */
#ifndef WIREFRAME_PARMS
#define WIREFRAME_PARMS "0xff,3,0,32+8+8+8,0.15+0.30+5.0+0.125"
#endif
char *wireframe_str = NULL;
char *wireframe_copyrect = NULL;
#ifndef WIREFRAME_COPYRECT
#define WIREFRAME_COPYRECT 1
#endif
#if WIREFRAME_COPYRECT
char *wireframe_copyrect_default = "always";
#else
char *wireframe_copyrect_default = "never";
#endif
int wireframe_in_progress = 0;

typedef struct winattr {
	Window win;
	int fetched;
	int valid;
	int x, y;
	int width, height;
	int depth;
	int class;
	int backing_store;
	int map_state;
	int rx, ry;
	double time; 
} winattr_t;
winattr_t *stack_list = NULL;
int stack_list_len = 0;
int stack_list_num = 0;
winattr_t *stack_clip = NULL;
int stack_clip_len = 0;
int stack_clip_num = 0;

/* T+B+L+R,tkey+presist_key,tmouse+persist_mouse */
#ifndef SCROLL_COPYRECT_PARMS
#define SCROLL_COPYRECT_PARMS "0+64+32+32,0.02+0.10+0.9,0.03+0.06+0.5+0.1+5.0"
#endif
char *scroll_copyrect_str = NULL;
#ifndef SCROLL_COPYRECT
#define SCROLL_COPYRECT 1
#endif
char *scroll_copyrect = NULL;
#if SCROLL_COPYRECT
#if 1
char *scroll_copyrect_default = "always";	/* -scrollcopyrect */
#else
char *scroll_copyrect_default = "keys";
#endif
#else
char *scroll_copyrect_default = "never";
#endif
char *scroll_key_list_str = NULL;
KeySym *scroll_key_list = NULL;
int pointer_queued_sent = 0;

int scrollcopyrect_min_area = 60000;	/* minimum rectangle area */
int debug_scroll = 0;
double pointer_flush_delay = 0.0;
double last_scroll_event = 0.0;
int max_scroll_keyrate = 0;
double max_keyrepeat_time = 0.0;
char *max_keyrepeat_str = NULL;
char *max_keyrepeat_str0 = "4-20";
int max_keyrepeat_lo = 1, max_keyrepeat_hi = 40;
enum scroll_types {
	SCR_NONE = 0,
	SCR_MOUSE,
	SCR_KEY,
	SCR_FAIL,
	SCR_SUCCESS
};
int last_scroll_type = SCR_NONE;

char **scroll_good_all = NULL;
char **scroll_good_key = NULL;
char **scroll_good_mouse = NULL;
char *scroll_good_str = NULL;
char *scroll_good_str0 =
/*	"##Firefox-bin," */
/*	"##Gnome-terminal," */
/*	"##XTerm", */
	"##Nomatch"
;

char **scroll_skip_all = NULL;
char **scroll_skip_key = NULL;
char **scroll_skip_mouse = NULL;
char *scroll_skip_str = NULL;
char *scroll_skip_str0 =
/*	"##Konsole,"	 * no problems, known heuristics do not work */
	"##Soffice.bin," /* big problems, no clips, scrolls outside area */
	"##StarOffice"
;

char **scroll_term = NULL;
char *scroll_term_str = NULL;
char *scroll_term_str0 =
	"term"
;

#ifndef NOREPEAT
#define NOREPEAT 1
#endif
int no_autorepeat = NOREPEAT;	/* turn off autorepeat with clients */
int no_repeat_countdown = 2;
int watch_bell = 1;		/* watch for the bell using XKEYBOARD */
int sound_bell = 1;		/* actually send it */
int xkbcompat = 0;		/* ignore XKEYBOARD extension */
int use_xkb_modtweak = 0;	/* -xkb */
#ifndef SKIPDUPS
#define SKIPDUPS 1
#endif
int skip_duplicate_key_events = SKIPDUPS;
char *skip_keycodes = NULL;
#ifndef ADDKEYSYMS
#define ADDKEYSYMS 1
#endif
int add_keysyms = ADDKEYSYMS;	/* automatically add keysyms to X server */

char *remap_file = NULL;	/* -remap */
char *pointer_remap = NULL;
/* use the various ways of updating pointer */
#ifndef POINTER_MODE_DEFAULT
#define POINTER_MODE_DEFAULT 2
#endif
#define POINTER_MODE_NOFB 2
int pointer_mode = POINTER_MODE_DEFAULT;
int pointer_mode_max = 4;	
int single_copytile = 0;	/* use the old way copy_tiles() */
int single_copytile_orig = 0;
int single_copytile_count = 0;
int tile_shm_count = 0;

int using_shm = 1;		/* whether mit-shm is used */
int flip_byte_order = 0;	/* sometimes needed when using_shm = 0 */
/*
 * waitms is the msec to wait between screen polls.  Not too old h/w shows
 * poll times of 10-35ms, so maybe this value cuts the idle load by 2 or so.
 */
int waitms = 30;
double wait_ui = 2.0;
int wait_bog = 1;
int defer_update = 30;	/* deferUpdateTime ms to wait before sends. */

int screen_blank = 60;	/* number of seconds of no activity to throttle */
			/* down the screen polls.  zero to disable. */
int take_naps = 1;	/* -nap/-nonap */
int naptile = 4;	/* tile change threshold per poll to take a nap */
int napfac = 4;		/* time = napfac*waitms, cut load with extra waits */
int napmax = 1500;	/* longest nap in ms. */
int ui_skip = 10;	/* see watchloop.  negative means ignore input */

int watch_selection = 1;	/* normal selection/cutbuffer maintenance */
int watch_primary = 1;		/* more dicey, poll for changes in PRIMARY */

char *sigpipe = NULL;		/* skip, ignore, exit */

/* visual stuff for -visual override or -overlay */
VisualID visual_id = (VisualID) 0;
int visual_depth = 0;

/* for -overlay mode on Solaris/IRIX.  X server draws cursor correctly.  */
int overlay = 0;
int overlay_cursor = 1;

int xshm_present = 0;
int xtest_present = 0;
int xtrap_present = 0;
int xrecord_present = 0;
int xkb_present = 0;
int xinerama_present = 0;

/* tile heuristics: */
double fs_frac = 0.75;	/* threshold tile fraction to do fullscreen updates. */
int tile_fuzz = 2;	/* tolerance for suspecting changed tiles touching */
			/* a known changed tile. */
int grow_fill = 3;	/* do the grow islands heuristic with this width. */
int gaps_fill = 4;	/* do a final pass to try to fill gaps between tiles. */

int debug_pointer = 0;
int debug_keyboard = 0;

int quiet = 0;

/* info about command line opts */
int got_rfbport = 0;
int got_alwaysshared = 0;
int got_nevershared = 0;
int got_cursorpos = 0;
int got_pointer_mode = -1;
int got_noviewonly = 0;
int got_wirecopyrect = 0;
int got_scrollcopyrect = 0;
int got_noxkb = 0;
int got_nomodtweak = 0;

/* threaded vs. non-threaded (default) */
#if LIBVNCSERVER_X11VNC_THREADED && ! defined(X11VNC_THREADED)
#define X11VNC_THREADED
#endif

#if LIBVNCSERVER_HAVE_LIBPTHREAD && defined(X11VNC_THREADED)
	int use_threads = 1;
#else
	int use_threads = 0;
#endif


/* -- util.h -- */

#define NONUL(x) ((x) ? (x) : "")

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
#define X_INIT INIT_MUTEX(x11Mutex)
#if 1
#define X_LOCK       LOCK(x11Mutex)
#define X_UNLOCK   UNLOCK(x11Mutex)
#else
int hxl = 0;
#define X_LOCK       fprintf(stderr, "*** X_LOCK**[%05d]  %d%s\n", \
	__LINE__, hxl, hxl ? "  BAD-PRE-LOCK":""); LOCK(x11Mutex); hxl = 1;
#define X_UNLOCK     fprintf(stderr, "    x_unlock[%05d]  %d%s\n", \
	__LINE__, hxl, !hxl ? "  BAD-PRE-UNLOCK":""); UNLOCK(x11Mutex); hxl = 0;
#endif

MUTEX(scrollMutex);
#define SCR_LOCK   if (use_threads) {LOCK(scrollMutex);}
#define SCR_UNLOCK if (use_threads) {UNLOCK(scrollMutex);}
#define SCR_INIT INIT_MUTEX(scrollMutex)

/* -- util.c -- */

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

int nabs(int n) {
	if (n < 0) {
		return -n;
	} else {
		return n;
	}
}

double dabs(double x) {
	if (x < 0.0) {
		return -x;
	} else {
		return x;
	}
}

void lowercase(char *str) {
	char *p;
	if (str == NULL) {
		return;
	}
	p = str;
	while (*p != '\0') {
		*p = tolower(*p);
		p++;
	}
}

void uppercase(char *str) {
	char *p;
	if (str == NULL) {
		return;
	}
	p = str;
	while (*p != '\0') {
		*p = toupper(*p);
		p++;
	}
}

char *lblanks(char *str) {
	char *p = str;
	while (*p) {
		if (! isspace(*p)) {
			break;
		}
		p++;
	}
	return p;
}

int scan_hexdec(char *str, unsigned long *num) {
	if (sscanf(str, "0x%lx", num) != 1) {
		if (sscanf(str, "%ld", num) != 1) {
			return 0;
		}
	}
	return 1;
}

int parse_geom(char *str, int *wp, int *hp, int *xp, int *yp, int W, int H) {
	int w, h, x, y;
	/* handle +/-x and +/-y */
	if (sscanf(str, "%dx%d+%d+%d", &w, &h, &x, &y) == 4) {
		;
	} else if (sscanf(str, "%dx%d-%d+%d", &w, &h, &x, &y) == 4) {
		w = nabs(w);
		x = W - x - w;
	} else if (sscanf(str, "%dx%d+%d-%d", &w, &h, &x, &y) == 4) {
		h = nabs(h);
		y = H - y - h;
	} else if (sscanf(str, "%dx%d-%d-%d", &w, &h, &x, &y) == 4) {
		w = nabs(w);
		h = nabs(h);
		x = W - x - w;
		y = H - y - h;
	} else {
		return 0;
	}
	*wp = w;
	*hp = h;
	*xp = x;
	*yp = y;
	return 1;
}

void set_env(char *name, char *value) {
	char *str;
	str = (char *)malloc(strlen(name)+strlen(value)+2);
	sprintf(str, "%s=%s", name, value);
	putenv(str);
}

int pick_windowid(unsigned long *num) {
	char line[512];
	int ok = 0, n = 0, msec = 10, secmax = 15;
	FILE *p;

	if (use_dpy) {
		set_env("DISPLAY", use_dpy);
	}
	if (no_external_cmds) {
		rfbLog("cannot run external commands in -nocmds mode:\n");
		rfbLog("   \"%s\"\n", "xwininfo");
		rfbLog("   exiting.\n");
		clean_up_exit(1);
	}
	p = popen("xwininfo", "r");

	if (! p) {
		return 0;
	}

	fprintf(stderr, "\n");
	fprintf(stderr, "  Please select the window for x11vnc to poll\n");
	fprintf(stderr, "  by clicking the mouse in that window.\n");
	fprintf(stderr, "\n");

	while (msec * n++ < 1000 * secmax) {
		unsigned long tmp;
		char *q;
		fd_set set;
		struct timeval tv;

		if (screen && screen->clientHead) {
			/* they may be doing the pointer-pick thru vnc: */
			int nfds;
			tv.tv_sec = 0;
			tv.tv_usec = msec * 1000;
			FD_ZERO(&set);
			FD_SET(fileno(p), &set);

			nfds = select(fileno(p)+1, &set, NULL, NULL, &tv);
			
			if (nfds == 0 || nfds < 0) {
				/* 
				 * select timedout or error.
				 * note this rfbPE takes about 30ms too:
				 */
				rfbPE(-1);
				XFlush(dpy);
				continue;
			}
		}
		
		if (fgets(line, 512, p) == NULL) {
			break;
		}
		q = strstr(line, " id: 0x"); 
		if (q) {
			q += 5;
			if (sscanf(q, "0x%lx ", &tmp) == 1) {
				ok = 1;
				*num = tmp;
				fprintf(stderr, "  Picked: 0x%lx\n\n", tmp);
				break;
			}
		}
	}
	pclose(p);
	return ok;
}

Window query_pointer(Window start) {
	Window r, c;	
	int rx, ry, wx, wy;
	unsigned int mask;
	if (start == None) {
		start = rootwin;
	}
	if (XQueryPointer(dpy, start, &r, &c, &rx, &ry, &wx, &wy, &mask)) {
		return c;
	} else {
		return None;
	}
}

char *bitprint(unsigned int st, int nbits) {
	static char str[33];
	int i, mask;
	if (nbits > 32) {
		nbits = 32;
	}
	for (i=0; i<nbits; i++) {
		str[i] = '0';
	}
	str[nbits] = '\0';
	mask = 1;
	for (i=nbits-1; i>=0; i--) {
		if (st & mask) {
			str[i] = '1';
		}
		mask = mask << 1;
	}
	return str;	/* take care to use or copy immediately */
}

char *get_user_name(void) {
	char *user = NULL;

	user = getenv("USER");
	if (user == NULL) {
		user = getenv("LOGNAME");
	}

#if LIBVNCSERVER_HAVE_PWD_H
	if (user == NULL) {
		struct passwd *pw = getpwuid(getuid());
		if (pw) {
			user = pw->pw_name;
		}
	}
#endif

	if (user) {
		return(strdup(user));
	} else {
		return(strdup("unknown-user"));
	}
}

char *get_home_dir(void) {
	char *home = NULL;

	home = getenv("HOME");

#if LIBVNCSERVER_HAVE_PWD_H
	if (home == NULL) {
		struct passwd *pw = getpwuid(getuid());
		if (pw) {
			home = pw->pw_dir;
		}
	}
#endif

	if (home) {
		return(strdup(home));
	} else {
		return(strdup("/"));
	}
}

char *get_shell(void) {
	char *shell = NULL;

	shell = getenv("SHELL");

#if LIBVNCSERVER_HAVE_PWD_H
	if (shell == NULL) {
		struct passwd *pw = getpwuid(getuid());
		if (pw) {
			shell = pw->pw_shell;
		}
	}
#endif

	if (shell) {
		return(strdup(shell));
	} else {
		return(strdup("/bin/sh"));
	}
}

/* -- user.c -- */

int switch_user(char *, int);
int switch_user_env(uid_t, char*, char *, int);
void try_to_switch_users(void);
char *guess_desktop(void);

/* tasks for after we switch */
void switch_user_task_dummy(void) {
	;	/* dummy does nothing */
}
void switch_user_task_solid_bg(void) {
	/* we have switched users, some things to do. */
	if (use_solid_bg && client_count) {
		solid_bg(0);
	}
}

void check_switched_user(void) {
	static time_t sched_switched_user = 0;
	static int did_solid = 0;
	static int did_dummy = 0;
	int delay = 15;
	time_t now = time(0);

	if (started_as_root == 1 && users_list) {
		try_to_switch_users();
		if (started_as_root == 2) {
			/*
			 * schedule the switch_user_tasks() call
			 * 15 secs is for piggy desktops to start up.
			 * might not be enough for slow machines...
			 */
			sched_switched_user = now;
			did_dummy = 0;
			did_solid = 0;
			/* add other activities */
		}
	}
	if (! sched_switched_user) {
		return;
	}

	if (! did_dummy) {
		switch_user_task_dummy();
		did_dummy = 1;
	}
	if (! did_solid) {
		int doit = 0;
		char *ss = solid_str;
		if (now >= sched_switched_user + delay) {
			doit = 1;
		} else if (ss && strstr(ss, "root:") == ss) {
		    	if (now >= sched_switched_user + 3) {
				doit = 1;
			}
		} else if (strcmp("root", guess_desktop())) {
			usleep(1000 * 1000);
			doit = 1;
		}
		if (doit) {
			switch_user_task_solid_bg();
			did_solid = 1;
		}
	}

	if (did_dummy && did_solid) {
		sched_switched_user = 0;
	}
}

/* utilities for switching users */
char *get_login_list(int with_display) {
	char *out;
#if LIBVNCSERVER_HAVE_UTMPX_H
	int i, cnt, max = 200, ut_namesize = 32;
	int dpymax = 1000, sawdpy[1000];
	struct utmpx *utx;

	/* size based on "username:999," * max */
	out = (char *) malloc(max * (ut_namesize+1+3+1) + 1);
	out[0] = '\0';

	for (i=0; i<dpymax; i++) {
		sawdpy[i] = 0;
	}

	setutxent();
	cnt = 0;
	while (1) {
		char *user, *line, *host, *id;
		char tmp[10];
		int d = -1;
		utx = getutxent();
		if (! utx) {
			break;
		}
		if (utx->ut_type != USER_PROCESS) {
			continue;
		}
		user = lblanks(utx->ut_user);
		if (*user == '\0') {
			continue;
		}
		if (strchr(user, ',')) {
			continue;	/* unlikely, but comma is our sep. */
		}

		line = lblanks(utx->ut_line);
		host = lblanks(utx->ut_host);
		id   = lblanks(utx->ut_id);

		if (with_display) {
			if (0 && line[0] != ':' && strcmp(line, "dtlocal")) {
				/* XXX useful? */
				continue;
			}

			if (line[0] == ':') {
				if (sscanf(line, ":%d", &d) != 1)  {
					d = -1;
				}
			}
			if (d < 0 && host[0] == ':') {
				if (sscanf(host, ":%d", &d) != 1)  {
					d = -1;
				}
			}
			if (d < 0 && id[0] == ':') {
				if (sscanf(id, ":%d", &d) != 1)  {
					d = -1;
				}
			}

			if (d < 0 || d >= dpymax || sawdpy[d]) {
				continue;
			}
			sawdpy[d] = 1;
			sprintf(tmp, ":%d", d);
		} else {
			/* try to eliminate repeats */
			int repeat = 0;
			char *q;

			q = out;
			while ((q = strstr(q, user)) != NULL) {
				char *p = q + strlen(user) + strlen(":DPY");
				if (q == out || *(q-1) == ',') {
					/* bounded on left. */
					if (*p == ',' || *p == '\0') {
						/* bounded on right. */
						repeat = 1;
						break;
					}
				}
				q = p;
			}
			if (repeat) {
				continue;
			}
			sprintf(tmp, ":DPY");
		}

		if (*out) {
			strcat(out, ",");
		}
		strcat(out, user);
		strcat(out, tmp);

		cnt++;
		if (cnt >= max) {
			break;
		}
	}
	endutxent();
#else
	out = strdup("");
#endif
	return out;
}

char **user_list(char *user_str) {
	int n, i;
	char *p, **list;
	
	p = user_str;
	n = 1;
	while (*p++) {
		if (*p == ',') {
			n++;
		}
	}
	list = (char **) malloc((n+1)*(sizeof(char *)));

	p = strtok(user_str, ",");
	i = 0;
	while (p) {
		list[i++] = p;
		p = strtok(NULL, ",");
	}
	list[i] = NULL;
	return list;
}

void user2uid(char *user, uid_t *uid, char **name, char **home) {
	int numerical = 1;
	char *q;

	*uid = (uid_t) -1;
	*name = NULL;
	*home = NULL;

	q = user;
	while (*q) {
		if (! isdigit(*q++)) {
			numerical = 0;
			break;
		}
	}

	if (numerical) {
		int u = atoi(user);

		if (u < 0) {
			return;
		}
		*uid = (uid_t) u;
	}

#if LIBVNCSERVER_HAVE_PWD_H
	if (1) {
		struct passwd *pw;
		if (numerical) {
			pw = getpwuid(*uid);
		} else {
			pw = getpwnam(user);
		}
		if (pw) {
			*uid  = pw->pw_uid;
			*name = pw->pw_name;	/* n.b. use immediately */
			*home = pw->pw_dir;
		}
	}
#endif
}

int try_user_and_display(uid_t, char*);

int lurk(char **users) {
	uid_t uid;
	int success = 0, dmin = -1, dmax = -1;
	char *p, *logins, **u;

	if ((u = users) != NULL && *u != NULL && *(*u) == ':') {
		int len;
		char *tmp;

		/* extract min and max display numbers */
		tmp = *u;
		if (strchr(tmp, '-')) {
			if (sscanf(tmp, ":%d-%d", &dmin, &dmax) != 2) {
				dmin = -1;
				dmax = -1;
			}
		}
		if (dmin < 0) {
			if (sscanf(tmp, ":%d", &dmin) != 1) {
				dmin = -1;
				dmax = -1;
			} else {
				dmax = dmin;
			}
		}
		if ((dmin < 0 || dmax < 0) || dmin > dmax || dmax > 10000) {
			dmin = -1;
			dmax = -1;
		}

		/* get user logins regardless of having a display: */
		logins = get_login_list(0);

		/*
		 * now we append the list in users (might as well try
		 * them) this will probably allow weird ways of starting
		 * xservers to work.
		 */
		len = strlen(logins);
		u++;
		while (*u != NULL) {
			len += strlen(*u) + strlen(":DPY,");
			u++;
		}
		tmp = (char *) malloc(len+1);
		strcpy(tmp, logins);

		/* now concatenate them: */
		u = users+1;
		while (*u != NULL) {
			char *q, chk[100];
			snprintf(chk, 100, "%s:DPY", *u);
			q = strstr(tmp, chk);
			if (q) {
				char *p = q + strlen(chk);
				
				if (q == tmp || *(q-1) == ',') {
					/* bounded on left. */
					if (*p == ',' || *p == '\0') {
						/* bounded on right. */
						u++;
						continue;
					}
				}
			}
			
			if (*tmp) {
				strcat(tmp, ",");
			}
			strcat(tmp, *u);
			strcat(tmp, ":DPY");
			u++;
		}
		free(logins);
		logins = tmp;
		
	} else {
		logins = get_login_list(1);
	}
	
	p = strtok(logins, ",");
	while (p) {
		char *user, *name, *home, dpystr[10];
		char *q, *t;
		int ok = 1, dn;
		
		t = strdup(p);	/* bob:0 */
		q = strchr(t, ':'); 
		if (! q) {
			free(t);
			break;
		}
		*q = '\0';
		user = t;
		snprintf(dpystr, 10, ":%s", q+1);

		if (users) {
			u = users;
			ok = 0;
			while (*u != NULL) {
				if (*(*u) == ':') {
					u++;
					continue;
				}
				if (!strcmp(user, *u++)) {
					ok = 1;
					break;
				}
			}
		}

		user2uid(user, &uid, &name, &home);
		free(t);

		if (! uid) {
			ok = 0;
		}

		if (! ok) {
			p = strtok(NULL, ",");
			continue;
		}
		
		for (dn = dmin; dn <= dmax; dn++) {
			if (dn >= 0) {
				sprintf(dpystr, ":%d", dn);
			}
			if (try_user_and_display(uid, dpystr)) {
				if (switch_user_env(uid, name, home, 0)) {
					rfbLog("lurk: now user: %s @ %s\n",
					    name, dpystr);
					started_as_root = 2;
					success = 1;
				}
				set_env("DISPLAY", dpystr);
				break;
			}
		}
		if (success) {
			 break;
		}

		p = strtok(NULL, ",");
	}
	free(logins);
	return success;
}

void lurk_loop(char *str) {
	char *tstr = NULL, **users = NULL;

	if (strstr(str, "lurk=") != str) {
		exit(1);
	}
	rfbLog("lurking for logins using: '%s'\n", str);
	if (strlen(str) > strlen("lurk=")) {
		char *q = strchr(str, '=');
		tstr = strdup(q+1);
		users = user_list(tstr);
	}

	while (1) {
		if (lurk(users)) {
			break;
		}
		sleep(3);
	}
	if (tstr) {
		free(tstr);
	}
	if (users) {
		free(users);
	}
}

int guess_user_and_switch(char *str, int fb_mode) {
	char *dstr, *d = DisplayString(dpy);
	char *p, *tstr = NULL, *allowed = NULL, *logins, **users = NULL;
	int dpy1, ret = 0;

	/* pick out ":N" */
	dstr = strchr(d, ':');
	if (! dstr) {
		return 0;
	}
	if (sscanf(dstr, ":%d", &dpy1) != 1) {
		return 0;
	}
	if (dpy1 < 0) {
		return 0;
	}

	if (strstr(str, "guess=") == str && strlen(str) > strlen("guess=")) {
		allowed = strchr(str, '=');
		allowed++;

		tstr = strdup(allowed);
		users = user_list(tstr);
	}

	/* loop over the utmpx entries looking for this display */
	logins = get_login_list(1);
	p = strtok(logins, ",");
	while (p) {
		char *user, *q, *t;
		int dpy2, ok = 1;

		t = strdup(p);
		q = strchr(t, ':'); 
		if (! q) {
			free(t);
			break;
		}
		*q = '\0';
		user = t;
		dpy2 = atoi(q+1);

		if (users) {
			char **u = users;
			ok = 0;
			while (*u != NULL) {
				if (!strcmp(user, *u++)) {
					ok = 1;
					break;
				}
			}
		}
		if (dpy1 != dpy2) {
			ok = 0;
		}

		if (! ok) {
			free(t);
			p = strtok(NULL, ",");
			continue;
		}
		if (switch_user(user, fb_mode)) {
			rfbLog("switched to guessed user: %s\n", user);
			free(t);
			ret = 1;
			break;
		}

		p = strtok(NULL, ",");
	}
	if (tstr) {
		free(tstr);
	}
	if (users) {
		free(users);
	}
	if (logins) {
		free(logins);
	}
	return ret;
}

int try_user_and_display(uid_t uid, char *dpystr) {
	/* NO strtoks */
#if LIBVNCSERVER_HAVE_FORK && LIBVNCSERVER_HAVE_SYS_WAIT_H && LIBVNCSERVER_HAVE_PWD_H
	pid_t pid, pidw;
	char *home, *name;
	int st;
	struct passwd *pw;
	
	pw = getpwuid(uid);
	if (pw) {
		name = pw->pw_name;
		home = pw->pw_dir;
	} else {
		return 0;
	}

	/* 
	 * We fork here and try to open the display again as the
	 * new user.  Unreadable XAUTHORITY could be a problem...
	 * This is not really needed since we have DISPLAY open but:
	 * 1) is a good indicator this user owns the session and  2)
	 * some activities do spawn new X apps, e.g.  xmessage(1), etc.
	 */
	if ((pid = fork()) > 0) {
		;
	} else if (pid == -1) {
		fprintf(stderr, "could not fork\n");
		rfbLogPerror("fork");
		return 0;
	} else {
		/* child */
		Display *dpy2 = NULL;
		int rc;

		rc = switch_user_env(uid, name, home, 0); 
		if (! rc) {
			exit(1);
		}

		fclose(stderr);
		dpy2 = XOpenDisplay(dpystr);
		if (dpy2) {
			XCloseDisplay(dpy2);
			exit(0);	/* success */
		} else {
			exit(2);	/* fail */
		}
	}

	/* see what the child says: */
	pidw = waitpid(pid, &st, 0);
	if (pidw == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0) {
		return 1;
	}
#endif	/* LIBVNCSERVER_HAVE_FORK ... */
	return 0;
}

int switch_user(char *user, int fb_mode) {
	/* NO strtoks */
	int doit = 0;
	uid_t uid = 0;
	char *name, *home;

	if (*user == '+') {
		doit = 1;
		user++;
	}

	if (strstr(user, "guess=") == user) {
		return guess_user_and_switch(user, fb_mode);
	}

	user2uid(user, &uid, &name, &home);

	if (uid == -1 || uid == 0) {
		return 0;
	}

	if (! doit && dpy) {
		/* see if this display works: */
		char *dstr = DisplayString(dpy);
		doit = try_user_and_display(uid, dstr);
	}

	if (doit) {
		int rc = switch_user_env(uid, name, home, fb_mode);
		if (rc) {
			started_as_root = 2;
		}
		return rc;
	} else {
		return 0;
	}
}

int switch_user_env(uid_t uid, char *name, char *home, int fb_mode) {
	/* NO strtoks */
	char *xauth;
	int reset_fb = 0;

#if !LIBVNCSERVER_HAVE_SETUID
	return 0;
#else
	/*
	 * OK tricky here, we need to free the shm... otherwise
	 * we won't be able to delete it as the other user...
	 */
	if (fb_mode == 1 && using_shm) {
		reset_fb = 1;
		clean_shm(0);
		free_tiles();
	}
	if (setuid(uid) != 0) {
		if (reset_fb) {
			/* 2 means we did clean_shm and free_tiles */
			do_new_fb(2);
		}
		return 0;
	}
#endif
	if (reset_fb) {
		do_new_fb(2);
	}

	xauth = getenv("XAUTHORITY");
	if (xauth && access(xauth, R_OK) != 0) {
		*(xauth-2) = '_';	/* yow */
	}
	
	set_env("USER", name);
	set_env("LOGNAME", name);
	set_env("HOME", home);
	return 1;
}

void try_to_switch_users(void) {
	static time_t last_try = 0;
	time_t now = time(0);
	char *users, *p;

	if (getuid() && geteuid()) {
		rfbLog("try_to_switch_users: not root\n");
		started_as_root = 2;
		return;
	}
	if (!last_try) {
		last_try = now;
	} else if (now <= last_try + 2) {
		/* try every 3 secs or so */
		return;
	}
	last_try = now;

	users = strdup(users_list);

	if (strstr(users, "guess=") == users) {
		if (switch_user(users, 1)) {
			started_as_root = 2;
		}
		free(users);
		return;
	}

	p = strtok(users, ",");
	while (p) {
		if (switch_user(p, 1)) {
			started_as_root = 2;
			rfbLog("try_to_switch_users: now %s\n", p);
			break;
		}
		p = strtok(NULL, ",");
	}
	free(users);
}

/* -- inet.c -- */
/*
 * Simple utility to map host name to dotted IP address.  Ignores aliases.
 * Up to caller to free returned string.
 */
char *host2ip(char *host) {
	struct hostent *hp;
	struct sockaddr_in addr;
	char *str;

	if (! host_lookup) {
		return NULL;
	}

	hp = gethostbyname(host);
	if (!hp) {
		return NULL;
	}
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr =  *(unsigned long *)hp->h_addr;
	str = strdup(inet_ntoa(addr.sin_addr));
	return str;
}

char *raw2host(char *raw, int len) {
	char *str;
#if LIBVNCSERVER_HAVE_NETDB_H && LIBVNCSERVER_HAVE_NETINET_IN_H
	struct hostent *hp;

	if (! host_lookup) {
		return strdup("unknown");
	}

	hp = gethostbyaddr(raw, len, AF_INET);
	if (!hp) {
		return strdup(inet_ntoa(*((struct in_addr *)raw)));
	}
	str = strdup(hp->h_name);
#else
	str = strdup("unknown");
#endif
	return str;
}

char *raw2ip(char *raw) {
	return strdup(inet_ntoa(*((struct in_addr *)raw)));
}

char *ip2host(char *ip) {
	char *str;
#if LIBVNCSERVER_HAVE_NETDB_H && LIBVNCSERVER_HAVE_NETINET_IN_H
	struct hostent *hp;
	in_addr_t iaddr;

	if (! host_lookup) {
		return strdup("unknown");
	}

	iaddr = inet_addr(ip);
	if (iaddr == htonl(INADDR_NONE)) {
		return strdup("unknown");
	}

	hp = gethostbyaddr((char *)&iaddr, sizeof(in_addr_t), AF_INET);
	if (!hp) {
		return strdup("unknown");
	}
	str = strdup(hp->h_name);
#else
	str = strdup("unknown");
#endif
	return str;
}

int dotted_ip(char *host) {
	char *p = host;
	while (*p != '\0') {
		if (*p == '.' || isdigit(*p)) {
			p++;
			continue;
		}
		return 0;
	}
	return 1;
}

int get_port(int sock, int remote) {
	struct sockaddr_in saddr;
	int saddr_len, saddr_port;
	
	saddr_len = sizeof(saddr);
	memset(&saddr, 0, sizeof(saddr));
	saddr_port = -1;
	if (remote) {
		if (!getpeername(sock, (struct sockaddr *)&saddr, &saddr_len)) {
			saddr_port = ntohs(saddr.sin_port);
		}
	} else {
		if (!getsockname(sock, (struct sockaddr *)&saddr, &saddr_len)) {
			saddr_port = ntohs(saddr.sin_port);
		}
	}
	return saddr_port;
}

int get_remote_port(int sock) {
	return get_port(sock, 1);
}

int get_local_port(int sock) {
	return get_port(sock, 0);
}

char *get_host(int sock, int remote) {
	struct sockaddr_in saddr;
	int saddr_len, saddr_port;
	char *saddr_ip_str = NULL;
	
	saddr_len = sizeof(saddr);
	memset(&saddr, 0, sizeof(saddr));
	saddr_port = -1;
#if LIBVNCSERVER_HAVE_NETINET_IN_H
	if (remote) {
		if (!getpeername(sock, (struct sockaddr *)&saddr, &saddr_len)) {
			saddr_ip_str = inet_ntoa(saddr.sin_addr);
		}
	} else {
		if (!getsockname(sock, (struct sockaddr *)&saddr, &saddr_len)) {
			saddr_ip_str = inet_ntoa(saddr.sin_addr);
		}
	}
#endif
	if (! saddr_ip_str) {
		saddr_ip_str = "unknown";
	}
	return strdup(saddr_ip_str);
}

char *get_remote_host(int sock) {
	return get_host(sock, 1);
}

char *get_local_host(int sock) {
	return get_host(sock, 0);
}

char *ident_username(rfbClientPtr client) {
	ClientData *cd = (ClientData *) client->clientData;
	char *str, *newhost, *user = NULL, *newuser = NULL;
	int len;

	if (cd) {
		user = cd->username;
	}
	if (!user || *user == '\0') {
		char msg[128];
		int n, sock, ok = 0;

		if ((sock = rfbConnectToTcpAddr(client->host, 113)) < 0) {
			rfbLog("could not connect to ident: %s:%d\n",
			    client->host, 113);
		} else {
			int ret;
			fd_set rfds;
			struct timeval tv;
			int rport = get_remote_port(client->sock);
			int lport = get_local_port(client->sock);

			sprintf(msg, "%d, %d\r\n", rport, lport);
			n = write(sock, msg, strlen(msg));

			FD_ZERO(&rfds);
			FD_SET(sock, &rfds);
			tv.tv_sec  = 4;
			tv.tv_usec = 0;
			ret = select(sock+1, &rfds, NULL, NULL, &tv); 

			if (ret > 0) {
				int i;
				char *q, *p;
				for (i=0; i<128; i++) {
					msg[i] = '\0';
				}
				usleep(250*1000);
				n = read(sock, msg, 127);
				close(sock);
				if (n <= 0) goto badreply;

				/* 32782 , 6000 : USERID : UNIX :runge */
				q = strstr(msg, "USERID");
				if (!q) goto badreply;
				q = strstr(q, ":");
				if (!q) goto badreply;
				q++;
				q = strstr(q, ":");
				if (!q) goto badreply;
				q++;
				q = lblanks(q);
				p = q;
				while (*p) {
					if (*p == '\r' || *p == '\n') {
						*p = '\0';
					}
					p++;
				}
				ok = 1;
				if (strlen(q) > 24) {
					*(q+24) = '\0';
				}
				newuser = strdup(q);

				badreply:
				n = 0;	/* avoid syntax error */
			} else {
				close(sock);
			}
		}
		if (! ok || !newuser) {
			newuser = strdup("unknown-user");
		}
		if (cd) {
			free(cd->username);
			cd->username = newuser;
		}
		user = newuser;
	}
	newhost = ip2host(client->host);
	len = strlen(user) + 1 + strlen(newhost) + 1;
	str = (char *)malloc(len);
	sprintf(str, "%s@%s", user, newhost);
	free(newhost);
	return str;
}

/* -- ximage.c -- */

/* 
 * used in rfbGetScreen and rfbNewFramebuffer: and estimate to the number
 * of bits per color, of course for some visuals, e.g. 565, the number
 * is not the same for each color.  This is just a sane default.
 */
int guess_bits_per_color(int bits_per_pixel) {
	int bits_per_color;
	
	/* first guess, spread them "evenly" over R, G, and B */
	bits_per_color = bits_per_pixel/3;
	if (bits_per_color < 1) {
		bits_per_color = 1;	/* 1bpp, 2bpp... */
	}

	/* choose safe values for usual cases: */
	if (bits_per_pixel == 8) {
		bits_per_color = 2;
	} else if (bits_per_pixel == 15 || bits_per_pixel == 16) {
		bits_per_color = 5;
	} else if (bits_per_pixel == 24 || bits_per_pixel == 32) {
		bits_per_color = 8;
	}
	return bits_per_color;
}

/*
 * Kludge to interpose image gets and limit to a subset rectangle of
 * the rootwin.  This is the -sid option trying to work around invisible
 * saveUnders menu, etc, windows.  Also -clip option.
 */
int rootshift = 0;
int clipshift = 0;

#define ADJUST_ROOTSHIFT \
	if (rootshift && subwin) { \
		d = rootwin; \
		x += off_x; \
		y += off_y; \
	} \
	if (clipshift) { \
		x += coff_x; \
		y += coff_y; \
	}

/*
 * Wrappers for Image related X calls
 */
Status XShmGetImage_wr(Display *disp, Drawable d, XImage *image, int x, int y,
    unsigned long mask) {

	ADJUST_ROOTSHIFT

	/* Note: the Solaris overlay stuff is all non-shm (using_shm = 0) */

#if LIBVNCSERVER_HAVE_XSHM
	return XShmGetImage(disp, d, image, x, y, mask); 
#else
	return (Status) 0;
#endif
}

XImage *XShmCreateImage_wr(Display* disp, Visual* vis, unsigned int depth,
    int format, char* data, XShmSegmentInfo* shminfo, unsigned int width,
    unsigned int height) {

#if LIBVNCSERVER_HAVE_XSHM
	return XShmCreateImage(disp, vis, depth, format, data, shminfo,
	    width, height); 
#else
	return (XImage *) 0;
#endif
}

Status XShmAttach_wr(Display *disp, XShmSegmentInfo *shminfo) {
#if LIBVNCSERVER_HAVE_XSHM
	return XShmAttach(disp, shminfo);
#else
	return (Status) 0;
#endif
}

Status XShmDetach_wr(Display *disp, XShmSegmentInfo *shminfo) {
#if LIBVNCSERVER_HAVE_XSHM
	return XShmDetach(disp, shminfo);
#else
	return (Status) 0;
#endif
}

Bool XShmQueryExtension_wr(Display *disp) {
#if LIBVNCSERVER_HAVE_XSHM
	return XShmQueryExtension(disp);
#else
	return False;
#endif
}

/* wrapper for overlay screen reading: */

XImage *xreadscreen(Display *disp, Drawable d, int x, int y,
    unsigned int width, unsigned int height, Bool show_cursor) {
#ifdef SOLARIS_OVERLAY
	return XReadScreen(disp, d, x, y, width, height,
	    show_cursor);
#endif
#ifdef IRIX_OVERLAY
	{	unsigned long hints = 0, hints_ret;
		if (show_cursor) hints |= XRD_READ_POINTER;
		return XReadDisplay(disp, d, x, y, width, height,
		    hints, &hints_ret);
	}
#endif
	return NULL;
}

XImage *XGetSubImage_wr(Display *disp, Drawable d, int x, int y,
    unsigned int width, unsigned int height, unsigned long plane_mask,
    int format, XImage *dest_image, int dest_x, int dest_y) {

	ADJUST_ROOTSHIFT

	if (overlay && dest_x == 0 && dest_y == 0) {
		size_t size = dest_image->height * dest_image->bytes_per_line;
		XImage *xi;

		xi = xreadscreen(disp, d, x, y, width, height,
		    (Bool) overlay_cursor);

		if (! xi) return NULL;

		/*
		 * There is extra overhead from memcpy and free...
		 * this is not like the real XGetSubImage().  We hope
		 * this significant overhead is still small compared to
		 * the time to retrieve the fb data.
		 */
		memcpy(dest_image->data, xi->data, size);

		XDestroyImage(xi);
		return (dest_image);
	}
	return XGetSubImage(disp, d, x, y, width, height, plane_mask,
	    format, dest_image, dest_x, dest_y);
}

XImage *XGetImage_wr(Display *disp, Drawable d, int x, int y,
    unsigned int width, unsigned int height, unsigned long plane_mask,
    int format) {

	ADJUST_ROOTSHIFT

	if (overlay) {
		return xreadscreen(disp, d, x, y, width, height,
		    (Bool) overlay_cursor);
	}
	return XGetImage(disp, d, x, y, width, height, plane_mask, format);
}

XImage *XCreateImage_wr(Display *disp, Visual *visual, unsigned int depth,
    int format, int offset, char *data, unsigned int width,
    unsigned int height, int bitmap_pad, int bytes_per_line) {
	/*
	 * This is a kludge to get a created XImage to exactly match what
	 * XReadScreen returns: we noticed the rgb masks are different
	 * from XCreateImage with the high color visual (red mask <->
	 * blue mask).  Note we read from the root window(!) then free
	 * the data.
	 */

	if (raw_fb) {	/* raw_fb hack */
		XImage *xi;
		xi = (XImage *) malloc(sizeof(XImage));
		memset(xi, 0, sizeof(XImage));
		xi->depth = depth;
		xi->bits_per_pixel = (depth == 24) ? 32 : depth;
		xi->format = format;
		xi->xoffset = offset;
		xi->data = data;
		xi->width = width;
		xi->height = height;
		xi->bitmap_pad = bitmap_pad;
		xi->bytes_per_line = bytes_per_line ? bytes_per_line : 
		    xi->width * xi->bits_per_pixel / 8;
		return xi;
	}

	if (overlay) {
		XImage *xi;
		xi = xreadscreen(disp, window, 0, 0, width, height, False);
		if (xi == NULL) {
			return xi;
		}
		if (xi->data != NULL) {
			free(xi->data);
		}
		xi->data = data;
		return xi;
	}

	return XCreateImage(disp, visual, depth, format, offset, data,
	    width, height, bitmap_pad, bytes_per_line);
}

void copy_raw_fb(XImage *dest, int x, int y, unsigned int w, unsigned int h) {
	char *src, *dst;
	int line, pixelsize = bpp/8;
	int bpl = wdpy_x * pixelsize;

	if (clipshift) {
		x += coff_x;
		y += coff_y;
	}
	if (! raw_fb_seek) {
		src = raw_fb_addr + raw_fb_offset + bpl*y + pixelsize*x;
		dst = dest->data;

		for (line = 0; line < h; line++) {
			memcpy(dst, src, w * pixelsize);
			src += bpl;
			dst += dest->bytes_per_line;
		}
	} else{
		int n, len, del, sz = w * pixelsize;
		off_t off = (off_t) (raw_fb_offset + bpl*y + pixelsize*x);

		lseek(raw_fb_fd, off, SEEK_SET);
		dst = dest->data;

		for (line = 0; line < h; line++) {
			len = sz;
			del = 0;
			while (len > 0) {
				n = read(raw_fb_fd, dst + del, len);

				if (n > 0) {
					del += n;
					len -= n;
				} else if (n == 0) {
					break;
				} else {
					/* overkill... */
					if (errno != EINTR && errno != EAGAIN) {
						break;
					}
				}
			}
			if (bpl > sz) {
				off = (off_t) (bpl - sz);
				lseek(raw_fb_fd, off, SEEK_CUR);
			}
			dst += dest->bytes_per_line;
		}
	}
}

void copy_image(XImage *dest, int x, int y, unsigned int w, unsigned int h) {
	/* default (w=0, h=0) is the fill the entire XImage */
	if (w < 1)  {
		w = dest->width;
	}
	if (h < 1)  {
		h = dest->height;
	}

	if (use_snapfb && snap_fb && dest != snaprect) {
		char *src, *dst;
		int line, pixelsize = bpp/8;

		src = snap->data + snap->bytes_per_line*y + pixelsize*x;
		dst = dest->data;
		for (line = 0; line < h; line++) {
			memcpy(dst, src, w * pixelsize);
			src += snap->bytes_per_line;
			dst += dest->bytes_per_line;
		}

	} else if (raw_fb) {
		copy_raw_fb(dest, x, y, w, h);

	} else if (using_shm && w == dest->width && h == dest->height) {
		XShmGetImage_wr(dpy, window, dest, x, y, AllPlanes);

	} else {
		XGetSubImage_wr(dpy, window, x, y, w, h, AllPlanes,
		    ZPixmap, dest, 0, 0);
	}
}

#define DEBUG_SKIPPED_INPUT(dbg, str) \
	if (dbg) { \
		rfbLog("skipped input: %s\n", str); \
	}

/*
 * wrappers for XTestFakeKeyEvent, etc..
 * also for XTrap equivalents XESimulateXEventRequest
 */

void XTRAP_FakeKeyEvent_wr(Display* dpy, KeyCode key, Bool down,
    unsigned long delay) {

	if (! xtrap_present) {
		DEBUG_SKIPPED_INPUT(debug_keyboard, "keyboard: no-XTRAP");
		return;
	}
#if LIBVNCSERVER_HAVE_LIBXTRAP
	XESimulateXEventRequest(trap_ctx, down ? KeyPress : KeyRelease,
	    key, 0, 0, 0);
#else
	DEBUG_SKIPPED_INPUT(debug_keyboard, "keyboard: no-XTRAP-build");
#endif
}

void XTestFakeKeyEvent_wr(Display* dpy, KeyCode key, Bool down,
    unsigned long delay) {
	if (debug_keyboard) {
		rfbLog("XTestFakeKeyEvent(dpy, keycode=0x%x \"%s\", %s)\n",
		    key, XKeysymToString(XKeycodeToKeysym(dpy, key, 0)),
		    down ? "down":"up");
	}
	if (down) {
		last_keyboard_keycode = -key;
	} else {
		last_keyboard_keycode = key;
	}

	if (xtrap_input) {
		return XTRAP_FakeKeyEvent_wr(dpy, key, down, delay);
	}

	if (! xtest_present) {
		DEBUG_SKIPPED_INPUT(debug_keyboard, "keyboard: no-XTEST");
		return;
	}
	if (debug_keyboard) {
		rfbLog("calling XTestFakeKeyEvent(%d, %d)  %.4f\n",
		    key, down, dnow() - x11vnc_start);	
	}
#if LIBVNCSERVER_HAVE_XTEST
	XTestFakeKeyEvent(dpy, key, down, delay);
#endif
}

void XTRAP_FakeButtonEvent_wr(Display* dpy, unsigned int button, Bool is_press,
    unsigned long delay) {

	if (! xtrap_present) {
		DEBUG_SKIPPED_INPUT(debug_keyboard, "button: no-XTRAP");
		return;
	}
#if LIBVNCSERVER_HAVE_LIBXTRAP
	XESimulateXEventRequest(trap_ctx,
	    is_press ? ButtonPress : ButtonRelease, button, 0, 0, 0);
#else
	DEBUG_SKIPPED_INPUT(debug_keyboard, "button: no-XTRAP-build");
#endif
}

void XTestFakeButtonEvent_wr(Display* dpy, unsigned int button, Bool is_press,
    unsigned long delay) {

	if (xtrap_input) {
		return XTRAP_FakeButtonEvent_wr(dpy, button, is_press, delay);
	}

	if (! xtest_present) {
		DEBUG_SKIPPED_INPUT(debug_keyboard, "button: no-XTEST");
		return;
	}
	if (debug_pointer) {
		rfbLog("calling XTestFakeButtonEvent(%d, %d)  %.4f\n",
		    button, is_press, dnow() - x11vnc_start);	
	}
#if LIBVNCSERVER_HAVE_XTEST
    	XTestFakeButtonEvent(dpy, button, is_press, delay);
#endif
}

void XTRAP_FakeMotionEvent_wr(Display* dpy, int screen, int x, int y,
    unsigned long delay) {

	if (! xtrap_present) {
		DEBUG_SKIPPED_INPUT(debug_keyboard, "motion: no-XTRAP");
		return;
	}
#if LIBVNCSERVER_HAVE_LIBXTRAP
	XESimulateXEventRequest(trap_ctx, MotionNotify, 0, x, y, 0);
#else
	DEBUG_SKIPPED_INPUT(debug_keyboard, "motion: no-XTRAP-build");
#endif
}

void XTestFakeMotionEvent_wr(Display* dpy, int screen, int x, int y,
    unsigned long delay) {

	if (xtrap_input) {
		return XTRAP_FakeMotionEvent_wr(dpy, screen, x, y, delay);
	}

	if (debug_pointer) {
		rfbLog("calling XTestFakeMotionEvent(%d, %d)  %.4f\n",
		    x, y, dnow() - x11vnc_start);	
	}
#if LIBVNCSERVER_HAVE_XTEST
	XTestFakeMotionEvent(dpy, screen, x, y, delay);
#endif
}

Bool XTestCompareCurrentCursorWithWindow_wr(Display* dpy, Window w) {
	if (! xtest_present) {
		return False;
	}
#if LIBVNCSERVER_HAVE_XTEST
	return XTestCompareCurrentCursorWithWindow(dpy, w);
#else
	return False;
#endif
}

Bool XTestCompareCursorWithWindow_wr(Display* dpy, Window w, Cursor cursor) {
	if (! xtest_present) {
		return False;
	}
#if LIBVNCSERVER_HAVE_XTEST
	return XTestCompareCursorWithWindow(dpy, w, cursor);
#else
	return False;
#endif
}

Bool XTestQueryExtension_wr(Display *dpy, int *ev, int *er, int *maj,
    int *min) {
#if LIBVNCSERVER_HAVE_XTEST
	return XTestQueryExtension(dpy, ev, er, maj, min);
#else
	return False;
#endif
}

void XTestDiscard_wr(Display *dpy) {
	if (! xtest_present) {
		return;
	}
#if LIBVNCSERVER_HAVE_XTEST
	XTestDiscard(dpy);
#endif
}

Bool XETrapQueryExtension_wr(Display *dpy, int *ev, int *er, int *op) {
#if LIBVNCSERVER_HAVE_LIBXTRAP
	return XETrapQueryExtension(dpy, (INT32 *)ev, (INT32 *)er,
	    (INT32 *)op);
#else
	return False;
#endif
}

int XTestGrabControl_wr(Display *dpy, Bool impervious) {
	if (! xtest_present) {
		return 0;
	}
#if LIBVNCSERVER_HAVE_XTEST && LIBVNCSERVER_HAVE_XTESTGRABCONTROL
	XTestGrabControl(dpy, impervious);
	return 1;
#else
	return 0;
#endif
}

int XTRAP_GrabControl_wr(Display *dpy, Bool impervious) {
	if (! xtrap_present) {
		return 0;
	}
#if LIBVNCSERVER_HAVE_LIBXTRAP
	  else {
		ReqFlags requests;

		if (! impervious) {
			if (trap_ctx) {
				XEFreeTC(trap_ctx);
			}
			trap_ctx = NULL;
			return 1;
		}

		if (! trap_ctx) {
			trap_ctx = XECreateTC(dpy, 0, NULL);
			if (! trap_ctx) {
				rfbLog("DEC-XTRAP XECreateTC failed.  Watch "
				    "out for XGrabServer from wm's\n");
				return 0;
			}
			XEStartTrapRequest(trap_ctx);
			memset(requests, 0, sizeof(requests));
			BitTrue(requests, X_GrabServer);
			BitTrue(requests, X_UngrabServer);
			XETrapSetRequests(trap_ctx, True, requests);
			XETrapSetGrabServer(trap_ctx, True);
		}
		return 1;
	}
#endif
	return 0;
}

void disable_grabserver(Display *in_dpy, int change) {
	int ok = 0;
	static int didmsg = 0;

	if (! xtrap_input) {
		if (XTestGrabControl_wr(in_dpy, True)) {
			if (change) {
				XTRAP_GrabControl_wr(in_dpy, False);
			}
			if (! didmsg) {
				rfbLog("GrabServer control via XTEST.\n"); 
				didmsg = 1;
			}
			ok = 1;
		} else {
			if (XTRAP_GrabControl_wr(in_dpy, True)) {
				ok = 1;
				if (! didmsg) {
					rfbLog("Using DEC-XTRAP for protection"
					    " from XGrabServer.\n");
					didmsg = 1;
				}
			}
		}
	} else {
		if (XTRAP_GrabControl_wr(in_dpy, True)) {
			if (change) {
				XTestGrabControl_wr(in_dpy, False);
			}
			if (! didmsg) {
				rfbLog("GrabServer control via DEC-XTRAP.\n"); 
				didmsg = 1;
			}
			ok = 1;
		} else {
			if (XTestGrabControl_wr(in_dpy, True)) {
				ok = 1;
				if (! didmsg) {
					rfbLog("DEC-XTRAP XGrabServer "
					    "protection not available, "
					    "using XTEST.\n");
					didmsg = 1;
				}
			}
		}
	}
	if (! ok && ! didmsg) {
		rfbLog("No XTEST or DEC-XTRAP protection from XGrabServer.\n");
		rfbLog("Deadlock if your window manager calls XGrabServer!!\n");
	}
}

Bool XRecordQueryVersion_wr(Display *dpy, int *maj, int *min) {
#if LIBVNCSERVER_HAVE_RECORD
	return XRecordQueryVersion(dpy, maj, min);
#else
	return False;
#endif
}

#if LIBVNCSERVER_HAVE_RECORD
XRecordRange *rr_CA = NULL;
XRecordRange *rr_CW = NULL;
XRecordRange *rr_GS = NULL;
XRecordRange *rr_scroll[10];
XRecordContext rc_scroll;
XRecordClientSpec rcs_scroll;
XRecordRange *rr_grab[10];
XRecordContext rc_grab;
XRecordClientSpec rcs_grab;

void record_grab(XPointer, XRecordInterceptData *);
#endif

int xrecording = 0;
int xrecord_set_by_keys = 0;
int xrecord_set_by_mouse = 0;
Window xrecord_focus_window = None;
Window xrecord_wm_window = None;
Window xrecord_ptr_window = None;
KeySym xrecord_keysym = NoSymbol;
#define NAMEINFO 2048
char xrecord_name_info[NAMEINFO];

char *xerror_string(XErrorEvent *error);
int trap_record_xerror(Display *, XErrorEvent *);
int trapped_record_xerror;
XErrorEvent *trapped_record_xerror_event;

void xrecord_grabserver(int start) {
	XErrorHandler old_handler = NULL;
	int rc;

	if (! gdpy_ctrl || ! gdpy_data) {
		return;
	}
#if LIBVNCSERVER_HAVE_RECORD
	if (!start) {
		if (! rc_grab) {
			return;
		}
		XRecordDisableContext(gdpy_ctrl, rc_grab);
		XRecordFreeContext(gdpy_ctrl, rc_grab);
		XFlush(gdpy_ctrl);
		rc_grab = 0;
		return;
	}

	xserver_grabbed = 0;

	rr_grab[0] = rr_GS;
	rcs_grab = XRecordAllClients;

	rc_grab = XRecordCreateContext(gdpy_ctrl, 0, &rcs_grab, 1, rr_grab, 1);
	trapped_record_xerror = 0;
	old_handler = XSetErrorHandler(trap_record_xerror);

	XSync(gdpy_ctrl, True);

	if (! rc_grab || trapped_record_xerror) {
		XCloseDisplay(gdpy_ctrl);
		XCloseDisplay(gdpy_data);
		gdpy_ctrl = NULL;
		gdpy_data = NULL;
		XSetErrorHandler(old_handler);
		return;
	}
	rc = XRecordEnableContextAsync(gdpy_data, rc_grab, record_grab, NULL);
	if (!rc || trapped_record_xerror) {
		XCloseDisplay(gdpy_ctrl);
		XCloseDisplay(gdpy_data);
		gdpy_ctrl = NULL;
		gdpy_data = NULL;
		XSetErrorHandler(old_handler);
		return;
	}
	XSetErrorHandler(old_handler);
	XFlush(gdpy_data);
#endif
}

void initialize_xrecord(void) {
	use_xrecord = 0;
	if (! xrecord_present) {
		return;
	}
	if (nofb) {
		return;
	}
	if (noxrecord) {
		return;
	}
#if LIBVNCSERVER_HAVE_RECORD

	if (rr_CA) XFree(rr_CA);
	if (rr_CW) XFree(rr_CW);
	if (rr_GS) XFree(rr_GS);

	rr_CA = XRecordAllocRange();
	rr_CW = XRecordAllocRange();
	rr_GS = XRecordAllocRange();
	if (!rr_CA || !rr_CW || !rr_GS) {
		return;
	}
	/* protocol request ranges: */
	rr_CA->core_requests.first = X_CopyArea;
	rr_CA->core_requests.last  = X_CopyArea;
	
	rr_CW->core_requests.first = X_ConfigureWindow;
	rr_CW->core_requests.last  = X_ConfigureWindow;

	rr_GS->core_requests.first = X_GrabServer;
	rr_GS->core_requests.last  = X_UngrabServer;

	X_LOCK;
	/* open a 2nd control connection to DISPLAY: */
	if (rdpy_data) {
		XCloseDisplay(rdpy_data);
		rdpy_data = NULL;
	}
	if (rdpy_ctrl) {
		XCloseDisplay(rdpy_ctrl);
		rdpy_ctrl = NULL;
	}
	rdpy_ctrl = XOpenDisplay(DisplayString(dpy));
	XSync(dpy, True);
	XSync(rdpy_ctrl, True);
	/* open datalink connection to DISPLAY: */
	rdpy_data = XOpenDisplay(DisplayString(dpy));
	if (!rdpy_ctrl || ! rdpy_data) {
		X_UNLOCK;
		return;
	}
	disable_grabserver(rdpy_ctrl, 0);
	disable_grabserver(rdpy_data, 0);

	use_xrecord = 1;

	/*
	 * now set up the GrabServer watcher.  We get GrabServer
	 * deadlock in XRecordCreateContext() even with XTestGrabServer
	 * in place, why?  Not sure, so we manually watch for grabs...
	 */
	if (gdpy_data) {
		XCloseDisplay(gdpy_data);
		gdpy_data = NULL;
	}
	if (gdpy_ctrl) {
		XCloseDisplay(gdpy_ctrl);
		gdpy_ctrl = NULL;
	}
	xserver_grabbed = 0;

	gdpy_ctrl = XOpenDisplay(DisplayString(dpy));
	XSync(dpy, True);
	XSync(gdpy_ctrl, True);
	gdpy_data = XOpenDisplay(DisplayString(dpy));
	if (gdpy_ctrl && gdpy_data) {
		disable_grabserver(gdpy_ctrl, 0);
		disable_grabserver(gdpy_data, 0);
		xrecord_grabserver(1);
	}
	X_UNLOCK;
#endif
}

void shutdown_xrecord(void) {
#if LIBVNCSERVER_HAVE_RECORD

	if (rr_CA) XFree(rr_CA);
	if (rr_CW) XFree(rr_CW);
	if (rr_GS) XFree(rr_GS);

	rr_CA = NULL;
	rr_CW = NULL;
	rr_GS = NULL;

	X_LOCK;
	if (rdpy_ctrl && rc_scroll) {
		XRecordDisableContext(rdpy_ctrl, rc_scroll);
		XRecordFreeContext(rdpy_ctrl, rc_scroll);
		XSync(rdpy_ctrl, False);
		rc_scroll = 0;
	}
		
	if (gdpy_ctrl && rc_grab) {
		XRecordDisableContext(gdpy_ctrl, rc_grab);
		XRecordFreeContext(gdpy_ctrl, rc_grab);
		XSync(gdpy_ctrl, False);
		rc_grab = 0;
	}
		
	if (rdpy_data) {
		XCloseDisplay(rdpy_data);
		rdpy_data = NULL;
	}
	if (rdpy_ctrl) {
		XCloseDisplay(rdpy_ctrl);
		rdpy_ctrl = NULL;
	}
	if (gdpy_data) {
		XCloseDisplay(gdpy_data);
		gdpy_data = NULL;
	}
	if (gdpy_ctrl) {
		XCloseDisplay(gdpy_ctrl);
		gdpy_ctrl = NULL;
	}
	xserver_grabbed = 0;
	X_UNLOCK;
#endif
	use_xrecord = 0;
}

int xrecord_skip_keysym(rfbKeySym keysym) {
	KeySym sym = (KeySym) keysym;
	int ok = -1, matched = 0;

	if (scroll_key_list) {
		int k, exclude = 0;
		if (scroll_key_list[0]) {
			exclude = 1;
		}
		k = 1;
		while (scroll_key_list[k] != NoSymbol) {
			if (scroll_key_list[k++] == sym) {
				matched = 1;
				break;
			}
		}
		if (exclude) {
			if (matched) {
				return 1;
			} else {
				ok = 1;
			}
		} else {
			if (matched) {
				ok = 1;
			} else {
				ok = 0;
			}
		}
	}
	if (ok == 1) {
		return 0;
	} else if (ok == 0) {
		return 1;
	}

	/* apply various heuristics: */

	if (IsModifierKey(sym)) {
		/* Shift, Control, etc, usu. generate no scrolls */
		return 1;
	}
	if (sym == XK_space && scroll_term) {
		/* space in a terminal is usu. full page... */
		Window win;
		static Window prev_top = None;
		int size = 256;
		static char name[256];

		X_LOCK;
		win = query_pointer(rootwin);
		X_UNLOCK;
		if (win != None && win != rootwin) {
			if (prev_top != None && win == prev_top) {
				;	/* use cached result */
			} else {
				prev_top = win;
				X_LOCK;
				win = descend_pointer(6, win, name, size);
				X_UNLOCK;
			}
			if (match_str_list(name, scroll_term)) {
				return 1;
			}
		}
	}

	/* TBD use typing_rate() so */
	return 0;
}

int xrecord_skip_button(int new, int old) {
	return 0;
}

int xrecord_vi_scroll_keysym(rfbKeySym keysym) {
	KeySym sym = (KeySym) keysym;
	if (sym == XK_J || sym == XK_j || sym == XK_K || sym == XK_k) {
		return 1;	/* vi */
	}
	if (sym == XK_D || sym == XK_d || sym == XK_U || sym == XK_u) {
		return 1;	/* Ctrl-d/u */
	}
	if (sym == XK_Z || sym == XK_z) {
		return 1;	/* zz, zt, zb .. */
	}
	return 0;
}

int xrecord_emacs_scroll_keysym(rfbKeySym keysym) {
	KeySym sym = (KeySym) keysym;
	if (sym == XK_N || sym == XK_n || sym == XK_P || sym == XK_p) {
		return 1;	/* emacs */
	}
	/* Must be some more ... */
	return 0;
}

int xrecord_scroll_keysym(rfbKeySym keysym) {
	KeySym sym = (KeySym) keysym;
	/* X11/keysymdef.h */

	if (sym == XK_Return || sym == XK_KP_Enter || sym == XK_Linefeed) {
		return 1;	/* Enter */
	}
	if (sym==XK_Up || sym==XK_KP_Up || sym==XK_Down || sym==XK_KP_Down) {
		return 1;	/* U/D arrows */
	}
	if (sym == XK_Left || sym == XK_KP_Left || sym == XK_Right ||
	    sym == XK_KP_Right) {
		return 1;	/* L/R arrows */
	}
	if (xrecord_vi_scroll_keysym(keysym)) {
		return 1;
	}
	if (xrecord_emacs_scroll_keysym(keysym)) {
		return 1;
	}
	return 0;
}

#define SCR_ATTR_CACHE 8
winattr_t scr_attr_cache[SCR_ATTR_CACHE];
double attr_cache_max_age = 1.5;

int lookup_attr_cache(Window win, int *cache_index, int *next_index) {
	double now, t, oldest;
	int i, old_index = -1, count = 0;
	Window cwin;

	*cache_index = -1;
	*next_index  = -1;
	
	if (win == None) {
		return 0;
	}
	if (attr_cache_max_age == 0.0) {
		return 0;
	}

	dtime0(&now);
	for (i=0; i < SCR_ATTR_CACHE; i++) {

		cwin = scr_attr_cache[i].win;
		t = scr_attr_cache[i].time;

		if (now > t + attr_cache_max_age) {
			/* expire it even if it is the one we want */
			scr_attr_cache[i].win = cwin = None;
			scr_attr_cache[i].fetched = 0;
			scr_attr_cache[i].valid = 0;
		}

		if (*next_index == -1 && cwin == None) {
			*next_index = i;
		}
		if (*next_index == -1) {
			/* record oldest */
			if (old_index == -1 || t < oldest) {
				oldest = t;
				old_index = i;
			}
		}
		if (cwin != None) {
			count++;
		}
		if (cwin == win) {
			if (*cache_index == -1) {
				*cache_index = i;
			} else {
				/* remove dups */
				scr_attr_cache[i].win = None;
				scr_attr_cache[i].fetched = 0;
				scr_attr_cache[i].valid = 0;
			}
		}
	}
	if (*next_index == -1) {
		*next_index = old_index;
	}

if (0) fprintf(stderr, "lookup_attr_cache count: %d\n", count);
	if (*cache_index != -1) {
		return 1;
	} else {
		return 0;
	}
}


typedef struct scroll_event {
	Window win, frame;
	int dx, dy;
	int x, y, w, h;
	double t;
	int win_x, win_y, win_w, win_h;	
	int new_x, new_y, new_w, new_h;	
} scroll_event_t;

#define SCR_EV_MAX 128
scroll_event_t scr_ev[SCR_EV_MAX];
int scr_ev_cnt = 0;

XID xrecord_seq = 0;
double xrecord_start = 0.0;

#if LIBVNCSERVER_HAVE_RECORD
void record_CA(XPointer ptr, XRecordInterceptData *rec_data) {
	xCopyAreaReq *req;
	Window src = None, dst = None, c;
	XWindowAttributes attr;
	int src_x, src_y, dst_x, dst_y, rx, ry;
	int good = 1, dx, dy, k=0, i;
	unsigned int w, h;
	int dba = 0, db = debug_scroll;
	int cache_index, next_index, valid;

	if (dba || db) {
		if (rec_data->category == XRecordFromClient) {
			req = (xCopyAreaReq *) rec_data->data;
			if (req->reqType == X_CopyArea) {
				src = req->srcDrawable;
				dst = req->dstDrawable;
			}
		}
	}

if (dba || db > 1) fprintf(stderr, "record_CA-%d id_base: 0x%lx  ptr: 0x%lx "
	"seq: 0x%lx rc: 0x%lx  cat: %d  swapped: %d 0x%lx/0x%lx\n", k++,
	rec_data->id_base, (unsigned long) ptr, xrecord_seq, rc_scroll,
	rec_data->category, rec_data->client_swapped, src, dst);

	if (! xrecording) {
		return;
	}
if (db > 1) fprintf(stderr, "record_CA-%d\n", k++);

	if (rec_data->id_base == 0) {
		return;
	}
if (db > 1) fprintf(stderr, "record_CA-%d\n", k++);

	if ((XID) ptr != xrecord_seq) {
		return;
	}
if (db > 1) fprintf(stderr, "record_CA-%d\n", k++);

	if (rec_data->category != XRecordFromClient) {
		return;
	}
if (db > 1) fprintf(stderr, "record_CA-%d\n", k++);

	req = (xCopyAreaReq *) rec_data->data;

	if (req->reqType != X_CopyArea) {
		return;
	}
if (db > 1) fprintf(stderr, "record_CA-%d\n", k++);

/*

xterm, gnome-terminal, others.

Note we miss the X_ImageText8 that clears the block cursor.  So there is a
short period of time with a painting error: two cursors, one above the other.

 X_ImageText8 
    draw: 0x8c00017 nChars: 1, gc: 0x8c00013, x: 101, y: 585, chars=' '
 X_ClearArea 
    window: 0x8c00018, x:   2, y: 217, w:  10, h:   5
 X_FillPoly 
    draw: 0x8c00018 gc: 0x8c0000a, shape: 0, coordMode: 0,
 X_FillPoly 
    draw: 0x8c00018 gc: 0x8c0000b, shape: 0, coordMode: 0,
 X_CopyArea 
    src: 0x8c00017, dst: 0x8c00017, gc: 0x8c00013, srcX:  17, srcY:  15, dstX:  17, dstY:   2, w: 480, h: 572
 X_ChangeWindowAttributes 
 X_ClearArea 
    window: 0x8c00017, x:  17, y: 574, w: 480, h:  13
 X_ChangeWindowAttributes 

 */

	src = req->srcDrawable;
	dst = req->dstDrawable;
	src_x = req->srcX;
	src_y = req->srcY;
	dst_x = req->dstX;
	dst_y = req->dstY;
	w = req->width;
	h = req->height;

	if (w*h < scrollcopyrect_min_area) {
		good = 0;
	} else if (!src || !dst) {
		good = 0;
	} else if (src != dst) {
		good = 0;
	} else if (scr_ev_cnt >= SCR_EV_MAX) {
		good = 0;
	}

	dx = dst_x - src_x;
	dy = dst_y - src_y;

	if (dx != 0 && dy != 0) {
		good = 0;
	}

	if (! good) {
		return;
	}
if (db > 1) fprintf(stderr, "record_CA-%d\n", k++);

	/*
	 * after all of the above succeeds, now contact X server.
	 * we try to get away with some caching here.
	 */
	if (lookup_attr_cache(src, &cache_index, &next_index)) {
		i = cache_index;
		attr.x = scr_attr_cache[i].x;
		attr.y = scr_attr_cache[i].y;
		attr.width = scr_attr_cache[i].width;
		attr.height = scr_attr_cache[i].height;
		attr.map_state = scr_attr_cache[i].map_state;
		rx = scr_attr_cache[i].rx;
		ry = scr_attr_cache[i].ry;
		valid = scr_attr_cache[i].valid;

	} else {
		valid = valid_window(src, &attr, 1);

		if (valid) {
			if (!xtranslate(src, rootwin, 0, 0, &rx, &ry, &c, 1)) {
				valid = 0;
			}
		}
		if (next_index >= 0) {
			i = next_index;
			scr_attr_cache[i].win = src;
			scr_attr_cache[i].fetched = 1;
			scr_attr_cache[i].valid = valid;
			scr_attr_cache[i].time = dnow();
			if (valid) {
				scr_attr_cache[i].x = attr.x;
				scr_attr_cache[i].y = attr.y;
				scr_attr_cache[i].width = attr.width;
				scr_attr_cache[i].height = attr.height;
				scr_attr_cache[i].depth = attr.depth;
				scr_attr_cache[i].class = attr.class;
				scr_attr_cache[i].backing_store =
				    attr.backing_store;
				scr_attr_cache[i].map_state = attr.map_state;

				scr_attr_cache[i].rx = rx;
				scr_attr_cache[i].ry = ry;
			}
		}
	}

	if (! valid) {
		return;
	}
if (db > 1) fprintf(stderr, "record_CA-%d\n", k++);

	if (attr.map_state != IsViewable) {
		return;
	}


if (0 || dba || db) {
	double st, dt;
	st = (double) rec_data->server_time/1000.0;
	dt = (dnow() - servertime_diff) - st;
	fprintf(stderr, "record_CA-%d *FOUND_SCROLL: src: 0x%lx dx: %d dy: %d "
	"x: %d y: %d w: %d h: %d st: %.4f %.4f  %.4f\n", k++, src, dx, dy,
	src_x, src_y, w, h, st, dt, dnow() - x11vnc_start);
}

	i = scr_ev_cnt;

	scr_ev[i].win = src;
	scr_ev[i].frame = None;
	scr_ev[i].dx = dx;
	scr_ev[i].dy = dy;
	scr_ev[i].x = rx + dst_x;
	scr_ev[i].y = ry + dst_y;
	scr_ev[i].w = w;
	scr_ev[i].h = h;
	scr_ev[i].t = ((double) rec_data->server_time)/1000.0;
	scr_ev[i].win_x = rx;
	scr_ev[i].win_y = ry;
	scr_ev[i].win_w = attr.width;
	scr_ev[i].win_h = attr.height;
	scr_ev[i].new_x = 0;
	scr_ev[i].new_y = 0;
	scr_ev[i].new_w = 0;
	scr_ev[i].new_h = 0;

	if (dx == 0) {
		if (dy > 0) {
			scr_ev[i].new_x = rx + src_x;
			scr_ev[i].new_y = ry + src_y;
			scr_ev[i].new_w = w;
			scr_ev[i].new_h = dy;
		} else {
			scr_ev[i].new_x = rx + src_x;
			scr_ev[i].new_y = ry + dst_y + h;
			scr_ev[i].new_w = w;
			scr_ev[i].new_h = -dy;
		}
	} else if (dy == 0) {
		if (dx > 0) {
			scr_ev[i].new_x = rx + src_x;
			scr_ev[i].new_y = rx + src_y;
			scr_ev[i].new_w = dx;
			scr_ev[i].new_h = h;
		} else {
			scr_ev[i].new_x = rx + dst_x + w;
			scr_ev[i].new_y = ry + src_y;
			scr_ev[i].new_w = -dx;
			scr_ev[i].new_h = h;
		}
	}

	scr_ev_cnt++;
}

typedef struct cw_event {
	Window win;
	int x, y, w, h;
} cw_event_t;

#define MAX_CW 128
cw_event_t cw_events[MAX_CW];

void record_CW(XPointer ptr, XRecordInterceptData *rec_data) {
	xConfigureWindowReq *req;
	Window win = None, c;
	Window src = None, dst = None;
	XWindowAttributes attr;
	int absent = 0x100000;
	int src_x, src_y, dst_x, dst_y, rx, ry;
	int good = 1, dx, dy, k=0, i, j, match, list[3];
	int f_x, f_y, f_w, f_h;
	int x, y, w, h;
	int x0, y0, w0, h0, x1, y1, w1, h1, x2, y2, w2, h2;
	static int index = 0;
	unsigned long vals[4];
	unsigned tmask;
	char *data;
	int dba = 0, db = debug_scroll;
	int cache_index, next_index, valid;

	if (db) {
		if (rec_data->category == XRecordFromClient) {
			req = (xConfigureWindowReq *) rec_data->data;
			if (req->reqType == X_ConfigureWindow) {
				src = req->window;
			}
		}
	}

if (dba || db > 1) fprintf(stderr, "record_CW-%d id_base: 0x%lx  ptr: 0x%lx "
	"seq: 0x%lx rc: 0x%lx  cat: %d  swapped: %d 0x%lx/0x%lx\n", k++,
	rec_data->id_base, (unsigned long) ptr, xrecord_seq, rc_scroll,
	rec_data->category, rec_data->client_swapped, src, dst);


	if (! xrecording) {
		return;
	}
if (db > 1) fprintf(stderr, "record_CW-%d\n", k++);

	if ((XID) ptr != xrecord_seq) {
		return;
	}
if (db > 1) fprintf(stderr, "record_CW-%d\n", k++);

	if (rec_data->id_base == 0) {
		return;
	}
if (db > 1) fprintf(stderr, "record_CW-%d\n", k++);

	if (rec_data->category == XRecordStartOfData) {
		index = 0;
		return;
	}
if (db > 1) fprintf(stderr, "record_CW-%d\n", k++);

	if (rec_data->category != XRecordFromClient) {
		return;
	}
if (db > 1) fprintf(stderr, "record_CW-%d\n", k++);

	if (rec_data->client_swapped) {
		return;
	}
if (db > 1) fprintf(stderr, "record_CW-%d\n", k++);

	req = (xConfigureWindowReq *) rec_data->data;

	if (req->reqType != X_ConfigureWindow) {
		return;
	}
if (db > 1) fprintf(stderr, "record_CW-%d\n", k++);

	tmask = req->mask;

	tmask &= ~CWX;
	tmask &= ~CWY;
	tmask &= ~CWWidth;
	tmask &= ~CWHeight;

	if (tmask) {
		/* require no more than these 4 flags */
		return;
	}
if (db > 1) fprintf(stderr, "record_CW-%d\n", k++);

	f_x = req->mask & CWX;
	f_y = req->mask & CWY;
	f_w = req->mask & CWWidth;
	f_h = req->mask & CWHeight;

	if (! f_x || ! f_y)  {
		if (f_w && f_h) {
			;	/* netscape 4.x style */
		} else {
			return;
		}
	}
if (db > 1) fprintf(stderr, "record_CW-%d\n", k++);

	if ( (f_w && !f_h) || (!f_w && f_h) ) {
		return;
	}
if (db > 1) fprintf(stderr, "record_CW-%d\n", k++);
		
	for (i=0; i<4; i++) {
		vals[i] = 0;
	}

	data = (char *)req;
	data += sz_xConfigureWindowReq;

	for (i=0; i<req->length; i++) {
		unsigned long v;
		v = *( (unsigned long *) data);
		vals[i] = v;
		data += 4;
	}


	if (index >= MAX_CW) {
		int i, j;

		/* FIXME, circular, etc. */
		for (i=0; i<2; i++) {
			j = MAX_CW - 2 + i;
			cw_events[i].win = cw_events[j].win;
			cw_events[i].x = cw_events[j].x;
			cw_events[i].y = cw_events[j].y;
			cw_events[i].w = cw_events[j].w;
			cw_events[i].h = cw_events[j].h;
		}
		index = 2;
	}

	if (! f_x && ! f_y) {
		/* netscape 4.x style  CWWidth,CWHeight */
		vals[2] = vals[0];
		vals[3] = vals[1];
		vals[0] = 0;
		vals[1] = 0;
	}

	cw_events[index].win = req->window;

	if (! f_x) {
		cw_events[index].x = absent;
	} else {
		cw_events[index].x = (int) vals[0];
	}
	if (! f_y) {
		cw_events[index].y = absent;
	} else {
		cw_events[index].y = (int) vals[1];
	}

	if (! f_w) {
		cw_events[index].w = absent;
	} else {
		cw_events[index].w = (int) vals[2];
	}
	if (! f_h) {
		cw_events[index].h = absent;
	} else {
		cw_events[index].h = (int) vals[3];
	}

	x = cw_events[index].x;
	y = cw_events[index].y;
	w = cw_events[index].w;
	h = cw_events[index].h;
	win = cw_events[index].win;

if (dba || db) fprintf(stderr, "  record_CW ind: %d win: 0x%lx x: %d y: %d w: %d h: %d\n",
	index, win, x, y, w, h);

	index++;

	if (index < 3) {
		good = 0;
	} else if (w != absent && h != absent &&
	    w*h < scrollcopyrect_min_area) {
		good = 0;
	}

	if (! good) {
		return;
	}
if (db > 1) fprintf(stderr, "record_CW-%d\n", k++);

	match = 0;
	for (j=index - 1; j >= 0; j--) {
		if (cw_events[j].win == win) {
			list[match++] = j;
		}
		if (match >= 3) {
			break;
		}
	}

	if (match != 3) {
		return;
	}
if (db > 1) fprintf(stderr, "record_CW-%d\n", k++);

/*

Mozilla:

Up arrow: window moves down a bit (dy > 0):

 X_ConfigureWindow 
    length: 7, window: 0x2e000cd, mask: 0xf, v0 0,  v1 -18,  v2 760,  v3 906,  v4 327692,  v5 48234701,  v6 3, 
        CW-mask: CWX,CWY,CWWidth,CWHeight,
 X_ConfigureWindow 
    length: 5, window: 0x2e000cd, mask: 0x3, v0 0,  v1 0,  v2 506636,  v3 48234701,  v4 48234511, 
        CW-mask: CWX,CWY,
 X_ConfigureWindow 
    length: 7, window: 0x2e000cd, mask: 0xf, v0 0,  v1 0,  v2 760,  v3 888,  v4 65579,  v5 0,  v6 108009, 
        CW-mask: CWX,CWY,CWWidth,CWHeight,

Down arrow: window moves up a bit (dy < 0):

 X_ConfigureWindow 
    length: 7, window: 0x2e000cd, mask: 0xf, v0 0,  v1 0,  v2 760,  v3 906,  v4 327692,  v5 48234701,  v6 262147, 
        CW-mask: CWX,CWY,CWWidth,CWHeight,
 X_ConfigureWindow 
    length: 5, window: 0x2e000cd, mask: 0x3, v0 0,  v1 -18,  v2 506636,  v3 48234701,  v4 48234511, 
        CW-mask: CWX,CWY,
 X_ConfigureWindow 
    length: 7, window: 0x2e000cd, mask: 0xf, v0 0,  v1 0,  v2 760,  v3 888,  v4 96555,  v5 48265642,  v6 48265262, 
        CW-mask: CWX,CWY,CWWidth,CWHeight,


Netscape 4.x

Up arrow:
71.76142   0.01984 X_ConfigureWindow
    length: 7, window: 0x9800488, mask: 0xf, v0 0,  v1 -15,  v2 785,  v3 882,  v4 327692,  v5 159384712,  v6 1769484,
        CW-mask: CWX,CWY,CWWidth,CWHeight,
71.76153   0.00011 X_ConfigureWindow
    length: 5, window: 0x9800488, mask: 0xc, v0 785,  v1 867,  v2 329228,  v3 159384712,  v4 159383555,
        CW-mask:       CWWidth,CWHeight,
                XXX,XXX
71.76157   0.00003 X_ConfigureWindow
    length: 5, window: 0x9800488, mask: 0x3, v0 0,  v1 0,  v2 131132,  v3 159385313,  v4 328759,
        CW-mask: CWX,CWY,
                         XXX,XXX

Down arrow:
72.93147   0.01990 X_ConfigureWindow
    length: 5, window: 0x9800488, mask: 0xc, v0 785,  v1 882,  v2 328972,  v3 159384712,  v4 159383555,
        CW-mask:       CWWidth,CWHeight,
                XXX,XXX
72.93156   0.00009 X_ConfigureWindow
    length: 5, window: 0x9800488, mask: 0x3, v0 0,  v1 -15,  v2 458764,  v3 159384712,  v4 159383567,
        CW-mask: CWX,CWY,
72.93160   0.00004 X_ConfigureWindow
    length: 7, window: 0x9800488, mask: 0xf, v0 0,  v1 0,  v2 785,  v3 867,  v4 131132,  v5 159385335,  v6 328759,
        CW-mask: CWX,CWY,CWWidth,CWHeight,


sadly, probably need to handle some more...

 */
	x0 = cw_events[list[2]].x;
	y0 = cw_events[list[2]].y;
	w0 = cw_events[list[2]].w;
	h0 = cw_events[list[2]].h;

	x1 = cw_events[list[1]].x;
	y1 = cw_events[list[1]].y;
	w1 = cw_events[list[1]].w;
	h1 = cw_events[list[1]].h;

	x2 = cw_events[list[0]].x;
	y2 = cw_events[list[0]].y;
	w2 = cw_events[list[0]].w;
	h2 = cw_events[list[0]].h;

	/* see NS4 XXX's above: */
	if (w2 == absent || h2 == absent) {
		/* up arrow */
		if (w2 == absent) {
			w2 = w1;
		}
		if (h2 == absent) {
			h2 = h1;
		}
	}
	if (x1 == absent || y1 == absent) {
		/* up arrow */
		if (x1 == absent) {
			x1 = x2;
		}
		if (y1 == absent) {
			y1 = y2;
		}
	}
	if (x0 == absent || y0 == absent) {
		/* down arrow */
		if (x0 == absent) {
			/* hmmm... what to do */
			x0 = x2;
		}
		if (y0 == absent) {
			y0 = y2;
		}
	}

if (dba) fprintf(stderr, "%d/%d/%d/%d  %d/%d/%d/%d  %d/%d/%d/%d\n", x0, y0, w0, h0, x1, y1, w1, h1, x2, y2, w2, h2);

	dy = y1 - y0;
	dx = x1 - x0;

	src_x = x2;
	src_y = y2;
	w = w2;
	h = h2;

	/* check w and h before we modify them */
	if (w <= 0 || h <= 0) {
		good = 0;
	} else if (w == absent || h == absent) {
		good = 0;
	}
	if (! good) {
		return;
	}
if (db > 1) fprintf(stderr, "record_CW-%d\n", k++);

	if (dy > 0) {
		h -= dy;	
	} else {
		h += dy;	
		src_y -= dy;
	}
	if (dx > 0) {
		w -= dx;	
	} else {
		w += dx;	
		src_x -= dx;
	}

	dst_x = src_x + dx;
	dst_y = src_y + dy;

	if (x0 == absent || x1 == absent || x2 == absent) {
		good = 0;
	} else if (y0 == absent || y1 == absent || y2 == absent) {
		good = 0;
	} else if (dx != 0 && dy != 0) {
		good = 0;
	} else if (w0 - w2 != nabs(dx)) {
		good = 0;
	} else if (h0 - h2 != nabs(dy)) {
		good = 0;
	} else if (scr_ev_cnt >= SCR_EV_MAX) {
		good = 0;
	}

	if (! good) {
		return;
	}
if (db > 1) fprintf(stderr, "record_CW-%d\n", k++);

	/*
	 * geometry OK.
	 * after all of the above succeeds, now contact X server.
	 */
	if (lookup_attr_cache(win, &cache_index, &next_index)) {
		i = cache_index;
		attr.x = scr_attr_cache[i].x;
		attr.y = scr_attr_cache[i].y;
		attr.width = scr_attr_cache[i].width;
		attr.height = scr_attr_cache[i].height;
		attr.map_state = scr_attr_cache[i].map_state;
		rx = scr_attr_cache[i].rx;
		ry = scr_attr_cache[i].ry;
		valid = scr_attr_cache[i].valid;

if (0) fprintf(stderr, "lookup_attr_cache hit:  %2d %2d 0x%lx %d\n",
    cache_index, next_index, win, valid);

	} else {
		valid = valid_window(win, &attr, 1);

if (0) fprintf(stderr, "lookup_attr_cache MISS: %2d %2d 0x%lx %d\n",
    cache_index, next_index, win, valid);

		if (valid) {
			if (!xtranslate(win, rootwin, 0, 0, &rx, &ry, &c, 1)) {
				valid = 0;
			}
		}
		if (next_index >= 0) {
			i = next_index;
			scr_attr_cache[i].win = win;
			scr_attr_cache[i].fetched = 1;
			scr_attr_cache[i].valid = valid;
			scr_attr_cache[i].time = dnow();
			if (valid) {
				scr_attr_cache[i].x = attr.x;
				scr_attr_cache[i].y = attr.y;
				scr_attr_cache[i].width = attr.width;
				scr_attr_cache[i].height = attr.height;
				scr_attr_cache[i].depth = attr.depth;
				scr_attr_cache[i].class = attr.class;
				scr_attr_cache[i].backing_store =
				    attr.backing_store;
				scr_attr_cache[i].map_state = attr.map_state;

				scr_attr_cache[i].rx = rx;
				scr_attr_cache[i].ry = ry;
			}
		}
	}

	if (! valid) {
		return;
	}
if (db > 1) fprintf(stderr, "record_CW-%d\n", k++);

	if (attr.map_state != IsViewable) {
		return;
	}
if (db > 1) fprintf(stderr, "record_CW-%d\n", k++);

if (0 || dba || db) {
	double st, dt;
	st = (double) rec_data->server_time/1000.0;
	dt = (dnow() - servertime_diff) - st;
	fprintf(stderr, "record_CW-%d *FOUND_SCROLL: win: 0x%lx dx: %d dy: %d "
	"x: %d y: %d w: %d h: %d  st: %.4f  dt: %.4f  %.4f\n", k++, win,
	dx, dy, src_x, src_y, w, h, st, dt, dnow() - x11vnc_start);
}

	i = scr_ev_cnt;

	scr_ev[i].win = win;
	scr_ev[i].frame = None;
	scr_ev[i].dx = dx;
	scr_ev[i].dy = dy;
	scr_ev[i].x = rx + dst_x;
	scr_ev[i].y = ry + dst_y;
	scr_ev[i].w = w;
	scr_ev[i].h = h;
	scr_ev[i].t = ((double) rec_data->server_time)/1000.0;
	scr_ev[i].win_x = rx;
	scr_ev[i].win_y = ry;
	scr_ev[i].win_w = attr.width;
	scr_ev[i].win_h = attr.height;
	scr_ev[i].new_x = 0;
	scr_ev[i].new_y = 0;
	scr_ev[i].new_w = 0;
	scr_ev[i].new_h = 0;

	if (dx == 0) {
		if (dy > 0) {
			scr_ev[i].new_x = rx + src_x;
			scr_ev[i].new_y = ry + src_y;
			scr_ev[i].new_w = w;
			scr_ev[i].new_h = dy;
		} else {
			scr_ev[i].new_x = rx + src_x;
			scr_ev[i].new_y = ry + dst_y + h;
			scr_ev[i].new_w = w;
			scr_ev[i].new_h = -dy;
		}
	} else if (dy == 0) {
		if (dx > 0) {
			scr_ev[i].new_x = rx + src_x;
			scr_ev[i].new_y = rx + src_y;
			scr_ev[i].new_w = dx;
			scr_ev[i].new_h = h;
		} else {
			scr_ev[i].new_x = rx + dst_x + w;
			scr_ev[i].new_y = ry + src_y;
			scr_ev[i].new_w = -dx;
			scr_ev[i].new_h = h;
		}
	}

	/* indicate we have a new one */
	scr_ev_cnt++;

	index = 0;
}

void record_switch(XPointer ptr, XRecordInterceptData *rec_data) {
	static int first = 1;
	xReq *req;

	if (first) {
		int i;
		for (i=0; i<SCR_ATTR_CACHE; i++) {
			scr_attr_cache[i].win = None;
			scr_attr_cache[i].fetched = 0;
			scr_attr_cache[i].valid = 0;
			scr_attr_cache[i].time = 0.0;
		}
		first = 0;
	}

	/* should handle control msgs, start/stop/etc */
	if (rec_data->category == XRecordStartOfData) {
		record_CW(ptr, rec_data);
	} else if (rec_data->category == XRecordEndOfData) {
		;
	} else if (rec_data->category == XRecordClientStarted) {
		;
	} else if (rec_data->category == XRecordClientDied) {
		;
	} else if (rec_data->category == XRecordFromServer) {
		;
	}

	if (rec_data->category != XRecordFromClient) {
		XRecordFreeData(rec_data);
		return;
	}

	req = (xReq *) rec_data->data;

	if (req->reqType == X_CopyArea) {
		record_CA(ptr, rec_data);
	} else if (req->reqType == X_ConfigureWindow) {
		record_CW(ptr, rec_data);
	} else {
		;
	}
	XRecordFreeData(rec_data);
}

void record_grab(XPointer ptr, XRecordInterceptData *rec_data) {
	xReq *req;

	/* should handle control msgs, start/stop/etc */
	if (rec_data->category == XRecordStartOfData) {
		;
	} else if (rec_data->category == XRecordEndOfData) {
		;
	} else if (rec_data->category == XRecordClientStarted) {
		;
	} else if (rec_data->category == XRecordClientDied) {
		;
	} else if (rec_data->category == XRecordFromServer) {
		;
	}

	if (rec_data->category != XRecordFromClient) {
		XRecordFreeData(rec_data);
		return;
	}

	req = (xReq *) rec_data->data;

	if (req->reqType == X_GrabServer) {
		double now = dnow() - x11vnc_start;
		xserver_grabbed++;
		if (0) rfbLog("X server Grabbed:    %d %.5f\n", xserver_grabbed, now);
		if (xserver_grabbed > 1) {
			/* 
			 * some apps do multiple grabs... very unlikely
			 * two apps will be doing it at same time.
			 */
			xserver_grabbed = 1;
		}
	} else if (req->reqType == X_UngrabServer) {
		double now = dnow() - x11vnc_start;
		xserver_grabbed--;
		if (xserver_grabbed < 0) {
			xserver_grabbed = 0;
		}
		if (0) rfbLog("X server Un-Grabbed: %d %.5f\n", xserver_grabbed, now);
	} else {
		;
	}
	XRecordFreeData(rec_data);
}
#endif

void check_xrecord_grabserver(void) {
	int last_val, cnt = 0, i, max = 10;
	double d;
#if LIBVNCSERVER_HAVE_RECORD
	if (!gdpy_ctrl || !gdpy_data) {
		return;
	}

if (0)	dtime0(&d);
	XFlush(gdpy_ctrl);
	for (i=0; i<max; i++) {
		last_val = xserver_grabbed;
		XRecordProcessReplies(gdpy_data);
		if (xserver_grabbed != last_val) {
			cnt++;
		} else if (i > 2) {
			break;
		}
	}
	if (cnt) {
		XFlush(gdpy_ctrl);
	}
if (0) {
	d = dtime(&d);
fprintf(stderr, "check_xrecord_grabserver: cnt=%d i=%d %.4f\n", cnt, i, d);
}
#endif
}

#if LIBVNCSERVER_HAVE_RECORD
void shutdown_record_context(XRecordContext rc, int bequiet, int reopen) {
	int ret1, ret2;
	int verb = (!bequiet && !quiet);

	if (0 || debug_scroll) {
		rfbLog("shutdown_record_context(0x%lx, %d, %d)\n", rc,
		    bequiet, reopen);
		verb = 1;
	}

	ret1 = XRecordDisableContext(rdpy_ctrl, rc);
	if (!ret1 && verb) {
		rfbLog("XRecordDisableContext(0x%lx) failed.\n", rc);	
	}
	ret2 = XRecordFreeContext(rdpy_ctrl, rc);
	if (!ret2 && verb) {
		rfbLog("XRecordFreeContext(0x%lx) failed.\n", rc);	
	}
	XFlush(rdpy_ctrl);

	if (reopen == 2 && ret1 && ret2) {
		reopen = 0;	/* 2 means reopen only on failure  */
	}
	if (reopen && gdpy_ctrl) {
		check_xrecord_grabserver();
		if (xserver_grabbed) {
			rfbLog("shutdown_record_context: skip reopen,"
			    " server grabbed\n");	
			reopen = 0;
		}
	}
	if (reopen) {
		char *dpystr = DisplayString(dpy);

		if (debug_scroll) {
			rfbLog("closing RECORD data connection.\n");
		}
		XCloseDisplay(rdpy_data);
		rdpy_data = NULL;

		if (debug_scroll) {
			rfbLog("closing RECORD control connection.\n");
		}
		XCloseDisplay(rdpy_ctrl);
		rdpy_ctrl = NULL;

		rdpy_ctrl = XOpenDisplay(dpystr);

		if (! rdpy_ctrl) {
			rfbLog("Failed to reopen RECORD control connection:"
			    "%s\n", dpystr);
			rfbLog("  disabling RECORD scroll detection.\n");
			use_xrecord = 0;
			return;
		}
		XSync(dpy, False);

		disable_grabserver(rdpy_ctrl, 0);
		XSync(rdpy_ctrl, True);

		rdpy_data = XOpenDisplay(dpystr);

		if (! rdpy_data) {
			rfbLog("Failed to reopen RECORD data connection:"
			    "%s\n", dpystr);
			rfbLog("  disabling RECORD scroll detection.\n");
			XCloseDisplay(rdpy_ctrl);
			rdpy_ctrl = NULL;
			use_xrecord = 0;
			return;
		}
		disable_grabserver(rdpy_data, 0);

		if (debug_scroll || (! bequiet && reopen == 2)) {
			rfbLog("reopened RECORD data and control display"
			    " connections: %s\n", dpystr);
		}
	}
}
#endif

void check_xrecord_reset(void) {
	static double last_reset = 0.0;
	int reset_time  = 120, reset_idle  = 15;
	int reset_time2 = 600, reset_idle2 = 40;
	double now;
	XErrorHandler old_handler = NULL;

	if (gdpy_ctrl) {
		X_LOCK;
		check_xrecord_grabserver();
		X_UNLOCK;
	} else {
		/* more dicey if not watching grabserver */
		reset_time = reset_time2;
		reset_idle = reset_idle2;
	}

	if (!use_xrecord) {
		return;
	}
	if (xrecording) {
		return;
	}
	if (button_mask) {
		return;
	}
	if (xserver_grabbed) {
		return;
	}

#if LIBVNCSERVER_HAVE_RECORD
	if (! rc_scroll) {
		return;
	}
	now = dnow();
	if (last_reset == 0.0) {
		last_reset = now;
		return;
	}
	/*
	 * try to wait for a break in input to reopen the displays
	 * this is only to avoid XGrabServer deadlock on the repopens.
	 */
	if (now < last_reset + reset_time) {
		return;
	}
	if (now < last_pointer_click_time + reset_idle)  {
		return;
	}
	if (now < last_keyboard_time + reset_idle)  {
		return;
	}
	X_LOCK;
	trapped_record_xerror = 0;
	old_handler = XSetErrorHandler(trap_record_xerror);

	/* unlikely, but check again since we will definitely be doing it. */
	if (gdpy_ctrl) {
		check_xrecord_grabserver();
		if (xserver_grabbed) {
			XSetErrorHandler(old_handler);
			X_UNLOCK;
			return;
		}
	}
	
	shutdown_record_context(rc_scroll, 0, 1);
	rc_scroll = 0;

	XSetErrorHandler(old_handler);
	X_UNLOCK;

	last_reset = now;
#endif
}

#define RECORD_ERROR_MSG \
	if (! quiet) { \
		rfbLog("trapped RECORD XError: %s %d/%d/%d (0x%lx)\n", \
		    xerror_string(trapped_record_xerror_event), \
		    (int) trapped_record_xerror_event->error_code, \
		    (int) trapped_record_xerror_event->request_code, \
		    (int) trapped_record_xerror_event->minor_code, \
		    (int) trapped_record_xerror_event->resourceid); \
	}

void xrecord_watch(int start, int setby) {
	Window focus, wm, c, clast;
	static double create_time = 0.0;
	double now;
	static double last_error = 0.0;
	int rc, db = debug_scroll;
	int do_shutdown = 0;
	int reopen_dpys = 1;
	XErrorHandler old_handler = NULL;
	static Window last_win = None, last_result = None;

if (0) db = 1;

	if (nofb) {
		xrecording = 0;
		return;
	}
	if (use_threads) {
		/* XXX not working */
		use_xrecord = 0;
		xrecording = 0;
		return;
	}

	dtime0(&now);
	if (now < last_error + 0.5) {
		return;
	}

	if (gdpy_ctrl) {
		X_LOCK;
		check_xrecord_grabserver();
		X_UNLOCK;
		if (xserver_grabbed) {
if (db) fprintf(stderr, "xrecord_watch: %d/%d  out xserver_grabbed\n", start, setby);
			return;
		}
	}

#if LIBVNCSERVER_HAVE_RECORD
	if (! start) {
		int shut_reopen = 2, shut_time = 25;
if (db) fprintf(stderr, "XRECORD OFF: %d/%d  %.4f\n", xrecording, setby, now - x11vnc_start);
		xrecording = 0;
		if (! rc_scroll) {
			xrecord_focus_window = None;
			xrecord_wm_window = None;
			xrecord_ptr_window = None;
			xrecord_keysym = NoSymbol;
			rcs_scroll = 0;
			return;
		}

		if (! do_shutdown && now > create_time + shut_time) {
			/* XXX unstable if we keep a RECORD going forever */
			do_shutdown = 1;
		}

		SCR_LOCK;
		
		if (do_shutdown) {
if (db > 1) fprintf(stderr, "=== shutdown-scroll 0x%lx\n", rc_scroll);
			X_LOCK;
			trapped_record_xerror = 0;
			old_handler = XSetErrorHandler(trap_record_xerror);

			shutdown_record_context(rc_scroll, 0, shut_reopen);
			rc_scroll = 0;

			/*
			 * n.b. there is a grabserver issue wrt
			 * XRecordCreateContext() even though rdpy_ctrl
			 * is set imprevious to grabs.  Perhaps a bug
			 * in the X server or library...
			 *
			 * If there are further problems, a thought
			 * to recreate rc_scroll right after the
			 * reopen.
			 */

			if (! use_xrecord) {
				XSetErrorHandler(old_handler);
				X_UNLOCK;
				SCR_UNLOCK;
				return;
			}

			XRecordProcessReplies(rdpy_data);

			if (trapped_record_xerror) {
				RECORD_ERROR_MSG;
				last_error = now;
			}

			XSetErrorHandler(old_handler);
			X_UNLOCK;
			SCR_UNLOCK;

		} else {
			if (rcs_scroll) {
if (db > 1) fprintf(stderr, "=== disab-scroll 0x%lx 0x%lx\n", rc_scroll, rcs_scroll);
				X_LOCK;
				trapped_record_xerror = 0;
				old_handler =
				    XSetErrorHandler(trap_record_xerror);

				rcs_scroll = XRecordCurrentClients;
				XRecordUnregisterClients(rdpy_ctrl, rc_scroll,
				    &rcs_scroll, 1);
				XRecordDisableContext(rdpy_ctrl, rc_scroll);
				XFlush(rdpy_ctrl);
				XRecordProcessReplies(rdpy_data);

				if (trapped_record_xerror) {
					RECORD_ERROR_MSG;

					shutdown_record_context(rc_scroll,
					    0, reopen_dpys);
					rc_scroll = 0;

					last_error = now;

					if (! use_xrecord) {
						XSetErrorHandler(old_handler);
						X_UNLOCK;
						SCR_UNLOCK;
						return;
					}
				}
				XSetErrorHandler(old_handler);
				X_UNLOCK;
			}
		}

		SCR_UNLOCK;
		/*
		 * XXX if we do a XFlush(rdpy_ctrl) here we get:
		 *

		X Error of failed request:  XRecordBadContext
		  Major opcode of failed request:  145 (RECORD)
		  Minor opcode of failed request:  5 (XRecordEnableContext)
		  Context in failed request:  0x2200013
		  Serial number of failed request:  29
		  Current serial number in output stream:  29

		 *
		 * need to figure out what is going on... since it may lead
		 * infrequent failures.
		 */
		xrecord_focus_window = None;
		xrecord_wm_window = None;
		xrecord_ptr_window = None;
		xrecord_keysym = NoSymbol;
		rcs_scroll = 0;
		return;
	}
if (db) fprintf(stderr, "XRECORD ON:  %d/%d  %.4f\n", xrecording, setby, now - x11vnc_start);

	if (xrecording) {
		return;
	}

	if (do_shutdown && rc_scroll) {
		static int didmsg = 0;
		/* should not happen... */
		if (0 || !didmsg) {
			rfbLog("warning: do_shutdown && rc_scroll\n");
			didmsg = 1;
		}
		xrecord_watch(0, SCR_NONE);
	}

	xrecording = 0;
	xrecord_focus_window = None;
	xrecord_wm_window = None;
	xrecord_ptr_window = None;
	xrecord_keysym = NoSymbol;
	xrecord_set_by_keys  = 0;
	xrecord_set_by_mouse = 0;

	/* get the window with focus and mouse pointer: */
	clast = None;
	focus = None;
	wm = None;

	X_LOCK;
	SCR_LOCK;
#if 0
	/*
	 * xrecord_focus_window / focus not currently used... save a
	 * round trip to the X server for now.
	 * N.B. our heuristic is inaccurate: if he is scrolling and
	 * drifts off of the scrollbar onto another application we
	 * will catch that application, not the starting ones.
	 * check_xrecord_{keys,mouse} mitigates this somewhat by
	 * delaying calls to xrecord_watch as much as possible.
	 */
	XGetInputFocus(dpy, &focus, &i);
#endif

	wm = query_pointer(rootwin);
	if (wm) {
		c = wm;
	} else {
		c = rootwin;
	}

	/* descend a bit to avoid wm frames: */
	if (c != rootwin && c == last_win) {
		/* use cached results to avoid roundtrips: */
		clast = last_result;
	} else if (scroll_good_all == NULL && scroll_skip_all == NULL) {
		/* more efficient if name info not needed. */
		xrecord_name_info[0] = '\0';
		clast = descend_pointer(6, c, NULL, 0);
	} else {
		char *nm;
		int matched_good = 0, matched_skip = 0;

		clast = descend_pointer(6, c, xrecord_name_info, NAMEINFO);
if (db) fprintf(stderr, "name_info: %s\n", xrecord_name_info);

		nm = xrecord_name_info;

		if (scroll_good_all) {
			matched_good += match_str_list(nm, scroll_good_all);
		}
		if (setby == SCR_KEY && scroll_good_key) {
			matched_good += match_str_list(nm, scroll_good_key);
		}
		if (setby == SCR_MOUSE && scroll_good_mouse) {
			matched_good += match_str_list(nm, scroll_good_mouse);
		}
		if (scroll_skip_all) {
			matched_skip += match_str_list(nm, scroll_skip_all);
		}
		if (setby == SCR_KEY && scroll_skip_key) {
			matched_skip += match_str_list(nm, scroll_skip_key);
		}
		if (setby == SCR_MOUSE && scroll_skip_mouse) {
			matched_skip += match_str_list(nm, scroll_skip_mouse);
		}

		if (!matched_good && matched_skip) {
			clast = None;
		}
	}
	if (c != rootwin) {
		/* cache results for possible use next call */
		last_win = c;
		last_result = clast;
	}

	if (!clast || clast == rootwin) {
if (db) fprintf(stderr, "--- xrecord_watch: SKIP.\n");
		X_UNLOCK;
		SCR_UNLOCK;
		return;
	}

	/* set protocol request ranges: */
	rr_scroll[0] = rr_CA;
	rr_scroll[1] = rr_CW;

	/*
	 * start trapping... there still are some occasional failures
	 * not yet understood, likely some race condition WRT the 
	 * context being setup.
	 */
	trapped_record_xerror = 0;
	old_handler = XSetErrorHandler(trap_record_xerror);

	if (! rc_scroll) {
		/* do_shutdown case or first time in */

		if (gdpy_ctrl) {
			/*
			 * Even though rdpy_ctrl is impervious to grabs
			 * at this point, we still get deadlock, why?
			 * It blocks in the library find_display() call.
			 */
			check_xrecord_grabserver();
			if (xserver_grabbed) {
				XSetErrorHandler(old_handler);
				X_UNLOCK;
				SCR_UNLOCK;
				return;
			}
		}
		rcs_scroll = (XRecordClientSpec) clast;
		rc_scroll = XRecordCreateContext(rdpy_ctrl, 0, &rcs_scroll, 1,
		    rr_scroll, 2);

		if (! do_shutdown) {
			XSync(rdpy_ctrl, False);
		}
if (db) fprintf(stderr, "NEW rc:    0x%lx\n", rc_scroll);
		if (rc_scroll) {
			dtime0(&create_time);
		} else {
			rcs_scroll = 0;
		}

	} else if (! do_shutdown) {
		if (rcs_scroll) {
			/*
			 * should have been unregistered in xrecord_watch(0)...
			 */
			rcs_scroll = XRecordCurrentClients;
			XRecordUnregisterClients(rdpy_ctrl, rc_scroll,
			    &rcs_scroll, 1);

if (db > 1) fprintf(stderr, "=2= unreg-scroll 0x%lx 0x%lx\n", rc_scroll, rcs_scroll);

		}
		
		rcs_scroll = (XRecordClientSpec) clast;

if (db > 1) fprintf(stderr, "=-=   reg-scroll 0x%lx 0x%lx\n", rc_scroll, rcs_scroll);

		if (!XRecordRegisterClients(rdpy_ctrl, rc_scroll, 0,
		    &rcs_scroll, 1, rr_scroll, 2)) {
			if (1 || now > last_error + 60) {
				rfbLog("failed to register client 0x%lx with"
				    " X RECORD context rc_scroll.\n", clast);
			}
			last_error = now;
			rcs_scroll = 0;
			/* continue on for now... */
		}
	}

	XFlush(rdpy_ctrl);

if (db) fprintf(stderr, "rc_scroll: 0x%lx\n", rc_scroll);
	if (trapped_record_xerror) {
		RECORD_ERROR_MSG;
	}

	if (! rc_scroll) {
		XSetErrorHandler(old_handler);
		X_UNLOCK;
		SCR_UNLOCK;
		use_xrecord = 0;
		rfbLog("failed to create X RECORD context rc_scroll.\n");
		rfbLog("  switching to -noscrollcopyrect mode.\n");
		return;
	} else if (! rcs_scroll || trapped_record_xerror) {
		/* try again later */
		shutdown_record_context(rc_scroll, 0, reopen_dpys);
		rc_scroll = 0;
		last_error = now;

		XSetErrorHandler(old_handler);
		X_UNLOCK;
		SCR_UNLOCK;
		return;
	}

	xrecord_focus_window = focus;
#if 0
	/* xrecord_focus_window currently unused. */
	if (! xrecord_focus_window) {
		xrecord_focus_window = clast;
	}
#endif
	xrecord_wm_window = wm;
	if (! xrecord_wm_window) {
		xrecord_wm_window = clast;
	}

	xrecord_ptr_window = clast;

	xrecording = 1;
	xrecord_seq++;
	dtime0(&xrecord_start);

	rc = XRecordEnableContextAsync(rdpy_data, rc_scroll, record_switch,
	    (XPointer) xrecord_seq);

	if (!rc || trapped_record_xerror) {
		if (1 || now > last_error + 60) {
			rfbLog("failed to enable RECORD context "
			    "rc_scroll: 0x%lx rc: %d\n", rc_scroll, rc);
			if (trapped_record_xerror) {
				RECORD_ERROR_MSG;
			}
		}
		shutdown_record_context(rc_scroll, 0, reopen_dpys);
		rc_scroll = 0;
		last_error = now;
		xrecording = 0;
		/* continue on for now... */
	}
	XSetErrorHandler(old_handler);

	/* XXX this may cause more problems than it solves... */
	if (use_xrecord) {
		XFlush(rdpy_data);
	}

	X_UNLOCK;
	SCR_UNLOCK;
#endif
}

/* -- cleanup.c -- */
/*
 * Exiting and error handling routines
 */

static int exit_flag = 0;
int exit_sig = 0;

void clean_shm(int quick) {
	int i, cnt = 0;

	if (raw_fb) quick = 1;	/* raw_fb hack */

	/*
	 * to avoid deadlock, etc, under quick=1 we just delete the shm
	 * areas and leave the X stuff hanging.
	 */
	if (quick) {
		shm_delete(&scanline_shm);
		shm_delete(&fullscreen_shm);
		shm_delete(&snaprect_shm);
	} else {
		shm_clean(&scanline_shm, scanline);
		shm_clean(&fullscreen_shm, fullscreen);
		shm_clean(&snaprect_shm, snaprect);
	}

	/* 
	 * Here we have to clean up quite a few shm areas for all
	 * the possible tile row runs (40 for 1280), not as robust
	 * as one might like... sometimes need to run ipcrm(1). 
	 */
	for(i=1; i<=ntiles_x; i++) {
		if (i > tile_shm_count) {
			break;
		}
		if (quick) {
			shm_delete(&tile_row_shm[i]);
		} else {
			shm_clean(&tile_row_shm[i], tile_row[i]);
		}
		cnt++;
		if (single_copytile_count && i >= single_copytile_count) {
			break;
		}
	}
	if (!quiet) {
		rfbLog("deleted %d tile_row polling images.\n", cnt);
	}
}

/*
 * Normal exiting
 */
void clean_up_exit (int ret) {
	exit_flag = 1;

	/* remove the shm areas: */
	clean_shm(0);

	if (! dpy) exit(ret);	/* raw_rb hack */

	/* X keyboard cleanups */
	delete_added_keycodes(0);

	if (clear_mods == 1) {
		clear_modifiers(0);
	} else if (clear_mods == 2) {
		clear_keys();
	}

	if (no_autorepeat) {
		autorepeat(1, 0);
	}
	if (use_solid_bg) {
		solid_bg(1);
	}
	X_LOCK;
	XTestDiscard_wr(dpy);
#if LIBVNCSERVER_HAVE_LIBXDAMAGE
	if (xdamage) {
		XDamageDestroy(dpy, xdamage);
	}
#endif
#if LIBVNCSERVER_HAVE_LIBXTRAP
	if (trap_ctx) {
		XEFreeTC(trap_ctx);
	}
#endif
	/* XXX rdpy_ctrl, etc. cannot close w/o blocking */
	XCloseDisplay(dpy);
	X_UNLOCK;

	fflush(stderr);
	exit(ret);
}

/* X11 error handlers */

static XErrorHandler   Xerror_def;
static XIOErrorHandler XIOerr_def;
XErrorEvent *trapped_xerror_event;
int trapped_xerror = 0;
int trapped_xioerror = 0;
int trapped_getimage_xerror = 0;
int trapped_record_xerror = 0;

int trap_xerror(Display *d, XErrorEvent *error) {
	trapped_xerror = 1;
	trapped_xerror_event = error;
	return 0;
}

int trap_xioerror(Display *d) {
	trapped_xioerror = 1;
	return 0;
}

int trap_getimage_xerror(Display *d, XErrorEvent *error) {
	trapped_getimage_xerror = 1;
	trapped_xerror_event = error;
	return 0;
}

int trap_record_xerror(Display *d, XErrorEvent *error) {
	trapped_record_xerror = 1;
	trapped_record_xerror_event = error;
	return 0;
}

void interrupted(int);

static int Xerror(Display *d, XErrorEvent *error) {
	X_UNLOCK;
	interrupted(0);
	return (*Xerror_def)(d, error);
}

static int XIOerr(Display *d) {
	X_UNLOCK;
	interrupted(-1);
	return (*XIOerr_def)(d);
}

char *xerrors[] = {
	"Success",
	"BadRequest",
	"BadValue",
	"BadWindow",
	"BadPixmap",
	"BadAtom",
	"BadCursor",
	"BadFont",
	"BadMatch",
	"BadDrawable",
	"BadAccess",
	"BadAlloc",
	"BadColor",
	"BadGC",
	"BadIDChoice",
	"BadName",
	"BadLength",
	"BadImplementation",
	"unknown"
};
int xerrors_max = BadImplementation;

char *xerror_string(XErrorEvent *error) {
	int index = -1;
	if (error) {
		index = (int) error->error_code;
	}
	if (0 <= index && index <= xerrors_max) {
		return xerrors[index];
	} else {
		return xerrors[xerrors_max+1];
	}
}

char *crash_stack_command1 = NULL;
char *crash_stack_command2 = NULL;
char *crash_debug_command = NULL;
int crash_debug = 1;

void initialize_crash_handler(void) {
	int pid = program_pid;
	crash_stack_command1 = malloc(1000);
	crash_stack_command2 = malloc(1000);
	crash_debug_command = malloc(1000);

	snprintf(crash_stack_command1, 500, "echo where > /tmp/gdb.%d;"
	    " env PATH=$PATH:/usr/local/bin:/usr/sfw/bin:/usr/bin"
	    " gdb -x /tmp/gdb.%d -batch -n %s %d;"
	    " rm -f /tmp/gdb.%d", pid, pid, program_name, pid, pid);
	snprintf(crash_stack_command2, 500, "pstack %d", program_pid);
	
	snprintf(crash_debug_command, 500, "gdb %s %d", program_name, pid);
}

void crash_shell_help(void) {
	int pid = program_pid;
	fprintf(stderr, "\n");
	fprintf(stderr, "   *** Welcome to the x11vnc crash shell! ***\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "PROGRAM: %s  PID: %d\n", program_name, pid);
	fprintf(stderr, "\n");
	fprintf(stderr, "POSSIBLE DEBUGGER COMMAND:\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  %s\n", crash_debug_command);
	fprintf(stderr, "\n");
	fprintf(stderr, "Press \"q\" to quit.\n");
	fprintf(stderr, "Press \"h\" or \"?\" for this help.\n");
	fprintf(stderr, "Press \"s\" to try to run some commands to"
	    " show a stack trace (gdb/pstack).\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Anything else is passed to -Q query function.\n");
	fprintf(stderr, "\n");
}

void crash_shell(void) {
	char qry[1000], cmd[1000], line[1000];
	char *str, *p;

	crash_shell_help();
	fprintf(stderr, "\ncrash> ");
	while (fgets(line, 1000, stdin) != NULL) {
		str = lblanks(line);

		p = str;
		while(*p) {
			if (*p == '\n') {
				*p = '\0';
			}
			p++;
		}

		if (*str == 'q' && *(str+1) == '\0') {
			fprintf(stderr, "quiting.\n");
			return;
		} else if (*str == 'h' && *(str+1) == '\0') {
			crash_shell_help();
		} else if (*str == '?' && *(str+1) == '\0') {
			crash_shell_help();
		} else if (*str == 's' && *(str+1) == '\0') {
			sprintf(cmd, "sh -c '(%s) &'", crash_stack_command1);
			fprintf(stderr, "\nrunning:\n\t%s\n\n",
			    crash_stack_command1);
			system(cmd);
			usleep(1000*1000);

			sprintf(cmd, "sh -c '(%s) &'", crash_stack_command2);
			fprintf(stderr, "\nrunning:\n\t%s\n\n",
			    crash_stack_command2);
			system(cmd);
			usleep(1000*1000);
		} else {
			snprintf(qry, 1000, "qry=%s", str);
			p = process_remote_cmd(qry, 1);
			fprintf(stderr, "\n\nresult:\n%s\n", p);
			free(p);
		}
		
		fprintf(stderr, "crash> ");
	}
}

/*
 * General problem handler
 */
void interrupted (int sig) {
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
	} else if (sig == -1) {
		fprintf(stderr, "caught XIO error:\n");
	} else {
		fprintf(stderr, "caught signal: %d\n", sig);
	}
	if (sig == SIGINT) {
		shut_down = 1;
		return;
	}

	X_UNLOCK;

	/* remove the shm areas with quick=1: */
	clean_shm(1);

	if (sig == -1) {
		/* not worth trying any more cleanup, X server probably gone */
		exit(3);
	}

	/* X keyboard cleanups */
	delete_added_keycodes(0);

	if (clear_mods == 1) {
		clear_modifiers(0);
	} else if (clear_mods == 2) {
		clear_keys();
	}
	if (no_autorepeat) {
		autorepeat(1, 0);
	}
	if (use_solid_bg) {
		solid_bg(1);
	}

	if (crash_debug) {
		crash_shell();
	}

	if (sig) {
		exit(2);
	}
}

/* trapping utility to check for a valid window: */
int valid_window(Window win, XWindowAttributes *attr_ret, int bequiet) {
	XErrorHandler old_handler;
	XWindowAttributes attr, *pattr;
	int ok = 0;

	if (attr_ret == NULL) {
		pattr = &attr;
	} else {
		pattr = attr_ret;
	}

	trapped_xerror = 0;
	old_handler = XSetErrorHandler(trap_xerror);
	if (XGetWindowAttributes(dpy, win, pattr)) {
		ok = 1;
	}
	if (trapped_xerror && trapped_xerror_event) {
		if (! quiet && ! bequiet) {
			rfbLog("valid_window: trapped XError: %s (0x%lx)\n",
			    xerror_string(trapped_xerror_event), win);
		}
		ok = 0;
	}
	XSetErrorHandler(old_handler);
	trapped_xerror = 0;
	
	return ok;
}

Bool xtranslate(Window src, Window dst, int src_x, int src_y, int *dst_x,
    int *dst_y, Window *child, int bequiet) {
	XErrorHandler old_handler;
	Bool ok = False;

	trapped_xerror = 0;
	old_handler = XSetErrorHandler(trap_xerror);
	if (XTranslateCoordinates(dpy, src, dst, src_x, src_y, dst_x,
	    dst_y, child)) {
		ok = True;
	}
	if (trapped_xerror && trapped_xerror_event) {
		if (! quiet && ! bequiet) {
			rfbLog("xtranslate: trapped XError: %s (0x%lx)\n",
			    xerror_string(trapped_xerror_event), src);
		}
		ok = False;
	}
	XSetErrorHandler(old_handler);
	trapped_xerror = 0;
	
	return ok;
}

int wait_until_mapped(Window win) {
	int ms = 50, waittime = 30;
	time_t start = time(0);
	XWindowAttributes attr;

	while (1) {
		if (! valid_window(win, NULL, 0)) {
			if (time(0) > start + waittime) {
				return 0;
			}
			usleep(ms * 1000);
			continue;
		}
		if (! XGetWindowAttributes(dpy, win, &attr)) {
			return 0;
		}
		if (attr.map_state == IsViewable) {
			return 1;
		}
		usleep(ms * 1000);
	}
	return 0;
}

int get_window_size(Window win, int *x, int *y) {
	XWindowAttributes attr;
	/* valid_window? */
	if (valid_window(win, &attr, 1)) {
		*x = attr.width;
		*y = attr.height;
		return 1;
	} else {
		return 0;
	}
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

	if (!sigpipe || *sigpipe == '\0' || !strcmp(sigpipe, "skip")) {
		;
	} else if (!strcmp(sigpipe, "ignore")) {
#ifdef SIG_IGN
		signal(SIGPIPE, SIG_IGN);
#endif
	} else if (!strcmp(sigpipe, "exit")) {
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

/*
 * check that all clients are in RFB_NORMAL state
 */
int all_clients_initialized(void) {
	rfbClientIteratorPtr iter;
	rfbClientPtr cl;
	int ok = 1;

	if (! screen) {
		return ok;
	}

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

char *list_clients(void) {
	rfbClientIteratorPtr iter;
	rfbClientPtr cl;
	char *list, tmp[32];
	int count = 0;

	if (!screen) {
		return strdup("");
	}

	iter = rfbGetClientIterator(screen);
	while( (cl = rfbClientIteratorNext(iter)) ) {
		count++;
	}
	rfbReleaseClientIterator(iter);

	/*
	 * each client:
         * <id>:<ip>:<port>:<user>:<hostname>:<input>:<loginview>,
	 * 8+1+16+1+5+1+24+1+256+1+5+1+1+1
	 * 123.123.123.123:60000/0x11111111-rw,
	 * so count+1 * 400 must cover it.
	 */
	list = (char *) malloc((count+1)*400);
	
	list[0] = '\0';

	iter = rfbGetClientIterator(screen);
	while( (cl = rfbClientIteratorNext(iter)) ) {
		ClientData *cd = (ClientData *) cl->clientData;
		if (*list != '\0') {
			strcat(list, ",");
		}
		sprintf(tmp, "0x%x:", cd->uid);
		strcat(list, tmp);
		strcat(list, cl->host);
		strcat(list, ":");
		sprintf(tmp, "%d:", cd->client_port);
		strcat(list, tmp);
		if (*(cd->username) == '\0') {
			char *s = ident_username(cl);
			if (s) free(s);
		}
		strcat(list, cd->username);
		strcat(list, ":");
		strcat(list, cd->hostname);
		strcat(list, ":");
		strcat(list, cd->input);
		strcat(list, ":");
		sprintf(tmp, "%d", cd->login_viewonly);
		strcat(list, tmp);
	}
	rfbReleaseClientIterator(iter);
	return list;
}

/* count number of clients supporting NewFBSize */
int new_fb_size_clients(rfbScreenInfoPtr s) {
	rfbClientIteratorPtr iter;
	rfbClientPtr cl;
	int count = 0;

	if (! s) {
		return 0;
	}

	iter = rfbGetClientIterator(s);
	while( (cl = rfbClientIteratorNext(iter)) ) {
		if (cl->useNewFBSize) {
			count++;
		}
	}
	rfbReleaseClientIterator(iter);
	return count;
}

void close_all_clients(void) {
	rfbClientIteratorPtr iter;
	rfbClientPtr cl;

	if (! screen) {
		return;
	}

	iter = rfbGetClientIterator(screen);
	while( (cl = rfbClientIteratorNext(iter)) ) {
		rfbCloseClient(cl);
		rfbClientConnectionGone(cl);
	}
	rfbReleaseClientIterator(iter);
}

rfbClientPtr *client_match(char *str) {
	rfbClientIteratorPtr iter;
	rfbClientPtr cl, *cl_list;
	int i, n, host_warn = 0, hex_warn = 0;

	n = client_count + 10;
	cl_list = (rfbClientPtr *) malloc(n * sizeof(rfbClientPtr));
	
	i = 0;
	iter = rfbGetClientIterator(screen);
	while( (cl = rfbClientIteratorNext(iter)) ) {
		if (strstr(str, "0x") == str) {
			int id;
			ClientData *cd = (ClientData *) cl->clientData;
			if (sscanf(str, "0x%x", &id) != 1) {
				if (hex_warn++) {
					continue;
				}
				rfbLog("skipping bad client hex id: %s\n", str);
				continue;
			}
			if ( cd->uid == id) {
				cl_list[i++] = cl;
			}
		} else {
			char *rstr = str;
			if (! dotted_ip(str))  {
				rstr = host2ip(str);
				if (rstr == NULL || *rstr == '\0') {
					if (host_warn++) {
						continue;
					}
					rfbLog("skipping bad lookup: \"%s\"\n",
					    str);
					continue;
				}
				rfbLog("lookup: %s -> %s\n", str, rstr);
			}
			if (!strcmp(rstr, cl->host)) {
				cl_list[i++] = cl;
			}
			if (rstr != str) {
				free(rstr);
			}
		}
		if (i >= n - 1) {
			break;
		}
	}
	rfbReleaseClientIterator(iter);

	cl_list[i] = NULL;

	return cl_list;
}

void close_clients(char *str) {
	rfbClientPtr *cl_list, *cp;

	if (!strcmp(str, "all") || !strcmp(str, "*")) {
		close_all_clients();
		return;
	}

	if (! screen) {
		return;
	}
	
	cl_list = client_match(str);

	cp = cl_list;
	while (*cp) {
		rfbCloseClient(*cp);
		rfbClientConnectionGone(*cp);
		cp++;
	}
	free(cl_list);
}

void set_client_input(char *str) {
	rfbClientPtr *cl_list, *cp;
	char *p, *val;

	/* str is "match:value" */

	if (! screen) {
		return;
	}

	p = strchr(str, ':');
	if (! p) {
		return;
	}
	*p = '\0';
	p++;
	val = short_kmb(p);
	
	cl_list = client_match(str);

	cp = cl_list;
	while (*cp) {
		ClientData *cd = (ClientData *) (*cp)->clientData;
		cd->input[0] = '\0';
		strcat(cd->input, "_");
		strcat(cd->input, val);
		cp++;
	}

	free(val);
	free(cl_list);
}

void set_child_info(void) {
	char pid[16];
	/* set up useful environment for child process */
	sprintf(pid, "%d", (int) getpid());
	set_env("X11VNC_PID", pid);
	if (program_name) {
		/* e.g. for remote control -R */
		set_env("X11VNC_PROG", program_name);
	}
	if (program_cmdline) {
		set_env("X11VNC_CMDLINE", program_cmdline);
	}
	if (raw_fb_str) {
		set_env("X11VNC_RAWFB_STR", raw_fb_str);
	} else {
		set_env("X11VNC_RAWFB_STR", "");
	}
}

/*
 * utility to run a user supplied command setting some RFB_ env vars.
 * used by, e.g., accept_client() and client_gone()
 */
static int run_user_command(char *cmd, rfbClientPtr client, char *mode) {
	char *old_display = NULL;
	char *addr = client->host;
	char str[100];
	int rc;
	ClientData *cd = (ClientData *) client->clientData;

	if (addr == NULL || addr[0] == '\0') {
		addr = "unknown-host";
	}

	/* set RFB_CLIENT_ID to semi unique id for command to use */
	if (cd && cd->uid) {
		sprintf(str, "0x%x", cd->uid);
	} else {
		/* not accepted yet: */
		sprintf(str, "0x%x", clients_served);
	}
	set_env("RFB_CLIENT_ID", str);

	/* set RFB_CLIENT_IP to IP addr for command to use */
	set_env("RFB_CLIENT_IP", addr);

	/* set RFB_X11VNC_PID to our pid for command to use */
	sprintf(str, "%d", (int) getpid());
	set_env("RFB_X11VNC_PID", str);

	/* set RFB_CLIENT_PORT to peer port for command to use */
	if (cd && cd->client_port > 0) {
		sprintf(str, "%d", cd->client_port);
	} else {
		sprintf(str, "%d", get_remote_port(client->sock));
	}
	set_env("RFB_CLIENT_PORT", str);

	set_env("RFB_MODE", mode);

	/* 
	 * now do RFB_SERVER_IP and RFB_SERVER_PORT (i.e. us!)
	 * This will establish a 5-tuple (including tcp) the external
	 * program can potentially use to work out the virtual circuit
	 * for this connection.
	 */
	if (cd && cd->server_ip) {
		set_env("RFB_SERVER_IP", cd->server_ip);
	} else {
		char *sip = get_local_host(client->sock);
		set_env("RFB_SERVER_IP", sip);
		free(sip);
	}

	if (cd && cd->server_port > 0) {
		sprintf(str, "%d", cd->server_port);
	} else {
		sprintf(str, "%d", get_local_port(client->sock));
	}
	set_env("RFB_SERVER_PORT", str);

	/* 
	 * Better set DISPLAY to the one we are polling, if they
	 * want something trickier, they can handle on their own
	 * via environment, etc. 
	 */
	if (getenv("DISPLAY")) {
		old_display = strdup(getenv("DISPLAY"));
	}

	if (raw_fb && ! dpy) {	/* raw_fb hack */
		set_env("DISPLAY", "rawfb");
	} else {
		set_env("DISPLAY", DisplayString(dpy));
	}

	/*
	 * work out the number of clients (have to use client_count
	 * since there is deadlock in rfbGetClientIterator) 
	 */
	sprintf(str, "%d", client_count);
	set_env("RFB_CLIENT_COUNT", str);

	if (no_external_cmds) {
		rfbLog("cannot run external commands in -nocmds mode:\n");
		rfbLog("   \"%s\"\n", cmd);
		rfbLog("   exiting.\n");
		clean_up_exit(1);
	}
	rfbLog("running command:\n");
	rfbLog("  %s\n", cmd);

	/* XXX need to close port 5900, etc.. */
	rc = system(cmd);

	if (rc >= 256) {
		rc = rc/256;
	}
	rfbLog("command returned: %d\n", rc);

	if (old_display) {
		set_env("DISPLAY", old_display);
		free(old_display);
	}

	return rc;
}

/*
 * callback for when a client disconnects
 */
static void client_gone(rfbClientPtr client) {

	client_count--;
	if (client_count < 0) client_count = 0;

	speeds_net_rate_measured = 0;
	speeds_net_latency_measured = 0;

	rfbLog("client_count: %d\n", client_count);

	if (no_autorepeat && client_count == 0) {
		autorepeat(1, 0);
	}
	if (use_solid_bg && client_count == 0) {
		solid_bg(1);
	}
	if (gone_cmd && *gone_cmd != '\0') {
		rfbLog("client_gone: using cmd for: %s\n", client->host);
		run_user_command(gone_cmd, client, "gone");
	}

	if (client->clientData) {
		ClientData *cd = (ClientData *) client->clientData;
		if (cd) {
			if (cd->server_ip) {
				free(cd->server_ip);
			}
			if (cd->hostname) {
				free(cd->hostname);
			}
			if (cd->username) {
				free(cd->username);
			}
		}
		free(client->clientData);
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
		if (shared && client_count > 0)  {
			rfbLog("connect_once: other shared clients still "
			    "connected, not exiting.\n");
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

	if (deny_all) {
		rfbLog("check_access: new connections are currently "
		    "blocked.\n");
		return 0;
	}
	if (addr == NULL || *addr == '\0') {
		rfbLog("check_access: denying empty host IP address string.\n");
		return 0;
	}

	if (allow_list == NULL) {
		/* set to "" to possibly append allow_once */
		allow_list = strdup("");
	}
	if (*allow_list == '\0' && allow_once == NULL) {
		/* no constraints, accept it */
		return 1;
	}

	if (strchr(allow_list, '/')) {
		/* a file of IP addresess or prefixes */
		int len, len2 = 0;
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
		if (allow_once) {
			len2 = strlen(allow_once) + 2;
			len += len2;
		}
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
			if (strlen(list) + strlen(line) >= len - len2) {
				/* file grew since our stat() */
				break;
			}
			strcat(list, line);
		}
		fclose(in);
		if (allow_once) {
			strcat(list, "\n");
			strcat(list, allow_once);
			strcat(list, "\n");
		}
	} else {
		int len = strlen(allow_list) + 1;
		if (allow_once) {
			len += strlen(allow_once) + 1;
		}
		list = malloc(len);
		list[0] = '\0';
		strcat(list, allow_list);
		if (allow_once) {
			strcat(list, ",");
			strcat(list, allow_once);
		}
	}

	if (allow_once) {
		free(allow_once);
		allow_once = NULL;
	}
	
	p = strtok(list, ", \t\n\r");
	while (p) {
		char *chk, *q, *r = NULL;
		if (*p == '\0') {
			p = strtok(NULL, ", \t\n\r");
			continue;	
		}
		if (! dotted_ip(p)) {
			r = host2ip(p);
			if (r == NULL || *r == '\0') {
				rfbLog("check_access: bad lookup \"%s\"\n", p);
				p = strtok(NULL, ", \t\n\r");
				continue;
			}
			rfbLog("check_access: lookup %s -> %s\n", p, r);
			chk = r;
		} else {
			chk = p;
		}

		q = strstr(addr, chk);
		if (chk[strlen(chk)-1] != '.') {
			if (!strcmp(addr, chk)) {
				if (chk != p) {
					rfbLog("check_access: client %s "
					    "matches host %s=%s\n", addr,
					    chk, p);
				} else {
					rfbLog("check_access: client %s "
					    "matches host %s\n", addr, chk);
				}
				allowed = 1;
			} else if(!strcmp(chk, "localhost") &&
			    !strcmp(addr, "127.0.0.1")) {
				allowed = 1;
			}
		} else if (q == addr) {
			rfbLog("check_access: client %s matches pattern %s\n",
			    addr, chk);
			allowed = 1;
		}
		p = strtok(NULL, ", \t\n\r");
		if (r) {
			free(r);
		}
		if (allowed) {
			break;
		}
	}
	free(list);
	return allowed;
}

/*
 * x11vnc's first (and only) visible widget: accept/reject dialog window.
 * We go through this pain to avoid dependency on libXt...
 */
static int ugly_accept_window(char *addr, char *userhost, int X, int Y,
    int timeout, char *mode) {

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
	char stri[100];
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
	int x, y, w = 345, h = 175, ret = 0;
	int X_sh = 20, Y_sh = 30, dY = 20;
	int Ye_x = 20,  Ye_y = 0, Ye_w = 45, Ye_h = 20;
	int No_x = 75,  No_y = 0, No_w = 45, No_h = 20; 
	int Vi_x = 130, Vi_y = 0, Vi_w = 45, Vi_h = 20; 

	if (raw_fb && ! dpy) return 0;	/* raw_fb hack */

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

	/* XXX handle coff_x/coff_y? */
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

	snprintf(strh, 100, "x11vnc: accept connection from %s?", addr);
	snprintf(stri, 100, "        (%s)", userhost);
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
			    stri, strlen(stri));
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
				XDrawString(dpy, awin, gc, Vi_x+tw,
				    Vi_y+Vi_h-5, str_v, strlen(str_v));
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
			XSelectInput(dpy, awin, 0);
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

	if (accept_cmd == NULL || *accept_cmd == '\0') {
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
		char *userhost = ident_username(client);

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
		if ((ret = ugly_accept_window(addr, userhost, x, y, timeout,
		    mode))) {
			free(userhost);
			if (ret == 2) {
				rfbLog("accept_client: viewonly: %s\n", addr);
				client->viewOnly = TRUE;
			}
			rfbLog("accept_client: popup accepted: %s\n", addr);
			return 1;
		} else {
			free(userhost);
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
		rc = run_user_command(cmd, client, "accept");

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
	char line[VNC_CONNECT_MAX], host[VNC_CONNECT_MAX];
	static int first_warn = 1, truncate_ok = 1;
	static time_t last_time = 0; 
	time_t now = time(0);

	if (last_time == 0) {
		last_time = now;
	}
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

	if (fgets(line, VNC_CONNECT_MAX, in) != NULL) {
		if (sscanf(line, "%s", host) == 1) {
			if (strlen(host) > 0) {
				char *str = strdup(host);
				if (strlen(str) > 38) {
					char trim[100]; 
					trim[0] = '\0';
					strncat(trim, str, 38);
					rfbLog("read connect file: %s ...\n",
					    trim);
				} else {
					rfbLog("read connect file: %s\n", str);
				}
				client_connect = str;
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
	int rport = 5500, len = strlen(str);

	if (len < 1) {
		return 0;
	}
	if (len > 1024) {
		rfbLog("reverse_connect: string too long: %d bytes\n", len);
		return 0;
	}
	if (!screen) {
		rfbLog("reverse_connect: screen not setup yet.\n");
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
		rport = atoi(p+1);
		*p = '\0';
	}

	cl = rfbReverseConnection(screen, host, rport);
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
			rfbPE(-1);
		}
		cnt += n;

		p = strtok(NULL, ", \t\r\n");
		if (p) {
			t = 0;
			while (t < sleep_between_host) {
				usleep(dt * 1000);
				rfbPE(-1);
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
		rfbPE(-1);
		usleep(dt * 1000);
		t += dt;
	}
}

/*
 * Routines for monitoring the VNC_CONNECT property for changes.
 * The vncconnect(1) will set it on our X display.
 */
void set_vnc_connect_prop(char *str) {
	XChangeProperty(dpy, rootwin, vnc_connect_prop, XA_STRING, 8,
	    PropModeReplace, (unsigned char *)str, strlen(str));
}

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
	if (strlen(vnc_connect_str) > 38) {
		char trim[100]; 
		trim[0] = '\0';
		strncat(trim, vnc_connect_str, 38);
		rfbLog("read VNC_CONNECT: %s ...\n", trim);
		
	} else {
		rfbLog("read VNC_CONNECT: %s\n", vnc_connect_str);
	}
}

/*
 * check if client_connect has been set, if so make the reverse connections.
 */
static void send_client_connect(void) {
	if (client_connect != NULL) {
		char *str = client_connect;
		if (strstr(str, "cmd=") == str || strstr(str, "qry=") == str) {
			process_remote_cmd(client_connect, 0);
		} else if (strstr(str, "ans=") || strstr(str, "aro=") == str) {
			;
		} else if (strstr(str, "ack=") == str) {
			;
		} else {
			reverse_connect(client_connect);
		}
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
	ClientData *cd; 

	last_event = last_input = time(0);

	if (inetd) {
		/* 
		 * Set this so we exit as soon as connection closes,
		 * otherwise client_gone is only called after RFB_CLIENT_ACCEPT
		 */
		client->clientGoneHook = client_gone;
	}

	clients_served++;

	if (connect_once) {
		if (screen->dontDisconnect && screen->neverShared) {
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

	client->clientData = (void *) calloc(sizeof(ClientData), 1);
	cd = (ClientData *) client->clientData;

	cd->uid = clients_served;

	cd->client_port = get_remote_port(client->sock);
	cd->server_port = get_local_port(client->sock);
	cd->server_ip   = get_local_host(client->sock);
	cd->hostname = ip2host(client->host);
	cd->username = strdup("");

	cd->input[0] = '-';
	cd->login_viewonly = -1;

	client->clientGoneHook = client_gone;

	if (client_count) {
		speeds_net_rate_measured = 0;
		speeds_net_latency_measured = 0;
	}
	client_count++;

	last_keyboard_input = last_pointer_input = time(0);

	if (no_autorepeat && client_count == 1 && ! view_only) {
		/*
		 * first client, turn off X server autorepeat
		 * XXX handle dynamic change of view_only and per-client.
		 */
		autorepeat(0, 0);
	}
	if (use_solid_bg && client_count == 1) {
		solid_bg(0);
	}

	if (pad_geometry) {
		install_padded_fb(pad_geometry);
	}

	cd->timer = dnow();
	cd->send_cmp_rate = 0.0;
	cd->send_raw_rate = 0.0;
	cd->latency = 0.0;
	cd->cmp_bytes_sent = 0;
	cd->raw_bytes_sent = 0;

	accepted_client = 1;
	last_client = time(0);

	return(RFB_CLIENT_ACCEPT);
}

void check_new_clients(void) {
	static int last_count = 0;
	rfbClientIteratorPtr iter;
	rfbClientPtr cl;
	
	if (client_count == last_count) {
		return;
	}

	if (! all_clients_initialized()) {
		return;
	}

	last_count = client_count;
	if (! client_count) {
		return;
	}
	if (! screen) {
		return;
	}

	iter = rfbGetClientIterator(screen);
	while( (cl = rfbClientIteratorNext(iter)) ) {
		ClientData *cd = (ClientData *) cl->clientData;

		if (cd->login_viewonly < 0) {
			/* this is a general trigger to initialize things */
			if (cl->viewOnly) {
				cd->login_viewonly = 1;
				if (allowed_input_view_only) {
					cl->viewOnly = FALSE;
					cd->input[0] = '\0';
					strncpy(cd->input,
					    allowed_input_view_only, CILEN);
				}
			} else {
				cd->login_viewonly = 0;
				if (allowed_input_normal) {
					cd->input[0] = '\0';
					strncpy(cd->input,
					    allowed_input_normal, CILEN);
				}
			}
		}
	}
	rfbReleaseClientIterator(iter);
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
		XTestFakeKeyEvent_wr(dpy, keycode, False, CurrentTime);
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
			XTestFakeKeyEvent_wr(dpy, keycode, False, CurrentTime);
		}
	}
	XFlush(dpy);
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
static int save_auto_repeat = -1;

int get_autorepeat_state(void) {
	XKeyboardState kstate;
	X_LOCK;
	XGetKeyboardControl(dpy, &kstate);
	X_UNLOCK;
	return kstate.global_auto_repeat;
}

int get_initial_autorepeat_state(void) {
	if (save_auto_repeat < 0) {
		save_auto_repeat = get_autorepeat_state();
	}
	return save_auto_repeat;
}

void autorepeat(int restore, int bequiet) {
	int global_auto_repeat;
	XKeyboardControl kctrl;

	if (raw_fb && ! dpy) return;	/* raw_fb hack */

	if (restore) {
		if (save_auto_repeat < 0) {
			return;		/* nothing to restore */
		}
		global_auto_repeat = get_autorepeat_state();
		X_LOCK;
		/* read state and skip restore if equal (e.g. no clients) */
		if (global_auto_repeat == save_auto_repeat) {
			X_UNLOCK;
			return;
		}

		kctrl.auto_repeat_mode = save_auto_repeat;
		XChangeKeyboardControl(dpy, KBAutoRepeatMode, &kctrl);
		XFlush(dpy);
		X_UNLOCK;

		if (! bequiet && ! quiet) {
			rfbLog("Restored X server key autorepeat to: %d\n",
			    save_auto_repeat);
		}
	} else {
		global_auto_repeat = get_autorepeat_state();
		if (save_auto_repeat < 0) {
			/*
			 * we only remember the state at startup
			 * to avoid confusing ourselves later on.
			 */
			save_auto_repeat = global_auto_repeat;
		}

		X_LOCK;
		kctrl.auto_repeat_mode = AutoRepeatModeOff;
		XChangeKeyboardControl(dpy, KBAutoRepeatMode, &kctrl);
		XFlush(dpy);
		X_UNLOCK;

		if (! bequiet && ! quiet) {
			rfbLog("Disabled X server key autorepeat.\n");
			if (no_repeat_countdown >= 0) {
				rfbLog("  to force back on run: 'xset r on' (%d "
				    "times)\n", no_repeat_countdown+1);
			}
		}
	}
}

/*
 * We periodically delete any keysyms we have added, this is to
 * lessen our effect on the X server state if we are terminated abruptly
 * and cannot clear them and also to clear out any strange little used
 * ones that would just fill up the keymapping. 
 */
void check_add_keysyms(void) {
	static time_t last_check = 0;
	int clear_freq = 300, quiet = 1, count; 
	time_t now = time(0);
	if (now > last_check + clear_freq) {
		count = count_added_keycodes();
		/*
		 * only really delete if they have not typed recently
		 * and we have added 8 or more.
		 */
		if (now > last_keyboard_input + 5 && count >= 8) {
			X_LOCK;
			delete_added_keycodes(quiet);
			X_UNLOCK;
		}
		last_check = now;
	}
}

static KeySym added_keysyms[0x100];

/* these are just for rfbLog messages: */
static KeySym alltime_added_keysyms[1024];
static int alltime_len = 1024;
static int alltime_num = 0;

int add_keysym(KeySym keysym) {
	int minkey, maxkey, syms_per_keycode;
	int kc, n, ret = 0;
	static int first = 1;
	KeySym *keymap;


	if (first) {
		for (n=0; n < 0x100; n++) {
			added_keysyms[n] = NoSymbol;
		}
		first = 0;
	}

	if (raw_fb && ! dpy) return 0;	/* raw_fb hack */

	if (keysym == NoSymbol) {
		return 0;
	}
	/* there can be a race before MappingNotify */
	for (n=0; n < 0x100; n++) {
		if (added_keysyms[n] == keysym) {
			return n;
		}
	}

	XDisplayKeycodes(dpy, &minkey, &maxkey);
	keymap = XGetKeyboardMapping(dpy, minkey, (maxkey - minkey + 1),
	    &syms_per_keycode);

	for (kc = minkey+1; kc <= maxkey; kc++) {
		int i, j, didmsg = 0, is_empty = 1;
		char *str;
		KeySym new[8];

		for (n=0; n < syms_per_keycode; n++) {
			if (keymap[ (kc-minkey) * syms_per_keycode + n]
			    != NoSymbol) {
				is_empty = 0;
				break;
			}
		}
		if (! is_empty) {
			continue;
		}

		for (i=0; i<8; i++) {
			new[i] = NoSymbol;
		}
		if (add_keysyms == 2) {
			new[0] = keysym;	/* XXX remove me */
		} else {
			for(i=0; i < syms_per_keycode; i++) {
				new[i] = keysym;
				if (i >= 7) break;
			}
		}

		XChangeKeyboardMapping(dpy, kc, syms_per_keycode,
		    new, 1);

		if (alltime_num >= alltime_len) {
			didmsg = 1;	/* something weird */
		} else {
			for (j=0; j<alltime_num; j++) {
				if (alltime_added_keysyms[j] == keysym) {
					didmsg = 1;
					break;
				}
			}
		}
		if (! didmsg) {
			str = XKeysymToString(keysym);
			rfbLog("added missing keysym to X display: %03d "
			    "0x%x \"%s\"\n", kc, keysym, str ? str : "null");

			if (alltime_num < alltime_len) {
				alltime_added_keysyms[alltime_num++] = keysym;
			}
		}

		XFlush(dpy);
		added_keysyms[kc] = keysym;
		ret = kc;
		break;
	}
	XFree(keymap);
	return ret;
}

void delete_keycode(KeyCode kc, int bequiet) {
	int minkey, maxkey, syms_per_keycode, i;
	KeySym *keymap;
	KeySym ksym, new[8];
	char *str;

	if (raw_fb && ! dpy) return;	/* raw_fb hack */

	XDisplayKeycodes(dpy, &minkey, &maxkey);
	keymap = XGetKeyboardMapping(dpy, minkey, (maxkey - minkey + 1),
	    &syms_per_keycode);

	for (i=0; i<8; i++) {
		new[i] = NoSymbol;
	}

	XChangeKeyboardMapping(dpy, kc, syms_per_keycode, new, 1);

	if (! bequiet && ! quiet) {
		ksym = XKeycodeToKeysym(dpy, kc, 0);
		str = XKeysymToString(ksym);
		rfbLog("deleted keycode from X display: %03d 0x%x \"%s\"\n",
		    kc, ksym, str ? str : "null");
	}

	XFree(keymap);
	XFlush(dpy);
}

int count_added_keycodes(void) {
	int kc, count = 0;
	for (kc = 0; kc < 0x100; kc++) {
		if (added_keysyms[kc] != NoSymbol) {
			count++;
		}
	}
	return count;
}

void delete_added_keycodes(int bequiet) {
	int kc;
	for (kc = 0; kc < 0x100; kc++) {
		if (added_keysyms[kc] != NoSymbol) {
			delete_keycode(kc, bequiet);
			added_keysyms[kc] = NoSymbol;
		}
	}
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

void add_remap(char *line) {
	char str1[256], str2[256];
	KeySym ksym1, ksym2;
	int isbtn = 0, i;
	static keyremap_t *current = NULL;
	keyremap_t *remap;

	if (sscanf(line, "%s %s", str1, str2) != 2) {
		rfbLog("remap: bad line: %s\n", line);
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
		return;
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

void add_dead_keysyms(char *str) {
	char *p, *q;
	int i;
	char *list[] = {
		"g grave dead_grave",
		"a acute dead_acute",
		"c asciicircum dead_circumflex",
		"t asciitilde dead_tilde",
		"m macron dead_macron",
		"b breve dead_breve",
		"D abovedot dead_abovedot",
		"d diaeresis dead_diaeresis",
		"o degree dead_abovering",
		"A doubleacute dead_doubleacute",
		"r caron dead_caron",
		"e cedilla dead_cedilla",
/*		"x XXX-ogonek dead_ogonek", */
/*		"x XXX-belowdot dead_belowdot", */
/*		"x XXX-hook dead_hook", */
/*		"x XXX-horn dead_horn", */
		NULL
	};

	p = str;

	while (*p != '\0') {
		if (isspace(*p)) {
			*p = '\0';
		}
		p++;
	}

	if (!strcmp(str, "DEAD")) {
		for (i = 0; list[i] != NULL; i++) {
			p = list[i] + 2;
			add_remap(p);
		}
	} else if (!strcmp(str, "DEAD=missing")) {
		for (i = 0; list[i] != NULL; i++) {
			KeySym ksym, ksym2;
			int inmap = 0;

			p = strdup(list[i] + 2);
			q = strchr(p, ' ');
			if (q == NULL) {
				free(p);
				continue;
			}
			*q = '\0';
			ksym = XStringToKeysym(p);
			*q = ' ';
			if (ksym == NoSymbol) {
				free(p);
				continue;
			}
			if (XKeysymToKeycode(dpy, ksym)) {
				inmap = 1;
			}
#if LIBVNCSERVER_HAVE_XKEYBOARD
			if (! inmap && xkb_present) {
				int kc, grp, lvl;
				for (kc = 0; kc < 0x100; kc++) {
				    for (grp = 0; grp < 4; grp++) {
					for (lvl = 0; lvl < 8; lvl++) {
						ksym2 = XkbKeycodeToKeysym(dpy,
						    kc, grp, lvl);
						if (ksym2 == NoSymbol) {
							continue;
						}
						if (ksym2 == ksym) {
							inmap = 1;
							break;
						}
					}
				    }
				}
			}
#endif
			if (! inmap) {
				add_remap(p);
			}
			free(p);
		}
	} else if ((p = strchr(str, '=')) != NULL) {
		while (*p != '\0') {
			for (i = 0; list[i] != NULL; i++) {
				q = list[i];
				if (*p == *q) {
					q += 2;
					add_remap(q);
					break;
				}
			}
			p++;
		}
	}
}

/*
 * process the -remap string (file or mapping string)
 */
void initialize_remap(char *infile) {
	FILE *in;
	char *p, *q, line[256];

	if (keyremaps != NULL) {
		/* free last remapping */
		keyremap_t *next_remap, *curr_remap = keyremaps;
		while (curr_remap != NULL) {
			next_remap = curr_remap->next;
			free(curr_remap);
			curr_remap = next_remap;
		}
		keyremaps = NULL;
	}
	if (infile == NULL || *infile == '\0') {
		/* just unset remapping */
		return;
	}

	in = fopen(infile, "r"); 
	if (in == NULL) {
		/* assume cmd line key1-key2,key3-key4 */
		if (strstr(infile, "DEAD") == infile) {
			;
		} else if (!strchr(infile, '-')) {
			rfbLog("remap: cannot open: %s\n", infile);
			rfbLogPerror("fopen");
			clean_up_exit(1);
		}
		if ((in = tmpfile()) == NULL) {
			rfbLog("remap: cannot open tmpfile for %s\n", infile);
			rfbLogPerror("tmpfile");
			clean_up_exit(1);
		}

		/* copy in the string to file format */
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
		p = lblanks(line);
		if (*p == '\0') {
			continue;
		}
		if (strchr(line, '#')) {
			continue;
		}

		if (strstr(p, "DEAD") == p)  {
			add_dead_keysyms(p);
			continue;
		}
		if ((q = strchr(line, '-')) != NULL) {
			/* allow Keysym1-Keysym2 notation */
			*q = ' ';	
		}
		add_remap(p);
	}
	fclose(in);
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

/*
 * for trying to order the keycodes to avoid problems, note the
 * *first* keycode bound to it.  kc_vec will be a permutation
 * of 1...256 to get them in the preferred order.
 */
static int kc_vec[0x100];
static int kc1_shift, kc1_control, kc1_caplock, kc1_alt;
static int kc1_meta, kc1_numlock, kc1_super, kc1_hyper;
static int kc1_mode_switch, kc1_iso_level3_shift, kc1_multi_key;
	
#if !LIBVNCSERVER_HAVE_XKEYBOARD

/* empty functions for no xkb */
static void initialize_xkb_modtweak(void) {}
static void xkb_tweak_keyboard(rfbBool down, rfbKeySym keysym,
    rfbClientPtr client) {
}
static void switch_to_xkb_if_better(void) {}

#else

static void switch_to_xkb_if_better(void) {
	KeySym keysym, *keymap;
	int miss_noxkb[256], miss_xkb[256], missing_noxkb = 0, missing_xkb = 0;
	int i, j, k, n, minkey, maxkey, syms_per_keycode;
	int syms_gt_4 = 0;
	int kc, grp, lvl;

	/* non-alphanumeric on us keyboard */
	KeySym must_have[] = {
		XK_exclam,
		XK_at,
		XK_numbersign,
		XK_dollar,
		XK_percent,
/*		XK_asciicircum, */
		XK_ampersand,
		XK_asterisk,
		XK_parenleft,
		XK_parenright,
		XK_underscore,
		XK_plus,
		XK_minus,
		XK_equal,
		XK_bracketleft,
		XK_bracketright,
		XK_braceleft,
		XK_braceright,
		XK_bar,
		XK_backslash,
		XK_semicolon,
/*		XK_apostrophe, */
		XK_colon,
		XK_quotedbl,
		XK_comma,
		XK_period,
		XK_less,
		XK_greater,
		XK_slash,
		XK_question,
/*		XK_asciitilde, */
/*		XK_grave, */
		NoSymbol
	};

	if (! use_modifier_tweak || got_noxkb) {
		return;
	}
	if (use_xkb_modtweak) {
		/* already using it */
		return;
	}

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

	k = 0;
	while (must_have[k] != NoSymbol) {
		int gotit = 0;
		KeySym must = must_have[k];
		for (i = minkey; i <= maxkey; i++) {
		    for (j = 0; j < syms_per_keycode; j++) {
			keysym = keymap[(i-minkey) * syms_per_keycode + j];
			if (j >= 4) {
			    if (k == 0 && keysym != NoSymbol) {
				/* for k=0 count the high keysyms */
				syms_gt_4++;
				if (debug_keyboard > 1) {
					char *str = XKeysymToString(keysym);
					fprintf(stderr, "- high keysym mapping"
					    ": at %3d j=%d "
					    "'%s'\n", i, j, str ? str:"null");
				}
			    }
			    continue;
			}
			if (keysym == must) {
				if (debug_keyboard > 1) {
					char *str = XKeysymToString(must);
					fprintf(stderr, "- at %3d j=%d found "
					    "'%s'\n", i, j, str ? str:"null");
				}
				/* n.b. do not break, see syms_gt_4 above. */
				gotit = 1;
			}
		    }
		}
		if (! gotit) {
			if (debug_keyboard > 1) {
				char *str = XKeysymToString(must);
				KeyCode kc = XKeysymToKeycode(dpy, must);
				fprintf(stderr, "- did not find 0x%lx '%s'\t"
				    "Ks2Kc: %d\n", must, str ? str:"null", kc); 
				if (kc != None) {
					int j2;
					for(j2=0; j2<syms_per_keycode; j2++) {
						keysym = keymap[(kc-minkey) *
						    syms_per_keycode + j2];
						fprintf(stderr, "  %d=0x%lx",
						    j2, keysym);
					}
					fprintf(stderr, "\n");
				}
			}
			missing_noxkb++;
			miss_noxkb[k] = 1;
		} else {
			miss_noxkb[k] = 0;
		}
		k++;
	}
	n = k;

	XFree(keymap);
	if (missing_noxkb == 0 && syms_gt_4 >= 8) {
		rfbLog("XKEYBOARD: number of keysyms per keycode %d "
		    "is greater\n", syms_per_keycode);
		rfbLog("  than 4 and %d keysyms are mapped above 4.\n",
		    syms_gt_4);
		rfbLog("  Automatically switching to -xkb mode.\n");
		rfbLog("  If this makes the key mapping worse you can\n");
		rfbLog("  disable it with the \"-noxkb\" option.\n");
		rfbLog("  Also, remember \"-remap DEAD\" for accenting"
		    " characters.\n");

		use_xkb_modtweak = 1;
		return;

	} else if (missing_noxkb == 0) {
		rfbLog("XKEYBOARD: all %d \"must have\" keysyms accounted"
		    " for.\n", n);
		rfbLog("  Not automatically switching to -xkb mode.\n");
		rfbLog("  If some keys still cannot be typed, try using"
		    " -xkb.\n");
		rfbLog("  Also, remember \"-remap DEAD\" for accenting"
		    " characters.\n");
		return;
	}

	for (k=0; k<n; k++) {
		miss_xkb[k] = 1;
	}

	for (kc = 0; kc < 0x100; kc++) {
	    for (grp = 0; grp < GRP; grp++) {
		for (lvl = 0; lvl < LVL; lvl++) {
			/* look up the Keysym, if any */
			keysym = XkbKeycodeToKeysym(dpy, kc, grp, lvl);
			if (keysym == NoSymbol) {
				continue;
			}
			for (k=0; k<n; k++) {
				if (keysym == must_have[k]) {
					miss_xkb[k] = 0;
				}
			}
		}
	    }
	}

	for (k=0; k<n; k++) {
		if (miss_xkb[k]) {
			missing_xkb++;
		}
	}

	rfbLog("\n");
	if (missing_xkb < missing_noxkb) {
		rfbLog("XKEYBOARD:\n");
		rfbLog("Switching to -xkb mode to recover these keysyms:\n");
	} else {
		rfbLog("XKEYBOARD: \"must have\" keysyms better accounted"
		    " for\n");
		rfbLog("under -noxkb mode: not switching to -xkb mode:\n");
	}

	rfbLog("   xkb  noxkb   Keysym  (\"X\" means present)\n");
	rfbLog("   ---  -----   -----------------------------\n");
	for (k=0; k<n; k++) {
		char *xx, *xn, *name;

		keysym = must_have[k];
		if (keysym == NoSymbol) {
			continue;
		}
		if (!miss_xkb[k] && !miss_noxkb[k]) {
			continue;
		}
		if (miss_xkb[k]) {
			xx = "   ";
		} else {
			xx = " X ";
		}
		if (miss_noxkb[k]) {
			xn = "   ";
		} else {
			xn = " X ";
		}
		name = XKeysymToString(keysym);
		rfbLog("   %s  %s     0x%lx  %s\n", xx, xn, keysym,
		    name ? name : "null");
	}
	rfbLog("\n");

	if (missing_xkb < missing_noxkb) {
		rfbLog("  If this makes the key mapping worse you can\n");
		rfbLog("  disable it with the \"-noxkb\" option.\n");
		rfbLog("\n");

		use_xkb_modtweak = 1;

	} else {
		rfbLog("  If some keys still cannot be typed, try using"
		    " -xkb.\n");
		rfbLog("  Also, remember \"-remap DEAD\" for accenting"
		    " characters.\n");
	}
}

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

	/* first keycode for a modifier type (multi_key too) */
	kc1_shift = -1;
	kc1_control = -1;
	kc1_caplock = -1;
	kc1_alt = -1;
	kc1_meta = -1;
	kc1_numlock = -1;
	kc1_super = -1;
	kc1_hyper = -1;
	kc1_mode_switch = -1;
	kc1_iso_level3_shift = -1;
	kc1_multi_key = -1;

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

			if (ks == XK_Shift_L || ks == XK_Shift_R) {
				if (kc1_shift == -1) {
					kc1_shift = kc;
				}
			}
			if (ks == XK_Control_L || ks == XK_Control_R) {
				if (kc1_control == -1) {
					kc1_control = kc;
				}
			}
			if (ks == XK_Caps_Lock || ks == XK_Caps_Lock) {
				if (kc1_caplock == -1) {
					kc1_caplock = kc;
				}
			}
			if (ks == XK_Alt_L || ks == XK_Alt_R) {
				if (kc1_alt == -1) {
					kc1_alt = kc;
				}
			}
			if (ks == XK_Meta_L || ks == XK_Meta_R) {
				if (kc1_meta == -1) {
					kc1_meta = kc;
				}
			}
			if (ks == XK_Num_Lock) {
				if (kc1_numlock == -1) {
					kc1_numlock = kc;
				}
			}
			if (ks == XK_Super_L || ks == XK_Super_R) {
				if (kc1_super == -1) {
					kc1_super = kc;
				}
			}
			if (ks == XK_Hyper_L || ks == XK_Hyper_R) {
				if (kc1_hyper == -1) {
					kc1_hyper = kc;
				}
			}
			if (ks == XK_Mode_switch) {
				if (kc1_mode_switch == -1) {
					kc1_mode_switch = kc;
				}
			}
			if (ks == XK_ISO_Level3_Shift) {
				if (kc1_iso_level3_shift == -1) {
					kc1_iso_level3_shift = kc;
				}
			}
			if (ks == XK_Multi_key) {	/* not a modifier.. */
				if (kc1_multi_key == -1) {
					kc1_multi_key = kc;
				}
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
				    kc, grp+1, lvl+1, bitprint(ms, 8));
				fprintf(stderr, "state=%s ",
				    bitprint(xkbstate[kc][grp][lvl], 8));
				fprintf(stderr, "ignore=%s ",
				    bitprint(xkbignore[kc][grp][lvl], 8));
				fprintf(stderr, " ks=0x%08lx \"%s\"\n",
				    ks, XKeysymToString(ks));
			}
		}
	    }
	}

	/*
	 * kc_vec will be used in some places to find modifiers, etc
	 * we apply some permutations to it as workarounds.
	 */
	for (kc = 0; kc < 0x100; kc++) {
		kc_vec[kc] = kc;
	}

	if (kc1_mode_switch != -1 && kc1_iso_level3_shift != -1) {
		if (kc1_mode_switch < kc1_iso_level3_shift) {
			/* we prefer XK_ISO_Level3_Shift: */
			kc_vec[kc1_mode_switch] = kc1_iso_level3_shift;
			kc_vec[kc1_iso_level3_shift] = kc1_mode_switch;
		}
	}
	/* any more? need to watch for undoing the above. */

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

	int kc, grp, lvl, i, kci;
	int kc_f[0x100], grp_f[0x100], lvl_f[0x100], state_f[0x100], found;
	int ignore_f[0x100];
	unsigned int state;


	/* these are used for finding modifiers, etc */
	XkbStateRec kbstate;
	int got_kbstate = 0;
	int Kc_f, Grp_f, Lvl_f;
	static int Kc_last_down = -1;
	static KeySym Ks_last_down = NoSymbol;

	X_LOCK;

	if (debug_keyboard) {
		char *str = XKeysymToString(keysym);

		if (debug_keyboard > 1) {
			rfbLog("----------start-xkb_tweak_keyboard (%s) "
			    "--------\n", down ? "DOWN" : "UP");
		}

		rfbLog("xkb_tweak_keyboard: %s keysym=0x%x \"%s\"\n",
		    down ? "down" : "up", (int) keysym, str ? str : "null");
	}

	/*
	 * set everything to not-yet-found.
	 * these "found" arrays (*_f) let us dynamically consider the
	 * one-to-many Keysym -> Keycode issue.  we set the size at 256,
	 * but of course only very few will be found.
	 */
	for (i = 0; i < 0x100; i++) {
		kc_f[i]    = -1;
		grp_f[i]   = -1;
		lvl_f[i]   = -1;
		state_f[i] = -1;
		ignore_f[i] = -1;
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
				    bitprint(state, 8));
				fprintf(stderr, "    ignorable : %s\n",
				    bitprint(xkbignore[kc][grp][lvl], 8));
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
			ignore_f[found] = xkbignore[kc][grp][lvl];
			found++;
		}
	    }
	}

#define PKBSTATE  \
	fprintf(stderr, "    --- current mod state:\n"); \
	fprintf(stderr, "    mods      : %s\n", bitprint(kbstate.mods, 8)); \
	fprintf(stderr, "    base_mods : %s\n", bitprint(kbstate.base_mods, 8)); \
	fprintf(stderr, "    latch_mods: %s\n", bitprint(kbstate.latched_mods, 8)); \
	fprintf(stderr, "    lock_mods : %s\n", bitprint(kbstate.locked_mods, 8)); \
	fprintf(stderr, "    compat    : %s\n", bitprint(kbstate.compat_state, 8));

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

	if (!found && add_keysyms && keysym && ! IsModifierKey(keysym)) {
		int new_kc = add_keysym(keysym);
		if (new_kc != 0) {
			found = 1;
			kc_f[0] = new_kc;
			grp_f[0] = 0; 
			lvl_f[0] = 0; 
			state_f[0] = 0;
		}
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
	 * we try to optimize here if found > 1
	 * e.g. minimize lvl or grp, or other things to give
	 * "safest" scenario to simulate the keystrokes.
	 */

	if (found > 1) {
		if (down) {
			int l, score[0x100];
			int best, best_score = -1;
			/* need to break the tie... */
			if (! got_kbstate) {
				XkbGetState(dpy, XkbUseCoreKbd, &kbstate);
				got_kbstate = 1;
			}
			for (l=0; l < found; l++) {
				int myscore = 0, b = 0x1, i;
				int curr, curr_state = kbstate.mods;
				int need, need_state = state_f[l];
				int ignore_state = ignore_f[l];

				/* see how many modifiers need to be changed */
				for (i=0; i<8; i++) {
					curr = b & curr_state;
					need = b & need_state;
					if (! (b & ignore_state)) {
						;
					} else if (curr == need) {
						;
					} else {
						myscore++;
					}
					b = b << 1;
				}
				myscore *= 100;

				/* throw in some minimization of lvl too: */
				myscore += 2*lvl_f[l] + grp_f[l];

				score[l] = myscore;
				if (debug_keyboard > 1) {
					fprintf(stderr, "    *** score for "
					    "keycode %03d: %4d\n",
					    kc_f[l], myscore);
				}
			}
			for (l=0; l < found; l++) {
				int myscore = score[l];
				if (best_score == -1 || myscore < best_score) {
					best = l;
					best_score = myscore;
				}
			}
			Kc_f = kc_f[best];
			Grp_f = grp_f[best];
			Lvl_f = lvl_f[best];
			state = state_f[best];
			
		} else {
			Kc_f = -1;
			if (keysym == Ks_last_down) {
				int l;
				for (l=0; l < found; l++) {
					if (Kc_last_down == kc_f[l]) {
						Kc_f = Kc_last_down;
						break;
					}
				}
			}

			if (Kc_f == -1) {
				/* hope for the best... XXX check mods */
				Kc_f = kc_f[0];
			}
		}
	} else {
		Kc_f = kc_f[0];
		Grp_f = grp_f[0];
		Lvl_f = lvl_f[0];
		state = state_f[0];
	}

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
		fprintf(stderr, ", picked this one: %03d  (last down: %03d)\n",
		    Kc_f, Kc_last_down);
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

		/* remember these to aid the subsequent up case: */
		Ks_last_down = keysym;
		Kc_last_down = Kc_f;

		if (! got_kbstate) {
			/* get the current modifier state if we haven't yet */
			XkbGetState(dpy, XkbUseCoreKbd, &kbstate);
			got_kbstate = 1;
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
				    "need it to be: %d %s\n", i, bitprint(b, 8),
				    needmods[i], needmods[i] ? "down" : "up");
			}

			/*
			 * Again, an inefficient loop, this time just
			 * looking for modifiers...
			 * 
			 * note the use of kc_vec to prefer XK_ISO_Level3_Shift
			 * over XK_Mode_switch.
			 */
			for (kci = kc_min; kci <= kc_max; kci++) {
			  for (grp = 0; grp < grp_max+1; grp++) {
			    for (lvl = 0; lvl < lvl_max+1; lvl++) {
				int skip = 1, dbmsg = 0;

				kc = kc_vec[kci];

				ms = xkbmodifiers[kc][grp][lvl];
				if (! ms || ms != b) {
					continue;
				}

				if (skipkeycode[kc] && debug_keyboard) {
				    fprintf(stderr, "    xxx skipping keycode:"
					" %d G%d/L%d\n", kc, grp+1, lvl+1);
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
					fprintf(stderr, "    === for mod=%s "
					    "found kc=%03d/G%d/L%d it is %d "
					    "%s skip=%d (%s)\n", bitprint(b,8),
					    kc, grp+1, lvl+1, kt, kt ?
					    "down" : "up  ", skip, str ?
					    str : "null");
				}

				if (! skip && needmods[i] !=
				    keystate[kc] && sentmods[i] == 0) {
					sentmods[i] = kc;
					dbmsg = 1;
				}

				if (debug_keyboard > 1 && dbmsg) {
					int nm = needmods[i];
					fprintf(stderr, "    >>> we choose "
					    "kc=%03d=0x%02x to change it to: "
					    "%d %s\n", kc, kc, nm, nm ?
					    "down" : "up");
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
			XTestFakeKeyEvent_wr(dpy, shift_is_down, False,
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
			XTestFakeKeyEvent_wr(dpy, sentmods[i], dn, CurrentTime);
		}
		for (j=0; j<8; j++) {
			/* next, do the Mod downs */
			i = Ilist[j];
			if (sentmods[i] == 0) continue;
			dn = (Bool) needmods[i];
			if (!dn) continue;
			XTestFakeKeyEvent_wr(dpy, sentmods[i], dn, CurrentTime);
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
			XTestFakeKeyEvent_wr(dpy, shift_is_down, True,
			    CurrentTime);
		}

		/*
		 * With the above modifier work done, send the actual keycode:
		 */
		XTestFakeKeyEvent_wr(dpy, Kc_f, (Bool) down, CurrentTime);

		/*
		 * Now undo the modifier work:
		 */
		for (j=7; j>=0; j--) {
			/* reverse Mod downs we did */
			i = Ilist[j];
			if (sentmods[i] == 0) continue;
			dn = (Bool) needmods[i];
			if (!dn) continue;
			XTestFakeKeyEvent_wr(dpy, sentmods[i], !dn,
			    CurrentTime);
		}
		for (j=7; j>=0; j--) {
			/* finally reverse the Mod ups we did */
			i = Ilist[j];
			if (sentmods[i] == 0) continue;
			dn = (Bool) needmods[i];
			if (dn) continue;
			XTestFakeKeyEvent_wr(dpy, sentmods[i], !dn,
			    CurrentTime);
		}

	} else { /* for up case, hopefully just need to pop it up: */

		XTestFakeKeyEvent_wr(dpy, Kc_f, (Bool) down, CurrentTime);
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

/* workaround for X11R5, Latin 1 only */
#ifndef XConvertCase
#define XConvertCase(sym, lower, upper) \
*(lower) = sym; \
*(upper) = sym; \
if (sym >> 8 == 0) { \
    if ((sym >= XK_A) && (sym <= XK_Z)) \
        *(lower) += (XK_a - XK_A); \
    else if ((sym >= XK_a) && (sym <= XK_z)) \
        *(upper) -= (XK_a - XK_A); \
    else if ((sym >= XK_Agrave) && (sym <= XK_Odiaeresis)) \
        *(lower) += (XK_agrave - XK_Agrave); \
    else if ((sym >= XK_agrave) && (sym <= XK_odiaeresis)) \
        *(upper) -= (XK_agrave - XK_Agrave); \
    else if ((sym >= XK_Ooblique) && (sym <= XK_Thorn)) \
        *(lower) += (XK_oslash - XK_Ooblique); \
    else if ((sym >= XK_oslash) && (sym <= XK_thorn)) \
        *(upper) -= (XK_oslash - XK_Ooblique); \
}
#endif

char *short_kmb(char *str) {
	int i, saw_k = 0, saw_m = 0, saw_b = 0, n = 10;
	char *p, tmp[10];
	
	for (i=0; i<n; i++) {
		tmp[i] = '\0';
	}

	p = str;
	i = 0;
	while (*p) {
		if ((*p == 'K' || *p == 'k') && !saw_k) {
			tmp[i++] = 'K';
			saw_k = 1;
		} else if ((*p == 'M' || *p == 'm') && !saw_m) {
			tmp[i++] = 'M';
			saw_m = 1;
		} else if ((*p == 'B' || *p == 'b') && !saw_b) {
			tmp[i++] = 'B';
			saw_b = 1;
		}
		p++;
	}
	return(strdup(tmp));
}

void initialize_allowed_input(void) {
	char *str;

	if (allowed_input_normal) {
		free(allowed_input_normal);
	}
	if (allowed_input_view_only) {
		free(allowed_input_view_only);
	}

	if (! allowed_input_str) {
		allowed_input_normal = strdup("KMB");
		allowed_input_view_only = strdup("");
	} else {
		char *p, *str = strdup(allowed_input_str);
		p = strchr(str, ',');
		if (p) {
			allowed_input_view_only = strdup(p+1);
			*p = '\0';
			allowed_input_normal = strdup(str);
		} else {
			allowed_input_normal = strdup(str);
			allowed_input_view_only = strdup("");
		}
		free(str);
	}

	/* shorten them */
	str = short_kmb(allowed_input_normal);
	free(allowed_input_normal);
	allowed_input_normal = str;

	str = short_kmb(allowed_input_view_only);
	free(allowed_input_view_only);
	allowed_input_view_only = str;

	if (screen) {
		rfbClientIteratorPtr iter;
		rfbClientPtr cl;

		iter = rfbGetClientIterator(screen);
		while( (cl = rfbClientIteratorNext(iter)) ) {
			ClientData *cd = (ClientData *) cl->clientData;

			if (cd->input[0] == '=') {
				;	/* custom setting */
			} else if (cd->login_viewonly) {
				if (*allowed_input_view_only != '\0') {
					cl->viewOnly = FALSE;
					cd->input[0] = '\0';
					strncpy(cd->input,
					    allowed_input_view_only, CILEN);
				} else {
					cl->viewOnly = TRUE;
				}
			} else {
				if (allowed_input_normal) {
					cd->input[0] = '\0';
					strncpy(cd->input,
					    allowed_input_normal, CILEN);
				}
			}
		}
		rfbReleaseClientIterator(iter);
	}
}

void initialize_keyboard_and_pointer(void) {

	if (raw_fb && ! dpy) return;	/* raw_fb hack */

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
#if 0
				sym =XKeysymToString(XKeycodeToKeysym(dpy,i,j));
#else
				keysym = keymap[(i-minkey)*syms_per_keycode+j];
				sym = XKeysymToString(keysym);
#endif
				fprintf(stderr, "%-18s ", sym ? sym : "null");
				if (j == syms_per_keycode - 1) {
					fprintf(stderr, "\n");
				}
			}
			if (j >= 4) {
				/*
				 * Something wacky in the keymapping.
				 * Ignore these non Shift/AltGr chords
				 * for now... n.b. we try to automatically
				 * switch to -xkb for this case.
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
		XTestFakeKeyEvent_wr(dpy, left_shift_code, !dn, CurrentTime);
	    }
	    if (mod_state & RIGHTSHIFT) {
		XTestFakeKeyEvent_wr(dpy, right_shift_code, !dn, CurrentTime);
	    }
	}
	if ( ! is_shift && mod == 1 ) {
	    XTestFakeKeyEvent_wr(dpy, left_shift_code, dn, CurrentTime);
	}
	if ( altgr && (mod_state & ALTGR) && mod != 2 ) {
	    XTestFakeKeyEvent_wr(dpy, altgr, !dn, CurrentTime);
	}
	if ( altgr && ! (mod_state & ALTGR) && mod == 2 ) {
	    XTestFakeKeyEvent_wr(dpy, altgr, dn, CurrentTime);
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
	if (k == NoSymbol && add_keysyms && ! IsModifierKey(keysym)) {
		int new_kc = add_keysym(keysym);
		if (new_kc) {
			k = new_kc;
		}
	}
	if (debug_keyboard) {
		rfbLog("modifier_tweak_keyboard: KeySym 0x%x \"%s\" -> "
		    "KeyCode 0x%x%s\n", (int) keysym, XKeysymToString(keysym),
		    (int) k, k ? "" : " *ignored*");
	}
	if ( k != NoSymbol ) {
		X_LOCK;
		XTestFakeKeyEvent_wr(dpy, k, (Bool) down, CurrentTime);
		X_UNLOCK;
	} 

	if ( tweak ) {
		tweak_mod(modifiers[keysym], False);
	}
}

typedef struct allowed_input {
	int keystroke;
	int motion;
	int button;
} allowed_input_t;

void get_allowed_input(rfbClientPtr client, allowed_input_t *input) {
	ClientData *cd;
	char *str;

	input->keystroke = 0;
	input->motion    = 0;
	input->button    = 0;

	if (! client) {
		return;
	}

	cd = (ClientData *) client->clientData;
	
	if (cd->input[0] != '-') {
		str = cd->input;
	} else if (client->viewOnly) {
		if (allowed_input_view_only) {
			str = allowed_input_view_only;
		} else {
			str = "";
		}
	} else {
		if (allowed_input_normal) {
			str = allowed_input_normal;
		} else {
			str = "KMB";
		}
	}

	while (*str) {
		if (*str == 'K') {
			input->keystroke = 1;
		} else if (*str == 'M') {
			input->motion = 1;
		} else if (*str == 'B') {
			input->button = 1;
		}
		str++;
	}
}

/* for -pipeinput mode */
void pipe_keyboard(rfbBool down, rfbKeySym keysym, rfbClientPtr client) {
	int can_input = 0, uid;
	allowed_input_t input;
	char *name;
	ClientData *cd = (ClientData *) client->clientData;

	if (pipeinput_fh == NULL) {
		return;
	}

	if (! view_only) {
		get_allowed_input(client, &input);
		if (input.motion || input.button) {
			can_input = 1;	/* XXX distinguish later */
		}
	}
	uid = cd->uid;
	if (! can_input) {
		uid = -uid;
	}

	X_LOCK;
	name = XKeysymToString(keysym);
	X_UNLOCK;

	fprintf(pipeinput_fh, "Keysym %d %d %u %s %s\n", uid, down,
	    keysym, name, down ? "KeyPress" : "KeyRelease");

	fflush(pipeinput_fh);
	check_pipeinput();
}

typedef struct keyevent {
	rfbKeySym sym;
	rfbBool down;
	double time;
} keyevent_t;

#define KEY_HIST 256
int key_history_idx = -1;
keyevent_t key_history[KEY_HIST];

double typing_rate(double time_window, int *repeating) {
	double dt = 1.0, now = dnow();
	KeySym key = NoSymbol;
	int i, idx, cnt = 0, repeat_keys = 0;

	if (key_history_idx == -1) {
		if (repeating) {
			*repeating = 0;
		}
		return 0.0;
	}
	if (time_window > 0.0) {
		dt = time_window;
	}
	for (i=0; i<KEY_HIST; i++) {
		idx = key_history_idx - i;
		if (idx < 0) {
			idx += KEY_HIST;
		}
		if (! key_history[idx].down) {
			continue;
		}
		if (now > key_history[idx].time + dt) {
			break;
		}
		cnt++;
		if (key == NoSymbol) {
			key = key_history[idx].sym;
			repeat_keys = 1;
		} else if (key == key_history[idx].sym) {
			repeat_keys++;
		}
	}

	if (repeating) {
		if (repeat_keys >= 2) {
			*repeating = repeat_keys;
		} else {
			*repeating = 0;
		}
	}

	/*
	 * n.b. keyrate could seem very high with libvncserver buffering them
	 * so avoid using small dt.
	 */
	return ((double) cnt)/dt;
}

/*
 * key event handler.  See the above functions for contortions for
 * running under -modtweak.
 */
static rfbClientPtr last_keyboard_client = NULL;

void keyboard(rfbBool down, rfbKeySym keysym, rfbClientPtr client) {
	KeyCode k;
	int idx, isbutton = 0;
	allowed_input_t input;
	time_t now = time(0);
	double tnow;
	static int skipped_last_down;
	static rfbBool last_down;
	static rfbKeySym last_keysym = NoSymbol;
	static rfbKeySym max_keyrepeat_last_keysym = NoSymbol;
	static double max_keyrepeat_last_time = 0.0;

	dtime0(&tnow);

	if (debug_keyboard) {
		char *str;
		X_LOCK;
		str = XKeysymToString(keysym);
		rfbLog("keyboard(%s, 0x%x \"%s\")  %.4f\n", down ? "down":"up",
		    (int) keysym, str ? str : "null", tnow - x11vnc_start);
		X_UNLOCK;
	}

	if (skip_duplicate_key_events) {
		if (keysym == last_keysym && down == last_down) {
			if (debug_keyboard) {
				rfbLog("skipping dup key event: %d 0x%x\n",
				    down, keysym);
			}
			return;
		}
	}

	last_down = down;
	last_keysym = keysym;
	last_keyboard_time = tnow;

	last_rfb_down = down;
	last_rfb_keysym = keysym;
	last_rfb_keytime = tnow;
	last_rfb_key_accepted = FALSE;

	if (key_history_idx == -1) {
		for (idx=0; idx<KEY_HIST; idx++) {
			key_history[idx].sym = NoSymbol;
			key_history[idx].down = FALSE;
			key_history[idx].time = 0.0;
		}
	}
	idx = ++key_history_idx;
	if (key_history_idx >= KEY_HIST) {
		key_history_idx = 0;
		idx = 0;
	}
	key_history[idx].sym = keysym;
	key_history[idx].down = down;
	key_history[idx].time = tnow;

	if (use_xdamage && down && skip_duplicate_key_events &&
	    (keysym == XK_Alt_L || keysym == XK_Super_L)) {
		int i, k, run = 0;
		double delay = 1.0;
		for (i=0; i<16; i++) {
			k = idx - i;
			if (k < 0) k += KEY_HIST;
			if (!key_history[k].down) {
				continue;
			}
			if (key_history[k].time < tnow - delay) {
				break;
			} else if (key_history[k].sym == XK_Alt_L) {
				run++;
			} else if (key_history[k].sym == XK_Super_L) {
				run++;
			} else {
				break;
			}
		}
		if (run == 3) {
			rfbLog("3*Alt_L, calling: set_xdamage_mark()\n");
			set_xdamage_mark(0, 0, dpy_x, dpy_y);
		} else if (run == 4) {
			rfbLog("4*Alt_L, calling: refresh_screen(0)\n");
			refresh_screen(0);
		} else if (run == 5) {
			rfbLog("5*Alt_L, setting: do_copy_screen\n");
			do_copy_screen = 1;
		}
	}

	if (!down && skipped_last_down) {
		int db = debug_scroll;
		if (keysym == max_keyrepeat_last_keysym) {
			skipped_last_down = 0;
			if (db) rfbLog("--- scroll keyrate skipping 0x%lx %s "
			    "%.4f  %.4f\n", keysym, down ? "down":"up  ",
			    tnow - x11vnc_start, tnow - max_keyrepeat_last_time); 
			return;
		}
	}
	if (down && max_keyrepeat_time > 0.0) {
		int skip = 0;
		int db = debug_scroll;

		if (max_keyrepeat_last_keysym != NoSymbol &&
		    max_keyrepeat_last_keysym != keysym) {
			;
		} else {
			if (tnow < max_keyrepeat_last_time+max_keyrepeat_time) {
				skip = 1;
			}
		}
		max_keyrepeat_time = 0.0;
		if (skip) {
			if (db) rfbLog("--- scroll keyrate skipping 0x%lx %s "
			    "%.4f  %.4f\n", keysym, down ? "down":"up  ",
			    tnow - x11vnc_start, tnow - max_keyrepeat_last_time); 
			max_keyrepeat_last_keysym = keysym;
			skipped_last_down = 1;
			return;
		} else {
			if (db) rfbLog("--- scroll keyrate KEEPING  0x%lx %s "
			    "%.4f  %.4f\n", keysym, down ? "down":"up  ",
			    tnow - x11vnc_start, tnow - max_keyrepeat_last_time); 
		}
	}
	max_keyrepeat_last_keysym = keysym;
	max_keyrepeat_last_time = tnow;
	skipped_last_down = 0;
	last_rfb_key_accepted = TRUE;

	if (pipeinput_fh != NULL) {
		pipe_keyboard(down, keysym, client);
		if (! pipeinput_tee) {
			if (! view_only || raw_fb) {	/* raw_fb hack */
				last_keyboard_client = client;
				last_event = last_input = now;
				last_keyboard_input = now;
		
				last_keysym = keysym;

				last_rfb_down = down;
				last_rfb_keysym = keysym;
				last_rfb_keytime = tnow;

				got_user_input++;
				got_keyboard_input++;
			}
			return;
		}
	}

	if (view_only) {
		return;
	}
	get_allowed_input(client, &input);
	if (! input.keystroke) {
		return;
	}

	last_keyboard_client = client;
	last_event = last_input = now;
	last_keyboard_input = now;

	last_keysym = keysym;

	last_rfb_down = down;
	last_rfb_keysym = keysym;
	last_rfb_keytime = tnow;

	got_user_input++;
	got_keyboard_input++;
	
	if (raw_fb && ! dpy) return;	/* raw_fb hack */

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

	if (use_xrecord && ! xrecording && down) {
		if (scaling && ! got_scrollcopyrect) {
			;
		} else if (!strcmp(scroll_copyrect, "never")) {
			;
		} else if (!strcmp(scroll_copyrect, "mouse")) {
			;
		} else if (! xrecord_skip_keysym(keysym)) {
			snapshot_stack_list(0, 0.25);
			xrecord_watch(1, SCR_KEY);
			xrecord_set_by_keys = 1;
			xrecord_keysym = keysym;
		} else {
			if (debug_scroll) {
				char *str = XKeysymToString(keysym);
				rfbLog("xrecord_skip_keysym: %s\n",
				    str ? str : "NoSymbol");
			}
		}
	}

	if (isbutton) {
		int mask, button = (int) keysym;
		if (! down) {
			return;	/* nothing to send */
		}
		if (debug_keyboard) {
			rfbLog("keyboard(): remapping keystroke to button %d"
			    " click\n", button);
		}
		dtime0(&last_key_to_button_remap_time);

		X_LOCK;
		/*
		 * This in principle can be a little dicey... i.e. even
		 * remap the button click to keystroke sequences!
		 * Usually just will simulate the button click.
		 */
		mask = 1<<(button-1);
		do_button_mask_change(mask, button);	/* down */
		mask = 0;
		do_button_mask_change(mask, button);	/* up */
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

	if (k == NoSymbol && add_keysyms && ! IsModifierKey(keysym)) {
		int new_kc = add_keysym(keysym);
		if (new_kc) {
			k = new_kc;
		}
	}
	if (debug_keyboard) {
		char *str = XKeysymToString(keysym);
		rfbLog("keyboard(): KeySym 0x%x \"%s\" -> KeyCode 0x%x%s\n",
		    (int) keysym, str ? str : "null", (int) k,
		    k ? "" : " *ignored*");
	}

	if ( k != NoSymbol ) {
		XTestFakeKeyEvent_wr(dpy, k, (Bool) down, CurrentTime);
		XFlush(dpy);
	}

	X_UNLOCK;
}

/* -- pointer.c -- */
/*
 * pointer event (motion and button click) handling routines.
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
				X_LOCK;
				ksym = XStringToKeysym(t); /* string value */
				X_UNLOCK;
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
				/*
				 * XXX may not work with -modtweak or -xkb
				 */
				X_LOCK;
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
				X_UNLOCK;
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
	X_UNLOCK;

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

	if (pointer_remap && *pointer_remap != '\0') {
		/* -buttonmap, format is like: 12-21=2 */
		char *p, *q, *remap = strdup(pointer_remap);	
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
		free(remap);
	}
}

/*
 * For use in the -wireframe stuff, save the stacking order of the direct
 * children of the root window.  Ideally done before we send ButtonPress
 * to the X server.
 */
void snapshot_stack_list(int free_only, double allowed_age) {
	static double last_snap = 0.0, last_free = 0.0;
	double now; 
	int num, rc, i;
	Window r, w;
	Window *list;

	if (! stack_list) {
		stack_list = (winattr_t *) malloc(256*sizeof(winattr_t));
		stack_list_num = 0;
		stack_list_len = 256;
	}

	dtime0(&now);
	if (free_only) {
		/* we really don't free it, just reset to zero windows */
		stack_list_num = 0;
		last_free = now;
		return;
	}

	if (stack_list_num && now < last_snap + allowed_age) {
		return;
	}

	stack_list_num = 0;
	last_free = now;

	X_LOCK;
	rc = XQueryTree(dpy, rootwin, &r, &w, &list, &num);

	if (! rc) {
		stack_list_num = 0;
		last_free = now;
		last_snap = 0.0;
		X_UNLOCK;
		return;
	}

	last_snap = now;
	if (num > stack_list_len) {
		int n = 2*num;
		free(stack_list);
		stack_list = (winattr_t *) malloc(n*sizeof(winattr_t));
		stack_list_len = n;
	}
	for (i=0; i<num; i++) {
		stack_list[i].win = list[i];
		stack_list[i].fetched = 0;
		stack_list[i].valid = 0;
		stack_list[i].time = now;
	}
	stack_list_num = num;

	XFree(list);
	X_UNLOCK;
}

void update_stack_list(void) {
	int k;
	double now;
	XWindowAttributes attr;

	if (! stack_list) {
		return;
	}
	if (! stack_list_num) {
		return;
	}

	dtime0(&now);
	
	for (k=0; k < stack_list_num; k++) {
		Window win = stack_list[k].win;
		if (!valid_window(win, &attr, 1)) {
			stack_list[k].valid = 0;
		} else {
			stack_list[k].valid = 1;
			stack_list[k].x = attr.x;
			stack_list[k].y = attr.y;
			stack_list[k].width = attr.width;
			stack_list[k].height = attr.height;
			stack_list[k].depth = attr.depth;
			stack_list[k].class = attr.class;
			stack_list[k].backing_store = attr.backing_store;
			stack_list[k].map_state = attr.map_state;

			/* root_x, root_y not used for stack_list usage: */
			stack_list[k].rx = -1;
			stack_list[k].ry = -1;
		}
		stack_list[k].fetched = 1;
		stack_list[k].time = now;
	}
if (0) fprintf(stderr, "update_stack_list[%d]: %.4f  %.4f\n", stack_list_num, now - x11vnc_start, dtime(&now));
}

/*
 * Send a pointer position event to the X server.
 */
static void update_x11_pointer_position(int x, int y) {
	int rc;

	if (raw_fb && ! dpy) return;	/* raw_fb hack */

	X_LOCK;
	if (use_xwarppointer) {
		/*
		 * off_x and off_y not needed with XWarpPointer since
		 * window is used:
		 */
		XWarpPointer(dpy, None, window, 0, 0, 0, 0, x + coff_x,
		    y + coff_y);
	} else {
		XTestFakeMotionEvent_wr(dpy, scr, x + off_x + coff_x,
		    y + off_y + coff_y, CurrentTime);
	}
	X_UNLOCK;

	if (cursor_x != x || cursor_y != y) {
		last_pointer_motion_time = dnow();
	}

	cursor_x = x;
	cursor_y = y;

	/* record the x, y position for the rfb screen as well. */
	cursor_position(x, y);

	/* change the cursor shape if necessary */
	rc = set_cursor(x, y, get_which_cursor());
	cursor_changes += rc;

	last_event = last_input = last_pointer_input = time(0);
}

void do_button_mask_change(int mask, int button) {
	int mb, k, i = button-1;

	/*
	 * this expands to any pointer_map button -> keystrokes
	 * remappings.  Usually just k=0 and we send one button event.
	 */
	for (k=0; k < MAX_BUTTON_EVENTS; k++) {
		int bmask = (mask & (1<<i));

		if (pointer_map[i+1][k].end) {
			break;
		}

		if (pointer_map[i+1][k].button) {
			/* send button up or down */

			mb = pointer_map[i+1][k].button;
			if ((num_buttons && mb > num_buttons) || mb < 1) {
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
			XTestFakeButtonEvent_wr(dpy, mb, (mask & (1<<i))
			    ? True : False, CurrentTime);
		} else {
			/* send keysym up or down */
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
				XTestFakeKeyEvent_wr(dpy, key, True,
				    CurrentTime);
			}
			if (up) {
				XTestFakeKeyEvent_wr(dpy, key, False,
				    CurrentTime);
			}
		}
	}
}

/*
 * Send a pointer button event to the X server.
 */
static void update_x11_pointer_mask(int mask) {
	int snapped, xr_mouse = 1, i;

	last_event = last_input = last_pointer_input = time(0);

	if (raw_fb && ! dpy) return;	/* raw_fb hack */

	if (mask != button_mask) {
		last_pointer_click_time = dnow();
	}

	if (scaling && ! got_scrollcopyrect) {
		xr_mouse = 0;
	} else if (nofb) {
		xr_mouse = 0;
	} else if (!strcmp(scroll_copyrect, "never")) {
		xr_mouse = 0;
	} else if (!strcmp(scroll_copyrect, "keys")) {
		xr_mouse = 0;
	} else if (xrecord_skip_button(mask, button_mask)) {
		xr_mouse = 0;
	}

	if (mask && use_xrecord && ! xrecording && xr_mouse) {
		static int px, py, x, y, w, h, got_wm_frame;
		static XWindowAttributes attr;
		Window frame = None, mwin = None;
		int skip = 0;

		if (!button_mask) {
			if (get_wm_frame_pos(&px, &py, &x, &y, &w, &h,
			    &frame, &mwin)) {
				got_wm_frame = 1;
if (debug_scroll > 1) fprintf(stderr, "wm_win: 0x%lx\n", mwin);
				if (mwin != None) {
					if (!valid_window(mwin, &attr, 1)) {
						mwin = None;
					}
				}
			} else {
				got_wm_frame = 0;
			}
		}
		if (got_wm_frame) {
			if (wireframe && near_wm_edge(x, y, w, h, px, py)) {
				/* step out of wireframe's way */
				skip = 1;
			} else {
				int ok = 0;
				int btn4 = (1<<3);
				int btn5 = (1<<4);

				if (near_scrollbar_edge(x, y, w, h, px, py)) {
					ok = 1;
				}
				if (mask & (btn4|btn5)) {
					/* scroll wheel mouse */
					ok = 1;
				}
				if (mwin != None) {
					/* skinny internal window */
					int w = attr.width;
					int h = attr.height;
					if (h > 10 * w || w > 10 * h) {
if (debug_scroll > 1) fprintf(stderr, "internal scrollbar: %dx%d\n", w, h);
						ok = 1;
					}
				}
				if (! ok) {
					skip = 1;
				}
			}
		}

		if (! skip) {
			xrecord_watch(1, SCR_MOUSE);
			snapshot_stack_list(0, 0.50);
			snapped = 1;
			if (button_mask) {
				xrecord_set_by_mouse = 1;
			} else {
				update_stack_list();
				xrecord_set_by_mouse = 2;
			}
		}
	}

	if (mask && !button_mask) {
		/* button down, snapshot the stacking list before flushing */
		if (wireframe && !wireframe_in_progress &&
		    strcmp(wireframe_copyrect, "never")) {
			if (! snapped) {
				snapshot_stack_list(0, 0.0);
			}
		}
	}

	X_LOCK;

	/* look for buttons that have be clicked or released: */
	for (i=0; i < MAX_BUTTONS; i++) {
	    if ( (button_mask & (1<<i)) != (mask & (1<<i)) ) {
		if (debug_pointer) {
			rfbLog("pointer(): mask change: mask: 0x%x -> "
			    "0x%x button: %d\n", button_mask, mask,i+1);
		}
		do_button_mask_change(mask, i+1);	/* button # is i+1 */
	    }
	}

	X_UNLOCK;

	/*
	 * Remember the button state for next time and also for the
	 * -nodragging case:
	 */
	button_mask_prev = button_mask;
	button_mask = mask;
}

/* for -pipeinput */


void pipe_pointer(int mask, int x, int y, rfbClientPtr client) {
	int can_input = 0, uid;
	allowed_input_t input;
	ClientData *cd = (ClientData *) client->clientData;
	char hint[MAX_BUTTONS * 20];

	if (pipeinput_fh == NULL) {
		return;
	}

	if (! view_only) {
		get_allowed_input(client, &input);
		if (input.motion || input.button) {
			can_input = 1;	/* XXX distinguish later */
		}
	}
	uid = cd->uid;
	if (! can_input) {
		uid = -uid;
	}

	hint[0] = '\0';
	if (mask == button_mask) {
		strcat(hint, "None");
	} else {
		int i, old, new, m = 1, cnt = 0;
		for (i=0; i<MAX_BUTTONS; i++) {
			char s[20];

			old = button_mask & m;
			new = mask & m;
			m = m << 1;

			if (old == new) {
				continue;
			}
			if (hint[0] != '\0') {
				strcat(hint, ",");
			}
			if (new && ! old) {
				sprintf(s, "ButtonPress-%d", i+1);
				cnt++;
			} else if (! new && old)  {
				sprintf(s, "ButtonRelease-%d", i+1);
				cnt++;
			}
			strcat(hint, s);
		}
		if (! cnt) {
			strcpy(hint, "None");
		}
	}

	fprintf(pipeinput_fh, "Pointer %d %d %d %d %s\n", uid, x, y,
	    mask, hint);
	fflush(pipeinput_fh);
	check_pipeinput();
}

/*
 * Actual callback from libvncserver when it gets a pointer event.
 * This may queue pointer events rather than sending them immediately
 * to the X server. (see update_x11_pointer*())
 */
void pointer(int mask, int x, int y, rfbClientPtr client) {
	allowed_input_t input;
	int sent = 0, buffer_it = 0;
	double now;

	if (debug_pointer && mask >= 0) {
		static int show_motion = -1;
		static double last_pointer = 0.0;
		double tnow, dt;
		static int last_x, last_y;
		if (show_motion == -1) {
			if (getenv("X11VNC_DB_NOMOTION")) {
				show_motion = 0;
			} else {
				show_motion = 1;
			}
		}
		dtime0(&tnow);
		tnow -= x11vnc_start;
		dt = tnow - last_pointer;
		last_pointer = tnow;
		if (show_motion) {
			rfbLog("pointer(mask: 0x%x, x:%4d, y:%4d) "
			    "dx: %3d dy: %3d dt: %.4f t: %.4f\n", mask, x, y,
			    x - last_x, y - last_y, dt, tnow);
		}
		last_x = x;
		last_y = y;
	}

	if (scaling) {
		/* map from rfb size to X11 size: */
		x = ((double) x / scaled_x) * dpy_x;
		x = nfix(x, dpy_x);
		y = ((double) y / scaled_y) * dpy_y;
		y = nfix(y, dpy_y);
	}

	if (pipeinput_fh != NULL && mask >= 0) {
		pipe_pointer(mask, x, y, client);
		if (! pipeinput_tee) {
			if (! view_only || raw_fb) {	/* raw_fb hack */
				got_user_input++;
				got_keyboard_input++;
				last_pointer_client = client;
			}
			if (view_only && raw_fb) {
				/* raw_fb hack track button state */
				button_mask_prev = button_mask;
				button_mask = mask;
			}
			return;
		}
	}

	if (view_only) {
		return;
	}

	now = dnow();

	if (mask >= 0) {
		/*
		 * mask = -1 is a special case call from scan_for_updates()
		 * to flush the event queue; there is no real pointer event.
		 */
		get_allowed_input(client, &input);
		if (! input.motion && ! input.button) {
			return;
		}

		got_user_input++;
		got_pointer_input++;
		last_pointer_client = client;

		last_pointer_time = now;
	}

	/*
	 * The following is hopefully an improvement wrt response during
	 * pointer user input (window drags) for the threaded case.
	 * See check_user_input() for the more complicated things we do
	 * in the non-threaded case.
	 */
	if ((use_threads && pointer_mode != 1) || pointer_flush_delay > 0.0) {
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

		if (pointer_flush_delay > 0.0) {
			maxwait = pointer_flush_delay;
		}
		if (mask >= 0) {
			if (fb_copy_in_progress || pointer_flush_delay > 0.0) {
				buffer_it = 1;
			}
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
		if (buffer_it) {
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
				if (! input.button) {
					ev[i][0] = -1;
				}
				if (! input.motion) {
					ev[i][1] = -1;
					ev[i][2] = -1;
				}
				UNLOCK(pointerMutex);
				if (debug_pointer) {
					rfbLog("pointer(): deferring event %d"
					    " %.4f\n", i, tmr - x11vnc_start);
				}
				return;
			}
		}

		/* time to send the queue */
		for (i=0; i<nevents; i++) {
			int sent = 0;
			if (mask < 0 && client != NULL) {
				/* hack to only push the latest event */
				if (i < nevents - 1) {
					if (debug_pointer) {
						rfbLog("- skip deferred event:"
						    " %d\n", i);
					}
					continue;
				}
			}
			if (debug_pointer) {
				rfbLog("pointer(): sending event %d %.4f\n",
				    i+1, dnow() - x11vnc_start);
			}
			if (ev[i][1] >= 0) {
				update_x11_pointer_position(ev[i][1], ev[i][2]);
				sent = 1;
			}
			if (ev[i][0] >= 0) {
				update_x11_pointer_mask(ev[i][0]);
				sent = 1;
			}

			if (sent) {
				pointer_queued_sent++;
			}
		}
		if (nevents && dt > maxwait) {
		    if (dpy) {	/* raw_fb hack */
			if (mask < 0) {
				if (debug_pointer) {
					rfbLog("pointer(): calling XFlush "
					    "%.4f\n", dnow() - x11vnc_start);
				}
				X_LOCK;
				XFlush(dpy);	
				X_UNLOCK;
			}
		    }
		}
		nevents = 0;	/* reset everything */
		dt = 0.0;
		dtime0(&tmr);

		UNLOCK(pointerMutex);
	}
	if (mask < 0) {		/* -1 just means flush the event queue */
		if (debug_pointer) {
			rfbLog("pointer(): flush only.  %.4f\n",
			    dnow() - x11vnc_start);
		}
		return;
	}

	/* update the X display with the event: */
	if (input.motion) {
		update_x11_pointer_position(x, y);
		sent = 1;
	}
	if (input.button) {
		if (mask != button_mask) {
			button_change_x = cursor_x;
			button_change_y = cursor_y;
		}
		update_x11_pointer_mask(mask);
		sent = 1;
	}

	if (nofb && sent) {
		/* 
		 * nofb is for, e.g. Win2VNC, where fastest pointer
		 * updates are desired.
		 */
		X_LOCK;
		XFlush(dpy);
		X_UNLOCK;
	} else if (buffer_it) {
		if (debug_pointer) {
			rfbLog("pointer(): calling XFlush+"
			    "%.4f\n", dnow() - x11vnc_start);
		}
		X_LOCK;
		XFlush(dpy);	
		X_UNLOCK;
	}
}

int check_pipeinput(void) {
	if (! pipeinput_fh) {
		return 1;
	}
	if (ferror(pipeinput_fh)) {
		rfbLog("pipeinput pipe has ferror. %p\n", pipeinput_fh);
		
		if (pipeinput_opts && strstr(pipeinput_opts, "reopen")) {
			rfbLog("restarting -pipeinput pipe...\n");
			initialize_pipeinput();
			if (pipeinput_fh) {
				return 1;
			} else {
				return 0;
			}
		} else {
			rfbLog("closing -pipeinput pipe...\n");
			pclose(pipeinput_fh);
			pipeinput_fh = NULL;
			return 0;
		}
	}
	return 1;
}

void initialize_pipeinput(void) {
	char *p;

	if (pipeinput_fh != NULL) {
		rfbLog("closing pipeinput stream: %p\n", pipeinput_fh);
		pclose(pipeinput_fh);
		pipeinput_fh = NULL;
	}

	pipeinput_tee = 0;
	if (pipeinput_opts) {
		free(pipeinput_opts);
		pipeinput_opts = NULL;
	}

	if (! pipeinput_str) {
		return;
	}

	/* look for options:  tee, reopen, ... */
	p = strchr(pipeinput_str, ':');
	if (p != NULL) {
		char *str, *opt, *q;
		int got = 0;
		*p = '\0';
		str = strdup(pipeinput_str);
		opt = strdup(pipeinput_str);
		*p = ':';
		q = strtok(str, ",");
		while (q) {
			if (!strcmp(q, "key") || !strcmp(q, "keycodes")) {
				got = 1;
			}
			if (!strcmp(q, "reopen")) {
				got = 1;
			}
			if (!strcmp(q, "tee")) {
				pipeinput_tee = 1;
				got = 1;
			}
			q = strtok(NULL, ",");
		}
		if (got) {
			pipeinput_opts = opt;
		} else {
			free(opt);
		}
		free(str);
		p++;
	} else {
		p = pipeinput_str;
	}

	set_child_info();
	if (no_external_cmds) {
		rfbLog("cannot run external commands in -nocmds mode:\n");
		rfbLog("   \"%s\"\n", p);
		rfbLog("   exiting.\n");
		clean_up_exit(1);
	}
	rfbLog("pipeinput: starting: \"%s\"...\n", p);
	pipeinput_fh = popen(p, "w");

	if (! pipeinput_fh) {
		rfbLog("popen(\"%s\", \"w\") failed.\n", p);
		rfbLogPerror("popen");
		rfbLog("Disabling -pipeinput mode.\n");
		return;
	}

	fprintf(pipeinput_fh, "%s",
"# \n"
"# Format of the -pipeinput stream:\n"
"# --------------------------------\n"
"#\n"
"# Lines like these beginning with '#' are to be ignored.\n"
"#\n"
"# Pointer events (mouse motion and button clicks) come in the form:\n"
"#\n"
"#\n"
"# Pointer <client#> <x> <y> <mask> <hint>\n"
"#\n"
"#\n"
"# The <client#> is a decimal integer uniquely identifying the client\n"
"# that generated the event.  If it is negative that means this event\n"
"# would have been discarded since the client was viewonly.\n"
"#\n"
"# <x> and <y> are decimal integers reflecting the position on the screen\n"
"# the event took place at.\n"
"#\n"
"# <mask> is the button mask indicating the button press state, as normal\n"
"# 0 means no buttons pressed, 1 means button 1 is down 3 (11) means buttons\n"
"# 1 and 2 are down, etc.\n"
"#\n"
"# <hint> is a string containing no spaces and may be ignored.\n"
"# It contains some interpretation about what has happened.\n"
"# It can be:\n"
"#\n"
"#	None		(nothing to report)\n"
"#	ButtonPress-N	(this event will cause button-1 to be pressed) \n"
"#	ButtonRelease-N	(this event will cause button-1 to be released) \n"
"#\n"
"# if two more more buttons change state in one event they are listed\n"
"# separated by commas.\n"
"#\n"
"# One might parse a Pointer line with:\n"
"#\n"
"# int client, x, y, mask; char *hint;\n"
"# sscanf(line, \"Pointer %d %d %d %s\", &client, &x, &y, &mask, &hint);\n"
"#\n"
"#\n"
"# Keysym events (keyboard presses and releases) come in the form:\n"
"#\n"
"#\n"
"# Keysym <client#> <down> <keysym#> <keysym-name> <hint>\n"
"#\n"
"#\n"
"# The <client#> is as with Pointer.\n"
"#\n"
"# <down> is a decimal either 1 or 0 indicating KeyPress or KeyRelease,\n"
"# respectively.\n"
"#\n"
"# <keysym#> is a decimal integer incidating the Keysym of the event.\n"
"#\n"
"# <keysym-name> is the corresponding Keysym name.\n"
"#\n"
"# See the file /usr/include/X11/keysymdef.h for the mappings.\n"
"# You basically remove the leading 'XK_' prefix from the macro name in\n"
"# that file to get the Keysym name.\n"
"#\n"
"# One might parse a Keysym line with:\n"
"#\n"
"# int client, down, keysym; char *name, *hint;\n"
"# sscanf(line, \"Keysym %d %d %s %s\", &client, &down, &keysym, &name, &hint);\n"
"#\n"
"# The <hint> value is currently just None, KeyPress, or KeyRelease.\n"
"#\n"
"# In the future <hint> will provide a hint for the sequence of KeyCodes\n"
"# (i.e. keyboard scancodes) that x11vnc would inject to an X display to\n"
"# simulate the Keysym.\n"
"#\n"
"# You see, some Keysyms will require more than one injected Keycode to\n"
"# generate the symbol.  E.g. the Keysym \"ampersand\" going down usually\n"
"# requires a Shift key going down, then the key with the \"&\" on it going\n"
"# down, and, perhaps, the Shift key going up (that is how x11vnc does it).\n"
"#\n"
"# The Keysym => Keycode(s) stuff gets pretty messy.  Hopefully the Keysym\n"
"# info will be enough for most purposes (having identical keyboards on\n"
"# both sides helps).\n"
"#\n"
"# Here comes your stream.  The following token will always indicate the\n"
"# end of this informational text:\n"
"# END_OF_TOP\n"
);
	fflush(pipeinput_fh);
	if (raw_fb_str) {
		/* the pipe program may actually create the fb */
		sleep(1);
	}
}

/* -- xkb_bell.c -- */
/*
 * Bell event handling.  Requires XKEYBOARD extension.
 */
int xkb_base_event_type = 0;

#if LIBVNCSERVER_HAVE_XKEYBOARD
/*
 * check for XKEYBOARD, set up xkb_base_event_type
 */
void initialize_xkb(void) {
	int ir, reason;
	int op, ev, er, maj, min;
	
	if (xkbcompat) {
		xkb_present = 0;
	} else if (! XkbQueryExtension(dpy, &op, &ev, &er, &maj, &min)) {
		if (! quiet) {
			rfbLog("warning: XKEYBOARD extension not present.\n");
		}
		xkb_present = 0;
	} else {
		xkb_present = 1;
	}

	if (! xkb_present) {
		return;
	}

	if (! XkbOpenDisplay(DisplayString(dpy), &xkb_base_event_type, &ir,
	    NULL, NULL, &reason) ) {
		if (! quiet) {
			rfbLog("warning: disabling XKEYBOARD. XkbOpenDisplay"
			    " failed.\n");
		}
		xkb_base_event_type = 0;
		xkb_present = 0;
	}
}

void initialize_watch_bell(void) {
	if (! xkb_present) {
		if (! quiet) {
			rfbLog("warning: disabling bell. XKEYBOARD ext. "
			    "not present.\n");
		}
		watch_bell = 0;
		sound_bell = 0;
		return;
	}

	XkbSelectEvents(dpy, XkbUseCoreKbd, XkbBellNotifyMask, 0);

	if (! watch_bell) {
		return;
	}
	if (! XkbSelectEvents(dpy, XkbUseCoreKbd, XkbBellNotifyMask,
	    XkbBellNotifyMask) ) {
		if (! quiet) {
			rfbLog("warning: disabling bell. XkbSelectEvents"
			    " failed.\n");
		}
		watch_bell = 0;
		sound_bell = 0;
	}
}

/*
 * We call this periodically to process any bell events that have 
 * taken place.
 */
void check_bell_event(void) {
	XEvent xev;
	XkbAnyEvent *xkb_ev;
	int got_bell = 0;

	if (! xkb_base_event_type) {
		return;
	}

	/* caller does X_LOCK */
	if (! XCheckTypedEvent(dpy, xkb_base_event_type, &xev)) {
		return;
	}
	if (! watch_bell) {
		/* we return here to avoid xkb events piling up */
		return;
	}

	xkb_ev = (XkbAnyEvent *) &xev;
	if (xkb_ev->xkb_type == XkbBellNotify) {
		got_bell = 1;
	}

	if (got_bell && sound_bell) {
		if (! all_clients_initialized()) {
			rfbLog("check_bell_event: not sending bell: "
			    "uninitialized clients\n");
		} else {
			if (screen && client_count) {
				rfbSendBell(screen);
			}
		}
	}
}
#else
void initialize_watch_bell(void) {
	watch_bell = 0;
	sound_bell = 0;
}
void check_bell_event(void) {}
#endif

/* -- xrandr.h -- */

time_t last_subwin_trap = 0;
int subwin_trap_count = 0;

XErrorHandler old_getimage_handler;
#define XRANDR_SET_TRAP_RET(x,y)  \
	if (subwin || xrandr) { \
		trapped_getimage_xerror = 0; \
		old_getimage_handler = XSetErrorHandler(trap_getimage_xerror); \
		if (check_xrandr_event(y)) { \
			trapped_getimage_xerror = 0; \
			XSetErrorHandler(old_getimage_handler);	 \
			return(x); \
		} \
	}
#define XRANDR_CHK_TRAP_RET(x,y)  \
	if (subwin || xrandr) { \
		if (trapped_getimage_xerror) { \
			if (subwin) { \
				static int last = 0; \
				subwin_trap_count++; \
				if (time(0) > last_subwin_trap + 60) { \
					rfbLog("trapped GetImage xerror" \
					    " in SUBWIN mode. [%d]\n", \
					    subwin_trap_count); \
					last_subwin_trap = time(0); \
					last = subwin_trap_count; \
				} \
				if (subwin_trap_count - last > 30) { \
					/* window probably iconified */ \
					usleep(1000*1000); \
				} \
			} else { \
				rfbLog("trapped GetImage xerror" \
				    " in XRANDR mode.\n"); \
			} \
			trapped_getimage_xerror = 0; \
			XSetErrorHandler(old_getimage_handler);	 \
			check_xrandr_event(y); \
			X_UNLOCK; \
			return(x); \
		} \
	}

/* -- xrandr.c -- */

void initialize_xrandr(void) {
	if (xrandr_present) {
#if LIBVNCSERVER_HAVE_LIBXRANDR
		Rotation rot;

		X_LOCK;
		xrandr_width  = XDisplayWidth(dpy, scr);
		xrandr_height = XDisplayHeight(dpy, scr);
		XRRRotations(dpy, scr, &rot);
		xrandr_rotation = (int) rot;
		if (xrandr) {
			XRRSelectInput(dpy, rootwin, RRScreenChangeNotifyMask);
		} else {
			XRRSelectInput(dpy, rootwin, 0);
		}
		X_UNLOCK;
#endif
	} else if (xrandr) {
		rfbLog("-xrandr mode specified, but no RANDR support on\n");
		rfbLog(" display or in client library. Disabling -xrandr "
		    "mode.\n");
		xrandr = 0;
	}
}

void handle_xrandr_change(int, int);

int handle_subwin_resize(char *msg) {
	int new_x, new_y;
	int i, check = 10, ms = 250;	/* 2.5 secs total... */

	if (! subwin) {
		return 0;	/* hmmm... */
	}
	if (! valid_window(subwin, NULL, 0)) {
		rfbLog("subwin 0x%lx went away!\n", subwin);
		X_UNLOCK;
		clean_up_exit(1);
	}
	if (! get_window_size(subwin, &new_x, &new_y)) {
		rfbLog("could not get size of subwin 0x%lx\n", subwin);
		X_UNLOCK;
		clean_up_exit(1);
	}
	if (wdpy_x == new_x && wdpy_y == new_y) {
		/* no change */
		return 0;
	}

	/* window may still be changing (e.g. drag resize) */
	for (i=0; i < check; i++) {
		int newer_x, newer_y;
		usleep(ms * 1000);

		if (! get_window_size(subwin, &newer_x, &newer_y)) {
			rfbLog("could not get size of subwin 0x%lx\n", subwin);
			clean_up_exit(1);
		}
		if (new_x == newer_x && new_y == newer_y) {
			/* go for it... */
			break;
		} else {
			rfbLog("subwin 0x%lx still changing size...\n", subwin);
			new_x = newer_x;
			new_y = newer_y;
		}
	}

	rfbLog("subwin 0x%lx new size: x: %d -> %d, y: %d -> %d\n",
	    subwin, wdpy_x, new_x, wdpy_y, new_y);
	rfbLog("calling handle_xrandr_change() for resizing\n");

	X_UNLOCK;
	handle_xrandr_change(new_x, new_y);
	return 1;
}

int known_xrandr_mode(char *);

void handle_xrandr_change(int new_x, int new_y) {
	rfbClientIteratorPtr iter;
	rfbClientPtr cl;

	/* sanity check xrandr_mode */
	if (! xrandr_mode) {
		xrandr_mode = strdup("default");
	} else if (! known_xrandr_mode(xrandr_mode)) {
		free(xrandr_mode);
		xrandr_mode = strdup("default");
	}
	rfbLog("xrandr_mode: %s\n", xrandr_mode);
	if (!strcmp(xrandr_mode, "exit")) {
		close_all_clients();
		rfbLog("  shutting down due to XRANDR event.\n");
		clean_up_exit(0);
	}
	if (!strcmp(xrandr_mode, "newfbsize") && screen) {
		iter = rfbGetClientIterator(screen);
		while( (cl = rfbClientIteratorNext(iter)) ) {
			if (cl->useNewFBSize) {
				continue;
			}
			rfbLog("  closing client %s (no useNewFBSize"
			    " support).\n", cl->host);
			rfbCloseClient(cl);
			rfbClientConnectionGone(cl);
		}
		rfbReleaseClientIterator(iter);
	}
	
	/* default, resize, and newfbsize create a new fb: */
	rfbLog("check_xrandr_event: trying to create new framebuffer...\n");
	if (new_x < wdpy_x || new_y < wdpy_y) {
		check_black_fb();
	}
	do_new_fb(1);
	rfbLog("check_xrandr_event: fb       WxH: %dx%d\n", wdpy_x, wdpy_y);
}

int check_xrandr_event(char *msg) {
	XEvent xev;
	if (subwin) {
		return handle_subwin_resize(msg);
	}
#if LIBVNCSERVER_HAVE_LIBXRANDR
	if (! xrandr || ! xrandr_present) {
		return 0;
	}
	if (xrandr_base_event_type && XCheckTypedEvent(dpy,
	    xrandr_base_event_type + RRScreenChangeNotify, &xev)) {
		int do_change;
		XRRScreenChangeNotifyEvent *rev;

		rev = (XRRScreenChangeNotifyEvent *) &xev;
		rfbLog("check_xrandr_event():\n");
		rfbLog("Detected XRANDR event at location '%s':\n", msg);
		rfbLog("  serial:          %d\n", (int) rev->serial);
		rfbLog("  timestamp:       %d\n", (int) rev->timestamp);
		rfbLog("  cfg_timestamp:   %d\n", (int) rev->config_timestamp);
		rfbLog("  size_id:         %d\n", (int) rev->size_index);
		rfbLog("  sub_pixel:       %d\n", (int) rev->subpixel_order);
		rfbLog("  rotation:        %d\n", (int) rev->rotation);
		rfbLog("  width:           %d\n", (int) rev->width);
		rfbLog("  height:          %d\n", (int) rev->height);
		rfbLog("  mwidth:          %d mm\n", (int) rev->mwidth);
		rfbLog("  mheight:         %d mm\n", (int) rev->mheight);
		rfbLog("\n");
		rfbLog("check_xrandr_event: previous WxH: %dx%d\n",
		    wdpy_x, wdpy_y);
		if (wdpy_x == rev->width && wdpy_y == rev->height &&
		    xrandr_rotation == (int) rev->rotation) {
		    rfbLog("check_xrandr_event: no change detected.\n");
			do_change = 0;
		} else {
			do_change = 1;
		}

		xrandr_width  = rev->width;
		xrandr_height = rev->height;
		xrandr_timestamp = rev->timestamp;
		xrandr_cfg_time  = rev->config_timestamp;
		xrandr_rotation = (int) rev->rotation;

		rfbLog("check_xrandr_event: updating config...\n");
		XRRUpdateConfiguration(&xev);

		if (do_change) {
			X_UNLOCK;
			handle_xrandr_change(rev->width, rev->height);
		}
		rfbLog("check_xrandr_event: current  WxH: %dx%d\n",
		    XDisplayWidth(dpy, scr), XDisplayHeight(dpy, scr));
		rfbLog("check_xrandr_event(): returning control to"
		    " caller...\n");
		return do_change;
	}
#endif
	return 0;
}

int known_xrandr_mode(char *s) {
/*
 * default:	
 * resize:	the default
 * exit:	shutdown clients and exit.
 * newfbsize:	shutdown clients that do not support NewFBSize encoding.
 */
	if (strcmp(s, "default") && strcmp(s, "resize") && 
	    strcmp(s, "exit") && strcmp(s, "newfbsize")) {
		return 0;
	} else {
		return 1;
	}
}

int known_sigpipe_mode(char *s) {
/*
 * skip, ignore, exit
 */
	if (strcmp(s, "skip") && strcmp(s, "ignore") && 
	    strcmp(s, "exit")) {
		return 0;
	} else {
		return 1;
	}
}

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
static char *xcut_str = NULL;

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
	if (xcut_str) {
		length = strlen(xcut_str);
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

		data = (unsigned char *)xcut_str;

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
	if (!screen) {
		return;
	}
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
 *
 * Also: XFIXES has XFixesSelectSelectionInput().
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
	if (!screen) {
		return;
	}
	rfbSendServerCutText(screen, selection_str, newlen);
}

/* -- xevents.c -- */

void initialize_vnc_connect_prop() {
	vnc_connect_str[0] = '\0';
	vnc_connect_prop = XInternAtom(dpy, "VNC_CONNECT", False);
}

void initialize_xevents(void) {
	static int did_xselect_input = 0;
	static int did_xcreate_simple_window = 0;
	static int did_vnc_connect_prop = 0;
	static int did_xfixes = 0;
	static int did_xdamage = 0;
	static int did_xrandr = 0;

	if ((watch_selection || vnc_connect) && !did_xselect_input) {
		/*
		 * register desired event(s) for notification.
		 * PropertyChangeMask is for CUT_BUFFER0 changes.
		 * XXX: does this cause a flood of other stuff?
		 */
		X_LOCK;
		XSelectInput(dpy, rootwin, PropertyChangeMask);
		X_UNLOCK;
		did_xselect_input = 1;
	}
	if (watch_selection && !did_xcreate_simple_window) {
		/* create fake window for our selection ownership, etc */

		X_LOCK;
		selwin = XCreateSimpleWindow(dpy, rootwin, 0, 0, 1, 1, 0, 0, 0);
		X_UNLOCK;
		did_xcreate_simple_window = 1;
	}

	if (xrandr && !did_xrandr) {
		initialize_xrandr();
		did_xrandr = 1;
	}
	if (vnc_connect && !did_vnc_connect_prop) {
		initialize_vnc_connect_prop();
		did_vnc_connect_prop = 1;
	}
	if (xfixes_present && use_xfixes && !did_xfixes) {
		initialize_xfixes();
		did_xfixes = 1;
	}
	if (xdamage_present && !did_xdamage) {
		initialize_xdamage();
		did_xdamage = 1;
	}
}

void print_xevent_bases(void) {
	fprintf(stderr, "X event bases: xkb=%d, xtest=%d, xrandr=%d, "
	    "xfixes=%d, xdamage=%d, xtrap=%d\n", xkb_base_event_type,
	    xtest_base_event_type, xrandr_base_event_type,
	    xfixes_base_event_type, xdamage_base_event_type,
	    xtrap_base_event_type);
	fprintf(stderr, "  MapNotify=%d, ClientMsg=%d PropNotify=%d "
	    "SelNotify=%d, SelRequest=%d\n", MappingNotify, ClientMessage,
	    PropertyNotify, SelectionNotify, SelectionRequest);
	fprintf(stderr, "  SelClear=%d, Expose=%d\n", SelectionClear, Expose);
}

void sync_tod_with_servertime() {
	static Atom servertime = None;
	XEvent xev;
	char diff[64];
	static int seq = 0;
	int i, db = 0;

	if (! servertime) {
		servertime = XInternAtom(dpy, "X11VNC_SERVERTIME_DIFF", False);
	}
	if (! servertime) {
		return;
	}

	XSync(dpy, False);
	while (XCheckTypedEvent(dpy, PropertyNotify, &xev)) {
		;
	}

	snprintf(diff, 64, "%.6f/%08d", servertime_diff, seq++); 
	XChangeProperty(dpy, rootwin, servertime, XA_STRING, 8,
	    PropModeReplace, (unsigned char *) diff, strlen(diff));
	XSync(dpy, False);

	for (i=0; i < 10; i++) {
		int k, got = 0;
		
		for (k=0; k < 5; k++) {
			while (XCheckTypedEvent(dpy, PropertyNotify, &xev)) {
				if (xev.xproperty.atom == servertime) {
					double stime;
					
					stime = (double) xev.xproperty.time;
					stime = stime/1000.0;
					servertime_diff = dnow() - stime;
					if (db) rfbLog("set servertime_diff: "
					    "%.6f\n", servertime_diff);
					got = 1;
				}
			}
		}
		if (got) {
			break;
		}
		usleep(1000);
	}
}

void check_autorepeat() {
	static time_t last_check = 0;
	time_t now = time(0);
	int autorepeat_is_on, autorepeat_initially_on, idle_timeout = 300;
	static int idle_reset = 0;

	if (! no_autorepeat || ! client_count) {
		return;
	}
	if (now <= last_check + 1) {
		return;
	}
	last_check = now;

	autorepeat_is_on = get_autorepeat_state();
	autorepeat_initially_on = get_initial_autorepeat_state();

	if (view_only) {
		if (! autorepeat_is_on) {
			autorepeat(1, 1);
		}
		return;
	}

	if (now > last_keyboard_input + idle_timeout) {
		/* autorepeat should be on when idle */
		if (! autorepeat_is_on && autorepeat_initially_on) {
			static time_t last_msg = 0;
			static int cnt = 0;
			if (now > last_msg + idle_timeout && cnt++ < 5) {
				rfbLog("idle keyboard:   turning X autorepeat"
				    " back on.\n");
				last_msg = now;
			}
			autorepeat(1, 1);
			idle_reset = 1;
		}
	} else {
		if (idle_reset) {
			static time_t last_msg = 0;
			static int cnt = 0;
			if (now > last_msg + idle_timeout && cnt++ < 5) {
				rfbLog("active keyboard: turning X autorepeat"
				    " off.\n");
				last_msg = now;
			}
			autorepeat(0, 1);
			idle_reset = 0;

		} else if (no_repeat_countdown && autorepeat_is_on) {
			int n = no_repeat_countdown - 1;
			if (n >= 0) {
				rfbLog("Battling with something for "
				    "-norepeat!! (%d resets left)\n", n);
			} else {
				rfbLog("Battling with something for "
				    "-norepeat!!\n");
			}
			if (no_repeat_countdown > 0) {
				no_repeat_countdown--;
			}
			autorepeat(1, 0);
			autorepeat(0, 0);
		}
	}
}

/*
 * This routine is periodically called to check for selection related
 * and other X11 events and respond to them as needed.
 */
void check_xevents(void) {
	XEvent xev;
	int have_clients = 0;
	static int sent_some_sel = 0;
	static time_t last_request = 0;
	static time_t last_call = 0;
	static time_t last_bell = 0;
	static time_t last_init_check = 0;
	static time_t last_sync = 0;
	static time_t last_time_sync = 0;
	time_t now = time(0);

	if (raw_fb && ! dpy) return;	/* raw_fb hack */

	if (now > last_init_check+1) {
		last_init_check = now;
		initialize_xevents();
	}

	if (screen && screen->clientHead) {
		have_clients = 1;
	}

	X_LOCK;
	/*
	 * There is a bug where we have to wait before sending text to
	 * the client... so instead of sending right away we wait a
	 * the few seconds.
	 */
	if (have_clients && watch_selection && !sent_some_sel
	    && now > last_client + sel_waittime) {
		if (XGetSelectionOwner(dpy, XA_PRIMARY) == None) {
			cutbuffer_send();
		}
		sent_some_sel = 1;
	}
	if (! have_clients) {
		/*
		 * If we don't have clients we can miss the X server
		 * going away until a client connects.
		 */
		static time_t last_X_ping = 0;
		if (now > last_X_ping + 5) {
			last_X_ping = now;
			XGetSelectionOwner(dpy, XA_PRIMARY);
		}
	}

	if (now > last_call+1) {
		/* we only check these once a second or so. */
		int n = 0;
		while (XCheckTypedEvent(dpy, MappingNotify, &xev)) {
			XRefreshKeyboardMapping((XMappingEvent *) &xev);
			n++;
		}
		if (n && use_modifier_tweak) {
			X_UNLOCK;
			initialize_modtweak();
			X_LOCK;
		}
		if (xtrap_base_event_type) {
			int base = xtrap_base_event_type;
			while (XCheckTypedEvent(dpy, base, &xev)) {
				;
			}
		}
		if (xtest_base_event_type) {
			int base = xtest_base_event_type;
			while (XCheckTypedEvent(dpy, base, &xev)) {
				;
			}
		}
		/*
		 * we can get ClientMessage from our XSendEvent() call in 
		 * selection_request().
		 */
		while (XCheckTypedEvent(dpy, ClientMessage, &xev)) {
			;
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

	if (now > last_time_sync + 30) {
		sync_tod_with_servertime();
		last_time_sync = now;
	}

#if LIBVNCSERVER_HAVE_LIBXRANDR
	if (xrandr) {
		check_xrandr_event("check_xevents");
	}
#endif
#if LIBVNCSERVER_HAVE_LIBXFIXES
	if (xfixes_present && use_xfixes && xfixes_base_event_type) {
		if (XCheckTypedEvent(dpy, xfixes_base_event_type +
		    XFixesCursorNotify, &xev)) {
			got_xfixes_cursor_notify++;
		}
	}
#endif

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
				if (xcut_str) {
					free(xcut_str);
					xcut_str = NULL;
				}
			}
		}
	}

	if (watch_bell || now > last_bell+1) {
		last_bell = now;
		check_bell_event();
	}

#ifndef DEBUG_XEVENTS
#define DEBUG_XEVENTS 1
#endif
#if DEBUG_XEVENTS
	if (debug_xevents) {
		static time_t last_check = 0;
		static time_t reminder = 0;
		static int freq = 0;

		if (! freq) {
			if (getenv("X11VNC_REMINDER_RATE")) {
				freq = atoi(getenv("X11VNC_REMINDER_RATE"));
			} else {
				freq = 300;
			}
		}

		if (now > last_check + 1) {
			int ev_type_max = 300, ev_size = 400;
			XEvent xevs[400];
			int i, tot = XEventsQueued(dpy, QueuedAlready);

			if (reminder == 0 || (tot && now > reminder + freq)) {
				print_xevent_bases();
				reminder = now;
			}
			last_check = now;

			if (tot) {
		    		fprintf(stderr, "Total events queued: %d\n",
				    tot);
			}
			for (i=1; i<ev_type_max; i++) {
				int k, n = 0;
				while (XCheckTypedEvent(dpy, i, xevs+n)) {
					if (++n >= ev_size) {
						break;
					}
				}
				if (n) {
					fprintf(stderr, "  %d%s events of type"
					    " %d queued\n", n,
					    (n >= ev_size) ? "+" : "", i);
				}
				for (k=n-1; k >= 0; k--) {
					XPutBackEvent(dpy, xevs+k);
				}
			}
		}
	}
#endif

	if (now > last_sync + 1200) {
		/* kludge for any remaining event leaks */
		int bugout = use_xdamage ? 500 : 50;
		int qlen, i;
		if (last_sync != 0) {
			qlen = XEventsQueued(dpy, QueuedAlready);
			if (qlen >= bugout) {
				rfbLog("event leak: %d queued, "
				    " calling XSync(dpy, True)\n", qlen);  
				rfbLog("  for diagnostics run: 'x11vnc -R"
				    " debug_xevents:1'\n");
				XSync(dpy, True);
			}
		}
		last_sync = now;

		/* clear these, we don't want any events on them */
		if (rdpy_ctrl) {
			qlen = XEventsQueued(rdpy_ctrl, QueuedAlready);
			for (i=0; i<qlen; i++) {
				XNextEvent(rdpy_ctrl, &xev);
			}
		}
		if (gdpy_ctrl) {
			qlen = XEventsQueued(gdpy_ctrl, QueuedAlready);
			for (i=0; i<qlen; i++) {
				XNextEvent(gdpy_ctrl, &xev);
			}
		}
	}
	X_UNLOCK;

	last_call = now;
}

/*
 * hook called when a VNC client sends us some "XCut" text (rfbClientCutText).
 */
void xcut_receive(char *text, int len, rfbClientPtr cl) {
	allowed_input_t input;

	if (raw_fb && ! dpy) return;	/* raw_fb hack */

	if (!watch_selection) {
		return;
	}
	if (view_only) {
		return;
	}
	if (text == NULL || len == 0) {
		return;
	}
	get_allowed_input(cl, &input);
	if (!input.keystroke && !input.motion && !input.button) {
		/* maybe someday KMBC for cut text... */
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
	if (xcut_str != NULL) {
		free(xcut_str);
	}
	xcut_str = (unsigned char *)
	    malloc((size_t) (len+1) * sizeof(unsigned char));
	strncpy(xcut_str, text, len);
	xcut_str[len] = '\0';	/* make sure null terminated */

	/* copy this text to CUT_BUFFER0 as well: */
	XChangeProperty(dpy, rootwin, XA_CUT_BUFFER0, XA_STRING, 8,
	    PropModeReplace, (unsigned char *) text, len);
	XFlush(dpy);

	X_UNLOCK;

	set_cutbuffer = 1;
}

/* -- remote.c -- */

/*
 * for the wild-n-crazy -remote/-R interface.
 */
int send_remote_cmd(char *cmd, int query, int wait) {
	FILE *in = NULL;

	if (client_connect_file) {
		in = fopen(client_connect_file, "w");
		if (in == NULL) {
			fprintf(stderr, "send_remote_cmd: could not open "
			    "connect file \"%s\" for writing\n",
			    client_connect_file);
			perror("fopen");
			return 1;
		}
	} else if (vnc_connect_prop == None) {
		initialize_vnc_connect_prop();
		if (vnc_connect_prop == None) {
			fprintf(stderr, "send_remote_cmd: could not obtain "
			    "VNC_CONNECT X property\n");
			return 1;
		}
	}

	if (in != NULL) {
		fprintf(stderr, "sending remote command: \"%s\"\nvia connect"
		    " file: %s\n", cmd, client_connect_file);
		fprintf(in, "%s\n", cmd);
		fclose(in);
	} else {
		fprintf(stderr, "sending remote command: \"%s\" via VNC_CONNECT"
		    " X property.\n", cmd);
		set_vnc_connect_prop(cmd);
		XFlush(dpy);
	}

	if (query || wait) {
		char line[VNC_CONNECT_MAX];	
		int rc=1, i=0, max=70, ms_sl=50;

		if (!strcmp(cmd, "cmd=stop")) {
			max = 20;
		}
		for (i=0; i<max; i++) {
			usleep(ms_sl * 1000);
			if (client_connect_file) {
				char *q;
				in = fopen(client_connect_file, "r");
				if (in == NULL) {
					fprintf(stderr, "send_remote_cmd: could"
					    " not open connect file \"%s\" for"
					    " writing\n", client_connect_file);
					perror("fopen");
					return 1;
				}
				fgets(line, VNC_CONNECT_MAX, in);
				fclose(in);
				q = line;
				while (*q != '\0') {
					if (*q == '\n') *q = '\0';
					q++;
				}
			} else {
				read_vnc_connect_prop();
				strncpy(line, vnc_connect_str, VNC_CONNECT_MAX);
			}
			if (strcmp(cmd, line)){
				if (query) {
					fprintf(stdout, "%s\n", line);
					fflush(stdout);
				}
				rc = 0;
				break;
			}
		}
		if (rc) {
			fprintf(stderr, "error: could not connect to "
			    "an x11vnc server at %s  (rc=%d)\n",
			    client_connect_file ? client_connect_file
			    : DisplayString(dpy), rc);
		}
		return rc;
	}
	return 0;
}

int do_remote_query(char *remote_cmd, char *query_cmd, int remote_sync) {
	char *rcmd = NULL, *qcmd = NULL;
	int rc = 1;

	if (remote_cmd) {
		rcmd = (char *)malloc(strlen(remote_cmd) + 5);
		strcpy(rcmd, "cmd=");
		strcat(rcmd, remote_cmd);
	}
	if (query_cmd) {
		qcmd = (char *)malloc(strlen(query_cmd) + 5);
		strcpy(qcmd, "qry=");
		strcat(qcmd, query_cmd);
	}
	
	if (rcmd && qcmd) {
		rc = send_remote_cmd(rcmd, 0, 1);
		if (rc) {
			free(rcmd);
			free(qcmd);
			return(rc);
		}
		rc = send_remote_cmd(qcmd, 1, 1);
	} else if (rcmd) {
		rc = send_remote_cmd(rcmd, 0, remote_sync);
		free(rcmd);
	} else if (qcmd) {
		rc = send_remote_cmd(qcmd, 1, 1);
		free(qcmd);
	}
	return rc;
}

char *add_item(char *instr, char *item) {
	char *p, *str;
	int len, saw_item = 0;

	if (! instr || *instr == '\0') {
		str = strdup(item);
		return str;
	}
	len = strlen(instr) + 1 + strlen(item) + 1;
	str = (char *)malloc(len);
	str[0] = '\0';

	/* n.b. instr will be modified; caller replaces with returned string */
	p = strtok(instr, ",");
	while (p) {
		if (!strcmp(p, item)) {
			if (saw_item) {
				p = strtok(NULL, ",");
				continue;
			}
			saw_item = 1;
		} else if (*p == '\0') {
			p = strtok(NULL, ",");
			continue;
		}
		if (str[0]) {
			strcat(str, ",");
		}
		strcat(str, p);
		p = strtok(NULL, ",");
	}
	if (! saw_item) {
		if (str[0]) {
			strcat(str, ",");
		}
		strcat(str, item);
	}
	return str;
}

char *delete_item(char *instr, char *item) {
	char *p, *str;
	int len;

	if (! instr || *instr == '\0') {
		str = strdup("");
		return str;
	}
	len = strlen(instr) + 1;
	str = (char *)malloc(len);
	str[0] = '\0';

	/* n.b. instr will be modified; caller replaces with returned string */
	p = strtok(instr, ",");
	while (p) {
		if (!strcmp(p, item) || *p == '\0') {
			p = strtok(NULL, ",");
			continue;
		}
		if (str[0]) {
			strcat(str, ",");
		}
		strcat(str, p);
		p = strtok(NULL, ",");
	}
	return str;
}

void if_8bpp_do_new_fb(void) {
	if (bpp == 8) {
		do_new_fb(0);
	} else {
		rfbLog("  bpp(%d) is not 8bpp, not resetting fb\n", bpp);
	}
}

void check_black_fb(void) {
	if (!screen) {
		return;
	}
	if (new_fb_size_clients(screen) != client_count) {
		rfbLog("trying to send a black fb for non-newfbsize"
		    " clients %d != %d\n", client_count,
		    new_fb_size_clients(screen));
		push_black_screen(4);
	}
}

int check_httpdir(void) {
	if (http_dir) {
		return 1;
	} else {
		char *prog = NULL, *httpdir, *q;
		struct stat sbuf;
		int len;

		rfbLog("check_httpdir: trying to guess httpdir...\n");
		if (program_name[0] == '/') {
			prog = strdup(program_name);
		} else {
			char cwd[1024];
			getcwd(cwd, 1024);
			len = strlen(cwd) + 1 + strlen(program_name) + 1;
			prog = (char *) malloc(len);
			snprintf(prog, len, "%s/%s", cwd, program_name);
			if (stat(prog, &sbuf) != 0) {
				char *path = strdup(getenv("PATH"));
				char *p, *base;
				base = strrchr(program_name, '/');
				if (base) {
					base++;
				} else {
					base = program_name;
				}
				
				p = strtok(path, ":");
				while(p) {
					free(prog);
					len = strlen(p) + 1 + strlen(base) + 1;
					prog = (char *) malloc(len);
					snprintf(prog, len, "%s/%s", p, base);
					if (stat(prog, &sbuf) == 0) {
						break;
					}
					p = strtok(NULL, ":");
				}
				free(path);
			}
		}
		/*
		 * /path/to/bin/x11vnc
		 * /path/to/bin/../share/x11vnc/classes
		 *                    12345678901234567
		 */
		if ((q = strrchr(prog, '/')) == NULL) {
			rfbLog("check_httpdir: bad program path: %s\n", prog);
			free(prog);
			return 0;
		}

		len = strlen(prog) + 17 + 1;
		*q = '\0';
		httpdir = (char *) malloc(len);
		snprintf(httpdir, len, "%s/../share/x11vnc/classes", prog);
		free(prog);

		if (stat(httpdir, &sbuf) == 0) {
			/* good enough for me */
			rfbLog("check_httpdir: guessed directory:\n");
			rfbLog("   %s\n", httpdir);
			http_dir = httpdir;
			return 1;
		} else {
			/* try some hardwires: */
			if (stat("/usr/local/share/x11vnc/classes",
			    &sbuf) == 0) {
				http_dir =
				    strdup("/usr/local/share/x11vnc/classes");	
				return 1;
			}
			if (stat("/usr/share/x11vnc/classes", &sbuf) == 0) {
				http_dir = strdup("/usr/share/x11vnc/classes");	
				return 1;
			}
			rfbLog("check_httpdir: bad guess:\n");
			rfbLog("   %s\n", httpdir);
			return 0;
		}
	}
}

void http_connections(int on) {
	if (!screen) {
		return;
	}
	if (on) {
		rfbLog("http_connections: turning on http service.\n");
		screen->httpInitDone = FALSE;
		screen->httpDir = http_dir;
		if (check_httpdir()) {
			rfbHttpInitSockets(screen);
		}
	} else {
		rfbLog("http_connections: turning off http service.\n");
		if (screen->httpListenSock > -1) {
			close(screen->httpListenSock);
		}
		screen->httpListenSock = -1;
		screen->httpDir = NULL;
	}
}

void reset_httpport(int old, int new) {
	int hp = new;
	if (hp < 0) {
		rfbLog("reset_httpport: bad httpport: %d\n", hp);
	} else if (hp == old) {
		rfbLog("reset_httpport: unchanged httpport: %d\n", hp);
	} else if (inetd) {
		rfbLog("reset_httpport: cannot set httpport: %d"
		    " in inetd.\n", hp);
	} else if (screen) {
		screen->httpPort = hp;
		screen->httpInitDone = FALSE;
		if (screen->httpListenSock > -1) {
			close(screen->httpListenSock);
		}
		rfbLog("reset_httpport: setting httpport %d -> %d.\n",
		    old == -1 ? hp : old, hp);
		rfbHttpInitSockets(screen);
	}
}

void reset_rfbport(int old, int new)  {
	int rp = new;
	if (rp < 0) {
		rfbLog("reset_rfbport: bad rfbport: %d\n", rp);
	} else if (rp == old) {
		rfbLog("reset_rfbport: unchanged rfbport: %d\n", rp);
	} else if (inetd) {
		rfbLog("reset_rfbport: cannot set rfbport: %d"
		    " in inetd.\n", rp);
	} else if (screen) {
		rfbClientIteratorPtr iter;
		rfbClientPtr cl;
		int maxfd;
		if (rp == 0) {
			screen->autoPort = TRUE;
		} else {
			screen->autoPort = FALSE;
		}
		screen->port = rp;
		screen->socketState = RFB_SOCKET_INIT;

		if (screen->listenSock > -1) {
			close(screen->listenSock);
		}

		rfbLog("reset_rfbport: setting rfbport %d -> %d.\n",
		    old == -1 ? rp : old, rp);
		rfbInitSockets(screen);

		maxfd = screen->maxFd;
		if (screen->udpSock > 0 && screen->udpSock > maxfd) {
			maxfd = screen->udpSock;
		}
		iter = rfbGetClientIterator(screen);
		while( (cl = rfbClientIteratorNext(iter)) ) {
			if (cl->sock > -1) {
				FD_SET(cl->sock, &(screen->allFds));
				if (cl->sock > maxfd) {
					maxfd = cl->sock;
				}
			}
		}
		rfbReleaseClientIterator(iter);

		screen->maxFd = maxfd;

		set_vnc_desktop_name();
	}
}

/*
 * Do some sanity checking of the permissions on the XAUTHORITY and the
 * -connect file.  This is -privremote.  What should be done is check
 * for an empty host access list, currently we lazily do not bring in
 * libXau yet.
 */
int remote_control_access_ok(void) {
	struct stat sbuf;

	if (client_connect_file) {
		if (stat(client_connect_file, &sbuf) == 0) {
			if (sbuf.st_mode & S_IWOTH) {
				rfbLog("connect file is writable by others.\n");
				rfbLog("   %s\n", client_connect_file);
				return 0;
			}
			if (sbuf.st_mode & S_IWGRP) {
				rfbLog("connect file is writable by group.\n");
				rfbLog("   %s\n", client_connect_file);
				return 0;
			}
		}
	}

	if (dpy) {
		char tmp[1000];
		char *home, *xauth;
		char *dpy_str = DisplayString(dpy);
		Display *dpy2;
		XHostAddress *xha;
		Bool enabled;
		int n;

		home = get_home_dir();
		if (getenv("XAUTHORITY") != NULL) {
			xauth = getenv("XAUTHORITY");
		} else if (home) {
			int len = 1000 - strlen("/.Xauthority") - 1;
			strncpy(tmp, home, len); 
			strcat(tmp, "/.Xauthority");
			xauth = tmp;
		} else {
			rfbLog("cannot determine default XAUTHORITY.\n");
			return 0;
		}
		if (home) {
			free(home);
		}
		if (stat(xauth, &sbuf) == 0) {
			if (sbuf.st_mode & S_IWOTH) {
				rfbLog("XAUTHORITY is writable by others!!\n");
				rfbLog("   %s\n", xauth);
				return 0;
			}
			if (sbuf.st_mode & S_IWGRP) {
				rfbLog("XAUTHORITY is writable by group!!\n");
				rfbLog("   %s\n", xauth);
				return 0;
			}
			if (sbuf.st_mode & S_IROTH) {
				rfbLog("XAUTHORITY is readable by others.\n");
				rfbLog("   %s\n", xauth);
				return 0;
			}
			if (sbuf.st_mode & S_IRGRP) {
				rfbLog("XAUTHORITY is readable by group.\n");
				rfbLog("   %s\n", xauth);
				return 0;
			}
		}

		xha = XListHosts(dpy, &n, &enabled);
		if (! enabled) {
			rfbLog("X access control is disabled, X clients can\n");
			rfbLog("   connect from any host.  Run 'xhost -'\n");
			return 0;
		}
		if (xha) {
			int i;
			rfbLog("The following hosts can connect w/o X11 "
			    "auth:\n");
			for (i=0; i<n; i++) {
				if (xha[i].family == FamilyInternet) {
					char *str = raw2host(xha[i].address,
					    xha[i].length);
					char *ip = raw2ip(xha[i].address);
					rfbLog("  %s/%s\n", str, ip);
					free(str);
					free(ip);
				} else {
					rfbLog("  unknown-%d\n", i+1);
				}
			}
			XFree(xha);
			return 0;
		}

		if (getenv("XAUTHORITY")) {
			xauth = strdup(getenv("XAUTHORITY"));
		} else {
			xauth = NULL;
		}
		set_env("XAUTHORITY", "/impossible/xauthfile");

		fprintf(stderr, "\nChecking if display %s requires "
		    "XAUTHORITY\n", dpy_str);
		fprintf(stderr, "   -- (ignore any Xlib: errors that"
		    " follow) --\n");
		dpy2 = XOpenDisplay(dpy_str); 
		fflush(stderr);
		fprintf(stderr, "   -- (done checking) --\n\n");

		if (xauth) {
			set_env("XAUTHORITY", xauth);
			free(xauth);
		} else {
			xauth = getenv("XAUTHORITY");
			if (xauth) {
				*(xauth-2) = '_';	/* yow */
			}
		}
		if (dpy2) {
			rfbLog("XAUTHORITY is not required on display.\n");
			rfbLog("   %s\n", DisplayString(dpy));
			XCloseDisplay(dpy2);
			return 0;
		}

	}
	return 1;
}

/*
 * Huge, ugly switch to handle all remote commands and queries
 * -remote/-R and -query/-Q.
 */
char *process_remote_cmd(char *cmd, int stringonly) {
#if REMOTE_CONTROL
	char *p = cmd;
	char *co = "";
	char buf[VNC_CONNECT_MAX]; 
	int bufn = VNC_CONNECT_MAX;
	int query = 0;
	static char *prev_cursors_mode = NULL;

	if (! accept_remote_cmds) {
		rfbLog("remote commands disabled: %s\n", cmd);
		return NULL;
	}

	if (priv_remote) {
		if (! remote_control_access_ok()) {
			rfbLog("** Disabling remote commands in -privremote "
			    "mode.\n");
			accept_remote_cmds = 0;
			return NULL;
		}
	}

	strcpy(buf, "");
	if (strstr(cmd, "cmd=") == cmd) {
		p += strlen("cmd=");
	} else if (strstr(cmd, "qry=") == cmd) {
		query = 1;
		if (strchr(cmd, ',')) {
			/* comma separated batch mode */
			char *s, *q, *res;
			char tmp[512];
			strcpy(buf, "");
			s = strdup(cmd + strlen("qry="));
			q = strtok(s, ","); 
			while (q) {
				strcpy(tmp, "qry=");
				strncat(tmp, q, 500);
				res = process_remote_cmd(tmp, 1);
				if (res && strlen(buf)+strlen(res)
				    >= VNC_CONNECT_MAX - 1) {
					rfbLog("overflow in process_remote_cmd:"
					    " %s -- %s\n", buf, res);
					free(res);
					break;
				}
				if (res) {
					strcat(buf, res);
					free(res);
				}
				q = strtok(NULL, ",");
				if (q) {
					strcat(buf, ",");
				}
			}
			free(s);
			goto qry;
		}
		p += strlen("qry=");
	} else {
		rfbLog("ignoring malformed command: %s\n", cmd);
		goto done;
	}

	/* always call like: COLON_CHECK("foobar:") */
#define COLON_CHECK(str) \
	if (strstr(p, str) != p) { \
		co = ":"; \
		if (! query) { \
			goto done; \
		} \
	} else { \
		char *q = strchr(p, ':'); \
		if (query && q != NULL) { \
			*(q+1) = '\0'; \
		} \
	}

#define NOTAPP \
	if (query) { \
		if (strchr(p, ':')) { \
			snprintf(buf, bufn, "ans=%sN/A", p); \
		} else { \
			snprintf(buf, bufn, "ans=%s:N/A", p); \
		} \
		goto qry; \
	}

#define NOTAPPRO \
	if (query) { \
		if (strchr(p, ':')) { \
			snprintf(buf, bufn, "aro=%sN/A", p); \
		} else { \
			snprintf(buf, bufn, "aro=%s:N/A", p); \
		} \
		goto qry; \
	}

/*
 * Maybe add: passwdfile logfile bg rfbauth passwd...
 */
	if (!strcmp(p, "stop") || !strcmp(p, "quit") ||
	    !strcmp(p, "exit") || !strcmp(p, "shutdown")) {
		NOTAPP
		close_all_clients();
		rfbLog("remote_cmd: setting shut_down flag\n");
		shut_down = 1;

	} else if (!strcmp(p, "ping")) {
		query = 1;
		if (rfb_desktop_name) {
			snprintf(buf, bufn, "ans=%s:%s", p, rfb_desktop_name);
		} else {
			snprintf(buf, bufn, "ans=%s:%s", p, "unknown");
		}
		goto qry;

	} else if (!strcmp(p, "blacken") || !strcmp(p, "zero")) {
		NOTAPP
		push_black_screen(4);
	} else if (!strcmp(p, "refresh")) {
		NOTAPP
		refresh_screen(1);
	} else if (!strcmp(p, "reset")) {
		NOTAPP
		do_new_fb(1);
	} else if (strstr(p, "zero:") == p) { /* skip-cmd-list */
		int x1, y1, x2, y2;
		NOTAPP
		p += strlen("zero:");
		if (sscanf(p, "%d,%d,%d,%d", &x1, &y1, &x2, &y2) == 4)  {
			int mark = 1;
			rfbLog("zeroing rect: %s\n", p);
			if (x1 < 0 || x2 < 0) {
				x1 = nabs(x1);
				x2 = nabs(x2);
				mark = 0;	/* hack for testing */
			}

			zero_fb(x1, y1, x2, y2);
			if (mark) {
				mark_rect_as_modified(x1, y1, x2, y2, 1);
			}
			push_sleep(4);
		}
	} else if (strstr(p, "damagefb:") == p) { /* skip-cmd-list */
		int delay;
		NOTAPP
		p += strlen("damagefb:");
		if (sscanf(p, "%d", &delay) == 1)  {
			rfbLog("damaging client fb's for %d secs "
			    "(by not marking rects.)\n", delay);
			damage_time = time(0);
			damage_delay = delay;
		}

	} else if (strstr(p, "close") == p) {
		NOTAPP
		COLON_CHECK("close:")
		p += strlen("close:");
		close_clients(p);
	} else if (strstr(p, "disconnect") == p) {
		NOTAPP
		COLON_CHECK("disconnect:")
		p += strlen("disconnect:");
		close_clients(p);

	} else if (strstr(p, "id") == p) {
		int ok = 0;
		Window twin;
		COLON_CHECK("id:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s0x%lx", p, co,
			    rootshift ? 0 : subwin);
			goto qry;
		}
		p += strlen("id:");
		if (*p == '\0' || !strcmp("root", p)) {
			/* back to root win */
			twin = 0x0;
			ok = 1;
		} else if (!strcmp("pick", p)) {
			twin = 0x0;
			if (safe_remote_only) {
				rfbLog("unsafe: '-id pick'\n");
			} else if (pick_windowid(&twin)) {
				ok = 1;
			}
		} else if (! scan_hexdec(p, &twin)) {
			rfbLog("-id: skipping incorrect hex/dec number:"
			    " %s\n", p);
		} else {
			ok = 1;
		}
		if (ok) {
			if (twin && ! valid_window(twin, NULL, 0)) {
				rfbLog("skipping invalid sub-window: 0x%lx\n",
				    twin);
			} else {
				subwin = twin;
				rootshift = 0;
				check_black_fb();
				do_new_fb(1);
			}
		}
	} else if (strstr(p, "sid") == p) {
		int ok = 0;
		Window twin;
		COLON_CHECK("sid:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s0x%lx", p, co,
			    !rootshift ? 0 : subwin);
			goto qry;
		}
		p += strlen("sid:");
		if (*p == '\0' || !strcmp("root", p)) {
			/* back to root win */
			twin = 0x0;
			ok = 1;
		} else if (!strcmp("pick", p)) {
			twin = 0x0;
			if (safe_remote_only) {
				rfbLog("unsafe: '-sid pick'\n");
			} else if (pick_windowid(&twin)) {
				ok = 1;
			}
		} else if (! scan_hexdec(p, &twin)) {
			rfbLog("-sid: skipping incorrect hex/dec number: %s\n", p);
		} else {
			ok = 1;
		}
		if (ok) {
			if (twin && ! valid_window(twin, NULL, 0)) {
				rfbLog("skipping invalid sub-window: 0x%lx\n",
				    twin);
			} else {
				subwin = twin;
				rootshift = 1;
				check_black_fb();
				do_new_fb(1);
			}
		}
	} else if (strstr(p, "waitmapped") == p) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    subwin_wait_mapped);
			goto qry;
		}
		subwin_wait_mapped = 1;
	} else if (strstr(p, "nowaitmapped") == p) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    !subwin_wait_mapped);
			goto qry;
		}
		subwin_wait_mapped = 0;

	} else if (!strcmp(p, "clip") ||
	    strstr(p, "clip:") == p) {	/* skip-cmd-list */
		COLON_CHECK("clip:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(clip_str));
			goto qry;
		}
		p += strlen("clip:");
		if (clip_str) free(clip_str);
		clip_str = strdup(p);

		/* OK, this requires a new fb... */
		do_new_fb(1);

	} else if (!strcmp(p, "flashcmap")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, flash_cmap);
			goto qry;
		}
		rfbLog("remote_cmd: turning on flashcmap mode.\n");
		flash_cmap = 1;
	} else if (!strcmp(p, "noflashcmap")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !flash_cmap);
			goto qry;
		}
		rfbLog("remote_cmd: turning off flashcmap mode.\n");
		flash_cmap = 0;
		
	} else if (strstr(p, "shiftcmap") == p) {
		COLON_CHECK("shiftcmap:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co, shift_cmap);
			goto qry;
		}
		p += strlen("shiftcmap:");
		shift_cmap = atoi(p);
		rfbLog("remote_cmd: set -shiftcmap %d\n", shift_cmap);
		do_new_fb(1);

	} else if (!strcmp(p, "truecolor")) {
		int orig = force_indexed_color;
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    !force_indexed_color);
			goto qry;
		}
		rfbLog("remote_cmd: turning off notruecolor mode.\n");
		force_indexed_color = 0;
		if (orig != force_indexed_color) {
			if_8bpp_do_new_fb();
		}
	} else if (!strcmp(p, "notruecolor")) {
		int orig = force_indexed_color;
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    force_indexed_color);
			goto qry;
		}
		rfbLog("remote_cmd: turning on notruecolor mode.\n");
		force_indexed_color = 1;
		if (orig != force_indexed_color) {
			if_8bpp_do_new_fb();
		}
		
	} else if (!strcmp(p, "overlay")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, overlay);
			goto qry;
		}
		rfbLog("remote_cmd: turning on -overlay mode.\n");
		if (!overlay_present) {
			rfbLog("skipping: overlay extension not present.\n");
		} else if (overlay) {
			rfbLog("skipping: already in -overlay mode.\n");
		} else {
			int reset_mem = 0;
			/* here we go... */
			if (using_shm) {
				rfbLog("setting -noshm mode.\n");
				using_shm = 0;
				reset_mem = 1;
			}
			overlay = 1;
			do_new_fb(reset_mem);
		}
	} else if (!strcmp(p, "nooverlay")) {
		int orig = overlay;
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !overlay);
			goto qry;
		}
		rfbLog("remote_cmd: turning off overlay mode\n");
		overlay = 0;
		if (!overlay_present) {
			rfbLog("warning: overlay extension not present.\n");
		} else if (!orig) {
			rfbLog("skipping: already not in -overlay mode.\n");
		} else {
			/* here we go... */
			do_new_fb(0);
		}
		
	} else if (!strcmp(p, "overlay_cursor") ||
	    !strcmp(p, "overlay_yescursor") ||
	    !strcmp(p, "nooverlay_nocursor")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, overlay_cursor);
			goto qry;
		}
		rfbLog("remote_cmd: turning on overlay_cursor mode.\n");
		overlay_cursor = 1;
		if (!overlay_present) {
			rfbLog("warning: overlay extension not present.\n");
		} else if (!overlay) {
			rfbLog("warning: not in -overlay mode.\n");
		} else {
			rfbLog("You may want to run -R noshow_cursor or\n");
			rfbLog(" -R cursor:none to disable any extra "
			    "cursors.\n");
		}
	} else if (!strcmp(p, "nooverlay_cursor") ||
	    !strcmp(p, "nooverlay_yescursor") ||
	    !strcmp(p, "overlay_nocursor")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !overlay_cursor);
			goto qry;
		}
		rfbLog("remote_cmd: turning off overlay_cursor mode\n");
		overlay_cursor = 0;
		if (!overlay_present) {
			rfbLog("warning: overlay extension not present.\n");
		} else if (!overlay) {
			rfbLog("warning: not in -overlay mode.\n");
		} else {
			rfbLog("You may want to run -R show_cursor or\n");
			rfbLog(" -R cursor:... to re-enable any cursors.\n");
		}
		
	} else if (strstr(p, "visual") == p) {
		COLON_CHECK("visual:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(visual_str));
			goto qry;
		}
		p += strlen("visual:");
		if (visual_str) free(visual_str);
		visual_str = strdup(p);

		/* OK, this requires a new fb... */
		do_new_fb(0);

	} else if (!strcmp(p, "scale") ||
		    strstr(p, "scale:") == p) {	/* skip-cmd-list */
		COLON_CHECK("scale:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(scale_str));
			goto qry;
		}
		p += strlen("scale:");
		if (scale_str) free(scale_str);
		scale_str = strdup(p);

		/* OK, this requires a new fb... */
		check_black_fb();
		do_new_fb(0);

	} else if (!strcmp(p, "scale_cursor") ||
		    strstr(p, "scale_cursor:") == p) {	/* skip-cmd-list */
		COLON_CHECK("scale_cursor:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(scale_cursor_str));
			goto qry;
		}
		p += strlen("scale_cursor:");
		if (scale_cursor_str) free(scale_cursor_str);
		if (*p == '\0') {
			scale_cursor_str = NULL;
		} else {
			scale_cursor_str = strdup(p);
		}
		setup_cursors_and_push();

	} else if (!strcmp(p, "viewonly")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, view_only);
			goto qry;
		}
		rfbLog("remote_cmd: enable viewonly mode.\n");
		view_only = 1;
	} else if (!strcmp(p, "noviewonly")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !view_only);
			goto qry;
		}
		rfbLog("remote_cmd: disable viewonly mode.\n");
		view_only = 0;
		if (raw_fb) set_raw_fb_params(0);

	} else if (!strcmp(p, "shared")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, shared); goto qry;
		}
		rfbLog("remote_cmd: enable sharing.\n");
		shared = 1;
		if (screen) {
			screen->alwaysShared = TRUE;
			screen->neverShared = FALSE;
		}
	} else if (!strcmp(p, "noshared")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !shared); goto qry;
		}
		rfbLog("remote_cmd: disable sharing.\n");
		shared = 0;
		if (screen) {
			screen->alwaysShared = FALSE;
			screen->neverShared = TRUE;
		}

	} else if (!strcmp(p, "forever")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, 1-connect_once);
			goto qry;
		}
		rfbLog("remote_cmd: enable -forever mode.\n");
		connect_once = 0;
	} else if (!strcmp(p, "noforever") || !strcmp(p, "once")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, connect_once);
			goto qry;
		}
		rfbLog("remote_cmd: disable -forever mode.\n");
		connect_once = 1;

	} else if (strstr(p, "timeout") == p) {
		int to;
		COLON_CHECK("timeout:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co,
			    first_conn_timeout);
			goto qry;
		}
		p += strlen("timeout:");
		to = atoi(p);
		if (to > 0 ) {
			to = -to;
		}
		first_conn_timeout = to;
		rfbLog("remote_cmd: set -timeout to %d\n", -to);

	} else if (!strcmp(p, "deny") || !strcmp(p, "lock")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, deny_all);
			goto qry;
		}
		rfbLog("remote_cmd: denying new connections.\n");
		deny_all = 1;
	} else if (!strcmp(p, "nodeny") || !strcmp(p, "unlock")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !deny_all);
			goto qry;
		}
		rfbLog("remote_cmd: allowing new connections.\n");
		deny_all = 0;

	} else if (strstr(p, "connect") == p) {
		NOTAPP
		COLON_CHECK("connect:")
		p += strlen("connect:");
		/* this is a reverse connection */
		reverse_connect(p);

	} else if (strstr(p, "allowonce") == p) {
		COLON_CHECK("allowonce:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(allow_once));
			goto qry;
		}
		p += strlen("allowonce:");
		allow_once = strdup(p);
		rfbLog("remote_cmd: set allow_once %s\n", allow_once);

	} else if (strstr(p, "allow") == p) {
		char *before, *old;
		COLON_CHECK("allow:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(allow_list));
			goto qry;
		}
		p += strlen("allow:");
		if (allow_list && strchr(allow_list, '/')) {
			rfbLog("remote_cmd: cannot use allow:host\n");
			rfbLog("in '-allow %s' mode.\n", allow_list);
			goto done;
		}
		if (allow_list) {
			before = strdup(allow_list);
		} else {
			before = strdup("");
		}

		old = allow_list;
		if (*p == '+') {
			p++;
			allow_list = add_item(allow_list, p);
		} else if (*p == '-') {
			p++;
			allow_list = delete_item(allow_list, p);
		} else {
			allow_list = strdup(p);
		}

		if (strcmp(before, allow_list)) {
			rfbLog("remote_cmd: modified allow_list:\n");
			rfbLog(" from: \"%s\"\n", before);
			rfbLog(" to:   \"%s\"\n", allow_list);
		}
		if (old) free(old);
		free(before);

	} else if (!strcmp(p, "localhost")) {
		char *before, *old;
		if (query) {
			int state = 0;
			char *s = allow_list;
			if (s && (!strcmp(s, "127.0.0.1") ||
			    !strcmp(s, "localhost"))) {
				state = 1;
			}
			snprintf(buf, bufn, "ans=%s:%d", p, state);
			goto qry;
		}
		if (allow_list) {
			before = strdup(allow_list);
		} else {
			before = strdup("");
		}
		old = allow_list;

		allow_list = strdup("127.0.0.1");

		if (strcmp(before, allow_list)) {
			rfbLog("remote_cmd: modified allow_list:\n");
			rfbLog(" from: \"%s\"\n", before);
			rfbLog(" to:   \"%s\"\n", allow_list);
		}
		if (old) free(old);
		free(before);

		if (listen_str) {
			free(listen_str);
		}
		listen_str = strdup("localhost");

		screen->listenInterface = htonl(INADDR_LOOPBACK);
		rfbLog("listening on loopback network only.\n");
		rfbLog("allow list is: '%s'\n", NONUL(allow_list));
		reset_rfbport(-1, screen->port);
		if (screen->httpListenSock > -1) {
			reset_httpport(-1, screen->httpPort);
		}
	} else if (!strcmp(p, "nolocalhost")) {
		char *before, *old;
		if (query) {
			int state = 0;
			char *s = allow_list;
			if (s && (!strcmp(s, "127.0.0.1") ||
			    !strcmp(s, "localhost"))) {
				state = 1;
			}
			snprintf(buf, bufn, "ans=%s:%d", p, !state);
			goto qry;
		}
		if (allow_list) {
			before = strdup(allow_list);
		} else {
			before = strdup("");
		}
		old = allow_list;

		allow_list = strdup("");

		if (strcmp(before, allow_list)) {
			rfbLog("remote_cmd: modified allow_list:\n");
			rfbLog(" from: \"%s\"\n", before);
			rfbLog(" to:   \"%s\"\n", allow_list);
		}
		if (old) free(old);
		free(before);

		if (listen_str) {
			free(listen_str);
		}
		listen_str = NULL;

		screen->listenInterface = htonl(INADDR_ANY);
		rfbLog("listening on ALL network interfaces.\n");
		rfbLog("allow list is: '%s'\n", NONUL(allow_list));
		reset_rfbport(-1, screen->port);
		if (screen->httpListenSock > -1) {
			reset_httpport(-1, screen->httpPort);
		}

	} else if (strstr(p, "listen") == p) {
		char *before;
		int ok, mod = 0;

		COLON_CHECK("listen:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(listen_str));
			goto qry;
		}
		if (listen_str) {
			before = strdup(listen_str);
		} else {
			before = strdup("");
		}
		p += strlen("listen:");

		listen_str = strdup(p);

		if (strcmp(before, listen_str)) {
			rfbLog("remote_cmd: modified listen_str:\n");
			rfbLog(" from: \"%s\"\n", before);
			rfbLog(" to:   \"%s\"\n", listen_str);
			mod = 1;
		}

		ok = 1;
		if (listen_str == NULL || *listen_str == '\0' ||
		    !strcmp(listen_str, "any")) {
			screen->listenInterface = htonl(INADDR_ANY);
		} else if (!strcmp(listen_str, "localhost")) {
			screen->listenInterface = htonl(INADDR_LOOPBACK);
		} else {
			struct hostent *hp;
			in_addr_t iface = inet_addr(listen_str);
			if (iface == htonl(INADDR_NONE)) {
				if (!host_lookup) {
					ok = 0;
				} else if (!(hp = gethostbyname(listen_str))) {
					ok = 0;
				} else {
					iface = *(unsigned long *)hp->h_addr;
				}
			}
			if (ok) {
				screen->listenInterface = iface;
			}
		}

		if (ok && mod) {
			int is_loopback = 0;
			in_addr_t iface = screen->listenInterface;

			if (allow_list) {
				if (!strcmp(allow_list, "127.0.0.1") ||
				    !strcmp(allow_list, "localhost")) {
					is_loopback = 1;
				}
			}
			if (iface != htonl(INADDR_LOOPBACK)) {
			    if (is_loopback) {
				rfbLog("re-setting -allow list to all "
				   "hosts for non-loopback listening.\n");
				free(allow_list);
				allow_list = NULL;
			    }
			} else {
			    if (!is_loopback) {
				if (allow_list) {
					free(allow_list);
				}
				rfbLog("setting -allow list to 127.0.0.1\n");
				allow_list = strdup("127.0.0.1");
			    }
			}
		}
		if (ok) {
			rfbLog("allow list is: '%s'\n", NONUL(allow_list));
			reset_rfbport(-1, screen->port);
			if (screen->httpListenSock > -1) {
				reset_httpport(-1, screen->httpPort);
			}
			free(before);
		} else {
			rfbLog("bad listen string: %s\n", listen_str);
			free(listen_str);
			listen_str = before;
		}
	} else if (!strcmp(p, "lookup")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, host_lookup);
			goto qry;
		}
		rfbLog("remote_cmd: enabling hostname lookup.\n");
		host_lookup = 1;
	} else if (!strcmp(p, "nolookup")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !host_lookup);
			goto qry;
		}
		rfbLog("remote_cmd: disabling hostname lookup.\n");
		host_lookup = 0;

	} else if (strstr(p, "accept") == p) {
		COLON_CHECK("accept:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(accept_cmd));
			goto qry;
		}
		if (safe_remote_only) {
			rfbLog("unsafe: %s\n", p);
		} else {
			p += strlen("accept:");
			if (accept_cmd) free(accept_cmd);
			accept_cmd = strdup(p);
		}

	} else if (strstr(p, "gone") == p) {
		COLON_CHECK("gone:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(gone_cmd));
			goto qry;
		}
		if (safe_remote_only) {
			rfbLog("unsafe: %s\n", p);
		} else {
			p += strlen("gone:");
			if (gone_cmd) free(gone_cmd);
			gone_cmd = strdup(p);
		}

	} else if (!strcmp(p, "shm")) {
		int orig = using_shm;
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, using_shm);
			goto qry;
		}
		rfbLog("remote_cmd: turning off noshm mode.\n");
		using_shm = 1;
		if (raw_fb) set_raw_fb_params(0);

		if (orig != using_shm) {
			do_new_fb(1);
		} else {
			rfbLog(" already in shm mode.\n");
		}
	} else if (!strcmp(p, "noshm")) {
		int orig = using_shm;
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !using_shm);
			goto qry;
		}
		rfbLog("remote_cmd: turning on noshm mode.\n");
		using_shm = 0;
		if (orig != using_shm) {
			do_new_fb(1);
		} else {
			rfbLog(" already in noshm mode.\n");
		}
		
	} else if (!strcmp(p, "flipbyteorder")) {
		int orig = flip_byte_order;
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, flip_byte_order);
			goto qry;
		}
		rfbLog("remote_cmd: turning on flipbyteorder mode.\n");
		flip_byte_order = 1;
		if (orig != flip_byte_order) {
			if (! using_shm) {
				do_new_fb(1);
			} else {
				rfbLog("  using shm, not resetting fb\n");
			}
		}
	} else if (!strcmp(p, "noflipbyteorder")) {
		int orig = flip_byte_order;
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !flip_byte_order);
			goto qry;
		}
		rfbLog("remote_cmd: turning off flipbyteorder mode.\n");
		flip_byte_order = 0;
		if (orig != flip_byte_order) {
			if (! using_shm) {
				do_new_fb(1);
			} else {
				rfbLog("  using shm, not resetting fb\n");
			}
		}
		
	} else if (!strcmp(p, "onetile")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, single_copytile);
			goto qry;
		}
		rfbLog("remote_cmd: enable -onetile mode.\n");
		single_copytile = 1;
	} else if (!strcmp(p, "noonetile")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !single_copytile);
			goto qry;
		}
		rfbLog("remote_cmd: disable -onetile mode.\n");
		if (tile_shm_count < ntiles_x) {
			rfbLog(" this has no effect: tile_shm_count=%d"
			    " ntiles_x=%d\n", tile_shm_count, ntiles_x);
			
		}
		single_copytile = 0;

	} else if (strstr(p, "solid_color") == p) {
		/*
		 * n.b. this solid stuff perhaps should reflect
		 * safe_remote_only but at least the command names
		 * are fixed.
		 */
		char *new;
		int doit = 1;
		COLON_CHECK("solid_color:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(solid_str));
			goto qry;
		}
		p += strlen("solid_color:");
		if (*p != '\0') {
			new = strdup(p);
		} else {
			new = strdup(solid_default);
		}
		rfbLog("remote_cmd: solid %s -> %s\n", NONUL(solid_str), new);

		if (solid_str) {
			if (!strcmp(solid_str, new)) {
				doit = 0;
			}
			free(solid_str);
		}
		solid_str = new;
		use_solid_bg = 1;
		if (raw_fb) set_raw_fb_params(0);

		if (doit && client_count) {
			solid_bg(0);
		}
	} else if (!strcmp(p, "solid")) {
		int orig = use_solid_bg;
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, use_solid_bg);
			goto qry;
		}
		rfbLog("remote_cmd: enable -solid mode\n");
		if (! solid_str) {
			solid_str = strdup(solid_default);
		}
		use_solid_bg = 1;
		if (raw_fb) set_raw_fb_params(0);
		if (client_count && !orig) {
			solid_bg(0);
		}
	} else if (!strcmp(p, "nosolid")) {
		int orig = use_solid_bg;
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !use_solid_bg);
			goto qry;
		}
		rfbLog("remote_cmd: disable -solid mode\n");
		use_solid_bg = 0;
		if (client_count && orig) {
			solid_bg(1);
		}

	} else if (strstr(p, "blackout") == p) {
		char *before, *old;
		COLON_CHECK("blackout:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(blackout_str));
			goto qry;
		}
		p += strlen("blackout:");
		if (blackout_str) {
			before = strdup(blackout_str);
		} else {
			before = strdup("");
		}
		old = blackout_str;
		if (*p == '+') {
			p++;
			blackout_str = add_item(blackout_str, p);
		} else if (*p == '-') {
			p++;
			blackout_str = delete_item(blackout_str, p);
		} else {
			blackout_str = strdup(p);
		}
		if (strcmp(before, blackout_str)) {
			rfbLog("remote_cmd: changing -blackout\n");
			rfbLog(" from: %s\n", before);
			rfbLog(" to:   %s\n", blackout_str);
			if (0 && !strcmp(blackout_str, "") &&
			    single_copytile_orig != single_copytile) {
				rfbLog("resetting single_copytile to: %d\n",
				    single_copytile_orig);
				single_copytile = single_copytile_orig;
			}
			initialize_blackouts_and_xinerama();
		}
		if (old) free(old);
		free(before);

	} else if (!strcmp(p, "xinerama")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, xinerama);
			goto qry;
		}
		rfbLog("remote_cmd: enable xinerama mode. (if applicable).\n");
		xinerama = 1;
		initialize_blackouts_and_xinerama();
	} else if (!strcmp(p, "noxinerama")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !xinerama);
			goto qry;
		}
		rfbLog("remote_cmd: disable xinerama mode. (if applicable).\n");
		xinerama = 0;
		initialize_blackouts_and_xinerama();

	} else if (!strcmp(p, "xtrap")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, xtrap_input);
			goto qry;
		}
		rfbLog("remote_cmd: enable xtrap input mode."
		    "(if applicable).\n");
		if (! xtrap_input) {
			xtrap_input = 1;
			disable_grabserver(dpy, 1);
		}

	} else if (!strcmp(p, "noxtrap")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !xtrap_input);
			goto qry;
		}
		rfbLog("remote_cmd: disable xtrap input mode."
		    "(if applicable).\n");
		if (xtrap_input) {
			xtrap_input = 0;
			disable_grabserver(dpy, 1);
		}

	} else if (!strcmp(p, "xrandr")) {
		int orig = xrandr;
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, xrandr); goto qry;
		}
		if (xrandr_present) {
			rfbLog("remote_cmd: enable xrandr mode.\n");
			xrandr = 1;
			if (raw_fb) set_raw_fb_params(0);
			if (! xrandr_mode) {
				xrandr_mode = strdup("default");
			}
			if (orig != xrandr) {
				initialize_xrandr();
			}
		} else {
			rfbLog("remote_cmd: XRANDR ext. not present.\n");
		}
	} else if (!strcmp(p, "noxrandr")) {
		int orig = xrandr;
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !xrandr); goto qry;
		}
		xrandr = 0;
		if (xrandr_present) {
			rfbLog("remote_cmd: disable xrandr mode.\n");
			if (orig != xrandr) {
				initialize_xrandr();
			}
		} else {
			rfbLog("remote_cmd: XRANDR ext. not present.\n");
		}
	} else if (strstr(p, "xrandr_mode") == p) {
		int orig = xrandr;
		COLON_CHECK("xrandr_mode:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(xrandr_mode));
			goto qry;
		}
		p += strlen("xrandr_mode:");
		if (!strcmp("none", p)) {
			xrandr = 0;
		} else {
			if (known_xrandr_mode(p)) {
				if (xrandr_mode) free(xrandr_mode);
				xrandr_mode = strdup(p);
			} else {
				rfbLog("skipping unknown xrandr mode: %s\n", p);
				goto done;
			}
			xrandr = 1;
		}
		if (xrandr_present) {
			if (xrandr) {
				rfbLog("remote_cmd: enable xrandr mode.\n");
			} else {
				rfbLog("remote_cmd: disable xrandr mode.\n");
			}
			if (! xrandr_mode) {
				xrandr_mode = strdup("default");
			}
			if (orig != xrandr) {
				initialize_xrandr();
			}
		} else {
			rfbLog("remote_cmd: XRANDR ext. not present.\n");
		}

	} else if (strstr(p, "padgeom") == p) {
		COLON_CHECK("padgeom:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(pad_geometry));
			goto qry;
		}
		p += strlen("padgeom:");
		if (!strcmp("force", p) || !strcmp("do",p) || !strcmp("go",p)) {
			rfbLog("remote_cmd: invoking install_padded_fb()\n");
			install_padded_fb(pad_geometry);
		} else {
			if (pad_geometry) free(pad_geometry);
			pad_geometry = strdup(p);
			rfbLog("remote_cmd: set padgeom to: %s\n",
			    pad_geometry);
		}

	} else if (!strcmp(p, "quiet") || !strcmp(p, "q")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, quiet); goto qry;
		}
		rfbLog("remote_cmd: turning on quiet mode.\n");
		quiet = 1;
	} else if (!strcmp(p, "noquiet")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !quiet); goto qry;
		}
		rfbLog("remote_cmd: turning off quiet mode.\n");
		quiet = 0;
		
	} else if (!strcmp(p, "modtweak")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, use_modifier_tweak);
			goto qry;
		}
		rfbLog("remote_cmd: enabling -modtweak mode.\n");
		if (! use_modifier_tweak) {
			use_modifier_tweak = 1;
			initialize_modtweak();
		}
		use_modifier_tweak = 1;

	} else if (!strcmp(p, "nomodtweak")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    !use_modifier_tweak);
			goto qry;
		}
		rfbLog("remote_cmd: enabling -nomodtweak mode.\n");
		got_nomodtweak = 1;
		use_modifier_tweak = 0;

	} else if (!strcmp(p, "xkb")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, use_xkb_modtweak);
			goto qry;
		}
		if (! xkb_present) {
			rfbLog("remote_cmd: cannot enable -xkb "
			    "modtweak mode (not supported on X display)\n");
			goto done;
		}
		rfbLog("remote_cmd: enabling -xkb modtweak mode"
		    " (if supported).\n");
		if (! use_modifier_tweak || ! use_xkb_modtweak) {
			use_modifier_tweak = 1;
			use_xkb_modtweak = 1;
			initialize_modtweak();
		}
		use_modifier_tweak = 1;
		use_xkb_modtweak = 1;

	} else if (!strcmp(p, "noxkb")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !use_xkb_modtweak);
			goto qry;
		}
		if (! xkb_present) {
			rfbLog("remote_cmd: cannot disable -xkb "
			    "modtweak mode (not supported on X display)\n");
			goto done;
		}
		rfbLog("remote_cmd: disabling -xkb modtweak mode.\n");
		use_xkb_modtweak = 0;
		got_noxkb = 1;
		initialize_modtweak();

	} else if (strstr(p, "skip_keycodes") == p) {
		COLON_CHECK("skip_keycodes:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(skip_keycodes));
			goto qry;
		}
		p += strlen("skip_keycodes:");
		rfbLog("remote_cmd: setting xkb -skip_keycodes"
		    " to:\n\t'%s'\n", p);
		if (! xkb_present) {
			rfbLog("remote_cmd: warning xkb not present\n");
		} else if (! use_xkb_modtweak) {
			rfbLog("remote_cmd: turning on xkb.\n");
			use_xkb_modtweak = 1;
			if (! use_modifier_tweak) {
				rfbLog("remote_cmd: turning on modtweak.\n");
				use_modifier_tweak = 1;
			}
		}
		if (skip_keycodes) free(skip_keycodes);
		skip_keycodes = strdup(p);
		initialize_modtweak();

	} else if (!strcmp(p, "skip_dups")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    skip_duplicate_key_events);
			goto qry;
		}
		rfbLog("remote_cmd: enabling -skip_dups mode\n");
		skip_duplicate_key_events = 1;
	} else if (!strcmp(p, "noskip_dups")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    !skip_duplicate_key_events);
			goto qry;
		}
		rfbLog("remote_cmd: disabling -skip_dups mode\n");
		skip_duplicate_key_events = 0;

	} else if (!strcmp(p, "add_keysyms")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, add_keysyms);
			goto qry;
		}
		rfbLog("remote_cmd: enabling -add_keysyms mode.\n");
		add_keysyms = 1;

	} else if (!strcmp(p, "noadd_keysyms")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !add_keysyms);
			goto qry;
		}
		rfbLog("remote_cmd: disabling -add_keysyms mode.\n");
		add_keysyms = 0;

	} else if (!strcmp(p, "clear_mods")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, clear_mods == 1);
			goto qry;
		}
		rfbLog("remote_cmd: enabling -clear_mods mode.\n");
		clear_mods = 1;
		clear_modifiers(0);

	} else if (!strcmp(p, "noclear_mods")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    !(clear_mods == 1));
			goto qry;
		}
		rfbLog("remote_cmd: disabling -clear_mods mode.\n");
		clear_mods = 0;

	} else if (!strcmp(p, "clear_keys")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    clear_mods == 2);
			goto qry;
		}
		rfbLog("remote_cmd: enabling -clear_keys mode.\n");
		clear_mods = 2;
		clear_keys();

	} else if (!strcmp(p, "noclear_keys")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    !(clear_mods == 2));
			goto qry;
		}
		rfbLog("remote_cmd: disabling -clear_keys mode.\n");
		clear_mods = 0;

	} else if (strstr(p, "remap") == p) {
		char *before, *old;
		COLON_CHECK("remap:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(remap_file));
			goto qry;
		}
		p += strlen("remap:");
		if ((*p == '+' || *p == '-') && remap_file &&
		    strchr(remap_file, '/')) {
			rfbLog("remote_cmd: cannot use remap:+/-\n");
			rfbLog("in '-remap %s' mode.\n", remap_file);
			goto done;
		}
		if (remap_file) {
			before = strdup(remap_file);
		} else {
			before = strdup("");
		}
		old = remap_file;
		if (*p == '+') {
			p++;
			remap_file = add_item(remap_file, p);
		} else if (*p == '-') {
			p++;
			remap_file = delete_item(remap_file, p);
			if (! strchr(remap_file, '-')) {
				*remap_file = '\0';
			}
		} else {
			remap_file = strdup(p);
		}
		if (strcmp(before, remap_file)) {
			rfbLog("remote_cmd: changed -remap\n");
			rfbLog(" from: %s\n", before);
			rfbLog(" to:   %s\n", remap_file);
			initialize_remap(remap_file);
		}
		if (old) free(old);
		free(before);

	} else if (!strcmp(p, "repeat")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !no_autorepeat);
			goto qry;
		}
		rfbLog("remote_cmd: enabling -repeat mode.\n");
		autorepeat(1, 0);	/* restore initial setting */
		no_autorepeat = 0;

	} else if (!strcmp(p, "norepeat")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, no_autorepeat);
			goto qry;
		}
		rfbLog("remote_cmd: enabling -norepeat mode.\n");
		no_autorepeat = 1;
		if (no_repeat_countdown >= 0) {
			no_repeat_countdown = 2;
		}
		if (client_count && ! view_only) {
			autorepeat(0, 0);	/* disable if any clients */
		}

	} else if (!strcmp(p, "fb")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !nofb);
			goto qry;
		}
		if (nofb) {
			rfbLog("remote_cmd: disabling nofb mode.\n");
			rfbLog("  you may need to these turn back on:\n");
			rfbLog("     xfixes, xdamage, solid, flashcmap\n");
			rfbLog("     overlay, shm, noonetile, nap, cursor\n");
			rfbLog("     cursorpos, cursorshape, bell.\n");
			nofb = 0;
			do_new_fb(1);
		}
	} else if (!strcmp(p, "nofb")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, nofb);
			goto qry;
		}
		if (!nofb) {
			rfbLog("remote_cmd: enabling nofb mode.\n");
			if (main_fb) {
				push_black_screen(4);
			}
			nofb = 1;
			sound_bell = 0;
			initialize_watch_bell();
			set_nofb_params();
			do_new_fb(1);
		}

	} else if (!strcmp(p, "bell")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, sound_bell);
			goto qry;
		}
		rfbLog("remote_cmd: enabling bell (if supported).\n");
		initialize_watch_bell();
		sound_bell = 1;

	} else if (!strcmp(p, "nobell")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !sound_bell);
			goto qry;
		}
		rfbLog("remote_cmd: disabling bell.\n");
		initialize_watch_bell();
		sound_bell = 0;

	} else if (!strcmp(p, "sel")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, watch_selection);
			goto qry;
		}
		rfbLog("remote_cmd: enabling watch selection+primary.\n");
		watch_selection = 1;
		watch_primary = 1;

	} else if (!strcmp(p, "nosel")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !watch_selection);
			goto qry;
		}
		rfbLog("remote_cmd: disabling watch selection+primary.\n");
		watch_selection = 0;
		watch_primary = 0;

	} else if (!strcmp(p, "primary")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, watch_primary);
			goto qry;
		}
		rfbLog("remote_cmd: enabling watch_primary.\n");
		watch_primary = 1;

	} else if (!strcmp(p, "noprimary")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !watch_primary);
			goto qry;
		}
		rfbLog("remote_cmd: disabling watch_primary.\n");
		watch_primary = 0;

	} else if (!strcmp(p, "set_no_cursor")) { /* skip-cmd-list */
		rfbLog("remote_cmd: calling set_no_cursor()\n");
		set_no_cursor();

	} else if (!strcmp(p, "cursorshape")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    cursor_shape_updates);
			goto qry;
		}
		rfbLog("remote_cmd: turning on cursorshape mode.\n");

		set_no_cursor();
		cursor_shape_updates = 1;
		restore_cursor_shape_updates(screen);
		first_cursor();
	} else if (!strcmp(p, "nocursorshape")) {
		int i, max = 5;
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    !cursor_shape_updates);
			goto qry;
		}
		rfbLog("remote_cmd: turning off cursorshape mode.\n");
		
		set_no_cursor();
		for (i=0; i<max; i++) {
			/* XXX: try to force empty cursor back to client */
			rfbPE(-1);
		}
		cursor_shape_updates = 0;
		disable_cursor_shape_updates(screen);
		first_cursor();

	} else if (!strcmp(p, "cursorpos")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    cursor_pos_updates);
			goto qry;
		}
		rfbLog("remote_cmd: turning on cursorpos mode.\n");
		cursor_pos_updates = 1;
	} else if (!strcmp(p, "nocursorpos")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    !cursor_pos_updates);
			goto qry;
		}
		rfbLog("remote_cmd: turning off cursorpos mode.\n");
		cursor_pos_updates = 0;

	} else if (strstr(p, "cursor") == p) {
		COLON_CHECK("cursor:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(multiple_cursors_mode));
			goto qry;
		}
		p += strlen("cursor:");
		if (multiple_cursors_mode) {
			if (prev_cursors_mode) free(prev_cursors_mode);
			prev_cursors_mode = strdup(multiple_cursors_mode);
			free(multiple_cursors_mode);
		}
		multiple_cursors_mode = strdup(p);

		rfbLog("remote_cmd: changed -cursor mode "
		    "to: %s\n", multiple_cursors_mode);

		if (strcmp(multiple_cursors_mode, "none") && !show_cursor) {
			show_cursor = 1;
			rfbLog("remote_cmd: changed show_cursor "
			    "to: %d\n", show_cursor);
		}
		initialize_cursors_mode();
		first_cursor();

	} else if (!strcmp(p, "show_cursor")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, show_cursor);
			goto qry;
		}
		rfbLog("remote_cmd: enabling show_cursor.\n");
		show_cursor = 1;
		if (multiple_cursors_mode && !strcmp(multiple_cursors_mode,
		    "none")) {
			free(multiple_cursors_mode);
			if (prev_cursors_mode) {
				multiple_cursors_mode =
				    strdup(prev_cursors_mode);
			} else {
				multiple_cursors_mode = strdup("default");
			}
			rfbLog("remote_cmd: changed -cursor mode "
			    "to: %s\n", multiple_cursors_mode);
		}
		initialize_cursors_mode();
		first_cursor();
	} else if (!strcmp(p, "noshow_cursor") || !strcmp(p, "nocursor")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !show_cursor);
			goto qry;
		}
		if (prev_cursors_mode) free(prev_cursors_mode);
		prev_cursors_mode = strdup(multiple_cursors_mode);

		rfbLog("remote_cmd: disabling show_cursor.\n");
		show_cursor = 0;
		initialize_cursors_mode();
		first_cursor();

	} else if (strstr(p, "arrow") == p) {
		COLON_CHECK("arrow:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co, alt_arrow);
			goto qry;
		}
		p += strlen("arrow:");
		alt_arrow = atoi(p);
		rfbLog("remote_cmd: setting alt_arrow: %d.\n", alt_arrow);
		setup_cursors_and_push();

	} else if (!strcmp(p, "xfixes")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, use_xfixes);
			goto qry;
		}
		if (! xfixes_present) {
			rfbLog("remote_cmd: cannot enable xfixes "
			    "(not supported on X display)\n");
			goto done;
		}
		rfbLog("remote_cmd: enabling -xfixes"
		    " (if supported).\n");
		use_xfixes = 1;
		initialize_xfixes();
		first_cursor();
	} else if (!strcmp(p, "noxfixes")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !use_xfixes);
			goto qry;
		}
		if (! xfixes_present) {
			rfbLog("remote_cmd: disabling xfixes  "
			    "(but not supported on X display)\n");
			goto done;
		}
		rfbLog("remote_cmd: disabling -xfixes.\n");
		use_xfixes = 0;
		initialize_xfixes();
		first_cursor();

	} else if (!strcmp(p, "xdamage")) {
		int orig = use_xdamage;
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, use_xdamage);
			goto qry;
		}
		if (! xdamage_present) {
			rfbLog("remote_cmd: cannot enable xdamage hints "
			    "(not supported on X display)\n");
			goto done;
		}
		rfbLog("remote_cmd: enabling xdamage hints"
		    " (if supported).\n");
		use_xdamage = 1;
		if (use_xdamage != orig) {
			initialize_xdamage();
			create_xdamage_if_needed();
		}
	} else if (!strcmp(p, "noxdamage")) {
		int orig = use_xdamage;
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !use_xdamage);
			goto qry;
		}
		if (! xdamage_present) {
			rfbLog("remote_cmd: disabling xdamage hints "
			    "(but not supported on X display)\n");
			goto done;
		}
		rfbLog("remote_cmd: disabling xdamage hints.\n");
		use_xdamage = 0;
		if (use_xdamage != orig) {
			initialize_xdamage();
			destroy_xdamage_if_needed();
		}

	} else if (strstr(p, "xd_area") == p) {
		int a;
		COLON_CHECK("xd_area:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co,
			    xdamage_max_area);
			goto qry;
		}
		p += strlen("xd_area:");
		a = atoi(p);
		if (a >= 0) {
			rfbLog("remote_cmd: setting xdamage_max_area "
			    "%d -> %d.\n", xdamage_max_area, a);
			xdamage_max_area = a;
		}
	} else if (strstr(p, "xd_mem") == p) {
		double a;
		COLON_CHECK("xd_mem:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%.3f", p, co,
			    xdamage_memory);
			goto qry;
		}
		p += strlen("xd_mem:");
		a = atof(p);
		if (a >= 0.0) {
			rfbLog("remote_cmd: setting xdamage_memory "
			    "%.3f -> %.3f.\n", xdamage_memory, a);
			xdamage_memory = a;
		}

	} else if (strstr(p, "alphacut") == p) {
		int a;
		COLON_CHECK("alphacut:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co,
			    alpha_threshold);
			goto qry;
		}
		p += strlen("alphacut:");
		a = atoi(p);
		if (a < 0) a = 0;
		if (a > 256) a = 256;	/* allow 256 for testing. */
		if (alpha_threshold != a) {
			rfbLog("remote_cmd: setting alphacut "
			    "%d -> %d.\n", alpha_threshold, a);
			if (a == 256) {
				rfbLog("note: alphacut=256 leads to completely"
				    " transparent cursors.\n");
			}
			alpha_threshold = a;
			setup_cursors_and_push();
		}
	} else if (strstr(p, "alphafrac") == p) {
		double a;
		COLON_CHECK("alphafrac:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%f", p, co,
			    alpha_frac);
			goto qry;
		}
		p += strlen("alphafrac:");
		a = atof(p);
		if (a < 0.0) a = 0.0;
		if (a > 1.0) a = 1.0;
		if (alpha_frac != a) {
			rfbLog("remote_cmd: setting alphafrac "
			    "%f -> %f.\n", alpha_frac, a);
			alpha_frac = a;
			setup_cursors_and_push();
		}
	} else if (strstr(p, "alpharemove") == p) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, alpha_remove);
			goto qry;
		}
		if (!alpha_remove) {
			rfbLog("remote_cmd: enable alpharemove\n");
			alpha_remove = 1;
			setup_cursors_and_push();
		}
	} else if (strstr(p, "noalpharemove") == p) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !alpha_remove);
			goto qry;
		}
		if (alpha_remove) {
			rfbLog("remote_cmd: disable alpharemove\n");
			alpha_remove = 0;
			setup_cursors_and_push();
		}
	} else if (strstr(p, "alphablend") == p) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, alpha_blend);
			goto qry;
		}
		if (!alpha_blend) {
			rfbLog("remote_cmd: enable alphablend\n");
			alpha_remove = 0;
			alpha_blend = 1;
			setup_cursors_and_push();
		}
	} else if (strstr(p, "noalphablend") == p) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !alpha_blend);
			goto qry;
		}
		if (alpha_blend) {
			rfbLog("remote_cmd: disable alphablend\n");
			alpha_blend = 0;
			setup_cursors_and_push();
		}

	} else if (strstr(p, "xwarppointer") == p || strstr(p, "xwarp") == p) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, use_xwarppointer);
			goto qry;
		}
		rfbLog("remote_cmd: turning on xwarppointer mode.\n");
		use_xwarppointer = 1;
	} else if (strstr(p, "noxwarppointer") == p ||
		    strstr(p, "noxwarp") == p) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !use_xwarppointer);
			goto qry;
		}
		rfbLog("remote_cmd: turning off xwarppointer mode.\n");
		use_xwarppointer = 0;

	} else if (strstr(p, "buttonmap") == p) {
		COLON_CHECK("buttonmap:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(pointer_remap));
			goto qry;
		}
		p += strlen("buttonmap:");
		if (pointer_remap) free(pointer_remap);
		pointer_remap = strdup(p);

		rfbLog("remote_cmd: setting -buttonmap to:\n\t'%s'\n", p);
		initialize_pointer_map(p);

	} else if (!strcmp(p, "dragging")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, show_dragging);
			goto qry;
		}
		rfbLog("remote_cmd: enabling mouse dragging mode.\n");
		show_dragging = 1;
	} else if (!strcmp(p, "nodragging")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !show_dragging);
			goto qry;
		}
		rfbLog("remote_cmd: enabling mouse nodragging mode.\n");
		show_dragging = 0;

	} else if (strstr(p, "wireframe_mode") == p) {
		COLON_CHECK("wireframe_mode:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    wireframe_str ? wireframe_str : WIREFRAME_PARMS);
			goto qry;
		}
		p += strlen("wireframe_mode:");
		if (*p) {
			if (wireframe_str) {
				free(wireframe_str);
			}
			wireframe_str = strdup(p);
			parse_wireframe();
		}
		rfbLog("remote_cmd: enabling -wireframe mode.\n");
		wireframe = 1;
	} else if (strstr(p, "wireframe:") == p) {	/* skip-cmd-list */
		COLON_CHECK("wireframe:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co, wireframe);
			goto qry;
		}
		p += strlen("wireframe:");
		if (*p) {
			if (wireframe_str) {
				free(wireframe_str);
			}
			wireframe_str = strdup(p);
			parse_wireframe();
		}
		rfbLog("remote_cmd: enabling -wireframe mode.\n");
		wireframe = 1;
	} else if (strstr(p, "wf:") == p) {	/* skip-cmd-list */
		COLON_CHECK("wf:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co, wireframe);
			goto qry;
		}
		p += strlen("wf:");
		if (*p) {
			if (wireframe_str) {
				free(wireframe_str);
			}
			wireframe_str = strdup(p);
			parse_wireframe();
		}
		rfbLog("remote_cmd: enabling -wireframe mode.\n");
		wireframe = 1;
	} else if (!strcmp(p, "wireframe") || !strcmp(p, "wf")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, wireframe);
			goto qry;
		}
		rfbLog("remote_cmd: enabling -wireframe mode.\n");
		wireframe = 1;
	} else if (!strcmp(p, "nowireframe") || !strcmp(p, "nowf")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !wireframe);
			goto qry;
		}
		rfbLog("remote_cmd: enabling -nowireframe mode.\n");
		wireframe = 0;

	} else if (strstr(p, "wirecopyrect") == p) {
		COLON_CHECK("wirecopyrect:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(wireframe_copyrect));
			goto qry;
		}
		p += strlen("wirecopyrect:");

		set_wirecopyrect_mode(p);
		rfbLog("remote_cmd: changed -wirecopyrect mode "
		    "to: %s\n", NONUL(wireframe_copyrect));
		got_wirecopyrect = 1;
	} else if (strstr(p, "wcr") == p) {
		COLON_CHECK("wcr:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(wireframe_copyrect));
			goto qry;
		}
		p += strlen("wcr:");

		set_wirecopyrect_mode(p);
		rfbLog("remote_cmd: changed -wirecopyrect mode "
		    "to: %s\n", NONUL(wireframe_copyrect));
		got_wirecopyrect = 1;
	} else if (!strcmp(p, "nowirecopyrect") || !strcmp(p, "nowcr")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%s", p,
			    NONUL(wireframe_copyrect));
			goto qry;
		}

		set_wirecopyrect_mode("never");
		rfbLog("remote_cmd: changed -wirecopyrect mode "
		    "to: %s\n", NONUL(wireframe_copyrect));

	} else if (strstr(p, "scr_area") == p) {
		COLON_CHECK("scr_area:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co,
			    scrollcopyrect_min_area);
			goto qry;
		}
		p += strlen("scr_area:");

		scrollcopyrect_min_area = atoi(p);
		rfbLog("remote_cmd: changed -scr_area to: %d\n",
		    scrollcopyrect_min_area);

	} else if (strstr(p, "scr_skip") == p) {
		char *s = scroll_skip_str;
		COLON_CHECK("scr_skip:")
		if (!s || *s == '\0') s = scroll_skip_str0;
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co, NONUL(s));
			goto qry;
		}
		p += strlen("scr_skip:");
		if (scroll_skip_str) {
			free(scroll_skip_str);
		}

		scroll_skip_str = strdup(p);
		rfbLog("remote_cmd: changed -scr_skip to: %s\n",
		    scroll_skip_str);
		initialize_scroll_matches();
	} else if (strstr(p, "scr_inc") == p) {
		char *s = scroll_good_str;
		if (!s || *s == '\0') s = scroll_good_str0;
		COLON_CHECK("scr_inc:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co, NONUL(s));
			goto qry;
		}
		p += strlen("scr_inc:");
		if (scroll_good_str) {
			free(scroll_good_str);
		}

		scroll_good_str = strdup(p);
		rfbLog("remote_cmd: changed -scr_inc to: %s\n",
		    scroll_good_str);
		initialize_scroll_matches();
	} else if (strstr(p, "scr_keys") == p) {
		COLON_CHECK("scr_keys:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(scroll_key_list_str));
			goto qry;
		}
		p += strlen("scr_keys:");
		if (scroll_key_list_str) {
			free(scroll_key_list_str);
		}

		scroll_key_list_str = strdup(p);
		rfbLog("remote_cmd: changed -scr_keys to: %s\n",
		    scroll_key_list_str);
		initialize_scroll_keys();
	} else if (strstr(p, "scr_term") == p) {
		char *s = scroll_term_str;
		if (!s || *s == '\0') s = scroll_term_str0;
		COLON_CHECK("scr_term:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co, NONUL(s));
			goto qry;
		}
		p += strlen("scr_term:");
		if (scroll_term_str) {
			free(scroll_term_str);
		}

		scroll_term_str = strdup(p);
		rfbLog("remote_cmd: changed -scr_term to: %s\n",
		    scroll_term_str);
		initialize_scroll_term();

	} else if (strstr(p, "scr_keyrepeat") == p) {
		char *s = max_keyrepeat_str;
		if (!s || *s == '\0') s = max_keyrepeat_str0;
		COLON_CHECK("scr_keyrepeat:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co, NONUL(s));
			goto qry;
		}
		p += strlen("scr_keyrepeat:");
		if (max_keyrepeat_str) {
			free(max_keyrepeat_str);
		}

		max_keyrepeat_str = strdup(p);
		rfbLog("remote_cmd: changed -scr_keyrepeat to: %s\n",
		    max_keyrepeat_str);
		initialize_max_keyrepeat();

	} else if (strstr(p, "scr_parms") == p) {
		COLON_CHECK("scr_parms:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    scroll_copyrect_str ? scroll_copyrect_str
			    : SCROLL_COPYRECT_PARMS);
			goto qry;
		}
		p += strlen("scr_parms:");
		if (*p) {
			if (scroll_copyrect_str) {
				free(scroll_copyrect_str);
			}
			set_scrollcopyrect_mode("always");
			scroll_copyrect_str = strdup(p);
			parse_scroll_copyrect();
		}
		rfbLog("remote_cmd: set -scr_parms %s.\n",
		    NONUL(scroll_copyrect_str));
		got_scrollcopyrect = 1;

	} else if (strstr(p, "scrollcopyrect") == p) {
		COLON_CHECK("scrollcopyrect:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(scroll_copyrect));
			goto qry;
		}
		p += strlen("scrollcopyrect:");

		set_scrollcopyrect_mode(p);
		rfbLog("remote_cmd: changed -scrollcopyrect mode "
		    "to: %s\n", NONUL(scroll_copyrect));
		got_scrollcopyrect = 1;
	} else if (!strcmp(p, "scr") ||
	    strstr(p, "scr:") == p) {	/* skip-cmd-list */
		COLON_CHECK("scr:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(scroll_copyrect));
			goto qry;
		}
		p += strlen("scr:");

		set_scrollcopyrect_mode(p);
		rfbLog("remote_cmd: changed -scrollcopyrect mode "
		    "to: %s\n", NONUL(scroll_copyrect));
		got_scrollcopyrect = 1;
	} else if (!strcmp(p, "noscrollcopyrect") || !strcmp(p, "noscr")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%s", p,
			    NONUL(scroll_copyrect));
			goto qry;
		}

		set_scrollcopyrect_mode("never");
		rfbLog("remote_cmd: changed -scrollcopyrect mode "
		    "to: %s\n", NONUL(scroll_copyrect));

	} else if (!strcmp(p, "noxrecord")) {
		int orig = noxrecord;
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, noxrecord);
			goto qry;
		}
		noxrecord = 1;
		rfbLog("set noxrecord to: %d\n", noxrecord);
		if (orig != noxrecord) {
			shutdown_xrecord();
		}
	} else if (!strcmp(p, "xrecord")) {
		int orig = noxrecord;
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !noxrecord);
			goto qry;
		}
		noxrecord = 0;
		rfbLog("set noxrecord to: %d\n", noxrecord);
		if (orig != noxrecord) {
			initialize_xrecord();
		}

	} else if (strstr(p, "pointer_mode") == p) {
		int pm;
		COLON_CHECK("pointer_mode:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co, pointer_mode);
			goto qry;
		}
		p += strlen("pointer_mode:");
		pm = atoi(p);
		if (pm < 0 || pm > pointer_mode_max) {
			rfbLog("remote_cmd: pointer_mode out of range:"
			   " 1-%d: %d\n", pointer_mode_max, pm);
		} else {
			rfbLog("remote_cmd: setting pointer_mode %d\n", pm);
			pointer_mode = pm;
		}
	} else if (strstr(p, "pm") == p) {
		int pm;
		COLON_CHECK("pm:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co, pointer_mode);
			goto qry;
		}
		p += strlen("pm:");
		pm = atoi(p);
		if (pm < 0 || pm > pointer_mode_max) {
			rfbLog("remote_cmd: pointer_mode out of range:"
			   " 1-%d: %d\n", pointer_mode_max, pm);
		} else {
			rfbLog("remote_cmd: setting pointer_mode %d\n", pm);
			pointer_mode = pm;
		}

	} else if (strstr(p, "input_skip") == p) {
		int is;
		COLON_CHECK("input_skip:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co, ui_skip);
			goto qry;
		}
		p += strlen("input_skip:");
		is = atoi(p);
		rfbLog("remote_cmd: setting input_skip %d\n", is);
		ui_skip = is;

	} else if (strstr(p, "input") == p) {
		int doit = 1;
		COLON_CHECK("input:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(allowed_input_str));
			goto qry;
		}
		p += strlen("input:");
		if (allowed_input_str && !strcmp(p, allowed_input_str)) {
			doit = 0;
		}
		rfbLog("remote_cmd: setting input %s\n", p);
		if (allowed_input_str) free(allowed_input_str);
		if (*p == '\0') {
			allowed_input_str = NULL;
		} else {
			allowed_input_str = strdup(p);
		}
		if (doit) {
			initialize_allowed_input();
		}
	} else if (strstr(p, "client_input") == p) {
		NOTAPP
		COLON_CHECK("client_input:")
		p += strlen("client_input:");
		set_client_input(p);

	} else if (strstr(p, "speeds") == p) {
		COLON_CHECK("speeds:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(speeds_str));
			goto qry;
		}
		p += strlen("speeds:");
		if (speeds_str) free(speeds_str);
		speeds_str = strdup(p);

		rfbLog("remote_cmd: setting -speeds to:\n\t'%s'\n", p);
		initialize_speeds();

	} else if (!strcmp(p, "debug_pointer") || !strcmp(p, "dp")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, debug_pointer);
			goto qry;
		}
		rfbLog("remote_cmd: turning on debug_pointer.\n");
		debug_pointer = 1;
	} else if (!strcmp(p, "nodebug_pointer") || !strcmp(p, "nodp")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !debug_pointer);
			goto qry;
		}
		rfbLog("remote_cmd: turning off debug_pointer.\n");
		debug_pointer = 0;

	} else if (!strcmp(p, "debug_keyboard") || !strcmp(p, "dk")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, debug_keyboard);
			goto qry;
		}
		rfbLog("remote_cmd: turning on debug_keyboard.\n");
		debug_keyboard = 1;
	} else if (!strcmp(p, "nodebug_keyboard") || !strcmp(p, "nodk")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !debug_keyboard);
			goto qry;
		}
		rfbLog("remote_cmd: turning off debug_keyboard.\n");
		debug_keyboard = 0;

	} else if (strstr(p, "deferupdate") == p) {
		int d;
		COLON_CHECK("deferupdate:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co,
			    screen->deferUpdateTime);
			goto qry;
		}
		p += strlen("deferupdate:");
		d = atoi(p);
		if (d < 0) d = 0;
		rfbLog("remote_cmd: setting defer to %d ms.\n", d);
		screen->deferUpdateTime = d;
		
	} else if (strstr(p, "defer") == p) {
		int d;
		COLON_CHECK("defer:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co,
			    screen->deferUpdateTime);
			goto qry;
		}
		p += strlen("defer:");
		d = atoi(p);
		if (d < 0) d = 0;
		rfbLog("remote_cmd: setting defer to %d ms.\n", d);
		/* XXX not part of API? */
		screen->deferUpdateTime = d;
		
	} else if (strstr(p, "wait_ui") == p) {
		double w;
		COLON_CHECK("wait_ui:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%.2f", p, co, wait_ui);
			goto qry;
		}
		p += strlen("wait_ui:");
		w = atof(p);
		if (w <= 0) w = 1.0;
		rfbLog("remote_cmd: setting wait_ui factor %.2f -> %.2f\n",
		    wait_ui, w);
		wait_ui = w;

	} else if (!strcmp(p, "wait_bog")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co, wait_bog);
			goto qry;
		}
		wait_bog = 1;
		rfbLog("remote_cmd: setting wait_bog to %d\n", wait_bog);
	} else if (!strcmp(p, "nowait_bog")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co, !wait_bog);
			goto qry;
		}
		wait_bog = 0;
		rfbLog("remote_cmd: setting wait_bog to %d\n", wait_bog);

	} else if (strstr(p, "wait") == p) {
		int w;
		COLON_CHECK("wait:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co, waitms);
			goto qry;
		}
		p += strlen("wait:");
		w = atoi(p);
		if (w < 0) w = 0;
		rfbLog("remote_cmd: setting wait %d -> %d ms.\n", waitms, w);
		waitms = w;

	} else if (strstr(p, "readtimeout") == p) {
		int w, orig = rfbMaxClientWait;
		COLON_CHECK("readtimeout:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co,
			    rfbMaxClientWait/1000);
			goto qry;
		}
		p += strlen("readtimeout:");
		w = atoi(p) * 1000;
		if (w <= 0) w = 0;
		rfbLog("remote_cmd: setting rfbMaxClientWait %d -> "
		    "%d msec.\n", orig, w);
		rfbMaxClientWait = w;

	} else if (!strcmp(p, "nap")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, take_naps);
			    goto qry;
		}
		rfbLog("remote_cmd: turning on nap mode.\n");
		take_naps = 1;
	} else if (!strcmp(p, "nonap")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !take_naps);
			    goto qry;
		}
		rfbLog("remote_cmd: turning off nap mode.\n");
		take_naps = 0;

	} else if (strstr(p, "sb") == p) {
		int w;
		COLON_CHECK("sb:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co, screen_blank);
			goto qry;
		}
		p += strlen("sb:");
		w = atoi(p);
		if (w < 0) w = 0;
		rfbLog("remote_cmd: setting screen_blank %d -> %d sec.\n",
		    screen_blank, w);
		screen_blank = w;
	} else if (strstr(p, "screen_blank") == p) {
		int w;
		COLON_CHECK("screen_blank:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co, screen_blank);
			goto qry;
		}
		p += strlen("screen_blank:");
		w = atoi(p);
		if (w < 0) w = 0;
		rfbLog("remote_cmd: setting screen_blank %d -> %d sec.\n",
		    screen_blank, w);
		screen_blank = w;

	} else if (strstr(p, "fs") == p) {
		COLON_CHECK("fs:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%f", p, co, fs_frac);
			goto qry;
		}
		p += strlen("fs:");
		fs_frac = atof(p);
		rfbLog("remote_cmd: setting -fs frac to %f\n", fs_frac);

	} else if (strstr(p, "gaps") == p) {
		int g;
		COLON_CHECK("gaps:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co, gaps_fill);
			goto qry;
		}
		p += strlen("gaps:");
		g = atoi(p);
		if (g < 0) g = 0;
		rfbLog("remote_cmd: setting gaps_fill %d -> %d.\n",
		    gaps_fill, g);
		gaps_fill = g;
	} else if (strstr(p, "grow") == p) {
		int g;
		COLON_CHECK("grow:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co, grow_fill);
			goto qry;
		}
		p += strlen("grow:");
		g = atoi(p);
		if (g < 0) g = 0;
		rfbLog("remote_cmd: setting grow_fill %d -> %d.\n",
		    grow_fill, g);
		grow_fill = g;
	} else if (strstr(p, "fuzz") == p) {
		int f;
		COLON_CHECK("fuzz:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co, tile_fuzz);
			goto qry;
		}
		p += strlen("fuzz:");
		f = atoi(p);
		if (f < 0) f = 0;
		rfbLog("remote_cmd: setting tile_fuzz %d -> %d.\n",
		    tile_fuzz, f);
		grow_fill = f;

	} else if (!strcmp(p, "snapfb")) {
		int orig = use_snapfb;
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, use_snapfb);
			    goto qry;
		}
		rfbLog("remote_cmd: turning on snapfb mode.\n");
		use_snapfb = 1;
		if (orig != use_snapfb) {
			do_new_fb(1);
		}
	} else if (!strcmp(p, "nosnapfb")) {
		int orig = use_snapfb;
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !use_snapfb);
			    goto qry;
		}
		rfbLog("remote_cmd: turning off snapfb mode.\n");
		use_snapfb = 0;
		if (orig != use_snapfb) {
			do_new_fb(1);
		}

	} else if (strstr(p, "rawfb") == p) {
		COLON_CHECK("rawfb:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(raw_fb_str));
			goto qry;
		}
		p += strlen("rawfb:");
		if (raw_fb_str) free(raw_fb_str);
		raw_fb_str = strdup(p);
		if (safe_remote_only && strstr(p, "setup:") == p) { /* skip-cmd-list */
			/* n.b. we still allow filename, shm, of rawfb */
			fprintf(stderr, "unsafe rawfb setup: %s\n", p);
			exit(1);
		}

		rfbLog("remote_cmd: setting -rawfb to:\n\t'%s'\n", p);

		if (*raw_fb_str == '\0') {
			free(raw_fb_str);
			raw_fb_str = NULL;
			rfbLog("restoring per-rawfb settings...\n");
			set_raw_fb_params(1);
		}
		rfbLog("hang on tight, here we go...\n");
		do_new_fb(1);

	} else if (strstr(p, "progressive") == p) {
		int f;
		COLON_CHECK("progressive:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co,
			    screen->progressiveSliceHeight);
			goto qry;
		}
		p += strlen("progressive:");
		f = atoi(p);
		if (f < 0) f = 0;
		rfbLog("remote_cmd: setting progressive %d -> %d.\n",
		    screen->progressiveSliceHeight, f);
		screen->progressiveSliceHeight = f;

	} else if (strstr(p, "rfbport") == p) {
		int rp, orig = screen->port;
		COLON_CHECK("rfbport:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co, screen->port);
			goto qry;
		}
		p += strlen("rfbport:");
		rp = atoi(p);
		reset_rfbport(orig, rp);

	} else if (!strcmp(p, "http")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    (screen->httpListenSock > -1));
			goto qry;
		}
		if (screen->httpListenSock > -1) {
			rfbLog("already listening for http connections.\n");
		} else {
			rfbLog("turning on listening for http connections.\n");
			if (check_httpdir()) {
				http_connections(1);
			}
		}
	} else if (!strcmp(p, "nohttp")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    !(screen->httpListenSock > -1));
			goto qry;
		}
		if (screen->httpListenSock < 0) {
			rfbLog("already not listening for http connections.\n");
		} else {
			rfbLog("turning off listening for http connections.\n");
			if (check_httpdir()) {
				http_connections(0);
			}
		}

	} else if (strstr(p, "httpport") == p) {
		int hp, orig = screen->httpPort;
		COLON_CHECK("httpport:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co,
			    screen->httpPort);
			goto qry;
		}
		p += strlen("httpport:");
		hp = atoi(p);
		reset_httpport(orig, hp);

	} else if (strstr(p, "httpdir") == p) {
		COLON_CHECK("httpdir:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(http_dir));
			goto qry;
		}
		p += strlen("httpdir:");
		if (http_dir && !strcmp(http_dir, p)) {
			rfbLog("no change in httpdir: %s\n", http_dir);
		} else {
			if (http_dir) {
				free(http_dir);
			}
			http_dir = strdup(p);
			http_connections(0);
			if (*p != '\0') {
				http_connections(1);
			}
		}

	} else if (!strcmp(p, "enablehttpproxy")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    screen->httpEnableProxyConnect != 0);
			goto qry;
		}
		rfbLog("turning on enablehttpproxy.\n");
		screen->httpEnableProxyConnect = 1;
	} else if (!strcmp(p, "noenablehttpproxy")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    screen->httpEnableProxyConnect == 0);
			goto qry;
		}
		rfbLog("turning off enablehttpproxy.\n");
		screen->httpEnableProxyConnect = 0;

	} else if (!strcmp(p, "alwaysshared")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    screen->alwaysShared != 0);
			goto qry;
		}
		rfbLog("turning on alwaysshared.\n");
		screen->alwaysShared = 1;
	} else if (!strcmp(p, "noalwaysshared")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    screen->alwaysShared == 0);
			goto qry;
		}
		rfbLog("turning off alwaysshared.\n");
		screen->alwaysShared = 0;

	} else if (!strcmp(p, "nevershared")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    screen->neverShared != 0);
			goto qry;
		}
		rfbLog("turning on nevershared.\n");
		screen->neverShared = 1;
	} else if (!strcmp(p, "noalwaysshared")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    screen->neverShared == 0);
			goto qry;
		}
		rfbLog("turning off nevershared.\n");
		screen->neverShared = 0;

	} else if (!strcmp(p, "dontdisconnect")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    screen->dontDisconnect != 0);
			goto qry;
		}
		rfbLog("turning on dontdisconnect.\n");
		screen->dontDisconnect = 1;
	} else if (!strcmp(p, "nodontdisconnect")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    screen->dontDisconnect == 0);
			goto qry;
		}
		rfbLog("turning off dontdisconnect.\n");
		screen->dontDisconnect = 0;

	} else if (!strcmp(p, "desktop") ||
	    strstr(p, "desktop:") == p) {	/* skip-cmd-list */
		COLON_CHECK("desktop:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(rfb_desktop_name));
			goto qry;
		}
		p += strlen("desktop:");
		if (rfb_desktop_name) {
			free(rfb_desktop_name);
		}
		rfb_desktop_name = strdup(p);
		screen->desktopName = rfb_desktop_name;
		rfbLog("remote_cmd: setting desktop name to %s\n",
		    rfb_desktop_name);

	} else if (!strcmp(p, "debug_xevents")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, debug_xevents);
			goto qry;
		}
		debug_xevents = 1;
		rfbLog("set debug_xevents to: %d\n", debug_xevents);
	} else if (!strcmp(p, "nodebug_xevents")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !debug_xevents);
			goto qry;
		}
		debug_xevents = 0;
		rfbLog("set debug_xevents to: %d\n", debug_xevents);
	} else if (strstr(p, "debug_xevents") == p) {
		COLON_CHECK("debug_xevents:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co, debug_xevents);
			goto qry;
		}
		p += strlen("debug_xevents:");
		debug_xevents = atoi(p);
		rfbLog("set debug_xevents to: %d\n", debug_xevents);

	} else if (!strcmp(p, "debug_xdamage")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, debug_xdamage);
			goto qry;
		}
		debug_xdamage = 1;
		rfbLog("set debug_xdamage to: %d\n", debug_xdamage);
	} else if (!strcmp(p, "nodebug_xdamage")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !debug_xdamage);
			goto qry;
		}
		debug_xdamage = 0;
		rfbLog("set debug_xdamage to: %d\n", debug_xdamage);
	} else if (strstr(p, "debug_xdamage") == p) {
		COLON_CHECK("debug_xdamage:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co, debug_xdamage);
			goto qry;
		}
		p += strlen("debug_xdamage:");
		debug_xdamage = atoi(p);
		rfbLog("set debug_xdamage to: %d\n", debug_xdamage);

	} else if (!strcmp(p, "debug_wireframe")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, debug_wireframe);
			goto qry;
		}
		debug_wireframe = 1;
		rfbLog("set debug_wireframe to: %d\n", debug_wireframe);
	} else if (!strcmp(p, "nodebug_wireframe")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !debug_wireframe);
			goto qry;
		}
		debug_wireframe = 0;
		rfbLog("set debug_wireframe to: %d\n", debug_wireframe);
	} else if (strstr(p, "debug_wireframe") == p) {
		COLON_CHECK("debug_wireframe:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co,
			    debug_wireframe);
			goto qry;
		}
		p += strlen("debug_wireframe:");
		debug_wireframe = atoi(p);
		rfbLog("set debug_wireframe to: %d\n", debug_wireframe);

	} else if (!strcmp(p, "debug_scroll")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, debug_scroll);
			goto qry;
		}
		debug_scroll = 1;
		rfbLog("set debug_scroll to: %d\n", debug_scroll);
	} else if (!strcmp(p, "nodebug_scroll")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !debug_scroll);
			goto qry;
		}
		debug_scroll = 0;
		rfbLog("set debug_scroll to: %d\n", debug_scroll);
	} else if (strstr(p, "debug_scroll") == p) {
		COLON_CHECK("debug_scroll:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co,
			    debug_scroll);
			goto qry;
		}
		p += strlen("debug_scroll:");
		debug_scroll = atoi(p);
		rfbLog("set debug_scroll to: %d\n", debug_scroll);

	} else if (!strcmp(p, "debug_tiles") || !strcmp(p, "dbt")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, debug_tiles);
			goto qry;
		}
		debug_tiles = 1;
		rfbLog("set debug_tiles to: %d\n", debug_tiles);
	} else if (!strcmp(p, "nodebug_tiles") || !strcmp(p, "nodbt")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !debug_tiles);
			goto qry;
		}
		debug_tiles = 0;
		rfbLog("set debug_tiles to: %d\n", debug_tiles);
	} else if (strstr(p, "debug_tiles") == p) {
		COLON_CHECK("debug_tiles:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co,
			    debug_tiles);
			goto qry;
		}
		p += strlen("debug_tiles:");
		debug_tiles = atoi(p);
		rfbLog("set debug_tiles to: %d\n", debug_tiles);

	} else if (!strcmp(p, "dbg")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, crash_debug);
			goto qry;
		}
		crash_debug = 1;
		rfbLog("set crash_debug to: %d\n", crash_debug);
	} else if (!strcmp(p, "nodbg")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !crash_debug);
			goto qry;
		}
		crash_debug = 0;
		rfbLog("set crash_debug to: %d\n", crash_debug);

	} else if (strstr(p, "hack") == p) { /* skip-cmd-list */
		COLON_CHECK("hack:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co, hack_val);
			goto qry;
		}
		p += strlen("hack:");
		hack_val = atoi(p);
		rfbLog("set hack_val to: %d\n", hack_val);

	} else if (!strcmp(p, "noremote")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    !accept_remote_cmds);
			goto qry;
		}
		rfbLog("remote_cmd: disabling remote commands.\n");
		accept_remote_cmds = 0; /* cannot be turned back on. */

	} else if (query) {
		/* read-only variables that can only be queried: */

		if (!strcmp(p, "display")) {
			if (raw_fb) {
				snprintf(buf, bufn, "aro=%s:rawfb:%p",
				    p, raw_fb_addr);
			} else if (! dpy) {
				snprintf(buf, bufn, "aro=%s:%s", p,
				    "no-display");
			} else {
				char *d;
				d = DisplayString(dpy);
				if (! d) d = "unknown";
				if (*d == ':') {
					snprintf(buf, bufn, "aro=%s:%s%s", p,
					    this_host(), d);
				} else {
					snprintf(buf, bufn, "aro=%s:%s", p, d);
				}
			}
		} else if (!strcmp(p, "vncdisplay")) {
			snprintf(buf, bufn, "aro=%s:%s", p,
			    NONUL(vnc_desktop_name));
		} else if (!strcmp(p, "desktopname")) {
			snprintf(buf, bufn, "aro=%s:%s", p,
			    NONUL(rfb_desktop_name));
		} else if (!strcmp(p, "http_url")) {
			if (screen->httpListenSock > -1) {
				snprintf(buf, bufn, "aro=%s:http://%s:%d", p,
				    NONUL(screen->thisHost), screen->httpPort);
			} else {
				snprintf(buf, bufn, "aro=%s:%s", p,
				    "http_not_active");
			}
		} else if (!strcmp(p, "auth")) {
			snprintf(buf, bufn, "aro=%s:%s", p, NONUL(auth_file));
		} else if (!strcmp(p, "users")) {
			snprintf(buf, bufn, "aro=%s:%s", p, NONUL(users_list));
		} else if (!strcmp(p, "rootshift")) {
			snprintf(buf, bufn, "aro=%s:%d", p, rootshift);
		} else if (!strcmp(p, "clipshift")) {
			snprintf(buf, bufn, "aro=%s:%d", p, clipshift);
		} else if (!strcmp(p, "scale_str")) {
			snprintf(buf, bufn, "aro=%s:%s", p, NONUL(scale_str));
		} else if (!strcmp(p, "scaled_x")) {
			snprintf(buf, bufn, "aro=%s:%d", p, scaled_x);
		} else if (!strcmp(p, "scaled_y")) {
			snprintf(buf, bufn, "aro=%s:%d", p, scaled_y);
		} else if (!strcmp(p, "scale_numer")) {
			snprintf(buf, bufn, "aro=%s:%d", p, scale_numer);
		} else if (!strcmp(p, "scale_denom")) {
			snprintf(buf, bufn, "aro=%s:%d", p, scale_denom);
		} else if (!strcmp(p, "scale_fac")) {
			snprintf(buf, bufn, "aro=%s:%f", p, scale_fac);
		} else if (!strcmp(p, "scaling_blend")) {
			snprintf(buf, bufn, "aro=%s:%d", p, scaling_blend);
		} else if (!strcmp(p, "scaling_nomult4")) {
			snprintf(buf, bufn, "aro=%s:%d", p, scaling_nomult4);
		} else if (!strcmp(p, "scaling_pad")) {
			snprintf(buf, bufn, "aro=%s:%d", p, scaling_pad);
		} else if (!strcmp(p, "scaling_interpolate")) {
			snprintf(buf, bufn, "aro=%s:%d", p,
			    scaling_interpolate);
		} else if (!strcmp(p, "inetd")) {
			snprintf(buf, bufn, "aro=%s:%d", p, inetd);
		} else if (!strcmp(p, "privremote")) {
			snprintf(buf, bufn, "aro=%s:%d", p, priv_remote);
		} else if (!strcmp(p, "unsafe")) {
			snprintf(buf, bufn, "aro=%s:%d", p, !safe_remote_only);
		} else if (!strcmp(p, "safer")) {
			snprintf(buf, bufn, "aro=%s:%d", p, more_safe);
		} else if (!strcmp(p, "nocmds")) {
			snprintf(buf, bufn, "aro=%s:%d", p, no_external_cmds);
		} else if (!strcmp(p, "passwdfile")) {
			snprintf(buf, bufn, "aro=%s:%s", p, NONUL(passwdfile));
		} else if (!strcmp(p, "using_shm")) {
			snprintf(buf, bufn, "aro=%s:%d", p, !using_shm);
		} else if (!strcmp(p, "logfile") || !strcmp(p, "o")) {
			snprintf(buf, bufn, "aro=%s:%s", p, NONUL(logfile));
		} else if (!strcmp(p, "flag")) {
			snprintf(buf, bufn, "aro=%s:%s", p, NONUL(flagfile));
		} else if (!strcmp(p, "rc")) {
			snprintf(buf, bufn, "aro=%s:%s", p, NONUL(rc_rcfile));
		} else if (!strcmp(p, "norc")) {
			snprintf(buf, bufn, "aro=%s:%d", p, rc_norc);
		} else if (!strcmp(p, "h") || !strcmp(p, "help") ||
		    !strcmp(p, "V") || !strcmp(p, "version") ||
		    !strcmp(p, "lastmod")) {
			snprintf(buf, bufn, "aro=%s:%s", p, NONUL(lastmod));
		} else if (!strcmp(p, "bg")) {
			snprintf(buf, bufn, "aro=%s:%d", p, opts_bg);
		} else if (!strcmp(p, "sigpipe")) {
			snprintf(buf, bufn, "aro=%s:%s", p, NONUL(sigpipe));
		} else if (!strcmp(p, "threads")) {
			snprintf(buf, bufn, "aro=%s:%d", p, use_threads);
		} else if (!strcmp(p, "readrate")) {
			snprintf(buf, bufn, "aro=%s:%d", p, get_read_rate());
		} else if (!strcmp(p, "netrate")) {
			snprintf(buf, bufn, "aro=%s:%d", p, get_net_rate());
		} else if (!strcmp(p, "netlatency")) {
			snprintf(buf, bufn, "aro=%s:%d", p, get_net_latency());
		} else if (!strcmp(p, "pipeinput")) {
			snprintf(buf, bufn, "aro=%s:%s", p,
			    NONUL(pipeinput_str));
		} else if (!strcmp(p, "clients")) {
			char *str = list_clients();
			snprintf(buf, bufn, "aro=%s:%s", p, str);
			free(str);
		} else if (!strcmp(p, "client_count")) {
			snprintf(buf, bufn, "aro=%s:%d", p, client_count);
		} else if (!strcmp(p, "pid")) {
			snprintf(buf, bufn, "aro=%s:%d", p, (int) getpid());
		} else if (!strcmp(p, "ext_xtest")) {
			snprintf(buf, bufn, "aro=%s:%d", p, xtest_present);
		} else if (!strcmp(p, "ext_xtrap")) {
			snprintf(buf, bufn, "aro=%s:%d", p, xtrap_present);
		} else if (!strcmp(p, "ext_xrecord")) {
			snprintf(buf, bufn, "aro=%s:%d", p, xrecord_present);
		} else if (!strcmp(p, "ext_xkb")) {
			snprintf(buf, bufn, "aro=%s:%d", p, xkb_present);
		} else if (!strcmp(p, "ext_xshm")) {
			snprintf(buf, bufn, "aro=%s:%d", p, xshm_present);
		} else if (!strcmp(p, "ext_xinerama")) {
			snprintf(buf, bufn, "aro=%s:%d", p, xinerama_present);
		} else if (!strcmp(p, "ext_overlay")) {
			snprintf(buf, bufn, "aro=%s:%d", p, overlay_present);
		} else if (!strcmp(p, "ext_xfixes")) {
			snprintf(buf, bufn, "aro=%s:%d", p, xfixes_present);
		} else if (!strcmp(p, "ext_xdamage")) {
			snprintf(buf, bufn, "aro=%s:%d", p, xdamage_present);
		} else if (!strcmp(p, "ext_xrandr")) {
			snprintf(buf, bufn, "aro=%s:%d", p, xrandr_present);
		} else if (!strcmp(p, "rootwin")) {
			snprintf(buf, bufn, "aro=%s:0x%x", p,
			    (unsigned int) rootwin);
		} else if (!strcmp(p, "num_buttons")) {
			snprintf(buf, bufn, "aro=%s:%d", p, num_buttons);
		} else if (!strcmp(p, "button_mask")) {
			snprintf(buf, bufn, "aro=%s:%d", p, button_mask);
		} else if (!strcmp(p, "mouse_x")) {
			snprintf(buf, bufn, "aro=%s:%d", p, cursor_x);
		} else if (!strcmp(p, "mouse_y")) {
			snprintf(buf, bufn, "aro=%s:%d", p, cursor_y);
		} else if (!strcmp(p, "bpp")) {
			snprintf(buf, bufn, "aro=%s:%d", p, bpp);
		} else if (!strcmp(p, "depth")) {
			snprintf(buf, bufn, "aro=%s:%d", p, depth);
		} else if (!strcmp(p, "indexed_color")) {
			snprintf(buf, bufn, "aro=%s:%d", p, indexed_color);
		} else if (!strcmp(p, "dpy_x")) {
			snprintf(buf, bufn, "aro=%s:%d", p, dpy_x);
		} else if (!strcmp(p, "dpy_y")) {
			snprintf(buf, bufn, "aro=%s:%d", p, dpy_y);
		} else if (!strcmp(p, "wdpy_x")) {
			snprintf(buf, bufn, "aro=%s:%d", p, wdpy_x);
		} else if (!strcmp(p, "wdpy_y")) {
			snprintf(buf, bufn, "aro=%s:%d", p, wdpy_y);
		} else if (!strcmp(p, "off_x")) {
			snprintf(buf, bufn, "aro=%s:%d", p, off_x);
		} else if (!strcmp(p, "off_y")) {
			snprintf(buf, bufn, "aro=%s:%d", p, off_y);
		} else if (!strcmp(p, "cdpy_x")) {
			snprintf(buf, bufn, "aro=%s:%d", p, cdpy_x);
		} else if (!strcmp(p, "cdpy_y")) {
			snprintf(buf, bufn, "aro=%s:%d", p, cdpy_y);
		} else if (!strcmp(p, "coff_x")) {
			snprintf(buf, bufn, "aro=%s:%d", p, coff_x);
		} else if (!strcmp(p, "coff_y")) {
			snprintf(buf, bufn, "aro=%s:%d", p, coff_y);
		} else if (!strcmp(p, "rfbauth")) {
			NOTAPPRO
		} else if (!strcmp(p, "passwd")) {
			NOTAPPRO
		} else {
			NOTAPP
		}
		goto qry;
	} else {
		char tmp[100];
		NOTAPP
		rfbLog("remote_cmd: warning unknown\n");
		strncpy(tmp, p, 90);
		rfbLog("command \"%s\"\n", tmp);
		goto done;
	}

	done:

	if (*buf == '\0') {
		sprintf(buf, "ack=1");
	}

	qry:

	if (stringonly) {
		return strdup(buf);
	} else if (client_connect_file) {
		FILE *out = fopen(client_connect_file, "w");
		if (out != NULL) {
			fprintf(out, "%s\n", buf);
			fclose(out);
			usleep(20*1000);
		}
	} else {
		if (dpy) {	/* raw_fb hack */
			set_vnc_connect_prop(buf);
			XFlush(dpy);
		}
	}
#endif
	return NULL;
}

/* -- xdamage.c -- */

sraRegionPtr *xdamage_regions = NULL;
int xdamage_ticker = 0;

/* for stats */
int XD_skip = 0, XD_tot = 0, XD_des = 0;

void record_desired_xdamage_rect(int x, int y, int w, int h) {
	/*
	 * Unfortunately we currently can't trust an xdamage event
	 * to correspond to real screen damage.  E.g. focus-in for
	 * mozilla (depending on wm) will mark the whole toplevel
	 * area as damaged, when only the border has changed.
	 * Similar things for terminal windows.
	 *
	 * This routine uses some heuristics to detect small enough
	 * damage regions that we will not have a performance problem
	 * if we believe them even though they are wrong.  We record
	 * the corresponding tiles the damage regions touch.
	 */
	int dt_x, dt_y, nt_x1, nt_y1, nt_x2, nt_y2, nt;
	int ix, iy, cnt = 0;
	int area = w*h, always_accept = 0;
	/*
	 * XXX: not working yet, slow and overlaps with scan_display()
	 * probably slow because tall skinny rectangles very inefficient
	 * in general and in direct_fb_copy() (100X slower then horizontal).
	 */
	int use_direct_fb_copy = 0;
	int wh_min, wh_max;
	static int first = 1, udfb = 0;
	if (first) {
		if (getenv("XD_DFC")) {
			udfb = 1;
		}
		first = 0;
	}
	if (udfb) {
		use_direct_fb_copy = 1;
	}

	if (xdamage_max_area <= 0) {
		always_accept = 1;
	}

	if (!always_accept && area > xdamage_max_area) {
		return;
	}

	dt_x = w / tile_x;
	dt_y = h / tile_y;

	if (w < h) {
		wh_min = w;
		wh_max = h;
	} else {
		wh_min = h;
		wh_max = w;
	}

	if (!always_accept && dt_y >= 3 && area > 4000)  {
		/*
		 * if it is real it should be caught by a normal scanline
		 * poll, but we might as well keep if small (tall line?).
		 */
		return;
	}
	
	if (use_direct_fb_copy) {
		X_UNLOCK;
		direct_fb_copy(x, y, x + w, y + h, 1);
		xdamage_direct_count++;
		X_LOCK;
	} else if (0 && wh_min < tile_x/4 && wh_max > 30 * wh_min) {
		/* try it for long, skinny rects, XXX still no good */
		X_UNLOCK;
		direct_fb_copy(x, y, x + w, y + h, 1);
		xdamage_direct_count++;
		X_LOCK;
	} else {
		nt_x1 = nfix(  (x)/tile_x, ntiles_x);
		nt_x2 = nfix((x+w)/tile_x, ntiles_x);
		nt_y1 = nfix(  (y)/tile_y, ntiles_y);
		nt_y2 = nfix((y+h)/tile_y, ntiles_y);

		/*
		 * loop over the rectangle of tiles (1 tile for a small
		 * input rect).
		 */
		for (ix = nt_x1; ix <= nt_x2; ix++) {
			for (iy = nt_y1; iy <= nt_y2; iy++) {
				nt = ix + iy * ntiles_x;
				cnt++;
				if (! tile_has_xdamage_diff[nt]) {
					XD_des++;
					tile_has_xdamage_diff[nt] = 1;
				}
				/* not used: */
				tile_row_has_xdamage_diff[iy] = 1;
				xdamage_tile_count++;
			}
		}
	}
	if (debug_xdamage > 1) {
		fprintf(stderr, "xdamage: desired: %dx%d+%d+%d\tA: %6d  tiles="
		    "%02d-%02d/%02d-%02d  tilecnt: %d\n", w, h, x, y,
		    w * h, nt_x1, nt_x2, nt_y1, nt_y2, cnt);
	}
}

void add_region_xdamage(sraRegionPtr new_region) {
	sraRegionPtr reg;
	int prev_tick, nreg;

	if (! xdamage_regions) {
		return;
	}

	nreg = (xdamage_memory * NSCAN) + 1;
	prev_tick = xdamage_ticker - 1;
	if (prev_tick < 0) {
		prev_tick = nreg - 1;
	}

	reg = xdamage_regions[prev_tick];  
	if (reg != NULL) {
if (0) fprintf(stderr, "add_region_xdamage: prev_tick: %d reg %p\n", prev_tick, reg);
		sraRgnOr(reg, new_region);
	}
}

void clear_xdamage_mark_region(sraRegionPtr markregion, int flush) {
#if LIBVNCSERVER_HAVE_LIBXDAMAGE
	XEvent ev;
	sraRegionPtr tmpregion;
	int count = 0;

	if (! xdamage_present || ! use_xdamage) {
		return;
	}
	if (! xdamage) {
		return;
	}
	if (! xdamage_base_event_type) {
		return;
	}

	X_LOCK;
	if (flush) {
		XFlush(dpy);
	}
	while (XCheckTypedEvent(dpy, xdamage_base_event_type+XDamageNotify, &ev)) {
		count++;
	}
	/* clear the whole damage region */
	XDamageSubtract(dpy, xdamage, None, None);
	X_UNLOCK;

	if (debug_tiles || debug_xdamage) {
		fprintf(stderr, "clear_xdamage_mark_region: %d\n", count);
	}

	if (! markregion) {
		/* NULL means mark the whole display */
		tmpregion = sraRgnCreateRect(0, 0, dpy_x, dpy_y);
		add_region_xdamage(tmpregion);
		sraRgnDestroy(tmpregion);
	} else {
		add_region_xdamage(markregion);
	}
#endif
}

int collect_xdamage(int scancnt, int call) {
#if LIBVNCSERVER_HAVE_LIBXDAMAGE
	XDamageNotifyEvent *dev;
	XEvent ev;
	sraRegionPtr tmpregion;
	sraRegionPtr reg;
	static int rect_count = 0;
	int nreg, ccount = 0, dcount = 0, ecount = 0;
	static time_t last_rpt = 0;
	time_t now;
	int x, y, w, h, x2, y2;
	int i, dup, next, dup_max = 0;
#define DUPSZ 32
	int dup_x[DUPSZ], dup_y[DUPSZ], dup_w[DUPSZ], dup_h[DUPSZ];
	double tm, dt;

	if (! xdamage_present || ! use_xdamage) {
		return 0;
	}
	if (! xdamage) {
		return 0;
	}
	if (! xdamage_base_event_type) {
		return 0;
	}

	dtime0(&tm);

	nreg = (xdamage_memory * NSCAN) + 1;

	if (call == 0) {
		xdamage_ticker = (xdamage_ticker+1) % nreg;
		xdamage_direct_count = 0;
		reg = xdamage_regions[xdamage_ticker];  
		sraRgnMakeEmpty(reg);
	} else {
		reg = xdamage_regions[xdamage_ticker];  
	}


	X_LOCK;
if (0)	XFlush(dpy);
if (0)	XEventsQueued(dpy, QueuedAfterFlush);
	while (XCheckTypedEvent(dpy, xdamage_base_event_type+XDamageNotify, &ev)) {
		/*
		 * TODO max cut off time in this loop?
		 * Could check QLength and if huge just mark the whole
		 * screen.
		 */
		ecount++;
		if (ev.type != xdamage_base_event_type + XDamageNotify) {
			break;
		}
		dev = (XDamageNotifyEvent *) &ev;
		if (dev->damage != xdamage) {
			continue;	/* not ours! */
		}

		x = dev->area.x;
		y = dev->area.y;
		w = dev->area.width;
		h = dev->area.height;

		/*
		 * we try to manually remove some duplicates because
		 * certain activities can lead to many 10's of dups
		 * in a row.  The region work can be costly and reg is
		 * later used in xdamage_hint_skip loops, so it is good
		 * to skip them if possible.
		 */
		dup = 0;
		for (i=0; i < dup_max; i++) {
			if (dup_x[i] == x && dup_y[i] == y && dup_w[i] == w &&
			    dup_h[i] == h) {
				dup = 1;
				break;
			}
		}
		if (dup) {
			dcount++;
			continue;
		}
		if (dup_max < DUPSZ) {
			next = dup_max;
			dup_max++;
		} else {
			next = (next+1) % DUPSZ;
		}
		dup_x[next] = x;
		dup_y[next] = y;
		dup_w[next] = w;
		dup_h[next] = h;

		/* translate if needed */
		if (clipshift) {
			/* set coords relative to fb origin */
			if (0 && rootshift) {
				/*
				 * Note: not needed because damage is
				 * relative to subwin, not rootwin.
				 */
				x = x - off_x;
				y = y - off_y;
			}
			if (clipshift) {
				x = x - coff_x;
				y = y - coff_y;
			}

			x2 = x + w;		/* upper point */
			x  = nfix(x,  dpy_x);	/* place both in fb area */
			x2 = nfix(x2, dpy_x+1);
			w = x2 - x;		/* recompute w */
			
			y2 = y + h;
			y  = nfix(y,  dpy_y);
			y2 = nfix(y2, dpy_y+1);
			h = y2 - y;

			if (w <= 0 || h <= 0) {
				continue;
			}
		}
		if (debug_xdamage > 2) {
			fprintf(stderr, "xdamage: -> event %dx%d+%d+%d area:"
			    " %d  dups: %d  %s\n", w, h, x, y, w*h, dcount,
			    (w*h > xdamage_max_area) ? "TOO_BIG" : "");
		}

		record_desired_xdamage_rect(x, y, w, h);

		tmpregion = sraRgnCreateRect(x, y, x + w, y + h); 
		sraRgnOr(reg, tmpregion);
		sraRgnDestroy(tmpregion);
		rect_count++;
		ccount++;
	}
	/* clear the whole damage region for next time. XXX check */
	if (call == 1) {
		XDamageSubtract(dpy, xdamage, None, None);
	}
	X_UNLOCK;

	if (0 && xdamage_direct_count) {
		fb_push();
	}

	dt = dtime(&tm);
	if ((debug_tiles > 1 && ecount) || (debug_tiles && ecount > 200)
	    || debug_xdamage > 1) {
		fprintf(stderr, "collect_xdamage(%d): %.4f t: %.4f ev/dup/accept"
		    "/direct %d/%d/%d/%d\n", call, dt, tm - x11vnc_start, ecount,
		    dcount, ccount, xdamage_direct_count); 
	}
	now = time(0);
	if (! last_rpt) {
		last_rpt = now;
	}
	if (now > last_rpt + 15) {
		double rat = -1.0;

		if (XD_tot) {
			rat = ((double) XD_skip)/XD_tot;
		}
		if (debug_tiles || debug_xdamage) {
			fprintf(stderr, "xdamage: == scanline skip/tot: "
			    "%04d/%04d =%.3f  rects: %d  desired: %d\n",
			    XD_skip, XD_tot, rat, rect_count, XD_des);
		}
			
		XD_skip = 0;
		XD_tot  = 0;
		XD_des  = 0;
		rect_count = 0;
		last_rpt = now;
	}
#endif
	return 0;
}

int xdamage_hint_skip(int y) {
	static sraRegionPtr scanline = NULL;
	sraRegionPtr reg, tmpl;
	int ret, i, n, nreg;

	if (! xdamage_present || ! use_xdamage) {
		return 0;	/* cannot skip */
	}
	if (! xdamage_regions) {
		return 0;	/* cannot skip */
	}

	if (! scanline) {
		/* keep it around to avoid malloc etc, recreate */
		scanline = sraRgnCreate();
	}

	tmpl = sraRgnCreateRect(0, y, dpy_x, y+1);

	nreg = (xdamage_memory * NSCAN) + 1;
	ret = 1;
	for (i=0; i<nreg; i++) {
		/* go back thru the history starting at most recent */
		n = (xdamage_ticker + nreg - i) % nreg;
		reg = xdamage_regions[n];  
		if (sraRgnEmpty(reg)) {
			/* checking for emptiness is very fast */
			continue;
		}
		sraRgnMakeEmpty(scanline);
		sraRgnOr(scanline, tmpl);
		if (sraRgnAnd(scanline, reg)) {
			ret = 0;
			break;
		}
	}
	sraRgnDestroy(tmpl);

	return ret;
}

void initialize_xdamage(void) {
	sraRegionPtr *ptr;
	int i, nreg;

	if (! xdamage_present) {
		use_xdamage = 0;
	}
	if (xdamage_regions)  {
		ptr = xdamage_regions;
		while (*ptr != NULL) {
			sraRgnDestroy(*ptr);
			ptr++;
		}
		free(xdamage_regions);
		xdamage_regions = NULL;
	}
	if (use_xdamage) {
		nreg = (xdamage_memory * NSCAN) + 2;
		xdamage_regions = (sraRegionPtr *)
		    malloc(nreg * sizeof(sraRegionPtr));
		for (i = 0; i < nreg; i++) {
			ptr = xdamage_regions+i;
			if (i == nreg - 1) {
				*ptr = NULL;
			} else {
				*ptr = sraRgnCreate();
				sraRgnMakeEmpty(*ptr);
			}
		}
		/* set so will be 0 in first collect_xdamage call */
		xdamage_ticker = -1;
	}
}

void create_xdamage_if_needed(void) {

	if (raw_fb && ! dpy) return;	/* raw_fb hack */

#if LIBVNCSERVER_HAVE_LIBXDAMAGE
	if (! xdamage) {
		X_LOCK;
		xdamage = XDamageCreate(dpy, window, XDamageReportRawRectangles); 
		XDamageSubtract(dpy, xdamage, None, None);
		X_UNLOCK;
		rfbLog("created xdamage object: 0x%lx\n", xdamage);
	}
#endif
}

void destroy_xdamage_if_needed(void) {

	if (raw_fb && ! dpy) return;	/* raw_fb hack */

#if LIBVNCSERVER_HAVE_LIBXDAMAGE
	if (xdamage) {
		XEvent ev;
		X_LOCK;
		XDamageDestroy(dpy, xdamage);
		XFlush(dpy);
		if (xdamage_base_event_type) {
			while (XCheckTypedEvent(dpy,
			    xdamage_base_event_type+XDamageNotify, &ev)) {
				;
			}
		}
		X_UNLOCK;
		rfbLog("destroyed xdamage object: 0x%lx\n", xdamage);
		xdamage = 0;
	}
#endif
}

void check_xdamage_state(void) {
	if (! xdamage_present) {
		return;
	}
	/*
	 * Create or destroy the Damage object as needed, we don't want
	 * one if no clients are connected.
	 */
	if (client_count && use_xdamage) {
		create_xdamage_if_needed();
		if (xdamage_scheduled_mark > 0.0 && dnow() >
		    xdamage_scheduled_mark) {
			if (xdamage_scheduled_mark_region) {
				mark_region_for_xdamage(
				    xdamage_scheduled_mark_region);
				sraRgnDestroy(xdamage_scheduled_mark_region);
				xdamage_scheduled_mark_region = NULL;
			} else {
				mark_for_xdamage(0, 0, dpy_x, dpy_y);
			}
			xdamage_scheduled_mark = 0.0;
		}
	} else {
		destroy_xdamage_if_needed();
	}
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
	rfbCursorPtr rfb;
} cursor_info_t;

void curs_copy(cursor_info_t *dest, cursor_info_t *src) {
	if (src->data != NULL) {
		dest->data = strdup(src->data);
	} else {
		dest->data = NULL;
	}
	if (src->mask != NULL) {
		dest->mask = strdup(src->mask);
	} else {
		dest->mask = NULL;
	}
	dest->wx = src->wx;
	dest->wy = src->wy;
	dest->sx = src->sx;
	dest->sy = src->sy;
	dest->reverse = src->reverse;
	dest->rfb = src->rfb;
}

/* empty cursor */
static char* curs_empty_data =
"  "
"  ";

static char* curs_empty_mask =
"  "
"  ";
static cursor_info_t cur_empty = {NULL, NULL, 2, 2, 0, 0, 0, NULL};

/* dot cursor */
static char* curs_dot_data =
"  "
" x";

static char* curs_dot_mask =
"  "
" x";
static cursor_info_t cur_dot = {NULL, NULL, 2, 2, 0, 0, 0, NULL};


/* main cursor */
static char* curs_arrow_data =
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

static char* curs_arrow_mask =
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
static cursor_info_t cur_arrow = {NULL, NULL, 18, 18, 0, 0, 1, NULL};

static char* curs_arrow2_data =
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

static char* curs_arrow2_mask =
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
static cursor_info_t cur_arrow2 = {NULL, NULL, 18, 18, 0, 0, 0, NULL};

static char* curs_arrow3_data = 
"                "
" xx             "
" xxxx           "
"  xxxxx         "
"  xxxxxxx       "
"   xxxxxxxx     "
"   xxxxxxxxxx   "
"    xxxxx       "
"    xxxxx       "
"     xx  x      "
"     xx   x     "
"      x    x    "
"      x     x   "
"             x  "
"              x "
"                ";

static char* curs_arrow3_mask = 
"xxx             "
"xxxxx           "
"xxxxxxx         "
" xxxxxxxx       "
" xxxxxxxxxx     "
"  xxxxxxxxxxxx  "
"  xxxxxxxxxxxx  "
"   xxxxxxxxxxx  "
"   xxxxxxx      "
"    xxxxxxx     "
"    xxxx xxx    "
"     xxx  xxx   "
"     xxx   xxx  "
"     xxx    xxx "
"             xxx"
"              xx";

static cursor_info_t cur_arrow3 = {NULL, NULL, 16, 16, 0, 0, 1, NULL};

static char* curs_arrow4_data = 
"                "
" xx             "
" xxxx           "
"  xxxxx         "
"  xxxxxxx       "
"   xxxxxxxx     "
"   xxxxxxxxxx   "
"    xxxxx       "
"    xxxxx       "
"     xx  x      "
"     xx   x     "
"      x    x    "
"      x     x   "
"             x  "
"              x "
"                ";

static char* curs_arrow4_mask = 
"xxx             "
"xxxxx           "
"xxxxxxx         "
" xxxxxxxx       "
" xxxxxxxxxx     "
"  xxxxxxxxxxxx  "
"  xxxxxxxxxxxx  "
"   xxxxxxxxxxx  "
"   xxxxxxx      "
"    xxxxxxx     "
"    xxxx xxx    "
"     xxx  xxx   "
"     xxx   xxx  "
"     xxx    xxx "
"             xxx"
"              xx";

static cursor_info_t cur_arrow4 = {NULL, NULL, 16, 16, 0, 0, 0, NULL};

static char* curs_arrow5_data = 
"x              "
" xx            "
" xxxx          "
"  xxxxx        "
"  xxxxxxx      "
"   xxx         "
"   xx x        "
"    x  x       "
"    x   x      "
"         x     "
"          x    "
"           x   "
"            x  "
"             x "
"              x";

static char* curs_arrow5_mask = 
"xx             "
"xxxx           "
" xxxxx         "
" xxxxxxx       "
"  xxxxxxxx     "
"  xxxxxxxx     "
"   xxxxx       "
"   xxxxxx      "
"    xx xxx     "
"     x  xxx    "
"         xxx   "
"          xxx  "
"           xxx "
"            xxx"
"             xx";

static cursor_info_t cur_arrow5 = {NULL, NULL, 15, 15, 0, 0, 1, NULL};

static char* curs_arrow6_data = 
"x              "
" xx            "
" xxxx          "
"  xxxxx        "
"  xxxxxxx      "
"   xxx         "
"   xx x        "
"    x  x       "
"    x   x      "
"         x     "
"          x    "
"           x   "
"            x  "
"             x "
"              x";

static char* curs_arrow6_mask = 
"xx             "
"xxxx           "
" xxxxx         "
" xxxxxxx       "
"  xxxxxxxx     "
"  xxxxxxxx     "
"   xxxxx       "
"   xxxxxx      "
"    xx xxx     "
"     x  xxx    "
"         xxx   "
"          xxx  "
"           xxx "
"            xxx"
"             xx";

static cursor_info_t cur_arrow6 = {NULL, NULL, 15, 15, 0, 0, 0, NULL};

int alt_arrow_max = 6;
/*
 * It turns out we can at least detect mouse is on the root window so 
 * show it (under -cursor X) with this familiar cursor... 
 */
static char* curs_root_data =
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

static char* curs_root_mask =
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
static cursor_info_t cur_root = {NULL, NULL, 18, 18, 8, 8, 1, NULL};

static char* curs_fleur_data = 
"                "
"       xx       "
"      xxxx      "
"     xxxxxx     "
"       xx       "
"   x   xx   x   "
"  xx   xx   xx  "
" xxxxxxxxxxxxxx "
" xxxxxxxxxxxxxx "
"  xx   xx   xx  "
"   x   xx   x   "
"       xx       "
"     xxxxxx     "
"      xxxx      "
"       xx       "
"                ";

static char* curs_fleur_mask = 
"      xxxx      "
"      xxxxx     "
"     xxxxxx     "
"    xxxxxxxx    "
"   x xxxxxx x   "
"  xxx xxxx xxx  "
"xxxxxxxxxxxxxxxx"
"xxxxxxxxxxxxxxxx"
"xxxxxxxxxxxxxxxx"
"xxxxxxxxxxxxxxxx"
"  xxx xxxx xxx  "
"   x xxxxxx x   "
"    xxxxxxxx    "
"     xxxxxx     "
"      xxxx      "
"      xxxx      ";

static cursor_info_t cur_fleur = {NULL, NULL, 16, 16, 8, 8, 1, NULL};

static char* curs_plus_data = 
"            "
"     xx     "
"     xx     "
"     xx     "
"     xx     "
" xxxxxxxxxx "
" xxxxxxxxxx "
"     xx     "
"     xx     "
"     xx     "
"     xx     "
"            ";

static char* curs_plus_mask = 
"    xxxx    "
"    xxxx    "
"    xxxx    "
"    xxxx    "
"xxxxxxxxxxxx"
"xxxxxxxxxxxx"
"xxxxxxxxxxxx"
"xxxxxxxxxxxx"
"    xxxx    "
"    xxxx    "
"    xxxx    "
"    xxxx    ";
static cursor_info_t cur_plus = {NULL, NULL, 12, 12, 5, 6, 1, NULL};

static char* curs_xterm_data = 
"                "
"     xxx xxx    "
"       xxx      "
"        x       "
"        x       "
"        x       "
"        x       "
"        x       "
"        x       "
"        x       "
"        x       "
"        x       "
"        x       "
"       xxx      "
"     xxx xxx    "
"                ";

static char* curs_xterm_mask = 
"    xxxx xxxx   "
"    xxxxxxxxx   "
"    xxxxxxxxx   "
"      xxxxx     "
"       xxx      "
"       xxx      "
"       xxx      "
"       xxx      "
"       xxx      "
"       xxx      "
"       xxx      "
"       xxx      "
"      xxxxx     "
"    xxxxxxxxx   "
"    xxxxxxxxx   "
"    xxxx xxxx   ";
static cursor_info_t cur_xterm = {NULL, NULL, 16, 16, 8, 8, 1, NULL};

enum cursor_names {
	CURS_EMPTY = 0,
	CURS_DOT,

	CURS_ARROW,
	CURS_ROOT,
	CURS_WM,
	CURS_TERM,
	CURS_PLUS,

	CURS_DYN1,
	CURS_DYN2,
	CURS_DYN3,
	CURS_DYN4,
	CURS_DYN5,
	CURS_DYN6,
	CURS_DYN7,
	CURS_DYN8,
	CURS_DYN9,
	CURS_DYN10,
	CURS_DYN11,
	CURS_DYN12,
	CURS_DYN13,
	CURS_DYN14,
	CURS_DYN15,
	CURS_DYN16
};

#define CURS_DYN_MIN CURS_DYN1
#define CURS_DYN_MAX CURS_DYN16
#define CURS_DYN_NUM (CURS_DYN_MAX - CURS_DYN_MIN + 1)

#define CURS_MAX 32
static cursor_info_t *cursors[CURS_MAX];

void setup_cursors_and_push(void) {
	setup_cursors();
	first_cursor();
}

void first_cursor(void) {
	if (! screen) {
		return;
	}
	if (! show_cursor) {
		screen->cursor = NULL;
	} else {
		got_xfixes_cursor_notify++;
		set_rfb_cursor(get_which_cursor());
		set_cursor_was_changed(screen);
	}
}

void setup_cursors(void) {
	rfbCursorPtr rfb_curs;
	char *scale = NULL;
	int i, j, n = 0;
	static int first = 1;

	rfbLog("setting up %d cursors...\n", CURS_MAX);

	if (first) {
		for (i=0; i<CURS_MAX; i++) {
			cursors[i] = NULL;
		}
	}
	first = 0;

	if (screen) {
		screen->cursor = NULL;
		LOCK(screen->cursorMutex);
	}

	for (i=0; i<CURS_MAX; i++) {
		cursor_info_t *ci;
		if (cursors[i]) {
			/* clear out any existing ones: */
			ci = cursors[i];
			if (ci->rfb) {
				/* this is the rfbCursor part: */
				if (ci->rfb->richSource) {
					free(ci->rfb->richSource);
				}
				if (ci->rfb->source) {
					free(ci->rfb->source);
				}
				if (ci->rfb->mask) {
					free(ci->rfb->mask);
				}
				free(ci->rfb);
			}
			if (ci->data) {
				free(ci->data);
			}
			if (ci->mask) {
				free(ci->mask);
			}
			free(ci);
		}

		/* create new struct: */
		ci = (cursor_info_t *) malloc(sizeof(cursor_info_t));
		ci->data = NULL; 
		ci->mask = NULL; 
		ci->wx = 0;
		ci->wy = 0;
		ci->sx = 0;
		ci->sy = 0;
		ci->reverse = 0;
		ci->rfb = NULL;
		cursors[i] = ci;
	}

	/* clear any xfixes cursor cache (no freeing is done) */
	get_xfixes_cursor(1);

	/* manually fill in the data+masks: */
	cur_empty.data	= curs_empty_data;
	cur_empty.mask	= curs_empty_mask;

	cur_dot.data	= curs_dot_data;
	cur_dot.mask	= curs_dot_mask;

	cur_arrow.data	= curs_arrow_data;
	cur_arrow.mask	= curs_arrow_mask;
	cur_arrow2.data	= curs_arrow2_data;
	cur_arrow2.mask	= curs_arrow2_mask;
	cur_arrow3.data	= curs_arrow3_data;
	cur_arrow3.mask	= curs_arrow3_mask;
	cur_arrow4.data	= curs_arrow4_data;
	cur_arrow4.mask	= curs_arrow4_mask;
	cur_arrow5.data	= curs_arrow5_data;
	cur_arrow5.mask	= curs_arrow5_mask;
	cur_arrow6.data	= curs_arrow6_data;
	cur_arrow6.mask	= curs_arrow6_mask;

	cur_root.data	= curs_root_data;
	cur_root.mask	= curs_root_mask;

	cur_plus.data	= curs_plus_data;
	cur_plus.mask	= curs_plus_mask;

	cur_fleur.data	= curs_fleur_data;
	cur_fleur.mask	= curs_fleur_mask;

	cur_xterm.data	= curs_xterm_data;
	cur_xterm.mask	= curs_xterm_mask;

	curs_copy(cursors[CURS_EMPTY], &cur_empty);	n++;
	curs_copy(cursors[CURS_DOT],   &cur_dot);	n++;

	if (alt_arrow < 1 || alt_arrow > alt_arrow_max) {
		alt_arrow = 1;
	}
	if (alt_arrow == 1) {
		curs_copy(cursors[CURS_ARROW], &cur_arrow);	n++;
	} else if (alt_arrow == 2) {
		curs_copy(cursors[CURS_ARROW], &cur_arrow2);	n++;
	} else if (alt_arrow == 3) {
		curs_copy(cursors[CURS_ARROW], &cur_arrow3);	n++;
	} else if (alt_arrow == 4) {
		curs_copy(cursors[CURS_ARROW], &cur_arrow4);	n++;
	} else if (alt_arrow == 5) {
		curs_copy(cursors[CURS_ARROW], &cur_arrow5);	n++;
	} else if (alt_arrow == 6) {
		curs_copy(cursors[CURS_ARROW], &cur_arrow6);	n++;
	} else {
		alt_arrow = 1;
		curs_copy(cursors[CURS_ARROW], &cur_arrow);	n++;
	}

	curs_copy(cursors[CURS_ROOT], &cur_root);	n++;
	curs_copy(cursors[CURS_WM],   &cur_fleur);	n++;
	curs_copy(cursors[CURS_TERM], &cur_xterm);	n++;
	curs_copy(cursors[CURS_PLUS], &cur_plus);	n++;

	if (scale_cursor_str) {
		scale = scale_cursor_str;
	} else if (scaling && scale_str) {
		scale = scale_str;
	}
	/* scale = NULL zeroes everything */
	parse_scale_string(scale, &scale_cursor_fac, &scaling_cursor,
	    &scaling_cursor_blend, &j, &j, &scaling_cursor_interpolate,
	    &scale_cursor_numer, &scale_cursor_denom);

	for (i=0; i<n; i++) {
		/* create rfbCursors for the special cursors: */

		cursor_info_t *ci = cursors[i];

		if (scaling_cursor && scale_cursor_fac != 1.0) {
			int w, h, x, y, i;
			unsigned long *pixels;

			w = ci->wx;
			h = ci->wy;

			pixels = (unsigned long *) malloc(4*w*h);

			i = 0;
			for (y=0; y<h; y++) {
				for (x=0; x<w; x++) {
					char d = ci->data[i];
					char m = ci->mask[i];
					unsigned long *p;

					p = pixels + i;

					/* set alpha on */
					*p = 0xff000000;

					if (d == ' ' && m == ' ') {
						/* alpha off */
						*p = 0x00000000;
					} else if (d != ' ') {
						/* body */
						if (ci->reverse) {
							*p |= 0x00000000;
						} else {
							*p |= 0x00ffffff;
						}
					} else if (m != ' ') {
						/* edge */
						if (ci->reverse) {
							*p |= 0x00ffffff;
						} else {
							*p |= 0x00000000;
						}
					}
					i++;
				}
			}

			rfb_curs = pixels2curs(pixels, w, h, ci->sx, ci->sy,
			    bpp/8);

			free(pixels);

		} else {

			/* standard X cursor */
			rfb_curs = rfbMakeXCursor(ci->wx, ci->wy,
			    ci->data, ci->mask);

			if (ci->reverse) {
				rfb_curs->foreRed   = 0x0000;
				rfb_curs->foreGreen = 0x0000;
				rfb_curs->foreBlue  = 0x0000;
				rfb_curs->backRed   = 0xffff;
				rfb_curs->backGreen = 0xffff;
				rfb_curs->backBlue  = 0xffff;
			}
			rfb_curs->xhot = ci->sx;
			rfb_curs->yhot = ci->sy;
			rfb_curs->cleanup = FALSE;
			rfb_curs->cleanupSource = FALSE;
			rfb_curs->cleanupMask = FALSE;
			rfb_curs->cleanupRichSource = FALSE;

			if (bpp == 8 && indexed_color) {
				/*
				 * use richsource in PseudoColor for better
				 * looking cursors (i.e. two-color).
				 */
				int x, y, k = 0, bw;
				int black = 0, white = 1;
				char d, m;

				if (dpy) {	/* raw_fb hack */
					black = BlackPixel(dpy, scr);
					white = WhitePixel(dpy, scr);
				}

				rfb_curs->richSource
				    = (char *)calloc(ci->wx * ci->wy, 1);

				for (y = 0; y < ci->wy; y++) {
				    for (x = 0; x < ci->wx; x++) {
					d = *(ci->data + k);
					m = *(ci->mask + k);
					if (d == ' ' && m == ' ') {
						k++;
						continue;
					} else if (m != ' ' && d == ' ') {
						bw = black;
					} else {
						bw = white;
					}
					if (ci->reverse) {
						if (bw == black) {
							bw = white;
						} else {
							bw = black;
						}
					}
					*(rfb_curs->richSource+k) =
					    (unsigned char) bw;
					k++;
				    }
				}
			}
		}
		ci->rfb = rfb_curs;
	}
	if (screen) {
		UNLOCK(screen->cursorMutex);
	}
	rfbLog("  done.\n");
}

typedef struct win_str_info {
	char *wm_name;
	char *res_name;
	char *res_class;
} win_str_info_t;

/*
 * Descends window tree at pointer until the window cursor matches the current 
 * cursor.  So far only used to detect if mouse is on root background or not.
 * (returns 0 in that case, 1 otherwise).
 *
 */
void tree_descend_cursor(int *depth, Window *w, win_str_info_t *winfo) {
	Window r, c;
	int i, rx, ry, wx, wy;
	unsigned int mask;
	Window wins[10];
	int descend, maxtries = 10;
	char *name, *s = multiple_cursors_mode;
	static XClassHint *classhint = NULL;
	int nm_info = 1;
	XErrorHandler old_handler;

	X_LOCK;

	if (!strcmp(s, "default") || !strcmp(s, "X") || !strcmp(s, "arrow")) {
		nm_info = 0;
	}

	*(winfo->wm_name)   = '\0';
	*(winfo->res_name)  = '\0';
	*(winfo->res_class) = '\0';


	/* some times a window can go away before we get to it */
	trapped_xerror = 0;
	old_handler = XSetErrorHandler(trap_xerror);

	c = window;
	descend = -1;

	while (c) {
		wins[++descend] = c;
		if (descend >= maxtries - 1) {
			break;
		}
		if ( XTestCompareCurrentCursorWithWindow_wr(dpy, c) ) {
			break;
		}
		/* TBD: query_pointer() */
		XQueryPointer(dpy, c, &r, &c, &rx, &ry, &wx, &wy, &mask);
	}

	if (nm_info) {
		int got_wm_name = 0, got_res_name = 0, got_res_class = 0;

		if (! classhint) {
			classhint = XAllocClassHint();
		}

		for (i = descend; i >=0; i--) {
			c = wins[i];
			if (! c) {
				continue;
			}
			
			if (! got_wm_name && XFetchName(dpy, c, &name)) {
				if (name) {
					if (*name != '\0') {
						strcpy(winfo->wm_name, name);
						got_wm_name = 1;
					}
					XFree(name);
				}
			}
			if (classhint && (! got_res_name || ! got_res_class)) {
			    if (XGetClassHint(dpy, c, classhint)) {
				char *p;
				p = classhint->res_name;
				if (p) {
					if (*p != '\0' && ! got_res_name) {
						strcpy(winfo->res_name, p);
						got_res_name = 1;
					}
					XFree(p);
					classhint->res_name = NULL;
				}
				p = classhint->res_class;
				if (p) {
					if (*p != '\0' && ! got_res_class) {
						strcpy(winfo->res_class, p);
						got_res_class = 1;
					}
					XFree(p);
					classhint->res_class = NULL;
				}
			    }
			}
		}
	}

	XSetErrorHandler(old_handler);
	trapped_xerror = 0;

	X_UNLOCK;

	*depth = descend;
	*w = wins[descend];
}

void initialize_xfixes(void) {
#if LIBVNCSERVER_HAVE_LIBXFIXES
	if (xfixes_present) {
		X_LOCK;
		if (use_xfixes) {
			XFixesSelectCursorInput(dpy, rootwin,
				XFixesDisplayCursorNotifyMask);
		} else {
			XFixesSelectCursorInput(dpy, rootwin, 0);
		}
		X_UNLOCK;
	}
#endif
}

rfbCursorPtr pixels2curs(unsigned long *pixels, int w, int h,
    int xhot, int yhot, int Bpp) {
	rfbCursorPtr c;
	static unsigned long black = 0, white = 1;
	static int first = 1;
	char *bitmap, *rich, *alpha;
	char *new_pixels = NULL;
	int n_opaque, n_trans, n_alpha, len, histo[256];
	int send_alpha = 0, alpha_shift, thresh;
	int i, x, y;

	if (first && dpy) {	/* raw_fb hack */
		X_LOCK;
		black = BlackPixel(dpy, scr);
		white = WhitePixel(dpy, scr);
		X_UNLOCK;
		first = 0;
	}

	if (scaling_cursor && scale_cursor_fac != 1.0) {
		int W, H;

		W = w;
		H = h;

		w = scale_round(W, scale_cursor_fac);
		h = scale_round(H, scale_cursor_fac);

		new_pixels = (char *) malloc(4 * w * h);

		scale_rect(scale_cursor_fac, scaling_cursor_blend,
		    scaling_cursor_interpolate, 4,
		    (char *) pixels, 4*W, new_pixels, 4*w,
		    W, H, w, h,
		    0, 0, W, H, 0);

		pixels = (unsigned long *) new_pixels;

		xhot = scale_round(xhot, scale_cursor_fac);
		yhot = scale_round(yhot, scale_cursor_fac);
	}

	len = w * h;
	/* for bitmap data */
	bitmap = (char *)malloc(len+1);
	bitmap[len] = '\0';

	/* for rich cursor pixel data */
	rich  = (char *)calloc(Bpp*len, 1);
	alpha = (char *)calloc(1*len, 1);

	n_opaque = 0;
	n_trans = 0;
	n_alpha = 0;
	for (i=0; i<256; i++) {
		histo[i] = 0;
	}

	i = 0;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			unsigned long a;

			a = 0xff000000 & (*(pixels+i));
			a = a >> 24;	/* alpha channel */
			if (a > 0) {
				n_alpha++;
			}
			histo[a]++;
			if (a < alpha_threshold) {
				n_trans++;
			} else {
				n_opaque++;
			}
			i++;
		}
	}
	if (alpha_blend) {
		send_alpha = 0;
		if (Bpp == 4) {
			send_alpha = 1;
		}
		alpha_shift = 24;
		if (main_red_shift == 24 || main_green_shift == 24 ||
		    main_blue_shift == 24)  {
			alpha_shift = 0;	/* XXX correct? */
		}
	}
	if (n_opaque >= alpha_frac * n_alpha) {
		thresh = alpha_threshold;
	} else {
		n_opaque = 0;
		for (i=255; i>=0; i--) {
			n_opaque += histo[i];
			thresh = i;
			if (n_opaque >= alpha_frac * n_alpha) {
				break;
			}
		}
	}

	i = 0;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			unsigned long r, g, b, a;
			unsigned int ui;
			char *p;

			a = 0xff000000 & (*(pixels+i));
			a = a >> 24;	/* alpha channel */


			if (a < thresh) {
				bitmap[i] = ' ';
			} else {
				bitmap[i] = 'x';
			}

			r = 0x00ff0000 & (*(pixels+i));
			g = 0x0000ff00 & (*(pixels+i));
			b = 0x000000ff & (*(pixels+i));
			r = r >> 16;	/* red */
			g = g >> 8;	/* green */
			b = b >> 0;	/* blue */

			if (alpha_remove && a != 0) {
				r = (255 * r) / a;
				g = (255 * g) / a;
				b = (255 * b) / a;
				if (r > 255) r = 255;
				if (g > 255) g = 255;
				if (b > 255) b = 255;
			}

			if (indexed_color) {
				/*
				 * Choose black or white for
				 * PseudoColor case.
				 */
				int value = (r+g+b)/3;
				if (value > 127) {
					ui = white;
				} else {
					ui = black;
				}
			} else {
				/*
				 * Otherwise map the RGB data onto
				 * the framebuffer format:
				 */
				r = (main_red_max   * r)/255;
				g = (main_green_max * g)/255;
				b = (main_blue_max  * b)/255;
				ui = 0;
				ui |= (r << main_red_shift);
				ui |= (g << main_green_shift);
				ui |= (b << main_blue_shift);
				if (send_alpha) {
					ui |= (a << alpha_shift);
				}
			}

			/* insert value into rich source: */
			p = rich + Bpp*i;

			if (Bpp == 1) {
				*((unsigned char *)p)
				= (unsigned char) ui;
			} else if (Bpp == 2) {
				*((unsigned short *)p)
				= (unsigned short) ui;
			} else if (Bpp == 3) {
				*((unsigned char *)p)
				= (unsigned char) ((ui & 0x0000ff) >> 0);
				*((unsigned char *)(p+1))
				= (unsigned char) ((ui & 0x00ff00) >> 8);
				*((unsigned char *)(p+2))
				= (unsigned char) ((ui & 0xff0000) >> 16);
			} else if (Bpp == 4) {
				*((unsigned int *)p)
				= (unsigned int) ui;
			}

			/* insert alpha value into alpha source: */
			p = alpha + i;
			*((unsigned char *)p) = (unsigned char) a;

			i++;
		}
	}

	/* create the cursor with the bitmap: */
	c = rfbMakeXCursor(w, h, bitmap, bitmap);
	free(bitmap);

	if (new_pixels) {
		free(new_pixels);
	}

	/* set up the cursor parameters: */
	c->xhot = xhot;
	c->yhot = yhot;
	c->cleanup = FALSE;
	c->cleanupSource = FALSE;
	c->cleanupMask = FALSE;
	c->cleanupRichSource = FALSE;
	c->richSource = rich;

	if (alpha_blend && !indexed_color) {
		c->alphaSource = alpha;
		c->alphaPreMultiplied = TRUE;
	} else {
		free(alpha);
		c->alphaSource = NULL;
	}
	return c;
}

int get_xfixes_cursor(int init) {
	static unsigned long last_cursor = 0;
	static int last_index = 0;
	static time_t curs_times[CURS_MAX];
	static unsigned long curs_index[CURS_MAX];
	int which = CURS_ARROW;

	if (init) {
		/* zero out our cache (cursors are not freed) */
		int i;
		for (i=0; i<CURS_MAX; i++) {
			curs_times[i] = 0;
			curs_index[i] = 0;
		}
		last_cursor = 0;
		last_index = 0;
		return -1;
	}

	if (xfixes_present) {
#if LIBVNCSERVER_HAVE_LIBXFIXES
		int use, oldest, i;
		time_t oldtime, now;
		XFixesCursorImage *xfc;

		if (! got_xfixes_cursor_notify && xfixes_base_event_type) {
			/* try again for XFixesCursorNotify event */
			XEvent xev;
			X_LOCK;
			if (XCheckTypedEvent(dpy, xfixes_base_event_type +
			    XFixesCursorNotify, &xev)) {
				got_xfixes_cursor_notify++;
			}
			X_UNLOCK;
		}
		if (! got_xfixes_cursor_notify) {
			/* evidently no cursor change, just return last one */
			if (last_index) {
				return last_index;
			} else {
				return CURS_ARROW;
			}
		}
		got_xfixes_cursor_notify = 0;

		/* retrieve the cursor info + pixels from server: */
		X_LOCK;
		xfc = XFixesGetCursorImage(dpy);
		X_UNLOCK;
		if (! xfc) {
			/* failure. */
			return(which);
		}

		if (xfc->cursor_serial == last_cursor) {
			/* same serial index: no change */
			X_LOCK;
			XFree(xfc);
			X_UNLOCK;
			if (last_index) {
				return last_index;
			} else {
				return CURS_ARROW;
			}
		}

		oldest = CURS_DYN_MIN;
		oldtime = curs_times[oldest];
		now = time(0);
		for (i = CURS_DYN_MIN; i <= CURS_DYN_MAX; i++) {
			if (curs_times[i] < oldtime) {
				/* watch for oldest one to overwrite */
				oldest = i;
				oldtime = curs_times[i];
			}
			if (xfc->cursor_serial == curs_index[i]) {
				/*
				 * got a hit with an existing cursor,
				 * use that one.
				 */
				last_cursor = curs_index[i];
				curs_times[i] = now;
				last_index = i;
				X_LOCK;
				XFree(xfc);
				X_UNLOCK;
				return last_index;
			}
		}

		/* we need to create the cursor and overwrite oldest */
		use = oldest;
		if (cursors[use]->rfb) {
			/* clean up oldest if it exists */
			if (cursors[use]->rfb->richSource) {
				free(cursors[use]->rfb->richSource);
			}
			if (cursors[use]->rfb->alphaSource) {
				free(cursors[use]->rfb->alphaSource);
			}
			if (cursors[use]->rfb->source) {
				free(cursors[use]->rfb->source);
			}
			if (cursors[use]->rfb->mask) {
				free(cursors[use]->rfb->mask);
			}
			free(cursors[use]->rfb);
		}

		/* place cursor into our collection */
		cursors[use]->rfb = pixels2curs(xfc->pixels, xfc->width,
		    xfc->height, xfc->xhot, xfc->yhot, bpp/8);

		/* update time and serial index: */
		curs_times[use] = now;
		curs_index[use] = xfc->cursor_serial;
		last_index = use;
		last_cursor = xfc->cursor_serial;

		which = last_index;

		X_LOCK;
		XFree(xfc);
		X_UNLOCK;
#endif
	}
	return(which);
}

int known_cursors_mode(char *s) {
/*
 * default:	see initialize_cursors_mode() for default behavior.
 * arrow:	unchanging white arrow.
 * Xn*:		show X on root background.  Optional n sets treedepth.
 * some:	do the heuristics for root, wm, term detection.
 * most:	if display have overlay or xfixes, show all cursors,
 *		otherwise do the same as "some"
 * none:	show no cursor.
 */
	if (strcmp(s, "default") && strcmp(s, "arrow") && *s != 'X' &&
	    strcmp(s, "some") && strcmp(s, "most") && strcmp(s, "none")) {
		return 0;
	} else {
		return 1;
	}
}

void initialize_cursors_mode(void) {
	char *s = multiple_cursors_mode;
	if (!s || !known_cursors_mode(s)) {
		rfbLog("unknown cursors mode: %s\n", s);
		rfbLog("resetting cursors mode to \"default\"\n");
		if (multiple_cursors_mode) free(multiple_cursors_mode);
		multiple_cursors_mode = strdup("default");
		s = multiple_cursors_mode;
	}
	if (!strcmp(s, "none")) {
		show_cursor = 0;
	} else {
		/* we do NOT set show_cursor = 1, let the caller do that */
	}

	show_multiple_cursors = 0;
	if (show_cursor) {
		if (!strcmp(s, "default")) {
			if(multiple_cursors_mode) free(multiple_cursors_mode);
			multiple_cursors_mode = strdup("X");
			s = multiple_cursors_mode;
		}
		if (*s == 'X' || !strcmp(s, "some") || !strcmp(s, "most")) {
			show_multiple_cursors = 1;
		} else {
			show_multiple_cursors = 0;
			/* hmmm, some bug going back to arrow mode.. */
			set_rfb_cursor(CURS_ARROW);
		}
		if (screen) {
			set_cursor_was_changed(screen);
		}
	} else {
		if (screen) {
			screen->cursor = NULL;	/* dangerous? */
			set_cursor_was_changed(screen);
		}
	}
}

int get_which_cursor(void) {
	int which = CURS_ARROW;

	if (show_multiple_cursors) {
		int depth;
		static win_str_info_t winfo;
		static int first = 1, depth_cutoff = -1;
		Window win;
		XErrorHandler old_handler;
		int mode = 0;

		if (drag_in_progress || button_mask) {
			/* XXX not exactly what we want for menus */
			return -1;
		}

		if (!strcmp(multiple_cursors_mode, "arrow")) {
			/* should not happen... */
			return CURS_ARROW;
		} else if (!strcmp(multiple_cursors_mode, "default")) {
			mode = 0;
		} else if (!strcmp(multiple_cursors_mode, "X")) {
			mode = 1;
		} else if (!strcmp(multiple_cursors_mode, "some")) {
			mode = 2;
		} else if (!strcmp(multiple_cursors_mode, "most")) {
			mode = 3;
		}

		if (mode == 3 && xfixes_present && use_xfixes) {
			return get_xfixes_cursor(0);
		}

		if (depth_cutoff < 0) {
			int din;
			if (sscanf(multiple_cursors_mode, "X%d", &din) == 1) {
				depth_cutoff = din;
			} else {
				depth_cutoff = 0;
			}
		}

		if (first) {
			winfo.wm_name   = (char *) malloc(1024);
			winfo.res_name  = (char *) malloc(1024);
			winfo.res_class = (char *) malloc(1024);
		}
		first = 0;
		
		tree_descend_cursor(&depth, &win, &winfo);

		if (depth <= depth_cutoff && !subwin) {
			which = CURS_ROOT;

		} else if (mode == 2 || mode == 3) {
			int which0 = which;

			/* apply crude heuristics to choose a cursor... */
			if (win) {
				int ratio = 10, x, y;
				unsigned int w, h, bw, d;  
				Window r;

				trapped_xerror = 0;
				X_LOCK;
				old_handler = XSetErrorHandler(trap_xerror);

				/* "narrow" windows are WM */
				if (XGetGeometry(dpy, win, &r, &x, &y, &w, &h,
				    &bw, &d)) {
					if (w > ratio * h || h > ratio * w) {
						which = CURS_WM;
					}
				}
				XSetErrorHandler(old_handler);
				X_UNLOCK;
				trapped_xerror = 0;
			}
			if (which == which0) {
				/* the string "term" mean I-beam. */
				char *name, *class;
				lowercase(winfo.res_name);
				lowercase(winfo.res_class);
				name  = winfo.res_name;
				class = winfo.res_class;
				if (strstr(name, "term")) {
					which = CURS_TERM;
				} else if (strstr(class, "term")) {
					which = CURS_TERM;
				} else if (strstr(name,  "text")) {
					which = CURS_TERM;
				} else if (strstr(class, "text")) {
					which = CURS_TERM;
				} else if (strstr(name,  "onsole")) {
					which = CURS_TERM;
				} else if (strstr(class, "onsole")) {
					which = CURS_TERM;
				} else if (strstr(name,  "cmdtool")) {
					which = CURS_TERM;
				} else if (strstr(class, "cmdtool")) {
					which = CURS_TERM;
				} else if (strstr(name,  "shelltool")) {
					which = CURS_TERM;
				} else if (strstr(class, "shelltool")) {
					which = CURS_TERM;
				}
			}
		}
	}
	return which;
}

void set_cursor_was_changed(rfbScreenInfoPtr s) {
	rfbClientIteratorPtr iter;
	rfbClientPtr cl;

	if (! s) {
		return;
	}
	iter = rfbGetClientIterator(s);
	while( (cl = rfbClientIteratorNext(iter)) ) {
		cl->cursorWasChanged = TRUE;
	}
	rfbReleaseClientIterator(iter);
}

void set_cursor_was_moved(rfbScreenInfoPtr s) {
	rfbClientIteratorPtr iter;
	rfbClientPtr cl;

	if (! s) {
		return;
	}
	iter = rfbGetClientIterator(s);
	while( (cl = rfbClientIteratorNext(iter)) ) {
		cl->cursorWasMoved = TRUE;
	}
	rfbReleaseClientIterator(iter);
}

void restore_cursor_shape_updates(rfbScreenInfoPtr s) {
	rfbClientIteratorPtr iter;
	rfbClientPtr cl;
	int count = 0;

	if (! s || ! s->clientHead) {
		return;
	}
	iter = rfbGetClientIterator(s);
	while( (cl = rfbClientIteratorNext(iter)) ) {
		int changed = 0;
		ClientData *cd = (ClientData *) cl->clientData;

		if (cd->had_cursor_shape_updates) {
			rfbLog("restoring enableCursorShapeUpdates for client"
			    " 0x%x\n", cl);
			cl->enableCursorShapeUpdates = TRUE;	
			changed = 1;
		}
		if (cd->had_cursor_pos_updates) {
			rfbLog("restoring enableCursorPosUpdates for client"
			    " 0x%x\n", cl);
			cl->enableCursorPosUpdates = TRUE;	
			changed = 1;
		}
		if (changed) {
			cl->cursorWasChanged = TRUE;
			count++;
		}
	}
	rfbReleaseClientIterator(iter);
}

void disable_cursor_shape_updates(rfbScreenInfoPtr s) {
	rfbClientIteratorPtr iter;
	rfbClientPtr cl;

	if (! s || ! s->clientHead) {
		return;
	}

	iter = rfbGetClientIterator(s);
	while( (cl = rfbClientIteratorNext(iter)) ) {
		ClientData *cd;
		cd = (ClientData *) cl->clientData;

		if (cl->enableCursorShapeUpdates) {
			cd->had_cursor_shape_updates = 1;
		}
		if (cl->enableCursorPosUpdates) {
			cd->had_cursor_pos_updates = 1;
		}
		
		cl->enableCursorShapeUpdates = FALSE;
		cl->enableCursorPosUpdates = FALSE;
		cl->cursorWasChanged = FALSE;
	}
	rfbReleaseClientIterator(iter);
}

int cursor_shape_updates_clients(rfbScreenInfoPtr s) {
	rfbClientIteratorPtr iter;
	rfbClientPtr cl;
	int count = 0;

	if (! s) {
		return 0;
	}
	iter = rfbGetClientIterator(s);
	while( (cl = rfbClientIteratorNext(iter)) ) {
		if (cl->enableCursorShapeUpdates) {
			count++;
		}
	}
	rfbReleaseClientIterator(iter);
	return count;
}

int cursor_pos_updates_clients(rfbScreenInfoPtr s) {
	rfbClientIteratorPtr iter;
	rfbClientPtr cl;
	int count = 0;

	if (! s) {
		return 0;
	}
	iter = rfbGetClientIterator(s);
	while( (cl = rfbClientIteratorNext(iter)) ) {
		if (cl->enableCursorPosUpdates) {
			count++;
		}
	}
	rfbReleaseClientIterator(iter);
	return count;
}

/*
 * Record rfb cursor position screen->cursorX, etc (a la defaultPtrAddEvent())
 * Then set up for sending rfbCursorPosUpdates back
 * to clients that understand them.  This seems to be TightVNC specific.
 */
void cursor_position(int x, int y) {
	rfbClientIteratorPtr iter;
	rfbClientPtr cl;
	int cnt = 0, nonCursorPosUpdates_clients = 0;
	int x_in = x, y_in = y;

	/* x and y are current positions of X11 pointer on the X11 display */
	if (!screen) {
		return;
	}

	if (scaling) {
		x = ((double) x / dpy_x) * scaled_x;
		x = nfix(x, scaled_x);
		y = ((double) y / dpy_y) * scaled_y;
		y = nfix(y, scaled_y);
	}

	if (x == screen->cursorX && y == screen->cursorY) {
		return;
	}

	LOCK(screen->cursorMutex);
	screen->cursorX = x;
	screen->cursorY = y;
	UNLOCK(screen->cursorMutex);

	iter = rfbGetClientIterator(screen);
	while( (cl = rfbClientIteratorNext(iter)) ) {
		if (! cl->enableCursorPosUpdates) {
			nonCursorPosUpdates_clients++;
			continue;
		}
		if (! cursor_pos_updates) {
			continue;
		}
		if (cl == last_pointer_client) {
			/*
			 * special case if this client was the last one to
			 * send a pointer position.
			 */
			if (x_in == cursor_x && y_in == cursor_y) {
				cl->cursorWasMoved = FALSE;
			} else {
				/* an X11 app evidently warped the pointer */
				if (debug_pointer) {
					rfbLog("cursor_position: warp "
					    "detected dx=%3d dy=%3d\n",
					    cursor_x - x, cursor_y - y);
				}
				cl->cursorWasMoved = TRUE;
				cnt++;
			}
		} else {
			cl->cursorWasMoved = TRUE;
			cnt++;
		}
	}
	rfbReleaseClientIterator(iter);

	if (debug_pointer && cnt) {
		rfbLog("cursor_position: sent position x=%3d y=%3d to %d"
		    " clients\n", x, y, cnt);
	}
}

void set_rfb_cursor(int which) {

	if (! show_cursor) {
		return;
	}
	if (! screen) {
		return;
	}
	
	if (!cursors[which] || !cursors[which]->rfb) {
		rfbLog("non-existent cursor: which=%d\n", which);
		return;
	} else {
		rfbSetCursor(screen, cursors[which]->rfb);
	}
}

void set_no_cursor(void) {
	set_rfb_cursor(CURS_EMPTY);
}

int set_cursor(int x, int y, int which) {
	static int last = -1;
	int changed_cursor = 0;

	if (which < 0) {
		which = last;	
	}
	if (last < 0 || which != last) {
		set_rfb_cursor(which);
		changed_cursor = 1;
	}
	last = which;

	return changed_cursor;
}

/*
 * routine called periodically to update cursor aspects, this catches
 * warps and cursor shape changes. 
 */
int check_x11_pointer(void) {
	Window root_w, child_w;
	rfbBool ret;
	int root_x, root_y, win_x, win_y;
	int x, y;
	unsigned int mask;

	if (raw_fb && ! dpy) return 0;	/* raw_fb hack */

	X_LOCK;
	ret = XQueryPointer(dpy, rootwin, &root_w, &child_w, &root_x, &root_y,
	    &win_x, &win_y, &mask);
	X_UNLOCK;

	if (! ret) {
		return 0;
	}
	if (debug_pointer) {
		static int last_x = -1, last_y = -1;
		if (root_x != last_x || root_y != last_y) {
			rfbLog("XQueryPointer:     x:%4d, y:%4d)\n",
			    root_x, root_y);
		}
		last_x = root_x;
		last_y = root_y;
	}

	/* offset subtracted since XQueryPointer relative to rootwin */
	x = root_x - off_x - coff_x;
	y = root_y - off_y - coff_y;

	/* record the cursor position in the rfb screen */
	cursor_position(x, y);

	/* change the cursor shape if necessary */
	return set_cursor(x, y, get_which_cursor());
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
void set_colormap(int reset) {
	static int first = 1;
	static XColor color[NCOLOR], prev[NCOLOR];
	Colormap cmap;
	Visual *vis;
	int i, ncells, diffs = 0;

	if (reset) {
		first = 1;
		if (screen->colourMap.data.shorts) {
			free(screen->colourMap.data.shorts);
		}
	}

	if (first) {
		screen->colourMap.count = NCOLOR;
		screen->serverFormat.trueColour = FALSE;
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
	vis = default_visual;

	if (subwin) {
		XWindowAttributes attr;

		if (XGetWindowAttributes(dpy, window, &attr)) {
			cmap = attr.colormap;
			vis = attr.visual;
			ncells = vis->map_entries;
		}
	}

	if (ncells != NCOLOR) {
		if (first && ! quiet) {
			rfbLog("set_colormap: number of cells is %d "
			    "instead of %d.\n", ncells, NCOLOR);
		}
		if (! shift_cmap) {
			screen->colourMap.count = ncells;
		}
	}

	if (flash_cmap && ! first) {
		XWindowAttributes attr;
		Window c;
		int tries = 0;

		c = window;
		while (c && tries++ < 16) {
			c = query_pointer(c);
			if (valid_window(c, &attr, 0)) {
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
		rfbLog("set_colormap: big problem: ncells=%d > %d\n",
		    ncells, NCOLOR);
	}

	if (vis->class == TrueColor || vis->class == DirectColor) {
		/*
		 * Kludge to make 8bpp TrueColor & DirectColor be like
		 * the StaticColor map.  The ncells = 8 is "8 per subfield"
		 * mentioned in xdpyinfo.  Looks OK... perhaps fortuitously.
		 */
		if (ncells == 8 && ! shift_cmap) {
			ncells = NCOLOR;
		}
	}

	for (i=0; i < ncells; i++) {
		color[i].pixel = i;
		color[i].pad = 0;
	}

	XQueryColors(dpy, cmap, color, ncells);

	X_UNLOCK;

	for(i = ncells - 1; i >= 0; i--) {
		int k = i + shift_cmap;

		screen->colourMap.data.shorts[i*3+0] = color[i].red;
		screen->colourMap.data.shorts[i*3+1] = color[i].green;
		screen->colourMap.data.shorts[i*3+2] = color[i].blue;

		if (prev[i].red   != color[i].red ||
		    prev[i].green != color[i].green || 
		    prev[i].blue  != color[i].blue ) {
			diffs++;
		}

		if (shift_cmap && k >= 0 && k < NCOLOR) {
			/* kludge to copy the colors to higher pixel values */
			screen->colourMap.data.shorts[k*3+0] = color[i].red;
			screen->colourMap.data.shorts[k*3+1] = color[i].green;
			screen->colourMap.data.shorts[k*3+2] = color[i].blue;
		}
	}

	if (diffs && ! first) {
		if (! all_clients_initialized()) {
			rfbLog("set_colormap: warning: sending cmap "
			    "with uninitialized clients.\n");
		}
		if (shift_cmap) {
			rfbSetClientColourMaps(screen, 0, NCOLOR);
		} else {
			rfbSetClientColourMaps(screen, 0, ncells);
		}
	}

	first = 0;
}

void debug_colormap(XImage *fb) {
	static int debug_cmap = -1;
	int i, k, histo[NCOLOR];

	if (debug_cmap < 0) {
		if (getenv("DEBUG_CMAP") != NULL) {
			debug_cmap = 1;
		} else {
			debug_cmap = 0;
		}
	}
	if (! debug_cmap) {
		return;
	}
	if (! fb) {
		return;
	}
	if (fb->bits_per_pixel > 8) {
		return;
	}

	for (i=0; i < NCOLOR; i++) {
		histo[i] = 0;
	}
	for (k = 0; k < fb->width * fb->height; k++) {
		unsigned char n;
		char c = *(fb->data + k);

		n = (unsigned char) c;
		histo[n]++;
	}
	fprintf(stderr, "\nColormap histogram for current screen contents:\n");
	for (i=0; i < NCOLOR; i++) {
		unsigned short r = screen->colourMap.data.shorts[i*3+0];
		unsigned short g = screen->colourMap.data.shorts[i*3+1];
		unsigned short b = screen->colourMap.data.shorts[i*3+2];

		fprintf(stderr, "   %03d: %7d %04x/%04x/%04x", i, histo[i],
		    r, g, b);
		if ((i+1) % 2 == 0)  {
			fprintf(stderr, "\n");
		}
	}
	fprintf(stderr, "\n");
}

/*
 * Experimental mode to force the visual of the window instead of querying
 * it.  Used for testing, overriding some rare cases (win2vnc), and for
 * -overlay .  Input string can be a decimal or 0x hex or something like
 * TrueColor or TrueColor:24 to force a depth as well.
 *
 * visual_id and possibly visual_depth are set.
 */
void set_visual(char *str) {
	int vis, vdepth, defdepth = DefaultDepth(dpy, scr);
	XVisualInfo vinfo;
	char *p, *vstring = strdup(str);

	visual_id = (VisualID) 0;
	visual_depth = 0;

	if (!strcmp(vstring, "ignore") || !strcmp(vstring, "default")
	    || !strcmp(vstring, "")) {
		free(vstring);
		return;
	}

	/* set visual depth */
	if ((p = strchr(vstring, ':')) != NULL) {
		visual_depth = atoi(p+1);
		*p = '\0';
		vdepth = visual_depth;
	} else {
		vdepth = defdepth; 
	}
	if (! quiet) {
		fprintf(stderr, "\nVisual Info:\n");
		fprintf(stderr, " set_visual(\"%s\")\n", str);
		fprintf(stderr, " visual_depth: %d\n", vdepth);
	}

	/* set visual id number */
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
			rfbLog("bad -visual arg: %s\n", vstring);
			X_UNLOCK;
			clean_up_exit(1);
		}
		visual_id = (VisualID) v_in;
		free(vstring);
		return;
	}

	if (! quiet) fprintf(stderr, " visual: %d\n", vis);
	if (XMatchVisualInfo(dpy, scr, visual_depth, vis, &vinfo)) {
		;
	} else if (XMatchVisualInfo(dpy, scr, defdepth, vis, &vinfo)) {
		;
	} else {
		rfbLog("could not find visual: %s\n", vstring);
		X_UNLOCK;
		clean_up_exit(1);
	}
	free(vstring);

	/* set numerical visual id. */
	visual_id = vinfo.visualid;
}

void set_nofb_params(void) {
	use_xfixes = 0;
	use_xdamage = 0;
	use_xrecord = 0;
	wireframe = 0;

	use_solid_bg = 0;
	overlay = 0;
	overlay_cursor = 0;

	using_shm = 0;
	single_copytile = 1;

	take_naps = 0;
	measure_speeds = 0;

	show_cursor = 0;
	show_multiple_cursors = 0;
	cursor_shape_updates = 0;
	if (! got_cursorpos) {
		cursor_pos_updates = 0;
	}

	if (! quiet) {
		rfbLog("disabling: xfixes, xdamage, solid, overlay, shm,\n");
		rfbLog("  wireframe, scrollcopyrect,\n");
		rfbLog("  noonetile, nap, cursor, %scursorshape\n",
		    got_cursorpos ? "" : "cursorpos, " );
		rfbLog("  in -nofb mode.\n");
	}
}

char *raw_fb_orig_dpy = NULL;

void set_raw_fb_params(int restore) {
	static int first = 1;
	static int vo0, us0, sm0, ws0, wp0, wb0, na0, tn0;  
	static int xr0, sb0;
	static char *mc0;

	/*
	 * set turn off a bunch of parameters not compatible with 
	 * -rawfb mode: 1) ignoring the X server 2) ignoring user input. 
	 */
	
	if (first) {
		/* at least save the initial settings... */
		vo0 = view_only;
		ws0 = watch_selection;
		wp0 = watch_primary;
		wb0 = watch_bell;
		na0 = no_autorepeat;
		sb0 = use_solid_bg;

		us0 = use_snapfb;
		sm0 = using_shm;
		tn0 = take_naps;
		xr0 = xrandr;
		mc0 = multiple_cursors_mode;

		first = 0;
	}

	if (restore) {
		view_only = vo0;
		watch_selection = ws0;
		watch_primary = wp0;
		watch_bell = wb0;
		no_autorepeat = na0;
		use_solid_bg = sb0;

		use_snapfb = us0;
		using_shm = sm0;
		take_naps = tn0;
		xrandr = xr0;
		multiple_cursors_mode = mc0;

		if (! dpy && raw_fb_orig_dpy) {
			dpy = XOpenDisplay(raw_fb_orig_dpy);
			if (dpy) {
				rfbLog("reopened DISPLAY: %s\n",
				    raw_fb_orig_dpy);
			} else {
				rfbLog("WARNING: failed to reopen "
				    "DISPLAY: %s\n", raw_fb_orig_dpy);
			}
		}
		return;
	}

	rfbLog("set_raw_fb_params: modifying settings for -rawfb mode.\n");

	if (got_noviewonly) {
		/*
		 * The user input parameters are not unset under
		 * -noviewonly... this usage should be very rare
		 * (i.e. rawfb but also send user input to the X
		 * display, most likely using /dev/fb0 for some reason...)
		 */
		rfbLog("rawfb: -noviewonly mode: still sending mouse and\n");
		rfbLog("rawfb:   keyboard input to the X DISPLAY!!\n");
	} else {
		/* Normal case: */
		if (! view_only) {
			rfbLog("rawfb: setting view_only\n");
			view_only = 1;
		}
		if (watch_selection) {
			rfbLog("rawfb: turning off watch_selection\n");
			watch_selection = 0;
		}
		if (watch_primary) {
			rfbLog("rawfb: turning off watch_primary\n");
			watch_primary = 0;
		}
		if (watch_bell) {
			rfbLog("rawfb: turning off watch_bell\n");
			watch_bell = 0;
		}
		if (no_autorepeat) {
			rfbLog("rawfb: turning off no_autorepeat\n");
			no_autorepeat = 0;
		}
		if (use_solid_bg) {
			rfbLog("rawfb: turning off use_solid_bg\n");
			use_solid_bg = 0;
		}
		multiple_cursors_mode = strdup("arrow");
	}
	if (use_snapfb) {
		rfbLog("rawfb: turning off use_snapfb\n");
		use_snapfb = 0;
	}
	if (using_shm) {
		rfbLog("rawfb: turning off using_shm\n");
		using_shm = 0;
	}
	if (take_naps) {
		rfbLog("rawfb: turning off take_naps\n");
		take_naps = 0;
	}
	if (xrandr) {
		rfbLog("rawfb: turning off xrandr\n");
		xrandr = 0;
	}
}

/*
 * Presumably under -nofb the clients will never request the framebuffer.
 * However, we have gotten such a request... so let's just give them
 * the current view on the display.  n.b. x2vnc and perhaps win2vnc
 * requests a 1x1 pixel for some workaround so sadly this evidently
 * nearly always happens.
 */
void nofb_hook(rfbClientPtr cl) {
	XImage *fb;
	rfbLog("framebuffer requested in -nofb mode by client %s\n", cl->host);
	/* ignore xrandr */
	fb = XGetImage_wr(dpy, window, 0, 0, dpy_x, dpy_y, AllPlanes, ZPixmap);
	main_fb = fb->data;
	rfb_fb = main_fb;
	screen->frameBuffer = rfb_fb;
	screen->displayHook = NULL;
}

void do_new_fb(int reset_mem) {
	XImage *fb;
	char *old_main = main_fb;
	char *old_rfb  = rfb_fb;

	/* for threaded we really should lock libvncserver out. */
	if (use_threads) {
		rfbLog("warning: changing framebuffers while threaded may\n");
		rfbLog(" not work, do not use -threads if problems arise.\n");
	}

	if (reset_mem == 1) {
		/* reset_mem == 2 is a hack for changing users... */
		clean_shm(0);
		free_tiles();
	}

	fb = initialize_xdisplay_fb();

	initialize_screen(NULL, NULL, fb);

	if (reset_mem) {
		initialize_tiles();
		initialize_blackouts_and_xinerama();
		initialize_polling_images();
	}

	if (old_main != old_rfb && old_main) {
		free(old_main);
	}
	if (old_rfb) {
		free(old_rfb);
	}
	fb0 = fb;
}

void remove_fake_fb(void) {
	if (! screen) {
		return;
	}
	rfbLog("removing fake fb: 0x%x\n", fake_fb);

	do_new_fb(1);

	/*
	 * fake_fb is freed in do_new_fb(), but we set to NULL here to
	 * indicate it is gone.
	 */
	fake_fb = NULL;
}

void install_fake_fb(int w, int h, int bpp) {
	int bpc;
	if (! screen) {
		return;
	}
	if (fake_fb) {
		free(fake_fb);
	}
	fake_fb = (char *) calloc(w*h*bpp/8, 1);
	if (! fake_fb) {
		rfbLog("could not create fake fb: %dx%d %d\n", w, h, bpp);
		return;
	}
	bpc = guess_bits_per_color(bpp);
	rfbLog("installing fake fb: %dx%d %d\n", w, h, bpp);
	rfbLog("rfbNewFramebuffer(0x%x, 0x%x, %d, %d, %d, %d, %d)\n",
	    screen, fake_fb, w, h, bpc, 1, bpp/8);

	rfbNewFramebuffer(screen, fake_fb, w, h, bpc, 1, bpp/8);
}

void check_padded_fb(void) {
	if (! fake_fb) {
		return;
	}
	if (time(0) > pad_geometry_time+1 && all_clients_initialized()) {
		remove_fake_fb();
	}
}

void install_padded_fb(char *geom) {
	int w, h;
	int ok = 1;
	if (! geom || *geom == '\0') {
		ok = 0;
	} else if (sscanf(geom, "%dx%d", &w, &h) != 2)  {
		ok = 0;
	}
	w = nabs(w);
	h = nabs(h);

	if (w < 5) w = 5;
	if (h < 5) h = 5;

	if (!ok) {
		rfbLog("skipping invalid pad geometry: '%s'\n", NONUL(geom));
		return;
	}
	install_fake_fb(w, h, bpp);
	pad_geometry_time = time(0);
}

void initialize_snap_fb(void) {
	if (snap_fb) {
		free(snap_fb);
	}
	snap = XGetImage_wr(dpy, window, 0, 0, dpy_x, dpy_y, AllPlanes,
	    ZPixmap);
	snap_fb = snap->data;
}


XImage *initialize_raw_fb(void) {
	char *str, *q;
	int w, h, b, shmid = 0;
	unsigned long rm = 0, gm = 0, bm = 0;
	static XImage ximage_struct;	/* n.b.: not (XImage *) */
	int closedpy = 1, i, m;
	
	if (raw_fb_addr || raw_fb_seek) {
		if (raw_fb_shm) {
			shmdt(raw_fb_addr);
#if LIBVNCSERVER_HAVE_MMAP
		} else if (raw_fb_mmap) {
			munmap(raw_fb_addr, raw_fb_mmap);
			if (raw_fb_fd >= 0) {
				close(raw_fb_fd);
			}
#endif
		} else if (raw_fb_seek) {
			if (raw_fb_fd >= 0) {
				close(raw_fb_fd);
			}
		}
		raw_fb_addr = NULL;
	}
	if (! raw_fb_str) {
		return NULL;
	}


	if ( (q = strstr(raw_fb_str, "setup:")) == raw_fb_str) {
		FILE *pipe;
		char line[1024], *t;

		set_child_info();
		q += strlen("setup:");
		if (no_external_cmds) {
			rfbLog("cannot run external commands in -nocmds "
			    "mode:\n");
			rfbLog("   \"%s\"\n", q);
			rfbLog("   exiting.\n");
			clean_up_exit(1);
		}
		rfbLog("running command to setup rawfb: %s\n", q);
		pipe = popen(q, "r");
		if (! pipe) {
			rfbLog("popen of setup command failed.\n");
			rfbLogPerror("popen");
			clean_up_exit(1);
		}
		line[0] = '\0';
		if (fgets(line, 1024, pipe) == NULL) {
			rfbLog("read of setup command failed.\n");
			clean_up_exit(1);
		}
		pclose(pipe);
		str = strdup(line);
		t = str;
		while (*t != '\0') {
			if (*t == '\n') {
				*t = '\0';
			}
			t++;
		}
		rfbLog("setup command returned: %s\n", str);

	} else {
		str = strdup(raw_fb_str);
	}

	/*
	 * uppercase means do not close the display (e.g. for remote control)
	 */
	if (strstr(str, "SHM:") == str) {
		closedpy = 0;
		str[0] = 's'; str[1] = 'h'; str[2] = 'm';
	} else if (strstr(str, "MAP:") == str) {
		closedpy = 0;
		str[0] = 'm'; str[1] = 'a'; str[2] = 'p';
	} else if (strstr(str, "MMAP:") == str) {
		closedpy = 0;
		str[0] = 'm'; str[1] = 'm'; str[2] = 'a'; str[3] = 'p';
	} else if (strstr(str, "FILE:") == str) {
		str[0] = 'f'; str[1] = 'i'; str[2] = 'l'; str[3] = 'e';
		closedpy = 0;
	}

	if (closedpy && !view_only && got_noviewonly) {
		rfbLog("not closing X DISPLAY under -noviewonly option.\n");
		closedpy = 0;
		if (! window) {
			window = rootwin;
		}
	}

	if (! raw_fb_orig_dpy && dpy) {
		raw_fb_orig_dpy = strdup(DisplayString(dpy));
	}
#ifndef BOLDLY_CLOSE_DISPLAY
#define BOLDLY_CLOSE_DISPLAY 1
#endif
#if BOLDLY_CLOSE_DISPLAY
	if (closedpy) {
		if (dpy) {
			rfbLog("closing X DISPLAY: %s in rawfb mode.\n",
			    DisplayString(dpy));
			XCloseDisplay(dpy);	/* yow! */
		}
		dpy = NULL;
	}
#endif

	/*
	 * -rawfb shm:163938442@640x480x32:ff/ff00/ff0000+3000
	 * -rawfb map:/path/to/file@640x480x32:ff/ff00/ff0000
	 * -rawfb file:/path/to/file@640x480x32:ff/ff00/ff0000
	 */

	raw_fb_offset = 0;

	/* +O offset */
	if ((q = strrchr(str, '+')) != NULL) {
		if (sscanf(q, "+%d", &raw_fb_offset) == 1) {
			*q = '\0';
		} else {
			raw_fb_offset = 0;
		}
	}
	/* :R/G/B masks */
	if ((q = strrchr(str, ':')) != NULL) {
		if (sscanf(q, ":%lx/%lx/%lx", &rm, &gm, &bm) == 3) {
			*q = '\0';
		} else if (sscanf(q, ":0x%lx/0x%lx/0x%lx", &rm, &gm, &bm)== 3) {
			*q = '\0';
		} else if (sscanf(q, ":%lu/%lu/%lu", &rm, &gm, &bm) == 3) {
			*q = '\0';
		} else {
			rm = 0;
			gm = 0;
			bm = 0;
		}
	}
	if ((q = strrchr(str, '@')) == NULL) {
		rfbLog("invalid rawfb str: %s\n", str);
		clean_up_exit(1);
	}
	/* @WxHxB */
	if (sscanf(q, "@%dx%dx%d", &w, &h, &b) != 3) {
		rfbLog("invalid rawfb str: %s\n", str);
		clean_up_exit(1);
	}
	*q = '\0';

	if (strstr(str, "shm:") != str && strstr(str, "mmap:") != str &&
	    strstr(str, "map:") != str && strstr(str, "file:") != str) {
		/* hmmm, not following directions, see if map: applies */
		struct stat sbuf;
		if (stat(str, &sbuf) == 0) {
			char *new;
			int len = strlen("map:") + strlen(str) + 1;
			rfbLog("no type prefix: %s\n", raw_fb_str);
			rfbLog("  but file exists, so assuming: map:%s\n",
			    raw_fb_str);
			new = (char *)malloc(len);
			strcpy(new, "map:");
			strcat(new, str);
			free(str);
			str = new;
		}
	}

	dpy_x = wdpy_x = w;
	dpy_y = wdpy_y = h;
	off_x = 0;
	off_y = 0;

	raw_fb_shm = 0;
	raw_fb_mmap = 0;
	raw_fb_seek = 0;
	raw_fb_fd = -1;
	raw_fb_addr = NULL;

	if (sscanf(str, "shm:%d", &shmid) == 1) {
		/* shm:N */
#if LIBVNCSERVER_HAVE_XSHM
		raw_fb_addr = (char *) shmat(shmid, 0, SHM_RDONLY);
		if (! raw_fb_addr) {
			rfbLog("failed to attach to shm: %d, %s\n", shmid, str);
			rfbLogPerror("shmat");
			clean_up_exit(1);
		}
		raw_fb_shm = 1;
		rfbLog("rawfb: shm: %d W: %d H: %d B: %d addr: %p\n",
		    shmid, w, h, b, raw_fb_addr);
#else
		rfbLog("x11vnc was compiled without shm support.\n");
		rfbLogPerror("shmat");
		clean_up_exit(1);
#endif
	} else if (strstr(str, "map:") == str || strstr(str, "mmap:") == str
	    || strstr(str, "file:") == str) {
		/* map:/path/... or file:/path  */
		int fd, do_mmap = 1, size;
		struct stat sbuf;

		if (*str == 'f') {
			do_mmap = 0;
		}
		q = strchr(str, ':');
		q++;

		fd = open(q, O_RDONLY);
		if (fd < 0) {
			rfbLog("failed to open file: %s, %s\n", q, str);
			rfbLogPerror("open");
			clean_up_exit(1);
		}
		raw_fb_fd = fd;

		size = w*h*b/8 + raw_fb_offset;
		if (fstat(fd, &sbuf) == 0) {
			if (S_ISREG(sbuf.st_mode)) {
				if (0) size = sbuf.st_size;
			} else {
				rfbLog("raw fb is non-regular file: %s\n", q);
			}
		}

		if (do_mmap) {
#if LIBVNCSERVER_HAVE_MMAP
			raw_fb_addr = mmap(0, size, PROT_READ, MAP_SHARED,
			    fd, 0);

			if (raw_fb_addr == MAP_FAILED || raw_fb_addr == NULL) {
				rfbLog("failed to mmap file: %s, %s\n", q, str);
				rfbLog("   raw_fb_addr: %p\n", raw_fb_addr);
				rfbLogPerror("mmap");
				clean_up_exit(1);
			}
			raw_fb_mmap = size;

			rfbLog("rawfb: mmap file: %s\n", q);
			rfbLog("   w: %d h: %d b: %d addr: %p sz: %d\n", w, h,
			    b, raw_fb_addr, size);
#else
			rfbLog("mmap(2) not supported on system, using"
			    " slower lseek(2)\n");
			raw_fb_seek = size;
#endif
		} else {
			raw_fb_seek = size;

			rfbLog("rawfb: seek file: %s\n", q);
			rfbLog("   W: %d H: %d B: %d sz: %d\n", w, h, b, size);
		}
	} else {
		rfbLog("invalid rawfb str: %s\n", str);
		clean_up_exit(1);
	}

	if (! raw_fb_image) {
		raw_fb_image = &ximage_struct;
	}

	initialize_clipshift();

	raw_fb = (char *) malloc(dpy_x * dpy_y * b/8);
	raw_fb_image->data = raw_fb;
	raw_fb_image->format = ZPixmap;
	raw_fb_image->width  = dpy_x;
	raw_fb_image->height = dpy_y;
	raw_fb_image->bits_per_pixel = b;
	raw_fb_image->bytes_per_line = dpy_x*b/8;

	if (rm == 0 && gm == 0 && bm == 0) {
		/* guess masks... */
		if (b == 24 || b == 32) {
			rm = 0xff0000;
			gm = 0x00ff00;
			bm = 0x0000ff;
		} else if (b == 16) {
			rm = 0xf800;
			gm = 0x07e0;
			bm = 0x001f;
		} else if (b == 8) {
			rm = 0x07;
			gm = 0x38;
			bm = 0xc0;
		}
	}

	raw_fb_image->red_mask = rm;
	raw_fb_image->green_mask = gm;
	raw_fb_image->blue_mask = bm;

	raw_fb_image->depth = 0;
	m = 1;
	for (i=0; i<32; i++)  {
		if (rm & m) {
			raw_fb_image->depth++;
		}
		if (gm & m) {
			raw_fb_image->depth++;
		}
		if (bm & m) {
			raw_fb_image->depth++;
		}
		m = m << 1;
	}
	if (! raw_fb_image->depth) { 
		raw_fb_image->depth = (b == 32) ? 24 : b;
	}

	if (clipshift) {
		memset(raw_fb, 0xff, dpy_x * dpy_y * b/8);
	} else if (raw_fb_addr) {
		memcpy(raw_fb, raw_fb_addr + raw_fb_offset, dpy_x*dpy_y*b/8);
	} else {
		memset(raw_fb, 0xff, dpy_x * dpy_y * b/8);
	}

	rfbLog("rawfb:  raw_fb %p\n", raw_fb);

	free(str);

	return raw_fb_image;
}

void initialize_clipshift(void) {
	clipshift = 0;
	cdpy_x = cdpy_y = coff_x = coff_y = 0;
	if (clip_str) {
		int w, h, x, y, bad = 0;
		if (parse_geom(clip_str, &w, &h, &x, &y, wdpy_x, wdpy_y)) {
			if (x < 0) {
				x = 0;
			}
			if (y < 0) {
				y = 0;
			}
			if (x + w > wdpy_x) {
				w = wdpy_x - x;
			}
			if (y + h > wdpy_y) {
				h = wdpy_y - y;
			}
			if (w <= 0 || h <= 0) {
				bad = 1;
			}
		} else {
			bad = 1;
		}
		if (bad) {
			rfbLog("skipping invalid -clip WxH+X+Y: %s\n",
			    clip_str); 
		} else {
			/* OK, change geom behind everyone's back... */
			cdpy_x = w;
			cdpy_y = h;
			coff_x = x;
			coff_y = y;

			clipshift = 1;

			dpy_x = cdpy_x;
			dpy_y = cdpy_y;
		}
	}
}

/*
 * initialize a fb for the X display
 */
XImage *initialize_xdisplay_fb(void) {
	XImage *fb;
	char *vis_str = visual_str;
	int try = 0, subwin_tries = 3;
	XErrorHandler old_handler;
	int subwin_bs;

	if (raw_fb_str) {
		return initialize_raw_fb();
	}

	X_LOCK;
	if (subwin) {
		if (subwin_wait_mapped) {
			wait_until_mapped(subwin);
		}
		if (!valid_window((Window) subwin, NULL, 0)) {
			rfbLog("invalid sub-window: 0x%lx\n", subwin);
			X_UNLOCK;
			clean_up_exit(1);
		}
	}
	
	if (overlay) {
		/* 
		 * ideally we'd like to not have to cook up the
		 * visual variables but rather let it all come out
		 * of XReadScreen(), however there is no way to get
		 * a default visual out of it, so we pretend -visual
		 * TrueColor:NN was supplied with NN usually 24.
		 */
		char str[32];
		Window twin = subwin ? subwin : rootwin;
		XImage *xi;

		xi = xreadscreen(dpy, twin, 0, 0, 8, 8, False);
		sprintf(str, "TrueColor:%d", xi->depth);
		if (xi->depth != 24 && ! quiet) {
			rfbLog("warning: overlay image has depth %d "
			    "instead of 24.\n", xi->depth);
		}
		XDestroyImage(xi);
		if (visual_str != NULL && ! quiet) {
			rfbLog("warning: replacing '-visual %s' by '%s' "
			    "for use with -overlay\n", visual_str, str);
		}
		vis_str = strdup(str);
	}

	if (vis_str != NULL) {
		set_visual(vis_str);
		if (vis_str != visual_str) {
			free(vis_str);
		}
	}

	/* set up parameters for subwin or non-subwin cases: */

	if (! subwin) {
		/* full screen */
		window = rootwin;
		dpy_x = wdpy_x = DisplayWidth(dpy, scr);
		dpy_y = wdpy_y = DisplayHeight(dpy, scr);
		off_x = 0;
		off_y = 0;
		/* this may be overridden via visual_id below */
		default_visual = DefaultVisual(dpy, scr);
	} else {
		/* single window */
		XWindowAttributes attr;

		window = (Window) subwin;
		if (! XGetWindowAttributes(dpy, window, &attr)) {
			rfbLog("invalid window: 0x%lx\n", window);
			X_UNLOCK;
			clean_up_exit(1);
		}
		dpy_x = wdpy_x = attr.width;
		dpy_y = wdpy_y = attr.height;

		subwin_bs = attr.backing_store;

		/* this may be overridden via visual_id below */
		default_visual = attr.visual;

		X_UNLOCK;
		set_offset();
		X_LOCK;
	}

	initialize_clipshift();

	/* initialize depth to reasonable value, visual_id may override */
	depth = DefaultDepth(dpy, scr);

	if (visual_id) {
		int n;
		XVisualInfo vinfo_tmpl, *vinfo;

		/*
		 * we are in here from -visual or -overlay options
		 * visual_id and visual_depth were set in set_visual().
		 */

		vinfo_tmpl.visualid = visual_id; 
		vinfo = XGetVisualInfo(dpy, VisualIDMask, &vinfo_tmpl, &n);
		if (vinfo == NULL || n == 0) {
			rfbLog("could not match visual_id: 0x%x\n",
			    (int) visual_id);
			X_UNLOCK;
			clean_up_exit(1);
		}
		default_visual = vinfo->visual;
		depth = vinfo->depth;
		if (visual_depth) {
			/* force it from -visual MooColor:NN */
			depth = visual_depth;
		}
		if (! quiet) {
			fprintf(stderr, " initialize_xdisplay_fb()\n");
			fprintf(stderr, " Visual*:    %p\n", vinfo->visual);
			fprintf(stderr, " visualid:   0x%x\n",
			    (int) vinfo->visualid);
			fprintf(stderr, " screen:     %d\n", vinfo->screen);
			fprintf(stderr, " depth:      %d\n", vinfo->depth);
			fprintf(stderr, " class:      %d\n", vinfo->class);
			fprintf(stderr, " red_mask:   0x%08lx  %s\n",
			    vinfo->red_mask, bitprint(vinfo->red_mask, 32));
			fprintf(stderr, " green_mask: 0x%08lx  %s\n",
			    vinfo->green_mask, bitprint(vinfo->green_mask, 32));
			fprintf(stderr, " blue_mask:  0x%08lx  %s\n",
			    vinfo->blue_mask, bitprint(vinfo->blue_mask, 32));
			fprintf(stderr, " cmap_size:  %d\n",
			    vinfo->colormap_size);
			fprintf(stderr, " bits b/rgb: %d\n",
			    vinfo->bits_per_rgb);
			fprintf(stderr, "\n");
		}
		XFree(vinfo);
	}

	if (! quiet) {
		rfbLog("Default visual ID: 0x%x\n",
		    (int) XVisualIDFromVisual(default_visual));
	}

	again:
	if (subwin) {
		int shift = 0;
		int subwin_x, subwin_y;
		int disp_x = DisplayWidth(dpy, scr);
		int disp_y = DisplayHeight(dpy, scr);
		Window twin;
		/* subwins can be a dicey if they are changing size... */
		trapped_xerror = 0;
		old_handler = XSetErrorHandler(trap_xerror);
		XTranslateCoordinates(dpy, window, rootwin, 0, 0, &subwin_x,
		    &subwin_y, &twin);

		if (subwin_x + wdpy_x > disp_x) {
			shift = 1;
			subwin_x = disp_x - wdpy_x - 3;
		}
		if (subwin_y + wdpy_y > disp_y) {
			shift = 1;
			subwin_y = disp_y - wdpy_y - 3;
		}
		if (subwin_x < 0) {
			shift = 1;
			subwin_x = 1;
		}
		if (subwin_y < 0) {
			shift = 1;
			subwin_y = 1;
		}

		if (shift) {
			XMoveWindow(dpy, window, subwin_x, subwin_y);
		}
		XMapRaised(dpy, window);
		XRaiseWindow(dpy, window);
		XFlush(dpy);
	}
	try++;

	if (nofb) {
		/* 
		 * For -nofb we do not allocate the framebuffer, so we
		 * can save a few MB of memory. 
		 */
		fb = XCreateImage_wr(dpy, default_visual, depth, ZPixmap,
		    0, NULL, dpy_x, dpy_y, BitmapPad(dpy), 0);

	} else if (visual_id) {
		/*
		 * we need to call XCreateImage to supply the visual
		 */
		fb = XCreateImage_wr(dpy, default_visual, depth, ZPixmap,
		    0, NULL, dpy_x, dpy_y, BitmapPad(dpy), 0);
		fb->data = (char *) malloc(fb->bytes_per_line * fb->height);

	} else {
		fb = XGetImage_wr(dpy, window, 0, 0, dpy_x, dpy_y, AllPlanes,
		    ZPixmap);
		if (! quiet) {
			rfbLog("Read initial data from X display into"
			    " framebuffer.\n");
		}
	}

	if (subwin) {
		XSetErrorHandler(old_handler);
		if (trapped_xerror) {
		    rfbLog("trapped GetImage at SUBWIN creation.\n");
		    if (try < subwin_tries) {
			usleep(250 * 1000);
			if (!get_window_size(window, &wdpy_x, &wdpy_y)) {
				rfbLog("could not get size of subwin "
				    "0x%lx\n", subwin);
				X_UNLOCK;
				clean_up_exit(1);
			}
			goto again;
		    }
		}
		trapped_xerror = 0;

	} else if (! fb && try == 1) {
		/* try once more */
		usleep(250 * 1000);
		goto again;
	}
	if (use_snapfb) {
		initialize_snap_fb();
	}
	X_UNLOCK;

	if (fb->bits_per_pixel == 24 && ! quiet) {
		rfbLog("warning: 24 bpp may have poor performance.\n");
	}
	return fb;
}

void parse_scale_string(char *str, double *factor, int *scaling, int *blend,
    int *nomult4, int *pad, int *interpolate, int *numer, int *denom) {

	int m, n;
	char *p, *tstr;
	double f;

	*factor = 1.0;
	*scaling = 0;
	*blend = 1;
	*nomult4 = 0;
	*pad = 0;
	*interpolate = 0;
	*numer = 0, *denom = 0;

	if (str == NULL || str[0] == '\0') {
		return;
	}
	tstr = strdup(str);
	
	if ( (p = strchr(tstr, ':')) != NULL) {
		/* options */
		if (strstr(p+1, "nb") != NULL) {
			*blend = 0;
		}
		if (strstr(p+1, "fb") != NULL) {
			*blend = 2;
		}
		if (strstr(p+1, "n4") != NULL) {
			*nomult4 = 1;
		}
		if (strstr(p+1, "in") != NULL) {
			*interpolate = 1;
		}
		if (strstr(p+1, "pad") != NULL) {
			*pad = 1;
		}
		*p = '\0';
	}
	if (strchr(tstr, '.') != NULL) {
		double test, diff, eps = 1.0e-7;
		if (sscanf(tstr, "%lf", &f) != 1) {
			rfbLog("bad -scale arg: %s\n", tstr);
			clean_up_exit(1);
		}
		*factor = (double) f;
		/* look for common fractions from small ints: */
		for (n=2; n<=10; n++) {
			for (m=1; m<n; m++) {
				test = ((double) m)/ n;
				diff = *factor - test;
				if (-eps < diff && diff < eps) {
					*numer = m;
					*denom = n;
					break;
				
				}
			}
			if (*denom) {
				break;
			}
		}
		if (*factor < 0.01) {
			rfbLog("-scale factor too small: %f\n", scale_fac);
			clean_up_exit(1);
		}
	} else {
		if (sscanf(tstr, "%d/%d", &m, &n) != 2) {
			if (sscanf(tstr, "%d", &m) != 1) {
				rfbLog("bad -scale arg: %s\n", tstr);
				clean_up_exit(1);
			} else {
				/* e.g. -scale 1 or -scale 2 */
				n = 1;
			}
		}
		if (n <= 0 || m <=0) {
			rfbLog("bad -scale arg: %s\n", tstr);
			clean_up_exit(1);
		}
		*factor = ((double) m)/ n;
		if (*factor < 0.01) {
			rfbLog("-scale factor too small: %f\n", *factor);
			clean_up_exit(1);
		}
		*numer = m;
		*denom = n;
	}
	if (*factor == 1.0) {
		if (! quiet) {
			rfbLog("scaling disabled for factor %f\n", *factor);
		}
	} else {
		*scaling = 1;
	}
	free(tstr);
}

int scale_round(int len, double fac) {
	double eps = 0.000001;
	
	len = (int) (len * fac + eps);
	if (len < 1) {
		len = 1;
	}
	return len;
}

void setup_scaling(int *width_in, int *height_in) {
	int width  = *width_in;
	int height = *height_in;

	parse_scale_string(scale_str, &scale_fac, &scaling, &scaling_blend,
	    &scaling_nomult4, &scaling_pad, &scaling_interpolate,
	    &scale_numer, &scale_denom);

	if (scaling) {
		width  = scale_round(width,  scale_fac);
		height = scale_round(height, scale_fac);
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

		*width_in  = width;
		*height_in = height;
	}
}

/*
 * initialize the rfb framebuffer/screen
 */
void initialize_screen(int *argc, char **argv, XImage *fb) {
	int have_masks = 0;
	int width  = fb->width;
	int height = fb->height;
	int create_screen = screen ? 0 : 1;
	int bits_per_color;
	
	main_bytes_per_line = fb->bytes_per_line;

	setup_scaling(&width, &height);
	

	if (scaling) {
		rfbLog("scaling screen: %dx%d -> %dx%d  scale_fac=%.5f\n",
		    fb->width, fb->height, scaled_x, scaled_y, scale_fac);

		rfb_bytes_per_line = (main_bytes_per_line / fb->width) * width;
	} else {
		rfb_bytes_per_line = main_bytes_per_line;
	}

	/*
	 * These are just hints wrt pixel format just to let
	 * rfbGetScreen/rfbNewFramebuffer proceed with reasonable
	 * defaults.  We manually set them in painful detail below.
	 */
	bits_per_color = guess_bits_per_color(fb->bits_per_pixel);

	/* n.b. samplesPerPixel (set = 1 here) seems to be unused. */
	if (create_screen) {
		screen = rfbGetScreen(argc, argv, width, height,
		    bits_per_color, 1, (int) fb->bits_per_pixel/8);
		if (screen && http_dir) {
			http_connections(1);
		}
	} else {
		/* set set frameBuffer member below. */
		rfbLog("rfbNewFramebuffer(0x%x, 0x%x, %d, %d, %d, %d, %d)\n",
		    screen, NULL, width, height,
		    bits_per_color, 1, fb->bits_per_pixel/8);

		/* these are probably overwritten, but just to be safe: */
		screen->bitsPerPixel = fb->bits_per_pixel;
		screen->depth = fb->depth;

		rfbNewFramebuffer(screen, NULL, width, height,
		    bits_per_color, 1, (int) fb->bits_per_pixel/8);
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

	if (create_screen && *argc != 1) {
		int i;
		rfbLog("*** unrecognized option(s) ***\n");
		for (i=1; i< *argc; i++)  {
			rfbLog("\t[%d]  %s\n", i, argv[i]);
		}
		rfbLog("For a list of options run: x11vnc -help\n");
		rfbLog("\n");
		rfbLog("Here is a list of removed or obsolete"
		    " options:\n");
		rfbLog("\n");
		rfbLog("removed: -hints, -nohints\n");
		rfbLog("removed: -cursorposall\n");
		rfbLog("\n");
		rfbLog("renamed: -old_copytile, use -onetile\n");
		rfbLog("renamed: -mouse,   use -cursor\n");
		rfbLog("renamed: -mouseX,  use -cursor X\n");
		rfbLog("renamed: -X,       use -cursor X\n");
		rfbLog("renamed: -nomouse, use -nocursor\n");
		rfbLog("renamed: -old_pointer, use -pointer_mode 1\n");
	
		clean_up_exit(1);
	}

	/* set up format from scratch: */
	screen->paddedWidthInBytes = rfb_bytes_per_line;
	screen->serverFormat.bitsPerPixel = fb->bits_per_pixel;
	screen->serverFormat.depth = fb->depth;
	screen->serverFormat.trueColour = TRUE;

	screen->serverFormat.redShift   = 0;
	screen->serverFormat.greenShift = 0;
	screen->serverFormat.blueShift  = 0;
	screen->serverFormat.redMax     = 0;
	screen->serverFormat.greenMax   = 0;
	screen->serverFormat.blueMax    = 0;

	/* these main_* formats are used generally. */
	main_red_shift   = 0;
	main_green_shift = 0;
	main_blue_shift  = 0;
	main_red_max     = 0;
	main_green_max   = 0;
	main_blue_max    = 0;
	main_red_mask    = fb->red_mask;
	main_green_mask  = fb->green_mask;
	main_blue_mask   = fb->blue_mask;


	have_masks = ((fb->red_mask|fb->green_mask|fb->blue_mask) != 0);
	if (force_indexed_color) {
		have_masks = 0;
	}

	if (! have_masks && screen->serverFormat.bitsPerPixel == 8
	    && dpy && CellsOfScreen(ScreenOfDisplay(dpy, scr))) {
		/* indexed color */
		if (!quiet) {
			rfbLog("X display %s is 8bpp indexed color\n",
			    DisplayString(dpy));
			if (! flash_cmap && ! overlay) {
				rfbLog("\n");
				rfbLog("In 8bpp PseudoColor mode if you "
				    "experience color\n");
				rfbLog("problems you may want to enable "
				    "following the\n");
				rfbLog("changing colormap by using the "
				    "-flashcmap option.\n");
				rfbLog("\n");
			}
		}
		screen->serverFormat.trueColour = FALSE;
		indexed_color = 1;
		set_colormap(1);
		debug_colormap(fb);
	} else {
		/* 
		 * general case, we call it truecolor, but could be direct
		 * color, static color, etc....
		 */
		if (! quiet) {
			if (raw_fb) {
				rfbLog("Raw fb at addr %p is %dbpp depth=%d "
				    "true color\n", raw_fb_addr,
				    fb->bits_per_pixel, fb->depth);
			} else {
				rfbLog("X display %s is %dbpp depth=%d true "
				    "color\n", DisplayString(dpy),
				    fb->bits_per_pixel, fb->depth);
			}
		}

		indexed_color = 0;

		/* convert masks to bit shifts and max # colors */
		if (fb->red_mask) {
			while (! (fb->red_mask
			    & (1 << screen->serverFormat.redShift))) {
				    screen->serverFormat.redShift++;
			}
		}
		if (fb->green_mask) {
			while (! (fb->green_mask
			    & (1 << screen->serverFormat.greenShift))) {
				    screen->serverFormat.greenShift++;
			}
		}
		if (fb->blue_mask) {
			while (! (fb->blue_mask
			    & (1 << screen->serverFormat.blueShift))) {
				    screen->serverFormat.blueShift++;
			}
		}
		screen->serverFormat.redMax
		    = fb->red_mask   >> screen->serverFormat.redShift;
		screen->serverFormat.greenMax
		    = fb->green_mask >> screen->serverFormat.greenShift;
		screen->serverFormat.blueMax
		    = fb->blue_mask  >> screen->serverFormat.blueShift;

		main_red_max     = screen->serverFormat.redMax;
		main_green_max   = screen->serverFormat.greenMax;
		main_blue_max    = screen->serverFormat.blueMax;

		main_red_shift   = screen->serverFormat.redShift;
		main_green_shift = screen->serverFormat.greenShift;
		main_blue_shift  = screen->serverFormat.blueShift;
	}

#if !SMALL_FOOTPRINT
	if (!quiet) {
		fprintf(stderr, "\n");
		fprintf(stderr, "FrameBuffer Info:\n");
		fprintf(stderr, " width:            %d\n", fb->width);
		fprintf(stderr, " height:           %d\n", fb->height);
		fprintf(stderr, " scaled_width:     %d\n", width);
		fprintf(stderr, " scaled_height:    %d\n", height);
		fprintf(stderr, " indexed_color:    %d\n", indexed_color);
		fprintf(stderr, " bits_per_pixel:   %d\n", fb->bits_per_pixel);
		fprintf(stderr, " depth:            %d\n", fb->depth);
		fprintf(stderr, " red_mask:   0x%08lx  %s\n", fb->red_mask,
		    bitprint(fb->red_mask, 32));
		fprintf(stderr, " green_mask: 0x%08lx  %s\n", fb->green_mask,
		    bitprint(fb->green_mask, 32));
		fprintf(stderr, " blue_mask:  0x%08lx  %s\n", fb->blue_mask,
		    bitprint(fb->blue_mask, 32));
		fprintf(stderr, " red:   max: %3d  shift: %2d\n",
			main_red_max, main_red_shift);
		fprintf(stderr, " green: max: %3d  shift: %2d\n",
			main_green_max, main_green_shift);
		fprintf(stderr, " blue:  max: %3d  shift: %2d\n",
			main_blue_max, main_blue_shift);
		fprintf(stderr, " mainfb_bytes_per_line: %d\n",
			main_bytes_per_line);
		fprintf(stderr, " rfb_fb_bytes_per_line: %d\n",
			rfb_bytes_per_line);
		switch(fb->format) {
		case XYBitmap:
			fprintf(stderr, " format:     XYBitmap\n"); break;
		case XYPixmap:
			fprintf(stderr, " format:     XYPixmap\n"); break;
		case ZPixmap:
			fprintf(stderr, " format:     ZPixmap\n"); break;
		default:
			fprintf(stderr, " format:     %d\n", fb->format); break;
		}
		switch(fb->byte_order) {
		case LSBFirst:
			fprintf(stderr, " byte_order: LSBFirst\n"); break;
		case MSBFirst:
			fprintf(stderr, " byte_order: MSBFirst\n"); break;
		default:
			fprintf(stderr, " byte_order: %d\n", fb->byte_order);
			break;
		}
		fprintf(stderr, " bitmap_pad:  %d\n", fb->bitmap_pad);
		fprintf(stderr, " bitmap_unit: %d\n", fb->bitmap_unit);
		switch(fb->bitmap_bit_order) {
		case LSBFirst:
			fprintf(stderr, " bitmap_bit_order: LSBFirst\n"); break;
		case MSBFirst:
			fprintf(stderr, " bitmap_bit_order: MSBFirst\n"); break;
		default:
			fprintf(stderr, " bitmap_bit_order: %d\n",
			fb->bitmap_bit_order); break;
		}
	}
	if (overlay && ! quiet) {
		rfbLog("\n");
		rfbLog("Overlay mode enabled:  If you experience color\n");
		rfbLog("problems when popup menus are on the screen, try\n");
		rfbLog("disabling SaveUnders in your X server, one way is\n");
		rfbLog("to start the X server with the '-su' option, e.g.:\n");
		rfbLog("Xsun -su ... see Xserver(1), xinit(1) for more info.\n");
		rfbLog("\n");
	}
#endif
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
	if (!quiet) {
		fprintf(stderr, " main_fb:     %p\n", main_fb);
		fprintf(stderr, " rfb_fb:      %p\n", rfb_fb);
		fprintf(stderr, "\n");
	}

	bpp   = screen->serverFormat.bitsPerPixel;
	depth = screen->serverFormat.depth;

	/* may need, bpp, main_red_max, etc. */
	parse_wireframe();
	parse_scroll_copyrect();

	setup_cursors_and_push();

	if (scaling) {
		mark_rect_as_modified(0, 0, dpy_x, dpy_y, 0);
	}

	if (! create_screen) {
		rfbClientIteratorPtr iter;
		rfbClientPtr cl;

		/* 
		 * since bits_per_color above may have been approximate,
		 * try to reset the individual translation tables...
		 * we do not seem to need this with rfbGetScreen()...
		 */
		if (!quiet) rfbLog("calling setTranslateFunction()...\n");
		iter = rfbGetClientIterator(screen);
		while ((cl = rfbClientIteratorNext(iter)) != NULL) {
			screen->setTranslateFunction(cl);
		}
		rfbReleaseClientIterator(iter);
		if (!quiet) rfbLog("  done.\n");
		do_copy_screen = 1;
		
		/* done for framebuffer change case */
		return;
	}

	/*
	 * the rest is screen server initialization, etc, only needed
	 * at screen creation time.
	 */

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
		screen->port = 0;

	} else if (! got_rfbport) {
		screen->autoPort = TRUE;
	}

	if (! got_nevershared && ! got_alwaysshared) {
		if (shared) {
			screen->alwaysShared = TRUE;
		} else {
			screen->dontDisconnect = TRUE;
			screen->neverShared = TRUE;
		}
	}
	/* XXX the following is based on libvncserver defaults. */
	if (screen->deferUpdateTime == 5) {
		/* XXX will be fixed someday */
		screen->deferUpdateTime = defer_update;
	}

	/* event callbacks: */
	screen->newClientHook = new_client;
	screen->kbdAddEvent = keyboard;
	screen->ptrAddEvent = pointer;
	screen->setXCutText = xcut_receive;

	rfbInitServer(screen);

	if (viewonly_passwd) {
		/* append the view only passwd after the normal passwd */
		char **passwds_new = malloc(3*sizeof(char**));
		char **passwds_old = (char **) screen->authPasswdData;
		passwds_new[0] = passwds_old[0];
		passwds_new[1] = viewonly_passwd;
		passwds_new[2] = NULL;
		screen->authPasswdData = (void*) passwds_new;
	}
}

/* -- solid.c -- */

void usr_bin_path(int restore) {
	static char *oldpath = NULL;
	char *newpath;
	char addpath[] = "/usr/bin:/bin:";

	if (restore) {
		if (oldpath) {
			set_env("PATH", oldpath);
			free(oldpath);
			oldpath = NULL;
		}
		return;
	}

	if (getenv("PATH")) {
		oldpath = strdup(getenv("PATH"));
	} else {
		oldpath = strdup("/usr/bin");
	}
	newpath = (char *) malloc(strlen(oldpath) + strlen(addpath) + 1);
	newpath[0] = '\0';
	strcat(newpath, addpath);
	strcat(newpath, oldpath);
	set_env("PATH", newpath);
	free(newpath);
}

int dt_cmd(char *cmd) {
	int rc;

	if (!cmd || *cmd == '\0') {
		return 0;
	}

	if (no_external_cmds) {
		rfbLog("cannot run external commands in -nocmds mode:\n");
		rfbLog("   \"%s\"\n", cmd);
		rfbLog("   dt_cmd: returning 1\n");
		return 1;
	}

	rfbLog("running command:\n  %s\n", cmd);
	usr_bin_path(0);
	rc = system(cmd);
	usr_bin_path(1);

	if (rc >= 256) {
		rc = rc/256;
	}
	return rc;
}

char *cmd_output(char *cmd) {
	FILE *p;
	static char output[50000];
	char line[1024];
	int rc;

	if (!cmd || *cmd == '\0') {
		return "";
	}

	if (no_external_cmds) {
		rfbLog("cannot run external commands in -nocmds mode:\n");
		rfbLog("   \"%s\"\n", cmd);
		rfbLog("   cmd_output: null string.\n");
		return "";
	}

	rfbLog("running pipe:\n  %s\n", cmd);
	usr_bin_path(0);
	p = popen(cmd, "r");
	usr_bin_path(1);

	output[0] = '\0';

	while (fgets(line, 1024, p) != NULL) {
		if (strlen(output) + strlen(line) + 1 < 50000) {
			strcat(output, line);
		}
	}
	rc = pclose(p);
	return(output);
}

void solid_root(char *color) {
	Window expose;
	static XImage *image = NULL;
	Pixmap pixmap;
	XGCValues gcv;
	GC gc;
	XSetWindowAttributes swa;
	Visual visual;
	unsigned long mask, pixel;
	XColor cdef;
	Colormap cmap;

	if (subwin || window != rootwin) {
		rfbLog("cannot set subwin to solid color, must be rootwin\n");
		return;
	}

	/* create the "clear" window just for generating exposures */
	swa.override_redirect = True;
	swa.backing_store = NotUseful;
	swa.save_under = False;
	swa.background_pixmap = None;
	visual.visualid = CopyFromParent;
	mask = (CWOverrideRedirect|CWBackingStore|CWSaveUnder|CWBackPixmap);
	expose = XCreateWindow(dpy, window, 0, 0, wdpy_x, wdpy_y, 0, depth,
	    InputOutput, &visual, mask, &swa);

	if (! color) {
		/* restore the root window from the XImage snapshot */
		pixmap = XCreatePixmap(dpy, window, wdpy_x, wdpy_y, depth);

		if (! image) {
			/* whoops */
			XDestroyWindow(dpy, expose);
			rfbLog("no root snapshot available.\n");
			return;
		}

		
		/* draw the image to a pixmap: */
		gcv.function = GXcopy;
		gcv.plane_mask = AllPlanes;
		gc = XCreateGC(dpy, window, GCFunction|GCPlaneMask, &gcv);

		XPutImage(dpy, pixmap, gc, image, 0, 0, 0, 0, wdpy_x, wdpy_y);

		gcv.foreground = gcv.background = BlackPixel(dpy, scr);
		gc = XCreateGC(dpy, window, GCForeground|GCBackground, &gcv);

		rfbLog("restoring root snapshot...\n");
		/* set the pixmap as the bg: */
		XSetWindowBackgroundPixmap(dpy, window, pixmap);
		XFreePixmap(dpy, pixmap);
		XClearWindow(dpy, window);
		XFlush(dpy);
		
		/* generate exposures */
		XMapWindow(dpy, expose);
		XSync(dpy, False);
		XDestroyWindow(dpy, expose);
		return;
	}

	if (! image) {
		/* need to retrieve a snapshot of the root background: */
		Window iwin;
		XSetWindowAttributes iswa;

		/* create image window: */
		iswa.override_redirect = True;
		iswa.backing_store = NotUseful;
		iswa.save_under = False;
		iswa.background_pixmap = ParentRelative;

		iwin = XCreateWindow(dpy, window, 0, 0, wdpy_x, wdpy_y, 0,
		    depth, InputOutput, &visual, mask, &iswa);

		rfbLog("snapshotting background...\n");

		XMapWindow(dpy, iwin);
		XSync(dpy, False);
		image = XGetImage(dpy, iwin, 0, 0, wdpy_x, wdpy_y, AllPlanes,
		    ZPixmap);
		XSync(dpy, False);
		XDestroyWindow(dpy, iwin);
	}

	/* use black for low colors or failure */
	pixel = BlackPixel(dpy, scr);
	if (depth > 8 || strcmp(color, solid_default)) {
		cmap = DefaultColormap (dpy, scr);
		if (XParseColor(dpy, cmap, color, &cdef) &&
		    XAllocColor(dpy, cmap, &cdef)) {
			pixel = cdef.pixel;
		} else {
			rfbLog("error parsing/allocing color: %s\n", color);
		}
	}

	rfbLog("setting solid background...\n");
	XSetWindowBackground(dpy, window, pixel);
	XMapWindow(dpy, expose);
	XSync(dpy, False);
	XDestroyWindow(dpy, expose);
}

void solid_cde(char *color) {
	int wsmax = 16;
	static XImage *image[16];
	static Window ws_wins[16];
	static int nws = -1;

	Window expose;
	Pixmap pixmap;
	XGCValues gcv;
	GC gc;
	XSetWindowAttributes swa;
	Visual visual;
	unsigned long mask, pixel;
	XColor cdef;
	Colormap cmap;
	int n;

	if (subwin || window != rootwin) {
		rfbLog("cannot set subwin to solid color, must be rootwin\n");
		return;
	}

	/* create the "clear" window just for generating exposures */
	swa.override_redirect = True;
	swa.backing_store = NotUseful;
	swa.save_under = False;
	swa.background_pixmap = None;
	visual.visualid = CopyFromParent;
	mask = (CWOverrideRedirect|CWBackingStore|CWSaveUnder|CWBackPixmap);
	expose = XCreateWindow(dpy, window, 0, 0, wdpy_x, wdpy_y, 0, depth,
	    InputOutput, &visual, mask, &swa);

	if (! color) {
		/* restore the backdrop windows from the XImage snapshots */

		for (n=0; n < nws; n++) {
			Window twin;

			if (! image[n]) {
				continue;
			}

			twin = ws_wins[n];
			if (! twin) {
				twin = rootwin;
			}
			if (! valid_window(twin, NULL, 0)) {
				continue;
			}

			pixmap = XCreatePixmap(dpy, twin, wdpy_x, wdpy_y,
			    depth);
			
			/* draw the image to a pixmap: */
			gcv.function = GXcopy;
			gcv.plane_mask = AllPlanes;
			gc = XCreateGC(dpy, twin, GCFunction|GCPlaneMask, &gcv);

			XPutImage(dpy, pixmap, gc, image[n], 0, 0, 0, 0,
			    wdpy_x, wdpy_y);

			gcv.foreground = gcv.background = BlackPixel(dpy, scr);
			gc = XCreateGC(dpy, twin, GCForeground|GCBackground,
			    &gcv);

			rfbLog("restoring CDE ws%d snapshot to 0x%lx\n",
			    n, twin);
			/* set the pixmap as the bg: */
			XSetWindowBackgroundPixmap(dpy, twin, pixmap);
			XFreePixmap(dpy, pixmap);
			XClearWindow(dpy, twin);
			XFlush(dpy);
		}
		
		/* generate exposures */
		XMapWindow(dpy, expose);
		XSync(dpy, False);
		XDestroyWindow(dpy, expose);
		return;
	}

	if (nws < 0) {
		/* need to retrieve snapshots of the ws backgrounds: */
		Window iwin, wm_win;
		XSetWindowAttributes iswa;
		Atom dt_list, wm_info, type;
		int format;
		unsigned long length, after;
		unsigned char *data;
		unsigned int * dp;

		nws = 0;

		/* extract the hidden wm properties about backdrops: */

		wm_info = XInternAtom(dpy, "_MOTIF_WM_INFO", True);
		if (wm_info == None) {
			return;
		}

		XGetWindowProperty(dpy, rootwin, wm_info, 0L, 10L, False,
		    AnyPropertyType, &type, &format, &length, &after, &data);

		/*
		 * xprop -notype -root _MOTIF_WM_INFO
		 * _MOTIF_WM_INFO = 0x2, 0x580028
		 */

		if (length < 2 || format != 32 || after != 0) {
			return;
		}

		dp = (unsigned int *) data;
		wm_win = (Window) *(dp+1);	/* 2nd item. */


		dt_list = XInternAtom(dpy, "_DT_WORKSPACE_LIST", True);
		if (dt_list == None) {
			return;
		}

		XGetWindowProperty(dpy, wm_win, dt_list, 0L, 10L, False,
		   AnyPropertyType, &type, &format, &length, &after, &data);

		nws = length;

		if (nws > wsmax) {
			nws = wsmax;
		}
		if (nws < 0) {
			nws = 0;
		}

		rfbLog("special CDE win: 0x%lx, %d workspaces\n", wm_win, nws);
		if (nws == 0) {
			return;
		}

		for (n=0; n<nws; n++) {
			Atom ws_atom;
			char tmp[32];
			Window twin;
			XWindowAttributes attr;
			int i, cnt;

			image[n] = NULL;
			ws_wins[n] = 0x0;

			sprintf(tmp, "_DT_WORKSPACE_INFO_ws%d", n);
			ws_atom = XInternAtom(dpy, tmp, False);
			if (ws_atom == None) {
				continue;
			}
			XGetWindowProperty(dpy, wm_win, ws_atom, 0L, 100L,
			   False, AnyPropertyType, &type, &format, &length,
			   &after, &data);

			if (format != 8 || after != 0) {
				continue;
			}
			/*
			 * xprop -notype -id wm_win
			 * _DT_WORKSPACE_INFO_ws0 = "One", "3", "0x2f2f4a",
			 * "0x63639c", "0x103", "1", "0x58044e"
			 */

			cnt = 0;
			twin = 0x0;
			for (i=0; i<length; i++) {
				if (*(data+i) != '\0') {
					continue;
				}
				cnt++;	/* count nulls to indicate field */
				if (cnt == 6) {
					/* one past the null: */
					char *q = (char *) (data+i+1);
					unsigned long in;
					if (sscanf(q, "0x%lx", &in) == 1) {
						twin = (Window) in;
						break;
					}
				}
			}
			ws_wins[n] = twin;

			if (! twin) {
				twin = rootwin;
			}

			XGetWindowAttributes(dpy, twin, &attr);
			if (twin != rootwin) {
				if (attr.map_state != IsViewable) {
					XMapWindow(dpy, twin);
				}
				XRaiseWindow(dpy, twin);
			}
			XSync(dpy, False);
		
			/* create image window: */
			iswa.override_redirect = True;
			iswa.backing_store = NotUseful;
			iswa.save_under = False;
			iswa.background_pixmap = ParentRelative;
			visual.visualid = CopyFromParent;

			iwin = XCreateWindow(dpy, twin, 0, 0, wdpy_x, wdpy_y,
			    0, depth, InputOutput, &visual, mask, &iswa);

			rfbLog("snapshotting CDE backdrop ws%d 0x%lx -> "
			    "0x%lx ...\n", n, twin, iwin);
			XMapWindow(dpy, iwin);
			XSync(dpy, False);

			image[n] = XGetImage(dpy, iwin, 0, 0, wdpy_x, wdpy_y,
			    AllPlanes, ZPixmap);
			XSync(dpy, False);
			XDestroyWindow(dpy, iwin);
			if (twin != rootwin) {
				XLowerWindow(dpy, twin);
				if (attr.map_state != IsViewable) {
					XUnmapWindow(dpy, twin);
				}
			}
		}
	}
	if (nws == 0) {
		return;
	}

	/* use black for low colors or failure */
	pixel = BlackPixel(dpy, scr);
	if (depth > 8 || strcmp(color, solid_default)) {
		cmap = DefaultColormap (dpy, scr);
		if (XParseColor(dpy, cmap, color, &cdef) &&
		    XAllocColor(dpy, cmap, &cdef)) {
			pixel = cdef.pixel;
		} else {
			rfbLog("error parsing/allocing color: %s\n", color);
		}
	}

	rfbLog("setting solid backgrounds...\n");

	for (n=0; n < nws; n++)  {
		Window twin = ws_wins[n];
		if (image[n] == NULL) {
			continue;
		}
		if (! twin)  {
			twin = rootwin;
		}
		XSetWindowBackground(dpy, twin, pixel);
	}
	XMapWindow(dpy, expose);
	XSync(dpy, False);
	XDestroyWindow(dpy, expose);
}

void solid_gnome(char *color) {
	char get_color[] = "gconftool-2 --get "
	    "/desktop/gnome/background/primary_color";
	char set_color[] = "gconftool-2 --set "
	    "/desktop/gnome/background/primary_color --type string '%s'";
	char get_option[] = "gconftool-2 --get "
	    "/desktop/gnome/background/picture_options";
	char set_option[] = "gconftool-2 --set "
	    "/desktop/gnome/background/picture_options --type string '%s'";
	static char *orig_color = NULL;
	static char *orig_option = NULL;
	char *cmd;
	
	if (! color) {
		if (! orig_color) {
			orig_color = strdup("#FFFFFF");
		}
		if (! orig_option) {
			orig_option = strdup("stretched");
		}
		if (strstr(orig_color, "'") != NULL)  {
			rfbLog("bad color: %s\n", orig_color);
			return;
		}
		if (strstr(orig_option, "'") != NULL)  {
			rfbLog("bad option: %s\n", orig_option);
			return;
		}
		cmd = (char *)malloc(strlen(set_option) - 2 +
		    strlen(orig_option) + 1);
		sprintf(cmd, set_option, orig_option);
		dt_cmd(cmd);
		free(cmd);
		cmd = (char *)malloc(strlen(set_color) - 2 +
		    strlen(orig_color) + 1);
		sprintf(cmd, set_color, orig_color);
		dt_cmd(cmd);
		free(cmd);
		return;
	}

	if (! orig_color) {
		char *q;
		orig_color = strdup(cmd_output(get_color));
		if (*orig_color == '\0') {
			orig_color = strdup("#FFFFFF");
		}
		if ((q = strchr(orig_color, '\n')) != NULL) {
			*q = '\0';
		}
	}
	if (! orig_option) {
		char *q;
		orig_option = strdup(cmd_output(get_option));
		if (*orig_option == '\0') {
			orig_option = strdup("stretched");
		}
		if ((q = strchr(orig_option, '\n')) != NULL) {
			*q = '\0';
		}
	}
	if (strstr(color, "'") != NULL)  {
		rfbLog("bad color: %s\n", color);
		return;
	}
	cmd = (char *)malloc(strlen(set_color) + strlen(color) + 1);
	sprintf(cmd, set_color, color);
	dt_cmd(cmd);
	free(cmd);

	cmd = (char *)malloc(strlen(set_option) + strlen("none") + 1);
	sprintf(cmd, set_option, "none");
	dt_cmd(cmd);
	free(cmd);
}

void solid_kde(char *color) {
	char set_color[] =
	    "dcop --user '%s' kdesktop KBackgroundIface setColor '%s' 1";
	char bg_off[] =
	    "dcop --user '%s' kdesktop KBackgroundIface setBackgroundEnabled 0";
	char bg_on[] =
	    "dcop --user '%s' kdesktop KBackgroundIface setBackgroundEnabled 1";
	char *cmd, *user = NULL;
	int len;

	user = get_user_name();
	if (strstr(user, "'") != NULL)  {
		rfbLog("bad user: %s\n", user);
		free(user);
		return;
	}

	if (! color) {
		len = strlen(bg_on) + strlen(user) + 1;
		cmd = (char *)malloc(len);
		sprintf(cmd, bg_on, user);
		dt_cmd(cmd);
		free(cmd);
		free(user);

		return;
	}

	if (strstr(color, "'") != NULL)  {
		rfbLog("bad color: %s\n", color);
		return;
	}

	len = strlen(set_color) + strlen(user) + strlen(color) + 1;
	cmd = (char *)malloc(len);
	sprintf(cmd, set_color, user, color);
	dt_cmd(cmd);
	free(cmd);

	len = strlen(bg_off) + strlen(user) + 1;
	cmd = (char *)malloc(len);
	sprintf(cmd, bg_off, user);
	dt_cmd(cmd);
	free(cmd);
	free(user);
}

char *guess_desktop() {
	Atom prop;
	prop = XInternAtom(dpy, "XFCE_DESKTOP_WINDOW", True);
	if (prop != None) {
		return "xfce";
	}
	prop = XInternAtom(dpy, "_QT_DESKTOP_PROPERTIES", True);
	if (prop != None) {
		return "kde";
	}
	prop = XInternAtom(dpy, "NAUTILUS_DESKTOP_WINDOW_ID", True);
	if (prop != None) {
		return "gnome";
	}
	prop = XInternAtom(dpy, "_MOTIF_WM_INFO", True);
	if (prop != None) {
		prop = XInternAtom(dpy, "_DT_WORKSPACE_LIST", True);
		if (prop != None) {
			return "cde";
		}
	}
	return "root";
}

void solid_bg(int restore) {
	static int desktop = -1;
	static int solid_on = 0;
	static char *prev_str;
	char *dtname, *color;

	if (started_as_root == 1 && users_list) {
		/* we are still root, don't try. */
		return;
	}

	if (restore) {
		if (! solid_on) {
			return;
		}
		if (desktop == 0) {
			solid_root(NULL);
		} else if (desktop == 1) {
			solid_gnome(NULL);
		} else if (desktop == 2) {
			solid_kde(NULL);
		} else if (desktop == 3) {
			solid_cde(NULL);
		}
		solid_on = 0;
		return;
	}
	if (! solid_str) {
		return;
	}
	if (solid_on && !strcmp(prev_str, solid_str)) {
		return;
	}
	if (strstr(solid_str, "guess:") == solid_str
	    || !strchr(solid_str, ':')) {
		dtname = guess_desktop();
		rfbLog("guessed desktop: %s\n", dtname);
	} else {
		if (strstr(solid_str, "gnome:") == solid_str) {
			dtname = "gnome";
		} else if (strstr(solid_str, "kde:") == solid_str) {
			dtname = "kde";
		} else if (strstr(solid_str, "cde:") == solid_str) {
			dtname = "cde";
		} else {
			dtname = "root";
		}
	}

	color = strchr(solid_str, ':');
	if (! color) {
		color = solid_str;
	} else {
		color++;
		if (*color == '\0') {
			color = solid_default;
		}
	}
	if (!strcmp(dtname, "gnome")) {
		desktop = 1;
		solid_gnome(color);
	} else if (!strcmp(dtname, "kde")) {
		desktop = 2;
		solid_kde(color);
	} else if (!strcmp(dtname, "cde")) {
		desktop = 3;
		solid_cde(color);
	} else {
		desktop = 0;
		solid_root(color);
	}
	if (prev_str) {
		free(prev_str);
	}
	prev_str = strdup(solid_str);
	solid_on = 1;
}

/* -- xinerama.c -- */
/*
 * routines related to xinerama and blacking out rectangles
 */

/* blacked-out region (-blackout, -xinerama) */
typedef struct bout {
	int x1, y1, x2, y2;
} blackout_t;
#define BO_MAX 16
typedef struct tbout {
	blackout_t bo[BO_MAX];	/* hardwired max rectangles. */
	int cover;
	int count;
} tile_blackout_t;

#define BLACKR_MAX 100
blackout_t blackr[BLACKR_MAX];	/* hardwired max blackouts */
tile_blackout_t *tile_blackout;
int blackouts = 0;

/*
 * Take a comma separated list of geometries: WxH+X+Y and register them as
 * rectangles to black out from the screen.
 */
void initialize_blackouts(char *list) {
	char *p, *blist = strdup(list);
	int x, y, X, Y, h, w, t;

	blackouts = 0;

	p = strtok(blist, ", \t");
	while (p) {
		if (! parse_geom(p, &w, &h, &x, &y, dpy_x, dpy_y)) {
			if (*p != '\0') {
				rfbLog("skipping invalid geometry: %s\n", p);
			}
			p = strtok(NULL, ", \t");
			continue;
		}
		w = nabs(w);
		h = nabs(h);
		x = nfix(x, dpy_x);
		y = nfix(y, dpy_y);
		X = x + w;
		Y = y + h;
		X = nfix(X, dpy_x+1);
		Y = nfix(Y, dpy_y+1);
		if (x > X) {
			t = X; X = x; x = t;
		}
		if (y > Y) {
			t = Y; Y = y; y = t;
		}
		if (x < 0 || x > dpy_x || y < 0 || y > dpy_y ||
		    X < 0 || X > dpy_x || Y < 0 || Y > dpy_y ||
		    x == X || y == Y) {
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
			if (blackouts >= BLACKR_MAX) {
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
	int debug_bo = 0;
	if (! blackouts) {
		return;
	}
	if (getenv("DEBUG_BLACKOUT") != NULL) {
		debug_bo = 1;
	}

	/* 
	 * to simplify things drop down to single copy mode, etc...
	 */
	single_copytile = 1;
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
				    sraRgnCreateRect(blackr[b].x1,
				    blackr[b].y1, blackr[b].x2, blackr[b].y2);

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
					if (debug_bo) {
 						fprintf(stderr, "full: %d=%d,%d"
						    "  (%d-%d)  (%d-%d)\n",
						    n, tx, ty, x1, x2, y1, y2);
					}
				} else {
					tile_blackout[n].cover = 1;
					if (debug_bo) {
						fprintf(stderr, "part: %d=%d,%d"
						    "  (%d-%d)  (%d-%d)\n",
						    n, tx, ty, x1, x2, y1, y2);
					}
				}

				if (++cnt >= BO_MAX) {
					rfbLog("too many blackout rectangles "
					    "for tile %d=%d,%d.\n", n, tx, ty);
					break;
				}
			}

			sraRgnReleaseIterator(iter);
			sraRgnDestroy(black_reg);
			sraRgnDestroy(tile_reg);

			tile_blackout[n].count = cnt;
			if (debug_bo && cnt > 1) {
 				rfbLog("warning: multiple region overlaps[%d] "
				    "for tile %d=%d,%d.\n", cnt, n, tx, ty);
			}
		}
	}
}

void initialize_xinerama (void) {
#if !LIBVNCSERVER_HAVE_LIBXINERAMA
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

	if (raw_fb && ! dpy) return;	/* raw_fb hack */

	if (! XineramaQueryExtension(dpy, &ev, &er)) {
		rfbLog("Xinerama: disabling: display does not support it.\n");
		xinerama = 0;
		xinerama_present = 0;
		return;
	}
	if (! XineramaIsActive(dpy)) {
		/* n.b. change to XineramaActive(dpy, window) someday */
		rfbLog("Xinerama: disabling: not active on display.\n");
		xinerama = 0;
		xinerama_present = 0;
		return;
	} 
	xinerama_present = 1;

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
	bstr = (char *) malloc(30 * (rcnt+1) * sizeof(char));
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
	initialize_blackouts(bstr);

	free(bstr);
	free(tstr);
#endif
}

void initialize_blackouts_and_xinerama(void) {
	if (blackout_str != NULL) {
		initialize_blackouts(blackout_str);
	}
	if (xinerama) {
		initialize_xinerama();
	}
	if (blackouts) {
		blackout_tiles();
		/* schedule a copy_screen(), now is too early. */
		do_copy_screen = 1;
	}
}

void push_sleep(n) {
	int i;
	for (i=0; i<n; i++) {
		rfbPE(-1);
		if (i != n-1 && defer_update) {
			usleep(defer_update * 1000);
		}
	}
}

/*
 * try to forcefully push a black screen to all connected clients
 */
void push_black_screen(int n) {
	if (!screen) {
		return;
	}
	zero_fb(0, 0, dpy_x, dpy_y);
	mark_rect_as_modified(0, 0, dpy_x, dpy_y, 1);
	push_sleep(n);
}

void refresh_screen(int push) {
	int i;
	if (!screen) {
		return;
	}
	mark_rect_as_modified(0, 0, dpy_x, dpy_y, 1);
	for (i=0; i<push; i++) {
		rfbPE(-1);
	}
}

/*
 * Fill the framebuffer with zero for the prescribed rectangle
 */
void zero_fb(int x1, int y1, int x2, int y2) {
	int pixelsize = bpp/8;
	int line, fill = 0;
	char *dst;
	
	if (x1 < 0 || x2 <= x1 || x2 > dpy_x) {
		return;
	}
	if (y1 < 0 || y2 <= y1 || y2 > dpy_y) {
		return;
	}
	if (! main_fb) {
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

typedef struct tile_change_region {
	/* start and end lines, along y, of the changed area inside a tile. */
	unsigned short first_line, last_line;
	short first_x, last_x;
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
	tile_has_xdamage_diff = (unsigned char *)
		malloc((size_t) (ntiles * sizeof(unsigned char)));
	tile_row_has_xdamage_diff = (unsigned char *)
		malloc((size_t) (ntiles_y * sizeof(unsigned char)));
	tile_tried    = (unsigned char *)
		malloc((size_t) (ntiles * sizeof(unsigned char)));
	tile_copied   = (unsigned char *)
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

void free_tiles(void) {
	if (tile_has_diff) {
		free(tile_has_diff);
		tile_has_diff = NULL;
	}
	if (tile_has_xdamage_diff) {
		free(tile_has_xdamage_diff);
		tile_has_xdamage_diff = NULL;
	}
	if (tile_row_has_xdamage_diff) {
		free(tile_row_has_xdamage_diff);
		tile_row_has_xdamage_diff = NULL;
	}
	if (tile_tried) {
		free(tile_tried);
		tile_tried = NULL;
	}
	if (tile_copied) {
		free(tile_copied);
		tile_copied = NULL;
	}
	if (tile_blackout) {
		free(tile_blackout);
		tile_blackout = NULL;
	}
	if (tile_region) {
		free(tile_region);
		tile_region = NULL;
	}
	if (tile_row) {
		free(tile_row);
		tile_row = NULL;
	}
	if (tile_row_shm) {
		free(tile_row_shm);
		tile_row_shm = NULL;
	}
	if (hint_list) {
		free(hint_list);
		hint_list = NULL;
	}
}

/*
 * silly function to factor dpy_y until fullscreen shm is not bigger than max.
 * should always work unless dpy_y is a large prime or something... under
 * failure fs_factor remains 0 and no fullscreen updates will be tried.
 */
static int fs_factor = 0;

static void set_fs_factor(int max) {
	int f, fac = 1, n = dpy_y;

	fs_factor = 0;
	if ((bpp/8) * dpy_x * dpy_y <= max)  {
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

char *flip_ximage_byte_order(XImage *xim) {
	char *order;
	if (xim->byte_order == LSBFirst) {
		order = "MSBFirst";
		xim->byte_order = MSBFirst;
		xim->bitmap_bit_order = MSBFirst;
	} else {
		order = "LSBFirst";
		xim->byte_order = LSBFirst;
		xim->bitmap_bit_order = LSBFirst;
	}
	return order;
}

/*
 * set up an XShm image, or if not using shm just create the XImage.
 */
static int shm_create(XShmSegmentInfo *shm, XImage **ximg_ptr, int w, int h,
    char *name) {

	XImage *xim;
	static int reported_flip = 0;

	shm->shmid = -1;
	shm->shmaddr = (char *) -1;
	*ximg_ptr = NULL;

	if (nofb) {
		return 1;
	}

	X_LOCK;

	if (! using_shm) {
		/* we only need the XImage created */
		xim = XCreateImage_wr(dpy, default_visual, depth, ZPixmap,
		    0, NULL, w, h, raw_fb ? 32 : BitmapPad(dpy), 0);

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
			char *order = flip_ximage_byte_order(xim);
			if (! reported_flip && ! quiet) {
				rfbLog("Changing XImage byte order"
				    " to %s\n", order);
				reported_flip = 1;
			}
		}

		*ximg_ptr = xim;
		return 1;
	}

	xim = XShmCreateImage_wr(dpy, default_visual, depth, ZPixmap, NULL,
	    shm, w, h);

	if (xim == NULL) {
		rfbErr("XShmCreateImage(%s) failed.\n", name);
		X_UNLOCK;
		return 0;
	}

	*ximg_ptr = xim;

#if LIBVNCSERVER_HAVE_XSHM
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

	if (! XShmAttach_wr(dpy, shm)) {
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
#endif

	X_UNLOCK;
	return 1;
}

void shm_delete(XShmSegmentInfo *shm) {
#if LIBVNCSERVER_HAVE_XSHM
	if (shm != NULL && shm->shmaddr != (char *) -1) {
		shmdt(shm->shmaddr);
	}
	if (shm != NULL && shm->shmid != -1) {
		shmctl(shm->shmid, IPC_RMID, 0);
	}
#endif
}

void shm_clean(XShmSegmentInfo *shm, XImage *xim) {

	X_LOCK;
#if LIBVNCSERVER_HAVE_XSHM
	if (shm != NULL && shm->shmid != -1 && dpy) {	/* raw_fb hack */
		XShmDetach_wr(dpy, shm);
	}
#endif
	if (xim != NULL) {
		XDestroyImage(xim);
		xim = NULL;
	}
	X_UNLOCK;

	shm_delete(shm);
}

void initialize_polling_images(void) {
	int i, MB = 1024 * 1024;

	/* set all shm areas to "none" before trying to create any */
	scanline_shm.shmid	= -1;
	scanline_shm.shmaddr	= (char *) -1;
	scanline		= NULL;
	fullscreen_shm.shmid	= -1;
	fullscreen_shm.shmaddr	= (char *) -1;
	fullscreen		= NULL;
	snaprect_shm.shmid	= -1;
	snaprect_shm.shmaddr	= (char *) -1;
	snaprect		= NULL;
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
	if (UT.sysname && strstr(UT.sysname, "Linux")) {
		set_fs_factor(10 * MB);
	} else {
		set_fs_factor(1 * MB);
	}
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
	if (use_snapfb) {
		if (! fs_factor) {
			rfbLog("warning: disabling -snapfb mode.\n");
			use_snapfb = 0;
		} else if (! shm_create(&snaprect_shm, &snaprect, dpy_x,
		    dpy_y/fs_factor, "snaprect")) {
			clean_up_exit(1);
		} 
	}

	/*
	 * for copy_tiles we need a lot of shared memory areas, one for
	 * each possible run length of changed tiles.  32 for 1024x768
	 * and 40 for 1280x1024, etc. 
	 */

	tile_shm_count = 0;
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
			single_copytile_count = i;
			single_copytile = 1;
		}
		tile_shm_count++;
		if (single_copytile && i >= 1) {
			/* only need 1x1 tiles */
			break;
		}
	}
	if (!quiet) {
		if (using_shm) {
			rfbLog("created %d tile_row shm polling images.\n",
			    tile_shm_count);
		} else {
			rfbLog("created %d tile_row polling images.\n",
			    tile_shm_count);
		}
	}
}

/*
 * A hint is a rectangular region built from 1 or more adjacent tiles
 * glued together.  Ultimately, this information in a single hint is sent
 * to libvncserver rather than sending each tile separately.
 */
static void create_tile_hint(int x, int y, int tw, int th, hint_t *hint) {
	int w = dpy_x - x;
	int h = dpy_y - y;

	if (w > tw) {
		w = tw;
	}
	if (h > th) {
		h = th;
	}

	hint->x = x;
	hint->y = y;
	hint->w = w;
	hint->h = h;
}

static void extend_tile_hint(int x, int y, int tw, int th, hint_t *hint) {
	int w = dpy_x - x;
	int h = dpy_y - y;

	if (w > tw) {
		w = tw;
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
	int x, y, i, n, ty, th, tx, tw;
	int hint_count = 0, in_run = 0;

	for (y=0; y < ntiles_y; y++) {
		for (x=0; x < ntiles_x; x++) {
			n = x + y * ntiles_x;

			if (tile_has_diff[n]) {
				ty = tile_region[n].first_line;
				th = tile_region[n].last_line - ty + 1;

				tx = tile_region[n].first_x;
				tw = tile_region[n].last_x - tx + 1;
				if (tx < 0) {
					tx = 0;
					tw = tile_x;
				}

				if (! in_run) {
					create_tile_hint( x * tile_x + tx,
					    y * tile_y + ty, tw, th, &hint);
					in_run = 1;
				} else {
					extend_tile_hint( x * tile_x + tx,
					    y * tile_y + ty, tw, th, &hint);
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

void scale_rect(double factor, int blend, int interpolate, int Bpp,
    char *src_fb, int src_bytes_per_line, char *dst_fb, int dst_bytes_per_line,
    int Nx, int Ny, int nx, int ny, int X1, int Y1, int X2, int Y2, int mark) {
/*
 * Notation:
 * "i" an x pixel index in the destination (scaled) framebuffer
 * "j" a  y pixel index in the destination (scaled) framebuffer
 * "I" an x pixel index in the source (un-scaled, i.e. main) framebuffer
 * "J" a  y pixel index in the source (un-scaled, i.e. main) framebuffer
 *
 *  Similarly for nx, ny, Nx, Ny, etc.  Lowercase: dest, Uppercase: source.
 */
	int i, j, i1, i2, j1, j2;	/* indices for scaled fb (dest) */
	int I, J, I1, I2, J1, J2;	/* indices for main fb   (source) */

	double w, wx, wy, wtot;	/* pixel weights */

	double x1, y1, x2, y2;	/* x-y coords for destination pixels edges */
	double dx, dy;		/* size of destination pixel */
	double ddx, ddy;	/* for interpolation expansion */

	char *src, *dest;	/* pointers to the two framebuffers */


	unsigned short us;
	unsigned char  uc;
	unsigned int   ui;

	int use_noblend_shortcut = 1;
	int shrink;		/* whether shrinking or expanding */
	static int constant_weights = -1, mag_int = -1;
	static int last_Nx = -1, last_Ny = -1, cnt = 0;
	static double last_factor = -1.0;
	int b, k;
	double pixave[4];	/* for averaging pixel values */

	if (factor <= 1.0) {
		shrink = 1;
	} else {
		shrink = 0;
	}

	/*
	 * N.B. width and height (real numbers) of a scaled pixel.
	 * both are > 1   (e.g. 1.333 for -scale 3/4)
	 * they should also be equal but we don't assume it.
	 *
	 * This new way is probably the best we can do, take the inverse
	 * of the scaling factor to double precision.
	 */
	dx = 1.0/factor;
	dy = 1.0/factor;

	/*
	 * There is some speedup if the pixel weights are constant, so
	 * let's special case these.
	 *
	 * If scale = 1/n and n divides Nx and Ny, the pixel weights
	 * are constant (e.g. 1/2 => equal on 2x2 square).
	 */
	if (factor != last_factor || Nx != last_Nx || Ny != last_Ny) {
		constant_weights = -1;
		mag_int = -1;
		last_Nx = Nx;
		last_Ny = Ny;
		last_factor = factor;
	}

	if (constant_weights < 0) {
		int n = 0;

		constant_weights = 0;
		mag_int = 0;

		for (i = 2; i<=128; i++) {
			double test = ((double) 1)/ i;
			double diff, eps = 1.0e-7;
			diff = factor - test;
			if (-eps < diff && diff < eps) {
				n = i;
				break;
			}
		}
		if (! blend || ! shrink || interpolate) {
			;
		} else if (n != 0) {
			if (Nx % n == 0 && Ny % n == 0) {
				static int didmsg = 0;
				if (mark && ! didmsg) {
					didmsg = 1;
					rfbLog("scale_and_mark_rect: using "
					    "constant pixel weight speedup "
					    "for 1/%d\n", n);
				}
				constant_weights = 1;
			}
		}

		n = 0;
		for (i = 2; i<=32; i++) {
			double test = (double) i;
			double diff, eps = 1.0e-7;
			diff = factor - test;
			if (-eps < diff && diff < eps) {
				n = i;
				break;
			}
		}
		if (! blend && factor > 1.0 && n) {
			mag_int = n;
		}
	}

	if (mark && factor > 1.0 && blend) {
		/*
		 * kludge: correct for interpolating blurring leaking
		 * up or left 1 destination pixel.
		 */
		if (X1 > 0) X1--;
		if (Y1 > 0) Y1--;
	}

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

	/* special case integer magnification with no blending */
	if (mark && ! blend && mag_int && Bpp != 3) {
		int jmin, jmax, imin, imax;

		/* outer loop over *source* pixels */
		for (J=Y1; J < Y2; J++) {
		    jmin = J * mag_int;
		    jmax = jmin + mag_int;
		    for (I=X1; I < X2; I++) {
			/* extract value */
			src = src_fb + J*src_bytes_per_line + I*Bpp;
			if (Bpp == 4) {
				ui = *((unsigned int *)src);
			} else if (Bpp == 2) {
				us = *((unsigned short *)src);
			} else if (Bpp == 1) {
				uc = *((unsigned char *)src);
			}
			imin = I * mag_int;
			imax = imin + mag_int;
			/* inner loop over *dest* pixels */
			for (j=jmin; j<jmax; j++) {
			    dest = dst_fb + j*dst_bytes_per_line + imin*Bpp;
			    for (i=imin; i<imax; i++) {
				if (Bpp == 4) {
					*((unsigned int *)dest) = ui;
				} else if (Bpp == 2) {
					*((unsigned short *)dest) = us;
				} else if (Bpp == 1) {
					*((unsigned char *)dest) = uc;
				}
				dest += Bpp;
			    }
			}
		    }
		}
		goto markit;
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

		if (shrink && ! interpolate) {
			J2 = (int) CEIL(y2) - 1;
			J2 = nfix(J2, Ny);
		} else {
			J2 = J1 + 1;	/* simple interpolation */
			ddy = y1 - J1;
		}

		/* destination char* pointer: */
		dest = dst_fb + j*dst_bytes_per_line + i1*Bpp;
		
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

			if (! blend && use_noblend_shortcut) {
				/*
				 * The noblend case involves no weights,
				 * and 1 pixel, so just copy the value
				 * directly.
				 */
				src = src_fb + J1*src_bytes_per_line + I1*Bpp;
				if (Bpp == 4) {
					*((unsigned int *)dest)
					    = *((unsigned int *)src);
				} else if (Bpp == 2) {
					*((unsigned short *)dest)
					    = *((unsigned short *)src);
				} else if (Bpp == 1) {
					*(dest) = *(src);
				} else if (Bpp == 3) {
					/* rare case */
					for (k=0; k<=2; k++) {
						*(dest+k) = *(src+k);
					}
				}
				dest += Bpp;
				continue;
			}
			
			if (shrink && ! interpolate) {
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
			 *
			 * Typical case when shrinking are 2x2 loop, so
			 * just two lines to worry about.
			 */
			for (J=J1; J<=J2; J++) {
			    /* see comments for I, x1, x2, etc. below */
			    if (constant_weights) {
				;
			    } else if (! blend) {
				if (J != J1) {
					continue;
				}
				wy = 1.0;

				/* interpolation scheme: */
			    } else if (! shrink || interpolate) {
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

			    src = src_fb + J*src_bytes_per_line + I1*Bpp;

			    for (I=I1; I<=I2; I++) {

				/* Work out the weight: */

				if (constant_weights) {
					;
				} else if (! blend) {
					/*
					 * Ugh, PseudoColor colormap is
					 * bad news, to avoid random
					 * colors just take the first
					 * pixel.  Or user may have
					 * specified :nb to fraction.
					 * The :fb will force blending
					 * for this case.
					 */
					if (I != I1) {
						continue;
					}
					wx = 1.0;

					/* interpolation scheme: */
				} else if (! shrink || interpolate) {
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
				if (Bpp == 4) {
					/* unroll the loops, can give 20% */
					pixave[0] += w *
					    ((unsigned char) *(src  ));
					pixave[1] += w *
					    ((unsigned char) *(src+1));
					pixave[2] += w *
					    ((unsigned char) *(src+2));
					pixave[3] += w *
					    ((unsigned char) *(src+3));
				} else if (Bpp == 2) {
					/*
					 * 16bpp: trickier with green
					 * split over two bytes, so we
					 * use the masks:
					 */
					us = *((unsigned short *) src);
					pixave[0] += w*(us & main_red_mask);
					pixave[1] += w*(us & main_green_mask);
					pixave[2] += w*(us & main_blue_mask);
				} else if (Bpp == 1) {
					pixave[0] += w *
					    ((unsigned char) *(src));
				} else {
					for (b=0; b<Bpp; b++) {
						pixave[b] += w *
						    ((unsigned char) *(src+b));
					}
				}
				src += Bpp;
			    }
			}

			if (wtot <= 0.0) {
				wtot = 1.0;
			}
			wtot = 1.0/wtot;	/* normalization factor */

			/* place weighted average pixel in the scaled fb: */
			if (Bpp == 4) {
				*(dest  ) = (char) (wtot * pixave[0]);
				*(dest+1) = (char) (wtot * pixave[1]);
				*(dest+2) = (char) (wtot * pixave[2]);
				*(dest+3) = (char) (wtot * pixave[3]);
			} else if (Bpp == 2) {
				/* 16bpp / 565 case: */
				pixave[0] *= wtot;
				pixave[1] *= wtot;
				pixave[2] *= wtot;
				us =  (main_red_mask   & (int) pixave[0])
				    | (main_green_mask & (int) pixave[1])
				    | (main_blue_mask  & (int) pixave[2]);
				*( (unsigned short *) dest ) = us;
			} else if (Bpp == 1) {
				*(dest) = (char) (wtot * pixave[0]);
			} else {
				for (b=0; b<Bpp; b++) {
					*(dest+b) = (char) (wtot * pixave[b]);
				}
			}
			dest += Bpp;
		}
	}
	markit:
	if (mark) {
		mark_rect_as_modified(i1, j1, i2, j2, 1);
	}
}

static void scale_and_mark_rect(int X1, int Y1, int X2, int Y2) {

	if (!screen || !rfb_fb || !main_fb) {
		return;
	}
	if (! screen->serverFormat.trueColour) {
		/*
		 * PseudoColor colormap... blending leads to random colors.
		 * User can override with ":fb"
		 */
		if (scaling_blend == 1) {
			/* :fb option sets it to 2 */
			if (default_visual->class == StaticGray) {
				/*
				 * StaticGray can be blended OK, otherwise
				 * user can disable with :nb
				 */
				;
			} else {
				scaling_blend = 0;
			}
		}
	}

	scale_rect(scale_fac, scaling_blend, scaling_interpolate, bpp/8,
	    main_fb, main_bytes_per_line, rfb_fb, rfb_bytes_per_line,
	    dpy_x, dpy_y, scaled_x, scaled_y, X1, Y1, X2, Y2, 1);
}

void mark_rect_as_modified(int x1, int y1, int x2, int y2, int force) {

	if (damage_time != 0) {
		/*
		 * This is not XDAMAGE, rather a hack for testing
		 * where we allow the framebuffer to be corrupted for
		 * damage_delay seconds.
		 */
		int debug = 0;
		if (time(0) > damage_time + damage_delay) {
			if (! quiet) {
				rfbLog("damaging turned off.\n");
			}
			damage_time = 0;
			damage_delay = 0;
		} else {
			if (debug) {
				rfbLog("damaging viewer fb by not marking "
				    "rect: %d,%d,%d,%d\n", x1, y1, x2, y2);
			}
			return;
		}
	}

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
 * due to contiguous memory access.  Not a great speedup, but in some
 * cases it can be up to 2X.  Even more on a SunRay or ShadowFB where
 * no graphics hardware is involved in the read.  Generally, graphics
 * devices are optimized for write, not read, so we are limited by the
 * read bandwidth, sometimes only 5 MB/sec on otherwise fast hardware.
 */
static int *first_line = NULL, *last_line;
static unsigned short *left_diff, *right_diff;

static int copy_tiles(int tx, int ty, int nt) {
	int x, y, line;
	int size_x, size_y, width1, width2;
	int off, len, n, dw, dx, t;
	int w1, w2, dx1, dx2;	/* tmps for normal and short tiles */
	int pixelsize = bpp/8;
	int first_min, last_max;
	int first_x = -1, last_x = -1;

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
		 * n.b. we are in single copy_tile mode: nt=1
		 */
		tile_has_diff[n] = 0;
		return(0);
	}

	X_LOCK;
	XRANDR_SET_TRAP_RET(-1, "copy_tile-set");
	/* read in the whole tile run at once: */
	copy_image(tile_row[nt], x, y, size_x, size_y);
	XRANDR_CHK_TRAP_RET(-1, "copy_tile-chk");

	X_UNLOCK;

	if (blackouts && tile_blackout[n].cover == 1) {
		/*
		 * If there are blackouts and this tile is partially covered
		 * we should re-black-out the portion.
		 * n.b. we are in single copy_tile mode: nt=1
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
		return(0);
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
		if (nt == 1) {
			/*
			 * optimization for tall skinny lines, e.g. wm
			 * frame. try to find first_x and last_x to limit
			 * the size of the hint.  could help for a slow
			 * link.  Unfortunately we spent a lot of time
			 * reading in the many tiles.
			 *
			 * BTW, we like to think the above memcpy leaves
			 * the data we use below in the cache... (but
			 * it could be two 128 byte segments at 32bpp)
			 * so this inner loop is not as bad as it seems.
			 */
			int k, kx;
			kx = pixelsize;
			for (k=0; k<size_x; k++) {
				if (memcmp(s_dst + k*kx, s_src + k*kx, kx))  {
					if (first_x == -1 || k < first_x) {
						first_x = k;
					}
					if (last_x == -1 || k > last_x) {
						last_x = k;
					}
				}
			}
		}
		s_src += tile_row[nt]->bytes_per_line;
		s_dst += main_bytes_per_line;
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

		tile_region[n+s].first_x = first_x;
		tile_region[n+s].last_x  = last_x;

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

		tile_copied[n+s] = 1;
	}

	return(1);
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
	int diffs = 0, ct;

	for (y=0; y < ntiles_y; y++) {
		for (x=0; x < ntiles_x; x++) {
			n = x + y * ntiles_x;

			if (tile_has_diff[n]) {
				ct = copy_tiles(x, y, 1);
				if (ct < 0) return ct;	/* fatal */
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
	int diffs = 0, ct;
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
				ct = copy_tiles(x - run, y, run);
				if (ct < 0) return ct;	/* fatal */

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
		/*
		 * Could some activity go here, to emulate threaded
		 * behavior by servicing some libvncserver tasks?
		 */
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
	int diffs = 0, ct;

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
				ct = copy_tiles(x, y-1, 1);
				if (ct < 0) return ct;	/* fatal */
			}
		}

		m = (x-1) + y * ntiles_x;	/* neighboring tile to left */

		if (x >= 1 && ! tile_has_diff[m] && tile_region[n].left_diff) {
			if (! tile_tried[m]) {
				tile_has_diff[m] = 2;
				ct = copy_tiles(x-1, y, 1);
				if (ct < 0) return ct;	/* fatal */
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

static int copy_tiles_additional_pass(void) {
	int x, y, n;
	int diffs = 0, ct;

	for (y=0; y < ntiles_y; y++) {
		for (x=0; x < ntiles_x; x++) {
			n = x + y * ntiles_x;		/* number of this tile */

			if (! tile_has_diff[n]) {
				continue;
			}
			if (tile_copied[n]) {
				continue;
			}

			ct = copy_tiles(x, y, 1);
			if (ct < 0) return ct;	/* fatal */
		}
	}
	for (n=0; n < ntiles; n++) {
		if (tile_has_diff[n]) {
			diffs++;
		}
	}
	return diffs;
}

static int gap_try(int x, int y, int *run, int *saw, int along_x) {
	int n, m, i, xt, yt, ct;

	n = x + y * ntiles_x;

	if (! tile_has_diff[n]) {
		if (*saw) {
			(*run)++;	/* extend the gap run. */
		}
		return 0;
	}
	if (! *saw || *run == 0 || *run > gaps_fill) {
		*run = 0;		/* unacceptable run. */
		*saw = 1;
		return 0;
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

		ct = copy_tiles(xt, yt, 1);
		if (ct < 0) return ct;	/* fatal */
	}
	*run = 0;
	*saw = 1;
	return 1;
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
	int n, diffs = 0, ct;

	/* horizontal: */
	for (y=0; y < ntiles_y; y++) {
		run = 0;
		saw = 0;
		for (x=0; x < ntiles_x; x++) {
			ct = gap_try(x, y, &run, &saw, 1);
			if (ct < 0) return ct;	/* fatal */
		}
	}

	/* vertical: */
	for (x=0; x < ntiles_x; x++) {
		run = 0;
		saw = 0;
		for (y=0; y < ntiles_y; y++) {
			ct = gap_try(x, y, &run, &saw, 0);
			if (ct < 0) return ct;	/* fatal */
		}
	}

	for (n=0; n < ntiles; n++) {
		if (tile_has_diff[n]) {
			diffs++;
		}
	}
	return diffs;
}

static int island_try(int x, int y, int u, int v, int *run) {
	int n, m, ct;

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
			return 0;
		} else if (*run < grow_fill) {
			return 0;
		}

		ct = copy_tiles(u, v, 1);
		if (ct < 0) return ct;	/* fatal */
	}
	return 1;
}

/*
 * Scan looking for discontinuities in tile_has_diff[].  Try to extend
 * the boundary of the discontinuity (i.e. make the island larger).
 * Vertical scans are skipped since they do not seem to yield much...
 */
static int grow_islands(void) {
	int x, y, n, run;
	int diffs = 0, ct;

	/*
	 * n.b. the way we scan here should keep an extension going,
	 * and so also fill in gaps effectively...
	 */

	/* left to right: */
	for (y=0; y < ntiles_y; y++) {
		run = 0;
		for (x=0; x <= ntiles_x - 2; x++) {
			ct = island_try(x, y, x+1, y, &run);
			if (ct < 0) return ct;	/* fatal */
		}
	}
	/* right to left: */
	for (y=0; y < ntiles_y; y++) {
		run = 0;
		for (x = ntiles_x - 1; x >= 1; x--) {
			ct = island_try(x, y, x-1, y, &run);
			if (ct < 0) return ct;	/* fatal */
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
int copy_screen(void) {
	int pixelsize = bpp/8;
	char *fbp;
	int i, y, block_size;

	if (! fs_factor) {
		return 0;
	}

	block_size = (dpy_x * (dpy_y/fs_factor) * pixelsize);

	if (! main_fb) {
		return 0;
	}
	fbp = main_fb;
	y = 0;

	X_LOCK;

	/* screen may be too big for 1 shm area, so broken into fs_factor */
	for (i=0; i < fs_factor; i++) {
		XRANDR_SET_TRAP_RET(-1, "copy_screen-set");
		copy_image(fullscreen, 0, y, 0, 0);
		XRANDR_CHK_TRAP_RET(-1, "copy_screen-chk");

		memcpy(fbp, fullscreen->data, (size_t) block_size);

		y += dpy_y / fs_factor;
		fbp += block_size;
	}

	X_UNLOCK;

	if (blackouts) {
		blackout_regions();
	}

	mark_rect_as_modified(0, 0, dpy_x, dpy_y, 0);
	return 0;
}

int copy_snap(void) {
	int pixelsize = bpp/8;
	char *fbp;
	int i, y, block_size;
	double dt;
	static int first = 1;

	if (! fs_factor) {
		return 0;
	}

	block_size = (dpy_x * (dpy_y/fs_factor) * pixelsize);

	if (! snap_fb || ! snap || ! snaprect) {
		return 0;
	}
	fbp = snap_fb;
	y = 0;

	dtime0(&dt);
	X_LOCK;

	/* screen may be too big for 1 shm area, so broken into fs_factor */
	for (i=0; i < fs_factor; i++) {
		XRANDR_SET_TRAP_RET(-1, "copy_snap-set");
		copy_image(snaprect, 0, y, 0, 0);
		XRANDR_CHK_TRAP_RET(-1, "copy_snap-chk");

		memcpy(fbp, snaprect->data, (size_t) block_size);

		y += dpy_y / fs_factor;
		fbp += block_size;
	}

	X_UNLOCK;
	dt = dtime(&dt);
	if (first) {
		rfbLog("copy_snap: time for -snapfb snapshot: %.3f sec\n", dt);
		first = 0;
	}

	return 0;
}


/*
 * Utilities for managing the "naps" to cut down on amount of polling.
 */
static void nap_set(int tile_cnt) {
	int nap_in = nap_ok;

	if (scan_count == 0) {
		/* roll up check for all NSCAN scans */
		nap_ok = 0;
		if (naptile && nap_diff_count < 2 * NSCAN * naptile) {
			/* "2" is a fudge to permit a bit of bg drawing */
			nap_ok = 1;
		}
		nap_diff_count = 0;
	}
	if (nap_ok && ! nap_in && use_xdamage) {
		if (XD_skip > 0.8 * XD_tot) 	{
			/* X DAMAGE is keeping load low, so skip nap */
			nap_ok = 0;
		}
	}

	if (show_cursor) {
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
			rfbPE(-1);
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
		rfbLog("reset rfbMaxClientWait to %d msec.\n",
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
	xtranslate(window, rootwin, 0, 0, &off_x, &off_y, &w, 0);
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
	int pixelsize = bpp/8;
	int x, y, w, n;
	int tile_count = 0;
	int nodiffs = 0, diff_hint;

	y = ystart;

	if (! main_fb) {
		rfbLog("scan_display: no main_fb!\n");
		return 0;
	}

	while (y < dpy_y) {

		if (use_xdamage) {
			XD_tot++;
			if (xdamage_hint_skip(y)) {
				XD_skip++;
				y += NSCAN;
				continue;
			}
		}

		/* grab the horizontal scanline from the display: */
		X_LOCK;
		XRANDR_SET_TRAP_RET(-1, "scan_display-set");
		copy_image(scanline, 0, y, 0, 0);
		XRANDR_CHK_TRAP_RET(-1, "scan_display-chk");
		X_UNLOCK;

		/* for better memory i/o try the whole line at once */
		src = scanline->data;
		dst = main_fb + y * main_bytes_per_line;

		if (! memcmp(dst, src, main_bytes_per_line)) {
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
			diff_hint = 0;

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
			} else if (xdamage_tile_count &&
			    tile_has_xdamage_diff[n]) {
				tile_has_xdamage_diff[n] = 2;
				diff_hint = 1;
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

			if (diff_hint || memcmp(dst, src, w * pixelsize)) {
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
 * returns number of changed tiles.
 */
int scan_for_updates(int count_only) {
	int i, tile_count, tile_diffs;
	int old_copy_tile;
	double frac1 = 0.1;   /* tweak parameter to try a 2nd scan_display() */
	double frac2 = 0.35;  /* or 3rd */
	double frac3 = 0.02;  /* do scan_display() again after copy_tiles() */
	for (i=0; i < ntiles; i++) {
		tile_has_diff[i] = 0;
		tile_has_xdamage_diff[i] = 0;
		tile_tried[i] = 0;
		tile_copied[i] = 0;
	}
	for (i=0; i < ntiles_y; i++) {
		/* could be useful, currently not used */
		tile_row_has_xdamage_diff[i] = 0;
	}
	xdamage_tile_count = 0;

	/*
	 * n.b. this program has only been tested so far with
	 * tile_x = tile_y = NSCAN = 32!
	 */

	if (!count_only) {
		scan_count++;
		scan_count %= NSCAN;

		/* some periodic maintenance */
		if (subwin) {
			set_offset();	/* follow the subwindow */
		}
		if (indexed_color && scan_count % 4 == 0) {
			/* check for changed colormap */
			set_colormap(0);
		}
		if (use_xdamage) {
			/* first pass collecting DAMAGE events: */
			collect_xdamage(scan_count, 0);
		}
	}

#define SCAN_FATAL(x) \
	if (x < 0) { \
		scan_in_progress = 0; \
		fb_copy_in_progress = 0; \
		return 0; \
	}

	/* scan with the initial y to the jitter value from scanlines: */
	scan_in_progress = 1;
	tile_count = scan_display(scanlines[scan_count], 0);
	SCAN_FATAL(tile_count);

	/*
	 * we do the XDAMAGE here too since after scan_display()
	 * there is a better chance we have received the events from
	 * the X server (otherwise the DAMAGE events will be processed
	 * in the *next* call, usually too late and wasteful since
	 * the unchanged tiles are read in again).
	 */
	if (use_xdamage) {
		collect_xdamage(scan_count, 1);
	}
	if (count_only) {
		scan_in_progress = 0;
		fb_copy_in_progress = 0;
		return tile_count;
	}

	if (xdamage_tile_count) {
		/* pick up "known" damaged tiles we missed in scan_display() */
		for (i=0; i < ntiles; i++) {
			if (tile_has_diff[i]) {
				continue;
			}
			if (tile_has_xdamage_diff[i]) {
				tile_has_diff[i] = 1;
				if (tile_has_xdamage_diff[i] == 1) {
					tile_has_xdamage_diff[i] = 2;
					tile_count++;
				}
			}
		}
	}

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
			SCAN_FATAL(tile_count);

			if (tile_count >= (1 + frac2) * tile_count_old) {
				/* on a roll... do a 3rd scan */
				cp = (NSCAN - scan_count + 7) % NSCAN;
				tile_count = scan_display(scanlines[cp], 1);
				SCAN_FATAL(tile_count);
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
			int cs;
			fb_copy_in_progress = 1;
			cs = copy_screen();
			fb_copy_in_progress = 0;
			SCAN_FATAL(cs);
			if (use_threads && pointer_mode != 1) {
				pointer(-1, 0, 0, NULL);
			}
			nap_check(tile_count);
			return tile_count;
		}
	}
	scan_in_progress = 0;

	/* copy all tiles with differences from display to rfb framebuffer: */
	fb_copy_in_progress = 1;

	if (single_copytile || tile_shm_count < ntiles_x) {
		/*
		 * Old way, copy I/O one tile at a time.
		 */
		old_copy_tile = 1;
	} else {
		/* 
		 * New way, does runs of horizontal tiles at once.
		 * Note that below, for simplicity, the extra tile finding
		 * (e.g. copy_tiles_backward_pass) is done the old way.
		 */
		old_copy_tile = 0;
	}
	if (old_copy_tile) {
		tile_diffs = copy_all_tiles();
	} else {
		tile_diffs = copy_all_tile_runs();
	}
	SCAN_FATAL(tile_diffs);

	/*
	 * This backward pass for upward and left tiles complements what
	 * was done in copy_all_tiles() for downward and right tiles.
	 */
	tile_diffs = copy_tiles_backward_pass();
	SCAN_FATAL(tile_diffs);

	if (tile_diffs > frac3 * ntiles) {
		/*
		 * we spent a lot of time in those copy_tiles, run
		 * another scan, maybe more of the screen changed.
		 */
		int cp = (NSCAN - scan_count + 13) % NSCAN;

		scan_in_progress = 1;
		tile_count = scan_display(scanlines[cp], 1);
		SCAN_FATAL(tile_count);
		scan_in_progress = 0;

		tile_diffs = copy_tiles_additional_pass();
		SCAN_FATAL(tile_diffs);
	}

	/* Given enough tile diffs, try the islands: */
	if (grow_fill && tile_diffs > 4) {
		tile_diffs = grow_islands();
	}
	SCAN_FATAL(tile_diffs);

	/* Given enough tile diffs, try the gaps: */
	if (gaps_fill && tile_diffs > 4) {
		tile_diffs = fill_tile_gaps();
	}
	SCAN_FATAL(tile_diffs);

	fb_copy_in_progress = 0;
	if (use_threads && pointer_mode != 1) {
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

	hint_updates();	/* use x0rfbserver hints algorithm */

	/* Work around threaded rfbProcessClientMessage() calls timeouts */
	if (use_threads) {
		ping_clients(tile_diffs);
	}


	nap_check(tile_diffs);
	return tile_diffs;
}

/* -- gui.c -- */
#if SMALL_FOOTPRINT
char gui_code[] = "";
#else
#include "tkx11vnc.h"
#endif

void run_gui(char *gui_xdisplay, int connect_to_x11vnc, int simple_gui,
    pid_t parent) {
	char *x11vnc_xdisplay = NULL;
	char extra_path[] = ":/usr/local/bin:/usr/bin/X11:/usr/sfw/bin"
	    ":/usr/X11R6/bin:/usr/openwin/bin:/usr/dt/bin";
	char cmd[100];
	char *wish = NULL, *orig_path, *full_path, *tpath, *p;
	char *old_xauth = NULL;
	int try_max = 4, sleep = 300;
	pid_t mypid = getpid();
	FILE *pipe, *tmpf;

	if (*gui_code == '\0') {
		rfbLog("gui not available in this program.\n");
		exit(0);
	}
	if (getenv("DISPLAY") != NULL) {
		/* worst case */
		x11vnc_xdisplay = strdup(getenv("DISPLAY"));
	}
	if (use_dpy) {
		/* better */
		x11vnc_xdisplay = strdup(use_dpy);
	}
	if (connect_to_x11vnc) {
		int rc, i;
		rfbLogEnable(0);
		if (! client_connect_file) {
			if (getenv("XAUTHORITY") != NULL) {
				old_xauth = strdup(getenv("XAUTHORITY"));
			} else {
				old_xauth = strdup("");
			}
			dpy = XOpenDisplay(x11vnc_xdisplay); 
			if (! dpy && auth_file) {
				set_env("XAUTHORITY", auth_file);
				dpy = XOpenDisplay(x11vnc_xdisplay);
			}
			if (! dpy && ! x11vnc_xdisplay) {
				/* worstest case */
				x11vnc_xdisplay = strdup(":0");
				dpy = XOpenDisplay(x11vnc_xdisplay); 
			}
			if (! dpy) {
				fprintf(stderr, "gui: could not open x11vnc "
				    "display: %s\n", NONUL(x11vnc_xdisplay));
				exit(1);
			}
			scr = DefaultScreen(dpy);
			rootwin = RootWindow(dpy, scr);
			initialize_vnc_connect_prop();
		}
		usleep(1000*1000);
		fprintf(stderr, "\n");
		for (i=0; i<try_max; i++) {
			usleep(sleep*1000);
			fprintf(stderr, "gui: pinging %s try=%d ...\n",
			    NONUL(x11vnc_xdisplay), i+1);
			rc = send_remote_cmd("qry=ping", 1, 1);
			if (rc == 0) {
				break;
			}
			if (parent && mypid != parent && kill(parent, 0) != 0) {
				fprintf(stderr, "gui: parent process %d has gone"
				    " away: bailing out.\n", parent);
				rc = 1;
				break;
			}
		}
		set_env("X11VNC_XDISPLAY", x11vnc_xdisplay);
		if (getenv("XAUTHORITY") != NULL) {
			set_env("X11VNC_AUTH_FILE", getenv("XAUTHORITY"));
		}
		if (rc == 0) {
			fprintf(stderr, "gui: ping succeeded.\n");
			set_env("X11VNC_CONNECT", "1");
		} else {
			fprintf(stderr, "gui: could not connect to: '%s', try"
			    " again manually.\n", x11vnc_xdisplay);
		}
		if (client_connect_file) {
			set_env("X11VNC_CONNECT_FILE", client_connect_file);
		}
		if (dpy)  {
			XCloseDisplay(dpy);
			dpy = NULL;
		}
		if (old_xauth) {
			if (*old_xauth == '\0') {
				/* wasn't set, hack it out if it is now */
				char *xauth = getenv("XAUTHORITY");
				if (xauth) {
					*(xauth-2) = '_';	/* yow */
				}
			} else {
				set_env("XAUTHORITY", old_xauth);
			}
			free(old_xauth);
		}
	}

	orig_path = getenv("PATH");
	if (! orig_path) {
		orig_path = strdup("/bin:/usr/bin:/usr/bin/X11");
	}
	full_path = (char *) malloc(strlen(orig_path)+strlen(extra_path)+1);
	strcpy(full_path, orig_path);
	strcat(full_path, extra_path);

	tpath = strdup(full_path);
	p = strtok(tpath, ":");

	while (p) {
		char *try;
		struct stat sbuf;
		char *wishes[] = {"wish", "wish8.3", "wish8.4", "wish8.5",
		    "wish8.0"};
		int nwishes = 3, i;

		try = (char *)malloc(strlen(p) + 1 + strlen("wish8.4") + 1);
		for (i=0; i<nwishes; i++) {
			sprintf(try, "%s/%s", p, wishes[i]);
			if (stat(try, &sbuf) == 0) {
				/* assume executable, should check mode */
				wish = wishes[i];
			}
		}
		free(try);
		if (wish) {
			break;
		}
		p = strtok(NULL, ":");
	}
	free(tpath);
	if (!wish) {
		wish = strdup("wish");
	}
	set_env("PATH", full_path);
	set_env("DISPLAY", gui_xdisplay);
	set_env("X11VNC_PROG", program_name);
	set_env("X11VNC_CMDLINE", program_cmdline);
	if (simple_gui) {
		set_env("X11VNC_SIMPLE_GUI", "1");
	}

	if (no_external_cmds) {
		fprintf(stderr, "cannot run external commands in -nocmds "
		    "mode:\n");
		fprintf(stderr, "   \"%s\"\n", "gui + wish");
		fprintf(stderr, "   exiting.\n");
		fflush(stderr);
		exit(1);
	}

	sprintf(cmd, "%s -", wish);
	tmpf = tmpfile();
	if (tmpf == NULL) {
		/* if no tmpfile, use a pipe */
		pipe = popen(cmd, "w");
		if (! pipe) {
			fprintf(stderr, "could not run: %s\n", cmd);
			perror("popen");
		}
		fprintf(pipe, "%s", gui_code);
		pclose(pipe);
	} else {
		/*
		 * we prefer a tmpfile since then this x11vnc process
		 * will then be gone, otherwise the x11vnc program text
		 * will still be in use.
		 */
		int n = fileno(tmpf);
		fprintf(tmpf, "%s", gui_code);
		fflush(tmpf);
		rewind(tmpf);
		dup2(n, 0);
		close(n);
		execlp(wish, wish, "-", (char *) NULL); 
		fprintf(stderr, "could not exec wish: %s -\n", wish);
		perror("execlp");
	}
	exit(0);
}

void do_gui(char *opts) {
	char *s, *p;
	char *old_xauth = NULL;
	char *gui_xdisplay = NULL;
	int start_x11vnc = 1;
	int connect_to_x11vnc = 0;
	int simple_gui = 0;
	Display *test_dpy;

	if (opts) {
		s = strdup(opts);
	} else {
		s = strdup("");
	}

	if (use_dpy) {
		/* worst case */
		gui_xdisplay = strdup(use_dpy);
		
	}
	if (getenv("DISPLAY") != NULL) {
		/* better */
		gui_xdisplay = strdup(getenv("DISPLAY"));
	}

	p = strtok(s, ",");

	while(p) {
		if (*p == '\0') {
			;
		} else if (strchr(p, ':') != NULL) {
			/* best */
			gui_xdisplay = strdup(p);
		} else if (!strcmp(p, "wait")) {
			start_x11vnc = 0;
			connect_to_x11vnc = 0;
		} else if (!strcmp(p, "conn") || !strcmp(p, "connect")) {
			start_x11vnc = 0;
			connect_to_x11vnc = 1;
		} else if (!strcmp(p, "ez") || !strcmp(p, "simple")) {
			simple_gui = 1;
		} else {
			fprintf(stderr, "unrecognized gui opt: %s\n", p);
		}
		
		p = strtok(NULL, ",");
	}
	free(s);
	if (start_x11vnc) {
		connect_to_x11vnc = 1;
	}

	if (! gui_xdisplay) {
		fprintf(stderr, "error: cannot determine X DISPLAY for gui"
		    " to display on.\n");
		exit(1);
	}
	test_dpy = XOpenDisplay(gui_xdisplay);
	if (! test_dpy && auth_file) {
		if (getenv("XAUTHORITY") != NULL) {
			old_xauth = strdup(getenv("XAUTHORITY"));
		}
		set_env("XAUTHORITY", auth_file);
		test_dpy = XOpenDisplay(gui_xdisplay);
	}
	if (! test_dpy) {
		if (! old_xauth && getenv("XAUTHORITY") != NULL) {
			old_xauth = strdup(getenv("XAUTHORITY"));
		}
		set_env("XAUTHORITY", "");
		test_dpy = XOpenDisplay(gui_xdisplay);
	}
	if (! test_dpy) {
		fprintf(stderr, "error: cannot connect to gui X DISPLAY: %s\n",
		    gui_xdisplay);
		exit(1);
	}
	XCloseDisplay(test_dpy);

	if (start_x11vnc) {
#if LIBVNCSERVER_HAVE_FORK
		/* fork into the background now */
		int p;
		pid_t parent = getpid();
		if ((p = fork()) > 0)  {
			;	/* parent */
		} else if (p == -1) {
			fprintf(stderr, "could not fork\n");
			perror("fork");
			clean_up_exit(1);
		} else {
			run_gui(gui_xdisplay, connect_to_x11vnc, simple_gui,
			    parent);
			exit(1);
		}
#else
		fprintf(stderr, "system does not support fork: start "
		    "x11vnc in the gui.\n");
		start_x11vnc = 0;
#endif
	}
	if (!start_x11vnc) {
		run_gui(gui_xdisplay, connect_to_x11vnc, simple_gui, 0);
		exit(1);
	}
	if (old_xauth) {
		set_env("XAUTHORITY", old_xauth);
	}
}

/* -- userinput.c -- */
/*
 * user input handling heuristics
 */

Window descend_pointer(int depth, Window start, char *name_info, int len) {
	Window r, c, clast;
	int i, rx, ry, wx, wy;
	int written = 0, filled = 0;
	char *store = NULL;
	unsigned int m;
	static XClassHint *classhint = NULL;
	static char *nm_cache = NULL;
	static int nm_cache_len = 0;
	static Window prev_start = None;

	if (! classhint) {
		classhint = XAllocClassHint();
	}

	if (! nm_cache) {
		nm_cache = (char *)malloc(1024);
		nm_cache_len = 1024;
		nm_cache[0] = '\0';
	}
	if (name_info && nm_cache_len < len) {
		if (nm_cache) {
			free(nm_cache);
		}
		nm_cache_len = 2*len;
		nm_cache = (char *)malloc(nm_cache_len);
	}

	if (name_info) {
		if (start != None && start == prev_start) {
			store = NULL;
			strncpy(name_info, nm_cache, len);
		} else {
			store = name_info;
			name_info[0] = '\0';
		}
	}

	if (start != None) {
		c = start;
		if (name_info) {
			prev_start = start;
		}
	} else {
		c = rootwin;	
	}

	for (i=0; i<depth; i++) {
		clast = c;
		if (store && ! filled) {
			char *name;
			if (XFetchName(dpy, clast, &name) && name != NULL) {
				int l = strlen(name);
				if (written + l+2 < len) {
					strcat(store, "^^");
					written += 2;
					strcat(store, name);
					written += l;
				} else {
					filled = 1;
				}
				XFree(name);
			}
		}
		if (store && classhint && ! filled) {
			classhint->res_name = NULL;
			classhint->res_class = NULL;
			if (XGetClassHint(dpy, clast, classhint)) {
				int l = 0;
				if (classhint->res_class) {
					l += strlen(classhint->res_class); 
				}
				if (classhint->res_name) {
					l += strlen(classhint->res_name); 
				}
				if (written + l+4 < len) {
					strcat(store, "##");
					if (classhint->res_class) {
						strcat(store,
						    classhint->res_class);
					}
					strcat(store, "++");
					if (classhint->res_name) {
						strcat(store,
						    classhint->res_name);
					}
					written += l+4;
				} else {
					filled = 1;
				}
				if (classhint->res_class) {
					XFree(classhint->res_class);
				}
				if (classhint->res_name) {
					XFree(classhint->res_name);
				}
			}
		}
		if (! XQueryPointer(dpy, c, &r, &c, &rx, &ry, &wx, &wy, &m)) {
			break;
		}
		if (! c) {
			break;
		}
	}
	if (start != None && name_info) {
		strncpy(nm_cache, name_info, nm_cache_len);
	}

	return clast;
}

/*
 * For -wireframe: find the direct child of rootwin that has the
 * pointer, assume that is the WM frame that contains the application
 * (i.e. wm reparents the app toplevel) return frame position and size
 * if successful.
 */
int get_wm_frame_pos(int *px, int *py, int *x, int *y, int *w, int *h,
    Window *frame, Window *win) {
	Window r, c;
	XWindowAttributes attr;
	Bool ret;
	int rootx, rooty, wx, wy;
	unsigned int mask;
	
	ret = XQueryPointer(dpy, rootwin, &r, &c, &rootx, &rooty, &wx, &wy,
	    &mask);

	*frame = c;

	/* current pointer position is returned too */
	*px = rootx;
	*py = rooty;

	if (!ret || ! c || c == rootwin) {
		/* no immediate child */
		return 0;
	}

	/* child window position and size */
	if (! valid_window(c, &attr, 1)) {
		return 0;
	}

	*x = attr.x;
	*y = attr.y;
	*w = attr.width;
	*h = attr.height;

	if (win != NULL) {
		*win = descend_pointer(5, c, NULL, 0);
	}

	return 1;
}

static int defer_update_nofb = 6;	/* defer a shorter time under -nofb */

int scrollcopyrect_top, scrollcopyrect_bot;
int scrollcopyrect_left, scrollcopyrect_right;
double scr_key_time, scr_key_persist;
double scr_mouse_time, scr_mouse_persist, scr_mouse_maxtime;
double scr_mouse_pointer_delay;
double scr_key_bdpush_time, scr_mouse_bdpush_time;

void parse_scroll_copyrect_str(char *scr) {
	char *p, *str;
	int i;
	char *part[10];

	for (i=0; i<10; i++) {
		part[i] = NULL;
	}

	if (scr == NULL || *scr == '\0') {
		return;
	}

	str = strdup(scr);

	p = strtok(str, ",");
	i = 0;
	while (p) {
		part[i++] = strdup(p);
		p = strtok(NULL, ",");
	}
	free(str);


	/*
	 * Top, Bottom, Left, Right tolerances for scrollbar locations.
	 */
	if ((str = part[0]) != NULL) {
		int t, b, l, r;
		if (sscanf(str, "%d+%d+%d+%d", &t, &b, &l, &r) == 4) {
			scrollcopyrect_top   = t;
			scrollcopyrect_bot   = b;
			scrollcopyrect_left  = l;
			scrollcopyrect_right = r;
		}
		free(str);
	}

	/* key scrolling timing heuristics. */
	if ((str = part[1]) != NULL) {
		double t1, t2, t3;
		if (sscanf(str, "%lf+%lf+%lf", &t1, &t2, &t3) == 3) {
			scr_key_time = t1;
			scr_key_persist = t2;
			scr_key_bdpush_time = t3;
		}
		free(str);
	}

	/* mouse scrolling timing heuristics. */
	if ((str = part[2]) != NULL) {
		double t1, t2, t3, t4, t5;
		if (sscanf(str, "%lf+%lf+%lf+%lf+%lf", &t1, &t2, &t3, &t4,
		    &t5) == 5) {
			scr_mouse_time = t1;
			scr_mouse_persist = t2;
			scr_mouse_bdpush_time = t3;
			scr_mouse_pointer_delay = t4;
			scr_mouse_maxtime = t5;
		}
		free(str);
	}
}

void parse_scroll_copyrect(void) {
	parse_scroll_copyrect_str(SCROLL_COPYRECT_PARMS);
	if (! scroll_copyrect_str) {
		scroll_copyrect_str = strdup(SCROLL_COPYRECT_PARMS);
	}
	parse_scroll_copyrect_str(scroll_copyrect_str);
}

/*
WIREFRAME_PARMS "0xff,2,0,30+6+6+6,0.05+0.3+2.0,8"
shade,linewidth,percent,T+B+L+R,t1+t2+t3
 */
#define LW_MAX 8
unsigned long wireframe_shade;
int wireframe_lw;
double wireframe_frac;
int wireframe_top, wireframe_bot, wireframe_left, wireframe_right;
double wireframe_t1, wireframe_t2, wireframe_t3, wireframe_t4;

/*
 * Parse the gory -wireframe string for parameters.
 */
void parse_wireframe_str(char *wf) {
	char *p, *str;
	int i;
	char *part[10];

	for (i=0; i<10; i++) {
		part[i] = NULL;
	}

	if (wf == NULL || *wf == '\0') {
		return;
	}

	str = strdup(wf);

	/* leading ",", make it start with ignorable string "z" */
	if (*str == ',') {
		char *tmp = (char *) malloc(strlen(str)+2);	
		strcpy(tmp, "z");
		strcat(tmp, str);
		free(str);
		str = tmp;
	}

	p = strtok(str, ",");
	i = 0;
	while (p) {
		part[i++] = strdup(p);
		p = strtok(NULL, ",");
	}
	free(str);


	/* Wireframe shade, color, RGB: */
	if ((str = part[0]) != NULL) {
		unsigned long n;
		int r, g, b, ok = 0;
		XColor cdef;
		Colormap cmap;
		if (dpy && (bpp == 32 || bpp == 16)) {
		 	cmap = DefaultColormap (dpy, scr);
			if (XParseColor(dpy, cmap, str, &cdef) &&
			    XAllocColor(dpy, cmap, &cdef)) {
				r = cdef.red   >> 8;
				g = cdef.green >> 8;
				b = cdef.blue  >> 8;
				if (r == 0 && g == 0) {
					g = 1;	/* need to be > 255 */
				}
				n = 0;
				n |= (r << main_red_shift);
				n |= (g << main_green_shift);
				n |= (b << main_blue_shift);
				wireframe_shade = n;
				ok = 1;
			}
		}
		if (ok) {
			;
		} else if (sscanf(str, "0x%lx", &n) == 1) {
			wireframe_shade = n;	
		} else if (sscanf(str, "%ld", &n) == 1) {
			wireframe_shade = n;	
		} else if (sscanf(str, "%lx", &n) == 1) {
			wireframe_shade = n;	
		}
		free(str);
	}

	/* linewidth: # of pixels wide for the wireframe lines */
	if ((str = part[1]) != NULL) {
		int n;
		if (sscanf(str, "%d", &n) == 1) {
			wireframe_lw = n;	
			if (wireframe_lw < 1) {
				wireframe_lw = 1; 
			}
			if (wireframe_lw > LW_MAX) {
				wireframe_lw = LW_MAX; 
			}
		}
		free(str);
	}

	/* percentage cutoff for opaque move/resize (like WM's) */
	if ((str = part[2]) != NULL) {
		if (*str == '\0') {
			;
		} else if (strchr(str, '.')) {
			wireframe_frac = atof(str);
		} else {
			wireframe_frac = ((double) atoi(str))/100.0;
		}
		free(str);
	}

	/*
	 * Top, Bottom, Left, Right tolerances to guess the wm frame is
	 * being grabbed (Top is traditionally bigger, i.e. titlebar):
	 */
	if ((str = part[3]) != NULL) {
		int t, b, l, r;
		if (sscanf(str, "%d+%d+%d+%d", &t, &b, &l, &r) == 4) {
			wireframe_top   = t;
			wireframe_bot   = b;
			wireframe_left  = l;
			wireframe_right = r;
		}
		free(str);
	}

	/* check_wireframe() timing heuristics. */
	if ((str = part[4]) != NULL) {
		double t1, t2, t3, t4;
		if (sscanf(str, "%lf+%lf+%lf+%lf", &t1, &t2, &t3, &t4) == 4) {
			wireframe_t1 = t1;
			wireframe_t2 = t2;
			wireframe_t3 = t3;
			wireframe_t4 = t4;
		}
		free(str);
	}
}

/*
 * First parse the defaults and apply any user supplied ones (may be a subset)
 */
void parse_wireframe(void) {
	parse_wireframe_str(WIREFRAME_PARMS);
	if (! wireframe_str) {
		wireframe_str = strdup(WIREFRAME_PARMS);
	}
	parse_wireframe_str(wireframe_str);
}

/*
 * Set wireframe_copyrect based on desired mode.
 */
void set_wirecopyrect_mode(char *str) {
	char *orig = wireframe_copyrect;
	if (str == NULL || *str == '\0') {
		wireframe_copyrect = strdup(wireframe_copyrect_default);
	} else if (!strcmp(str, "always") || !strcmp(str, "all")) {
		wireframe_copyrect = strdup("always");
	} else if (!strcmp(str, "top")) {
		wireframe_copyrect = strdup("top");
	} else if (!strcmp(str, "never") || !strcmp(str, "none")) {
		wireframe_copyrect = strdup("never");
	} else {
		if (! wireframe_copyrect) {
			wireframe_copyrect = strdup(wireframe_copyrect_default);
		}
		rfbLog("unknown -wirecopyrect mode: %s, using: %s\n", str,
		    wireframe_copyrect);
	}
	if (orig) {
		free(orig);
	}
}

/*
 * Set scroll_copyrect based on desired mode.
 */
void set_scrollcopyrect_mode(char *str) {
	char *orig = scroll_copyrect;
	if (str == NULL || *str == '\0') {
		scroll_copyrect = strdup(scroll_copyrect_default);
	} else if (!strcmp(str, "always") || !strcmp(str, "all") ||
		    !strcmp(str, "both")) {
		scroll_copyrect = strdup("always");
	} else if (!strcmp(str, "keys") || !strcmp(str, "keyboard")) {
		scroll_copyrect = strdup("keys");
	} else if (!strcmp(str, "mouse") || !strcmp(str, "pointer")) {
		scroll_copyrect = strdup("mouse");
	} else if (!strcmp(str, "never") || !strcmp(str, "none")) {
		scroll_copyrect = strdup("never");
	} else {
		if (! scroll_copyrect) {
			scroll_copyrect = strdup(scroll_copyrect_default);
		}
		rfbLog("unknown -scrollcopyrect mode: %s, using: %s\n", str,
		    scroll_copyrect);
	}
	if (orig) {
		free(orig);
	}
}

int match_str_list(char *str, char **list) {
	int i = 0, matched = 0;

	if (! list) {
		return matched;
	}
	while (list[i] != NULL) {
		if (!strcmp(list[i], "*")) {
			matched = 1;
			break;
		} else if (strstr(str, list[i])) {
			matched = 1;
			break;
		}
		i++;
	}
	return matched;
}

char **create_str_list(char *cslist) {
	int i, n;
	char *p, *str = strdup(cslist);
	char **list = NULL;
	
	n = 1;
	p = str;
	while (*p != '\0') {
		if (*p == ',') {
			n++;
		}
		p++;
	}

	list = (char **) malloc((n+1)*sizeof(char *));
	for(i=0; i < n+1; i++) {
		list[i] = NULL;
	}

	p = strtok(str, ",");
	i = 0;
	while (p && i < n) {
		list[i++] = strdup(p);
		p = strtok(NULL, ",");
	}
	free(str);

	return list;
}

void initialize_scroll_keys(void) {
	char *str, *p;
	int i, nkeys = 0, saw_builtin = 0;
	int ks_max = 2 * 0xFFFF;

	if (scroll_key_list) {
		free(scroll_key_list);
		scroll_key_list = NULL;
	}
	if (! scroll_key_list_str || *scroll_key_list_str == '\0') {
		return;
	}

	if (strstr(scroll_key_list_str, "builtin")) {
		int k;
		/* add in number of keysyms builtin gives */
		for (k=1; k<ks_max; k++)  {
			if (xrecord_scroll_keysym((rfbKeySym) k)) {
				nkeys++;
			}
		}
	}

	nkeys++;	/* first key, i.e. no commas. */
	p = str = strdup(scroll_key_list_str);
	while(*p) {
		if (*p == ',') {
			nkeys++;	/* additional key. */
		}
		p++;
	}
	
	nkeys++;	/* exclude/include 0 element */
	nkeys++;	/* trailing NoSymbol */

	scroll_key_list = (KeySym *)malloc(nkeys*sizeof(KeySym)); 
	for (i=0; i<nkeys; i++) {
		scroll_key_list[i] = NoSymbol;
	}
	if (*str == '-') {
		scroll_key_list[0] = 1;
		p = strtok(str+1, ",");
	} else {
		p = strtok(str, ",");
	}
	i = 1;
	while (p) {
		if (!strcmp(p, "builtin")) {
			int k;
			if (saw_builtin) {
				p = strtok(NULL, ",");
				continue;
			}
			saw_builtin = 1;
			for (k=1; k<ks_max; k++)  {
				if (xrecord_scroll_keysym((rfbKeySym) k)) {
					scroll_key_list[i++] = (rfbKeySym) k;
				}
			}
		} else {
			int in;
			if (sscanf(p, "%d", &in) == 1) {
				scroll_key_list[i++] = (rfbKeySym) in;
			} else if (sscanf(p, "0x%x", &in) == 1) {
				scroll_key_list[i++] = (rfbKeySym) in;
			} else if (XStringToKeysym(p) != NoSymbol) { 
				scroll_key_list[i++] = XStringToKeysym(p);
			} else {
				rfbLog("initialize_scroll_keys: skip unknown "
				    "keysym: %s\n", p);
			}
		}
		p = strtok(NULL, ",");
	}
	free(str);
}

void destroy_str_list(char **list) {
	int i = 0;
	if (! list) {
		return;
	}
	while (list[i] != NULL) {
		free(list[i++]);
	}
	free(list);
}

void initialize_scroll_matches(void) {
	char *str, *imp = "__IMPOSSIBLE_STR__";
	int i, n, nkey, nmouse;

	destroy_str_list(scroll_good_all);
	scroll_good_all = NULL;
	destroy_str_list(scroll_good_key);
	scroll_good_key = NULL;
	destroy_str_list(scroll_good_mouse);
	scroll_good_mouse = NULL;

	destroy_str_list(scroll_skip_all);
	scroll_skip_all = NULL;
	destroy_str_list(scroll_skip_key);
	scroll_skip_key = NULL;
	destroy_str_list(scroll_skip_mouse);
	scroll_skip_mouse = NULL;

	/* scroll_good: */
	if (scroll_good_str != NULL && *scroll_good_str != '\0') {
		str = scroll_good_str;
	} else {
		str = scroll_good_str0;
	}
	scroll_good_all = create_str_list(str);

	nkey = 0;
	nmouse = 0;
	n = 0;
	while (scroll_good_all[n] != NULL) {
		char *s = scroll_good_all[n++];
		if (strstr(s, "KEY:") == s) nkey++;
		if (strstr(s, "MOUSE:") == s) nmouse++;
	}
	if (nkey++) {
		scroll_good_key = (char **)malloc(nkey*sizeof(char *));
		for (i=0; i<nkey; i++) scroll_good_key[i] = NULL;
	}
	if (nmouse++) {
		scroll_good_mouse = (char **)malloc(nmouse*sizeof(char *));
		for (i=0; i<nmouse; i++) scroll_good_mouse[i] = NULL;
	}
	nkey = 0;
	nmouse = 0;
	for (i=0; i<n; i++) {
		char *s = scroll_good_all[i];
		if (strstr(s, "KEY:") == s) {
			scroll_good_key[nkey++] = strdup(s+strlen("KEY:"));
			free(s);
			scroll_good_all[i] = strdup(imp);
		} else if (strstr(s, "MOUSE:") == s) {
			scroll_good_mouse[nmouse++]=strdup(s+strlen("MOUSE:"));
			free(s);
			scroll_good_all[i] = strdup(imp);
		}
	}

	/* scroll_skip: */
	if (scroll_skip_str != NULL && *scroll_skip_str != '\0') {
		str = scroll_skip_str;
	} else {
		str = scroll_skip_str0;
	}
	scroll_skip_all = create_str_list(str);

	nkey = 0;
	nmouse = 0;
	n = 0;
	while (scroll_skip_all[n] != NULL) {
		char *s = scroll_skip_all[n++];
		if (strstr(s, "KEY:") == s) nkey++;
		if (strstr(s, "MOUSE:") == s) nmouse++;
	}
	if (nkey++) {
		scroll_skip_key = (char **)malloc(nkey*sizeof(char *));
		for (i=0; i<nkey; i++) scroll_skip_key[i] = NULL;
	}
	if (nmouse++) {
		scroll_skip_mouse = (char **)malloc(nmouse*sizeof(char *));
		for (i=0; i<nmouse; i++) scroll_skip_mouse[i] = NULL;
	}
	nkey = 0;
	nmouse = 0;
	for (i=0; i<n; i++) {
		char *s = scroll_skip_all[i];
		if (strstr(s, "KEY:") == s) {
			scroll_skip_key[nkey++] = strdup(s+strlen("KEY:"));
			free(s);
			scroll_skip_all[i] = strdup(imp);
		} else if (strstr(s, "MOUSE:") == s) {
			scroll_skip_mouse[nmouse++]=strdup(s+strlen("MOUSE:"));
			free(s);
			scroll_skip_all[i] = strdup(imp);
		}
	}
}

void initialize_scroll_term(void) {
	char *str;
	int n;

	destroy_str_list(scroll_term);
	scroll_term = NULL;

	if (scroll_term_str != NULL && *scroll_term_str != '\0') {
		str = scroll_term_str;
	} else {
		str = scroll_term_str0;
	}
	if (!strcmp(str, "none")) {
		return;
	}
	scroll_term = create_str_list(str);

	n = 0;
	while (scroll_term[n] != NULL) {
		char *s = scroll_good_all[n++];
		/* pull parameters out at some point */
		s = NULL;
	}
}

void initialize_max_keyrepeat(void) {
	char *str;
	int lo, hi;

	if (max_keyrepeat_str != NULL && *max_keyrepeat_str != '\0') {
		str = max_keyrepeat_str;
	} else {
		str = max_keyrepeat_str0;
	}

	if (sscanf(str, "%d-%d", &lo, &hi) != 2) {
		rfbLog("skipping invalid -scr_keyrepeat string: %s\n", str);
		sscanf(max_keyrepeat_str0, "%d-%d", &lo, &hi);
	}
	max_keyrepeat_lo = lo;
	max_keyrepeat_hi = hi;
	if (max_keyrepeat_lo < 1) {
		max_keyrepeat_lo = 1;
	}
	if (max_keyrepeat_hi > 40) {
		max_keyrepeat_hi = 40;
	}
}

typedef struct saveline {
	int x0, y0, x1, y1;
	int shift;
	int vert;
	int saved;
	char *data;
} saveline_t;

/*
 * Draw the wireframe box onto the framebuffer.  Saves the real
 * framebuffer data to some storage lines.  Restores previous lines.
 * use restore = 1 to clean up (done with animation).
 * This works with -scale.
 */
void draw_box(int x, int y, int w, int h, int restore) {
	int x0, y0, x1, y1, i, pixelsize = bpp/8;
	char *dst, *src;
	static saveline_t *save[4];
	static int first = 1, len = 0;
	int max = dpy_x > dpy_y ? dpy_x : dpy_y;
	int sz, lw = wireframe_lw;
	unsigned long shade = wireframe_shade;
	int color = 0;
	unsigned short us;
	unsigned long ul;

	if (max > len) {
		/* create/resize storage lines: */
		for (i=0; i<4; i++) {
			len = max;
			if (! first && save[i]) {
				if (save[i]->data) {
					free(save[i]->data);
				}
				free(save[i]);
			}
			save[i] = (saveline_t *) malloc(sizeof(saveline_t));
			save[i]->saved = 0;
			sz = (LW_MAX+1)*len*pixelsize;
			save[i]->data = (char *)malloc(sz);

			/* 
			 * Four types of lines:
			 *	0) top horizontal
			 *	1) bottom horizontal
			 *	2) left vertical
			 *	3) right vertical
			 *
			 * shift means shifted by width or height.
			 */
			if (i == 0) {
				save[i]->vert  = 0;
				save[i]->shift = 0;
			} else if (i == 1) {
				save[i]->vert  = 0;
				save[i]->shift = 1;
			} else if (i == 2) {
				save[i]->vert  = 1;
				save[i]->shift = 0;
			} else if (i == 3) {
				save[i]->vert  = 1;
				save[i]->shift = 1;
			}
		}
	}
	first = 0;

	/*
	 * restore any saved lines. see below for algorithm and
	 * how x0, etc. are used.  we just reverse those steps.
	 */
	for (i=0; i<4; i++) {
		int s = save[i]->shift;
		int yu, y_min = -1, y_max = -1;
		int y_start, y_stop, y_step;

		if (! save[i]->saved) {
			continue;
		}
		x0 = save[i]->x0;
		y0 = save[i]->y0;
		x1 = save[i]->x1;
		y1 = save[i]->y1;
		if (save[i]->vert) {
			y_start = y0+lw;
			y_stop  = y1-lw;
			y_step  = lw*pixelsize;
		} else {
			y_start = y0 - s*lw;
			y_stop  = y_start + lw;
			y_step  = max*pixelsize;
		}
		for (yu = y_start; yu < y_stop; yu++) {
			if (x0 == x1) {
				continue;
			}
			if (yu < 0 || yu >= dpy_y) {
				continue;
			}
			if (y_min < 0 || yu < y_min) {
				y_min = yu;
			}
			if (y_max < 0 || yu > y_max) {
				y_max = yu;
			}
			src = save[i]->data + (yu-y_start)*y_step;
			dst = main_fb + yu*main_bytes_per_line +
			    x0*pixelsize;
			memcpy(dst, src, (x1-x0)*pixelsize);
		}
		if (y_min >= 0) {
			mark_rect_as_modified(x0, y_min, x1, y_max+1, 0);
		}
		save[i]->saved = 0;
	}

	if (restore) {
		return;
	}

if (0) fprintf(stderr, "  DrawBox: %dx%d+%d+%d\n", w, h, x, y);

	/*
	 * work out shade/color for the wireframe line, could be a color
	 * for 16bpp or 24bpp.
	 */
	if (shade > 255) {
		if (pixelsize == 2) {
			us = (unsigned short) (shade & 0xffff);
			color = 1;
		} else if (pixelsize == 4) {
			ul = (unsigned long) shade;
			color = 1;
		} else {
			shade = shade % 256;
		}
	}

	for (i=0; i<4; i++)  {
		int s = save[i]->shift;
		int yu, y_min = -1, y_max = -1;
		int yblack = -1, xblack1 = -1, xblack2 = -1;
		int y_start, y_stop, y_step;

		if (save[i]->vert) {
			/*
			 * make the narrow x's be on the screen, let
			 * the y's hang off (not drawn).
			 */
			save[i]->x0 = x0 = nfix(x + s*w - s*lw, dpy_x);
			save[i]->y0 = y0 = y;
			save[i]->x1 = x1 = nfix(x + s*w - s*lw + lw, dpy_x);
			save[i]->y1 = y1 = y + h;

			/*
			 * start and stop a linewidth away from true edge,
			 * to avoid interfering with horizontal lines.
			 */
			y_start = y0+lw;
			y_stop  = y1-lw;
			y_step  = lw*pixelsize;

			/* draw a black pixel for the border if lw > 1 */
			if (s) {
				xblack1 = x1-1;
			} else {
				xblack1 = x0;
			}
		} else {
			/*
			 * make the wide x's be on the screen, let the y's
			 * hang off (not drawn).
			 */
			save[i]->x0 = x0 = nfix(x,     dpy_x);
			save[i]->y0 = y0 = y + s*h;
			save[i]->x1 = x1 = nfix(x + w, dpy_x);
			save[i]->y1 = y1 = y0 + lw;
			y_start = y0 - s*lw;
			y_stop  = y_start + lw;
			y_step  = max*pixelsize;

			/* draw a black pixels for the border if lw > 1 */
			if (s) {
				yblack = y_stop - 1;
			} else {
				yblack = y_start;
			}
			xblack1 = x0;
			xblack2 = x1-1;
		}

		/* now loop over the allowed y's for either case */
		for (yu = y_start; yu < y_stop; yu++) {
			if (x0 == x1) {
				continue;
			}
			if (yu < 0 || yu >= dpy_y) {
				continue;
			}

			/* record min and max y's for marking rectangle: */
			if (y_min < 0 || yu < y_min) {
				y_min = yu;
			}
			if (y_max < 0 || yu > y_max) {
				y_max = yu;
			}

			/* save fb data for this line: */
			save[i]->saved = 1;
			src = main_fb + yu*main_bytes_per_line +
			    x0*pixelsize;
			dst = save[i]->data + (yu-y_start)*y_step;
			memcpy(dst, src,   (x1-x0)*pixelsize);

			/* apply the shade/color to make the wireframe line: */
			if (! color) {
				memset(src, shade, (x1-x0)*pixelsize);
			} else {
				char *csrc = src;
				unsigned short *usp;
				unsigned long *ulp;
				int k;
				for (k=0; k < x1 - x0; k++) {
					if (pixelsize == 4) {
						ulp = (unsigned long *)csrc;
						*ulp = ul;
					} else if (pixelsize == 2) {
						usp = (unsigned short *)csrc;
						*usp = us;
					}
					csrc += pixelsize;
				}
			}

			/* apply black border for lw >= 2 */
			if (lw > 1) {
				if (yu == yblack) {
					memset(src, 0, (x1-x0)*pixelsize);
				}
				if (xblack1 >= 0) {
					src = src + (xblack1 - x0)*pixelsize;
					memset(src, 0, pixelsize);
				}
				if (xblack2 >= 0) {
					src = src + (xblack2 - x0)*pixelsize;
					memset(src, 0, pixelsize);
				}
			}
		}
		/* mark it for sending: */
		if (save[i]->saved) {
			mark_rect_as_modified(x0, y_min, x1, y_max+1, 0);
		}
	}
}

int direct_fb_copy(int x1, int y1, int x2, int y2, int mark) {
	char *src, *dst;
	int y, pixelsize = bpp/8;
	int xmin = -1, xmax = -1, ymin = -1, ymax = -1;
	int do_cmp = 2;
	double tm;
	int db = 0;

if (db) dtime0(&tm);

	x1 = nfix(x1, dpy_x);
	y1 = nfix(y1, dpy_y);
	x2 = nfix(x2, dpy_x+1);
	y2 = nfix(y2, dpy_y+1);

	if (x1 == x2) {
		return 1;
	}
	if (y1 == y2) {
		return 1;
	}

	X_LOCK;
	for (y = y1; y < y2; y++) {
		XRANDR_SET_TRAP_RET(0, "direct_fb_copy-set");
		copy_image(scanline, x1, y, x2 - x1, 1);
		XRANDR_CHK_TRAP_RET(0, "direct_fb_copy-chk");
		
		src = scanline->data;
		dst = main_fb + y * main_bytes_per_line + x1 * pixelsize;

		if (do_cmp == 0 || !mark) {
			memcpy(dst, src, (x2 - x1)*pixelsize);

		} else if (do_cmp == 1) {
			if (memcmp(dst, src, (x2 - x1)*pixelsize)) {
				if (ymin == -1 || y < ymin) {
					ymin = y;
				}
				if (ymax == -1 || y > ymax) {
					ymax = y;
				}
				memcpy(dst, src, (x2 - x1)*pixelsize);
			}

		} else if (do_cmp == 2) {
			int n, shift, xlo, xhi, k, block = 32;
			char *dst2, *src2;

			for (k=0; k*block < (x2 - x1); k++) {
				shift = k*block;
				xlo = x1  + shift;
				xhi = xlo + block;
				if (xhi > x2) {
					xhi = x2;
				}
				n = xhi - xlo;
				if (n < 1) {
					continue;
				}
				src2 = src + shift*pixelsize;
				dst2 = dst + shift*pixelsize;
				if (memcmp(dst2, src2, n*pixelsize)) {
					if (ymin == -1 || y < ymin) {
						ymin = y;
					}
					if (ymax == -1 || y > ymax) {
						ymax = y;
					}
					if (xmin == -1 || xlo < xmin) {
						xmin = xlo;
					}
					if (xmax == -1 || xhi > xmax) {
						xmax = xhi;
					}
					memcpy(dst2, src2, n*pixelsize);
				}
			}
		}
	}
	X_UNLOCK;

	if (do_cmp == 0) {
		xmin = x1;
		ymin = y1;
		xmax = x2;
		ymax = y2;
	} else if (do_cmp == 1) {
		xmin = x1;
		xmax = x2;
	}

	if (xmin < 0 || ymin < 0 || xmax < 0 || xmin < 0) {
		/* no diffs */
		return 1;
	}

	if (xmax < x2) {
		xmax++;
	}
	if (ymax < y2) {
		ymax++;
	}

	if (mark) {
		mark_rect_as_modified(xmin, ymin, xmax, ymax, 1);
	}

if (db) {
	fprintf(stderr, "direct_fb_copy: %dx%d+%d+%d - %d  %.4f\n",
		x2 - x1, y2 - y1, x1, y1, mark, dtime(&tm));
}

	return 1;
}

int do_bdpush(Window wm_win, int x0, int y0, int w0, int h0, int bdx,
    int bdy, int bdskinny) {

	XWindowAttributes attr;
	sraRectangleIterator *iter;
	sraRect rect;
	sraRegionPtr frame, whole, tmpregion;
	int tx1, ty1, tx2, ty2;
	static Window last_wm_win = None;
	static int last_x, last_y, last_w, last_h;
	int do_fb_push = 0;
	int db = debug_scroll;

	if (wm_win == last_wm_win) {
		attr.x = last_x;
		attr.y = last_y;
		attr.width = last_w;
		attr.height = last_h;
	} else {
		if (!valid_window(wm_win, &attr, 1)) {
			return do_fb_push;
		}
		last_wm_win = wm_win;
		last_x = attr.x;
		last_y = attr.y;
		last_w = attr.width;
		last_h = attr.height;
	}
if (db > 1) fprintf(stderr, "BDP  %d %d %d %d  %d %d %d  %d %d %d %d\n",
	x0, y0, w0, h0, bdx, bdy, bdskinny, last_x, last_y, last_w, last_h);

	/* wm frame: */
	tx1 = attr.x;
	ty1 = attr.y;
	tx2 = attr.x + attr.width;
	ty2 = attr.y + attr.height;

	whole = sraRgnCreateRect(0, 0, dpy_x, dpy_y);
	frame = sraRgnCreateRect(tx1, ty1, tx2, ty2);
	sraRgnAnd(frame, whole);

	/* scrolling window: */
	tmpregion = sraRgnCreateRect(x0, y0, x0 + w0, y0 + h0);
	sraRgnAnd(tmpregion, whole);

	sraRgnSubtract(frame, tmpregion);
	sraRgnDestroy(tmpregion);

	if (!sraRgnEmpty(frame)) {
		double dt = 0.0, dm;
		dtime0(&dm);
		iter = sraRgnGetIterator(frame);
		while (sraRgnIteratorNext(iter, &rect)) {
			tx1 = rect.x1;
			ty1 = rect.y1;
			tx2 = rect.x2;
			ty2 = rect.y2;

			if (bdskinny > 0) {
				int ok = 0;
				if (nabs(ty2-ty1) <= bdskinny) {
					ok = 1;
				}
				if (nabs(tx2-tx1) <= bdskinny) {
					ok = 1;
				}
				if (! ok) {
					continue;
				}
			}

			if (bdx >= 0) {
				if (bdx < tx1 || tx2 <= bdx) {
					continue;
				}
			}
			if (bdy >= 0) {
				if (bdy < ty1 || ty2 <= bdy) {
					continue;
				}
			}

			direct_fb_copy(tx1, ty1, tx2, ty2, 1);

			do_fb_push++;
			dt += dtime(&dm);
if (db > 1) fprintf(stderr, "  BDP(%d,%d-%d,%d)  dt: %.4f\n", tx1, ty1, tx2, ty2, dt);
		}
		sraRgnReleaseIterator(iter);
	}
	sraRgnDestroy(whole);
	sraRgnDestroy(frame);

	return do_fb_push;
}

int set_ypad(void) {
	int ev, ev_tot = scr_ev_cnt;
	static Window last_win = None;
	static double last_time = 0.0;
	static int y_accum = 0, last_sign = 0;
	double now, cut = 0.1;
	int dy_sum = 0, ys = 0, sign;
	int font_size = 15;
	int win_y, scr_y, loc_cut = 4*font_size, y_cut = 10*font_size;
	
	if (!xrecord_set_by_keys || !xrecord_name_info) {
		return 0;
	}
	if (xrecord_name_info[0] == '\0') {
		return 0;
	}
	if (! ev_tot) {
		return 0;
	}
	if (xrecord_keysym == NoSymbol)  {
		return 0;
	}
	if (!xrecord_scroll_keysym(xrecord_keysym)) {
		return 0;
	}
	if (!scroll_term) {
		return 0;
	}
	if (!match_str_list(xrecord_name_info, scroll_term)) {
		return 0;
	}

	for (ev=0; ev < ev_tot; ev++) {
		dy_sum += nabs(scr_ev[ev].dy);
		if (scr_ev[ev].dy < 0) {
			ys--;
		} else if (scr_ev[ev].dy > 0) {
			ys++;
		} else {
			ys = 0;
			break;
		}
		if (scr_ev[ev].win != scr_ev[0].win) {
			ys = 0;
			break;
		}
		if (scr_ev[ev].dx != 0) {
			ys = 0;
			break;
		}
	}
	if (ys != ev_tot && ys != -ev_tot) {
		return 0;
	}
	if (ys < 0) {
		sign = -1;
	} else {
		sign = 1;
	}

	if (sign > 0) {
		/*
		 * this case is not as useful as scrolling near the
		 * bottom of a terminal.  But there are problems for it too.
		 */
		return 0;
	}

	win_y = scr_ev[0].win_y + scr_ev[0].win_h;
	scr_y = scr_ev[0].y + scr_ev[0].h;
	if (nabs(scr_y - win_y) > loc_cut) {
		/* require it to be near the bottom. */
		return 0;
	}

	now = dnow();

	if (now < last_time + cut) {
		int ok = 1;
		if (last_win && scr_ev[0].win != last_win) {
			ok = 0;
		}
		if (last_sign && sign != last_sign) {
			ok = 0;
		}
		if (! ok) {
			last_win = None;
			last_sign = 0;
			y_accum = 0;
			last_time = 0.0;
			return 0;
		}
	} else {
		last_win = None;
		last_sign = 0;
		last_time = 0.0;
		y_accum = 0;
	}

	y_accum += sign * dy_sum;

	if (4 * nabs(y_accum) > scr_ev[0].h && y_cut) {
		;	/* TBD */
	}

	last_sign = sign;
	last_win = scr_ev[0].win;
	last_time = now;

	return y_accum;
}


#define PUSH_TEST(n)  \
if (n) { \
	double dt = 0.0, tm; dtime0(&tm); \
	fprintf(stderr, "PUSH---\n"); \
	while (dt < 2.0) { rfbPE(50000); dt += dtime(&tm); } \
	fprintf(stderr, "---PUSH\n"); \
}

int push_scr_ev(double *age, int type, int bdpush, int bdx, int bdy,
    int bdskinny) {
	Window frame, win, win0;
	int x, y, w, h, wx, wy, ww, wh, dx, dy;
	int x0, y0, w0, h0;
	int nx, ny, nw, nh;
	int dret = 1, do_fb_push = 0, obscured;
	int ev, ev_tot = scr_ev_cnt;
	double tm, dt, st, waittime = 0.125;
	double max_age = *age;
	int db = debug_scroll, rrate = get_read_rate();
	sraRegionPtr backfill, whole, tmpregion, tmpregion2;
	int link, latency, netrate;
	int ypad = 0;

	/* we return the oldest one. */
	*age = 0.0;

	if (ev_tot == 0) {
		return dret;
	}

	link = link_rate(&latency, &netrate);

	if (link == LR_DIALUP) {
		waittime *= 5;
	} else if (link == LR_BROADBAND) {
		waittime *= 3;
	} else if (latency > 80 || netrate < 40) {
		waittime *= 3;
	}

	backfill = sraRgnCreate();
	whole = sraRgnCreateRect(0, 0, dpy_x, dpy_y);

	win0 = scr_ev[0].win;
	x0 = scr_ev[0].win_x;
	y0 = scr_ev[0].win_y;
	w0 = scr_ev[0].win_w;
	h0 = scr_ev[0].win_h;

	ypad = set_ypad();

if (db) fprintf(stderr, "ypad: %d  dy[0]: %d\n", ypad, scr_ev[0].dy);

	for (ev=0; ev < ev_tot; ev++) {
		double ag;
	
		x   = scr_ev[ev].x;
		y   = scr_ev[ev].y;
		w   = scr_ev[ev].w;
		h   = scr_ev[ev].h;
		dx  = scr_ev[ev].dx;
		dy  = scr_ev[ev].dy;
		win = scr_ev[ev].win;
		wx  = scr_ev[ev].win_x;
		wy  = scr_ev[ev].win_y;
		ww  = scr_ev[ev].win_w;
		wh  = scr_ev[ev].win_h;
		nx  = scr_ev[ev].new_x;
		ny  = scr_ev[ev].new_y;
		nw  = scr_ev[ev].new_w;
		nh  = scr_ev[ev].new_h;
		st  = scr_ev[ev].t;

		ag = (dnow() - servertime_diff) - st;
		if (ag > *age) {
			*age = ag;
		}

		if (dabs(ag) > max_age) {
if (db) fprintf(stderr, "push_scr_ev: TOO OLD: %.4f :: (%.4f - %.4f) "
    "- %.4f \n", ag, dnow(), servertime_diff, st);				
			dret = 0;
			break;
		} else {
if (db) fprintf(stderr, "push_scr_ev: AGE:     %.4f\n", ag);
		}
		if (win != win0) {
if (db) fprintf(stderr, "push_scr_ev: DIFF WIN: 0x%lx != 0x%lx\n", win, win0);
			dret = 0;
			break;
		}
		if (wx != x0 || wy != y0) {
if (db) fprintf(stderr, "push_scr_ev: WIN SHIFT: %d %d, %d %d", wx, x0, wy, y0);
			dret = 0;
			break;
		}
		if (ww != w0 || wh != h0) {
if (db) fprintf(stderr, "push_scr_ev: WIN RESIZE: %d %d, %d %d", ww, w0, wh, h0);
			dret = 0;
			break;
		}
		if (w < 1 || h < 1 || ww < 1 || wh < 1) {
if (db) fprintf(stderr, "push_scr_ev: NEGATIVE h/w: %d %d %d %d\n", w, h, ww, wh);
			dret = 0;
			break;
		}

if (db > 1) fprintf(stderr, "push_scr_ev: got: %d x: %4d y: %3d"
    " w: %4d h: %3d  dx: %d dy: %d %dx%d+%d+%d   win: 0x%lx\n",
    ev, x, y, w, h, dx, dy, w, h, x, y, win);

if (db > 1) fprintf(stderr, "------------ got: %d x: %4d y: %3d"
    " w: %4d h: %3d %dx%d+%d+%d\n",
    ev, wx, wy, ww, wh, ww, wh, wx, wy);

if (db > 1) fprintf(stderr, "------------ got: %d x: %4d y: %3d"
    " w: %4d h: %3d %dx%d+%d+%d\n",
    ev, nx, ny, nw, nh, nw, nh, nx, ny);

		frame = None;
		if (xrecord_wm_window) {
			frame = xrecord_wm_window;
		}
		if (! frame) {
			X_LOCK;
			frame = query_pointer(rootwin);
			X_UNLOCK;
		}
		if (! frame) {
			frame = win;
		}

		dtime0(&tm);

		tmpregion = sraRgnCreateRect(0, 0, dpy_x, dpy_y);
		tmpregion2 = sraRgnCreateRect(wx, wy, wx+ww, wy+wh);
		sraRgnAnd(tmpregion2, whole);
		sraRgnSubtract(tmpregion, tmpregion2);
		sraRgnDestroy(tmpregion2);

		/* do the wm frame just incase the above is bogus too. */
		if (frame && frame != win) {
			int k, gotk = -1;
			for (k = stack_list_num - 1; k >= 0; k--) {
				if (stack_list[k].win == frame &&
				    stack_list[k].fetched && 
				    stack_list[k].valid && 
				    stack_list[k].map_state == IsViewable) {
					gotk = k;
					break;
				}
			}
			if (gotk != -1) {
				int tx1, ty1, tx2, ty2;
				tx1 = stack_list[gotk].x;
				ty1 = stack_list[gotk].y;
				tx2 = tx1 + stack_list[gotk].width;
				ty2 = ty1 + stack_list[gotk].height;
				tmpregion2 = sraRgnCreateRect(tx1,ty1,tx2,ty2);
				sraRgnAnd(tmpregion2, whole);
				sraRgnSubtract(tmpregion, tmpregion2);
				sraRgnDestroy(tmpregion2);
			}
		}

		/*
		 * XXX Need to also clip:
		 *	children of win
		 *	siblings of win higher in stacking order.
		 * ignore for now... probably will make some apps
		 * act very strangely.
		 */
if (ypad) {
	if (ypad < 0) {
		if (h > -ypad) {
			h += ypad;
		} else {
			ypad = 0;
		}
	} else {
		if (h > ypad) {
			y += ypad;
		} else {
			ypad = 0;
		}
	}
}
		
		if (try_copyrect(frame, x, y, w, h, dx, dy, &obscured,
		    tmpregion, waittime)) {
			last_scroll_type = type;
			dtime0(&last_scroll_event);

			do_fb_push++;
			urgent_update = 1;
			sraRgnDestroy(tmpregion);
			
PUSH_TEST(0);

		} else {
			dret = 0;
			sraRgnDestroy(tmpregion);
			break;	
		}
		dt = dtime(&tm);
if (0) fprintf(stderr, "  try_copyrect dt: %.4f\n", dt);

		if (ev > 0) {
			sraRgnOffset(backfill, dx, dy);
			sraRgnAnd(backfill, whole);
		}

if (ypad) {
	if (ypad < 0) {
		ny += ypad;	
		nh -= ypad;
	} else {
		;
	}
}

		tmpregion = sraRgnCreateRect(nx, ny, nx + nw, ny + nh);
		sraRgnAnd(tmpregion, whole);
		sraRgnOr(backfill, tmpregion);
		sraRgnDestroy(tmpregion);
	}

	/* try to update the backfill region (new window contents) */
	if (dret != 0) {
		double est, win_area = 0.0, area = 0.0;
		sraRectangleIterator *iter;
		sraRect rect;
		int tx1, ty1, tx2, ty2;

		tmpregion = sraRgnCreateRect(x0, y0, x0 + w0, y0 + h0);
		sraRgnAnd(tmpregion, whole);

		sraRgnAnd(backfill, tmpregion);

		iter = sraRgnGetIterator(tmpregion);
		while (sraRgnIteratorNext(iter, &rect)) {
			tx1 = rect.x1;
			ty1 = rect.y1;
			tx2 = rect.x2;
			ty2 = rect.y2;

			win_area += (tx2 - tx1)*(ty2 - ty1);
		}
		sraRgnReleaseIterator(iter);
		sraRgnDestroy(tmpregion);


		iter = sraRgnGetIterator(backfill);
		while (sraRgnIteratorNext(iter, &rect)) {
			tx1 = rect.x1;
			ty1 = rect.y1;
			tx2 = rect.x2;
			ty2 = rect.y2;

			area += (tx2 - tx1)*(ty2 - ty1);
		}
		sraRgnReleaseIterator(iter);

		est = (area * (bpp/8)) / (1000000.0 * rrate);
if (db) fprintf(stderr, "  area %.1f win_area %.1f est: %.4f", area, win_area, est);
		if (area > 0.90 * win_area) {
if (db) fprintf(stderr, "  AREA_TOO_MUCH");
			dret = 0;
		} else if (est > 0.6) {
if (db) fprintf(stderr, "  EST_TOO_LARGE");
			dret = 0;
		} else if (area <= 0.0) {
			;
		} else {
			dtime0(&tm);
			iter = sraRgnGetIterator(backfill);
			while (sraRgnIteratorNext(iter, &rect)) {
				tx1 = rect.x1;
				ty1 = rect.y1;
				tx2 = rect.x2;
				ty2 = rect.y2;
				tx1 = nfix(tx1, dpy_x);
				ty1 = nfix(ty1, dpy_y);
				tx2 = nfix(tx2, dpy_x+1);
				ty2 = nfix(ty2, dpy_y+1);

				dtime(&tm);
if (db) fprintf(stderr, "  DFC(%d,%d-%d,%d)", tx1, ty1, tx2, ty2);
				direct_fb_copy(tx1, ty1, tx2, ty2, 1);
				do_fb_push++;
PUSH_TEST(0);
			}
			sraRgnReleaseIterator(iter);
			dt = dtime(&tm);
if (db) fprintf(stderr, "  dfc---- dt: %.4f", dt);

		}
if (db &&  dret) fprintf(stderr, " **** dret=%d", dret);
if (db && !dret) fprintf(stderr, " ---- dret=%d", dret);
if (db) fprintf(stderr, "\n");
	}

if (db && bdpush) fprintf(stderr, "BDPUSH-TIME:  0x%lx\n", xrecord_wm_window);

	if (bdpush && xrecord_wm_window != None) {
		int x, y, w, h;
		x = scr_ev[0].x;
		y = scr_ev[0].y;
		w = scr_ev[0].w;
		h = scr_ev[0].h;
		do_fb_push += do_bdpush(xrecord_wm_window, x, y, w, h,
		    bdx, bdy, bdskinny); 
	}

	if (do_fb_push) {
		dtime0(&tm);
		fb_push();
		dt = dtime(&tm);
if (0) fprintf(stderr, "  fb_push dt: %.4f", dt);
	}

	sraRgnDestroy(backfill);
	sraRgnDestroy(whole);
	return dret;
}

void get_client_regions(int *req, int *mod, int *cpy, int *num)  {
	
	rfbClientIteratorPtr i;
	rfbClientPtr cl;

	*req = 0;
	*mod = 0;
	*cpy = 0;
	*num = 0;

	i = rfbGetClientIterator(screen);
	while( (cl = rfbClientIteratorNext(i)) ) {
		*req += sraRgnCountRects(cl->requestedRegion);
		*mod += sraRgnCountRects(cl->modifiedRegion);
		*cpy += sraRgnCountRects(cl->copyRegion);
		*num += 1;
	}
	rfbReleaseClientIterator(i);
}

/*
 * Wrapper to apply the rfbDoCopyRegion taking into account if scaling
 * is being done.  Note that copyrect under the scaling case is often
 * only approximate.
 */
void do_copyregion(sraRegionPtr region, int dx, int dy)  {
	sraRectangleIterator *iter;
	sraRect rect;
	int Bpp = bpp/8;
	int x1, y1, x2, y2, w, stride;
	int sx1, sy1, sx2, sy2, sdx, sdy;
	int req, mod, cpy, ncli;
	char *dst, *src;

	if (!scaling || rfb_fb == main_fb) {
		/* normal case */
		get_client_regions(&req, &mod, &cpy, &ncli);
if (debug_scroll > 1) fprintf(stderr, "<<<-rfbDoCopyRect req: %d mod: %d cpy: %d\n", req, mod, cpy); 
		rfbDoCopyRegion(screen, region, dx, dy);

		get_client_regions(&req, &mod, &cpy, &ncli);
if (debug_scroll > 1) fprintf(stderr, ">>>-rfbDoCopyRect req: %d mod: %d cpy: %d\n", req, mod, cpy); 

		return;
	}

	/* rarer case, we need to call rfbDoCopyRect with scaled xy */
	stride = dpy_x * Bpp;

	iter = sraRgnGetIterator(region);
	while(sraRgnIteratorNext(iter, &rect)) {
		int j;

		x1 = rect.x1;
		y1 = rect.y1;
		x2 = rect.x2;
		y2 = rect.y2;

		w = (x2 - x1)*Bpp; 
		dst = main_fb + y1*stride + x1*Bpp;
		src = main_fb + (y1-dy)*stride + (x1-dx)*Bpp;

		if (dy < 0) {
			for (j=y1; j<y2; j++) {
				memmove(dst, src, w);
				dst += stride;
				src += stride;
			}
		} else {
			dst += (y2 - y1 - 1)*stride;
			src += (y2 - y1 - 1)*stride;
			for (j=y2-1; j>y1; j--) {
				memmove(dst, src, w);
				dst -= stride;
				src -= stride;
			}
		}

		sx1 = ((double) x1 / dpy_x) * scaled_x;
		sy1 = ((double) y1 / dpy_y) * scaled_y;
		sx2 = ((double) x2 / dpy_x) * scaled_x;
		sy2 = ((double) y2 / dpy_y) * scaled_y;
		sdx = ((double) dx / dpy_x) * scaled_x;
		sdy = ((double) dy / dpy_y) * scaled_y;

		rfbDoCopyRect(screen, sx1, sy1, sx2, sy2, sdx, sdy);
	}
	sraRgnReleaseIterator(iter);
}

void fb_push_wait(double max_wait, int flags) {
	double tm, dt = 0.0;
	int req, mod, cpy, ncli;

	dtime0(&tm);	
	while (dt < max_wait) {
		int done = 1;
		rfbCFD(0);
		get_client_regions(&req, &mod, &cpy, &ncli);
		if (flags & FB_COPY && cpy) {
			done = 0;
		}
		if (flags & FB_MOD && mod) {
			done = 0;
		}
		if (flags & FB_REQ && req) {
			done = 0;
		}
		if (done) {
			break;
		}

		usleep(1000);
		fb_push();
		dt += dtime(&tm);
	}
}

void fb_push(void) {
	char *httpdir = screen->httpDir;
	int defer = screen->deferUpdateTime;
	int req0, mod0, cpy0, req1, mod1, cpy1, ncli;
	int db = (debug_scroll || debug_wireframe);

	screen->httpDir = NULL;
	screen->deferUpdateTime = 0;

if (db)	get_client_regions(&req0, &mod0, &cpy0, &ncli);

	rfbPE(0);

	screen->httpDir = httpdir;
	screen->deferUpdateTime = defer;

if (db) {
	get_client_regions(&req1, &mod1, &cpy1, &ncli);
	fprintf(stderr, "\nFB_push: req: %d/%d  mod: %d/%d  cpy: %d/%d  %.4f\n",
	req0, req1, mod0, mod1, cpy0, cpy1, dnow() - x11vnc_start);
}

}

/* 
 * utility routine for CopyRect of the window (but not CopyRegion)
 */
int crfix(int x, int dx, int Lx) {
	/* adjust x so that copy source is on screen */
	if (dx > 0) {
		if (x-dx < 0) {
			/* off on the left */
			x = dx;	
		}
	} else {
		if (x-dx >= Lx) {
			/* off on the right */
			x = Lx + dx - 1;
		}
	}
	return x;
}

typedef struct scroll_result {
	Window win;
	double time;
	int result;
} scroll_result_t;

#define SCR_RESULTS_MAX 64
scroll_result_t scroll_results[SCR_RESULTS_MAX];

int scrollability(Window win, int set) {
	double oldest = -1.0;
	int i, index = -1, next_index = -1;

	if (win == None) {
		return 0;
	}
	if (set == SCR_NONE) {
		/* lookup case */
		for (i=0; i<SCR_RESULTS_MAX; i++) {
			if (win == scroll_results[i].win) {
				return scroll_results[i].result;
			}
		}
		return 0;
	}

	for (i=0; i<SCR_RESULTS_MAX; i++) {
		if (oldest == -1.0 || scroll_results[i].time < oldest) {
			next_index = i;
			oldest = scroll_results[i].time;
		}
		if (win == scroll_results[i].win) {
			index = i;
			break;
		}
	}

	if (set == SCR_SUCCESS) {
		set = 1;
	} else if (set == SCR_FAIL) {
		set = -1;
	} else {
		set = 0;
	}
	if (index == -1) {
		scroll_results[next_index].win = win;
		scroll_results[next_index].time = dnow();
		scroll_results[next_index].result = set;
	} else {
		if (scroll_results[index].result == 1) {
			/*
			 * once a success, always a success, until they
			 * forget about us...
			 */
			set = 1;
		} else {
			scroll_results[index].time = dnow();
			scroll_results[index].result = set;
		}
	}
	return set;
}

int eat_pointer(int max_ptr_eat, int keep) {
	int i, count = 0,  gp = got_pointer_input;

	for (i=0; i<max_ptr_eat; i++) {
		rfbCFD(0);
		if (got_pointer_input > gp)  {
			count++;
if (0) fprintf(stderr, "GP*-%d\n", i);
			gp = got_pointer_input;
		} else if (i > keep) {
			break;
		}
	}
	return count;
}

void set_bdpush(int type, double *last_bdpush, int *pushit) {
	double now, delay = 0.0;
	int link, latency, netrate;

	*pushit = 0;

	if (type == SCR_MOUSE) {
		delay = scr_mouse_bdpush_time;
	} else if (type == SCR_KEY) {
		delay = scr_key_bdpush_time;
	}

	link = link_rate(&latency, &netrate);
	if (link == LR_DIALUP) {
		delay *= 1.5;
	} else if (link == LR_BROADBAND) {
		delay *= 1.25;
	}

	dtime0(&now);
	if (delay > 0.0 && now > *last_bdpush + delay) {
		*pushit = 1;
		*last_bdpush = now;
	}
}

void mark_for_xdamage(int x, int y, int w, int h) {
	int tx1, ty1, tx2, ty2;
	sraRegionPtr tmpregion;

	if (! use_xdamage) {
		return;
	}

	tx1 = nfix(x, dpy_x);
	ty1 = nfix(y, dpy_y);
	tx2 = nfix(x + w, dpy_x+1);
	ty2 = nfix(y + h, dpy_y+1);

	tmpregion = sraRgnCreateRect(tx1, ty1, tx2, ty2);
	add_region_xdamage(tmpregion);
	sraRgnDestroy(tmpregion);
}

void mark_region_for_xdamage(sraRegionPtr region) {
	sraRectangleIterator *iter;
	sraRect rect;
	iter = sraRgnGetIterator(region);
	while (sraRgnIteratorNext(iter, &rect)) {
		int x1 = rect.x1;
		int y1 = rect.y1;
		int x2 = rect.x2;
		int y2 = rect.y2;
		mark_for_xdamage(x1, y1, x2 - x1, y2 - y1);
	}
	sraRgnReleaseIterator(iter);
}

void set_xdamage_mark(int x, int y, int w, int h) {
	sraRegionPtr region;

	if (! use_xdamage) {
		return;
	}
	mark_for_xdamage(x, y, w, h);

	if (xdamage_scheduled_mark == 0.0) {
		xdamage_scheduled_mark = dnow() + 2.0;
	}

	if (xdamage_scheduled_mark_region == NULL) {
		xdamage_scheduled_mark_region = sraRgnCreate();
	}
	region = sraRgnCreateRect(x, y, x + w, y + w);
	sraRgnOr(xdamage_scheduled_mark_region, region);
	sraRgnDestroy(region);
}

int check_xrecord_keys(void) {
	static int last_wx, last_wy, last_ww, last_wh;
	double spin = 0.0, tm, tnow;
	int scr_cnt = 0, input = 0, scroll_rep;
	int get_out, got_one = 0, flush1 = 0, flush2 = 0;
	int gk, gk0, ret = 0, db = debug_scroll;
	int fail = 0;
	int link, latency, netrate;

	static double last_key_scroll = 0.0;
	static double persist_start = 0.0;
	static double last_bdpush = 0.0;
	static int persist_count = 0;
	double last_scroll, scroll_persist = scr_key_persist;
	double spin_fac = 1.0, scroll_fac = 2.0;
	double max_spin, max_long_spin = 0.3;
	double set_repeat_in;
	static double set_repeat = 0.0;

	set_repeat_in = set_repeat;
	set_repeat = 0.0;

	get_out = 1;
	if (got_keyboard_input) {
		get_out = 0;
	} 

	dtime0(&tnow);
	if (tnow < last_key_scroll + scroll_persist) {
		get_out = 0;
	}

	if (set_repeat_in > 0.0 && tnow < last_key_scroll + set_repeat_in) {
		get_out = 0;
	}

	if (get_out) {
		persist_start = 0.0;
		persist_count = 0;
		last_bdpush = 0.0;
		if (xrecording) {
			xrecord_watch(0, SCR_KEY);
		}
		return 0;
	}

	scroll_rep = scrollability(xrecord_ptr_window, SCR_NONE) + 1;

	max_spin = scr_key_time;

	if (set_repeat_in > 0.0 && tnow < last_key_scroll + 2*set_repeat_in) {
		max_spin = 2 * set_repeat_in;
	} else if (tnow < last_key_scroll + scroll_persist) {
		max_spin = 1.25*(tnow - last_key_scroll);
	} else if (tnow < last_key_to_button_remap_time + scroll_persist) {
		/* mostly a hack I use for testing -remap key -> btn4/btn5 */
		max_spin = scroll_persist;
	} else if (xrecord_scroll_keysym(last_rfb_keysym)) {
		spin_fac = scroll_fac;
	}
	if (max_spin > max_long_spin) {
		max_spin = max_long_spin;
	}

	/* XXX use this somehow  */
if (0)	link = link_rate(&latency, &netrate);

	gk = gk0 = got_keyboard_input;
	dtime0(&tm);

if (db) fprintf(stderr, "check_xrecord_keys: BEGIN LOOP: scr_ev_cnt: "
    "%d max: %.3f  %.4f\n", scr_ev_cnt, max_spin, tm - x11vnc_start);

	while (1) {

		if (scr_ev_cnt) {
			got_one = 1;

			scrollability(xrecord_ptr_window, SCR_SUCCESS);
			scroll_rep = 2;

			dtime0(&last_scroll);
			last_key_scroll = last_scroll;
			scr_cnt++;
			break;
		}

		X_LOCK;
		flush1 = 1;
		XFlush(dpy);
		X_UNLOCK;

		if (set_repeat_in > 0.0) {
			max_keyrepeat_time = set_repeat_in;
		}

		if (use_threads) {
			usleep(1000);
		} else {
			rfbCFD(1000);
		}
		spin += dtime(&tm);

		X_LOCK;
		if (got_keyboard_input > gk) {
			gk = got_keyboard_input;
			input++;
			if (set_repeat_in) {
				;
			} else if (xrecord_scroll_keysym(last_rfb_keysym)) {
				spin_fac = scroll_fac;
			}
if (0 || db) fprintf(stderr, "check_xrecord: more keys: %.3f  0x%x "
    " %.4f  %s  %s\n", spin, last_rfb_keysym, last_rfb_keytime - x11vnc_start,
    last_rfb_down ? "down":"up  ", last_rfb_key_accepted ? "accept":"skip");
			flush2 = 1;
			XFlush(dpy);
		}
		SCR_LOCK;
		XRecordProcessReplies(rdpy_data);
		SCR_UNLOCK;
		X_UNLOCK;

		if (spin >= max_spin * spin_fac) {
if (0 || db) fprintf(stderr, "check_xrecord: SPIN-OUT: %.3f/%.3f\n", spin,
    max_spin * spin_fac);
			fail = 1;
			break;
		}
	}

	max_keyrepeat_time = 0.0;

	if (scr_ev_cnt) {
		int dret, ev = scr_ev_cnt - 1;
		int bdx, bdy, bdskinny, bdpush = 0;
		double max_age = 0.25, age, tm, dt;
		static double last_scr_ev = 0.0;

		last_wx = scr_ev[ev].win_x;
		last_wy = scr_ev[ev].win_y;
		last_ww = scr_ev[ev].win_w;
		last_wh = scr_ev[ev].win_h;

		/* assume scrollbar on rhs: */
		bdx = last_wx + last_ww + 3;
		bdy = last_wy + last_wh/2;
		bdskinny = 32;
			
		if (persist_start == 0.0) {
			bdpush = 0;
		} else {
			set_bdpush(SCR_KEY, &last_bdpush, &bdpush);
		}

		dtime0(&tm);
		age = max_age;
		dret = push_scr_ev(&age, SCR_KEY, bdpush, bdx, bdy, bdskinny);
		dt = dtime(&tm);

		ret = 1 + dret;
		scr_ev_cnt = 0;

		if (ret == 2 && xrecord_scroll_keysym(last_rfb_keysym)) {
			int repeating;
			double time_lo = 1.0/max_keyrepeat_lo;
			double time_hi = 1.0/max_keyrepeat_hi;
			double rate = typing_rate(0.0, &repeating);
if (0 || db) fprintf(stderr, "Typing: dt: %.4f rate: %.1f\n", dt, rate);
			if (repeating) {
				/* n.b. the "quantum" is about 1/30 sec. */
				max_keyrepeat_time = 1.0*dt;
				if (max_keyrepeat_time > time_lo ||
				    max_keyrepeat_time < time_hi) {
					max_keyrepeat_time = 0.0;
				} else {
					set_repeat = max_keyrepeat_time;
if (0 || db) fprintf(stderr, "set max_keyrepeat_time: %.2f\n", max_keyrepeat_time);
				}
			}
		}

		last_scr_ev = dnow();
	}

	if ((got_one && ret < 2) || persist_count) {
		set_xdamage_mark(last_wx, last_wy, last_ww, last_wh);
	}

	if (fail) {
		scrollability(xrecord_ptr_window, SCR_FAIL);
	}

	if (xrecording) {
		if (ret < 2) {
			xrecord_watch(0, SCR_KEY);
		}
	}

	if (ret == 2) {
		if (persist_start == 0.0) {
			dtime(&persist_start);
			last_bdpush = persist_start;
		}
	} else {
		persist_start = 0.0;
		last_bdpush = 0.0;
	}

	/* since we've flushed it, we might as well avoid -input_skip */
	if (flush1 || flush2) {
		got_keyboard_input = 0;
		got_pointer_input = 0;
	}

	return ret;
}

int check_xrecord_mouse(void) {
	static int last_wx, last_wy, last_ww, last_wh;
	double spin = 0.0, tm, tnow;
	int i, scr_cnt = 0, input = 0, scroll_rep;
	int get_out, got_one = 0, flush1 = 0, flush2 = 0;
	int gp, gp0, ret = 0, db = debug_scroll;
	int gk, gk0;
	int fail = 0;
	int link, latency, netrate;

	int start_x, start_y, last_x, last_y;
	static double last_mouse_scroll = 0.0;
	double last_scroll;
	double max_spin[3], max_long[3], persist[3];
	double flush1_time = 0.01;
	static double last_flush = 0.0;
	double last_bdpush = 0.0, button_up_time = 0.0;
	int button_mask_save;
	int already_down = 0, max_ptr_eat = 20;
	static int want_back_in = 0;
	int came_back_in;

	int scroll_wheel = 0;
	int btn4 = (1<<3);
	int btn5 = (1<<4);

	get_out = 1;
	if (button_mask) {
		get_out = 0;
	} 
	if (want_back_in) {
		get_out = 0;
	} 
	dtime0(&tnow);
if (0) fprintf(stderr, "check_xrecord_mouse: IN xrecording: %d\n", xrecording);

	if (get_out) {
		if (xrecording) {
			xrecord_watch(0, SCR_MOUSE);
		}
		return 0;
	}

	scroll_rep = scrollability(xrecord_ptr_window, SCR_NONE) + 1;

	if (button_mask_prev) {
		already_down = 1;
	}
	if (want_back_in) {
		came_back_in = 1;
	} else {
		came_back_in = 0;
	}
	want_back_in = 0;

	if (button_mask & (btn4|btn5)) {
		scroll_wheel = 1;
	}

	/*
	 * set up times for the various "reputations"
	 *
	 * 0 => -1, has been tried but never found a scroll.
	 * 1 =>  0, has not been tried.
	 * 2 => +1, has been tried and found a scroll.
	 */

	/* first spin-out time (no events) */
	max_spin[0] = 1*scr_mouse_time;
	max_spin[1] = 2*scr_mouse_time;
	max_spin[2] = 4*scr_mouse_time;
	if (!already_down) {
		for (i=0; i<3; i++) {
			max_spin[i] *= 1.5;
		}
	}

	/* max time between events */
	persist[0] = 1*scr_mouse_persist;
	persist[1] = 2*scr_mouse_persist;
	persist[2] = 4*scr_mouse_persist;

	/* absolute max time in the loop */
	max_long[0] = scr_mouse_maxtime;
	max_long[1] = scr_mouse_maxtime;
	max_long[2] = scr_mouse_maxtime;

	pointer_flush_delay = scr_mouse_pointer_delay;

	/* slow links: */
	link = link_rate(&latency, &netrate);
	if (link == LR_DIALUP) {
		for (i=0; i<3; i++) {
			max_spin[i] *= 2.0;
		}
		pointer_flush_delay *= 2;
	} else if (link == LR_BROADBAND) {
		pointer_flush_delay *= 2;
	}

	gp = gp0 = got_pointer_input;
	gk = gk0 = got_keyboard_input;
	dtime0(&tm);

	/*
	 * this is used for border pushes (bdpush) to guess location
	 * of scrollbar (region rects containing this point are pushed).
	 */
	last_x = start_x = cursor_x;
	last_y = start_y = cursor_y;

if (db) fprintf(stderr, "check_xrecord_mouse: BEGIN LOOP: scr_ev_cnt: "
    "%d max: %.3f  %.4f\n", scr_ev_cnt, max_spin[scroll_rep], tm - x11vnc_start);

	while (1) {
		double spin_check;
		if (scr_ev_cnt) {
			int dret, ev = scr_ev_cnt - 1;
			int bdpush = 0, bdx, bdy, bdskinny;
			double tm, dt, age = 0.35;

			got_one = 1;
			scrollability(xrecord_ptr_window, SCR_SUCCESS);
			scroll_rep = 2;

			scr_cnt++;

			dtime0(&last_scroll);
			last_mouse_scroll = last_scroll;

			if (last_bdpush == 0.0) {
				last_bdpush = last_scroll;
			}

			bdx = start_x;
			bdy = start_y;
			bdskinny = 32;
			
			set_bdpush(SCR_MOUSE, &last_bdpush, &bdpush);

			dtime0(&tm);

			dret = push_scr_ev(&age, SCR_MOUSE, bdpush, bdx,
			    bdy, bdskinny);
			ret = 1 + dret;

			dt = dtime(&tm);

if (db) fprintf(stderr, "  dret: %d  scr_ev_cnt: %d dt: %.4f\n",
	dret, scr_ev_cnt, dt);

			last_wx = scr_ev[ev].win_x;
			last_wy = scr_ev[ev].win_y;
			last_ww = scr_ev[ev].win_w;
			last_wh = scr_ev[ev].win_h;
			scr_ev_cnt = 0;

			if (! dret) {
				break;
			}
			if (0 && button_up_time > 0.0) {
				/* we only take 1 more event with button up */
if (db) fprintf(stderr, "check_xrecord: BUTTON_UP_SCROLL: %.3f\n", spin);
				break;
			}
		}


		if (! flush1) {
			if (! already_down || (!scr_cnt && spin>flush1_time)) {
				flush1 = 1;
				X_LOCK;
				XFlush(dpy);
				X_UNLOCK;
				dtime0(&last_flush);
			}
		}

		if (use_threads) {
			usleep(1000);
		} else {
			rfbCFD(1000);
			rfbCFD(0);
		}
		spin += dtime(&tm);

		if (got_pointer_input > gp) {
			flush2 = 1;
			input += eat_pointer(max_ptr_eat, 1);
			gp = got_pointer_input;
		}
		if (got_keyboard_input > gk) {
			gk = got_keyboard_input;
			input++;
		}
		X_LOCK;
		SCR_LOCK;
		XRecordProcessReplies(rdpy_data);
		SCR_UNLOCK;
		X_UNLOCK;

		if (! input) {
			spin_check = 1.5 * max_spin[scroll_rep];
		} else {
			spin_check = max_spin[scroll_rep];
		}

		if (button_up_time > 0.0) {
			if (tm > button_up_time + max_spin[scroll_rep]) {
if (db) fprintf(stderr, "check_xrecord: SPIN-OUT-BUTTON_UP: %.3f/%.3f\n", spin, tm - button_up_time);
				break;
			}
		} else if (!scr_cnt) {
			if (spin >= spin_check) {

if (db) fprintf(stderr, "check_xrecord: SPIN-OUT-1: %.3f/%.3f\n", spin, spin_check);
				fail = 1;
				break;
			}
		} else {
			if (tm >= last_scroll + persist[scroll_rep]) {

if (db) fprintf(stderr, "check_xrecord: SPIN-OUT-2: %.3f/%.3f\n", spin, tm - last_scroll);
				break;
			}
		}
		if (spin >= max_long[scroll_rep]) {

if (db) fprintf(stderr, "check_xrecord: SPIN-OUT-3: %.3f/%.3f\n", spin, max_long[scroll_rep]);
			break;
		}

		if (! button_mask) {
			int doflush = 0;
			if (button_up_time > 0.0) {
				;
			} else if (came_back_in) {
				dtime0(&button_up_time);
				doflush = 1;
			} else if (scroll_wheel) {
if (db) fprintf(stderr, "check_xrecord: SCROLL-WHEEL-BUTTON-UP-KEEP-GOING:  %.3f/%.3f %d/%d %d/%d\n", spin, max_long[scroll_rep], last_x, last_y, cursor_x, cursor_y);
				doflush = 1;
				dtime0(&button_up_time);
			} else if (last_x == cursor_x && last_y == cursor_y) {
if (db) fprintf(stderr, "check_xrecord: BUTTON-UP:  %.3f/%.3f %d/%d %d/%d\n", spin, max_long[scroll_rep], last_x, last_y, cursor_x, cursor_y);
				break;
			} else {
if (db) fprintf(stderr, "check_xrecord: BUTTON-UP-KEEP-GOING:  %.3f/%.3f %d/%d %d/%d\n", spin, max_long[scroll_rep], last_x, last_y, cursor_x, cursor_y);
				doflush = 1;
				dtime0(&button_up_time);
			}
			if (doflush) {
				flush1 = 1;
				X_LOCK;
				XFlush(dpy);
				X_UNLOCK;
				dtime0(&last_flush);
			}
		}

		last_x = cursor_x;
		last_y = cursor_y;
	}

	if (got_one) {
		set_xdamage_mark(last_wx, last_wy, last_ww, last_wh);
	}

	if (fail) {
		scrollability(xrecord_ptr_window, SCR_FAIL);
	}

	/* flush any remaining pointer events. */
	button_mask_save = button_mask;
	pointer_queued_sent = 0;
	last_x = cursor_x;
	last_y = cursor_y;
	pointer(-1, 0, 0, NULL);
	pointer_flush_delay = 0.0;

	if (xrecording && pointer_queued_sent && button_mask_save &&
	    (last_x != cursor_x || last_y != cursor_y) ) {
if (db) fprintf(stderr, "  pointer() push yields events on: ret=%d\n", ret);
		if (ret == 2) {
if (db) fprintf(stderr, "  we decide to send ret=3\n");
			want_back_in = 1;
			ret = 3;
			flush2 = 1;
		} else {
			if (ret) {
				ret = 1;
			} else {
				ret = 0;
			}
			xrecord_watch(0, SCR_MOUSE);
		}
	} else {
		if (ret) {
			ret = 1;
		} else {
			ret = 0;
		}
		if (xrecording) {
			xrecord_watch(0, SCR_MOUSE);
		}
	}

	if (flush2) {
		X_LOCK;
		XFlush(dpy);
		XFlush(rdpy_ctrl);
		X_UNLOCK;

		flush2 = 1;
		dtime0(&last_flush);

if (db) fprintf(stderr, "FLUSH-2\n");
	}

	/* since we've flushed it, we might as well avoid -input_skip */
	if (flush1 || flush2) {
		got_keyboard_input = 0;
		got_pointer_input = 0;
	}

	if (ret) {
		return ret;
	} else if (scr_cnt) {
		return 1;
	} else {
		return 0;
	}
}

int check_xrecord(void) {
	int watch_keys = 0, watch_mouse = 0, consider_mouse;
	static int first = 1;
	static int mouse_wants_back_in = 0;

	if (first) {
		int i;
		for (i=0; i<SCR_RESULTS_MAX; i++) {
			scroll_results[i].win = None;
			scroll_results[i].time = 0.0;
			scroll_results[i].result = 0;
		}
		first = 0;
	}

	if (! use_xrecord) {
		return 0;
	}
	if (scaling && ! got_scrollcopyrect) {
		return 0;
	}

if (0) fprintf(stderr, "check_xrecord: IN xrecording: %d\n", xrecording);

	if (! xrecording) {
		return 0;
	}

	if (!strcmp(scroll_copyrect, "always")) {
		watch_keys = 1;
		watch_mouse = 1;
	} else if (!strcmp(scroll_copyrect, "keys")) {
		watch_keys = 1;
	} else if (!strcmp(scroll_copyrect, "mouse")) {
		watch_mouse = 1;
	}

	if (button_mask || mouse_wants_back_in) {
		consider_mouse = 1;
	} else {
		consider_mouse = 0;
	}
if (0) fprintf(stderr, "check_xrecord: button_mask: %d  mouse_wants_back_in: %d\n", button_mask, mouse_wants_back_in);

	if (watch_mouse && consider_mouse && xrecord_set_by_mouse) {
		int ret = check_xrecord_mouse();
		if (ret == 3) {
			mouse_wants_back_in = 1;
		} else {
			mouse_wants_back_in = 0;
		}
		return ret;
	} else if (watch_keys && xrecord_set_by_keys) {
		mouse_wants_back_in = 0;
		return check_xrecord_keys();
	} else {
		mouse_wants_back_in = 0;
		return 0;
	}
}

#define DB_SET \
	int db  = 0; \
	int db2 = 0; \
	if (debug_wireframe == 1) { \
		db = 1; \
	} \
	if (debug_wireframe == 2) { \
		db2 = 1; \
	} \
	if (debug_wireframe == 3) { \
		db = 1; \
		db2 = 1; \
	}

int try_copyrect(Window frame, int x, int y, int w, int h, int dx, int dy,
    int *obscured, sraRegionPtr extra_clip, double max_wait) {

	static int dt_bad = 0;
	static time_t dt_bad_check = 0;
	int x1, y1, x2, y2, sent_copyrect = 0;
	int req, mod, cpy, ncli;
	double tm, dt;
	DB_SET

	get_client_regions(&req, &mod, &cpy, &ncli);
	if (cpy) {
		/* one is still pending... try to force it out: */
		fb_push_wait(max_wait, FB_COPY);

		get_client_regions(&req, &mod, &cpy, &ncli);
	}
	if (cpy) {
		return 0;
	}

	*obscured = 0;
	/*
	 * XXX KDE and xfce do some weird things with the 
	 * stacking, it does not match XQueryTree.  Work around
	 * it for now by CopyRect-ing the *whole* on-screen 
	 * rectangle (whether obscured or not!)
	 */
	if (time(0) > dt_bad_check + 5) {
		char *dt = guess_desktop();
		if (!strcmp(dt, "kde")) {
			dt_bad = 1;
		} else if (!strcmp(dt, "xfce")) {
			dt_bad = 1;
		} else {
			dt_bad = 0;
		}
		dt_bad_check = time(0);
	}

	if (0 && dt_bad) {
		sraRegionPtr rect;
		/* send the whole thing... */
		x1 = crfix(nfix(x,   dpy_x), dx, dpy_x);
		y1 = crfix(nfix(y,   dpy_y), dy, dpy_y);
		x2 = crfix(nfix(x+w, dpy_x+1), dx, dpy_x+1);
		y2 = crfix(nfix(y+h, dpy_y+1), dy, dpy_y+1);

		rect = sraRgnCreateRect(x1, y1, x2, y2);
		do_copyregion(rect, dx, dy);
		sraRgnDestroy(rect);

		sent_copyrect = 1;
		*obscured = 1;	/* set to avoid an aggressive push */

	} else if (stack_list_num || dt_bad) {
		int k, tx1, tx2, ty1, ty2;
		sraRegionPtr moved_win, tmp_win, whole;
		sraRectangleIterator *iter;
		sraRect rect;
		int saw_me = 0;
		int orig_x, orig_y;
		XWindowAttributes attr;

		orig_x = x - dx;
		orig_y = y - dy;

		tx1 = nfix(orig_x,   dpy_x);
		ty1 = nfix(orig_y,   dpy_y);
		tx2 = nfix(orig_x+w, dpy_x+1);
		ty2 = nfix(orig_y+h, dpy_y+1);

if (db2) fprintf(stderr, "moved_win: %4d %3d, %4d %3d  0x%lx ---\n",
	tx1, ty1, tx2, ty2, frame);

		moved_win = sraRgnCreateRect(tx1, ty1, tx2, ty2);

		dtime0(&tm);

		X_LOCK;

		/*
		 * loop over the stack, top to bottom until we
		 * find our wm frame:
		 */
		for (k = stack_list_num - 1; k >= 0; k--) {
			Window swin;

			if (dt_bad) {
				break;
			}

			swin = stack_list[k].win;
			if (swin == frame) {
if (db2) {
saw_me = 1; fprintf(stderr, "  ----------\n");
} else {
				break;	
}
			}

			/* skip some unwanted cases: */
			if (swin == None) {
				continue;
			}
			if (! stack_list[k].fetched ||
			    stack_list[k].time > tm + 2.0) {
				if (!valid_window(swin, &attr, 1)) {
					stack_list[k].valid = 0;
				} else {
					stack_list[k].valid = 1;
					stack_list[k].x = attr.x;
					stack_list[k].y = attr.y;
					stack_list[k].width = attr.width;
					stack_list[k].height = attr.height;
					stack_list[k].depth = attr.depth;
					stack_list[k].class = attr.class;
					stack_list[k].backing_store =
					    attr.backing_store;
					stack_list[k].map_state =
					    attr.map_state;
				}
				stack_list[k].fetched = 1;
				stack_list[k].time = tm;
			}
			if (!stack_list[k].valid) {
				continue;
			}

			attr.x      = stack_list[k].x;
			attr.y      = stack_list[k].y;
			attr.depth  = stack_list[k].depth;
			attr.width  = stack_list[k].width;
			attr.height = stack_list[k].height;
			attr.map_state = stack_list[k].map_state;

			if (attr.map_state != IsViewable) {
				continue;
			}

			/*
			 * first subtract any overlap from the initial
			 * window rectangle
			 */

			/* clip the window to the visible screen: */
			tx1 = nfix(attr.x, dpy_x);
			ty1 = nfix(attr.y, dpy_y);
			tx2 = nfix(attr.x + attr.width,  dpy_x+1);
			ty2 = nfix(attr.y + attr.height, dpy_y+1);

if (db2) fprintf(stderr, "  tmp_win-1: %4d %3d, %4d %3d  0x%lx\n",
	tx1, ty1, tx2, ty2, swin);
if (db2 && saw_me) continue;

			/* see if window clips us: */
			tmp_win = sraRgnCreateRect(tx1, ty1, tx2, ty2);
			if (sraRgnAnd(tmp_win, moved_win)) {
				*obscured = 1;
if (db2) fprintf(stderr, "         : clips it.\n");
			}
			sraRgnDestroy(tmp_win);

			/* subtract it from our region: */
			tmp_win = sraRgnCreateRect(tx1, ty1, tx2, ty2);
			sraRgnSubtract(moved_win, tmp_win);
			sraRgnDestroy(tmp_win);

			/*
			 * next, subtract from the initial window rectangle
			 * anything that would clip it.
			 */

			/* clip the window to the visible screen: */
			tx1 = nfix(attr.x - dx, dpy_x);
			ty1 = nfix(attr.y - dy, dpy_y);
			tx2 = nfix(attr.x - dx + attr.width,  dpy_x+1);
			ty2 = nfix(attr.y - dy + attr.height, dpy_y+1);

if (db2) fprintf(stderr, "  tmp_win-2: %4d %3d, %4d %3d  0x%lx\n",
	tx1, ty1, tx2, ty2, swin);
if (db2 && saw_me) continue;

			/* subtract it from our region: */
			tmp_win = sraRgnCreateRect(tx1, ty1, tx2, ty2);
			sraRgnSubtract(moved_win, tmp_win);
			sraRgnDestroy(tmp_win);
		}
		X_UNLOCK;

		if (extra_clip && ! sraRgnEmpty(extra_clip)) {
		    whole = sraRgnCreateRect(0, 0, dpy_x, dpy_y);

		    iter = sraRgnGetIterator(extra_clip);
		    while (sraRgnIteratorNext(iter, &rect)) {
			/* clip the window to the visible screen: */
			tx1 = rect.x1;
			ty1 = rect.y1;
			tx2 = rect.x2;
			ty2 = rect.y2;
			tmp_win = sraRgnCreateRect(tx1, ty1, tx2, ty2);
			sraRgnAnd(tmp_win, whole);

			/* see if window clips us: */
			if (sraRgnAnd(tmp_win, moved_win)) {
				*obscured = 1;
			}
			sraRgnDestroy(tmp_win);

			/* subtract it from our region: */
			tmp_win = sraRgnCreateRect(tx1, ty1, tx2, ty2);
			sraRgnSubtract(moved_win, tmp_win);
			sraRgnDestroy(tmp_win);

			/*
			 * next, subtract from the initial window rectangle
			 * anything that would clip it.
			 */
			tmp_win = sraRgnCreateRect(tx1, ty1, tx2, ty2);
			sraRgnOffset(tmp_win, -dx, -dy);

			/* clip the window to the visible screen: */
			sraRgnAnd(tmp_win, whole);

			/* subtract it from our region: */
			sraRgnSubtract(moved_win, tmp_win);
			sraRgnDestroy(tmp_win);
		    }
		    sraRgnReleaseIterator(iter);
		    sraRgnDestroy(whole);
		}

		dt = dtime(&tm);
if (db2) fprintf(stderr, "  stack_work dt: %.4f\n", dt);

		if (*obscured && !strcmp(wireframe_copyrect, "top")) {
			;	/* cannot send CopyRegion */
		} else if (! sraRgnEmpty(moved_win)) {
			sraRegionPtr whole, shifted_region;

			whole = sraRgnCreateRect(0, 0, dpy_x, dpy_y);
			shifted_region = sraRgnCreateRgn(moved_win);
			sraRgnOffset(shifted_region, dx, dy);
			sraRgnAnd(shifted_region, whole);

			sraRgnDestroy(whole);

			/* now send the CopyRegion: */
			if (! sraRgnEmpty(shifted_region)) {
				dtime0(&tm);
				do_copyregion(shifted_region, dx, dy);
				dt = dtime(&tm);
if (db2) fprintf(stderr, "do_copyregion: %d %d %d %d  dx: %d  dy: %d dt: %.4f\n",
	tx1, ty1, tx2, ty2, dx, dy, dt);
				sent_copyrect = 1;
			}
			sraRgnDestroy(shifted_region);
		}
		sraRgnDestroy(moved_win);
	}
	return sent_copyrect;
}
		
int near_wm_edge(int x, int y, int w, int h, int px, int py) {
	/* heuristics: */
	int wf_t = wireframe_top;
	int wf_b = wireframe_bot;
	int wf_l = wireframe_left;
	int wf_r = wireframe_right;

	int near_edge = 0;
	
	if (wf_t || wf_b || wf_l || wf_r) {
		if (nabs(y - py) < wf_t) {
			near_edge = 1;
		}
		if (nabs(y + h - py) < wf_b) {
			near_edge = 1;
		}
		if (nabs(x - px) < wf_l) {
			near_edge = 1;
		}
		if (nabs(x + w - px) < wf_r) {
			near_edge = 1;
		}
	} else {
		/* all zero; always "near" edge: */
		near_edge = 1;
	}
	return near_edge;
}

int near_scrollbar_edge(int x, int y, int w, int h, int px, int py) {
	/* heuristics: */
	int sb_t = scrollcopyrect_top;
	int sb_b = scrollcopyrect_bot;
	int sb_l = scrollcopyrect_left;
	int sb_r = scrollcopyrect_right;

	int near_edge = 0;
	
	if (sb_t || sb_b || sb_l || sb_r) {
		if (nabs(y - py) < sb_t) {
			near_edge = 1;
		}
		if (nabs(y + h - py) < sb_b) {
			near_edge = 1;
		}
		if (nabs(x - px) < sb_l) {
			near_edge = 1;
		}
		if (nabs(x + w - px) < sb_r) {
			near_edge = 1;
		}
	} else {
		/* all zero; always "near" edge: */
		near_edge = 1;
	}
	return near_edge;
}

/*
 * Applied just before any check_user_input() modes.  Look for a
 * ButtonPress; find window it happened in; find the wm frame window
 * for it; watch for that window moving or resizing.  If it does, do the
 * wireframe animation.  Do this until ButtonRelease or timeouts occur.
 * Remove wireframe.
 *
 * Under -nowirecopyrect, return control to base scheme
 * (check_user_input() ...) that will repaint the screen with the window
 * in the new postion or size.  Under -wirecopyrect, apply rfbDoCopyRect
 * or rfbDoCopyRegion: this "pollutes" our framebuffer, but the normal
 * polling will quickly repair it. Under happy circumstances, this
 * reduces actual XShmGetImage work (i.e. if we correctly predicted how
 * the X fb has changed.
 *
 * -scale doesn't always work under -wirecopyrect, but the wireframe does.
 *
 * testing of this mode under -threads is incomplete.
 *
 * returns 1 if it did an animation, 0 if no move/resize triggers
 * went off.
 *
 * TBD: see if we can select StructureNotify ConfigureNotify events for
 * the toplevel windows to get better info on moves and resizes.
 */
int check_wireframe(void) {
	Window frame, orig_frame;
	XWindowAttributes attr;
	int dx, dy;

	int orig_px, orig_py, orig_x, orig_y, orig_w, orig_h;
	int px, py, x, y, w, h;
	int box_x, box_y, box_w, box_h;
	int orig_cursor_x, orig_cursor_y, g;
	int already_down = 0, win_gone = 0, win_unmapped = 0;
	double spin = 0.0, tm, last_ptr, last_draw;
	int frame_changed = 0, drew_box = 0, got_2nd_pointer = 0;
	int special_t1 = 0, break_reason = 0;
	static double first_dt_ave = 0.0;
	static int first_dt_cnt = 0;
	static time_t last_save_stacklist = 0;
	
	/* heuristics: */
	double first_event_spin   = wireframe_t1;
	double frame_changed_spin = wireframe_t2;
	double max_spin = wireframe_t3;
	double min_draw = wireframe_t4;
	int near_edge;
	DB_SET

	if (nofb) {
		return 0;
	}
	if (subwin) {
		return 0;	/* don't even bother for -id case */
	}
	if (! button_mask) {
		return 0;	/* no button pressed down */
	}
	if (!use_threads && !got_pointer_input) {
		return 0;	/* need ptr input, e.g. button down, motion */
	}

if (db) fprintf(stderr, "\n*** button down!!  x: %d  y: %d\n", cursor_x, cursor_y);

	/*
	 * Query where the pointer is and which child of the root
	 * window.  We will assume this is the frame the window manager
	 * makes when it reparents the toplevel window.
	 */
	X_LOCK;
	if (! get_wm_frame_pos(&px, &py, &x, &y, &w, &h, &frame, NULL)) {
if (db) fprintf(stderr, "NO get_wm_frame_pos: 0x%lx\n", frame);
		X_UNLOCK;
		return 0;
	}
	X_UNLOCK;
if (db) fprintf(stderr, "a: %d  wf: %.3f  A: %d\n", w*h, wireframe_frac, (dpy_x*dpy_y));

	/*
	 * apply the percentage size criterion (allow opaque moves for
	 * small windows)
	 */
	if ((double) w*h < wireframe_frac * (dpy_x * dpy_y)) {
if (db) fprintf(stderr, "small window %.3f\n", ((double) w*h)/(dpy_x * dpy_y));
		return 0;
	}
if (db) fprintf(stderr, "  frame: x: %d  y: %d  w: %d  h: %d  px: %d  py: %d  fr: 0x%lx\n", x, y, w, h, px, py, orig_frame);	

	/*
	 * see if the pointer is within range of the assumed wm frame
	 * decorations on the edge of the window.
	 */

	near_edge = near_wm_edge(x, y, w, h, px, py);
	if (! near_edge) {
if (db) fprintf(stderr, "INTERIOR\n");
		return 0;
	}

	wireframe_in_progress = 1;

	if (button_mask_prev) {
		already_down = 1;
	}
	
	if (! wireframe_str || !strcmp(wireframe_str, WIREFRAME_PARMS)) {
		int link, latency, netrate;
		static int didmsg = 0;

		link = link_rate(&latency, &netrate);
		if (link == LR_DIALUP || link == LR_BROADBAND) {
			/* slow link, e.g. dialup, increase timeouts: */
			first_event_spin   *= 2.0;
			frame_changed_spin *= 2.0;
			max_spin *= 2.0;
			min_draw *= 1.5;
			if (! didmsg) {
				rfbLog("increased wireframe timeouts for "
				    "slow network connection.\n");
				rfbLog("netrate: %d KB/sec, latency: %d ms\n",
				    netrate, latency);
				didmsg = 1;
			}
		}
	}

	/*
	 * pointer() should have snapped the stacking list for us, if
	 * not, do it now (if the XFakeButtonEvent has been flushed by
	 * now the stacking order may be incorrect).
	 */
	if (strcmp(wireframe_copyrect, "never")) {
		if (already_down) {
			double age = 0.0;
			/*
			 * see if we can reuse the stack list (pause
			 * with button down)
			 */
			if (stack_list_num) {
				int k, got_me = 0;
				for (k = stack_list_num -1; k >=0; k--) {
					if (frame == stack_list[k].win) {
						got_me = 1;
						break;
					}
				}
				if (got_me) {
					age = 1.0;
				}
				snapshot_stack_list(0, age);
			}
		}
		if (! stack_list_num) {
			snapshot_stack_list(0, 0.0);
		}
	}


	/* store initial parameters, we look for changes in them */
	orig_frame = frame;
	orig_px = px;		/* pointer position */
	orig_py = py;
	orig_x = x;		/* frame position */
	orig_y = y;
	orig_w = w;		/* frame size */
	orig_h = h;

	orig_cursor_x = cursor_x;
	orig_cursor_y = cursor_y;

	/* this is the box frame we would draw */
	box_x = x;
	box_y = y; 
	box_w = w;
	box_h = h; 

	dtime0(&tm);

	last_draw = spin;

	/* -threads support for check_wireframe() is rough... crash? */
	if (use_threads) {
		/* purge any stored up pointer events: */
		pointer(-1, 0, 0, NULL);
	}

	g = got_pointer_input;

	while (1) {

		X_LOCK;
		XFlush(dpy);
		X_UNLOCK;

		/* try do induce/waitfor some more user input */
		if (use_threads) {
			usleep(1000);
		} else if (drew_box) {
			rfbPE(1000);
		} else {
			rfbCFD(1000);
		}

		spin += dtime(&tm);

		/* check for any timeouts: */
		if (frame_changed) {
			double delay;
			/* max time we play this game: */
			if (spin > max_spin) {
if (db || db2) fprintf(stderr, " SPIN-OUT-MAX: %.3f\n", spin);
				break_reason = 1;
				break;
			}
			/* watch for pointer events slowing down: */
			if (special_t1) {
				delay = max_spin;
			} else {
				delay = 2.0* frame_changed_spin;
				if (spin > 3.0 * frame_changed_spin) {
					delay = 1.5 * delay;
				}
			}
			if (spin > last_ptr + delay) {
if (db || db2) fprintf(stderr, " SPIN-OUT-NOT-FAST: %.3f\n", spin);
				break_reason = 2;
				break;
			}
		} else if (got_2nd_pointer) {
			/*
			 * pointer is moving, max time we wait for wm
			 * move or resize to be detected
			 */
			if (spin > frame_changed_spin) {
if (db || db2) fprintf(stderr, " SPIN-OUT-NOFRAME-SPIN: %.3f\n", spin);
				break_reason = 3;
				break;
			}
		} else {
			/* max time we wait for any pointer input */
			if (spin > first_event_spin) {
if (db || db2) fprintf(stderr, " SPIN-OUT-NO2ND_PTR: %.3f\n", spin);
				break_reason = 4;
				break;
			}
		}

		/* see if some pointer input occurred: */
		if (got_pointer_input > g) {
if (db) fprintf(stderr, "  ++pointer event!! [%02d]  dt: %.3f  x: %d  y: %d  mask: %d\n", got_2nd_pointer+1, spin, cursor_x, cursor_y, button_mask);	

			g = got_pointer_input;

			X_LOCK;
			XFlush(dpy);
			X_UNLOCK;

			/* periodically try to let the wm get moving: */
			if (!frame_changed && got_2nd_pointer % 4 == 0) {
				if (got_2nd_pointer == 0) {
					usleep(50 * 1000);
				} else {
					usleep(25 * 1000);
				}
			}
			got_2nd_pointer++;
			last_ptr = spin;

			/*
			 * see where the pointer currently is.  It may
			 * not be our starting frame (i.e. mouse now
			 * outside of the moving window).
			 */
			frame = 0x0;
			X_LOCK;

			if (! get_wm_frame_pos(&px, &py, &x, &y, &w, &h,
			    &frame, NULL)) {
				frame = 0x0;
if (db) fprintf(stderr, "NO get_wm_frame_pos: 0x%lx\n", frame);
			}

			if (frame != orig_frame) {
				/* see if our original frame is still there */
				if (!valid_window(orig_frame, &attr, 1)) {
					X_UNLOCK;
					/* our window frame went away! */
					win_gone = 1;
if (db) fprintf(stderr, "FRAME-GONE: 0x%lx\n", orig_frame);
					break_reason = 5;
					break;
				}
				if (attr.map_state == IsUnmapped) {
					X_UNLOCK;
					/* our window frame is now unmapped! */
					win_unmapped = 1;
if (db) fprintf(stderr, "FRAME-UNMAPPED: 0x%lx\n", orig_frame);
					break_reason = 5;
					break;
				}

if (db) fprintf(stderr, "OUT-OF-FRAME: old: x: %d  y: %d  px: %d py: %d 0x%lx\n", x, y, px, py, frame);

				/* new parameters for our frame */
				x = attr.x;	/* n.b. rootwin is parent */
				y = attr.y;
				w = attr.width;
				h = attr.height;
			}
			X_UNLOCK;

			/* debugging output, to be removed: */
if (db) fprintf(stderr, "  frame: x: %d  y: %d  w: %d  h: %d  px: %d  py: %d  fr: 0x%lx\n", x, y, w, h, px, py, frame);	
if (db) fprintf(stderr, "        MO,PT,FR: %d/%d %d/%d %d/%d\n", cursor_x - orig_cursor_x, cursor_y - orig_cursor_y, px - orig_px, py - orig_py, x - orig_x, y - orig_y);	

			if (frame_changed && frame != orig_frame) {
if (db) fprintf(stderr, "CHANGED and window switch: 0x%lx\n", frame);
			}
			if (frame_changed && px - orig_px != x - orig_x) {
if (db) fprintf(stderr, "MOVED and diff DX\n");
			}
			if (frame_changed && py - orig_py != y - orig_y) {
if (db) fprintf(stderr, "MOVED and diff DY\n");
			}

			/* check and see if our frame has been resized: */
			if (!frame_changed && (w != orig_w || h != orig_h)) {
				int n;
				if (!already_down) {
					first_dt_ave += spin;
					first_dt_cnt++;
				}
				n = first_dt_cnt ? first_dt_cnt : 1;
				frame_changed = 2;

if (db) fprintf(stderr, "WIN RESIZE  1st-dt: %.3f\n", first_dt_ave/n);
			}

			/* check and see if our frame has been moved: */
			if (!frame_changed && (x != orig_x || y != orig_y)) {
				int n;
				if (!already_down) {
					first_dt_ave += spin;
					first_dt_cnt++;
				}
				n = first_dt_cnt ? first_dt_cnt : 1;
				frame_changed = 1;
if (db) fprintf(stderr, "FRAME MOVE  1st-dt: %.3f\n", first_dt_ave/n);
			}

			/*
			 * see if it is time to draw any or a new wireframe box
			 */
			if (frame_changed) {
				int drawit = 0;
				if (x != box_x || y != box_y) {
					/* moved since last */
					drawit = 1;
				} else if (w != box_w || h != box_h) {
					/* resize since last */
					drawit = 1;
				}
				if (drawit) {
					/*
					 * check time (to avoid too much
					 * animations on slow machines
					 * or links).
					 */
					if (spin > last_draw + min_draw ||
					    ! drew_box) {
						draw_box(x, y, w, h, 0);
						drew_box = 1;
						rfbPE(1000);
						last_draw = spin;
					}
				}
				box_x = x;
				box_y = y;
				box_w = w;
				box_h = h;
			}
		}

		/* 
		 * Now (not earlier) check if the button has come back up.
		 * we check here to get a better location and size of
		 * the final window.
		 */
		if (! button_mask) {
if (db || db2) fprintf(stderr, "NO button_mask\n");
			break_reason = 6;
			break;	
		}
	}

	if (! drew_box) {
		/* nice try, but no move or resize detected.  cleanup. */
		if (stack_list_num) {
			stack_list_num = 0;
		}
		wireframe_in_progress = 0;
		return 0;
	}

	/* remove the wireframe */
	draw_box(0, 0, 0, 0, 1);

	dx = x - orig_x;
	dy = y - orig_y;

	/*
	 * see if we can apply CopyRect or CopyRegion to the change:
	 */
	if (!strcmp(wireframe_copyrect, "never")) {
		;
	} else if (win_gone || win_unmapped) {
		;
	} else if (scaling && ! got_wirecopyrect) {
		;
	} else if (w != orig_w || h != orig_h) {
		;
	} else if (dx == 0 && dy == 0) {
		;
	} else {
		int spin_ms = (int) (spin * 1000 * 1000);
		int obscured, sent_copyrect = 0;

		/*
		 * set a timescale comparable to the spin time,
		 * but not too short or too long.
		 */
		if (spin_ms < 30) {
			spin_ms = 30;
		} else if (spin_ms > 400) {
			spin_ms = 400;
		}

		/* try to flush the wireframe removal: */
		fb_push_wait(0.1, FB_COPY|FB_MOD);

		/* try to send a clipped copyrect of translation: */
		sent_copyrect = try_copyrect(frame, x, y, w, h, dx, dy,
		    &obscured, NULL, 0.15);

if (db) fprintf(stderr, "send_copyrect: %d\n", sent_copyrect);
		if (sent_copyrect) {
			/* try to push the changes to viewers: */
			if (! obscured) {
				fb_push_wait(0.1, FB_COPY);
			} else {
				fb_push_wait(0.1, FB_COPY);
			}
		}
	}

	if (stack_list_num) {
		/* clean up stack_list for next time: */
		if (break_reason == 1 || break_reason == 2) {
			/*
			 * save the stack list, perhaps the user has
			 * paused with button down.
			 */
			last_save_stacklist = time(0);
		} else {
			stack_list_num = 0;
		}
	}

	/* final push (for -nowirecopyrect) */
	rfbPE(1000);
	wireframe_in_progress = 0;
	urgent_update = 1;
	if (use_xdamage) {
		/* DAMAGE can queue ~1000 rectangles for a move */
		clear_xdamage_mark_region(NULL, 1);
		xdamage_scheduled_mark = dnow() + 2.0;
	}

	return 1;
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
 * note: we do this even under -nofb
 *
 * return of 1 means watch_loop should short-circuit and reloop,
 * return of 0 means watch_loop should proceed to scan_for_updates().
 * (this is for pointer_mode == 1 mode, the others do it all internally,
 * cnt is also only for that mode).
 */

static void check_user_input2(double dt) {

	int eaten = 0, miss = 0, max_eat = 50, do_flush = 1;
	int g, g_in;
	double spin = 0.0, tm;
	double quick_spin_fac  = 0.40;
	double grind_spin_time = 0.175;

	dtime0(&tm);
	g = g_in = got_pointer_input;
	if (!got_pointer_input) {
		return;
	}
	/*
	 * Try for some "quick" pointer input processing.
	 *
	 * About as fast as we can, we try to process user input calling
	 * rfbProcessEvents or rfbCheckFds.  We do this for a time on
	 * order of the last scan_for_updates() time, dt, but if we stop
	 * getting user input we break out.  We will also break out if
	 * we have processed max_eat inputs.
	 *
	 * Note that rfbCheckFds() does not send any framebuffer updates,
	 * so is more what we want here, although it is likely they have
	 * all be sent already.
	 */
	while (1) {
		if (show_multiple_cursors) {
			rfbPE(1000);
		} else {
			rfbCFD(1000);
		}
		rfbCFD(0);

		spin += dtime(&tm);

		if (spin > quick_spin_fac * dt) {
			/* get out if spin time comparable to last scan time */
			break;
		}
		if (got_pointer_input > g) {
			int i, max_extra = max_eat / 2;
			g = got_pointer_input;
			eaten++;
			for (i=0; i<max_extra; i++)  {
				rfbCFD(0);
				if (got_pointer_input > g) {
					g = got_pointer_input;
					eaten++;
				} else if (i > 1) {
					break;
				}
			}
			X_LOCK;
			do_flush = 0;
if (0) fprintf(stderr, "check_user_input2-A: XFlush %.4f\n", tm);
			XFlush(dpy);
			X_UNLOCK;
			if (eaten < max_eat) {
				continue;
			}
		} else {
			miss++;
		}
		if (miss > 1) {	/* 1 means out on 2nd miss */
			break;
		}
	}
	if (do_flush) {
		X_LOCK;
if (0) fprintf(stderr, "check_user_input2-B: XFlush %.4f\n", tm);
		XFlush(dpy);
		X_UNLOCK;
	}


	/*
	 * Probably grinding with a lot of fb I/O if dt is this large.
	 * (need to do this more elegantly)
	 *
	 * Current idea is to spin our wheels here *not* processing any
	 * fb I/O, but still processing the user input.  This user input
	 * goes to the X display and changes it, but we don't poll it
	 * while we "rest" here for a time on order of dt, the previous
	 * scan_for_updates() time.  We also break out if we miss enough
	 * user input.
	 */
	if (dt > grind_spin_time) {
		int i, ms, split = 30;
		double shim;

		/*
		 * Break up our pause into 'split' steps.  We get at
		 * most one input per step.
		 */
		shim = 0.75 * dt / split;

		ms = (int) (1000 * shim);

		/* cutoff how long the pause can be */
		if (split * ms > 300) {
			ms = 300 / split;
		}

		spin = 0.0;
		dtime0(&tm);

		g = got_pointer_input;
		miss = 0;
		for (i=0; i<split; i++) {
			usleep(ms * 1000);
			if (show_multiple_cursors) {
				rfbPE(1000);
			} else {
				rfbCFD(1000);
			}
			spin += dtime(&tm);
			if (got_pointer_input > g) {
				int i, max_extra = max_eat / 2;
				for (i=0; i<max_extra; i++)  {
					rfbCFD(0);
					if (got_pointer_input > g) {
						g = got_pointer_input;
					} else if (i > 1) {
						break;
					}
				}
				X_LOCK;
if (0) fprintf(stderr, "check_user_input2-C: XFlush %.4f\n", tm);
				XFlush(dpy);
				X_UNLOCK;
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

static void check_user_input3(double dt, double dtr, int tile_diffs) {

	int allowed_misses, miss_tweak, i, g, g_in;
	int last_was_miss, consecutive_misses;
	double spin, spin_max, tm, to, dtm;
	int rfb_wait_ms = 2;
	static double dt_cut = 0.075;
	int gcnt, ginput;
	static int first = 1;

	if (first) {
		char *p = getenv("SPIN");
		if (p) {
			double junk;
			sscanf(p, "%lf,%lf", &dt_cut, &junk);
		}
		first = 0;
	}

	if (!got_pointer_input) {
		return;
	}


	if (dt < dt_cut) {
		dt = dt_cut;	/* this is to try to avoid early exit */
	}
	spin_max = 0.5;

	spin = 0.0;		/* amount of time spinning */
	allowed_misses = 10;	/* number of ptr inputs we can miss */
	miss_tweak = 8;
	last_was_miss = 0;
	consecutive_misses = 1;
	gcnt = 0;
	ginput = 0;

	dtime0(&tm);
	to = tm;	/* last time we did rfbPE() */

	g = g_in = got_pointer_input;

	while (1) {
		int got_input = 0;

		gcnt++;

		if (button_mask) {
			drag_in_progress = 1;
		}

		rfbCFD(rfb_wait_ms * 1000);

		dtm = dtime(&tm);
		spin += dtm;

		if (got_pointer_input == g) {
			if (last_was_miss) {
				consecutive_misses++;
			}
			last_was_miss = 1;
		} else {
			ginput++;
			if (ginput % miss_tweak == 0) {
				allowed_misses++;
			}
			consecutive_misses = 1;
			last_was_miss = 0;
		}

		if (spin > spin_max) {
			/* get out if spin time over limit */
			break;

		} else if (got_pointer_input > g) {
			/* received some input, flush to display. */
			got_input = 1;
			g = got_pointer_input;
			X_LOCK;
			XFlush(dpy);
			X_UNLOCK;
		} else if (--allowed_misses <= 0) {
			/* too many misses */
			break;
		} else if (consecutive_misses >=3) {
			/* too many misses */
			break;
		} else {
			/* these are misses */
			int wms = 0;
			if (gcnt == 1 && button_mask) {
				/*
				 * missed our first input, wait
				 * for a defer time. (e.g. on
				 * slow link) hopefully client
				 * will batch them.
				 */
				wms = 50;
			} else if (button_mask) {
				wms = 10;
			} else {
			}
			if (wms) {
				usleep(wms * 1000);
			}
		}
	}

	if (ginput >= 2) {
		/* try for a couple more quick ones */
		for (i=0; i<2; i++) {
			rfbCFD(rfb_wait_ms * 1000);
		}
	}

	drag_in_progress = 0;
}

int fb_update_sent(int *count) {
	static int last_count = 0;
	int sent = 0, rc = 0;
	rfbClientIteratorPtr i;
	rfbClientPtr cl;

	if (nofb) {
		return 0;
	}

	i = rfbGetClientIterator(screen);
	while( (cl = rfbClientIteratorNext(i)) ) {
		sent += cl->framebufferUpdateMessagesSent;
	}
	rfbReleaseClientIterator(i);
	if (sent != last_count) {
		rc = 1;
	}
	if (count != NULL) {
		*count = sent;
	}
	last_count = sent;
	return rc; 
}

static void check_user_input4(double dt, double dtr, int tile_diffs) {

	int g, g_in, i, ginput, gcnt, tmp;
	int last_was_miss, consecutive_misses;
	int min_frame_size = 10;	/* 10 tiles */
	double spin, tm, to, tc, dtm, rpe_last;
	int rfb_wait_ms = 2;
	static double dt_cut = 0.050;
	static int first = 1;

	int Btile = tile_x * tile_y * bpp/8; 	/* Bytes per tile */
	double Ttile, dt_use;
	double screen_rate = 6000000.;    /* 5 MB/sec */
	double vnccpu_rate = 80 * 100000.; /* 20 KB/sec @ 80X compression */
	double net_rate = 50000.;
	static double Tfac_r = 1.0, Tfac_v = 1.0, Tfac_n = 1.0, Tdelay = 0.001;
	static double dt_min = -1.0, dt_max = -1.0;
	double dt_min_fallback = 0.050;
	static int ssec = 0, total_calls = 0;
	static int push_frame = 0, update_count = 0;

	if (first) {
		char *p = getenv("SPIN");
		if (p) {
			sscanf(p, "%lf,%lf,%lf,%lf", &dt_cut, &Tfac_r, &Tfac_v, &Tfac_n);
		}
		first = 0;
		ssec = time(0);
	}

	total_calls++;

	if (dt_min < 0.0 || dt < dt_min) {
		if (dt > 0.0) {
			dt_min = dt;
		}
	}
	if (dt_min < 0.0) {
		/* sensible value for the very 1st call if dt = 0.0 */
		dt_min = dt_min_fallback;
	}
	if (dt_max < 0.0 || dt > dt_max) {
		dt_max = dt;
	}

	if (total_calls > 30 && dt_min > 0.0) {
		static int first = 1;
		/*
		 * dt_min will soon be the quickest time to do
		 * one scan_for_updates with no tiles copied.
		 * use this (instead of copy_tiles) to estimate
		 * screen read rate.
		 */
		screen_rate = (main_bytes_per_line * ntiles_y) / dt_min;
		if (first) {
			rfbLog("measured screen read rate: %.2f Bytes/sec\n",
			    screen_rate);
		}
		first = 0;
	}

	dtime0(&tm);

	if (dt < dt_cut) {
		dt_use = dt_cut;
	} else {
		dt_use = dt;
	}

	if (push_frame) {
		int cnt, iter = 0;
		double tp, push_spin = 0.0;
		dtime0(&tp);
		while (push_spin < dt_use * 0.5) {
			fb_update_sent(&cnt);
			if (cnt != update_count) {
				break;
			}
			/* damn, they didn't push our frame! */
			iter++;
			rfbPE(rfb_wait_ms * 1000);
			
			push_spin += dtime(&tp);
		}
		if (iter) {
			X_LOCK;
			XFlush(dpy);
			X_UNLOCK;
		}
		push_frame = 0;
		update_count = 0;
	}

	/*
	 * when we first enter we require some pointer input
	 */
	if (!got_pointer_input) {
		return;
	}

	vnccpu_rate = get_raw_rate();

	if ((tmp = get_read_rate()) != 0) {
		screen_rate = (double) tmp;
	}
	if ((tmp = get_net_rate()) != 0) {
		net_rate = (double) tmp;
	}
	net_rate = (vnccpu_rate/get_cmp_rate()) * net_rate;

	if ((tmp = get_net_latency()) != 0) {
		Tdelay = 0.5 * ((double) tmp)/1000.;
	}

	Ttile = Btile * (Tfac_r/screen_rate + Tfac_v/vnccpu_rate + Tfac_n/net_rate);

	spin = 0.0;		/* amount of time spinning */
	last_was_miss = 0;
	consecutive_misses = 1;
	gcnt = 0;
	ginput = 0;

	rpe_last = to = tc = tm;	/* last time we did rfbPE() */
	g = g_in = got_pointer_input;

	tile_diffs = 0;	/* reset our knowlegde of tile_diffs to zero */

	while (1) {
		int got_input = 0;

		gcnt++;

		if (button_mask) {
			/* this varible is used by our pointer handler */
			drag_in_progress = 1;
		}

		/* turn libvncserver crank to process events: */
		rfbCFD(rfb_wait_ms * 1000);

		dtm = dtime(&tm);
		spin += dtm;

		if ( (gcnt == 1 && got_pointer_input > g) || tm-tc > 2*dt_min) {
			tile_diffs = scan_for_updates(1);
			tc = tm;
		}

		if (got_pointer_input == g) {
			if (last_was_miss) {
				consecutive_misses++;
			}
			last_was_miss = 1;
		} else {
			ginput++;
			consecutive_misses = 1;
			last_was_miss = 0;
		}

		if (tile_diffs > min_frame_size && spin > Ttile * tile_diffs + Tdelay) {
			/* we think we can push the frame */
			push_frame = 1;
			fb_update_sent(&update_count);
			break;

		} else if (got_pointer_input > g) {
			/* received some input, flush it to display. */
			got_input = 1;
			g = got_pointer_input;
			X_LOCK;
			XFlush(dpy);
			X_UNLOCK;

		} else if (consecutive_misses >= 2) {
			/* too many misses in a row */
			break;

		} else {
			/* these are pointer input misses */
			int wms;
			if (gcnt == 1 && button_mask) {
				/*
				 * missed our first input, wait for
				 * a defer time. (e.g. on slow link)
				 * hopefully client will batch many
				 * of them for the next read.
				 */
				wms = 50;

			} else if (button_mask) {
				wms = 10;
			} else {
				wms = 0;
			}
			if (wms) {
				usleep(wms * 1000);
			}
		}
	}
	if (ginput >= 2) {
		/* try for a couple more quick ones */
		for (i=0; i<2; i++) {
			rfbCFD(rfb_wait_ms * 1000);
		}
	}
	drag_in_progress = 0;
}

static int check_user_input(double dt, double dtr, int tile_diffs, int *cnt) {

	if (raw_fb && ! dpy) return 0;	/* raw_fb hack */

	if (use_xrecord) {
		int rc = check_xrecord();
		/*
		 * 0: nothing found, proceed to other user input schemes.
		 * 1: events found, want to do a screen update now.
		 * 2: events found, want to loop back for some more.
		 * 3: events found, want to loop back for some more,
		 *    and not have rfbPE() called.
		 *
		 * For 0, we precede below, otherwise return rc-1.
		 */
if (debug_scroll && rc > 1) fprintf(stderr, "  CXR: check_user_input ret %d\n", rc - 1);
		if (rc == 0) {
			;	/* proceed below. */
		} else {
			return rc - 1;
		}
	}

	if (wireframe) {
		if (check_wireframe()) {
			return 0;
		}
	}

	if (pointer_mode == 1) {
		if ((got_user_input || ui_skip < 0) && *cnt % ui_skip != 0) {
			/* every ui_skip-th drops thru to scan */
			*cnt++;
			X_LOCK;
			XFlush(dpy);
			X_UNLOCK;
			return 1;	/* short circuit watch_loop */
		} else {
			return 0;
		}
	}
	if (pointer_mode >= 2 && pointer_mode <= 4) {
		if (got_keyboard_input) {
			/*
			 * for these modes, short circuit watch_loop on
			 * *keyboard* input.
			 */
			if (*cnt % ui_skip != 0) {
				*cnt++;
				return 1;
			}
		}
		/* otherwise continue below with pointer input method */
	}

	if (pointer_mode == 2) {
		check_user_input2(dt);
	} else if (pointer_mode == 3) {
		check_user_input3(dt, dtr, tile_diffs);
	} else if (pointer_mode == 4) {
		check_user_input4(dt, dtr, tile_diffs);
	}
	return 0;
}

/* -- x11vnc.c -- */
/*
 * main routine for the x11vnc program
 */

/*
 * simple function for measuring sub-second time differences, using
 * a double to hold the value.
 */
double dtime(double *t_old) {
	/* 
	 * usage: call with 0.0 to initialize, subsequent calls give
	 * the time difference since last call.
	 */
	double t_now, dt;
	struct timeval now;

	gettimeofday(&now, NULL);
	t_now = now.tv_sec + ( (double) now.tv_usec/1000000. );
	if (*t_old == 0.0) {
		*t_old = t_now;
		return t_now;
	}
	dt = t_now - *t_old;
	*t_old = t_now;
	return(dt);
}

/* common dtime() activities: */
double dtime0(double *t_old) {
	*t_old = 0.0;
	return dtime(t_old);
}

double dnow(void) {
	double t;
	return dtime0(&t);
}

void measure_display_hook(rfbClientPtr cl) {
	ClientData *cd = (ClientData *) cl->clientData;
	dtime0(&cd->timer);
}

int get_rate(int which) {
	rfbClientIteratorPtr iter;
	rfbClientPtr cl;
	int irate, irate_min = 1;	/* 1 KB/sec */
	int irate_max = 100000;		/* 100 MB/sec */
	int count = 0;
	double slowest = -1.0, rate;
	static double save_rate = 1000 * NETRATE0;
	
	iter = rfbGetClientIterator(screen);
	while( (cl = rfbClientIteratorNext(iter)) ) {
		ClientData *cd = (ClientData *) cl->clientData;

		if (cl->state != RFB_NORMAL) {
			continue;
		}
		if (cd->send_cmp_rate == 0.0 || cd->send_raw_rate == 0.0) {
			continue;
		}
		count++;
		
		if (which == 0) {
			rate = cd->send_cmp_rate;
		} else {
			rate = cd->send_raw_rate;
		}
		if (slowest == -1.0 || rate < slowest) {
			slowest = rate;
		}
		
	}
	rfbReleaseClientIterator(iter);

	if (! count) {
		return NETRATE0;
	}

	if (slowest == -1.0) {
		slowest = save_rate;
	} else {
		save_rate = slowest;
	}

	irate = (int) (slowest/1000.0);
	if (irate < irate_min) {
		irate = irate_min;
	}
	if (irate > irate_max) {
		irate = irate_max;
	}
if (0) fprintf(stderr, "get_rate(%d) %d %.3f/%.3f\n", which, irate, save_rate, slowest); 

	return irate;
}

int get_latency(void) {
	rfbClientIteratorPtr iter;
	rfbClientPtr cl;
	int ilat, ilat_min = 1;	/* 1 ms */
	int ilat_max = 2000;	/* 2 sec */
	double slowest = -1.0, lat;
	static double save_lat = ((double) LATENCY0)/1000.0;
	int count = 0;
	
	iter = rfbGetClientIterator(screen);
	while( (cl = rfbClientIteratorNext(iter)) ) {
		ClientData *cd = (ClientData *) cl->clientData;
		
		if (cl->state != RFB_NORMAL) {
			continue;
		}
		if (cd->latency == 0.0) {
			continue;
		}
		count++;

		lat = cd->latency;
		if (slowest == -1.0 || lat > slowest) {
			slowest = lat;
		}
	}
	rfbReleaseClientIterator(iter);

	if (! count) {
		return LATENCY0;
	}

	if (slowest == -1.0) {
		slowest = save_lat;
	} else {
		save_lat = slowest;
	}

	ilat = (int) (slowest * 1000.0);
	if (ilat < ilat_min) {
		ilat = ilat_min;
	}
	if (ilat > ilat_max) {
		ilat = ilat_max;
	}

	return ilat;
}

int get_cmp_rate(void) {
	return get_rate(0);
}

int get_raw_rate(void) {
	return get_rate(1);
}

void initialize_speeds(void) {
	char *s, *s_in, *p;
	int i;

	speeds_read_rate = 0;
	speeds_net_rate = 0;
	speeds_net_latency = 0;
	if (! speeds_str || *speeds_str == '\0') {
		s_in = strdup("");
	} else {
		s_in = strdup(speeds_str);
	}

	if (!strcmp(s_in, "modem")) {
		s = strdup("6,4,200");
	} else if (!strcmp(s_in, "dsl")) {
		s = strdup("6,100,50");
	} else if (!strcmp(s_in, "lan")) {
		s = strdup("6,5000,1");
	} else {
		s = strdup(s_in);
	}

	p = strtok(s, ",");
	i = 0;
	while (p) {
		double val;
		if (*p != '\0') {
			val = atof(p);
			if (i==0) {
				speeds_read_rate = (int) 1000000 * val;
			} else if (i==1) {
				speeds_net_rate = (int) 1000 * val;
			} else if (i==2) {
				speeds_net_latency = (int) val;
			}
		}
		i++;
		p = strtok(NULL, ",");
	}
	free(s);
	free(s_in);

	if (! speeds_read_rate) {
		int n = 0;
		double dt, timer;
		dtime0(&timer);
		if (fullscreen) {
			copy_image(fullscreen, 0, 0, 0, 0);
			n = fullscreen->bytes_per_line * fullscreen->height;
		} else if (scanline) {
			copy_image(scanline, 0, 0, 0, 0);
			n = scanline->bytes_per_line * scanline->height;
		}
		dt = dtime(&timer);
		if (n && dt > 0.0) {
			double rate = ((double) n) / dt;
			speeds_read_rate_measured = (int) (rate/1000000.0);
			if (speeds_read_rate_measured < 1) {
				speeds_read_rate_measured = 1;
			} else {
				rfbLog("fb read rate: %d MB/sec\n",
				    speeds_read_rate_measured);
			}
		}
	}
}

int get_read_rate(void) {
	if (speeds_read_rate) {
		return speeds_read_rate;
	}
	if (speeds_read_rate_measured) {
		return speeds_read_rate_measured;
	}
	return 0;
}

int link_rate(int *latency, int *netrate) {
	*latency = get_net_latency();
	*netrate = get_net_rate();

	if (speeds_str) {
		if (!strcmp(speeds_str, "modem")) {
			return LR_DIALUP;
		} else if (!strcmp(speeds_str, "dsl")) {
			return LR_BROADBAND;
		} else if (!strcmp(speeds_str, "lan")) {
			return LR_LAN;
		}
	}

	if (*latency == LATENCY0 && *netrate == NETRATE0)  {
		return LR_UNSET;
	} else if (*latency > 150 || *netrate < 20) {
		return LR_DIALUP;
	} else if (*latency > 50 || *netrate < 150) {
		return LR_BROADBAND;
	} else if (*latency < 10 && *netrate > 300) {
		return LR_LAN;
	} else {
		return LR_UNKNOWN;
	}
}

int get_net_rate(void) {
	int spm = speeds_net_rate_measured;
	if (speeds_net_rate) {
		return speeds_net_rate;
	}
	if (! spm || spm == NETRATE0) {
		speeds_net_rate_measured = get_cmp_rate();
	}
	if (speeds_net_rate_measured) {
		return speeds_net_rate_measured;
	}
	return 0;
}

int get_net_latency(void) {
	int spm = speeds_net_latency_measured;
	if (speeds_net_latency) {
		return speeds_net_latency;
	}
	if (! spm || spm == LATENCY0) {
		speeds_net_latency_measured = get_latency();
	}
	if (speeds_net_latency_measured) {
		return speeds_net_latency_measured;
	}
	return 0;
}

void measure_send_rates(int init) {
	double cmp_rate, raw_rate;
	static double now, start = 0.0;
	static rfbDisplayHookPtr orig_display_hook = NULL;
	double cmp_max = 1.0e+08;	/* 100 MB/sec */
	double cmp_min = 1000.0;	/* 9600baud */
	double lat_max = 5.0;		/* 5 sec */
	double lat_min = .0005;		/* 0.5 ms */
	int min_cmp = 10000, nclients;
	rfbClientIteratorPtr iter;
	rfbClientPtr cl;
	int db = 0, msg = 0;

	if (! measure_speeds) {
		return;
	}
	if (speeds_net_rate && speeds_net_latency) {
		return;
	}

	if (! orig_display_hook) {
		orig_display_hook = screen->displayHook;
	}

	if (start == 0.0) {
		dtime(&start);
	}
	dtime0(&now);
	now = now - start;

	nclients = 0;

	iter = rfbGetClientIterator(screen);
	while( (cl = rfbClientIteratorNext(iter)) ) {
		int defer, i, cbs, rbs;
		char *httpdir;
		double dt, dt1 = 0.0, dt2, dt3;
		double tm, spin_max = 15.0, spin_lat_max = 1.5;
		int got_t2 = 0, got_t3 = 0;
		ClientData *cd = (ClientData *) cl->clientData;

		if (cd->send_cmp_rate > 0.0) {
			continue;
		}
		nclients++;

		cbs = 0;
		for (i=0; i<MAX_ENCODINGS; i++) {
			cbs += cl->bytesSent[i];
		}
		rbs = cl->rawBytesEquivalent;

		if (init) {

if (db) fprintf(stderr, "%d client num rects req: %d  mod: %d  cbs: %d  "
    "rbs: %d  dt1: %.3f  t: %.3f\n", init,
    (int) sraRgnCountRects(cl->requestedRegion),
    (int) sraRgnCountRects(cl->modifiedRegion), cbs, rbs, dt1, now);

			cd->timer = dnow();
			cd->cmp_bytes_sent = cbs;
			cd->raw_bytes_sent = rbs;
			continue;
		}

		/* first part of the bulk transfer of initial screen */
		dt1 = dtime(&cd->timer);

if (db) fprintf(stderr, "%d client num rects req: %d  mod: %d  cbs: %d  "
    "rbs: %d  dt1: %.3f  t: %.3f\n", init,
    (int) sraRgnCountRects(cl->requestedRegion),
    (int) sraRgnCountRects(cl->modifiedRegion), cbs, rbs, dt1, now);

		if (dt1 <= 0.0) {
			continue;
		}

		cbs = cbs - cd->cmp_bytes_sent;
		rbs = rbs - cd->raw_bytes_sent;

		if (cbs < min_cmp) {
			continue;
		}

		rfbPE(1000);
		if (sraRgnCountRects(cl->modifiedRegion)) {
			rfbPE(1000);
		}

		defer = screen->deferUpdateTime;
		httpdir = screen->httpDir;
		screen->deferUpdateTime = 0;
		screen->httpDir = NULL;

		/* mark a small rectangle: */
		mark_rect_as_modified(0, 0, 16, 16, 1);

		dtime0(&tm);

		dt2 = 0.0;
		dt3 = 0.0;

		if (dt1 < 0.25) {
			/* try to cut it down to avoid long pauses. */
			spin_max = 5.0;
		}

		/* when req1 = 1 mod1 == 0, end of 2nd part of bulk transfer */
		while (1) {
			int req0, req1, mod0, mod1;
			req0 = sraRgnCountRects(cl->requestedRegion);
			mod0 = sraRgnCountRects(cl->modifiedRegion);
			if (use_threads) {
				usleep(1000);
			} else {
				if (mod0) {
					rfbPE(1000);
				} else {
					rfbCFD(1000);
				}
			}
			dt = dtime(&tm);
			dt2 += dt;
			if (dt2 > spin_max) {
				break;
			}
			req1 = sraRgnCountRects(cl->requestedRegion);
			mod1 = sraRgnCountRects(cl->modifiedRegion);

if (db) fprintf(stderr, "dt2 calc: num rects req: %d/%d mod: %d/%d  "
    "fbu-sent: %d  dt: %.4f dt2: %.4f  tm: %.4f\n",
    req0, req1, mod0, mod1, cl->framebufferUpdateMessagesSent, dt, dt2, tm);
			if (req1 != 0 && mod1 == 0) {
				got_t2 = 1;
				break;
			}
		}

		if (! got_t2) {
			dt2 = 0.0;
		} else {
			int tr, trm = 3;
			double dts[10];
			
			/*
			 * Note: since often select(2) cannot sleep
			 * less than 1/HZ (e.g. 10ms), the resolution
			 * of the latency may be messed up by something
			 * of this order.  Effect may occur on both ends,
			 * i.e. the viewer may not respond immediately.
			 */
		
			for (tr = 0; tr < trm; tr++) {
				usleep(5000);

				/* mark a 2nd small rectangle: */
				mark_rect_as_modified(0, 0, 16, 16, 1);
				i = 0;
				dtime0(&tm);
				dt3 = 0.0;

				/*
				 * when req1 > 0 and mod1 == 0, we say
				 * that is the "ping" time.
				 */
				while (1) {
					int req0, req1, mod0, mod1;

					req0 = sraRgnCountRects(
					    cl->requestedRegion);
					mod0 = sraRgnCountRects(
					    cl->modifiedRegion);

					if (i == 0) {
						rfbPE(0);
					} else {
						if (use_threads) {
							usleep(1000);
						} else {
							/* try to get it all */
							rfbCFD(1000*1000);
						}
					}
					dt = dtime(&tm);
					i++;

					dt3 += dt;
					if (dt3 > spin_lat_max) {
						break;
					}
					req1 = sraRgnCountRects(
					    cl->requestedRegion);

					mod1 = sraRgnCountRects(
					    cl->modifiedRegion);

if (db) fprintf(stderr, "dt3 calc: num rects req: %d/%d mod: %d/%d  "
    "fbu-sent: %d  dt: %.4f dt3: %.4f  tm: %.4f\n",
    req0, req1, mod0, mod1, cl->framebufferUpdateMessagesSent, dt, dt3, tm);

					if (req1 != 0 && mod1 == 0) {
						dts[got_t3++] = dt3;
						break;
					}
				}
			}
		
			if (! got_t3) {
				dt3 = 0.0;
			} else {
				if (got_t3 == 1) {
					dt3 = dts[0];
				} else if (got_t3 == 2) {
					dt3 = dts[1];
				} else {
					if (dts[2] >= 0.0) {
						double rat = dts[1]/dts[2];
						if (rat > 0.5 && rat < 2.0) {
							dt3 = dts[1]+dts[2];
							dt3 *= 0.5;
						} else {
							dt3 = dts[1];
						}
					} else {
						dt3 = dts[1];
					}
				}
			}
		}
		
		screen->deferUpdateTime = defer;
		screen->httpDir = httpdir;

		dt = dt1 + dt2;


		if (dt3 <= dt2/2.0) {
			/* guess only 1/2 a ping for reply... */
			dt = dt - dt3/2.0;
		}

		cmp_rate = cbs/dt;
		raw_rate = rbs/dt;

		if (cmp_rate > cmp_max) {
			cmp_rate = cmp_max;
		}
		if (cmp_rate <= cmp_min) {
			cmp_rate = cmp_min;
		}

		cd->send_cmp_rate = cmp_rate;
		cd->send_raw_rate = raw_rate;

		if (dt3 > lat_max) {
			dt3 = lat_max;
		}
		if (dt3 <= lat_min) {
			dt3 = lat_min;
		}

		cd->latency = dt3;
		
		rfbLog("client %d network rate %.1f KB/sec (%.1f eff KB/sec)\n",
		    cd->uid, cmp_rate/1000.0, raw_rate/1000.0);
		rfbLog("client %d latency:  %.1f ms\n", cd->uid, 1000.0*dt3);
		rfbLog("dt1: %.4f, dt2: %.4f dt3: %.4f bytes: %d\n",
		    dt1, dt2, dt3, cbs);
		msg = 1;
	}
	rfbReleaseClientIterator(iter);

	if (msg) {
		int link, latency, netrate;
		char *str = "error";

		link = link_rate(&latency, &netrate);
		if (link == LR_UNSET) {
			str = "LR_UNSET";
		} else if (link == LR_UNKNOWN) {
			str = "LR_UNKNOWN";
		} else if (link == LR_DIALUP) {
			str = "LR_DIALUP";
		} else if (link == LR_BROADBAND) {
			str = "LR_BROADBAND";
		} else if (link == LR_LAN) {
			str = "LR_LAN";
		}
		rfbLog("link_rate: %s - %d ms, %d KB/s\n", str, latency,
		    netrate);
	}

	if (init) {
		if (nclients) {
			screen->displayHook = measure_display_hook;
		}
	} else {
		screen->displayHook = orig_display_hook;
	}
}

void check_cursor_changes(void) {
	static double last_push = 0.0;

	cursor_changes += check_x11_pointer();

	if (cursor_changes) {
		double tm, max_push = 0.125, multi_push = 0.01, wait = 0.02;
		int cursor_shape, dopush = 0, link, latency, netrate;
		cursor_shape = cursor_shape_updates_clients(screen);
	
		dtime0(&tm);
		link = link_rate(&latency, &netrate);
		if (link == LR_DIALUP) {
			max_push = 0.2;
			wait = 0.05;
		} else if (link == LR_BROADBAND) {
			max_push = 0.075;
			wait = 0.05;
		} else if (link == LR_LAN) {
			max_push = 0.01;
		} else if (latency < 5 && netrate > 200) {
			max_push = 0.01;
		}
		
		if (tm > last_push + max_push) {
			dopush = 1;
		} else if (cursor_changes > 1 && tm > last_push + multi_push) {
			dopush = 1;
		}

		if (dopush) { 
			mark_rect_as_modified(0, 0, 1, 1, 1);
			fb_push_wait(wait, FB_MOD);
			last_push = tm;
		} else {
			rfbPE(0);
		}
	}
	cursor_changes = 0;
}

/*
 * utility wrapper to call rfbProcessEvents
 * checks that we are not in threaded mode.
 */
#define USEC_MAX 999999		/* libvncsever assumes < 1 second */
void rfbPE(long usec) {
	if (! screen) {
		return;
	}

	if (usec > USEC_MAX) {
		usec = USEC_MAX;
	}
	if (! use_threads) {
		rfbProcessEvents(screen, usec);
	}
}

void rfbCFD(long usec) {
	if (! screen) {
		return;
	}
	if (usec > USEC_MAX) {
		usec = USEC_MAX;
	}
	if (! use_threads) {
		rfbCheckFds(screen, usec);
	}
}

int choose_delay(double dt) {
	static double t0 = 0.0, t1 = 0.0, t2 = 0.0, now; 
	static int x0, y0, x1, y1, x2, y2, first = 1;
	int dx0, dy0, dx1, dy1, dm, i, msec = waitms;
	double cut1 = 0.15, cut2 = 0.075, cut3 = 0.25;
	double bogdown_time = 0.25, bave = 0.0;
	int bogdown = 1, bcnt = 0;
	int ndt = 8, nave = 3;
	double fac = 1.0;
	int db = 0;
	static double dts[8];

	if (waitms == 0) {
		return waitms;
	}
	if (nofb) {
		return waitms;
	}

	if (first) {
		for(i=0; i<ndt; i++) {
			dts[i] = 0.0;
		}
		first = 0;
	}

	now = dnow();

	/*
	 * first check for bogdown, e.g. lots of activity, scrolling text
	 * from command output, etc.
	 */
	if (nap_ok) {
		dt = 0.0;
	}
	if (! wait_bog) {
		bogdown = 0;

	} else if (button_mask || now < last_keyboard_time + 2*bogdown_time) {
		/*
		 * let scrolls & keyboard input through the normal way
		 * otherwise, it will likely just annoy them.
		 */
		bogdown = 0;

	} else if (dt > 0.0) {
		/*
		 * inspect recent dt's:
		 * 0 1 2 3 4 5 6 7 dt
		 *             ^ ^ ^
		 */
		for (i = ndt - (nave - 1); i < ndt; i++) {
			bave += dts[i];
			bcnt++;
			if (dts[i] < bogdown_time) {
				bogdown = 0;
				break;
			}
		}
		bave += dt;
		bcnt++;
		bave = bave / bcnt;
		if (dt < bogdown_time) {
			bogdown = 0;
		}
	} else {
		bogdown = 0;
	}
	/* shift for next time */
	for (i = 0; i < ndt-1; i++) {
		dts[i] = dts[i+1];
	}
	dts[ndt-1] = dt;

if (0 && dt > 0.0) fprintf(stderr, "dt: %.5f %.4f\n", dt, dnow() - x11vnc_start);
	if (bogdown) {
		if (use_xdamage) {
			/* DAMAGE can queue ~1000 rectangles for a scroll */
			clear_xdamage_mark_region(NULL, 0);
		}
		msec = (int) (1000 * 1.75 * bave);
		if (dts[ndt - nave - 1] > 0.75 * bave) {
			msec = 1.5 * msec;
			set_xdamage_mark(0, 0, dpy_x, dpy_y);
		}
		if (msec > 1500) {
			msec = 1500;
		}
		if (msec < waitms) {
			msec = waitms;
		}
		db = (db || debug_tiles);
		if (db) fprintf(stderr, "bogg[%d] %.3f %.3f %.3f %.3f\n",
		    msec, dts[ndt-4], dts[ndt-3], dts[ndt-2], dts[ndt-1]);
		return msec;
	}

	/* next check for pointer motion, keystrokes, to speed up */
	t2 = dnow();
	x2 = cursor_x;
	y2 = cursor_y;

	dx0 = nabs(x1 - x0);
	dy0 = nabs(y1 - y0);
	dx1 = nabs(x2 - x1);
	dy1 = nabs(y2 - y1);
	if (dx1 > dy1) {
		dm = dx1;
	} else {
		dm = dy1;
	}

	if ((dx0 || dy0) && (dx1 || dy1)) {
		if (t2 < t0 + cut1 || t2 < t1 + cut2 || dm > 20) {
			fac = wait_ui * 1.25;
		}
	} else if ((dx1 || dy1) && dm > 40) {
		fac = wait_ui;
	}

	if (fac == 1 && t2 < last_keyboard_time + cut3) {
		fac = wait_ui;
	}
	msec = (int) ((double) waitms / fac);
	if (msec == 0) {
		msec = 1;
	}

	x0 = x1;
	y0 = y1;
	t0 = t1;

	x1 = x2;
	y1 = y2;
	t1 = t2;

	return msec;
}

/*
 * main x11vnc loop: polls, checks for events, iterate libvncserver, etc.
 */
static void watch_loop(void) {
	int cnt = 0, tile_diffs = 0, skip_pe = 0;
	double tm, dtr, dt = 0.0;
	time_t start = time(0);

	if (use_threads) {
		rfbRunEventLoop(screen, -1, TRUE);
	}

	while (1) {

		got_user_input = 0;
		got_pointer_input = 0;
		got_keyboard_input = 0;
		urgent_update = 0;

		if (! use_threads) {
			dtime0(&tm);
			if (! skip_pe) {
				measure_send_rates(1);
				rfbPE(-1);
				measure_send_rates(0);
				fb_update_sent(NULL);
			}
			dtr = dtime(&tm);

			if (! cursor_shape_updates) {
				/* undo any cursor shape requests */
				disable_cursor_shape_updates(screen);
			}
			if (screen && screen->clientHead) {
				int ret = check_user_input(dt, dtr,
				    tile_diffs, &cnt);
				/* true means loop back for more input */
				if (ret == 2) {
					skip_pe = 1;
				}
				if (ret) {
if (debug_scroll) fprintf(stderr, "watch_loop: LOOP-BACK: %d\n", ret);
					continue;
				}
			}
		} else {
			if (0 && use_xrecord) {
				/* XXX not working */
				check_xrecord();
			}
			if (wireframe && button_mask) {
				check_wireframe();
			}
		}
		skip_pe = 0;

		if (shut_down) {
			clean_up_exit(0);
		}

		if (! urgent_update) {
			if (do_copy_screen) {
				do_copy_screen = 0;
				copy_screen();
			}

			check_new_clients();
			check_xevents();
			check_autorepeat();
			check_connect_inputs();		
			check_padded_fb();		
			check_xdamage_state();
			check_xrecord_reset();
			check_add_keysyms();
			if (started_as_root) {
				check_switched_user();
			}

			if (first_conn_timeout < 0) {
				start = time(0);
				first_conn_timeout = -first_conn_timeout;
			}
		}

		if (! screen || ! screen->clientHead) {
			/* waiting for a client */
			if (first_conn_timeout) {
				if (time(0) - start > first_conn_timeout) {
					rfbLog("No client after %d secs.\n",
					    first_conn_timeout);
					shut_down = 1;
				}
			}
			usleep(200 * 1000);
			continue;
		}

		if (first_conn_timeout && all_clients_initialized()) {
			first_conn_timeout = 0;
		}

		if (nofb) {
			/* no framebuffer polling needed */
			if (cursor_pos_updates) {
				check_x11_pointer();
			}
			continue;
		}

		if (button_mask && (!show_dragging || pointer_mode == 0)) {
			/*
			 * if any button is pressed do not update rfb
			 * screen, but do flush the X11 display.
			 */
			X_LOCK;
			XFlush(dpy);
			X_UNLOCK;
			dt = 0.0;
		} else {
			static double last_dt = 0.0;
			double xdamage_thrash = 0.4; 

			check_cursor_changes();

			/* for timing the scan to try to detect thrashing */

			if (use_xdamage && last_dt > xdamage_thrash)  {
				clear_xdamage_mark_region(NULL, 0);
			}
			dtime0(&tm);
			if (use_snapfb) {
				int t, tries = 5;
				copy_snap();
				for (t =0; t < tries; t++) {
					tile_diffs = scan_for_updates(0);
				}
			} else {
				tile_diffs = scan_for_updates(0);
			}
			dt = dtime(&tm);
			if (! nap_ok) {
				last_dt = dt;
			}

if ((debug_tiles || debug_scroll > 1 || debug_wireframe > 1)
    && (tile_diffs > 4 || debug_tiles > 1)) {
	double rate = (tile_x * tile_y * bpp/8 * tile_diffs) / dt;
	fprintf(stderr, "============================= TILES: %d  dt: %.4f"
	    "  t: %.4f  %.2f MB/s\n", tile_diffs, dt, tm - x11vnc_start,
	    rate/1000000.0);
}

		}

		/* sleep a bit to lessen load */
		if (! urgent_update) {
			int wait = choose_delay(dt);
			if (wait > 2*waitms) {
				/* bog case, break it up */
				nap_sleep(wait, 10);
			} else {
				usleep(wait * 1000);
			}
		}
		cnt++;
	}
}

/*
 * text printed out under -help option
 */
static void print_help(int mode) {
#if !SMALL_FOOTPRINT
	char help[] = 
"\n"
"x11vnc: allow VNC connections to real X11 displays. %s\n"
"\n"
"Typical usage is:\n"
"\n"
"   Run this command in a shell on the remote machine \"far-host\"\n"
"   with X session you wish to view:\n"
"\n"
"       x11vnc -display :0\n"
"\n"
"   Then run this in another window on the machine you are sitting at:\n"
"\n"
"       vncviewer far-host:0\n"
"\n"
"Once x11vnc establishes connections with the X11 server and starts listening\n"
"as a VNC server it will print out a string: PORT=XXXX where XXXX is typically\n"
"5900 (the default VNC server port).  One would next run something like\n"
"this on the local machine: \"vncviewer hostname:N\" where \"hostname\" is\n"
"the name of the machine running x11vnc and N is XXXX - 5900, i.e. usually\n"
"\"vncviewer hostname:0\".\n"
"\n"
"By default x11vnc will not allow the screen to be shared and it will exit\n"
"as soon as the client disconnects.  See -shared and -forever below to override\n"
"these protections.  See the FAQ for details how to tunnel the VNC connection\n"
"through an encrypted channel such as ssh(1).  In brief:\n"
"\n"
"       ssh -L 5900:localhost:5900 far-host 'x11vnc -localhost -display :0'\n"
"\n"
"       vncviewer -encodings 'copyrect tight zrle hextile' localhost:0\n"
"\n"
"For additional info see: http://www.karlrunge.com/x11vnc/\n"
"                    and  http://www.karlrunge.com/x11vnc/#faq\n"
"\n"
"\n"
"Rudimentary config file support: if the file $HOME/.x11vncrc exists then each\n"
"line in it is treated as a single command line option.  Disable with -norc.\n"
"For each option name, the leading character \"-\" is not required.  E.g. a\n"
"line that is either \"forever\" or \"-forever\" may be used and are equivalent.\n"
"Likewise \"wait 100\" or \"-wait 100\" are acceptable and equivalent lines.\n"
"The \"#\" character comments out to the end of the line in the usual way.\n"
"Leading and trailing whitespace is trimmed off.  Lines may be continued with\n"
"a \"\\\" as the last character of a line (it becomes a space character).\n"
"\n"
"Options:\n"
"\n"
"-display disp          X11 server display to connect to, usually :0.  The X\n"
"                       server process must be running on same machine and\n"
"                       support MIT-SHM.  Equivalent to setting the DISPLAY\n"
"                       environment variable to \"disp\".\n"
"-auth file             Set the X authority file to be \"file\", equivalent to\n"
"                       setting the XAUTHORITY environment variable to \"file\"\n"
"                       before startup.  Same as -xauth file.  See Xsecurity(7),\n"
"                       xauth(1) man pages for more info.\n"
"\n"
"-id windowid           Show the window corresponding to \"windowid\" not\n"
"                       the entire display.  New windows like popup menus,\n"
"                       transient toplevels, etc, may not be seen or may be\n"
"                       clipped.  Disabling SaveUnders or BackingStore in the\n"
"                       X server may help show them.  x11vnc may crash if the\n"
"                       window is initially partially obscured, changes size,\n"
"                       is iconified, etc.  Some steps are taken to avoid this\n"
"                       and the -xrandr mechanism is used to track resizes.  Use\n"
"                       xwininfo(1) to get the window id, or use \"-id pick\"\n"
"                       to have x11vnc run xwininfo(1) for you and extract\n"
"                       the id.  The -id option is useful for exporting very\n"
"                       simple applications (e.g. the current view on a webcam).\n"
"-sid windowid          As -id, but instead of using the window directly it\n"
"                       shifts a root view to it: this shows SaveUnders menus,\n"
"                       etc, although they will be clipped if they extend beyond\n"
"                       the window.\n"
"-clip WxH+X+Y          Only show the sub-region of the full display that\n"
"                       corresponds to the rectangle with size WxH and offset\n"
"                       +X+Y.  The VNC display has size WxH (i.e. smaller than\n"
"                       the full display).  This also works for -id/-sid mode\n"
"                       where the offset is relative to the upper left corner\n"
"                       of the selected window.\n"
"\n"
"-flashcmap             In 8bpp indexed color, let the installed colormap flash\n"
"                       as the pointer moves from window to window (slow).\n"
"-shiftcmap n           Rare problem, but some 8bpp displays use less than 256\n"
"                       colorcells (e.g. 16-color grayscale, perhaps the other\n"
"                       bits are used for double buffering) *and* also need to\n"
"                       shift the pixels values away from 0, .., ncells.  \"n\"\n"
"                       indicates the shift to be applied to the pixel values.\n"
"                       To see the pixel values set DEBUG_CMAP=1 to print out\n"
"                       a colormap histogram.  Example: -shiftcmap 240\n"
"-notruecolor           For 8bpp displays, force indexed color (i.e. a colormap)\n"
"                       even if it looks like 8bpp TrueColor (rare problem).\n"
"-visual n              Experimental option: probably does not do what you\n"
"                       think.  It simply *forces* the visual used for the\n"
"                       framebuffer; this may be a bad thing... (e.g. messes\n"
"                       up colors or cause a crash). It is useful for testing\n"
"                       and for some workarounds.  n may be a decimal number,\n"
"                       or 0x hex.  Run xdpyinfo(1) for the values.  One may\n"
"                       also use \"TrueColor\", etc. see <X11/X.h> for a list.\n"
"                       If the string ends in \":m\" then for better or for\n"
"                       worse the visual depth is forced to be m.\n"
"\n"
"-overlay               Handle multiple depth visuals on one screen, e.g. 8+24\n"
"                       and 24+8 overlay visuals (the 32 bits per pixel are\n"
"                       packed with 8 for PseudoColor and 24 for TrueColor).\n"
"\n"
"                       Currently -overlay only works on Solaris via\n"
"                       XReadScreen(3X11) and IRIX using XReadDisplay(3).\n"
"                       On Solaris there is a problem with image \"bleeding\"\n"
"                       around transient popup menus (but not for the menu\n"
"                       itself): a workaround is to disable SaveUnders\n"
"                       by passing the \"-su\" argument to Xsun (in\n"
"                       /etc/dt/config/Xservers).  Also note that the mouse\n"
"                       cursor shape is exactly correct in this mode.\n"
"\n"
"                       Use -overlay as a workaround for situations like these:\n"
"                       Some legacy applications require the default visual to\n"
"                       be 8bpp (8+24), or they will use 8bpp PseudoColor even\n"
"                       when the default visual is depth 24 TrueColor (24+8).\n"
"                       In these cases colors in some windows will be messed\n"
"                       up in x11vnc unless -overlay is used.  Another use of\n"
"                       -overlay is to enable showing the exact mouse cursor\n"
"                       shape (details below).\n"
"\n"
"                       Under -overlay, performance will be somewhat degraded\n"
"                       due to the extra image transformations required.\n"
"                       For optimal performance do not use -overlay, but rather\n"
"                       configure the X server so that the default visual is\n"
"                       depth 24 TrueColor and try to have all apps use that\n"
"                       visual (e.g. some apps have -use24 or -visual options).\n"
"-overlay_nocursor      Sets -overlay, but does not try to draw the exact mouse\n"
"                       cursor shape using the overlay mechanism.\n"
"\n"
"-scale fraction        Scale the framebuffer by factor \"fraction\".  Values\n"
"                       less than 1 shrink the fb, larger ones expand it.  Note:\n"
"                       image may not be sharp and response may be slower.\n"
"                       If \"fraction\" contains a decimal point \".\" it\n"
"                       is taken as a floating point number, alternatively\n"
"                       the notation \"m/n\" may be used to denote fractions\n"
"                       exactly, e.g. -scale 2/3\n"
"\n"
"                       Scaling Options: can be added after \"fraction\"\n"
"                       via \":\", to supply multiple \":\" options use\n"
"                       commas.  If you just want a quick, rough scaling\n"
"                       without blending, append \":nb\" to \"fraction\"\n"
"                       (e.g. -scale 1/3:nb).  No blending is the default\n"
"                       for 8bpp indexed color, to force blending for this\n"
"                       case use \":fb\".  For compatibility with vncviewers\n"
"                       the scaled width is adjusted to be a multiple of 4:\n"
"                       to disable this use \":n4\".  More esoteric options:\n"
"                       \":in\" use interpolation scheme even when shrinking,\n"
"                       \":pad\", pad scaled width and height to be multiples\n"
"                       of scaling denominator (e.g. 3 for 2/3).\n"
"\n"
"-scale_cursor frac     By default if -scale is supplied the cursor shape is\n"
"                       scaled by the same factor.  Depending on your usage,\n"
"                       you may want to scale the cursor independently of the\n"
"                       screen or not at all.  If you specify -scale_cursor\n"
"                       the cursor will be scaled by that factor.  When using\n"
"                       -scale mode to keep the cursor at its \"natural\" size\n"
"                       use \"-scale_cursor 1\".  Most of the \":\" scaling\n"
"                       options apply here as well.\n"
"\n"
"-viewonly              All VNC clients can only watch (default %s).\n"
"-shared                VNC display is shared (default %s).\n"
"-once                  Exit after the first successfully connected viewer\n"
"                       disconnects, opposite of -forever. This is the Default.\n"
"-forever               Keep listening for more connections rather than exiting\n"
"                       as soon as the first client(s) disconnect. Same as -many\n"
"-timeout n             Exit unless a client connects within the first n seconds\n"
"                       of startup.\n"
"-inetd                 Launched by inetd(1): stdio instead of listening socket.\n"
"                       Note: if you are not redirecting stderr to a log file\n"
"                       (via shell 2> or -o option) you must also specify the\n"
"                       -q option, otherwise the stderr goes to the viewer.\n"
"-http                  Instead of using -httpdir (see below) to specify\n"
"                       where the Java vncviewer applet is, have x11vnc try\n"
"                       to *guess* where the directory is by looking relative\n"
"                       to the program location and in standard locations\n"
"                       (/usr/local/share/x11vnc/classes, etc).\n"
"-connect string        For use with \"vncviewer -listen\" reverse connections.\n"
"                       If \"string\" has the form \"host\" or \"host:port\"\n"
"                       the connection is made once at startup.  Use commas\n"
"                       for a list of host's and host:port's.\n"
"\n"
"                       If \"string\" contains \"/\" it is instead interpreted\n"
"                       as a file to periodically check for new hosts.\n"
"                       The first line is read and then the file is truncated.\n"
"                       Be careful for this usage mode if x11vnc is running as\n"
"                       root (e.g. via inetd(1) or gdm(1)).\n"
"-vncconnect            Monitor the VNC_CONNECT X property set by the standard\n"
"-novncconnect          VNC program vncconnect(1).  When the property is\n"
"                       set to \"host\" or \"host:port\" establish a reverse\n"
"                       connection.  Using xprop(1) instead of vncconnect may\n"
"                       work (see the FAQ).  Default: %s\n"
"\n"
"-allow host1[,host2..] Only allow client connections from hosts matching\n"
"                       the comma separated list of hostnames or IP addresses.\n"
"                       Can also be a numerical IP prefix, e.g. \"192.168.100.\"\n"
"                       to match a simple subnet, for more control build\n"
"                       libvncserver with libwrap support (See the FAQ).  If the\n"
"                       list contains a \"/\" it instead is a interpreted as a\n"
"                       file containing addresses or prefixes that is re-read\n"
"                       each time a new client connects.  Lines can be commented\n"
"                       out with the \"#\" character in the usual way.\n"
"-localhost             Same as \"-allow 127.0.0.1\".\n"
"\n"
"                       Note: if you want to restrict which network interface\n"
"                       x11vnc listens on, see the -listen option below.\n"
"                       E.g. \"-listen localhost\" or \"-listen 192.168.3.21\".\n"
"                       As a special case, the option \"-localhost\" implies\n"
"                       \"-listen localhost\".\n"
"\n"
"                       For non-localhost -listen usage, if you use the remote\n"
"                       control mechanism (-R) to change the -listen interface\n"
"                       you may need to manually adjust the -allow list (and\n"
"                       vice versa) to avoid situations where no connections\n"
"                       (or too many) are allowed.\n"
"\n"
"-nolookup              Do not use gethostbyname() or gethostbyaddr() to look up\n"
"                       host names or IP numbers.  Use this if name resolution\n"
"                       is incorrectly set up and leads to long pauses as name\n"
"                       lookup times out, etc.\n"
"\n"
"-input string          Fine tuning of allowed user input.  If \"string\" does\n"
"                       not contain a comma \",\" the tuning applies only to\n"
"                       normal clients.  Otherwise the part before \",\" is\n"
"                       for normal clients and the part after for view-only\n"
"                       clients.  \"K\" is for Keystroke input, \"M\" for\n"
"                       Mouse-motion input, and \"B\" for Button-click input.\n"
"                       Their presence in the string enables that type of input.\n"
"                       E.g. \"-input M\" means normal users can only move\n"
"                       the mouse and  \"-input KMB,M\" lets normal users do\n"
"                       anything and enables view-only users to move the mouse.\n"
"                       This option is ignored when a global -viewonly is in\n"
"                       effect (all input is discarded).\n"
"-viewpasswd string     Supply a 2nd password for view-only logins.  The -passwd\n"
"                       (full-access) password must also be supplied.\n"
"-passwdfile filename   Specify libvncserver -passwd via the first line of the\n"
"                       file \"filename\" instead of via command line (where\n"
"                       others might see it via ps(1)).  If a second non blank\n"
"                       line exists in the file it is taken as a view-only\n"
"                       password (i.e. -viewpasswd) To supply an empty password\n"
"                       for either field the string \"__EMPTY__\" may be used.\n"
"                       Note: -passwdfile is a simple plaintext passwd, see\n"
"                       also -rfbauth and -storepasswd below for obfuscated\n"
"                       VNC password files.  Neither file should be readable\n"
"                       by others.\n"
"-storepasswd pass file Store password \"pass\" as the VNC password in the\n"
"                       file \"file\".  Once the password is stored the\n"
"                       program exits.  Use the password via \"-rfbauth file\"\n"
"\n"
"-accept string         Run a command (possibly to prompt the user at the\n"
"                       X11 display) to decide whether an incoming client\n"
"                       should be allowed to connect or not.  \"string\" is\n"
"                       an external command run via system(3) or some special\n"
"                       cases described below.  Be sure to quote \"string\"\n"
"                       if it contains spaces, shell characters, etc.  If the\n"
"                       external command returns 0 the client is accepted,\n"
"                       otherwise the client is rejected.  See below for an\n"
"                       extension to accept a client view-only.\n"
"\n"
"                       If x11vnc is running as root (say from inetd(1) or from\n"
"                       display managers xdm(1), gdm(1), etc), think about the\n"
"                       security implications carefully before supplying this\n"
"                       option (likewise for the -gone option).\n"
"\n"
"                       Environment: The RFB_CLIENT_IP environment variable will\n"
"                       be set to the incoming client IP number and the port\n"
"                       in RFB_CLIENT_PORT (or -1 if unavailable).  Similarly,\n"
"                       RFB_SERVER_IP and RFB_SERVER_PORT (the x11vnc side\n"
"                       of the connection), are set to allow identification\n"
"                       of the tcp virtual circuit.  The x11vnc process\n"
"                       id will be in RFB_X11VNC_PID, a client id number in\n"
"                       RFB_CLIENT_ID, and the number of other connected clients\n"
"                       in RFB_CLIENT_COUNT.  RFB_MODE will be \"accept\"\n"
"\n"
"                       If \"string\" is \"popup\" then a builtin popup window\n"
"                       is used.  The popup will time out after 120 seconds,\n"
"                       use \"popup:N\" to modify the timeout to N seconds\n"
"                       (use 0 for no timeout)\n"
"\n"
"                       If \"string\" is \"xmessage\" then an xmessage(1)\n"
"                       invocation is used for the command.  xmessage must be\n"
"                       installed on the machine for this to work.\n"
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
"                       Note that x11vnc blocks while the external command\n"
"                       or popup is running (other clients may see no updates\n"
"                       during this period).\n"
"\n"
"                       More -accept tricks: use \"popupmouse\" to only allow\n"
"                       mouse clicks in the builtin popup to be recognized.\n"
"                       Similarly use \"popupkey\" to only recognize\n"
"                       keystroke responses.  These are to help avoid the\n"
"                       user accidentally accepting a client by typing or\n"
"                       clicking. All 3 of the popup keywords can be followed\n"
"                       by +N+M to supply a position for the popup window.\n"
"                       The default is to center the popup window.\n"
"-gone string           As -accept, except to run a user supplied command when\n"
"                       a client goes away (disconnects).  RFB_MODE will be\n"
"                       set to \"gone\" and the other RFB_* variables are as\n"
"                       in -accept.  Unlike -accept, the command return code\n"
"                       is not interpreted by x11vnc.  Example: -gone 'xlock &'\n"
"\n"
"-users list            If x11vnc is started as root (say from inetd(1) or from\n"
"                       display managers xdm(1), gdm(1), etc), then as soon\n"
"                       as possible after connections to the X display are\n"
"                       established try to switch to one of the users in the\n"
"                       comma separated \"list\".  If x11vnc is not running as\n"
"                       root this option is ignored.\n"
"                       \n"
"                       Why use this option?  In general it is not needed since\n"
"                       x11vnc is already connected to the X display and can\n"
"                       perform its primary functions.  The option was added\n"
"                       to make some of the *external* utility commands x11vnc\n"
"                       occasionally runs work properly.  In particular under\n"
"                       GNOME and KDE to implement the \"-solid color\" feature\n"
"                       external commands (gconftool-2 and dcop) must be run\n"
"                       as the user owning the desktop session.  Since this\n"
"                       option switches userid it also affects the userid used\n"
"                       to run the processes for the -accept and -gone options.\n"
"                       It also affects the ability to read files for options\n"
"                       such as -connect, -allow, and -remap.  Note that the\n"
"                       -connect file is also sometimes written to.\n"
"                       \n"
"                       So be careful with this option since in many situations\n"
"                       its use can decrease security.\n"
"                       \n"
"                       The switch to a user will only take place if the\n"
"                       display can still be successfully opened as that user\n"
"                       (this is primarily to try to guess the actual owner\n"
"                       of the session). Example: \"-users fred,wilma,betty\".\n"
"                       Note that a malicious user \"barney\" by quickly using\n"
"                       \"xhost +\" when logging in may get x11vnc to switch\n"
"                       to user \"fred\".  What happens next?\n"
"                       \n"
"                       Under display managers it may be a long time before\n"
"                       the switch succeeds (i.e. a user logs in).  To make\n"
"                       it switch immediately regardless if the display\n"
"                       can be reopened prefix the username with the \"+\"\n"
"                       character. E.g. \"-users +bob\" or \"-users +nobody\".\n"
"                       The latter (i.e. switching immediately to user\n"
"                       \"nobody\") is probably the only use of this option\n"
"                       that increases security.\n"
"                       \n"
"                       To immediately switch to a user *before* connections\n"
"                       to the X display are made or any files opened use the\n"
"                       \"=\" character: \"-users =bob\".  That user needs to\n"
"                       be able to open the X display of course.\n"
"                       \n"
"                       The special user \"guess=\" means to examine the utmpx\n"
"                       database (see who(1)) looking for a user attached to\n"
"                       the display number (from DISPLAY or -display option)\n"
"                       and try him/her.  To limit the list of guesses, use:\n"
"                       \"-users guess=bob,betty\".\n"
"                       \n"
"                       Even more sinister is the special user \"lurk=\" that\n"
"                       means to try to guess the DISPLAY from the utmpx login\n"
"                       database as well.  So it \"lurks\" waiting for anyone\n"
"                       to log into an X session and then connects to it.\n"
"                       Specify a list of users after the = to limit which\n"
"                       users will be tried.  To enable a different searching\n"
"                       mode, if the first user in the list is something like\n"
"                       \":0\" or \":0-2\" that indicates a range of DISPLAY\n"
"                       numbers that will be tried (regardless of whether\n"
"                       they are in the utmpx database) for all users that\n"
"                       are logged in.  Examples: \"-users lurk=\" and also\n"
"                       \"-users lurk=:0-1,bob,mary\"\n"
"                       \n"
"                       Be especially careful using the \"guess=\" and \"lurk=\"\n"
"                       modes.  They are not recommended for use on machines\n"
"                       with untrustworthy local users.\n"
"                       \n"
"-noshm                 Do not use the MIT-SHM extension for the polling.\n"
"                       Remote displays can be polled this way: be careful this\n"
"                       can use large amounts of network bandwidth.  This is\n"
"                       also of use if the local machine has a limited number\n"
"                       of shm segments and -onetile is not sufficient.\n"
"-flipbyteorder         Sometimes needed if remotely polled host has different\n"
"                       endianness.  Ignored unless -noshm is set.\n"
"-onetile               Do not use the new copy_tiles() framebuffer mechanism,\n"
"                       just use 1 shm tile for polling.  Limits shm segments\n"
"                       used to 3.\n"
"\n"
"-solid [color]         To improve performance, when VNC clients are connected\n"
"                       try to change the desktop background to a solid color.\n"
"                       The [color] is optional: the default color is \"cyan4\".\n"
"                       For a different one specify the X color (rgb.txt name,\n"
"                       e.g. \"darkblue\" or numerical \"#RRGGBB\").\n"
"\n"
"                       Currently this option only works on GNOME, KDE, CDE,\n"
"                       and classic X (i.e. with the background image on the\n"
"                       root window).  The \"gconftool-2\" and \"dcop\" external\n"
"                       commands are run for GNOME and KDE respectively.\n"
"                       Other desktops won't work, e.g. Xfce (send us the\n"
"                       corresponding commands if you find them).  If x11vnc is\n"
"                       running as root (inetd(1) or gdm(1)), the -users option\n"
"                       may be needed for GNOME and KDE.  If x11vnc guesses\n"
"                       your desktop incorrectly, you can force it by prefixing\n"
"                       color with \"gnome:\", \"kde:\", \"cde:\" or \"root:\".\n"
"-blackout string       Black out rectangles on the screen. \"string\" is a\n"
"                       comma separated list of WxH+X+Y type geometries for\n"
"                       each rectangle.\n"
"-xinerama              If your screen is composed of multiple monitors\n"
"                       glued together via XINERAMA, and that screen is\n"
"                       non-rectangular this option will try to guess the\n"
"                       areas to black out (if your system has libXinerama).\n"
"\n"
"                       In general on XINERAMA displays you may need to use the\n"
"                       -xwarppointer option if the mouse pointer misbehaves.\n"
"\n"
"-xtrap                 Use the DEC-XTRAP extension for keystroke and mouse\n"
"                       input insertion.  For use on legacy systems, e.g. X11R5,\n"
"                       running an incomplete or missing XTEST extension.\n"
"                       By default DEC-XTRAP will be used if XTEST server grab\n"
"                       control is missing, use -xtrap to do the keystroke and\n"
"                       mouse insertion via DEC-XTRAP as well.\n"
"\n"
"-xrandr [mode]         If the display supports the XRANDR (X Resize, Rotate\n"
"                       and Reflection) extension, and you expect XRANDR events\n"
"                       to occur to the display while x11vnc is running, this\n"
"                       options indicates x11vnc should try to respond to\n"
"                       them (as opposed to simply crashing by assuming the\n"
"                       old screen size).  See the xrandr(1) manpage and run\n"
"                       'xrandr -q' for more info.  [mode] is optional and\n"
"                       described below.\n"
"\n"
"                       Since watching for XRANDR events and trapping errors\n"
"                       increases polling overhead, only use this option if\n"
"                       XRANDR changes are expected.  For example on a rotatable\n"
"                       screen PDA or laptop, or using a XRANDR-aware Desktop\n"
"                       where you resize often.  It is best to be viewing with a\n"
"                       vncviewer that supports the NewFBSize encoding, since it\n"
"                       knows how to react to screen size changes.  Otherwise,\n"
"                       libvncserver tries to do so something reasonable for\n"
"                       viewers that cannot do this (portions of the screen\n"
"                       may be clipped, unused, etc).\n"
"\n"
"                       \"mode\" defaults to \"resize\", which means create a\n"
"                       new, resized, framebuffer and hope all viewers can cope\n"
"                       with the change.  \"newfbsize\" means first disconnect\n"
"                       all viewers that do not support the NewFBSize VNC\n"
"                       encoding, and then resize the framebuffer.  \"exit\"\n"
"                       means disconnect all viewer clients, and then terminate\n"
"                       x11vnc.\n"
"-padgeom WxH           Whenever a new vncviewer connects, the framebuffer is\n"
"                       replaced with a fake, solid black one of geometry WxH.\n"
"                       Shortly afterwards the framebuffer is replaced with the\n"
"                       real one.  This is intended for use with vncviewers\n"
"                       that do not support NewFBSize and one wants to make\n"
"                       sure the initial viewer geometry will be big enough\n"
"                       to handle all subsequent resizes (e.g. under -xrandr,\n"
"                       -remote id:windowid, rescaling, etc.)\n"
"\n"
"-o logfile             Write stderr messages to file \"logfile\" instead of\n"
"                       to the terminal.  Same as \"-logfile file\".  To append\n"
"                       to the file use \"-oa file\" or \"-logappend file\".\n"
"-flag file             Write the \"PORT=NNNN\" (e.g. PORT=5900) string to\n"
"                       \"file\" in addition to stdout.  This option could be\n"
"                       useful by wrapper script to detect when x11vnc is ready.\n"
"\n"
"-rc filename           Use \"filename\" instead of $HOME/.x11vncrc for rc file.\n"
"-norc                  Do not process any .x11vncrc file for options.\n"
"\n"
"-h, -help              Print this help text.\n"
"-?, -opts              Only list the x11vnc options.\n"
"-V, -version           Print program version and last modification date.\n"
"\n"
"-dbg                   instead of exiting after cleaning up, run a simple\n"
"                       \"debug crash shell\" when fatal errors are trapped.\n"
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
"-modtweak              Option -modtweak automatically tries to adjust the AltGr\n"
"-nomodtweak            and Shift modifiers for differing language keyboards\n"
"                       between client and host.  Otherwise, only a single key\n"
"                       press/release of a Keycode is simulated (i.e. ignoring\n"
"                       the state of the modifiers: this usually works for\n"
"                       identical keyboards).  Also useful in resolving cases\n"
"                       where a Keysym is bound to multiple keys (e.g. \"<\" + \">\"\n"
"                       and \",\" + \"<\" keys).  Default: %s\n"
#if 0
"-isolevel3             When in modtweak mode, always send ISO_Level3_Shift to\n"
"                       the X server instead of Mode_switch (AltGr).\n"
#endif
"-xkb                   When in modtweak mode, use the XKEYBOARD extension (if\n"
"-noxkb                 the X display supports it) to do the modifier tweaking.\n"
"                       This is powerful and should be tried if there are still\n"
"                       keymapping problems when using -modtweak by itself.\n"
"                       The default is to check whether some common keysyms,\n"
"                       e.g. !, @, [, are only accessible via -xkb mode and if\n"
"                       so then automatically enable the mode.  To disable this\n"
"                       automatic detection use -noxkb.\n"
"-skip_keycodes string  Ignore the comma separated list of decimal keycodes.\n"
"                       Perhaps these are keycodes not on your keyboard but\n"
"                       your X server thinks exist.  Currently only applies\n"
"                       to -xkb mode.  Use this option to help x11vnc in the\n"
"                       reverse problem it tries to solve: Keysym -> Keycode(s)\n"
"                       when ambiguities exist (more than one Keycode per\n"
"                       Keysym).  Run 'xmodmap -pk' to see your keymapping.\n"
"                       Example: \"-skip_keycodes 94,114\"\n"
"-skip_dups             Some VNC viewers send impossible repeated key events,\n"
"-noskip_dups           e.g. key-down, key-down, key-up, key-up all for the\n"
"                       same key, or 20 downs in a row for the same key!\n"
"                       Setting -skip_dups means to skip these duplicates and\n"
"                       just process the first event.  Default: %s\n"
"-add_keysyms           If a Keysym is received from a VNC viewer and\n"
"-noadd_keysyms         that Keysym does not exist in the X server, then\n"
"                       add the Keysym to the X server's keyboard mapping.\n"
"                       Added Keysyms will be removed periodically and also\n"
"                       when x11vnc exits.  Default: %s\n"
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
"                       Note that this option and -clear_mods can interfere\n"
"                       with a person typing at the physical keyboard.\n"
"-remap string          Read Keysym remappings from file named \"string\".\n"
"                       Format is one pair of Keysyms per line (can be name\n"
"                       or hex value) separated by a space.  If no file named\n"
"                       \"string\" exists, it is instead interpreted as this\n"
"                       form: key1-key2,key3-key4,...  See <X11/keysymdef.h>\n"
"                       header file for a list of Keysym names, or use xev(1).\n"
"                       To map a key to a button click, use the fake Keysyms\n"
"                       \"Button1\", ..., etc. E.g: \"-remap Super_R-Button2\"\n"
"                       (useful for pasting on a laptop)\n"
"\n"
"                       Dead keys: \"dead\" (or silent, mute) keys are keys that\n"
"                       do not produce a character but must be followed by a 2nd\n"
"                       keystroke.  This is often used for accenting characters,\n"
"                       e.g. to put \"'\" on top of \"a\" by pressing the dead\n"
"                       key and then \"a\".  Note that this interpretation\n"
"                       is not part of core X11, it is up to the toolkit or\n"
"                       application to decide how to react to the sequence.\n"
"                       The X11 names for these keysyms are \"dead_grave\",\n"
"                       \"dead_acute\", etc.  However some VNC viewers send the\n"
"                       keysyms \"grave\", \"acute\" instead thereby disabling\n"
"                       the accenting.  To work around this -remap can be used.\n"
"                       For example \"-remap grave-dead_grave,acute-dead_acute\"\n"
"                       As a convenience, \"-remap DEAD\" applies these remaps:\n"
"\n"
"                               g     grave-dead_grave\n"
"                               a     acute-dead_acute\n"
"                               c     asciicircum-dead_circumflex\n"
"                               t     asciitilde-dead_tilde\n"
"                               m     macron-dead_macron\n"
"                               b     breve-dead_breve\n"
"                               D     abovedot-dead_abovedot\n"
"                               d     diaeresis-dead_diaeresis\n"
"                               o     degree-dead_abovering\n"
"                               A     doubleacute-dead_doubleacute\n"
"                               r     caron-dead_caron\n"
"                               e     cedilla-dead_cedilla\n"
"\n"
"                       If you just want a subset use the first letter\n"
"                       label, e.g. \"-remap DEAD=ga\" to get the first two.\n"
"                       Additional remaps may also be supplied via commas,\n"
"                       e.g.  \"-remap DEAD=ga,Super_R-Button2\".  Finally,\n"
"                       \"DEAD=missing\" means to apply all of the above as\n"
"                       long as the left hand member is not already in the\n"
"                       X11 keymap.\n"
"\n"
"-norepeat              Option -norepeat disables X server key auto repeat when\n"
"-repeat                VNC clients are connected and VNC keyboard input is\n"
"                       not idle for more than 5 minutes.  This works around a\n"
"                       repeating keystrokes bug (triggered by long processing\n"
"                       delays between key down and key up client events: either\n"
"                       from large screen changes or high latency).\n"
"                       Default: %s\n"
"\n"
"                       Note: your VNC viewer side will likely do autorepeating,\n"
"                       so this is no loss unless someone is simultaneously at\n"
"                       the real X display.\n"
"\n"
"                       Use \"-norepeat N\" to set how many times norepeat will\n"
"                       be reset if something else (e.g. X session manager)\n"
"                       disables it.  The default is 2.  Use a negative value\n"
"                       for unlimited resets.\n"
"\n"
"-nofb                  Ignore video framebuffer: only process keyboard and\n"
"                       pointer.  Intended for use with Win2VNC and x2vnc\n"
"                       dual-monitor setups.\n"
"-nobell                Do not watch for XBell events. (no beeps will be heard)\n"
"                       Note: XBell monitoring requires the XKEYBOARD extension.\n"
"-nosel                 Do not manage exchange of X selection/cutbuffer between\n"
"                       VNC viewers and the X server.\n"
"-noprimary             Do not poll the PRIMARY selection for changes to send\n"
"                       back to clients.  (PRIMARY is still set on received\n"
"                       changes, however).\n"
"\n"
"-cursor [mode]         Sets how the pointer cursor shape (little icon at the\n"
"-nocursor              mouse pointer) should be handled.  The \"mode\" string\n"
"                       is optional and is described below.  The default\n"
"                       is to show some sort of cursor shape(s).  How this\n"
"                       is done depends on the VNC viewer and the X server.\n"
"                       Use -nocursor to disable cursor shapes completely.\n"
"\n"
"                       Some VNC viewers support the TightVNC CursorPosUpdates\n"
"                       and CursorShapeUpdates extensions (cuts down on\n"
"                       network traffic by not having to send the cursor image\n"
"                       every time the pointer is moved), in which case these\n"
"                       extensions are used (see -nocursorshape and -nocursorpos\n"
"                       below to disable).  For other viewers the cursor shape\n"
"                       is written directly to the framebuffer every time the\n"
"                       pointer is moved or changed and gets sent along with\n"
"                       the other framebuffer updates.  In this case, there\n"
"                       will be some lag between the vnc viewer pointer and\n"
"                       the remote cursor position.\n"
"\n"
"                       If the X display supports retrieving the cursor shape\n"
"                       information from the X server, then the default is\n"
"                       to use that mode.  On Solaris this can be done with\n"
"                       the SUN_OVL extension using -overlay (see also the\n"
"                       -overlay_nomouse option).  A similar overlay scheme\n"
"                       is used on IRIX.  Xorg (e.g. Linux) and recent Solaris\n"
"                       Xsun servers support the XFIXES extension to retrieve\n"
"                       the exact cursor shape from the X server.  If XFIXES\n"
"                       is present it is preferred over Overlay and is used by\n"
"                       default (see -noxfixes below).  This can be disabled\n"
"                       with -nocursor, and also some values of the \"mode\"\n"
"                       option below.\n"
"                       \n"
"                       Note that under XFIXES cursors with transparency (alpha\n"
"                       channel) will not be exactly represented and one may\n"
"                       find Overlay preferable.  See also the -alphacut and\n"
"                       -alphafrac options below as fudge factors to try to\n"
"                       improve the situation for cursors with transparency\n"
"                       for a given theme.\n"
"\n"
"                       The \"mode\" string can be used to fine-tune the\n"
"                       displaying of cursor shapes.  It can be used the\n"
"                       following ways:\n"
"\n"
"                       \"-cursor arrow\" - just show the standard arrow\n"
"                       nothing more or nothing less.\n"
"\n"
"                       \"-cursor none\" - same as \"-nocursor\"\n"
"\n"
"                       \"-cursor X\" - when the cursor appears to be on the\n"
"                       root window, draw the familiar X shape.  Some desktops\n"
"                       such as GNOME cover up the root window completely,\n"
"                       and so this will not work, try \"X1\", etc, to try to\n"
"                       shift the tree depth.  On high latency links or slow\n"
"                       machines there will be a time lag between expected and\n"
"                       the actual cursor shape.\n"
"\n"
"                       \"-cursor some\" - like \"X\" but use additional\n"
"                       heuristics to try to guess if the window should have\n"
"                       a windowmanager-like resizer cursor or a text input\n"
"                       I-beam cursor.  This is a complete hack, but may be\n"
"                       useful in some situations because it provides a little\n"
"                       more feedback about the cursor shape.\n"
"\n"
"                       \"-cursor most\" - try to show as many cursors as\n"
"                       possible.  Often this will only be the same as \"some\"\n"
"                       unless the display has overlay visuals or XFIXES\n"
"                       extensions available.  On Solaris and IRIX if XFIXES\n"
"                       is not available, -overlay mode will be attempted.\n"
"\n"
"-arrow n               Choose an alternate \"arrow\" cursor from a set of\n"
"                       some common ones.  n can be 1 to %d.  Default is: %d\n"
"\n"
"-noxfixes              Do not use the XFIXES extension to draw the exact cursor\n"
"                       shape even if it is available.\n"
"-alphacut n            When using the XFIXES extension for the cursor shape,\n"
"                       cursors with transparency will not be displayed exactly\n"
"                       (but opaque ones will).  This option sets n as a cutoff\n"
"                       for cursors that have transparency (\"alpha channel\"\n"
"                       with values ranging from 0 to 255) Any cursor pixel with\n"
"                       alpha value less than n becomes completely transparent.\n"
"                       Otherwise the pixel is completely opaque.  Default %d\n"
"                       \n"
"                       Note: the options -alphacut, -alphafrac, and -alphafrac\n"
"                       may be removed if a more accurate internal method for\n"
"                       handling cursor transparency is implemented.\n"
"-alphafrac fraction    With the threshold in -alphacut some cursors will become\n"
"                       almost completely transparent because their alpha values\n"
"                       are not high enough.  For those cursors adjust the\n"
"                       alpha threshold until fraction of the non-zero alpha\n"
"                       channel pixels become opaque.  Default %.2f\n"
"-alpharemove           By default, XFIXES cursors pixels with transparency have\n"
"                       the alpha factor multiplied into the RGB color values\n"
"                       (i.e. that corresponding to blending the cursor with a\n"
"                       black background).  Specify this option to remove the\n"
"                       alpha factor. (useful for light colored semi-transparent\n"
"                       cursors).\n"
"-noalphablend          In XFIXES mode do not send cursor alpha channel data\n"
"                       to libvncserver.  The default is to send it.  The\n"
"                       alphablend effect will only be visible in -nocursorshape\n"
"                       mode or for clients with cursorshapeupdates turned\n"
"                       off. (However there is a hack for 32bpp with depth 24,\n"
"                       it uses the extra 8 bits to store cursor transparency\n"
"                       for use with a hacked vncviewer that applies the\n"
"                       transparency locally.  See the FAQ for more info).\n"
"\n"
"-nocursorshape         Do not use the TightVNC CursorShapeUpdates extension\n"
"                       even if clients support it.  See -cursor above.\n"
"-cursorpos             Option -cursorpos enables sending the X cursor position\n"
"-nocursorpos           back to all vnc clients that support the TightVNC\n"
"                       CursorPosUpdates extension.  Other clients will be able\n"
"                       to see the pointer motions. Default: %s\n"
"-xwarppointer          Move the pointer with XWarpPointer(3X) instead of\n"
"                       the XTEST extension.  Use this as a workaround\n"
"                       if the pointer motion behaves incorrectly, e.g.\n"
"                       on touchscreens or other non-standard setups.\n"
"                       Also sometimes needed on XINERAMA displays.\n"
"\n"
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
"                       See <X11/keysymdef.h> header file for a list of Keysyms,\n"
"                       or use the xev(1) program.  Note: mapping of button\n"
"                       clicks to Keysyms may not work if -modtweak or -xkb is\n"
"                       needed for the Keysym.\n"
"\n"
"                       If you include a modifier like \"Shift_L\" the\n"
"                       modifier's up/down state is toggled, e.g. to send\n"
"                       \"The\" use :Shift_L+t+Shift_L+h+e: (the 1st one is\n"
"                       shift down and the 2nd one is shift up). (note: the\n"
"                       initial state of the modifier is ignored and not reset)\n"
"                       To include button events use \"Button1\", ... etc.\n"
"\n"
"-nodragging            Do not update the display during mouse dragging events\n"
"                       (mouse button held down).  Greatly improves response on\n"
"                       slow setups, but you lose all visual feedback for drags,\n"
"                       text selection, and some menu traversals.  It overrides\n"
"                       any -pointer_mode setting\n"
"\n"
"-wireframe [str]       Try to detect window moves or resizes when a mouse\n"
"-nowireframe           button is held down and show a wireframe instead of\n"
"                       the full opaque window.  This is based completely on\n"
"                       heuristics and may not always work: it depends on your\n"
"                       window manager and even how you move things around.\n"
"                       See -pointer_mode below for discussion of the \"bogging\n"
"                       down\" problem this tries to avoid.\n"
"                       Default: %s\n"
"\n"
"                       Shorter aliases:  -wf [str]  and -nowf\n"
"\n"
"                       The value \"str\" is optional and, of course, is\n"
"                       packed with many tunable parameters for this scheme:\n"
"\n"
"                       Format: shade,linewidth,percent,T+B+L+R,t1+t2+t3+t4\n"
"                       Default: %s\n"
"\n"
"                       If you leave nothing between commas: \",,\" the default\n"
"                       value is used.  If you don't specify enough commas,\n"
"                       the trailing parameters are set to their defaults.\n"
"\n"
"                       \"shade\" indicate the \"color\" for the wireframe,\n"
"                       usually a greyscale: 0-255, however for 16 and 32bpp you\n"
"                       can specify an rgb.txt X color (e.g. \"dodgerblue\") or\n"
"                       a value > 255 is treated as RGB (e.g. red is 0xff0000).\n"
"                       \"linewidth\" sets the width of the wireframe in pixels.\n"
"                       \"percent\" indicates to not apply the wireframe scheme\n"
"                       to windows with area less than this percent of the\n"
"                       full screen.\n"
"\n"
"                       \"T+B+L+R\" indicates four integers for how close in\n"
"                       pixels the pointer has to be from the Top, Bottom, Left,\n"
"                       or Right edges of the window to consider wireframing.\n"
"                       This is a speedup to quickly exclude a window from being\n"
"                       wireframed: set them all to zero to not try the speedup\n"
"                       (scrolling and selecting text will likely be slower).\n"
"\n"
"                       \"t1+t2+t3+t4\" specify four floating point times in\n"
"                       seconds: t1 is how long to wait for the pointer to move,\n"
"                       t2 is how long to wait for the window to start moving\n"
"                       or being resized (for some window managers this can be\n"
"                       rather long), t3 is how long to keep a wireframe moving\n"
"                       before repainting the window. t4 is the minimum time\n"
"                       between sending wireframe \"animations\".  For a slow\n"
"                       link this might be a better choice: 0.25+0.6+6.0+0.15\n"
"\n"
"-wirecopyrect mode     Since the -wireframe mechanism evidently tracks moving\n"
"-nowirecopyrect        windows accurately, a speedup can be obtained by\n"
"                       telling the VNC viewers to locally copy the translated\n"
"                       window region.  This is the VNC CopyRect encoding:\n"
"                       the framebuffer update doesn't need to send the actual\n"
"                       new image data.\n"
"\n"
"                       Shorter aliases:  -wcr [mode]  and -nowcr\n"
"\n"
"                       \"mode\" can be \"never\" (same as -nowirecopyrect)\n"
"                       to never try the copyrect, \"top\" means only do it if\n"
"                       the window was not covered by any other windows, and\n"
"                       \"always\" means to translate the orginally unobscured\n"
"                       region (this may look odd as the remaining pieces come\n"
"                       in, but helps on a slow link).  Default: \"%s\"\n"
"\n"
"                       Note: there can be painting errors when using -scale\n"
"                       so CopyRect is skipped when scaling unless you specify\n"
"                       -wirecopyrect on the command line or by remote-control.\n"
"\n"
"-debug_wireframe       Turn on debugging info printout for the wireframe\n"
"                       heuristics.  \"-dwf\" is an alias.  Specify multiple\n"
"                       times for more output.\n"
"\n"
"-scrollcopyrect mode   Like -wirecopyrect, but use heuristics to try to guess\n"
"-noscrollcopyrect      if a window has scrolled its contents (either vertically\n"
"                       or horizontally).  This requires the RECORD X extension\n"
"                       to \"snoop\" on X applications (currently for certain\n"
"                       XCopyArea and XConfigureWindow X protocol requests).\n"
"                       Examples: Hitting <Return> in a terminal window when the\n"
"                       cursor was at the bottom, the text scrolls up one line.\n"
"                       Hitting <Down> arrow in a web browser window, the web\n"
"                       page scrolls up a small amount.\n"
"\n"
"                       Shorter aliases:  -scr [mode]  and -noscr\n"
"\n"
"                       This scheme will not always detect scrolls, but when\n"
"                       it does there is a nice speedup from using the VNC\n"
"                       CopyRect encoding (see -wirecopyrect).  The speedup\n"
"                       is both in reduced network traffic and reduced X\n"
"                       framebuffer polling/copying.  On the other hand, it may\n"
"                       induce undesired transients (e.g. a terminal cursor\n"
"                       being scrolled up when it should not be) or other\n"
"                       painting errors (window tearing, bunching-up, etc).\n"
"                       These are automatically repaired in a short period\n"
"                       of time.  If this is unacceptable disable the feature\n"
"                       with -noscrollcopyrect.\n"
"\n"
"                       \"mode\" can be \"never\" (same as -noscrollcopyrect)\n"
"                       to never try the copyrect, \"keys\" means to try it\n"
"                       in response to keystrokes only, \"mouse\" means to\n"
"                       try it in response to mouse events only, \"always\"\n"
"                       means to do both. Default: \"%s\"\n"
"\n"
"                       Note: there can be painting errors when using\n"
"                       -scale so CopyRect is skipped when scaling unless\n"
"                       you specify -scrollcopyrect on the command line or\n"
"                       by remote-control.\n"
"\n"
"-scr_area n            Set the minimum area in pixels for a rectangle\n"
"                       to be considered for the -scrollcopyrect detection\n"
"                       scheme.  This is to avoid wasting the effort on small\n"
"                       rectangles that would be quickly updated the normal way.\n"
"                       E.g. suppose an app updated the position of its skinny\n"
"                       scrollbar first and then shifted the large panel\n"
"                       it controlled.  We want to be sure to skip the small\n"
"                       scrollbar and get the large panel. Default: %d\n"
"\n"
"-scr_skip list         Skip scroll detection for applications matching\n"
"                       the comma separated list of strings in \"list\".\n"
"                       Some applications implement their scrolling in\n"
"                       strange ways where the XCopyArea, etc, also applies\n"
"                       to invisible portions of the window: if we CopyRect\n"
"                       those areas it looks awful during the scroll and\n"
"                       there may be painting errors left after the scroll.\n"
"                       Soffice.bin is the worst known offender.\n"
"\n"
"                       Use \"##\" to denote the start of the application class\n"
"                       (e.g. \"##XTerm\") and \"++\" to denote the start\n"
"                       of the application instance name (e.g. \"++xterm\").\n"
"                       The string your list is matched against is of the form\n"
"                       \"^^WM_NAME##Class++Instance<same-for-any-subwindows>\"\n"
"                       The \"xlsclients -la\" command will provide this info.\n"
"\n"
"                       If a pattern is prefixed with \"KEY:\" it only applies\n"
"                       to Keystroke generated scrolls (e.g. Up arrow).  If it\n"
"                       is prefixed with \"MOUSE:\" it only applies to Mouse\n"
"                       induced scrolls (e.g. dragging on a scrollbar).\n"
"                       Default: %s\n"
"\n"
"-scr_inc list          Opposite of -scr_skip: this list is consulted first\n"
"                       and if there is a match the window will be monitored\n"
"                       via RECORD for scrolls irrespective of -scr_skip.\n"
"                       Use -scr_skip '*' to skip anything that does not match\n"
"                       your -scr_inc.  Use -scr_inc '*' to include everything.\n"
"\n"
"-scr_keys list         For keystroke scroll detection, only apply the RECORD\n"
"                       heuristics to the comma separated list of keysyms in\n"
"                       \"list\".  You may find the RECORD overhead for every\n"
"                       one of your keystrokes disrupts typing too much, but you\n"
"                       don't want to turn it off completely with \"-scr mouse\"\n"
"                       and -scr_parms does not work or is too confusing.\n"
"\n"
"                       The listed keysyms can be numeric or the keysym\n"
"                       names in the <X11/keysymdef.h> header file or from the\n"
"                       xev(1) program.  Example: \"-scr_keys Up,Down,Return\".\n"
"                       One probably wants to have application specific lists\n"
"                       (e.g. for terminals, etc) but that is too icky to think\n"
"                       about for now...\n"
"\n"
"                       If \"list\" begins with the \"-\" character the list\n"
"                       is taken as an exclude list: all keysyms except those\n"
"                       list will be considered.  The special string \"builtin\"\n"
"                       expands to an internal list of keysyms that are likely\n"
"                       to cause scrolls.  BTW, by default modifier keys,\n"
"                       Shift_L, Control_R, etc, are skipped since they almost\n"
"                       never induce scrolling by themselves.\n"
"\n"
"-scr_term list         Yet another cosmetic kludge.  Apply shell/terminal\n"
"                       heuristics to applications matching comma separated\n"
"                       list (same as for -scr_skip/-scr_inc).  For example an\n"
"                       annoying transient under scroll detection is if you\n"
"                       hit Enter in a terminal shell with full text window,\n"
"                       the solid text cursor block will be scrolled up.\n"
"                       So for a short time there are two (or more) block\n"
"                       cursors on the screen.  There are similar scenarios,\n"
"                       (e.g. an output line is duplicated).\n"
"                       \n"
"                       These transients are induced by the approximation of\n"
"                       scroll detection (e.g. it detects the scroll, but not\n"
"                       the fact that the block cursor was cleared just before\n"
"                       the scroll).  In nearly all cases these transient errors\n"
"                       are repaired when the true X framebuffer is consulted\n"
"                       by the normal polling.  But they are distracting, so\n"
"                       what this option provides is extra \"padding\" near the\n"
"                       bottom of the terminal window: a few extra lines near\n"
"                       the bottom will not be scrolled, but rather updated\n"
"                       from the actual X framebuffer.  This usually reduces\n"
"                       the annoying artifacts.  Use \"none\" to disable.\n"
"                       Default: \"%s\"\n"
"\n"
"-scr_keyrepeat lo-hi   If a key is held down (or otherwise repeats rapidly) and\n"
"                       this induces a rapid sequence of scrolls (e.g. holding\n"
"                       down an Arrow key) the \"scrollcopyrect\" detection\n"
"                       and overhead may not be able to keep up.  A time per\n"
"                       single scroll estimate is performed and if that estimate\n"
"                       predicts a sustainable scrollrate of keys per second\n"
"                       between \"lo\" and \"hi\" then repeated keys will be\n"
"                       DISCARDED to maintain the scrollrate. For example your\n"
"                       key autorepeat may be 25 keys/sec, but for a large\n"
"                       window or slow link only 8 scrolls per second can be\n"
"                       sustained, then roughly 2 out of every 3 repeated keys\n"
"                       will be discarded during this period. Default: \"%s\"\n"
"\n"
"-scr_parms string      Set various parameters for the scrollcopyrect mode.\n"
"                       The format is similar to that for -wireframe and packed\n"
"                       with lots of parameters:\n"
"\n"
"                       Format: T+B+L+R,t1+t2+t3,s1+s2+s3+s4+s5\n"
"                       Default: %s\n"
"\n"
"                       If you leave nothing between commas: \",,\" the default\n"
"                       value is used.  If you don't specify enough commas,\n"
"                       the trailing parameters are set to their defaults.\n"
"\n"
"                       \"T+B+L+R\" indicates four integers for how close in\n"
"                       pixels the pointer has to be from the Top, Bottom, Left,\n"
"                       or Right edges of the window to consider scrollcopyrect.\n"
"                       If -wireframe overlaps it takes precedence.  This is a\n"
"                       speedup to quickly exclude a window from being watched\n"
"                       for scrollcopyrect: set them all to zero to not try\n"
"                       the speedup (things like selecting text will likely\n"
"                       be slower).\n"
"\n"
"                       \"t1+t2+t3\" specify three floating point times in\n"
"                       seconds that apply to scrollcopyrect detection with\n"
"                       *Keystroke* input: t1 is how long to wait after a key\n"
"                       is pressed for the first scroll, t2 is how long to keep\n"
"                       looking after a Keystroke scroll for more scrolls.\n"
"                       t3 is how frequently to try to update surrounding\n"
"                       scrollbars outside of the scrolling area (0.0 to\n"
"                       disable)\n"
"\n"
"                       \"s1+s2+s3+s4+s5\" specify five floating point times\n"
"                       in seconds that apply to scrollcopyrect detection with\n"
"                       *Mouse* input: s1 is how long to wait after a mouse\n"
"                       button is pressed for the first scroll, s2 is how long\n"
"                       to keep waiting for additional scrolls after the first\n"
"                       Mouse scroll was detected.  s3 is how frequently to\n"
"                       try to update surrounding scrollbars outside of the\n"
"                       scrolling area (0.0 to disable).  s4 is how long to\n"
"                       buffer pointer motion (to try to get fewer, bigger\n"
"                       mouse scrolls). s5 is the maximum time to spend just\n"
"                       updating the scroll window without updating the rest\n"
"                       of the screen.\n"
"\n"
"-debug_scroll          Turn on debugging info printout for the scroll\n"
"                       heuristics.  \"-ds\" is an alias.  Specify it multiple\n"
"                       times for more output.\n"
"\n"
"-noxrecord             Disable any use of the RECORD extension.  This is\n"
"                       currently used by the -scrollcopyrect scheme and to\n"
"                       monitor X server grabs.\n"
"\n"
"-pointer_mode n        Various pointer motion update schemes. \"-pm\" is\n"
"                       an alias.  The problem is pointer motion can cause\n"
"                       rapid changes on the screen: consider the rapid changes\n"
"                       when you drag a large window around.  Neither x11vnc's\n"
"                       screen polling and vnc compression routines nor the\n"
"                       bandwidth to the vncviewers can keep up these rapid\n"
"                       screen changes: everything will bog down when dragging\n"
"                       or scrolling.  So a scheme has to be used to \"eat\"\n"
"                       much of that pointer input before re-polling the screen\n"
"                       and sending out framebuffer updates. The mode number\n"
"                       \"n\" can be 0 to %d and selects one of the schemes\n"
"                       desribed below.\n"
"\n"
"                       n=0: does the same as -nodragging. (all screen polling\n"
"                       is suspended if a mouse button is pressed.)\n"
"\n"
"                       n=1: was the original scheme used to about Jan 2004:\n"
"                       it basically just skips -input_skip keyboard or pointer\n"
"                       events before repolling the screen.\n"
"\n"
"                       n=2 is an improved scheme: by watching the current rate\n"
"                       of input events it tries to detect if it should try to\n"
"                       \"eat\" additional pointer events before continuing.\n"
"\n"
"                       n=3 is basically a dynamic -nodragging mode: it detects\n"
"                       when the mouse motion has paused and then refreshes\n"
"                       the display.\n"
"\n"
"                       n=4 attempts to measures network rates and latency,\n"
"                       the video card read rate, and how many tiles have been\n"
"                       changed on the screen.  From this, it aggressively tries\n"
"                       to push screen \"frames\" when it decides it has enough\n"
"                       resources to do so.  NOT FINISHED.\n"
"\n"
"                       The default n is %d. Note that modes 2, 3, 4 will skip\n"
"                       -input_skip keyboard events (but it will not count\n"
"                       pointer events).  Also note that these modes are not\n"
"                       available in -threads mode which has its own pointer\n"
"                       event handling mechanism.\n"
"\n"
"                       To try out the different pointer modes to see which\n"
"                       one gives the best response for your usage, it is\n"
"                       convenient to use the remote control function, for\n"
"                       example \"x11vnc -R pm:4\" or the tcl/tk gui (Tuning ->\n"
"                       pointer_mode -> n).\n"
"\n"
"-input_skip n          For the pointer handling when non-threaded: try to\n"
"                       read n user input events before scanning display. n < 0\n"
"                       means to act as though there is always user input.\n"
"                       Default: %d\n"
"\n"
"-speeds rd,bw,lat      x11vnc tries to estimate some speed parameters that\n"
"                       are used to optimize scheduling (e.g. -pointer_mode\n"
"                       4) and other things.  Use the -speeds option to set\n"
"                       these manually.  The triple \"rd,bw,lat\" corresponds\n"
"                       to video h/w read rate in MB/sec, network bandwidth to\n"
"                       clients in KB/sec, and network latency to clients in\n"
"                       milliseconds, respectively.  If a value is left blank,\n"
"                       e.g. \"-speeds ,100,15\", then the internal scheme is\n"
"                       used to estimate the empty value(s).\n"
"\n"
"                       Note: use this option is currently NOT FINISHED.\n"
"\n"
"                       Typical PC video cards have read rates of 5-10 MB/sec.\n"
"                       If the framebuffer is in main memory instead of video\n"
"                       h/w (e.g. SunRay, shadowfb, Xvfb), the read rate may\n"
"                       be much faster.  \"x11perf -getimage500\" can be used\n"
"                       to get a lower bound (remember to factor in the bytes\n"
"                       per pixel).  It is up to you to estimate the network\n"
"                       bandwith and latency to clients.  For the latency the\n"
"                       ping(1) command can be used.\n"
"\n"
"                       For convenience there are some aliases provided,\n"
"                       e.g. \"-speeds modem\".  The aliases are: \"modem\" for\n"
"                       6,4,200; \"dsl\" for 6,100,50; and \"lan\" for 6,5000,1\n"
"\n"
"-debug_pointer         Print debugging output for every pointer event.\n"
"-debug_keyboard        Print debugging output for every keyboard event.\n"
"                       Same as -dp and -dk, respectively.  Use multiple\n"
"                       times for more output.\n"
"\n"
"-defer time            Time in ms to wait for updates before sending to client\n"
"                       (deferUpdateTime)  Default: %d\n"
"-wait time             Time in ms to pause between screen polls.  Used to cut\n"
"                       down on load.  Default: %d\n"
"-wait_ui factor        Factor by which to cut the -wait time if there\n"
"                       has been recent user input (pointer or keyboard).\n"
"                       Improves response, but increases the load whenever you\n"
"                       are moving the mouse or typing.  Default: %.2f\n"
"-nowait_bog            Do not detect if the screen polling is \"bogging down\"\n"
"                       and sleep more.  Some activities with no user input can\n"
"                       slow things down a lot: consider a large terminal window\n"
"                       with a long build running in it continously streaming\n"
"                       text output.  By default x11vnc will try to detect this\n"
"                       (3 screen polls in a row each longer than 0.25 sec with\n"
"                       no user input), and sleep up to 1.5 secs to let things\n"
"                       \"catch up\".  Use this option to disable the detection.\n"
"-readtimeout n         Set libvncserver rfbMaxClientWait to n seconds. On\n"
"                       slow links that take a long time to paint the first\n"
"                       screen libvncserver may hit the timeout and drop the\n"
"                       connection.  Default: %d seconds.\n"
"-nap                   Monitor activity and if it is low take longer naps\n"
"-nonap                 between screen polls to really cut down load when idle.\n"
"                       Default: %s\n"
"-sb time               Time in seconds after NO activity (e.g. screen blank)\n"
"                       to really throttle down the screen polls (i.e. sleep\n"
"                       for about 1.5 secs). Use 0 to disable.  Default: %d\n"
"\n"
"-noxdamage             Do not use the X DAMAGE extension to detect framebuffer\n"
"                       changes even if it is available.  Use -xdamage if your\n"
"                       default is to have it off.\n"
"\n"
"                       x11vnc's use of the DAMAGE extension: 1) significantly\n"
"                       reduces the load when the screen is not changing much,\n"
"                       and 2) detects changed areas (small ones by default)\n"
"                       more quickly.\n"
"\n"
"                       Currently the DAMAGE extension is overly conservative\n"
"                       and often reports large areas (e.g. a whole terminal\n"
"                       or browser window) as damaged even though the actual\n"
"                       changed region is much smaller (sometimes just a few\n"
"                       pixels).  So heuristics were introduced to skip large\n"
"                       areas and use the damage rectangles only as \"hints\"\n"
"                       for the traditional scanline polling.  The following\n"
"                       tuning parameters are introduced to adjust this\n"
"                       behavior:\n"
"\n"
"-xd_area A             Set the largest DAMAGE rectangle area \"A\" (in\n"
"                       pixels: width * height) to trust as truly damaged:\n"
"                       the rectangle will be copied from the framebuffer\n"
"                       (slow) no matter what.  Set to zero to trust *all*\n"
"                       rectangles. Default: %d\n"
"-xd_mem f              Set how long DAMAGE rectangles should be \"remembered\",\n"
"                       \"f\" is a floating point number and is in units of the\n"
"                       scanline repeat cycle time (%d iterations).  The default\n"
"                       (%.1f) should give no painting problems. Increase it if\n"
"                       there are problems or decrease it to live on the edge\n"
"                       (perhaps useful on a slow machine).\n"
"\n"
"-sigpipe string        Broken pipe (SIGPIPE) handling.  \"string\" can be\n"
"                       \"ignore\" or \"exit\".  For \"ignore\" libvncserver\n"
"                       will handle the abrupt loss of a client and continue,\n"
"                       for \"exit\" x11vnc will cleanup and exit at the 1st\n"
"                       broken connection.  Default: \"ignore\".\n"
"-threads               Whether or not to use the threaded libvncserver\n"
"-nothreads             algorithm [rfbRunEventLoop] if libpthread is available\n"
"                       Default: %s\n"
"\n"
"-fs f                  If the fraction of changed tiles in a poll is greater\n"
"                       than f, the whole screen is updated.  Default: %.2f\n"
"-gaps n                Heuristic to fill in gaps in rows or cols of n or\n"
"                       less tiles.  Used to improve text paging.  Default: %d\n"
"-grow n                Heuristic to grow islands of changed tiles n or wider\n"
"                       by checking the tile near the boundary.  Default: %d\n"
"-fuzz n                Tolerance in pixels to mark a tiles edges as changed.\n"
"                       Default: %d\n"
"-debug_tiles           Print debugging output for tiles, fb updates, etc.\n"
"\n"
"-snapfb                Instead of polling the X display framebuffer (fb) for\n"
"                       changes, periodically copy all of X display fb into main\n"
"                       memory and examine that copy for changes.  Under some\n"
"                       circumstances this will improve interactive response,\n"
"                       or at least make things look smoother, but in others\n"
"                       (many) it will make the response worse.  If the video\n"
"                       h/w fb is such that reading small tiles is very slow\n"
"                       this mode could help.  To keep the \"framerate\" up\n"
"                       the screen size x bpp cannot be too large.  Note that\n"
"                       this mode is very wasteful of memory I/O resources\n"
"                       (it makes full screen copies even if nothing changes).\n"
"                       It may be of use in video capture-like applications,\n"
"                       or where window tearing is a problem.\n"
"\n"
"-rawfb string          Experimental option, instead of polling X, poll the\n"
"                       memory object specified in \"string\".  For shared\n"
"                       memory segments it is of the form: \"shm:N@WxHxB\"\n"
"                       which specifies a shmid N and framebuffer Width, Height,\n"
"                       and Bits per pixel.  To memory map mmap(2) a file use:\n"
"                       \"map:/path/to/a/file@WxHxB\".  If there is trouble\n"
"                       with mmap, use  \"file:/...\" for slower lseek(2)\n"
"                       based reading.  If you do not supply a type \"map\"\n"
"                       is assumed if the file exists.\n"
"\n"
"                       If string is \"setup:cmd\", then the command \"cmd\"\n"
"                       is run and the first line from it is read and used\n"
"                       as \"string\".  This allows initializing the device,\n"
"                       determining WxHxB, etc. These are often done as root\n"
"                       so take care.\n"
"\n"
"                       Optional suffixes are \":R/G/B\" and \"+O\" to specify\n"
"                       red, green, and blue masks and an offset into the\n"
"                       memory object.  If the masks are not provided x11vnc\n"
"                       guesses them based on the bpp.\n"
"\n"
"                       Examples:\n"
"                           -rawfb shm:210337933@800x600x32:ff/ff00/ff0000\n"
"                           -rawfb map:/dev/fb0@1024x768x32\n"
"                           -rawfb map:/tmp/Xvfb_screen0@640x480x8+3232\n"
"                           -rawfb file:/tmp/my.pnm@250x200x24+37\n"
"\n"
"                       (see ipcs(1) and fbset(1) for the first two examples)\n"
"\n"
"                       All user input is discarded.  Most of the X11 (screen,\n"
"                       keyboard, mouse) options do not make sense and many\n"
"                       will cause this mode to crash, so please think twice\n"
"                       before setting/changing them.\n"
"\n"
"                       If you don't want x11vnc to close the X DISPLAY in\n"
"                       rawfb mode, then capitalize the prefix, SHM:, MAP:,\n"
"                       FILE:   Keeping the display open enables the default\n"
"                       remote-control channel, which could be useful.  Also,\n"
"                       if you also specify -noviewonly, then the mouse and\n"
"                       keyboard input are still sent to the X display, this\n"
"                       usage should be very rare, i.e. doing something strange\n"
"                       with /dev/fb0.\n"
"\n"
"-pipeinput cmd         Another experimental option: it lets you supply an\n"
"                       external command in \"cmd\" that x11vnc will pipe\n"
"                       all of the user input events to in a simple format.\n"
"                       In -pipeinput mode by default x11vnc will not process\n"
"                       any of the user input events.  If you prefix \"cmd\"\n"
"                       with \"tee:\" it will both send them to the pipe\n"
"                       command and process them.  For a description of the\n"
"                       format run \"-pipeinput tee:/bin/cat\".  Another prefix\n"
"                       is \"reopen\" which means to reopen pipe if it exits.\n"
"                       Separate multiple prefixes with commas.\n"
"\n"
"                       In combination with -rawfb one might be able to\n"
"                       do amusing things (e.g. control non-X devices).\n"
"                       To facilitate this, if -rawfb is in effect then the\n"
"                       value is stored in X11VNC_RAWFB_STR for the pipe command\n"
"                       to use if it wants. Do 'env | grep X11VNC' for more.\n"
"\n"
"-gui [gui-opts]        Start up a simple tcl/tk gui based on the the remote\n"
"                       control options -remote/-query described below.\n"
"                       Requires the \"wish\" program to be installed on the\n"
"                       machine.  \"gui-opts\" is not required: the default is\n"
"                       to start up both the gui and x11vnc with the gui showing\n"
"                       up on the X display in the environment variable DISPLAY.\n"
"\n"
"                       \"gui-opts\" can be a comma separated list of items.\n"
"                       Currently there are these types of items: 1) a gui mode,\n"
"                       a 2) gui \"simplicity\", and 3) the X display the gui\n"
"                       should display on.\n"
"\n"
"                       1) The gui mode can be \"start\", \"conn\", or \"wait\"\n"
"                       \"start\" is the default mode above and is not required.\n"
"                       \"conn\" means do not automatically start up x11vnc,\n"
"                       but instead just try to connect to an existing x11vnc\n"
"                       process.  \"wait\" means just start the gui and nothing\n"
"                       else (you will later instruct the gui to start x11vnc\n"
"                       or connect to an existing one.)\n"
"\n"
"                       2) The gui simplicity is off by default (a power-user\n"
"                       gui with all options is presented) To start with\n"
"                       something less daunting supply the string \"simple\"\n"
"                       (\"ez\" is an alias for this).  Once the gui is\n"
"                       started you can toggle between the two with \"Misc ->\n"
"                       simple_gui\".\n"
"\n"
"                       3) Note the possible confusion regarding the potentially\n"
"                       two different X displays: x11vnc polls one, but you\n"
"                       may want the gui to appear on another.  For example, if\n"
"                       you ssh in and x11vnc is not running yet you may want\n"
"                       the gui to come back to you via your ssh redirected X\n"
"                       display (e.g. localhost:10).\n"
"\n"
"                       Examples: \"x11vnc -gui\", \"x11vnc -gui ez\"\n"
"                       \"x11vnc -gui localhost:10\", \"x11vnc -gui conn,host:0\"\n"
"\n"
"                       If you do not specify a gui X display in \"gui-opts\"\n"
"                       then the DISPLAY environment variable and -display\n"
"                       option are tried (in that order).  Regarding the x11vnc\n"
"                       X display the gui will try to connect to, it first\n"
"                       tries -display and then DISPLAY.  For example, \"x11vnc\n"
"                       -display :0 -gui otherhost:0\", will remote control an\n"
"                       x11vnc polling :0 and display the gui on otherhost:0\n"
"\n"
"                       If you do not intend to start x11vnc from the gui\n"
"                       (i.e. just remote control an existing one), then the\n"
"                       gui process can run on a different machine from the\n"
"                       x11vnc server as long as X permissions, etc. permit\n"
"                       communication between the two.\n"
"\n"
"-remote command        Remotely control some aspects of an already running\n"
"                       x11vnc server.  \"-R\" and \"-r\" are aliases for\n"
"                       \"-remote\".  After the remote control command is\n"
"                       sent to the running server the 'x11vnc -remote ...'\n"
"                       command exits.  You can often use the -query command\n"
"                       (see below) to see if the x11vnc server processed your\n"
"                       -remote command.\n"
"\n"
"                       The default communication channel is that of X\n"
"                       properties (specifically VNC_CONNECT), and so this\n"
"                       command must be run with correct settings for DISPLAY\n"
"                       and possibly XAUTHORITY to connect to the X server\n"
"                       and set the property.  Alternatively, use the -display\n"
"                       and -auth options to set them to the correct values.\n"
"                       The running server cannot use the -novncconnect option\n"
"                       because that disables the communication channel.\n"
"                       See below for alternate channels.\n"
"\n"
"                       For example: 'x11vnc -remote stop' (which is the same as\n"
"                       'x11vnc -R stop') will close down the x11vnc server.\n"
"                       'x11vnc -R shared' will enable shared connections, and\n"
"                       'x11vnc -R scale:3/4' will rescale the desktop.\n"
"\n"
"                       Note: the more drastic the change induced by the -remote\n"
"                       command, the bigger the chance for bugs or crashes.\n"
"                       Please report reproducible bugs.\n"
"\n"
"                       The following -remote/-R commands are supported:\n"
"\n"
"                       stop            terminate the server, same as \"quit\"\n"
"                                       \"exit\" or \"shutdown\".\n"
"                       ping            see if the x11vnc server responds.\n"
"                                       Return is: ans=ping:<xdisplay>\n"
"                       blacken         try to push a black fb update to all\n"
"                                       clients (due to timings a client\n"
"                                       could miss it). Same as \"zero\", also\n"
"                                       \"zero:x1,y1,x2,y2\" for a rectangle.\n"
"                       refresh         send the entire fb to all clients.\n"
"                       reset           recreate the fb, polling memory, etc.\n"
/* ext. cmd. */
"                       id:windowid     set -id window to \"windowid\". empty\n"
"                                       or \"root\" to go back to root window\n"
"                       sid:windowid    set -sid window to \"windowid\"\n"
"                       waitmapped      wait until subwin is mapped.\n"
"                       nowaitmapped    do not wait until subwin is mapped.\n"
"                       clip:WxH+X+Y    set -clip mode to \"WxH+X+Y\"\n"
"                       flashcmap       enable  -flashcmap mode.\n"
"                       noflashcmap     disable -flashcmap mode.\n"
"                       shiftcmap:n     set -shiftcmap to n.\n"
"                       notruecolor     enable  -notruecolor mode.\n"
"                       truecolor       disable -notruecolor mode.\n"
"                       overlay         enable  -overlay mode (if applicable).\n"
"                       nooverlay       disable -overlay mode.\n"
"                       overlay_cursor  in -overlay mode, enable cursor drawing.\n"
"                       overlay_nocursor disable cursor drawing. same as\n"
"                                        nooverlay_cursor.\n"
"                       visual:vis      set -visual to \"vis\"\n"
"                       scale:frac      set -scale to \"frac\"\n"
"                       scale_cursor:f  set -scale_cursor to \"f\"\n"
"                       viewonly        enable  -viewonly mode.\n"
/* access view,share,forever */
"                       noviewonly      disable -viewonly mode.\n"
"                       shared          enable  -shared mode.\n"
"                       noshared        disable -shared mode.\n"
"                       forever         enable  -forever mode.\n"
"                       noforever       disable -forever mode.\n"
"                       timeout:n       reset -timeout to n, if there are\n"
"                                       currently no clients, exit unless one\n"
"                                       connects in the next n secs.\n"
/* access */
"                       http            enable  http client connections.\n"
"                       nohttp          disable http client connections.\n"
"                       deny            deny any new connections, same as \"lock\"\n"
"                       nodeny          allow new connections, same as \"unlock\"\n"
/* access, filename */
"                       connect:host    do reverse connection to host, \"host\"\n"
"                                       may be a comma separated list of hosts\n"
"                                       or host:ports.  See -connect.\n"
"                       disconnect:host disconnect any clients from \"host\"\n"
"                                       same as \"close:host\".  Use host\n"
"                                       \"all\" to close all current clients.\n"
"                                       If you know the client internal hex ID,\n"
"                                       e.g. 0x3 (returned by \"-query clients\"\n"
"                                       and RFB_CLIENT_ID) you can use that too.\n"
/* access */
"                       allowonce:host  For the next connection only, allow\n"
"                                       connection from \"host\".\n"
/* access */
"                       allow:hostlist  set -allow list to (comma separated)\n"
"                                       \"hostlist\". See -allow and -localhost.\n"
"                                       Do not use with -allow /path/to/file\n"
"                                       Use \"+host\" to add a single host, and\n"
"                                       use \"-host\" to delete a single host\n"
"                       localhost       enable  -localhost mode\n"
"                       nolocalhost     disable -localhost mode\n"
"                       listen:str      set -listen to str, empty to disable.\n"
"                       nolookup        enable  -nolookup mode.\n"
"                       lookup          disable -nolookup mode.\n"
"                       input:str       set -input to \"str\", empty to disable.\n"
"                       client_input:str set the K, M, B -input on a per-client\n"
"                                       basis.  select which client as for\n"
"                                       disconnect, e.g. client_input:host:MB\n"
"                                       or client_input:0x2:K\n"
/* ext. cmd. */
"                       accept:cmd      set -accept \"cmd\" (empty to disable).\n"
"                       gone:cmd        set -gone \"cmd\" (empty to disable).\n"
"                       noshm           enable  -noshm mode.\n"
"                       shm             disable -noshm mode (i.e. use shm).\n"
"                       flipbyteorder   enable -flipbyteorder mode, you may need\n"
"                                       to set noshm for this to do something.\n"
"                       noflipbyteorder disable -flipbyteorder mode.\n"
"                       onetile         enable  -onetile mode. (you may need to\n"
"                                       set shm for this to do something)\n"
"                       noonetile       disable -onetile mode.\n"
/* ext. cmd. */
"                       solid           enable  -solid mode\n"
"                       nosolid         disable -solid mode.\n"
"                       solid_color:color set -solid color (and apply it).\n"
"                       blackout:str    set -blackout \"str\" (empty to disable).\n"
"                                       See -blackout for the form of \"str\"\n"
"                                       (basically: WxH+X+Y,...)\n"
"                                       Use \"+WxH+X+Y\" to append a single\n"
"                                       rectangle use \"-WxH+X+Y\" to delete one\n"
"                       xinerama        enable  -xinerama mode. (if applicable)\n"
"                       noxinerama      disable -xinerama mode.\n"
"                       xtrap           enable  -xtrap input mode.\n"
"                       noxtrap         disable -xtrap input mode.\n"
"                       xrandr          enable  -xrandr mode. (if applicable)\n"
"                       noxrandr        disable -xrandr mode.\n"
"                       xrandr_mode:mode set the -xrandr mode to \"mode\".\n"
"                       padgeom:WxH     set -padgeom to WxH (empty to disable)\n"
"                                       If WxH is \"force\" or \"do\" the padded\n"
"                                       geometry fb is immediately applied.\n"
"                       quiet           enable  -quiet mode.\n"
"                       noquiet         disable -quiet mode.\n"
"                       modtweak        enable  -modtweak mode.\n"
"                       nomodtweak      enable  -nomodtweak mode.\n"
"                       xkb             enable  -xkb modtweak mode.\n"
"                       noxkb           disable -xkb modtweak mode.\n"
"                       skip_keycodes:str enable -xkb -skip_keycodes \"str\".\n"
"                       skip_dups       enable  -skip_dups mode.\n"
"                       noskip_dups     disable -skip_dups mode.\n"
"                       add_keysyms     enable -add_keysyms mode.\n"
"                       noadd_keysyms   stop adding keysyms. those added will\n"
"                                       still be removed at exit.\n"
"                       clear_mods      enable  -clear_mods mode and clear them.\n"
"                       noclear_mods    disable -clear_mods mode.\n"
"                       clear_keys      enable  -clear_keys mode and clear them.\n"
"                       noclear_keys    disable -clear_keys mode.\n"
/* filename */
"                       remap:str       set -remap \"str\" (empty to disable).\n"
"                                       See -remap for the form of \"str\"\n"
"                                       (basically: key1-key2,key3-key4,...)\n"
"                                       Use \"+key1-key2\" to append a single\n"
"                                       keymapping, use \"-key1-key2\" to delete.\n"
"                       norepeat        enable  -norepeat mode.\n"
"                       repeat          disable -norepeat mode.\n"
"                       nofb            enable  -nofb mode.\n"
"                       fb              disable -nofb mode.\n"
"                       bell            enable  bell (if supported).\n"
"                       nobell          disable bell.\n"
"                       nosel           enable  -nosel mode.\n"
"                       sel             disable -nosel mode.\n"
"                       noprimary       enable  -noprimary mode.\n"
"                       primary         disable -noprimary mode.\n"
"                       cursor:mode     enable  -cursor \"mode\".\n"
"                       show_cursor     enable  showing a cursor.\n"
"                       noshow_cursor   disable showing a cursor. (same as\n"
"                                       \"nocursor\")\n"
"                       arrow:n         set -arrow to alternate n.\n"
"                       xfixes          enable  xfixes cursor shape mode.\n"
"                       noxfixes        disable xfixes cursor shape mode.\n"
"                       alphacut:n      set -alphacut to n.\n"
"                       alphafrac:f     set -alphafrac to f.\n"
"                       alpharemove     enable  -alpharemove mode.\n"
"                       noalpharemove   disable -alpharemove mode.\n"
"                       alphablend      disable -noalphablend mode.\n"
"                       noalphablend    enable  -noalphablend mode.\n"
"                       cursorshape     disable -nocursorshape mode.\n"
"                       nocursorshape   enable  -nocursorshape mode.\n"
"                       cursorpos       disable -nocursorpos mode.\n"
"                       nocursorpos     enable  -nocursorpos mode.\n"
"                       xwarp           enable  -xwarppointer mode.\n"
"                       noxwarp         disable -xwarppointer mode.\n"
"                       buttonmap:str   set -buttonmap \"str\", empty to disable\n"
"                       dragging        disable -nodragging mode.\n"
"                       nodragging      enable  -nodragging mode.\n"
"                       wireframe       enable  -wireframe mode. same as \"wf\"\n"
"                       nowireframe     disable -wireframe mode. same as \"nowf\"\n"
"                       wireframe:str   enable  -wireframe mode string.\n"
"                       wireframe_mode:str enable  -wireframe mode string.\n"
"                       wirecopyrect:str set -wirecopyrect string. same as \"wcr:\"\n"
"                       scrollcopyrect:str set -scrollcopyrect string. same \"scr\"\n"
"                       noscrollcopyrect disable -scrollcopyrect mode. \"noscr\"\n"
"                       scr_area:n      set -scr_area to n\n"
"                       scr_skip:list   set -scr_skip to \"list\"\n"
"                       scr_inc:list    set -scr_inc to \"list\"\n"
"                       scr_keys:list   set -scr_keys to \"list\"\n"
"                       scr_term:list   set -scr_term to \"list\"\n"
"                       scr_keyrepeat:str set -scr_keyrepeat to \"str\"\n"
"                       scr_parms:str   set -scr_parms parameters.\n"
"                       noxrecord       disable all use of RECORD extension.\n"
"                       xrecord         enable  use of RECORD extension.\n"
"                       pointer_mode:n  set -pointer_mode to n. same as \"pm\"\n"
"                       input_skip:n    set -input_skip to n.\n"
"                       speeds:str      set -speeds to str.\n"
"                       debug_pointer   enable  -debug_pointer, same as \"dp\"\n"
"                       nodebug_pointer disable -debug_pointer, same as \"nodp\"\n"
"                       debug_keyboard   enable  -debug_keyboard, same as \"dk\"\n"
"                       nodebug_keyboard disable -debug_keyboard, same as \"nodk\"\n"
"                       defer:n         set -defer to n ms,same as deferupdate:n\n"
"                       wait:n          set -wait to n ms.\n"
"                       wait_ui:f       set -wait_ui factor to f.\n"
"                       wait_bog        disable -nowait_bog mode.\n"
"                       nowait_bog      enable  -nowait_bog mode.\n"
"                       readtimeout:n   set read timeout to n seconds.\n"
"                       nap             enable  -nap mode.\n"
"                       nonap           disable -nap mode.\n"
"                       sb:n            set -sb to n s, same as screen_blank:n\n"
"                       xdamage         enable  xdamage polling hints.\n"
"                       noxdamage       disable xdamage polling hints.\n"
"                       xd_area:A       set -xd_area max pixel area to \"A\"\n"
"                       xd_mem:f        set -xd_mem remembrance to \"f\"\n"
"                       fs:frac         set -fs fraction to \"frac\", e.g. 0.5\n"
"                       gaps:n          set -gaps to n.\n"
"                       grow:n          set -grow to n.\n"
"                       fuzz:n          set -fuzz to n.\n"
"                       snapfb          enable  -snapfb mode.\n"
"                       nosnapfb        disable -snapfb mode.\n"
"                       rawfb:str       set -rawfb mode to \"str\".\n"
"                       progressive:n   set libvncserver -progressive slice\n"
"                                       height parameter to n.\n"
"                       desktop:str     set -desktop name to str for new clients.\n"
"                       rfbport:n       set -rfbport to n.\n"
/* access */
"                       httpport:n      set -httpport to n.\n"
"                       httpdir:dir     set -httpdir to dir (and enable http).\n"
"                       enablehttpproxy   enable  -enablehttpproxy mode.\n"
"                       noenablehttpproxy disable -enablehttpproxy mode.\n"
"                       alwaysshared     enable  -alwaysshared mode.\n"
"                       noalwaysshared   disable -alwaysshared mode.\n"
"                                        (may interfere with other options)\n"
"                       nevershared      enable  -nevershared mode.\n"
"                       nonevershared    disable -nevershared mode.\n"
"                                        (may interfere with other options)\n"
"                       dontdisconnect   enable  -dontdisconnect mode.\n"
"                       nodontdisconnect disable -dontdisconnect mode.\n"
"                                        (may interfere with other options)\n"
"                       debug_xevents   enable  debugging X events.\n"
"                       nodebug_xevents disable debugging X events.\n"
"                       debug_xdamage   enable  debugging X DAMAGE mechanism.\n"
"                       nodebug_xdamage disable debugging X DAMAGE mechanism.\n"
"                       debug_wireframe enable   debugging wireframe mechanism.\n"
"                       nodebug_wireframe disable debugging wireframe mechanism.\n"
"                       debug_scroll    enable  debugging scrollcopy mechanism.\n"
"                       nodebug_scroll  disable debugging scrollcopy mechanism.\n"
"                       debug_tiles     enable  -debug_tiles\n"
"                       nodebug_tiles   disable -debug_tiles\n"
"                       dbg             enable  -dbg crash shell\n"
"                       nodbg           disable -dbg crash shell\n"
"\n"
"                       noremote        disable the -remote command processing,\n"
"                                       it cannot be turned back on.\n"
"\n"
"                       The vncconnect(1) command from standard VNC\n"
"                       distributions may also be used if string is prefixed\n"
"                       with \"cmd=\" E.g. 'vncconnect cmd=stop'.  Under some\n"
"                       circumstances xprop(1) can used if it supports -set\n"
"                       (see the FAQ).\n"
"\n"
"                       If \"-connect /path/to/file\" has been supplied to the\n"
"                       running x11vnc server then that file can be used as a\n"
"                       communication channel (this is the only way to remote\n"
"                       control one of many x11vnc's polling the same X display)\n"
"                       Simply run: 'x11vnc -connect /path/to/file -remote ...'\n"
"                       or you can directly write to the file via something\n"
"                       like: \"echo cmd=stop > /path/to/file\", etc.\n"
"\n"
"-query variable        Like -remote, except just query the value of\n"
"                       \"variable\".  \"-Q\" is an alias for \"-query\".\n"
"                       Multiple queries can be done by separating variables\n"
"                       by commas, e.g. -query var1,var2. The results come\n"
"                       back in the form ans=var1:value1,ans=var2:value2,...\n"
"                       to the standard output.  If a variable is read-only,\n"
"                       it comes back with prefix \"aro=\" instead of \"ans=\".\n"
"\n"
"                       Some -remote commands are pure actions that do not make\n"
"                       sense as variables, e.g. \"stop\" or \"disconnect\",\n"
"                       in these cases the value returned is \"N/A\".  To direct\n"
"                       a query straight to the VNC_CONNECT property or connect\n"
"                       file use \"qry=...\" instead of \"cmd=...\"\n"
"\n"
"                       Here is the current list of \"variables\" that can\n"
"                       be supplied to the -query command. This includes the\n"
"                       \"N/A\" ones that return no useful info.  For variables\n"
"                       names that do not correspond to an x11vnc option or\n"
"                       remote command, we hope the name makes it obvious what\n"
"                       the returned value corresponds to (hint: the ext_*\n"
"                       variables correspond to the presence of X extensions):\n"
"\n"
"                       ans= stop quit exit shutdown ping blacken zero\n"
"                       refresh reset close disconnect id sid waitmapped\n"
"                       nowaitmapped clip flashcmap noflashcmap shiftcmap\n"
"                       truecolor notruecolor overlay nooverlay overlay_cursor\n"
"                       overlay_yescursor nooverlay_nocursor nooverlay_cursor\n"
"                       nooverlay_yescursor overlay_nocursor visual scale\n"
"                       scale_cursor viewonly noviewonly shared noshared\n"
"                       forever noforever once timeout deny lock nodeny unlock\n"
"                       connect allowonce allow localhost nolocalhost listen\n"
"                       lookup nolookup accept gone shm noshm flipbyteorder\n"
"                       noflipbyteorder onetile noonetile solid_color solid\n"
"                       nosolid blackout xinerama noxinerama xtrap noxtrap\n"
"                       xrandr noxrandr xrandr_mode padgeom quiet q noquiet\n"
"                       modtweak nomodtweak xkb noxkb skip_keycodes skip_dups\n"
"                       noskip_dups add_keysyms noadd_keysyms clear_mods\n"
"                       noclear_mods clear_keys noclear_keys remap repeat\n"
"                       norepeat fb nofb bell nobell sel nosel primary noprimary\n"
"                       cursorshape nocursorshape cursorpos nocursorpos cursor\n"
"                       show_cursor noshow_cursor nocursor arrow xfixes\n"
"                       noxfixes xdamage noxdamage xd_area xd_mem alphacut\n"
"                       alphafrac alpharemove noalpharemove alphablend\n"
"                       noalphablend xwarppointer xwarp noxwarppointer\n"
"                       noxwarp buttonmap dragging nodragging wireframe_mode\n"
"                       wireframe wf nowireframe nowf wirecopyrect wcr\n"
"                       nowirecopyrect nowcr scr_area scr_skip scr_inc scr_keys\n"
"                       scr_term scr_keyrepeat scr_parms scrollcopyrect scr\n"
"                       noscrollcopyrect noscr noxrecord xrecord pointer_mode\n"
"                       pm input_skip input client_input speeds debug_pointer dp\n"
"                       nodebug_pointer nodp debug_keyboard dk nodebug_keyboard\n"
"                       nodk deferupdate defer wait_ui wait_bog nowait_bog wait\n"
"                       readtimeout nap nonap sb screen_blank fs gaps grow fuzz\n"
"                       snapfb nosnapfb rawfb progressive rfbport http nohttp\n"
"                       httpport httpdir enablehttpproxy noenablehttpproxy\n"
"                       alwaysshared noalwaysshared nevershared noalwaysshared\n"
"                       dontdisconnect nodontdisconnect desktop debug_xevents\n"
"                       nodebug_xevents debug_xevents debug_xdamage\n"
"                       nodebug_xdamage debug_xdamage debug_wireframe\n"
"                       nodebug_wireframe debug_wireframe debug_scroll\n"
"                       nodebug_scroll debug_scroll debug_tiles dbt\n"
"                       nodebug_tiles nodbt debug_tiles dbg nodbg noremote\n"
"\n"
"                       aro=  display vncdisplay desktopname http_url auth\n"
"                       users rootshift clipshift scale_str scaled_x scaled_y\n"
"                       scale_numer scale_denom scale_fac scaling_blend\n"
"                       scaling_nomult4 scaling_pad scaling_interpolate inetd\n"
"                       privremote unsafe safer nocmds passwdfile using_shm\n"
"                       logfile o flag rc norc h help V version lastmod bg\n"
"                       sigpipe threads readrate netrate netlatency pipeinput\n"
"                       clients client_count pid ext_xtest ext_xtrap ext_xrecord\n"
"                       ext_xkb ext_xshm ext_xinerama ext_overlay ext_xfixes\n"
"                       ext_xdamage ext_xrandr rootwin num_buttons button_mask\n"
"                       mouse_x mouse_y bpp depth indexed_color dpy_x dpy_y\n"
"                       wdpy_x wdpy_y off_x off_y cdpy_x cdpy_y coff_x coff_y\n"
"                       rfbauth passwd\n"
"\n"
"-sync                  By default -remote commands are run asynchronously, that\n"
"                       is, the request is posted and the program immediately\n"
"                       exits.  Use -sync to have the program wait for an\n"
"                       acknowledgement from the x11vnc server that command was\n"
"                       processed (somehow).  On the other hand -query requests\n"
"                       are always processed synchronously because they have\n"
"                       to wait for the result.\n"
"\n"
"                       Also note that if both -remote and -query requests are\n"
"                       supplied on the command line, the -remote is processed\n"
"                       first (synchronously: no need for -sync), and then\n"
"                       the -query request is processed in the normal way.\n"
"                       This allows for a reliable way to see if the -remote\n"
"                       command was processed by querying for any new settings.\n"
"                       Note however that there is timeout of a few seconds so\n"
"                       if the x11vnc takes longer than that to process the\n"
"                       requests the requestor will think that a failure has\n"
"                       taken place.\n"
"\n"
"-noremote              Do not process any remote control commands or queries.\n"
"-yesremote             Do process remote control commands or queries.\n"
"                       Default: %s\n"
"\n"
"                       A note about security wrt remote control commands.\n"
"                       If someone can connect to the X display and change\n"
"                       the property VNC_CONNECT, then they can remotely\n"
"                       control x11vnc.  Normally access to the X display is\n"
"                       protected.  Note that if they can modify VNC_CONNECT\n"
"                       on the X server, they have enough permissions to also\n"
"                       run their own x11vnc and thus have complete control\n"
"                       of the desktop.  If the  \"-connect /path/to/file\"\n"
"                       channel is being used, obviously anyone who can write\n"
"                       to /path/to/file can remotely control x11vnc.  So be\n"
"                       sure to protect the X display and that file's write\n"
"                       permissions.  See -privremote below.\n"
"\n"
"                       If you are paranoid and do not think -noremote is\n"
"                       enough, to disable the VNC_CONNECT property channel\n"
"                       completely use -novncconnect, or use the -safer\n"
"                       option that shuts many things off.\n"
"\n"
"-unsafe                A few remote commands are disabled by default\n"
"                       (currently: id:pick, accept:<cmd>, gone:<cmd>, and\n"
"                       rawfb:setup:<cmd>) because they are associated with\n"
"                       running external programs.  If you specify -unsafe, then\n"
"                       these remote-control commands are allowed.  Note that\n"
"                       you can still specify these parameters on the command\n"
"                       line, they just cannot be changed via remote-control.\n"
"-safer                 Equivalent to: -novncconnect -noremote and prohibiting\n"
"                       -gui and the -connect file. Shuts off communcation\n"
"                       channels.\n"
"-privremote            Perform some sanity checks and disable remote-control\n"
"                       commands if it appears that the X DISPLAY and/or\n"
"                       connectfile can be accessed by other users.  Once\n"
"                       remote-control is disabled it cannot be turned back on.\n"
"-nocmds                No external commands (e.g. system(3), popen(3), exec(3))\n"
"                       will be run.\n"
"\n"
"-deny_all              For use with -remote nodeny: start out denying all\n"
"                       incoming clients until \"-remote nodeny\" is used to\n"
"                       let them in.\n"
"%s\n"
"\n"
"These options are passed to libvncserver:\n"
"\n"
;
	/* have both our help and rfbUsage to stdout for more(1), etc. */
	dup2(1, 2);

	if (mode == 1) {
		char *p;	
		int l = 0;
		fprintf(stderr, "x11vnc: allow VNC connections to real "
		    "X11 displays. %s\n\nx11vnc options:\n", lastmod);
		p = strtok(help, "\n");
		while (p) {
			int w = 23;
			char tmp[100];
			if (p[0] == '-') {
				strncpy(tmp, p, w);
				fprintf(stderr, "  %s", tmp);
				l++;
				if (l % 2 == 0) {
					fprintf(stderr, "\n");
				}
			}
			p = strtok(NULL, "\n");
		}
		fprintf(stderr, "\n\nlibvncserver options:\n");
		rfbUsage();
		fprintf(stderr, "\n");
		exit(1);
	}
	fprintf(stderr, help, lastmod,
		view_only ? "on":"off",
		shared ? "on":"off",
		vnc_connect ? "-vncconnect":"-novncconnect",
		use_modifier_tweak ? "-modtweak":"-nomodtweak",
		skip_duplicate_key_events ? "-skip_dups":"-noskip_dups",
		add_keysyms ? "-add_keysyms":"-noadd_keysyms",
		no_autorepeat ? "-norepeat":"-repeat",
		alt_arrow_max, alt_arrow,
		alpha_threshold,
		alpha_frac,
		cursor_pos_updates ? "-cursorpos":"-nocursorpos",
		wireframe ? "-wireframe":"-nowireframe",
		WIREFRAME_PARMS,
		wireframe_copyrect_default,
		scroll_copyrect_default,
		scrollcopyrect_min_area,
		scroll_skip_str0 ? scroll_skip_str0 : "(empty)",
		scroll_term_str0,
		max_keyrepeat_str0,
		SCROLL_COPYRECT_PARMS,
		pointer_mode_max, pointer_mode,
		ui_skip,
		defer_update,
		waitms,
		wait_ui,
		rfbMaxClientWait/1000,
		take_naps ? "take naps":"no naps",
		screen_blank,
		xdamage_max_area, NSCAN, xdamage_memory,
		use_threads ? "-threads":"-nothreads",
		fs_frac,
		gaps_fill,
		grow_fill,
		tile_fuzz,
		accept_remote_cmds ? "-yesremote":"-noremote",
		""
	);

	rfbUsage();
#endif
	exit(1);
}

void set_vnc_desktop_name(void) {
	int sz = 256;
	sprintf(vnc_desktop_name, "unknown");
	if (inetd) {
		sprintf(vnc_desktop_name, "inetd-no-further-clients");
	}
	if (screen->port) {
		char *host = this_host();
		int lport = screen->port;
		char *iface = listen_str;

		if (iface != NULL && *iface != '\0' && strcmp(iface, "any")) {
			host = iface;
		}
		if (host != NULL) {
			/* note that vncviewer special cases 5900-5999 */
			if (inetd) {
				;	/* should not occur (port) */
			} else if (quiet) {
				if (lport >= 5900) {
					snprintf(vnc_desktop_name, sz, "%s:%d",
					    host, lport - 5900);
					fprintf(stderr, "The VNC desktop is "
					    "%s\n", vnc_desktop_name);
				} else {
					snprintf(vnc_desktop_name, sz, "%s:%d",
					    host, lport);
					fprintf(stderr, "The VNC desktop is "
					    "%s\n", vnc_desktop_name);
				}
			} else if (lport >= 5900) {
				snprintf(vnc_desktop_name, sz, "%s:%d",
				    host, lport - 5900);
				rfbLog("\n");
				rfbLog("The VNC desktop is %s\n",
				    vnc_desktop_name);
				if (lport >= 6000) {
					rfbLog("possible aliases:  %s:%d, "
					    "%s::%d\n", host, lport,
					    host, lport);
				}
			} else {
				snprintf(vnc_desktop_name, sz, "%s:%d",
				    host, lport);
				rfbLog("\n");
				rfbLog("The VNC desktop is %s\n",
				    vnc_desktop_name);
				rfbLog("possible alias:    %s::%d\n",
				    host, lport);
			}
		}
		fflush(stderr);	
		if (inetd) {
			;	/* should not occur (port != 0) */
		} else {
			fprintf(stdout, "PORT=%d\n", screen->port);
			fflush(stdout);	
			if (flagfile) {
				FILE *flag = fopen(flagfile, "w");
				if (flag) {
					fprintf(flag, "PORT=%d\n",screen->port);
					fflush(flag);	
					fclose(flag);
				} else {
					rfbLog("could not open flag file: %s\n",
					    flagfile);
				}
			}
		}
		fflush(stdout);	
	}
}

/*
 * utility to get the current host name
 */
#define MAXN 256

char *this_host(void) {
	char host[MAXN];
#if LIBVNCSERVER_HAVE_GETHOSTNAME
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
	if (subwin && valid_window(subwin, NULL, 0)) {
		char *name;
		if (XFetchName(dpy, subwin, &name)) {
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
	int limit = 0;

	if (UT.sysname == NULL) {
		return 0;
	}
	if (!strcmp(UT.sysname, "SunOS")) {
		char *r = UT.release;
		if (*r == '5' && *(r+1) == '.') {
			if (strchr("2345678", *(r+2)) != NULL) {
				limit = 1;
			}
		}
	} else if (!strcmp(UT.sysname, "Darwin")) {
		limit = 1;
	}
	if (limit && ! quiet) {
		fprintf(stderr, "reducing shm usage on %s %s (adding "
		    "-onetile)\n", UT.sysname, UT.release);
	}
	return limit;
}


/*
 * quick-n-dirty ~/.x11vncrc: each line (except # comments) is a cmdline option.
 */
static int argc2 = 0;
static char **argv2;

static void check_rcfile(int argc, char **argv) {
	int i, pwlast, norc = 0, argmax = 1024;
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
	rc_norc = norc;
	rc_rcfile = strdup("");
	if (norc) {
		;
	} else if (infile != NULL) {
		rc = fopen(infile, "r");
		rc_rcfile = strdup(infile);
		if (rc == NULL) {
			fprintf(stderr, "could not open rcfile: %s\n", infile);
			perror("fopen");
			exit(1);
		}
	} else {
		char *home = get_home_dir();
		if (! home) {
			norc = 1;
		} else {
			strncpy(rcfile, home, 500);
			free(home);

			strcat(rcfile, "/.x11vncrc");
			infile = rcfile;
			rc = fopen(rcfile, "r");
			if (rc == NULL) {
				norc = 1;
			} else {
				rc_rcfile = strdup(rcfile);
			}
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
		sz = sbuf.st_size+1;	/* allocate whole file size */
		if (sz < 1024) {
			sz = 1024;
		}

		buf = (char *) malloc(sz);

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
			p = lblanks(p);

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
			p = lblanks(p);
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
	pwlast = 0;
	for (i=1; i < argc; i++) {
		argv2[argc2++] = strdup(argv[i]);

		if (pwlast || !strcmp("-passwd", argv[i])
		    || !strcmp("-viewpasswd", argv[i])) {
			char *p = argv[i];		
			if (pwlast) {
				pwlast = 0;
			} else {
				pwlast = 1;
			}
			while (*p != '\0')
				*p++ = '\0';
		}
		if (argc2 >= argmax) {
			fprintf(stderr, "too many rcfile options\n");
			exit(1);
		}
	}
}

void immediate_switch_user(int argc, char* argv[]) {
	int i;
	for (i=1; i < argc; i++) {
		char *u;

		if (strcmp(argv[i], "-users")) {
			continue;
		}
		if (i == argc - 1) {
			fprintf(stderr, "not enough arguments for: -users\n");
			exit(1);
		}
		if (*(argv[i+1]) != '=') {
			break;
		}

		/* wants an immediate switch: =bob */
		u = strdup(argv[i+1]);
		*u = '+';
		if (strstr(u, "+guess") == u) {
			fprintf(stderr, "invalid user: %s\n", u+1);
			exit(1);
		}
		if (!switch_user(u, 0)) {
			fprintf(stderr, "Could not switch to user: %s\n", u+1);
			exit(1);
		} else {
			fprintf(stderr, "Switched to user: %s\n", u+1);
			started_as_root = 2;
		}
		free(u);
		break;
	}
}

int main(int argc, char* argv[]) {

	int i, len, tmpi;
	int ev, er, maj, min;
	char *arg;
	int remote_sync = 0;
	char *remote_cmd = NULL;
	char *query_cmd  = NULL;
	char *gui_str = NULL;
	int pw_loc = -1, got_passwd = 0, got_rfbauth = 0;
	int vpw_loc = -1;
	int dt = 0, bg = 0;
	int got_rfbwait = 0, got_deferupdate = 0, got_defer = 0;
	int got_httpdir = 0, try_http = 0;

	/* used to pass args we do not know about to rfbGetScreen(): */
	int argc_vnc = 1; char *argv_vnc[128];

	dtime0(&x11vnc_start);

	if (!getuid() || !geteuid()) {
		started_as_root = 1;

		/* check for '-users =bob' */
		immediate_switch_user(argc, argv);
	}

	argv_vnc[0] = strdup(argv[0]);
	program_name = strdup(argv[0]);
	program_pid = (int) getpid();

	solid_default = strdup(solid_default);	/* for freeing with -R */

	len = 0;
	for (i=1; i < argc; i++) {
		len += strlen(argv[i]) + 4 + 1;
	}
	program_cmdline = (char *)malloc(len+1);
	program_cmdline[0] = '\0';
	for (i=1; i < argc; i++) {
		char *s = argv[i];
		if (program_cmdline[0]) {
			strcat(program_cmdline, " ");
		}
		if (*s == '-') {
			strcat(program_cmdline, s);
		} else {
			strcat(program_cmdline, "{{");
			strcat(program_cmdline, s);
			strcat(program_cmdline, "}}");
		}
	}

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
			use_dpy = strdup(argv[++i]);
		} else if (!strcmp(arg, "-auth") || !strcmp(arg, "-xauth")) {
			CHECK_ARGC
			auth_file = strdup(argv[++i]);
		} else if (!strcmp(arg, "-id") || !strcmp(arg, "-sid")) {
			CHECK_ARGC
			if (!strcmp(arg, "-sid")) {
				rootshift = 1;
			} else {
				rootshift = 0;
			}
			i++;
			if (!strcmp("pick", argv[i])) {
				if (started_as_root) {
					fprintf(stderr, "unsafe: %s pick\n",
					    arg);
					exit(1);
				} else if (! pick_windowid(&subwin)) {
					fprintf(stderr, "bad %s pick\n", arg);
					exit(1);
				}
			} else if (! scan_hexdec(argv[i], &subwin)) {
				fprintf(stderr, "bad %s arg: %s\n", arg,
				    argv[i]);
				exit(1);
			}
		} else if (!strcmp(arg, "-waitmapped")) {
			subwin_wait_mapped = 1;
		} else if (!strcmp(arg, "-clip")) {
			CHECK_ARGC
			clip_str = strdup(argv[++i]);
		} else if (!strcmp(arg, "-flashcmap")) {
			flash_cmap = 1;
		} else if (!strcmp(arg, "-shiftcmap")) {
			CHECK_ARGC
			shift_cmap = atoi(argv[++i]);
		} else if (!strcmp(arg, "-notruecolor")) {
			force_indexed_color = 1;
		} else if (!strcmp(arg, "-overlay")) {
			overlay = 1;
		} else if (!strcmp(arg, "-overlay_nocursor")) {
			overlay = 1;
			overlay_cursor = 0;
		} else if (!strcmp(arg, "-overlay_yescursor")) {
			overlay = 1;
			overlay_cursor = 2;
		} else if (!strcmp(arg, "-visual")) {
			CHECK_ARGC
			visual_str = strdup(argv[++i]);
		} else if (!strcmp(arg, "-scale")) {
			CHECK_ARGC
			scale_str = strdup(argv[++i]);
		} else if (!strcmp(arg, "-scale_cursor")) {
			CHECK_ARGC
			scale_cursor_str = strdup(argv[++i]);
		} else if (!strcmp(arg, "-viewonly")) {
			view_only = 1;
		} else if (!strcmp(arg, "-noviewonly")) {
			view_only = 0;
			got_noviewonly = 1;
		} else if (!strcmp(arg, "-shared")) {
			shared = 1;
		} else if (!strcmp(arg, "-once")) {
			connect_once = 1;
		} else if (!strcmp(arg, "-many") || !strcmp(arg, "-forever")) {
			connect_once = 0;
		} else if (!strcmp(arg, "-timeout")) {
			CHECK_ARGC
			first_conn_timeout = atoi(argv[++i]);
		} else if (!strcmp(arg, "-users")) {
			CHECK_ARGC
			users_list = strdup(argv[++i]);
		} else if (!strcmp(arg, "-inetd")) {
			inetd = 1;
		} else if (!strcmp(arg, "-http")) {
			try_http = 1;
		} else if (!strcmp(arg, "-connect")) {
			CHECK_ARGC
			if (strchr(argv[++i], '/')) {
				client_connect_file = strdup(argv[i]);
			} else {
				client_connect = strdup(argv[i]);
			}
		} else if (!strcmp(arg, "-vncconnect")) {
			vnc_connect = 1;
		} else if (!strcmp(arg, "-novncconnect")) {
			vnc_connect = 0;
		} else if (!strcmp(arg, "-allow")) {
			CHECK_ARGC
			allow_list = strdup(argv[++i]);
		} else if (!strcmp(arg, "-localhost")) {
			allow_list = strdup("127.0.0.1");
		} else if (!strcmp(arg, "-nolookup")) {
			host_lookup = 0;
		} else if (!strcmp(arg, "-input")) {
			CHECK_ARGC
			allowed_input_str = strdup(argv[++i]);
		} else if (!strcmp(arg, "-viewpasswd")) {
			vpw_loc = i;
			CHECK_ARGC
			viewonly_passwd = strdup(argv[++i]);
		} else if (!strcmp(arg, "-passwdfile")) {
			CHECK_ARGC
			passwdfile = strdup(argv[++i]);
		} else if (!strcmp(arg, "-storepasswd")) {
			if (i+2 >= argc || rfbEncryptAndStorePasswd(argv[i+1],
			    argv[i+2]) != 0) {
				fprintf(stderr, "-storepasswd failed\n");
				exit(1);
			} else {
				fprintf(stderr, "stored passwd in file %s\n",
				    argv[i+2]);
				exit(0);
			}
		} else if (!strcmp(arg, "-accept")) {
			CHECK_ARGC
			accept_cmd = strdup(argv[++i]);
		} else if (!strcmp(arg, "-gone")) {
			CHECK_ARGC
			gone_cmd = strdup(argv[++i]);
		} else if (!strcmp(arg, "-noshm")) {
			using_shm = 0;
		} else if (!strcmp(arg, "-flipbyteorder")) {
			flip_byte_order = 1;
		} else if (!strcmp(arg, "-onetile")) {
			single_copytile = 1;
		} else if (!strcmp(arg, "-solid")) {
			use_solid_bg = 1;
			if (i < argc-1) {
				char *s = argv[i+1];
				if (s[0] != '-') {
					solid_str = strdup(s);
					i++;
				}
			}
			if (! solid_str) {
				solid_str = strdup(solid_default);
			}
		} else if (!strcmp(arg, "-blackout")) {
			CHECK_ARGC
			blackout_str = strdup(argv[++i]);
		} else if (!strcmp(arg, "-xinerama")) {
			xinerama = 1;
		} else if (!strcmp(arg, "-xtrap")) {
			xtrap_input = 1;
		} else if (!strcmp(arg, "-xrandr")) {
			xrandr = 1;
			if (i < argc-1) {
				char *s = argv[i+1];
				if (known_xrandr_mode(s)) {
					xrandr_mode = strdup(s);
					i++;
				}
			}
		} else if (!strcmp(arg, "-padgeom")
		    || !strcmp(arg, "-padgeometry")) {
			CHECK_ARGC
			pad_geometry = strdup(argv[++i]);
		} else if (!strcmp(arg, "-o") || !strcmp(arg, "-logfile")) {
			CHECK_ARGC
			logfile_append = 0;
			logfile = strdup(argv[++i]);
		} else if (!strcmp(arg, "-oa") || !strcmp(arg, "-logappend")) {
			CHECK_ARGC
			logfile_append = 1;
			logfile = strdup(argv[++i]);
		} else if (!strcmp(arg, "-flag")) {
			CHECK_ARGC
			flagfile = strdup(argv[++i]);
		} else if (!strcmp(arg, "-rc")) {
			i++;	/* done above */
		} else if (!strcmp(arg, "-norc")) {
			;	/* done above */
		} else if (!strcmp(arg, "-h") || !strcmp(arg, "-help")) {
			print_help(0);
		} else if (!strcmp(arg, "-?") || !strcmp(arg, "-opts")) {
			print_help(1);
		} else if (!strcmp(arg, "-V") || !strcmp(arg, "-version")) {
			fprintf(stdout, "x11vnc: %s\n", lastmod);
			exit(0);
		} else if (!strcmp(arg, "-dbg")) {
			crash_debug = 1;
		} else if (!strcmp(arg, "-q") || !strcmp(arg, "-quiet")) {
			quiet = 1;
		} else if (!strcmp(arg, "-bg") || !strcmp(arg, "-background")) {
#if LIBVNCSERVER_HAVE_SETSID
			bg = 1;
			opts_bg = bg;
#else
			fprintf(stderr, "warning: -bg mode not supported.\n");
#endif
		} else if (!strcmp(arg, "-modtweak")) {
			use_modifier_tweak = 1;
		} else if (!strcmp(arg, "-nomodtweak")) {
			use_modifier_tweak = 0;
			got_nomodtweak = 1;
		} else if (!strcmp(arg, "-isolevel3")) {
			use_iso_level3 = 1;
		} else if (!strcmp(arg, "-xkb")) {
			use_modifier_tweak = 1;
			use_xkb_modtweak = 1;
		} else if (!strcmp(arg, "-noxkb")) {
			use_xkb_modtweak = 0;
			got_noxkb = 1;
		} else if (!strcmp(arg, "-xkbcompat")) {
			xkbcompat = 1;
		} else if (!strcmp(arg, "-skip_keycodes")) {
			CHECK_ARGC
			skip_keycodes = strdup(argv[++i]);
		} else if (!strcmp(arg, "-skip_dups")) {
			skip_duplicate_key_events = 1;
		} else if (!strcmp(arg, "-noskip_dups")) {
			skip_duplicate_key_events = 0;
		} else if (!strcmp(arg, "-add_keysyms")) {
			add_keysyms++;
		} else if (!strcmp(arg, "-noadd_keysyms")) {
			add_keysyms = 0;
		} else if (!strcmp(arg, "-clear_mods")) {
			clear_mods = 1;
		} else if (!strcmp(arg, "-clear_keys")) {
			clear_mods = 2;
		} else if (!strcmp(arg, "-remap")) {
			CHECK_ARGC
			remap_file = strdup(argv[++i]);
		} else if (!strcmp(arg, "-norepeat")) {
			no_autorepeat = 1;
			if (i < argc-1) {
				char *s = argv[i+1];
				if (*s == '-') {
					s++;
				}
				if (isdigit(*s)) {
					no_repeat_countdown = atoi(argv[++i]);
				}
			}
		} else if (!strcmp(arg, "-repeat")) {
			no_autorepeat = 0;
		} else if (!strcmp(arg, "-nofb")) {
			nofb = 1;
		} else if (!strcmp(arg, "-nobell")) {
			watch_bell = 0;
			sound_bell = 0;
		} else if (!strcmp(arg, "-nosel")) {
			watch_selection = 0;
			watch_primary = 0;
		} else if (!strcmp(arg, "-noprimary")) {
			watch_primary = 0;
		} else if (!strcmp(arg, "-cursor")) {
			show_cursor = 1;
			if (i < argc-1) {
				char *s = argv[i+1];
				if (known_cursors_mode(s)) {
					multiple_cursors_mode = strdup(s);
					i++;
					if (!strcmp(s, "none")) {
						show_cursor = 0;
					}
				} 
			}
		} else if (!strcmp(arg, "-nocursor")) { 
			multiple_cursors_mode = strdup("none");
			show_cursor = 0;
		} else if (!strcmp(arg, "-arrow")) {
			CHECK_ARGC
			alt_arrow = atoi(argv[++i]);
		} else if (!strcmp(arg, "-xfixes")) { 
			use_xfixes = 1;
		} else if (!strcmp(arg, "-noxfixes")) { 
			use_xfixes = 0;
		} else if (!strcmp(arg, "-alphacut")) {
			CHECK_ARGC
			alpha_threshold = atoi(argv[++i]);
		} else if (!strcmp(arg, "-alphafrac")) {
			CHECK_ARGC
			alpha_frac = atof(argv[++i]);
		} else if (!strcmp(arg, "-alpharemove")) {
			alpha_remove = 1;
		} else if (!strcmp(arg, "-noalphablend")) {
			alpha_blend = 0;
		} else if (!strcmp(arg, "-nocursorshape")) {
			cursor_shape_updates = 0;
		} else if (!strcmp(arg, "-cursorpos")) {
			cursor_pos_updates = 1;
			got_cursorpos = 1;
		} else if (!strcmp(arg, "-nocursorpos")) {
			cursor_pos_updates = 0;
		} else if (!strcmp(arg, "-xwarppointer")) {
			use_xwarppointer = 1;
		} else if (!strcmp(arg, "-buttonmap")) {
			CHECK_ARGC
			pointer_remap = strdup(argv[++i]);
		} else if (!strcmp(arg, "-nodragging")) {
			show_dragging = 0;
		} else if (!strcmp(arg, "-wireframe")
		    || !strcmp(arg, "-wf")) {
			wireframe = 1;
			if (i < argc-1) {
				char *s = argv[i+1];
				if (*s != '-') {
					wireframe_str = strdup(argv[++i]);
				}
			}
		} else if (!strcmp(arg, "-nowireframe")
		    || !strcmp(arg, "-nowf")) {
			wireframe = 0;
		} else if (!strcmp(arg, "-wirecopyrect")
		    || !strcmp(arg, "-wcr")) {
			CHECK_ARGC
			set_wirecopyrect_mode(argv[++i]);
			got_wirecopyrect = 1;
		} else if (!strcmp(arg, "-nowirecopyrect")
		    || !strcmp(arg, "-nowf")) {
			set_wirecopyrect_mode("never");
		} else if (!strcmp(arg, "-debug_wireframe")
		    || !strcmp(arg, "-dwf")) {
			debug_wireframe++;
		} else if (!strcmp(arg, "-scrollcopyrect")
		    || !strcmp(arg, "-scr")) {
			CHECK_ARGC
			set_scrollcopyrect_mode(argv[++i]);
			got_scrollcopyrect = 1;
		} else if (!strcmp(arg, "-noscrollcopyrect")
		    || !strcmp(arg, "-noscr")) {
			set_scrollcopyrect_mode("never");
		} else if (!strcmp(arg, "-scr_area")) {
			int tn;
			CHECK_ARGC
			tn = atoi(argv[++i]);
			if (tn >= 0) {
				scrollcopyrect_min_area = tn;
			}
		} else if (!strcmp(arg, "-scr_skip")) {
			CHECK_ARGC
			scroll_skip_str = strdup(argv[++i]);
		} else if (!strcmp(arg, "-scr_inc")) {
			CHECK_ARGC
			scroll_good_str = strdup(argv[++i]);
		} else if (!strcmp(arg, "-scr_keys")) {
			CHECK_ARGC
			scroll_key_list_str = strdup(argv[++i]);
		} else if (!strcmp(arg, "-scr_term")) {
			CHECK_ARGC
			scroll_term_str = strdup(argv[++i]);
		} else if (!strcmp(arg, "-scr_keyrepeat")) {
			CHECK_ARGC
			max_keyrepeat_str = strdup(argv[++i]);
		} else if (!strcmp(arg, "-scr_parms")) {
			CHECK_ARGC
			scroll_copyrect_str = strdup(argv[++i]);
		} else if (!strcmp(arg, "-debug_scroll")
		    || !strcmp(arg, "-ds")) {
			debug_scroll++;
		} else if (!strcmp(arg, "-noxrecord")) {
			noxrecord = 1;
		} else if (!strcmp(arg, "-pointer_mode")
		    || !strcmp(arg, "-pm")) {
			char *p, *s;
			CHECK_ARGC
			s = argv[++i];
			if ((p = strchr(s, ':')) != NULL) {
				ui_skip = atoi(p+1);
				if (! ui_skip) ui_skip = 1;
				*p = '\0';
			}
			if (atoi(s) < 1 || atoi(s) > pointer_mode_max) {
				rfbLog("pointer_mode out of range 1-%d: %d\n",
				    pointer_mode_max, atoi(s));
			} else {
				pointer_mode = atoi(s);
				got_pointer_mode = pointer_mode;
			}
		} else if (!strcmp(arg, "-input_skip")) {
			CHECK_ARGC
			ui_skip = atoi(argv[++i]);
			if (! ui_skip) ui_skip = 1;
		} else if (!strcmp(arg, "-speeds")) {
			CHECK_ARGC
			speeds_str = strdup(argv[++i]);
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
		} else if (!strcmp(arg, "-wait_ui")) {
			CHECK_ARGC
			wait_ui = atof(argv[++i]);
		} else if (!strcmp(arg, "-nowait_bog")) {
			wait_bog = 0;
		} else if (!strcmp(arg, "-readtimeout")) {
			CHECK_ARGC
			rfbMaxClientWait = atoi(argv[++i]) * 1000;
		} else if (!strcmp(arg, "-nap")) {
			take_naps = 1;
		} else if (!strcmp(arg, "-nonap")) {
			take_naps = 0;
		} else if (!strcmp(arg, "-sb")) {
			CHECK_ARGC
			screen_blank = atoi(argv[++i]);
		} else if (!strcmp(arg, "-xdamage")) {
			use_xdamage = 1;
		} else if (!strcmp(arg, "-noxdamage")) {
			use_xdamage = 0;
		} else if (!strcmp(arg, "-xd_area")) {
			int tn;
			CHECK_ARGC
			tn = atoi(argv[++i]);
			if (tn >= 0) {
				xdamage_max_area = tn;
			}
		} else if (!strcmp(arg, "-xd_mem")) {
			double f;
			CHECK_ARGC
			f = atof(argv[++i]);
			if (f >= 0.0) {
				xdamage_memory = f;
			}
		} else if (!strcmp(arg, "-sigpipe")) {
			CHECK_ARGC
			if (known_sigpipe_mode(argv[++i])) {
				sigpipe = strdup(argv[i]);
			} else {
				fprintf(stderr, "bad -sigpipe arg: %s, must "
				    "be \"ignore\" or \"exit\"\n", argv[i]);
				exit(1);
			}
#if LIBVNCSERVER_HAVE_LIBPTHREAD
		} else if (!strcmp(arg, "-threads")) {
			use_threads = 1;
		} else if (!strcmp(arg, "-nothreads")) {
			use_threads = 0;
#endif
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
		} else if (!strcmp(arg, "-debug_tiles")
		    || !strcmp(arg, "-dbt")) {
			debug_tiles++;
		} else if (!strcmp(arg, "-snapfb")) {
			use_snapfb = 1;
		} else if (!strcmp(arg, "-rawfb")) {
			CHECK_ARGC
			raw_fb_str = strdup(argv[++i]);
		} else if (!strcmp(arg, "-pipeinput")) {
			CHECK_ARGC
			pipeinput_str = strdup(argv[++i]);
		} else if (!strcmp(arg, "-gui")) {
			launch_gui = 1;
			if (i < argc-1) {
				char *s = argv[i+1];
				if (*s != '-') {
					gui_str = strdup(s);
					i++;
				} 
			}
		} else if (!strcmp(arg, "-remote") || !strcmp(arg, "-R")
		    || !strcmp(arg, "-r") || !strcmp(arg, "-remote-control")) {
			CHECK_ARGC
			i++;
			if (!strcmp(argv[i], "ping")) {
				query_cmd = strdup(argv[i]);
			} else {
				remote_cmd = strdup(argv[i]);
			}
			quiet = 1;
			xkbcompat = 0;
		} else if (!strcmp(arg, "-query") || !strcmp(arg, "-Q")) {
			CHECK_ARGC
			query_cmd = strdup(argv[++i]);
			quiet = 1;
			xkbcompat = 0;
		} else if (!strcmp(arg, "-sync")) {
			remote_sync = 1;
		} else if (!strcmp(arg, "-noremote")) {
			accept_remote_cmds = 0;
		} else if (!strcmp(arg, "-yesremote")) {
			accept_remote_cmds = 1;
		} else if (!strcmp(arg, "-unsafe")) {
			safe_remote_only = 0;
		} else if (!strcmp(arg, "-privremote")) {
			priv_remote = 1;
		} else if (!strcmp(arg, "-safer")) {
			more_safe = 1;
		} else if (!strcmp(arg, "-nocmds")) {
			no_external_cmds = 1;
		} else if (!strcmp(arg, "-deny_all")) {
			deny_all = 1;
		} else if (!strcmp(arg, "-httpdir")) {
			CHECK_ARGC
			http_dir = strdup(argv[++i]);
			got_httpdir = 1;
		} else {
			if (!strcmp(arg, "-desktop") && i < argc-1) {
				dt = 1;
				rfb_desktop_name = strdup(argv[i+1]);
			}
			if (!strcmp(arg, "-passwd")) {
				pw_loc = i;
				got_passwd = 1;
			}
			if (!strcmp(arg, "-rfbauth")) {
				got_rfbauth = 1;
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
			if (!strcmp(arg, "-listen") && i < argc-1) {
				listen_str = strdup(argv[i+1]);
			}
			/* otherwise copy it for libvncserver use below. */
			if (argc_vnc < 100) {
				argv_vnc[argc_vnc++] = strdup(arg);
			}
		}
	}

	if (launch_gui) {
		do_gui(gui_str);
	}
	if (logfile) {
		int n;
		if (logfile_append) {
			n = open(logfile, O_WRONLY|O_CREAT|O_APPEND, 0666);
		} else {
			n = open(logfile, O_WRONLY|O_CREAT|O_TRUNC, 0666);
		}
		if (n < 0) {
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
			rfbLog("passing arg to libvncserver: %s\n", argv_vnc[i]);
			if (!strcmp(argv_vnc[i], "-passwd")) {
				i++;
			}
		}
	}

	if (client_connect_file && (remote_cmd || query_cmd)) {
		/* no need to open DISPLAY, just write it to the file now */
		int rc = do_remote_query(remote_cmd, query_cmd, remote_sync);
		fflush(stderr);
		fflush(stdout);
		exit(rc);
	}


	/*
	 * If -passwd was used, clear it out of argv.  This does not
	 * work on all UNIX, have to use execvp() in general...
	 */
	if (pw_loc > 0) {
		int i;
		for (i=pw_loc; i <= pw_loc+1; i++) {
			if (i < argc) {
				char *p = argv[i];		
				while (*p != '\0') {
					*p++ = '\0';
				}
			}
		}
	} else if (passwdfile) {
		/* read passwd from file */
		char line[1024];
		FILE *in;
		in = fopen(passwdfile, "r");
		if (in == NULL) {
			rfbLog("cannot open passwdfile: %s\n", passwdfile);
			rfbLogPerror("fopen");
			exit(1);
		}
		if (fgets(line, 1024, in) != NULL) {
			char *q;
			int len = strlen(line); 
			if (len > 0 && line[len-1] == '\n') {
				line[len-1] = '\0';
			}
			argv_vnc[argc_vnc++] = strdup("-passwd");
			got_passwd = 1;
			if (!strcmp(line, "__EMPTY__")) {
				argv_vnc[argc_vnc++] = strdup("");
			} else if ((q = strstr(line, "__ENDPASSWD__")) !=NULL) {
				*q = '\0';
				argv_vnc[argc_vnc++] = strdup(line);
			} else {
				argv_vnc[argc_vnc++] = strdup(line);
			}
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
					if (!strcmp(line, "__EMPTY__")) {
						viewonly_passwd = strdup("");
					} else if ((q = strstr(line,
					    "__ENDPASSWD__")) != NULL) {
						*q = '\0';
						viewonly_passwd = strdup(line);
					} else {
						viewonly_passwd = strdup(line);
					}
				} else {
					rfbLog("*** not setting"
					    " viewonly password to the 2nd"
					    " line of %s. (blank or other"
					    " problem)\n", passwdfile);
				}
			}
		} else {
			rfbLog("cannot read a line from passwdfile: %s\n",
			    passwdfile);
			exit(1);
		}
		fclose(in);
	}
	if (vpw_loc > 0) {
		int i;
		for (i=vpw_loc; i <= vpw_loc+1; i++) {
			if (i < argc) {
				char *p = argv[i];		
				while (*p != '\0') {
					*p++ = '\0';
				}
			}
		}
	} 
#ifdef HARDWIRE_PASSWD
	if (! got_rfbauth && ! got_passwd) {
		argv_vnc[argc_vnc++] = strdup("-passwd");
		argv_vnc[argc_vnc++] = strdup(HARDWIRE_PASSWD);
		got_passwd = 1;
		pw_loc = 100;
	}
#endif
#ifdef HARDWIRE_VIEWPASSWD
	if (! got_rfbauth && got_passwd && ! viewonly_passwd) {
		viewonly_passwd = strdup(HARDWIRE_VIEWPASSWD);
	}
#endif
	if (viewonly_passwd && pw_loc < 0) {
		rfbLog("-passwd must be supplied when using -viewpasswd\n");
		exit(1);
	}

	if (more_safe) {
		if (! quiet) {
			rfbLog("-safer mode:\n");
			rfbLog("   vnc_connect=0\n");
			rfbLog("   accept_remote_cmds=0\n");
			rfbLog("   safe_remote_only=1\n");
			rfbLog("   launch_gui=0\n");
		}
		vnc_connect = 0;
		accept_remote_cmds = 0;
		safe_remote_only = 1;
		launch_gui = 0;
	}

	/* fixup settings that do not make sense */
		
	if (use_threads && nofb && cursor_pos_updates) {
		if (! quiet) {
			rfbLog("disabling -threads under -nofb -cursorpos\n");
		}
		use_threads = 0;
	}
	if (tile_fuzz < 1) {
		tile_fuzz = 1;
	}
	if (waitms < 0) {
		waitms = 0;
	}

	if (alpha_threshold < 0) {
		alpha_threshold = 0;
	}
	if (alpha_threshold > 256) {
		alpha_threshold = 256;
	}
	if (alpha_frac < 0.0) {
		alpha_frac = 0.0;
	}
	if (alpha_frac > 1.0) {
		alpha_frac = 1.0;
	}
	if (alpha_blend) {
		alpha_remove = 0;
	}

	if (inetd) {
		shared = 0;
		connect_once = 1;
		bg = 0;
	}

	if (flip_byte_order && using_shm && ! quiet) {
		rfbLog("warning: -flipbyte order only works with -noshm\n");
	}

	if (! wireframe_copyrect) {
		set_wirecopyrect_mode(NULL);
	}
	if (! scroll_copyrect) {
		set_scrollcopyrect_mode(NULL);
	}
	initialize_scroll_matches();
	initialize_scroll_term();
	initialize_max_keyrepeat();

	/* increase rfbwait if threaded */
	if (use_threads && ! got_rfbwait) {
		rfbMaxClientWait = 604800000;
	}

	/* no framebuffer (Win2VNC) mode */

	if (nofb) {
		/* disable things that do not make sense with no fb */
		set_nofb_params();

		if (! got_deferupdate && ! got_defer) {
			/* reduce defer time under -nofb */
			defer_update = defer_update_nofb;
		}
		if (got_pointer_mode < 0) {
			pointer_mode = POINTER_MODE_NOFB;
		}
	}

	if (raw_fb_str) {
		set_raw_fb_params(0);
	}
	if (! got_deferupdate) {
		char tmp[40];
		/* XXX not working yet in libvncserver */
		sprintf(tmp, "%d", defer_update);
		argv_vnc[argc_vnc++] = strdup("-deferupdate");
		argv_vnc[argc_vnc++] = strdup(tmp);
	}

	if (debug_pointer || debug_keyboard) {
		if (bg || quiet) {
			rfbLog("disabling -bg/-q under -debug_pointer"
			    "/-debug_keyboard\n");
			bg = 0;
			quiet = 0;
		}
	}

	/* initialize added_keysyms[] array to zeros */
	add_keysym(NoSymbol);

	/* tie together cases of -localhost vs. -listen localhost */
	if (! listen_str) {
		if (allow_list && !strcmp(allow_list, "127.0.0.1")) {
			listen_str = strdup("localhost");
			argv_vnc[argc_vnc++] = strdup("-listen");
			argv_vnc[argc_vnc++] = strdup(listen_str);
		}
	} else if (!strcmp(listen_str, "localhost") ||
	    !strcmp(listen_str, "127.0.0.1")) {
		allow_list = strdup("127.0.0.1");
	}

	/* set OS struct UT */
	uname(&UT);

	initialize_crash_handler();

	if (! quiet) {
		fprintf(stderr, "\n");
		fprintf(stderr, "Settings:\n");
		fprintf(stderr, " display:    %s\n", use_dpy ? use_dpy
                    : "null");
		fprintf(stderr, " authfile:   %s\n", auth_file ? auth_file
                    : "null");
		fprintf(stderr, " subwin:     0x%lx\n", subwin);
		fprintf(stderr, " -sid mode:  %d\n", rootshift);
		fprintf(stderr, " clip:       %s\n", clip_str ? clip_str
                    : "null");
		fprintf(stderr, " flashcmap:  %d\n", flash_cmap);
		fprintf(stderr, " shiftcmap:  %d\n", shift_cmap);
		fprintf(stderr, " force_idx:  %d\n", force_indexed_color);
		fprintf(stderr, " visual:     %s\n", visual_str ? visual_str
                    : "null");
		fprintf(stderr, " overlay:    %d\n", overlay);
		fprintf(stderr, " ovl_cursor: %d\n", overlay_cursor);
		fprintf(stderr, " scaling:    %d %.5f\n", scaling, scale_fac);
		fprintf(stderr, " viewonly:   %d\n", view_only);
		fprintf(stderr, " shared:     %d\n", shared);
		fprintf(stderr, " conn_once:  %d\n", connect_once);
		fprintf(stderr, " timeout:    %d\n", first_conn_timeout);
		fprintf(stderr, " inetd:      %d\n", inetd);
		fprintf(stderr, " http:       %d\n", try_http);
		fprintf(stderr, " connect:    %s\n", client_connect
		    ? client_connect : "null");
		fprintf(stderr, " connectfile %s\n", client_connect_file
		    ? client_connect_file : "null");
		fprintf(stderr, " vnc_conn:   %d\n", vnc_connect);
		fprintf(stderr, " allow:      %s\n", allow_list ? allow_list
                    : "null");
		fprintf(stderr, " input:      %s\n", allowed_input_str
		    ? allowed_input_str : "null");
		fprintf(stderr, " passfile:   %s\n", passwdfile ? passwdfile
                    : "null");
		fprintf(stderr, " accept:     %s\n", accept_cmd ? accept_cmd
                    : "null");
		fprintf(stderr, " gone:       %s\n", gone_cmd ? gone_cmd
                    : "null");
		fprintf(stderr, " users:      %s\n", users_list ? users_list
                    : "null");
		fprintf(stderr, " using_shm:  %d\n", using_shm);
		fprintf(stderr, " flipbytes:  %d\n", flip_byte_order);
		fprintf(stderr, " onetile:    %d\n", single_copytile);
		fprintf(stderr, " solid:      %s\n", solid_str
		    ? solid_str : "null");
		fprintf(stderr, " blackout:   %s\n", blackout_str
		    ? blackout_str : "null");
		fprintf(stderr, " xinerama:   %d\n", xinerama);
		fprintf(stderr, " xtrap:      %d\n", xtrap_input);
		fprintf(stderr, " xrandr:     %d\n", xrandr);
		fprintf(stderr, " xrandrmode: %s\n", xrandr_mode ? xrandr_mode
		    : "null");
		fprintf(stderr, " padgeom:    %s\n", pad_geometry
		    ? pad_geometry : "null");
		fprintf(stderr, " logfile:    %s\n", logfile ? logfile
                    : "null");
		fprintf(stderr, " logappend:  %d\n", logfile_append);
		fprintf(stderr, " flag:       %s\n", flagfile ? flagfile
                    : "null");
		fprintf(stderr, " rc_file:    \"%s\"\n", rc_rcfile ? rc_rcfile
                    : "null");
		fprintf(stderr, " norc:       %d\n", rc_norc);
		fprintf(stderr, " dbg:        %d\n", crash_debug);
		fprintf(stderr, " bg:         %d\n", bg);
		fprintf(stderr, " mod_tweak:  %d\n", use_modifier_tweak);
		fprintf(stderr, " isolevel3:  %d\n", use_iso_level3);
		fprintf(stderr, " xkb:        %d\n", use_xkb_modtweak);
		fprintf(stderr, " skipkeys:   %s\n",
		    skip_keycodes ? skip_keycodes : "null");
		fprintf(stderr, " skip_dups:  %d\n", skip_duplicate_key_events);
		fprintf(stderr, " addkeysyms: %d\n", add_keysyms);
		fprintf(stderr, " xkbcompat:  %d\n", xkbcompat);
		fprintf(stderr, " clearmods:  %d\n", clear_mods);
		fprintf(stderr, " remap:      %s\n", remap_file ? remap_file
                    : "null");
		fprintf(stderr, " norepeat:   %d\n", no_autorepeat);
		fprintf(stderr, " norepeatcnt:%d\n", no_repeat_countdown);
		fprintf(stderr, " nofb:       %d\n", nofb);
		fprintf(stderr, " watchbell:  %d\n", watch_bell);
		fprintf(stderr, " watchsel:   %d\n", watch_selection);
		fprintf(stderr, " watchprim:  %d\n", watch_primary);
		fprintf(stderr, " cursor:     %d\n", show_cursor);
		fprintf(stderr, " multicurs:  %d\n", show_multiple_cursors);
		fprintf(stderr, " curs_mode:  %s\n", multiple_cursors_mode
		    ? multiple_cursors_mode : "null");
		fprintf(stderr, " arrow:      %d\n", alt_arrow);
		fprintf(stderr, " xfixes:     %d\n", use_xfixes);
		fprintf(stderr, " alphacut:   %d\n", alpha_threshold);
		fprintf(stderr, " alphafrac:  %.2f\n", alpha_frac);
		fprintf(stderr, " alpharemove:%d\n", alpha_remove);
		fprintf(stderr, " alphablend: %d\n", alpha_blend);
		fprintf(stderr, " cursorshape:%d\n", cursor_shape_updates);
		fprintf(stderr, " cursorpos:  %d\n", cursor_pos_updates);
		fprintf(stderr, " xwarpptr:   %d\n", use_xwarppointer);
		fprintf(stderr, " buttonmap:  %s\n", pointer_remap
		    ? pointer_remap : "null");
		fprintf(stderr, " dragging:   %d\n", show_dragging);
		fprintf(stderr, " wireframe:  %s\n", wireframe_str ?
		    wireframe_str : WIREFRAME_PARMS);
		fprintf(stderr, " wirecopy:   %s\n", wireframe_copyrect ?
		    wireframe_copyrect : wireframe_copyrect_default);
		fprintf(stderr, " scrollcopy: %s\n", scroll_copyrect ?
		    scroll_copyrect : scroll_copyrect_default);
		fprintf(stderr, "  scr_area:  %d\n", scrollcopyrect_min_area);
		fprintf(stderr, "  scr_skip:  %s\n", scroll_skip_str ?
		    scroll_skip_str : scroll_skip_str0);
		fprintf(stderr, "  scr_inc:   %s\n", scroll_good_str ?
		    scroll_good_str : scroll_good_str0);
		fprintf(stderr, "  scr_keys:  %s\n", scroll_key_list_str ?
		    scroll_key_list_str : "null");
		fprintf(stderr, "  scr_parms: %s\n", scroll_copyrect_str ?
		    scroll_copyrect_str : SCROLL_COPYRECT_PARMS);
		fprintf(stderr, " ptr_mode:   %d\n", pointer_mode);
		fprintf(stderr, " inputskip:  %d\n", ui_skip);
		fprintf(stderr, " speeds:     %s\n", speeds_str
		    ? speeds_str : "null");
		fprintf(stderr, " debug_ptr:  %d\n", debug_pointer);
		fprintf(stderr, " debug_key:  %d\n", debug_keyboard);
		fprintf(stderr, " defer:      %d\n", defer_update);
		fprintf(stderr, " waitms:     %d\n", waitms);
		fprintf(stderr, " wait_ui:    %.2f\n", wait_ui);
		fprintf(stderr, " nowait_bog: %d\n", !wait_bog);
		fprintf(stderr, " readtimeout: %d\n", rfbMaxClientWait/1000);
		fprintf(stderr, " take_naps:  %d\n", take_naps);
		fprintf(stderr, " sb:         %d\n", screen_blank);
		fprintf(stderr, " xdamage:    %d\n", use_xdamage);
		fprintf(stderr, "  xd_area:   %d\n", xdamage_max_area);
		fprintf(stderr, "  xd_mem:    %.3f\n", xdamage_memory);
		fprintf(stderr, " sigpipe:    %s\n", sigpipe
		    ? sigpipe : "null");
		fprintf(stderr, " threads:    %d\n", use_threads);
		fprintf(stderr, " fs_frac:    %.2f\n", fs_frac);
		fprintf(stderr, " gaps_fill:  %d\n", gaps_fill);
		fprintf(stderr, " grow_fill:  %d\n", grow_fill);
		fprintf(stderr, " tile_fuzz:  %d\n", tile_fuzz);
		fprintf(stderr, " snapfb:     %d\n", use_snapfb);
		fprintf(stderr, " rawfb:      %s\n", raw_fb_str
		    ? raw_fb_str : "null");
		fprintf(stderr, " pipeinput:  %s\n", pipeinput_str
		    ? pipeinput_str : "null");
		fprintf(stderr, " gui:        %d\n", launch_gui);
		fprintf(stderr, " gui_mode:   %s\n", gui_str
		    ? gui_str : "null");
		fprintf(stderr, " noremote:   %d\n", !accept_remote_cmds);
		fprintf(stderr, " unsafe:     %d\n", !safe_remote_only);
		fprintf(stderr, " privremote: %d\n", priv_remote);
		fprintf(stderr, " safer:      %d\n", more_safe);
		fprintf(stderr, " nocmds:     %d\n", no_external_cmds);
		fprintf(stderr, " deny_all:   %d\n", deny_all);
		fprintf(stderr, "\n");
		rfbLog("x11vnc version: %s\n", lastmod);
	} else {
		rfbLogEnable(0);
	}

	X_INIT;
	SCR_INIT;
	
	/* open the X display: */
	if (auth_file) {
		set_env("XAUTHORITY", auth_file);
	}
#if LIBVNCSERVER_HAVE_XKEYBOARD
	/*
	 * Disable XKEYBOARD before calling XOpenDisplay()
	 * this should be used if there is ambiguity in the keymapping. 
	 */
	if (xkbcompat) {
		Bool rc = XkbIgnoreExtension(True);
		if (! quiet) {
			rfbLog("Disabling xkb XKEYBOARD extension. rc=%d\n",
			    rc);
		}
		if (watch_bell) {
			watch_bell = 0;
			if (! quiet) rfbLog("Disabling bell.\n");
		}
	}
#else
	watch_bell = 0;
	use_xkb_modtweak = 0;
#endif

	if (users_list && strstr(users_list, "lurk=")) {
		if (use_dpy) {
			rfbLog("warning: -display does not make sense in "
			    "\"lurk=\" mode...\n");
		}
		lurk_loop(users_list);
	}

	if (use_dpy) {
		dpy = XOpenDisplay(use_dpy);
	} else if ( (use_dpy = getenv("DISPLAY")) ) {
		dpy = XOpenDisplay(use_dpy);
	} else {
		dpy = XOpenDisplay("");
	}

	if (! dpy && raw_fb_str) {
		rfbLog("continuing without X display in -rawfb mode, "
		    "hold on tight..\n");
		goto raw_fb_pass_go_and_collect_200_dollars;
	}

	if (! dpy && ! use_dpy && ! getenv("DISPLAY")) {
		int i, s = 4;
		rfbLog("\a\n");
		rfbLog("*** XOpenDisplay failed. No -display or DISPLAY.\n");
		rfbLog("*** Trying \":0\" in %d seconds.  Press Ctrl-C to"
		    " abort.\n", s);
		rfbLog("*** ");
		for (i=1; i<=s; i++)  {
			fprintf(stderr, "%d ", i);
			sleep(1);
		}
		fprintf(stderr, "\n");
		use_dpy = ":0";
		dpy = XOpenDisplay(use_dpy);
		if (dpy) {
			rfbLog("*** XOpenDisplay of \":0\" successful.\n");
		}
		rfbLog("\n");
	}

	if (! dpy) {
		rfbLog("XOpenDisplay failed (%s)\n", use_dpy ? use_dpy:"null");
		exit(1);
	} else if (use_dpy) {
		if (! quiet) rfbLog("Using X display %s\n", use_dpy);
	} else {
		if (! quiet) rfbLog("Using default X display.\n");
	}

	scr = DefaultScreen(dpy);
	rootwin = RootWindow(dpy, scr);

	if (! quiet) {
		rfbLog("\n");
		rfbLog("------------------ USEFUL INFORMATION ------------------\n");
	}

	if (remote_cmd || query_cmd) {
		int rc = do_remote_query(remote_cmd, query_cmd, remote_sync);
		XFlush(dpy);
		fflush(stderr);
		fflush(stdout);
		usleep(30 * 1000);	/* still needed? */
		XCloseDisplay(dpy);
		exit(rc);
	}

	if (priv_remote) {
		if (! remote_control_access_ok()) {
			rfbLog("** Disabling remote commands in -privremote "
			    "mode.\n");
			accept_remote_cmds = 0;
		}
	}

#if LIBVNCSERVER_HAVE_LIBXFIXES
	if (! XFixesQueryExtension(dpy, &xfixes_base_event_type, &er)) {
		if (! quiet) {
			rfbLog("Disabling XFIXES mode: display does not "
			    "support it.\n");
		}
		xfixes_base_event_type = 0;
		xfixes_present = 0;
	} else {
		xfixes_present = 1;
	}
#endif
	if (! xfixes_present) {
		use_xfixes = 0;
	}

#if LIBVNCSERVER_HAVE_LIBXDAMAGE
	if (! XDamageQueryExtension(dpy, &xdamage_base_event_type, &er)) {
		if (! quiet) {
			rfbLog("Disabling X DAMAGE mode: display does not "
			    "support it.\n");
		}
		xdamage_base_event_type = 0;
		xdamage_present = 0;
	} else {
		xdamage_present = 1;
	}
#endif
	if (! xdamage_present) {
		use_xdamage = 0;
	}
	if (! quiet && xdamage_present && use_xdamage) {
		rfbLog("X DAMAGE available on display, using it for"
		    " polling hints.\n");
		rfbLog("  To disable this behavior use: "
		    "'-noxdamage'\n");
	}

	if (! quiet && wireframe) {
		rfbLog("Wireframing: -wireframe mode is in effect for window moves.\n");
		rfbLog("  If this yields undesired behavior (poor response, painting\n");
		rfbLog("  errors, etc) it may be disabled:\n");
		rfbLog("   - use '-nowf' to disable wireframing completely.\n");
		rfbLog("   - use '-nowcr' to disable the Copy Rectangle after the\n");
		rfbLog("     moved window is released in the new position.\n");
		rfbLog("  Also see the -help entry for tuning parameters.\n");
	}

	tmpi = 1;
	if (scroll_copyrect) {
		if (strstr(scroll_copyrect, "never")) {
			tmpi = 0;
		}
	} else if (scroll_copyrect_default) {
		if (strstr(scroll_copyrect_default, "never")) {
			tmpi = 0;
		}
	}
	if (! quiet && tmpi) {
		rfbLog("Scroll Detection: -scrollcopyrect mode is in effect to\n");
		rfbLog("  use RECORD extension to try to detect scrolling windows\n");
		rfbLog("  (induced by either user keystroke or mouse input).\n");
		rfbLog("  If this yields undesired behavior (poor response, painting\n");
		rfbLog("  errors, etc) it may be disabled via: '-noscr'\n");
		rfbLog("  Also see the -help entry for tuning parameters.\n");
	}

	overlay_present = 0;
#ifdef SOLARIS_OVERLAY
	if (! XQueryExtension(dpy, "SUN_OVL", &maj, &ev, &er)) {
		if (! quiet && overlay) {
			rfbLog("Disabling -overlay: SUN_OVL "
			    "extension not available.\n");
		}
	} else {
		overlay_present = 1;
	}
#endif
#ifdef IRIX_OVERLAY
	if (! XReadDisplayQueryExtension(dpy, &ev, &er)) {
		if (! quiet && overlay) {
			rfbLog("Disabling -overlay: IRIX ReadDisplay "
			    "extension not available.\n");
		}
	} else {
		overlay_present = 1;
	}
#endif
	if (overlay && !overlay_present) {
		overlay = 0;
		overlay_cursor = 0;
	}

	/* cursor shapes setup */
	if (! multiple_cursors_mode) {
		multiple_cursors_mode = strdup("default");
	}
	if (show_cursor) {
		if(!strcmp(multiple_cursors_mode, "default")
		    && xfixes_present && use_xfixes) {
			free(multiple_cursors_mode);
			multiple_cursors_mode = strdup("most");

			if (! quiet) {
				rfbLog("XFIXES available on display, resetting"
				    " cursor mode\n");
				rfbLog("  to: '-cursor most'.\n");
				rfbLog("  to disable this behavior use: "
				    "'-cursor arrow'\n");
				rfbLog("  or '-noxfixes'.\n");
			}
		}
		if(!strcmp(multiple_cursors_mode, "most")) {
			if (xfixes_present && use_xfixes &&
			    overlay_cursor == 1) {
				if (! quiet) {
					rfbLog("using XFIXES for cursor "
					    "drawing.\n");
				}
				overlay_cursor = 0;
			}
		}
	}

	if (overlay) {
		using_shm = 0;
		if (flash_cmap && ! quiet) {
			rfbLog("warning: -flashcmap may be "
			    "incompatible with -overlay\n");
		}
		if (show_cursor && overlay_cursor) {
			char *s = multiple_cursors_mode;
			if (*s == 'X' || !strcmp(s, "some") ||
			    !strcmp(s, "arrow")) {
				/*
				 * user wants these modes, so disable fb cursor
				 */
				overlay_cursor = 0;
			} else {
				/*
				 * "default" and "most", we turn off
				 * show_cursor since it will automatically
				 * be in the framebuffer.
				 */
				show_cursor = 0;
			}
		}
	}

	initialize_cursors_mode();

	/* check for XTEST */
	if (! XTestQueryExtension_wr(dpy, &ev, &er, &maj, &min)) {
		if (! quiet) {
			rfbLog("WARNING: XTEST extension not available "
			    "(either missing from\n");
			rfbLog("  display or client library libXtst "
			    "missing at build time).\n");
			rfbLog("  MOST user input (pointer and keyboard) "
			    "will be DISCARDED.\n");
			rfbLog("  If display does have XTEST, be sure to "
			    "build x11vnc with\n");
			rfbLog("  a working libXtst build environment "
			    "(e.g. libxtst-dev,\n");
			rfbLog("  or other packages).\n");
			rfbLog("No XTEST extension, switching to "
			    "-xwarppointer mode for\n");
			rfbLog("  pointer motion input.\n");
		}
		xtest_present = 0;
		use_xwarppointer = 1;
	} else {
		xtest_present = 1;
		xtest_base_event_type = ev;
		if (maj <= 1 || (maj == 2 && min <= 2)) {
			/* no events defined as of 2.2 */
			xtest_base_event_type = 0;
		}
	}

	if (! XETrapQueryExtension_wr(dpy, &ev, &er, &maj)) {
		xtrap_present = 0;
	} else {
		xtrap_present = 1;
		xtrap_base_event_type = ev;
	}

	/*
	 * Window managers will often grab the display during resize,
	 * etc, using XGrabServer().  To avoid deadlock (our user resize
	 * input is not processed) we tell the server to process our
	 * requests during all grabs:
	 */
	disable_grabserver(dpy, 0);

	/* check for RECORD */
	if (! XRecordQueryVersion_wr(dpy, &maj, &min)) {
		xrecord_present = 0;
	} else {
		xrecord_present = 1;
	}

	initialize_xrecord();

	/* check for OS with small shm limits */
	if (using_shm && ! single_copytile) {
		if (limit_shm()) {
			single_copytile = 1;
		}
	}

	single_copytile_orig = single_copytile;

	/* check for MIT-SHM */
	if (! XShmQueryExtension_wr(dpy)) {
		xshm_present = 0;
		if (! using_shm) {
			if (! quiet) {
				rfbLog("info: display does not support"
				    " XShm.\n");
			}
		} else {
		    if (! quiet) {
			rfbLog("warning: XShm extension is not available.\n");
			rfbLog("For best performance the X Display should be"
			    " local. (i.e.\n");
			rfbLog("the x11vnc and X server processes should be"
			    " running on\n");
			rfbLog("the same machine.)\n");
#if LIBVNCSERVER_HAVE_XSHM
			rfbLog("Restart with -noshm to override this.\n");
		    }
		    exit(1);
#else
			rfbLog("Switching to -noshm mode.\n");
		    }
		    using_shm = 0;
#endif
		}
	}

#if LIBVNCSERVER_HAVE_XKEYBOARD
	/* check for XKEYBOARD */
	initialize_xkb();
	initialize_watch_bell();
	if (!xkb_present && use_xkb_modtweak) {
		if (! quiet) {
			rfbLog("warning: disabling xkb modtweak."
			    " XKEYBOARD ext. not present.\n");
		}
		use_xkb_modtweak = 0;
	}
#endif

	if (xkb_present && !use_xkb_modtweak && !got_noxkb) {
		if (use_modifier_tweak) {
			switch_to_xkb_if_better();
		}
	}

#if LIBVNCSERVER_HAVE_LIBXRANDR
	if (! XRRQueryExtension(dpy, &xrandr_base_event_type, &er)) {
		if (xrandr && ! quiet) {
			rfbLog("Disabling -xrandr mode: display does not"
			    " support X RANDR.\n");
		}
		xrandr_base_event_type = 0;
		xrandr = 0;
		xrandr_present = 0;
	} else {
		xrandr_present = 1;
	}
#endif

	if (! quiet) {
		rfbLog("--------------------------------------------------------\n");
		rfbLog("\n");
	}

	raw_fb_pass_go_and_collect_200_dollars:

	if (! dt) {
		static char str[] = "-desktop";
		argv_vnc[argc_vnc++] = str;
		argv_vnc[argc_vnc++] = choose_title(use_dpy);
		rfb_desktop_name = strdup(argv_vnc[argc_vnc-1]);
	}
	
	initialize_pipeinput();

	/*
	 * Create the XImage corresponding to the display framebuffer.
	 */

	fb0 = initialize_xdisplay_fb();

	/*
	 * n.b. we do not have to X_LOCK any X11 calls until watch_loop()
	 * is called since we are single-threaded until then.
	 */

	initialize_screen(&argc_vnc, argv_vnc, fb0);

	if (try_http && ! got_httpdir && check_httpdir()) {
		http_connections(1);
	}

	initialize_tiles();

	/* rectangular blackout regions */
	initialize_blackouts_and_xinerama();

	/* created shm or XImages when using_shm = 0 */
	initialize_polling_images();

	initialize_signals();

	initialize_speeds();

	initialize_keyboard_and_pointer();

	initialize_allowed_input();

	if (! inetd) {
		if (! screen->port || screen->listenSock < 0) {
			rfbLog("Error: could not obtain listening port.\n");
			clean_up_exit(1);
		}
	}
	if (! quiet) {
		rfbLog("screen setup finished.\n");
	}
	set_vnc_desktop_name();

#if LIBVNCSERVER_HAVE_FORK && LIBVNCSERVER_HAVE_SETSID
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
