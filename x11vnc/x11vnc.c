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
 * are also applied (currently there are a bit too many of these...)
 *
 * Another goal is to add many features that enable and incourage creative
 * Ausage and application of the tool.  pologies for the large number
 * Aof options!
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
 * callback from the X server, if that were generally possible... (Update:
 * this seems to be handled now with the X DAMAGE extension, but
 * unfortunately that doesn't seem to address the slow read from the
 * video h/w.  So, e.g., opaque moves and similar window activity can
 * be very painful; one has to modify one's behavior a bit.
 *
 * General audio at the remote display is lost unless one separately
 * sets up some audio side-channel such as esd.
 *
 * It does not appear possible to query the X server for the current
 * cursor shape.  We can use XTest to compare cursor to current window's
 * cursor, but we cannot extract what the cursor is... (Update: we now
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
 * poorly approximated if it has transparency.
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
 * the window not sufficiently mapped into server memory, etc (Update:
 * we now use the -xrandr mechanisms to trap errors for this mode).
 * SaveUnders menus, popups, etc will not be seen.
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

/*
 * if you are inserting this file, x11vnc.c into an old CVS tree you
 * may need to set OLD_TREE to 1.  See below for LibVNCServer 0.7 tips.
 */

#define OLD_TREE 0
#if OLD_TREE

/*
 * if you have a very old tree (LibVNCServer 0.6) and get errors these may
 * be need to be uncommented.  LibVNCServer <= 0.5 is no longer supported.
 * note the maxRectsPerUpdate below is a hack that may break some usage.
#define oldCursorX cursorX
#define oldCursorY cursorY
#define thisHost rfbThisHost
#define framebufferUpdateMessagesSent rfbFramebufferUpdateMessagesSent
#define bytesSent rfbBytesSent
#define rawBytesEquivalent rfbRawBytesEquivalent
#define progressiveSliceHeight maxRectsPerUpdate
 */

/* 
 * If you are building in an older libvncserver tree with this newer
 * x11vnc.c file using OLD_TREE=1 you may need to set some of these lines
 * since your older libvncserver configure is not setting them.
 *
 * For the features LIBVNCSERVER_HAVE_LIBXINERAMA and
 * LIBVNCSERVER_HAVE_XFIXES you may also need to add 
 * -lXinerama or -lXfixes, respectively, to the linking line, e.g.
 * by setting them in LD_FLAGS before running configure.
 */

#define LIBVNCSERVER_HAVE_XSHM 1
#define LIBVNCSERVER_HAVE_XTEST 1

#define LIBVNCSERVER_HAVE_PWD_H 1
#define LIBVNCSERVER_HAVE_SYS_WAIT_H 1
#define LIBVNCSERVER_HAVE_UTMPX_H 1

/*
#define LIBVNCSERVER_HAVE_LIBXINERAMA 1
#define LIBVNCSERVER_HAVE_XFIXES 1
#define LIBVNCSERVER_HAVE_LIBXDAMAGE 1
 */

#endif	/* OLD_TREE */

#include <unistd.h>
#include <signal.h>
#include <sys/utsname.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <X11/keysym.h>
#include <X11/Xatom.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

#include <rfb/rfb.h>
#include <rfb/rfbregion.h>

#if OLD_TREE
/*
 * This is another transient for building in older libvncserver trees,
 * due to the API change:
 */
#define dontDisconnect	rfbDontDisconnect
#define neverShared	rfbNeverShared
#define alwaysShared	rfbAlwaysShared
#define clientHead	rfbClientHead
#define serverFormat	rfbServerFormat
#define port		rfbPort
#define listenSock	rfbListenSock
#define deferUpdateTime	rfbDeferUpdateTime
#define authPasswdData	rfbAuthPasswdData
#define rfbEncryptAndStorePasswd	vncEncryptAndStorePasswd
#define maxClientWait	rfbMaxClientWait
#define rfbHttpInitSockets	httpInitSockets

#define RFBUNDRAWCURSOR(s) if (s) {rfbUndrawCursor(s);}
#else
#define RFBUNDRAWCURSOR(s)
#endif
/*
 * To get a clean build in a LibVNCServer 0.7 source tree no need for
 * OLD_TREE, you just need to either download the forgotten tkx11vnc.h
 * file or run:
 *
 *	echo 'char gui_code[] = "";' > tkx11vnc.h
 *
 * (this disables the gui) and uncomment this line:
#define rfbSetCursor(a, b) rfbSetCursor((a), (b), FALSE)
 */

/*
 * To get a clean build on LibVNCServer 0.7.1 no need for OLD_TREE,
 * just uncomment this line (note the maxRectsPerUpdate below is a hack
 * that may break some usage):
 *
#define listenInterface maxRectsPerUpdate
 */

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
static int xrandr_base_event_type;
#endif

int xfixes_present = 0;
int use_xfixes = 1;
int got_xfixes_cursor_notify = 0;
int alpha_threshold = 240;
double alpha_frac = 0.33;
int alpha_remove = 0;
int alpha_blend = 1;

int alt_arrow = 1;

#if LIBVNCSERVER_HAVE_LIBXFIXES
#include <X11/extensions/Xfixes.h>
static int xfixes_base_event_type;
#endif

int xdamage_present = 0;
int using_xdamage = 0;
int use_xdamage_hints = 1;	/* just use the xdamage rects. for scanline hints */
#if LIBVNCSERVER_HAVE_LIBXDAMAGE
#include <X11/extensions/Xdamage.h>
static int xdamage_base_event_type;
Damage xdamage = 0;
#endif
int xdamage_max_area = 20000;	/* pixels */
double xdamage_memory = 1.0;	/* in units of NSCAN */
int xdamage_tile_count;

int hack_val = 0;

/*               date +'lastmod: %Y-%m-%d' */
char lastmod[] = "0.7.2pre lastmod: 2005-03-19";

/* X display info */

Display *dpy = 0;		/* the single display screen we connect to */
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
int num_buttons = -1;

/* image structures */
XImage *scanline;
XImage *fullscreen;
XImage **tile_row;		/* for all possible row runs */
XImage *fb0;
XImage *snaprect = NULL;	/* for XShmGetImage (fs_factor) */
XImage *snap = NULL;		/* the full snap fb */

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
char *snap_fb = NULL;		/* used under -snapfb */
char *fake_fb = NULL;		/* used under -padgeom */
int rfb_bytes_per_line;
int main_bytes_per_line;
unsigned long  main_red_mask,  main_green_mask,  main_blue_mask;
unsigned short main_red_max,   main_green_max,   main_blue_max;
unsigned short main_red_shift, main_green_shift, main_blue_shift;

/* we now have a struct with client specific data: */
#define RATE_SAMPLES 5
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
	int set_cmp_bytes;
	int set_raw_bytes;
	double cmp_samp[RATE_SAMPLES];
	double raw_samp[RATE_SAMPLES];
	int sample;
} ClientData;

/* scaling parameters */
char *scale_str = NULL;
double scale_fac = 1.0;
int scaling = 0;
int scaling_noblend = 0;	/* no blending option (very course) */
int scaling_nomult4 = 0;	/* do not require width = n * 4 */
int scaling_pad = 0;		/* pad out scaled sizes to fit denominator */
int scaling_interpolate = 0;	/* use interpolation scheme when shrinking */
int scaled_x = 0, scaled_y = 0;	/* dimensions of scaled display */
int scale_numer = 0, scale_denom = 0;	/* n/m */

/* scale cursor */
char *scale_cursor_str = NULL;
double scale_cursor_fac = 1.0;
int scaling_cursor = 0;
int scaling_cursor_noblend = 0;
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
time_t last_event, last_input, last_client = 0;

/* last client to move pointer */
rfbClientPtr last_pointer_client = NULL;

int accepted_client = 0;
int client_count = 0;
int clients_served = 0;

/* more transient kludge variables: */
int cursor_x, cursor_y;		/* x and y from the viewer(s) */
int got_user_input = 0;
int got_pointer_input = 0;
int got_keyboard_input = 0;
int last_keyboard_input = 0;
int fb_copy_in_progress = 0;	
int drag_in_progress = 0;	
int shut_down = 0;	
int do_copy_screen = 0;	
time_t damage_time = 0;
int damage_delay = 0;

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
void autorepeat(int restore);
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
void delete_keycode(KeyCode);
void delete_added_keycodes(void);

double dtime(double *);

void initialize_blackouts(char *);
void initialize_blackouts_and_xinerama(void);
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
void create_xdamage(void);
void destroy_xdamage(void);
void initialize_xrandr(void);
XImage *initialize_xdisplay_fb(void);

void keyboard(rfbBool down, rfbKeySym keysym, rfbClientPtr client);

void XTestFakeKeyEvent_wr(Display*, KeyCode, Bool, unsigned long);
void XTestFakeButtonEvent_wr(Display*, unsigned int, Bool, unsigned long);
void XTestFakeMotionEvent_wr(Display*, int, int, int, unsigned long);
int XTestGrabControl_wr(Display*, Bool);
Bool XTestCompareCurrentCursorWithWindow_wr(Display*, Window);
Bool XTestCompareCursorWithWindow_wr(Display*, Window, Cursor);
Bool XTestQueryExtension_wr(Display*, int*, int*, int*, int*);
void XTestDiscard_wr(Display*);

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
void cursor_position(int, int);

void read_vnc_connect_prop(void);
void set_vnc_connect_prop(char *);
char *process_remote_cmd(char *, int);
void rfbPE(rfbScreenInfoPtr, long);
void rfbCFD(rfbScreenInfoPtr, long);
int scan_for_updates(int);
void set_colormap(int);
void set_offset(void);
void set_rfb_cursor(int);
void set_visual(char *vstring);
void set_cursor(int, int, int);
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

void shm_clean(XShmSegmentInfo *, XImage *);
void shm_delete(XShmSegmentInfo *);

void check_x11_pointer(void);
void check_bell_event(void);
void check_xevents(void);
char *this_host(void);
void set_vnc_desktop_name(void);

char *short_kmb(char *);

int get_cmp_rate(void);
int get_raw_rate(void);
int get_read_rate(void);
int get_net_rate(void);
int get_net_latency(void);
void measure_send_rates(int);

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
void refresh_screen(void);

/* -- options.h -- */
/* 
 * variables for the command line options
 */
char *use_dpy = NULL;		/* -display */
char *auth_file = NULL;		/* -auth/-xauth */
char *visual_str = NULL;	/* -visual */
char *logfile = NULL;		/* -o, -logfile */
int logfile_append = 0;
char *passwdfile = NULL;	/* -passwdfile */
char *blackout_str = NULL;	/* -blackout */
char *clip_str = NULL;		/* -clip */
int use_solid_bg = 0;		/* -solid */
char *solid_str = NULL;
char *solid_default = "cyan4";

char *speeds_str = NULL;	/* -speeds TBD */
int measure_speeds = 1;
int speeds_net_rate = 0;
int speeds_net_latency = 0;
int speeds_read_rate = 0;

char *rc_rcfile = NULL;		/* -rc */
int rc_norc = 0;
int opts_bg = 0;

int shared = 0;			/* share vnc display. */
int deny_all = 0;		/* global locking of new clients */
int accept_remote_cmds = 1;	/* -noremote */
int safe_remote_only = 0;	/* -safer, -unsafe */
int started_as_root = 0;
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
int connect_once = 1;		/* disconnect after first connection session. */
int first_conn_timeout = 0;	/* -timeout */
int flash_cmap = 0;		/* follow installed colormaps */
int force_indexed_color = 0;	/* whether to force indexed color for 8bpp */
int launch_gui = 0;		/* -gui */

int use_modifier_tweak = 1;	/* use the shift/altgr modifier tweak */
int use_iso_level3 = 0;		/* ISO_Level3_Shift instead of Mode_switch */
int clear_mods = 0;		/* -clear_mods (1) and -clear_keys (2) */
int nofb = 0;			/* do not send any fb updates */

unsigned long subwin = 0x0;	/* -id, -sid */
int subwin_wait_mapped = 0;

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
int no_autorepeat = 1;		/* turn off autorepeat with clients */
int no_repeat_countdown = 2;
int watch_bell = 1;		/* watch for the bell using XKEYBOARD */
int sound_bell = 1;		/* actually send it */
int xkbcompat = 0;		/* ignore XKEYBOARD extension */
int use_xkb = 0;		/* try to open Xkb connection (for bell or other) */
int use_xkb_modtweak = 0;	/* -xkb */
char *skip_keycodes = NULL;
int add_keysyms = 0;		/* automatically add keysyms to X server */

char *remap_file = NULL;	/* -remap */
char *pointer_remap = NULL;
/* use the various ways of updating pointer */
#define POINTER_MODE_DEFAULT 2
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

#if LIBVNCSERVER_HAVE_XSHM
int xshm_present = 1;
#else
int xshm_present = 0;
#endif
#if LIBVNCSERVER_HAVE_XTEST
int xtest_present = 1;
#else
int xtest_present = 0;
#endif
#if LIBVNCSERVER_HAVE_XKEYBOARD
int xkb_present = 1;
#else
int xkb_present = 0;
#endif
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
#define X_LOCK       LOCK(x11Mutex)
#define X_UNLOCK   UNLOCK(x11Mutex)
#define X_INIT INIT_MUTEX(x11Mutex)

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
				rfbPE(screen, -1);
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

void check_switched_user (void) {
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
		Display *dpy2 = 0;
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

char *ip2host(char *ip) {
	char *str;
#if LIBVNCSERVER_HAVE_NETDB_H && LIBVNCSERVER_HAVE_NETINET_IN_H
	struct hostent *hp;
	in_addr_t iaddr;

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
	} else if (using_shm && w == dest->width && h == dest->height) {
		XShmGetImage_wr(dpy, window, dest, x, y, AllPlanes);
	} else {
		XGetSubImage_wr(dpy, window, x, y, w, h, AllPlanes,
		    ZPixmap, dest, 0, 0);
	}
}

/*
 * wrappers for XTestFakeKeyEvent, etc..
 */
void XTestFakeKeyEvent_wr(Display* dpy, KeyCode key, Bool down,
    unsigned long delay) {
	if (debug_keyboard) {
		rfbLog("XTestFakeKeyEvent(dpy, keycode=0x%x \"%s\", %s)\n",
		    key, XKeysymToString(XKeycodeToKeysym(dpy, key, 0)),
		    down ? "down":"up");
	}
	if (! xtest_present) {
		return;
	}
	if (down) {
		last_keyboard_input = -key;
	} else {
		last_keyboard_input = key;
	}
#if LIBVNCSERVER_HAVE_XTEST
	XTestFakeKeyEvent(dpy, key, down, delay);
#endif
}

void XTestFakeButtonEvent_wr(Display* dpy, unsigned int button, Bool is_press,
    unsigned long delay) {
	if (! xtest_present) {
		return;
	}
#if LIBVNCSERVER_HAVE_XTEST
    	XTestFakeButtonEvent(dpy, button, is_press, delay);
#endif
}

void XTestFakeMotionEvent_wr(Display* dpy, int screen, int x, int y,
    unsigned long delay) {
	if (! xtest_present) {
		return;
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

int XTestGrabControl_wr(Display* dpy, Bool impervious) {
	if (! xtest_present) {
		return 0;
	}
#if LIBVNCSERVER_HAVE_XTEST
	return XTestGrabControl(dpy, impervious);
#else
	return 0;
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


/* -- cleanup.c -- */
/*
 * Exiting and error handling routines
 */

static int exit_flag = 0;
int exit_sig = 0;

void clean_shm(int quick) {
	int i, cnt = 0;

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

	/* X keyboard cleanups */
	delete_added_keycodes();

	if (clear_mods == 1) {
		clear_modifiers(0);
	} else if (clear_mods == 2) {
		clear_keys();
	}

	if (no_autorepeat) {
		autorepeat(1);
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
	delete_added_keycodes();

	if (clear_mods == 1) {
		clear_modifiers(0);
	} else if (clear_mods == 2) {
		clear_keys();
	}
	if (no_autorepeat) {
		autorepeat(1);
	}
	if (use_solid_bg) {
		solid_bg(1);
	}

	if (sig) {
		exit(2);
	}
}

/* trapping utility to check for a valid window: */
int valid_window(Window win) {
	XErrorHandler old_handler;
	XWindowAttributes attr;
	int ok = 0;

	trapped_xerror = 0;
	old_handler = XSetErrorHandler(trap_xerror);
	if (XGetWindowAttributes(dpy, win, &attr)) {
		ok = 1;
	}
	if (trapped_xerror && trapped_xerror_event && ! quiet) {
		rfbLog("trapped XError: %s (0x%lx)\n",
		    xerror_string(trapped_xerror_event), win);
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
		if (! valid_window(win)) {
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
	if (XGetWindowAttributes(dpy, win, &attr)) {
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

/*
 * utility to run a user supplied command setting some RFB_ env vars.
 * used by, e.g., accept_client() and client_gone()
 */
static int run_user_command(char *cmd, rfbClientPtr client, char *mode) {
	char *dpystr = DisplayString(dpy);
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
	set_env("DISPLAY", dpystr);

	/*
	 * work out the number of clients (have to use client_count
	 * since there is deadlock in rfbGetClientIterator) 
	 */
	sprintf(str, "%d", client_count);
	set_env("RFB_CLIENT_COUNT", str);

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
	rfbLog("client_count: %d\n", client_count);

	if (no_autorepeat && client_count == 0) {
		autorepeat(1);
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
	char line[1024], host[1024];
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

	if (fgets(line, 1024, in) != NULL) {
		if (sscanf(line, "%s", host) == 1) {
			if (strlen(host) > 0) {
				char *str = strdup(host);
				rfbLog("read connect file: %s\n", host);
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
	int i;
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
	client_count++;

	if (no_autorepeat && client_count == 1) {
		/* first client, turn off X server autorepeat */
		autorepeat(0);
	}
	if (use_solid_bg && client_count == 1) {
		solid_bg(0);
	}

	if (pad_geometry) {
		install_padded_fb(pad_geometry);
	}

	for (i=0; i<RATE_SAMPLES; i++) {
		cd->cmp_samp[i] = 5000;	/* 56k modem */
		cd->raw_samp[i] = 50000;
	}
	cd->sample = 0;

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
int get_autorepeat_state(void) {
	XKeyboardState kstate;
	X_LOCK;
	XGetKeyboardControl(dpy, &kstate);
	X_UNLOCK;
	return kstate.global_auto_repeat;
}

void autorepeat(int restore) {
	int global_auto_repeat;
	XKeyboardControl kctrl;
	static int save_auto_repeat = -1;

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

		rfbLog("Restored X server key autorepeat to: %d\n",
		    save_auto_repeat);
	} else {
		global_auto_repeat = get_autorepeat_state();
		save_auto_repeat = global_auto_repeat;

		X_LOCK;
		kctrl.auto_repeat_mode = AutoRepeatModeOff;
		XChangeKeyboardControl(dpy, KBAutoRepeatMode, &kctrl);
		XFlush(dpy);
		X_UNLOCK;

		rfbLog("Disabled X server key autorepeat.\n");
		if (no_repeat_countdown >= 0) {
			rfbLog("  you can run the command: 'xset r on' (%d "
			    "times)\n", no_repeat_countdown+1);
			rfbLog("  to force it back on.\n");
		}
	}
}

static KeySym added_keysyms[0x100];

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
		int i, is_empty = 1;
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
			new[0] = keysym;
		} else {
			for(i=0; i < syms_per_keycode; i++) {
				new[i] = keysym;
				if (i >= 7) break;
			}
		}

		XChangeKeyboardMapping(dpy, kc, syms_per_keycode,
		    new, 1);

		str = XKeysymToString(keysym);
		rfbLog("added missing keysym to X display: %03d 0x%x \"%s\"\n",
		    kc, keysym, str ? str : "null");

		XFlush(dpy);
		added_keysyms[kc] = keysym;
		ret = kc;
		break;
	}
	XFree(keymap);
	return ret;
}

void delete_keycode(KeyCode kc) {
	int minkey, maxkey, syms_per_keycode, i;
	KeySym *keymap;
	KeySym ksym, new[8];
	char *str;

	XDisplayKeycodes(dpy, &minkey, &maxkey);
	keymap = XGetKeyboardMapping(dpy, minkey, (maxkey - minkey + 1),
	    &syms_per_keycode);

	for (i=0; i<8; i++) {
		new[i] = NoSymbol;
	}
	XChangeKeyboardMapping(dpy, kc, syms_per_keycode, new, 1);

	ksym = XKeycodeToKeysym(dpy, kc, 0);
	str = XKeysymToString(ksym);
	rfbLog("deleted keycode from X display: %03d 0x%x \"%s\"\n",
	    kc, ksym, str ? str : "null");

	XFree(keymap);
	XFlush(dpy);
}

void delete_added_keycodes(void) {
	int kc;
	for (kc = 0; kc < 0x100; kc++) {
		if (added_keysyms[kc] != NoSymbol) {
			delete_keycode(kc);
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

/*
 * process the -remap string (file or mapping string)
 */
void initialize_remap(char *infile) {
	FILE *in;
	char *p, *q, line[256], str1[256], str2[256];
	int i;
	KeySym ksym1, ksym2;
	keyremap_t *remap, *current;

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
		int isbtn = 0;
		p = lblanks(line);
		if (*p == '\0') {
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

#if !LIBVNCSERVER_HAVE_XKEYBOARD

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
				    "need it to be: %d %s\n", i, bitprint(b, 8),
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
						    "(%s)\n", bitprint(b,8), kc,
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

/*
 * key event handler.  See the above functions for contortions for
 * running under -modtweak.
 */
static rfbClientPtr last_keyboard_client = NULL;

void keyboard(rfbBool down, rfbKeySym keysym, rfbClientPtr client) {
	KeyCode k;
	int isbutton = 0;
	allowed_input_t input;

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
	get_allowed_input(client, &input);
	if (! input.keystroke) {
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
		XTestFakeButtonEvent_wr(dpy, button, True, CurrentTime);
		XTestFakeButtonEvent_wr(dpy, button, False, CurrentTime);
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
 * Send a pointer position event to the X server.
 */
static void update_x11_pointer_position(int x, int y) {

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

	cursor_x = x;
	cursor_y = y;

	/* record the x, y position for the rfb screen as well. */
	cursor_position(x, y);

	/* change the cursor shape if necessary */
	set_cursor(x, y, get_which_cursor());

	last_event = last_input = time(0);

	if (nofb) {
		/* 
		 * nofb is for, e.g. Win2VNC, where fastest pointer
		 * updates are desired.
		 */
		X_LOCK;
		XFlush(dpy);
		X_UNLOCK;
	}
}

/*
 * Send a pointer button event to the X server.
 */
static void update_x11_pointer_mask(int mask) {
	int i, mb;

	last_event = last_input = time(0);

	X_LOCK;
	/* look for buttons that have be clicked or released: */
	for (i=0; i < MAX_BUTTONS; i++) {
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
				XTestFakeButtonEvent_wr(dpy, mb, (mask & (1<<i))
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
 * This may queue pointer events rather than sending them immediately
 * to the X server. (see update_x11_pointer*())
 */
void pointer(int mask, int x, int y, rfbClientPtr client) {
	allowed_input_t input;

	if (debug_pointer && mask >= 0) {
		static int show_motion = -1;
		if (show_motion == -1) {
			if (getenv("X11VNC_DB_NOMOTION")) {
				show_motion = 0;
			} else {
				show_motion = 1;
			}
		}
		if (show_motion) {
			rfbLog("pointer(mask: 0x%x, x:%4d, y:%4d)\n",
			    mask, x, y);
		}
	}

	if (view_only) {
		return;
	}
	get_allowed_input(client, &input);
	if (! input.motion && ! input.button) {
		return;
	}
	if (scaling) {
		/* map from rfb size to X11 size: */
		x = ((double) x / scaled_x) * dpy_x;
		x = nfix(x, dpy_x);
		y = ((double) y / scaled_y) * dpy_y;
		y = nfix(y, dpy_y);
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
	if (use_threads && pointer_mode != 1) {
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
				if (! input.button) {
					ev[i][0] = -1;
				}
				if (! input.motion) {
					ev[i][1] = -1;
					ev[i][2] = -1;
				}
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
			if (ev[i][1] >= 0) {
				update_x11_pointer_position(ev[i][1], ev[i][2]);
			}
			if (ev[i][0] >= 0) {
				update_x11_pointer_mask(ev[i][0]);
			}
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
	if (input.motion) {
		update_x11_pointer_position(x, y);
	}
	if (input.button) {
		update_x11_pointer_mask(mask);
	}
}

/* -- xkb_bell.c -- */
/*
 * Bell event handling.  Requires XKEYBOARD extension.
 */
#if LIBVNCSERVER_HAVE_XKEYBOARD

static int xkb_base_event_type;

/*
 * check for XKEYBOARD, set up xkb_base_event_type
 */
void initialize_xkb(void) {
	int ir, reason;
	int op, ev, er, maj, min;
	
	if (! XkbQueryExtension(dpy, &op, &ev, &er, &maj, &min)) {
		if (! quiet && use_xkb) {
			rfbLog("warning: XKEYBOARD extension not present.\n");
		}
		xkb_present = 0;
		use_xkb = 0;
		return;
	} else {
		xkb_present = 1;
	}
	if (! use_xkb) {
		return;
	}
	if (! XkbOpenDisplay(DisplayString(dpy), &xkb_base_event_type, &ir,
	    NULL, NULL, &reason) ) {
		if (! quiet) {
			rfbLog("warning: disabling XKEYBOARD. XkbOpenDisplay"
			    " failed.\n");
		}
		use_xkb = 0;
	}
}

void initialize_watch_bell(void) {
	if (! use_xkb) {
		if (! quiet) {
			rfbLog("warning: disabling bell. XKEYBOARD ext. "
			    "not present.\n");
		}
		watch_bell = 0;
		return;
	}
	if (! XkbSelectEvents(dpy, XkbUseCoreKbd, XkbBellNotifyMask,
	    XkbBellNotifyMask) ) {
		if (! quiet) {
			rfbLog("warning: disabling bell. XkbSelectEvents"
			    " failed.\n");
		}
		watch_bell = 0;
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

	if (! watch_bell) {
		return;
	}

	X_LOCK;
	if (! XCheckTypedEvent(dpy, xkb_base_event_type, &xev)) {
		X_UNLOCK;
		return;
	}
	xkb_ev = (XkbAnyEvent *) &xev;
	if (xkb_ev->xkb_type == XkbBellNotify) {
		got_bell = 1;
	}
	X_UNLOCK;

	if (got_bell && sound_bell) {
		if (! all_clients_initialized()) {
			rfbLog("check_bell_event: not sending bell: "
			    "uninitialized clients\n");
		} else {
			if (screen) {
				rfbSendBell(screen);
			}
		}
	}
}
#else
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

		xrandr_width  = XDisplayWidth(dpy, scr);
		xrandr_height = XDisplayHeight(dpy, scr);
		XRRRotations(dpy, scr, &rot);
		xrandr_rotation = (int) rot;
		if (xrandr) {
			XRRSelectInput(dpy, rootwin, RRScreenChangeNotifyMask);
		} else {
			XRRSelectInput(dpy, rootwin, 0);
		}
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
	if (! valid_window(subwin)) {
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
	if (XCheckTypedEvent(dpy, xrandr_base_event_type +
	    RRScreenChangeNotify, &xev)) {
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

	X_LOCK;
	if ((watch_selection || vnc_connect) && !did_xselect_input) {
		/*
		 * register desired event(s) for notification.
		 * PropertyChangeMask is for CUT_BUFFER0 changes.
		 * XXX: does this cause a flood of other stuff?
		 */
		XSelectInput(dpy, rootwin, PropertyChangeMask);
		did_xselect_input = 1;
	}
	if (watch_selection && !did_xcreate_simple_window) {
		/* create fake window for our selection ownership, etc */

		selwin = XCreateSimpleWindow(dpy, rootwin, 0, 0, 1, 1, 0, 0, 0);
		did_xcreate_simple_window = 1;
	}
	X_UNLOCK;

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

/*
 * This routine is periodically called to check for selection related
 * and other X11 events and respond to them as needed.
 */
void check_xevents(void) {
	XEvent xev;
	static int first = 1, sent_some_sel = 0;
	static time_t last_request = 0;
	time_t now = time(0);
	int have_clients = 0;


	if (first) {
		initialize_xevents();
	}
	first = 0;

	if (screen && screen->clientHead) {
		have_clients = 1;
	}
	X_LOCK;
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

	if (no_autorepeat && client_count && no_repeat_countdown) {
		static time_t last_check = 0;
		if (now > last_check + 1) {
			last_check = now;
			X_UNLOCK;
			if (get_autorepeat_state() != 0) {
				int n = no_repeat_countdown - 1;
				if (n >= 0) {
					rfbLog("Battling with something for "
					    "-norepeat!! (%d resets left)\n",n);
				} else {
					rfbLog("Battling with something for "
					    "-norepeat!!\n");
				}
				if (no_repeat_countdown > 0) {
					no_repeat_countdown--;
				}
				autorepeat(1);
				autorepeat(0);
			}
			X_LOCK;
		}
	}

	if (XCheckTypedEvent(dpy, MappingNotify, &xev)) {
		XRefreshKeyboardMapping((XMappingEvent *) &xev);
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

#if LIBVNCSERVER_HAVE_LIBXRANDR
	if (xrandr) {
		check_xrandr_event("check_xevents");
	}
#endif
#if LIBVNCSERVER_HAVE_LIBXFIXES
	if (XCheckTypedEvent(dpy, xfixes_base_event_type +
	    XFixesCursorNotify, &xev)) {
		got_xfixes_cursor_notify++;
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
	X_UNLOCK;
}

/*
 * hook called when a VNC client sends us some "XCut" text (rfbClientCutText).
 */
void xcut_receive(char *text, int len, rfbClientPtr cl) {
	allowed_input_t input;

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
			rfbLog("check_httpdir: guessed: %s\n", httpdir);
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
			rfbLog("check_httpdir: bad guess: %s\n", httpdir);
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
		screen->socketInitDone = FALSE;

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
 * Huge, ugly switch to handle all remote commands and queries
 * -remote/-R and -query/-Q.
 */
char *process_remote_cmd(char *cmd, int stringonly) {
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
 * Add: passwdfile logfile bg nofb rfbauth passwd noshm...
 */
	if (!strcmp(p, "stop") || !strcmp(p, "quit") ||
	    !strcmp(p, "exit") || !strcmp(p, "shutdown")) {
		NOTAPP
		close_all_clients();
		rfbLog("process_remote_cmd: setting shut_down flag\n");
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
		refresh_screen();
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
			if (twin && ! valid_window(twin)) {
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
			if (twin && ! valid_window(twin)) {
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

	} else if (strstr(p, "clip") == p) {
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
		rfbLog("process_remote_cmd: turning on flashcmap mode.\n");
		flash_cmap = 1;
	} else if (!strcmp(p, "noflashcmap")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !flash_cmap);
			goto qry;
		}
		rfbLog("process_remote_cmd: turning off flashcmap mode.\n");
		flash_cmap = 0;
		
	} else if (!strcmp(p, "truecolor")) {
		int orig = force_indexed_color;
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    !force_indexed_color);
			goto qry;
		}
		rfbLog("process_remote_cmd: turning off notruecolor mode.\n");
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
		rfbLog("process_remote_cmd: turning on notruecolor mode.\n");
		force_indexed_color = 1;
		if (orig != force_indexed_color) {
			if_8bpp_do_new_fb();
		}
		
	} else if (!strcmp(p, "overlay")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, overlay);
			goto qry;
		}
		rfbLog("process_remote_cmd: turning on -overlay mode.\n");
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
		rfbLog("process_remote_cmd: turning off overlay mode\n");
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
		rfbLog("process_remote_cmd: turning on overlay_cursor mode.\n");
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
		rfbLog("process_remote_cmd: turning off overlay_cursor mode\n");
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
		rfbLog("process_remote_cmd: enable viewonly mode.\n");
		view_only = 1;
	} else if (!strcmp(p, "noviewonly")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !view_only);
			goto qry;
		}
		rfbLog("process_remote_cmd: disable viewonly mode.\n");
		view_only = 0;

	} else if (!strcmp(p, "shared")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, shared); goto qry;
		}
		rfbLog("process_remote_cmd: enable sharing.\n");
		shared = 1;
		if (screen) {
			screen->alwaysShared = TRUE;
			screen->neverShared = FALSE;
		}
	} else if (!strcmp(p, "noshared")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !shared); goto qry;
		}
		rfbLog("process_remote_cmd: disable sharing.\n");
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
		rfbLog("process_remote_cmd: enable -forever mode.\n");
		connect_once = 0;
	} else if (!strcmp(p, "noforever") || !strcmp(p, "once")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, connect_once);
			goto qry;
		}
		rfbLog("process_remote_cmd: disable -forever mode.\n");
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
		rfbLog("process_remote_cmd: set -timeout to %d\n", -to);

	} else if (!strcmp(p, "deny") || !strcmp(p, "lock")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, deny_all);
			goto qry;
		}
		rfbLog("process_remote_cmd: denying new connections.\n");
		deny_all = 1;
	} else if (!strcmp(p, "nodeny") || !strcmp(p, "unlock")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !deny_all);
			goto qry;
		}
		rfbLog("process_remote_cmd: allowing new connections.\n");
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
		rfbLog("process_remote_cmd: set allow_once %s\n", allow_once);

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
			rfbLog("process_remote_cmd: cannot use allow:host\n");
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
			rfbLog("process_remote_cmd: modified allow_list:\n");
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
			rfbLog("process_remote_cmd: modified allow_list:\n");
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
			rfbLog("process_remote_cmd: modified allow_list:\n");
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
			rfbLog("process_remote_cmd: modified listen_str:\n");
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
				if (!(hp = gethostbyname(listen_str))) {
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
		rfbLog("process_remote_cmd: turning off noshm mode.\n");
		using_shm = 1;
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
		rfbLog("process_remote_cmd: turning on noshm mode.\n");
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
		rfbLog("process_remote_cmd: turning on flipbyteorder mode.\n");
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
		rfbLog("process_remote_cmd: turning off flipbyteorder mode.\n");
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
		rfbLog("process_remote_cmd: enable -onetile mode.\n");
		single_copytile = 1;
	} else if (!strcmp(p, "noonetile")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !single_copytile);
			goto qry;
		}
		rfbLog("process_remote_cmd: disable -onetile mode.\n");
		if (tile_shm_count < ntiles_x) {
			rfbLog(" this has no effect: tile_shm_count=%d"
			    " ntiles_x=%d\n", tile_shm_count, ntiles_x);
			
		}
		single_copytile = 0;

	} else if (strstr(p, "solid_color") == p) {
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
		rfbLog("process_remote_cmd: solid %s -> %s\n",
		    NONUL(solid_str), new);

		if (solid_str) {
			if (!strcmp(solid_str, new)) {
				doit = 0;
			}
			free(solid_str);
		}
		solid_str = new;
		use_solid_bg = 1;

		if (doit && client_count) {
			solid_bg(0);
		}
	} else if (!strcmp(p, "solid")) {
		int orig = use_solid_bg;
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, use_solid_bg);
			goto qry;
		}
		rfbLog("process_remote_cmd: enable -solid mode\n");
		if (! solid_str) {
			solid_str = strdup(solid_default);
		}
		use_solid_bg = 1;
		if (client_count && !orig) {
			solid_bg(0);
		}
	} else if (!strcmp(p, "nosolid")) {
		int orig = use_solid_bg;
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !use_solid_bg);
			goto qry;
		}
		rfbLog("process_remote_cmd: disable -solid mode\n");
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
			rfbLog("process_remote_cmd: changing -blackout\n");
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
		rfbLog("process_remote_cmd: enable xinerama mode."
		    "(if applicable).\n");
		xinerama = 1;
		initialize_blackouts_and_xinerama();
	} else if (!strcmp(p, "noxinerama")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !xinerama);
			goto qry;
		}
		rfbLog("process_remote_cmd: disable xinerama mode."
		    "(if applicable).\n");
		xinerama = 0;
		initialize_blackouts_and_xinerama();

	} else if (!strcmp(p, "xrandr")) {
		int orig = xrandr;
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, xrandr); goto qry;
		}
		if (xrandr_present) {
			rfbLog("process_remote_cmd: enable xrandr mode.\n");
			xrandr = 1;
			if (! xrandr_mode) {
				xrandr_mode = strdup("default");
			}
			if (orig != xrandr) {
				initialize_xrandr();
			}
		} else {
			rfbLog("process_remote_cmd: XRANDR ext. not "
			    "present.\n");
		}
	} else if (!strcmp(p, "noxrandr")) {
		int orig = xrandr;
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !xrandr); goto qry;
		}
		xrandr = 0;
		if (xrandr_present) {
			rfbLog("process_remote_cmd: disable xrandr mode.\n");
			if (orig != xrandr) {
				initialize_xrandr();
			}
		} else {
			rfbLog("process_remote_cmd: XRANDR ext. not "
			    "present.\n");
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
				rfbLog("process_remote_cmd: enable xrandr"
				    " mode.\n");
			} else {
				rfbLog("process_remote_cmd: disable xrandr"
				    " mode.\n");
			}
			if (! xrandr_mode) {
				xrandr_mode = strdup("default");
			}
			if (orig != xrandr) {
				initialize_xrandr();
			}
		} else {
			rfbLog("process_remote_cmd: XRANDR ext. not "
			    "present.\n");
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
			rfbLog("process_remote_cmd: invoking "
			    "install_padded_fb()\n");
			install_padded_fb(pad_geometry);
		} else {
			if (pad_geometry) free(pad_geometry);
			pad_geometry = strdup(p);
			rfbLog("process_remote_cmd: set padgeom to: %s\n",
			    pad_geometry);
		}

	} else if (!strcmp(p, "quiet") || !strcmp(p, "q")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, quiet); goto qry;
		}
		rfbLog("process_remote_cmd: turning on quiet mode.\n");
		quiet = 1;
	} else if (!strcmp(p, "noquiet")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !quiet); goto qry;
		}
		rfbLog("process_remote_cmd: turning off quiet mode.\n");
		quiet = 0;
		
	} else if (!strcmp(p, "modtweak")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, use_modifier_tweak);
			goto qry;
		}
		rfbLog("process_remote_cmd: enabling -modtweak mode.\n");
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
		rfbLog("process_remote_cmd: enabling -nomodtweak mode.\n");
		use_modifier_tweak = 0;

	} else if (!strcmp(p, "xkb")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, use_xkb_modtweak);
			goto qry;
		}
		if (! xkb_present) {
			rfbLog("process_remote_cmd: cannot enable -xkb "
			    "modtweak mode (not supported on X display)\n");
			goto done;
		}
		rfbLog("process_remote_cmd: enabling -xkb modtweak mode"
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
			rfbLog("process_remote_cmd: cannot disable -xkb "
			    "modtweak mode (not supported on X display)\n");
			goto done;
		}
		rfbLog("process_remote_cmd: disabling -xkb modtweak mode.\n");
		use_xkb_modtweak = 0;

	} else if (strstr(p, "skip_keycodes") == p) {
		COLON_CHECK("skip_keycodes:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%s", p, co,
			    NONUL(skip_keycodes));
			goto qry;
		}
		p += strlen("skip_keycodes:");
		rfbLog("process_remote_cmd: setting xkb -skip_keycodes"
		    " to:\n\t'%s'\n", p);
		if (! xkb_present) {
			rfbLog("process_remote_cmd: warning xkb not present\n");
		} else if (! use_xkb_modtweak) {
			rfbLog("process_remote_cmd: turning on xkb.\n");
			use_xkb_modtweak = 1;
			if (! use_modifier_tweak) {
				rfbLog("process_remote_cmd: turning on "
				    "modtweak.\n");
				use_modifier_tweak = 1;
			}
		}
		if (skip_keycodes) free(skip_keycodes);
		skip_keycodes = strdup(p);
		initialize_modtweak();

	} else if (!strcmp(p, "add_keysyms")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, add_keysyms);
			goto qry;
		}
		rfbLog("process_remote_cmd: enabling -add_keysyms mode.\n");
		add_keysyms = 1;

	} else if (!strcmp(p, "noadd_keysyms")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !add_keysyms);
			goto qry;
		}
		rfbLog("process_remote_cmd: disabling -add_keysyms mode.\n");
		add_keysyms = 0;

	} else if (!strcmp(p, "clear_mods")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, clear_mods == 1);
			goto qry;
		}
		rfbLog("process_remote_cmd: enabling -clear_mods mode.\n");
		clear_mods = 1;
		clear_modifiers(0);

	} else if (!strcmp(p, "noclear_mods")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    !(clear_mods == 1));
			goto qry;
		}
		rfbLog("process_remote_cmd: disabling -clear_mods mode.\n");
		clear_mods = 0;

	} else if (!strcmp(p, "clear_keys")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    clear_mods == 2);
			goto qry;
		}
		rfbLog("process_remote_cmd: enabling -clear_keys mode.\n");
		clear_mods = 2;
		clear_keys();

	} else if (!strcmp(p, "noclear_keys")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    !(clear_mods == 2));
			goto qry;
		}
		rfbLog("process_remote_cmd: disabling -clear_keys mode.\n");
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
			rfbLog("process_remote_cmd: cannot use remap:+/-\n");
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
			rfbLog("process_remote_cmd: changed -remap\n");
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
		rfbLog("process_remote_cmd: enabling -repeat mode.\n");
		autorepeat(1);		/* restore initial setting */
		no_autorepeat = 0;

	} else if (!strcmp(p, "norepeat")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, no_autorepeat);
			goto qry;
		}
		rfbLog("process_remote_cmd: enabling -norepeat mode.\n");
		no_autorepeat = 1;
		if (no_repeat_countdown >= 0) {
			no_repeat_countdown = 2;
		}
		if (client_count) {
			autorepeat(0);	/* disable if any clients */
		}

	} else if (!strcmp(p, "fb")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !nofb);
			goto qry;
		}
		if (nofb) {
			rfbLog("process_remote_cmd: disabling nofb mode.\n");
			nofb = 0;
			do_new_fb(1);
		}
	} else if (!strcmp(p, "nofb")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, nofb);
			goto qry;
		}
		if (!nofb) {
			rfbLog("process_remote_cmd: enabling nofb mode.\n");
			if (main_fb) {
				push_black_screen(4);
			}
			nofb = 1;
		}

	} else if (!strcmp(p, "bell")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, sound_bell);
			goto qry;
		}
		rfbLog("process_remote_cmd: enabling bell (if supported).\n");
		sound_bell = 1;

	} else if (!strcmp(p, "nobell")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !sound_bell);
			goto qry;
		}
		rfbLog("process_remote_cmd: disabling bell.\n");
		sound_bell = 0;

	} else if (!strcmp(p, "sel")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, watch_selection);
			goto qry;
		}
		rfbLog("process_remote_cmd: enabling watch_selection.\n");
		watch_selection = 1;

	} else if (!strcmp(p, "nosel")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !watch_selection);
			goto qry;
		}
		rfbLog("process_remote_cmd: disabling watch_selection.\n");
		watch_selection = 0;

	} else if (!strcmp(p, "primary")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, watch_primary);
			goto qry;
		}
		rfbLog("process_remote_cmd: enabling watch_primary.\n");
		watch_primary = 1;

	} else if (!strcmp(p, "noprimary")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !watch_primary);
			goto qry;
		}
		rfbLog("process_remote_cmd: disabling watch_primary.\n");
		watch_primary = 0;

	} else if (!strcmp(p, "set_no_cursor")) { /* skip-cmd-list */
		rfbLog("process_remote_cmd: calling set_no_cursor()\n");
		set_no_cursor();

	} else if (!strcmp(p, "cursorshape")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    cursor_shape_updates);
			goto qry;
		}
		rfbLog("process_remote_cmd: turning on cursorshape mode.\n");

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
		rfbLog("process_remote_cmd: turning off cursorshape mode.\n");
		
		set_no_cursor();
		for (i=0; i<max; i++) {
			/* XXX: try to force empty cursor back to client */
			rfbPE(screen, -1);
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
		rfbLog("process_remote_cmd: turning on cursorpos mode.\n");
		cursor_pos_updates = 1;
	} else if (!strcmp(p, "nocursorpos")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    !cursor_pos_updates);
			goto qry;
		}
		rfbLog("process_remote_cmd: turning off cursorpos mode.\n");
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

		rfbLog("process_remote_cmd: changed -cursor mode "
		    "to: %s\n", multiple_cursors_mode);

		if (strcmp(multiple_cursors_mode, "none") && !show_cursor) {
			show_cursor = 1;
			rfbLog("process_remote_cmd: changed show_cursor "
			    "to: %d\n", show_cursor);
		}
		initialize_cursors_mode();
		first_cursor();

	} else if (!strcmp(p, "show_cursor")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, show_cursor);
			goto qry;
		}
		rfbLog("process_remote_cmd: enabling show_cursor.\n");
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
			rfbLog("process_remote_cmd: changed -cursor mode "
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

		rfbLog("process_remote_cmd: disabling show_cursor.\n");
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
		rfbLog("process_remote_cmd: setting alt_arrow: %d.\n",
		    alt_arrow);
		setup_cursors_and_push();

	} else if (!strcmp(p, "xfixes")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, use_xfixes);
			goto qry;
		}
		if (! xfixes_present) {
			rfbLog("process_remote_cmd: cannot enable xfixes "
			    "(not supported on X display)\n");
			goto done;
		}
		rfbLog("process_remote_cmd: enabling -xfixes"
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
			rfbLog("process_remote_cmd: disabling xfixes  "
			    "(but not supported on X display)\n");
			goto done;
		}
		rfbLog("process_remote_cmd: disabling -xfixes.\n");
		use_xfixes = 0;
		initialize_xfixes();
		first_cursor();

	} else if (!strcmp(p, "xdamage")) {
		int orig = use_xdamage_hints;
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, use_xdamage_hints);
			goto qry;
		}
		if (! xdamage_present) {
			rfbLog("process_remote_cmd: cannot enable xdamage hints "
			    "(not supported on X display)\n");
			goto done;
		}
		rfbLog("process_remote_cmd: enabling xdamage hints"
		    " (if supported).\n");
		use_xdamage_hints = 1;
		if (use_xdamage_hints != orig) {
			initialize_xdamage();
			create_xdamage();
		}
	} else if (!strcmp(p, "noxdamage")) {
		int orig = use_xdamage_hints;
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !use_xdamage_hints);
			goto qry;
		}
		if (! xdamage_present) {
			rfbLog("process_remote_cmd: disabling xdamage hints "
			    "(but not supported on X display)\n");
			goto done;
		}
		rfbLog("process_remote_cmd: disabling xdamage hints.\n");
		use_xdamage_hints = 0;
		if (use_xdamage_hints != orig) {
			initialize_xdamage();
			destroy_xdamage();
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
			rfbLog("process_remote_cmd: setting xdamage_max_area "
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
			rfbLog("process_remote_cmd: setting xdamage_memory "
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
			rfbLog("process_remote_cmd: setting alphacut "
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
			rfbLog("process_remote_cmd: setting alphafrac "
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
			rfbLog("process_remote_cmd: enable alpharemove\n");
			alpha_remove = 1;
			setup_cursors_and_push();
		}
	} else if (strstr(p, "noalpharemove") == p) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !alpha_remove);
			goto qry;
		}
		if (alpha_remove) {
			rfbLog("process_remote_cmd: disable alpharemove\n");
			alpha_remove = 0;
			setup_cursors_and_push();
		}
	} else if (strstr(p, "alphablend") == p) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, alpha_blend);
			goto qry;
		}
		if (!alpha_blend) {
			rfbLog("process_remote_cmd: enable alphablend\n");
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
			rfbLog("process_remote_cmd: disable alphablend\n");
			alpha_blend = 0;
			setup_cursors_and_push();
		}

	} else if (strstr(p, "xwarp") == p || strstr(p, "xwarppointer") == p) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, use_xwarppointer);
			goto qry;
		}
		rfbLog("process_remote_cmd: turning on xwarppointer mode.\n");
		use_xwarppointer = 1;
	} else if (strstr(p, "noxwarp") == p ||
		    strstr(p, "noxwarppointer") == p) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !use_xwarppointer);
			goto qry;
		}
		rfbLog("process_remote_cmd: turning off xwarppointer mode.\n");
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

		rfbLog("process_remote_cmd: setting -buttonmap to:\n"
		    "\t'%s'\n", p);
		initialize_pointer_map(p);

	} else if (!strcmp(p, "dragging")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, show_dragging);
			goto qry;
		}
		rfbLog("process_remote_cmd: enabling mouse dragging mode.\n");
		show_dragging = 1;
	} else if (!strcmp(p, "nodragging")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !show_dragging);
			goto qry;
		}
		rfbLog("process_remote_cmd: enabling mouse nodragging mode.\n");
		show_dragging = 0;

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
			rfbLog("process_remote_cmd: pointer_mode out of range:"
			   " 1-%d: %d\n", pointer_mode_max, pm);
		} else {
			rfbLog("process_remote_cmd: setting pointer_mode %d\n",
			    pm);
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
			rfbLog("process_remote_cmd: pointer_mode out of range:"
			   " 1-%d: %d\n", pointer_mode_max, pm);
		} else {
			rfbLog("process_remote_cmd: setting pointer_mode %d\n",
			    pm);
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
		rfbLog("process_remote_cmd: setting input_skip %d\n", is);
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
		rfbLog("process_remote_cmd: setting input %s\n", p);
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

		rfbLog("process_remote_cmd: setting -speeds to:\n"
		    "\t'%s'\n", p);
		initialize_speeds();

	} else if (!strcmp(p, "debug_pointer") || !strcmp(p, "dp")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, debug_pointer);
			goto qry;
		}
		rfbLog("process_remote_cmd: turning on debug_pointer.\n");
		debug_pointer = 1;
	} else if (!strcmp(p, "nodebug_pointer") || !strcmp(p, "nodp")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !debug_pointer);
			goto qry;
		}
		rfbLog("process_remote_cmd: turning off debug_pointer.\n");
		debug_pointer = 0;

	} else if (!strcmp(p, "debug_keyboard") || !strcmp(p, "dk")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, debug_keyboard);
			goto qry;
		}
		rfbLog("process_remote_cmd: turning on debug_keyboard.\n");
		debug_keyboard = 1;
	} else if (!strcmp(p, "nodebug_keyboard") || !strcmp(p, "nodk")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !debug_keyboard);
			goto qry;
		}
		rfbLog("process_remote_cmd: turning off debug_keyboard.\n");
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
		rfbLog("process_remote_cmd: setting defer to %d ms.\n", d);
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
		rfbLog("process_remote_cmd: setting defer to %d ms.\n", d);
		/* XXX not part of API? */
		screen->deferUpdateTime = d;
		
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
		rfbLog("process_remote_cmd: setting wait %d -> %d ms.\n",
		    waitms, w);
		waitms = w;

	} else if (strstr(p, "rfbwait") == p) {
		int w, orig = rfbMaxClientWait;
		COLON_CHECK("rfbwait:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co,
			    rfbMaxClientWait);
			goto qry;
		}
		p += strlen("rfbwait:");
		w = atoi(p);
		if (w < 0) w = 0;
		rfbLog("process_remote_cmd: setting rfbMaxClientWait %d -> "
		    "%d ms.\n", orig, w);
		rfbMaxClientWait = w;
		if (screen) {
			/* current unused by libvncserver: */
			screen->maxClientWait = w;
		}

	} else if (!strcmp(p, "nap")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, take_naps);
			    goto qry;
		}
		rfbLog("process_remote_cmd: turning on nap mode.\n");
		take_naps = 1;
	} else if (!strcmp(p, "nonap")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, !take_naps);
			    goto qry;
		}
		rfbLog("process_remote_cmd: turning off nap mode.\n");
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
		rfbLog("process_remote_cmd: setting screen_blank %d -> %d"
		    " sec.\n", screen_blank, w);
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
		rfbLog("process_remote_cmd: setting screen_blank %d -> %d"
		    " sec.\n", screen_blank, w);
		screen_blank = w;

	} else if (strstr(p, "fs") == p) {
		COLON_CHECK("fs:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%f", p, co, fs_frac);
			goto qry;
		}
		p += strlen("fs:");
		fs_frac = atof(p);
		rfbLog("process_remote_cmd: setting -fs frac to %f\n", fs_frac);

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
		rfbLog("process_remote_cmd: setting gaps_fill %d -> %d.\n",
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
		rfbLog("process_remote_cmd: setting grow_fill %d -> %d.\n",
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
		rfbLog("process_remote_cmd: setting tile_fuzz %d -> %d.\n",
		    tile_fuzz, f);
		grow_fill = f;

	} else if (!strcmp(p, "snapfb")) {
		int orig = use_snapfb;
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p, use_snapfb);
			    goto qry;
		}
		rfbLog("process_remote_cmd: turning on snapfb mode.\n");
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
		rfbLog("process_remote_cmd: turning off snapfb mode.\n");
		use_snapfb = 0;
		if (orig != use_snapfb) {
			do_new_fb(1);
		}

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
		rfbLog("process_remote_cmd: setting progressive %d -> %d.\n",
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

	} else if (strstr(p, "desktop") == p) {
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
		rfbLog("process_remote_cmd: setting desktop name to %s\n",
		    rfb_desktop_name);

	} else if (!strcmp(p, "noremote")) {
		if (query) {
			snprintf(buf, bufn, "ans=%s:%d", p,
			    !accept_remote_cmds);
			goto qry;
		}
		rfbLog("process_remote_cmd: disabling remote commands.\n");
		accept_remote_cmds = 0; /* cannot be turned back on. */

	} else if (strstr(p, "hack:") == p) { /* skip-cmd-list */
		COLON_CHECK("hack:")
		if (query) {
			snprintf(buf, bufn, "ans=%s%s%d", p, co, hack_val);
			goto qry;
		}
		p += strlen("hack:");
		hack_val = atoi(p);
		rfbLog("set hack_val to: %d\n", hack_val);

	} else if (query) {
		/* read-only variables that can only be queried: */

		if (!strcmp(p, "display")) {
			char *d = DisplayString(dpy);
			if (! d) d = "unknown";
			if (*d == ':') {
				snprintf(buf, bufn, "aro=%s:%s%s", p,
				    this_host(), d);
			} else {
				snprintf(buf, bufn, "aro=%s:%s", p, d);
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
		} else if (!strcmp(p, "scaling_noblend")) {
			snprintf(buf, bufn, "aro=%s:%d", p, scaling_noblend);
		} else if (!strcmp(p, "scaling_nomult4")) {
			snprintf(buf, bufn, "aro=%s:%d", p, scaling_nomult4);
		} else if (!strcmp(p, "scaling_pad")) {
			snprintf(buf, bufn, "aro=%s:%d", p, scaling_pad);
		} else if (!strcmp(p, "scaling_interpolate")) {
			snprintf(buf, bufn, "aro=%s:%d", p,
			    scaling_interpolate);
		} else if (!strcmp(p, "inetd")) {
			snprintf(buf, bufn, "aro=%s:%d", p, inetd);
		} else if (!strcmp(p, "safer")) {
			snprintf(buf, bufn, "aro=%s:%d", p, safe_remote_only);
		} else if (!strcmp(p, "unsafe")) {
			snprintf(buf, bufn, "aro=%s:%d", p, !safe_remote_only);
		} else if (!strcmp(p, "passwdfile")) {
			snprintf(buf, bufn, "aro=%s:%s", p, NONUL(passwdfile));
		} else if (!strcmp(p, "using_shm")) {
			snprintf(buf, bufn, "aro=%s:%d", p, !using_shm);
		} else if (!strcmp(p, "logfile") || !strcmp(p, "o")) {
			snprintf(buf, bufn, "aro=%s:%s", p, NONUL(logfile));
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
		rfbLog("process_remote_cmd: warning unknown\n");
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
		set_vnc_connect_prop(buf);
		XFlush(dpy);
	}
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

	if (xdamage_max_area <= 0) {
		always_accept = 1;
	}

	if (!always_accept && area > xdamage_max_area) {
		return;
	}

	dt_x = w / tile_x;
	dt_y = h / tile_y;

	if (!always_accept && dt_y >= 2 && area > 1000)  {
		/*
		 * should be caught by a normal scanline poll, but we might
		 * as well keep if small.
		 */
		return;
	}

	nt_x1 = nfix(  (x)/tile_x, ntiles_x);
	nt_x2 = nfix((x+w)/tile_x, ntiles_x);
	nt_y1 = nfix(  (y)/tile_y, ntiles_y);
	nt_y2 = nfix((y+h)/tile_y, ntiles_y);


	/* loop over the rectangle of tiles (1 tile for a small input rect */
	for (ix = nt_x1; ix <= nt_x2; ix++) {
		for (iy = nt_y1; iy <= nt_y2; iy++) {
			nt = ix + iy * ntiles_x;
			cnt++;
			if (! tile_has_xdamage_diff[nt]) {
				XD_des++;
			}
			tile_has_xdamage_diff[nt] = 1;
			tile_row_has_xdamage_diff[iy] = 1;
			xdamage_tile_count++;
		}
	}
}

void collect_xdamage(int scancnt) {
#if LIBVNCSERVER_HAVE_LIBXDAMAGE
	XDamageNotifyEvent *dev;
#if 0
	XserverRegion xregion;
#endif
	XEvent ev;
	sraRegionPtr tmpregion;
	sraRegionPtr reg;
	static int rect_count = 0;
	int nreg, ccount = 0, dcount = 0;
	static time_t last_rpt = 0;
	time_t now;
	int x, y, w, h, x2, y2;
	int i, dup, next, dup_max = 0;
#define DUPSZ 16
	int dup_x[DUPSZ], dup_y[DUPSZ], dup_w[DUPSZ], dup_h[DUPSZ];

	if (! xdamage_present || ! using_xdamage) {
		return;
	}
	if (! xdamage) {
		return;
	}

	nreg = (xdamage_memory * NSCAN) + 1;
	xdamage_ticker = (xdamage_ticker+1) % nreg;
	reg = xdamage_regions[xdamage_ticker];  
	sraRgnMakeEmpty(reg);

	X_LOCK;
	while (XCheckTypedEvent(dpy, xdamage_base_event_type+XDamageNotify, &ev)) {
		/* TODO max cut off time in this loop? */
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
				 * not needed because damage is relative
				 * to subwin, not rootwin.
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
			x2 = nfix(x2, dpy_x);
			w = x2 - x;		/* recompute w */
			
			y2 = y + h;
			y  = nfix(y,  dpy_y);
			y2 = nfix(y2, dpy_y);
			h = y2 - y;

			if (w <= 0 || h <= 0) {
				continue;
			}
		}


		record_desired_xdamage_rect(x, y, w, h);

		tmpregion = sraRgnCreateRect(x, y, x + w, y + h); 
		sraRgnOr(reg, tmpregion);
		sraRgnDestroy(tmpregion);
		rect_count++;
		ccount++;
	}
	/* clear the whole damage region for next time. XXX check */
	XDamageSubtract(dpy, xdamage, None, None);
	X_UNLOCK;

	now = time(0);
	if (! last_rpt) {
		last_rpt = now;
	}
	if (now > last_rpt + 15) {
		double rat = -1.0;

		if (XD_tot) {
			rat = ((double) XD_skip)/XD_tot;
		}
			
if (0) fprintf(stderr, "skip/tot: %04d/%04d  rat=%.3f  rect_count: %d  desired_rects: %d\n", XD_skip, XD_tot, rat, rect_count, XD_des);
		XD_skip = 0;
		XD_tot  = 0;
		XD_des  = 0;
		rect_count = 0;
		last_rpt = now;
	}
#endif
}

int xdamage_hint_skip(int y) {
	static sraRegionPtr scanline = NULL;
	sraRegionPtr reg, tmpl;
	int ret, i, n, nreg;

	if (!xdamage_present || !using_xdamage || !use_xdamage_hints) {
		return 0;	/* cannot skip */
	}
	if (! xdamage_regions) {
		return 0;	/* cannot skip */
	}

	if (! scanline) {
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

	using_xdamage = 0;
	if (xdamage_present) {
		if (use_xdamage_hints) {
			using_xdamage = 1;
		}
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
	if (using_xdamage) {
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

void create_xdamage(void) {
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

void destroy_xdamage(void) {
#if LIBVNCSERVER_HAVE_LIBXDAMAGE
	if (xdamage) {
		X_LOCK;
		XDamageDestroy(dpy, xdamage);
		X_UNLOCK;
		rfbLog("destroyed xdamage object: 0x%lx\n", xdamage);
		xdamage = 0;
	}
#endif
}

void check_xdamage_state(void) {
	if (! using_xdamage || ! xdamage_present) {
		return;
	}
	/*
	 * Create or destroy the Damage object as needed, we don't want
	 * one if no clients are connected.
	 */
	if (client_count) {
		create_xdamage();
	} else {
		destroy_xdamage();
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
		RFBUNDRAWCURSOR(screen);
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
	    &scaling_cursor_noblend, &j, &j, &scaling_cursor_interpolate,
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
				char d, m;
				int black = BlackPixel(dpy, scr);
				int white = WhitePixel(dpy, scr);

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
 * It seems impossible to do, but if the actual cursor could ever be
 * determined we might want to hash that info on window ID or something...
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
		if (use_xfixes) {
			XFixesSelectCursorInput(dpy, rootwin,
				XFixesDisplayCursorNotifyMask);
		} else {
			XFixesSelectCursorInput(dpy, rootwin, 0);
		}
	}
#endif
}

rfbCursorPtr pixels2curs(unsigned long *pixels, int w, int h,
    int xhot, int yhot, int Bpp) {
	rfbCursorPtr c;
	static unsigned long black, white;
	static int first = 1;
	char *bitmap, *rich, *alpha;
	char *new_pixels = NULL;
	int n_opaque, n_trans, n_alpha, len, histo[256];
	int send_alpha = 0, alpha_shift, thresh;
	int i, x, y;

	if (first) {
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

		scale_rect(scale_cursor_fac, scaling_cursor_noblend,
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

#if !OLD_TREE
	if (alpha_blend && !indexed_color) {
		c->alphaSource = alpha;
		c->alphaPreMultiplied = TRUE;
	} else {
		free(alpha);
		c->alphaSource = NULL;
	}
#endif

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

		if (! got_xfixes_cursor_notify) {
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

		RFBUNDRAWCURSOR(screen);

		/* we need to create the cursor and overwrite oldest */
		use = oldest;
		if (cursors[use]->rfb) {
			/* clean up oldest if it exists */
			if (cursors[use]->rfb->richSource) {
				free(cursors[use]->rfb->richSource);
			}
#if !OLD_TREE
			if (cursors[use]->rfb->alphaSource) {
				free(cursors[use]->rfb->alphaSource);
			}
#endif
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
				lowercase(winfo.res_name);
				lowercase(winfo.res_class);
				if (strstr(winfo.res_name, "term")) {
					which = CURS_TERM;
				} else if (strstr(winfo.res_class, "term")) {
					which = CURS_TERM;
				} else if (strstr(winfo.res_name, "text")) {
					which = CURS_TERM;
				} else if (strstr(winfo.res_class, "text")) {
					which = CURS_TERM;
				} else if (strstr(winfo.res_name, "onsole")) {
					which = CURS_TERM;
				} else if (strstr(winfo.res_class, "onsole")) {
					which = CURS_TERM;
				}
			}
		}
	}
	return which;
}

#if OLD_TREE
/*
 * Some utilities for marking the little cursor patch region as
 * modified, etc.
 */
void mark_cursor_patch_modified(rfbScreenInfoPtr s, int old) {
	int curx, cury, xhot, yhot, w, h;
	int x1, x2, y1, y2;

	if (! s || ! s->cursor) {
		return;
	}

	if (old) {
		/* use oldCursor pos */
		curx = s->oldCursorX;
		cury = s->oldCursorY;
	} else {
		curx = s->cursorX;
		cury = s->cursorY;
	}
	
	xhot = s->cursor->xhot;
	yhot = s->cursor->yhot;
	w = s->cursor->width;
	h = s->cursor->height;

	x1 = curx - xhot;
	x2 = x1 + w;
	x1 = nfix(x1, s->width);
	x2 = nfix(x2, s->width);

	y1 = cury - yhot;
	y2 = y1 + h;
	y1 = nfix(y1, s->height);
	y2 = nfix(y2, s->height);

	rfbMarkRectAsModified(s, x1, y1, x1+x2, y1+y2);
}
#endif

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
#if OLD_TREE
	int x_old, y_old;
#endif

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

#if OLD_TREE
	x_old = screen->oldCursorX;
	y_old = screen->oldCursorY;

	if (screen->cursorIsDrawn) {
		rfbUndrawCursor(screen);
	}

	LOCK(screen->cursorMutex);
	if (! screen->cursorIsDrawn) {
		screen->cursorX = x;
		screen->cursorY = y;
	}
	UNLOCK(screen->cursorMutex);
#else
	LOCK(screen->cursorMutex);
	screen->cursorX = x;
	screen->cursorY = y;
	UNLOCK(screen->cursorMutex);
#endif

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

#if OLD_TREE
	if (nonCursorPosUpdates_clients && show_cursor) {
		if (x_old != x || y_old != y) {
			mark_cursor_patch_modified(screen, 0);
		}
	}
#endif

	if (debug_pointer && cnt) {
		rfbLog("cursor_position: sent position x=%3d y=%3d to %d"
		    " clients\n", x, y, cnt);
	}
}

#if !OLD_TREE
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

#else

void set_rfb_cursor(int which) {

	if (! show_cursor) {
		return;
	}
	if (! screen) {
		return;
	}
	
	if (screen->cursor) {
		int all_are_cursor_pos = 1;
		rfbClientIteratorPtr iter;
		rfbClientPtr cl;

		iter = rfbGetClientIterator(screen);
		while( (cl = rfbClientIteratorNext(iter)) ) {
			if (! cl->enableCursorPosUpdates) {
				all_are_cursor_pos = 0;
			}
			if (! cl->enableCursorShapeUpdates) {
				all_are_cursor_pos = 0;
			}
		}
		rfbReleaseClientIterator(iter);

		if (! all_are_cursor_pos) {
			mark_cursor_patch_modified(screen, 1);
		}
	}

	if (!cursors[which] || !cursors[which]->rfb) {
		rfbLog("non-existent cursor: which=%d\n", which);
		return;
	} else {
		rfbSetCursor(screen, cursors[which]->rfb, FALSE);
	}

	if (screen->underCursorBuffer == NULL &&
	    screen->underCursorBufferLen != 0) {
		LOCK(screen->cursorMutex);
		screen->underCursorBufferLen = 0;
		UNLOCK(screen->cursorMutex);
	}
	set_cursor_was_changed(screen);
}
#endif

void set_no_cursor(void) {
	RFBUNDRAWCURSOR(screen);
	set_rfb_cursor(CURS_EMPTY);
}

void set_cursor(int x, int y, int which) {
	static int last = -1;
	if (which < 0) {
		which = last;	
	}
	if (last < 0 || which != last) {
		set_rfb_cursor(which);
	}
	last = which;
}

/*
 * routine called periodically to update cursor aspects, this catches
 * warps and cursor shape changes. 
 */
void check_x11_pointer(void) {
	Window root_w, child_w;
	rfbBool ret;
	int root_x, root_y, win_x, win_y;
	int x, y;
	unsigned int mask;

	X_LOCK;
	ret = XQueryPointer(dpy, rootwin, &root_w, &child_w, &root_x, &root_y,
	    &win_x, &win_y, &mask);
	X_UNLOCK;

	if (! ret) {
		return;
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
	set_cursor(x, y, get_which_cursor());
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
		 * mentioned in xdpyinfo.  Looks OK... perhaps fortuitously.
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

/*
 * Presumably under -nofb the clients will never request the framebuffer.
 * However, we have gotten such a request... so let's just give them
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
	/* ignore xrandr */
	fb = XGetImage_wr(dpy, window, 0, 0, dpy_x, dpy_y, AllPlanes, ZPixmap);
	main_fb = fb->data;
	rfb_fb = main_fb;
	screen->frameBuffer = rfb_fb;
	loaded_fb = 1;
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

/*
 * initialize a fb for the X display
 */
XImage *initialize_xdisplay_fb(void) {
	XImage *fb;
	char *vis_str = visual_str;
	int try = 0, subwin_tries = 3;
	XErrorHandler old_handler;
	int subwin_bs;

	X_LOCK;
	if (subwin) {
		if (subwin_wait_mapped) {
			wait_until_mapped(subwin);
		}
		if (!valid_window((Window) subwin)) {
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
			fprintf(stderr, " Visual*:    0x%p\n", vinfo->visual);
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
		rfbLog("default visual ID: 0x%x\n",
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

		trapped_xerror = 0;
		old_handler = XSetErrorHandler(trap_xerror);
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

void parse_scale_string(char *str, double *factor, int *scaling, int *noblend,
    int *nomult4, int *pad, int *interpolate, int *numer, int *denom) {

	int m, n;
	char *p, *tstr;
	double f;

	*factor = 1.0;
	*scaling = 0;
	*noblend = 0;
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
			*noblend = 1;
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
	return len;
}

void setup_scaling(int *width_in, int *height_in) {
	int width  = *width_in;
	int height = *height_in;

	parse_scale_string(scale_str, &scale_fac, &scaling, &scaling_noblend,
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
	 * defaults.  We manually set them in painful detail
	 * below.
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

/*
 * This ifdef is a transient for source compatibility for people who download
 * the x11vnc.c file by itself and plop it down into their libvncserver tree.
 * Remove at some point.  BTW, this assumes no usage of earlier "0.7pre".
 */
#if OLD_TREE && defined(LIBVNCSERVER_VERSION)
	if (strcmp(LIBVNCSERVER_VERSION, "0.6"))
#endif
	{ 
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
	    && CellsOfScreen(ScreenOfDisplay(dpy, scr))) {
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
	} else {
		/* 
		 * general case, we call it truecolor, but could be direct
		 * color, static color, etc....
		 */
		if (! quiet) {
			rfbLog("X display %s is %dbpp depth=%d true "
			    "color\n", DisplayString(dpy), fb->bits_per_pixel,
			    fb->depth);
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
		fprintf(stderr, "\n");
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

	bpp   = screen->serverFormat.bitsPerPixel;
	depth = screen->serverFormat.depth;

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
			if (! valid_window(twin)) {
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
		X = nfix(X, dpy_x);
		Y = nfix(Y, dpy_y);
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
	 * to simplify things drop down to single copy mode, no vcr, etc...
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
		rfbPE(screen, -1);
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

void refresh_screen(void) {
	if (!screen) {
		return;
	}
	mark_rect_as_modified(0, 0, dpy_x, dpy_y, 1);
	rfbPE(screen, -1);
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
		    0, NULL, w, h, BitmapPad(dpy), 0);

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
	if (shm != NULL && shm->shmid != -1) {
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

void scale_rect(double factor, int noblend, int interpolate, int Bpp,
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
		if (noblend || ! shrink || interpolate) {
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
		if (noblend && factor > 1.0 && n) {
			mag_int = n;
		}
	}

	if (mark && factor > 1.0 && ! noblend) {
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
	if (mark && noblend && mag_int && Bpp != 3) {
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

			if (noblend && use_noblend_shortcut) {
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
			    } else if (noblend) {
				if (J != J1) {
					continue;
				}
				wy = 1.0;

				/* interpolation scheme: */
			    } else if (!shrink || interpolate) {
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
				} else if (noblend) {
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
				} else if (!shrink || interpolate) {
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
		 */
		scaling_noblend = 1;
	}

	scale_rect(scale_fac, scaling_noblend, scaling_interpolate, bpp/8,
	    main_fb, main_bytes_per_line, rfb_fb, rfb_bytes_per_line,
	    dpy_x, dpy_y, scaled_x, scaled_y, X1, Y1, X2, Y2, 1);
}

void mark_rect_as_modified(int x1, int y1, int x2, int y2, int force) {

	if (damage_time != 0) {
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
	double dt = 0.0;
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

	dtime(&dt);
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
	if (nap_ok && ! nap_in && using_xdamage) {
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
			rfbPE(screen, -1);
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

		if (using_xdamage) {
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
		if (using_xdamage) {
			collect_xdamage(scan_count);
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
			if (tile_has_xdamage_diff[i] == 1) {
				tile_has_xdamage_diff[i] = 2;
				tile_has_diff[i] = 1;
				tile_count++;
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

	hint_updates();	/* use krfb/x0rfbserver hints algorithm */

	/* Work around threaded rfbProcessClientMessage() calls timeouts */
	if (use_threads) {
		ping_clients(tile_diffs);
	}


	nap_check(tile_diffs);
	return tile_diffs;
}

/* -- gui.c -- */
#if OLD_TREE
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
		if (dpy)  {
			XCloseDisplay(dpy);
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
 * (this is for pointer_mode == 1 mode, the others do it all internally,
 * cnt is also only for that mode).
 */

static void check_user_input2(double dt) {

	int eaten = 0, miss = 0, max_eat = 50;
	int g, g_in;
	double spin = 0.0, tm = 0.0;
	double quick_spin_fac  = 0.40;
	double grind_spin_time = 0.175;



	dtime(&tm);
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
			rfbPE(screen, 1000);
		} else {
			rfbCFD(screen, 1000);
		}
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
		tm = 0.0;
		dtime(&tm);

		g = got_pointer_input;
		miss = 0;
		for (i=0; i<split; i++) {
			usleep(ms * 1000);
			if (show_multiple_cursors) {
				rfbPE(screen, 1000);
			} else {
				rfbCFD(screen, 1000);
			}
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

	tm = 0.0;		/* timer variable */
	dtime(&tm);
	to = tm;	/* last time we did rfbPE() */

	g = g_in = got_pointer_input;

	while (1) {
		int got_input = 0;

		gcnt++;

		if (button_mask) {
			drag_in_progress = 1;
		}

		rfbCFD(screen, rfb_wait_ms * 1000);

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
			rfbCFD(screen, rfb_wait_ms * 1000);
		}
	}

	drag_in_progress = 0;
}

int fb_update_sent(int *count) {
	static int last_count = 0;
	int sent = 0, rc = 0;
	rfbClientIteratorPtr i;
	rfbClientPtr cl;

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

	tm = 0.0;		/* timer variable */
	dtime(&tm);

	if (dt < dt_cut) {
		dt_use = dt_cut;
	} else {
		dt_use = dt;
	}

	if (push_frame) {
		int cnt, iter = 0;
		double tp = 0.0, push_spin = 0.0;
		dtime(&tp);
		while (push_spin < dt_use * 0.5) {
			fb_update_sent(&cnt);
			if (cnt != update_count) {
				break;
			}
			/* damn, they didn't push our frame! */
			iter++;
			rfbPE(screen, rfb_wait_ms * 1000);
			
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
		rfbCFD(screen, rfb_wait_ms * 1000);

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
			rfbCFD(screen, rfb_wait_ms * 1000);
		}
	}
	drag_in_progress = 0;
}

static int check_user_input(double dt, double dtr, int tile_diffs, int *cnt) {
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
	if (*t_old == 0) {
		*t_old = t_now;
		return t_now;
	}
	dt = t_now - *t_old;
	*t_old = t_now;
	return(dt);
}

void measure_display_hook(rfbClientPtr cl) {
	ClientData *cd = (ClientData *) cl->clientData;
	cd->timer = 0.0;
	dtime(&cd->timer);
}

void measure_send_rates_init(void) {
	int i, bs, rbs;
	rfbClientIteratorPtr iter;
	rfbClientPtr cl;

	screen->displayHook = measure_display_hook;

	iter = rfbGetClientIterator(screen);
	while( (cl = rfbClientIteratorNext(iter)) ) {
		ClientData *cd = (ClientData *) cl->clientData;
		bs = 0;
		for (i=0; i<MAX_ENCODINGS; i++) {
			bs += cl->bytesSent[i];
		}
		rbs = cl->rawBytesEquivalent;
		
		cd->set_cmp_bytes = bs;
		cd->set_raw_bytes = rbs;
		cd->timer = -1.0;
	}
	rfbReleaseClientIterator(iter);
}

int get_rate(int which) {
	rfbClientIteratorPtr iter;
	rfbClientPtr cl;
	int i, samples = RATE_SAMPLES;
	double dslowest = -1.0, dsum;
	
	iter = rfbGetClientIterator(screen);
	while( (cl = rfbClientIteratorNext(iter)) ) {
		ClientData *cd = (ClientData *) cl->clientData;

		dsum = 0.0;
		for (i=0; i<samples; i++) {
			if (which == 0) {
				dsum += cd->cmp_samp[i];
			} else {
				dsum += cd->raw_samp[i];
			}
		}
		dsum = dsum / samples;
		if (dsum > dslowest) {
			dslowest = dsum;
		}
		
	}
	rfbReleaseClientIterator(iter);

	if (dslowest < 0.0) {
		if (which == 0) {
			dslowest = 5000.0;
		} else {
			dslowest = 50000.0;
		}
	}
	return (int) dslowest;
}

int get_cmp_rate(void) {
	return get_rate(0);
}

int get_raw_rate(void) {
	return get_rate(1);
}

void initialize_speeds(void) {
	char *s, *p;
	int i;

	speeds_read_rate = 0;
	speeds_net_rate = 0;
	speeds_net_latency = 0;
	if (! speeds_str || *speeds_str == '\0') {
		return;
	}

	if (!strcmp(speeds_str, "modem")) {
		s = strdup("6,4,200");
	} else if (!strcmp(speeds_str, "dsl")) {
		s = strdup("6,100,50");
	} else if (!strcmp(speeds_str, "lan")) {
		s = strdup("6,5000,1");
	} else {
		s = strdup(speeds_str);
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
}

int get_read_rate(void) {
	if (speeds_read_rate) {
		return speeds_read_rate;
	}
	return 0;
}

int get_net_rate(void) {
	if (speeds_net_rate) {
		return speeds_net_rate;
	}
	return 0;
}

int get_net_latency(void) {
	if (speeds_net_latency) {
		return speeds_net_latency;
	}
	return 0;
}

void measure_send_rates(int init) {
	int i, j, nclient = 0;
	int min_width = 200;
	double dt, cmp_rate, raw_rate;
	rfbClientPtr id[100]; 
	double dts[100], dts_sorted[100], dtmp;
	int sorted[100], did[100], best;
	rfbClientIteratorPtr iter;
	rfbClientPtr cl;

	if (! measure_speeds) {
		return;
	}
	if (init) {
		measure_send_rates_init();
		return;
	}

	iter = rfbGetClientIterator(screen);
	while( (cl = rfbClientIteratorNext(iter)) ) {
		double tmp2;
		ClientData *cd = (ClientData *) cl->clientData;
		tmp2 = 0.0;
		dtime(&tmp2);
if (init) {
	continue;
}
		if (cd->timer <= 0.0) {
			continue;
		}
		dt = dtime(&cd->timer);
		cd->timer = dt;
		if (nclient < 100) {
			id[nclient] = cl;
			dts[nclient] = dt;
			nclient++;
		}
	}
	rfbReleaseClientIterator(iter);
if (init) {
	return;
}

	for (i=0; i<nclient; i++) {
		did[i] = 0;
	}
	for (i=0; i<nclient; i++) {
		dtmp = -1.0;
		best = -1;
		for (j=0; j<nclient; j++) {
			if (did[j]) {
				continue;
			}
			if (dts[j] > dtmp) {
				best = j;
				dtmp = dts[j];
			}
		} 
		did[best] = 1;
		sorted[i] = best;
		dts_sorted[i] = dts[best];
	}
	

	iter = rfbGetClientIterator(screen);
	while( (cl = rfbClientIteratorNext(iter)) ) {
		int db, dbr, cbs, rbs;
		ClientData *cd = (ClientData *) cl->clientData;

		dt = cd->timer;
		if (dt <= 0.0) {
			continue;
		}
		if (nclient > 1) {
			for (i=0; i<nclient; i++) {
				if (cl != id[i]) {
					continue;
				}
				for (j=0; j<nclient; j++) {
					if (sorted[j] == i) {
						if (j < nclient - 1) {
							dt -= dts_sorted[j+1];
						}
					}
				}
				break;
			}
		}
		if (dt <= 0.0) {
			continue;
		}

		cbs = 0;
		for (i=0; i<MAX_ENCODINGS; i++) {
			cbs += cl->bytesSent[i];
		}
		rbs = cl->rawBytesEquivalent;

		db  = cbs - cd->set_cmp_bytes;
		dbr = rbs - cd->set_raw_bytes;
		cmp_rate = db/dt;
		raw_rate = dbr/dt;
		if (dbr > min_width * min_width * bpp/8) {
			cd->sample++;
			if (cd->sample >= RATE_SAMPLES) {
				cd->sample = 0;
			}
			i = cd->sample;
			cd->cmp_samp[i] = cmp_rate;
			cd->raw_samp[i] = raw_rate;
		}
	}
	rfbReleaseClientIterator(iter);
}

/*
 * utility wrapper to call rfbProcessEvents
 * checks that we are not in threaded mode.
 */
void rfbPE(rfbScreenInfoPtr scr, long usec) {
	if (! scr) {
		return;
	}
	if (! use_threads) {
		rfbProcessEvents(scr, usec);
	}
}

void rfbCFD(rfbScreenInfoPtr scr, long usec) {
	if (! scr) {
		return;
	}
	if (! use_threads) {
		rfbCheckFds(scr, usec);
	}
}

/*
 * main x11vnc loop: polls, checks for events, iterate libvncserver, etc.
 */
static void watch_loop(void) {
	int cnt = 0, tile_diffs = 0;
	double dt = 0.0, dtr = 0.0;
	time_t start = time(0);

	if (use_threads) {
		rfbRunEventLoop(screen, -1, TRUE);
	}

	while (1) {

		got_user_input = 0;
		got_pointer_input = 0;
		got_keyboard_input = 0;

		if (! use_threads) {
			double tm = 0.0;
			dtime(&tm);
			rfbPE(screen, -1);
			dtr = dtime(&tm);
			fb_update_sent(NULL);

			if (! cursor_shape_updates) {
				/* undo any cursor shape requests */
				disable_cursor_shape_updates(screen);
			}
			if (screen && screen->clientHead && 
			    check_user_input(dt, dtr, tile_diffs, &cnt)) {
				/* true means loop back for more input */
				continue;
			}
		}

		if (shut_down) {
			clean_up_exit(0);
		}

		if (do_copy_screen) {
			do_copy_screen = 0;
			copy_screen();
		}

		check_new_clients();
		check_xevents();
		check_connect_inputs();		
		check_padded_fb();		
		if (started_as_root) {
			check_switched_user();
		}

		if (first_conn_timeout < 0) {
			start = time(0);
			first_conn_timeout = -first_conn_timeout;
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

		if (using_xdamage) {
			check_xdamage_state();
		}

		if (watch_bell) {
			/* n.b. assumes -nofb folks do not want bell... */
			check_bell_event();
		}

		if (button_mask && (!show_dragging || pointer_mode == 0)) {
			/*
			 * if any button is pressed do not update rfb
			 * screen, but do flush the X11 display.
			 */
			X_LOCK;
			XFlush(dpy);
			X_UNLOCK;
		} else {
			/* for timing the scan to try to detect thrashing */
			double tm = 0.0;
			dtime(&tm);

			RFBUNDRAWCURSOR(screen);
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
			check_x11_pointer();
		}

		/* sleep a bit to lessen load */
		usleep(waitms * 1000);
		cnt++;
	}
}

/*
 * text printed out under -help option
 */
static void print_help(int mode) {
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
"these protections.  See the FAQ on how to tunnel the VNC connection through\n"
"an encrypted channel such as ssh(1).\n"
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
"                       exactly, e.g. -scale 2/3.\n"
"\n"
"                       Scaling Options: can be added after \"fraction\" via\n"
"                       \":\", to supply multiple \":\" options use commas.\n"
"                       If you just want a quick, rough scaling without\n"
"                       blending, append \":nb\" to \"fraction\" (e.g. -scale\n"
"                       1/3:nb).  For compatibility with vncviewers the scaled\n"
"                       width is adjusted to be a multiple of 4: to disable\n"
"                       this use \":n4\".  More esoteric options: \":in\" use\n"
"                       interpolation scheme even when shrinking, \":pad\",\n"
"                       pad scaled width and height to be multiples of scaling\n"
"                       denominator (e.g. 3 for 2/3).\n"
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
"-rc filename           Use \"filename\" instead of $HOME/.x11vncrc for rc file.\n"
"-norc                  Do not process any .x11vncrc file for options.\n"
"-h, -help              Print this help text.\n"
"-?, -opts              Only list the x11vnc options.\n"
"-V, -version           Print program version and last modification date.\n"
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
"                       the X display supports it) to do the modifier tweaking.\n"
"                       This is powerful and should be tried if there are still\n"
"                       keymapping problems when using -modtweak by itself.\n"
"-skip_keycodes string  Ignore the comma separated list of decimal keycodes.\n"
"                       Perhaps these are keycodes not on your keyboard but\n"
"                       your X server thinks exist.  Currently only applies\n"
"                       to -xkb mode.  Use this option to help x11vnc in the\n"
"                       reverse problem it tries to solve: Keysym -> Keycode(s)\n"
"                       when ambiguities exist (more than one Keycode per\n"
"                       Keysym).  Run 'xmodmap -pk' to see your keymapping.\n"
"                       Example: \"-skip_keycodes 94,114\"\n"
"-add_keysyms           If a Keysym is received from a VNC viewer and\n"
"                       that Keysym does not exist in the X server, then\n"
"                       add the Keysym to the X server's keyboard mapping.\n"
"                       Added Keysyms will be removed when x11vnc exits.\n"
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
"-norepeat              Option -norepeat disables X server key auto repeat\n"
"-repeat                when VNC clients are connected.  This works around a\n"
"                       repeating keystrokes bug (triggered by long processing\n"
"                       delays between key down and key up client events:\n"
"                       either from large screen changes or high latency).\n"
"                       Note: your VNC viewer side will likely do autorepeating,\n"
"                       so this is no loss unless someone is simultaneously at\n"
"                       the real X display.  Default: %s\n"
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
"                       n=4: attempts to measures network rates and latency,\n"
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
"                       Typical PC video cards have read rates of 5-10 MB/sec.\n"
"                       If the framebuffer is in main memory instead of video\n"
"                       h/w (e.g. SunRay, shadowfb, Xvfb), the read rate may\n"
"                       be much faster.  \"x11perf -getimage500\" can be used\n"
"                       to get a lower bound (remember to factor in the bytes\n"
"                       per pixel).  It is up to you to estimate the network\n"
"                       bandwith to clients.  For the latency the ping(1)\n"
"                       command can be used.\n"
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
"-nap                   Monitor activity and if it is low take longer naps\n"
"-nonap                 between screen polls to really cut down load when idle.\n"
"                       Default: %s\n"
"-sb time               Time in seconds after NO activity (e.g. screen blank)\n"
"                       to really throttle down the screen polls (i.e. sleep\n"
"                       for about 1.5 secs). Use 0 to disable.  Default: %d\n"
"\n"
"-noxdamage             Do not use the X DAMAGE extension to detect framebuffer\n"
"                       changes even if it is available.\n"
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
"                                       e.g. 0x3 (returned by -query clients and\n"
"                                       RFB_CLIENT_ID), you can use that too.\n"
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
"                       pointer_mode:n  set -pointer_mode to n. same as \"pm\"\n"
"                       input_skip:n    set -input_skip to n.\n"
"                       speeds:str      set -speeds to str.\n"
"                       debug_pointer   enable  -debug_pointer, same as \"dp\"\n"
"                       nodebug_pointer disable -debug_pointer, same as \"nodp\"\n"
"                       debug_keyboard   enable  -debug_keyboard, same as \"dk\"\n"
"                       nodebug_keyboard disable -debug_keyboard, same as \"nodk\"\n"
"                       defer:n         set -defer to n ms,same as deferupdate:n\n"
"                       wait:n          set -wait to n ms.\n"
"                       rfbwait:n       set -rfbwait (rfbMaxClientWait) to n ms.\n"
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
"                       progressive:n   set libvncserver -progressive slice\n"
"                                       height parameter to n.\n"
"                       desktop:str     set -desktop name to str for new clients.\n"
"                       rfbport:n       set -rfbport to n.\n"
/* access */
"                       http            enable  http client connections.\n"
"                       nohttp          disable http client connections.\n"
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
"                       ans= stop quit exit shutdown ping blacken zero refresh\n"
"                       reset close disconnect id sid waitmapped nowaitmapped\n"
"                       clip flashcmap noflashcmap truecolor notruecolor\n"
"                       overlay nooverlay overlay_cursor overlay_yescursor\n"
"                       nooverlay_nocursor nooverlay_cursor nooverlay_yescursor\n"
"                       overlay_nocursor visual scale scale_cursor viewonly\n"
"                       noviewonly shared noshared forever noforever once\n"
"                       timeout deny lock nodeny unlock connect allowonce\n"
"                       allow localhost nolocalhost listen accept gone\n"
"                       shm noshm flipbyteorder noflipbyteorder onetile\n"
"                       noonetile solid_color solid nosolid blackout xinerama\n"
"                       noxinerama xrandr noxrandr xrandr_mode padgeom quiet\n"
"                       q noquiet modtweak nomodtweak xkb noxkb skip_keycodes\n"
"                       add_keysyms noadd_keysyms clear_mods noclear_mods\n"
"                       clear_keys noclear_keys remap repeat norepeat fb nofb\n"
"                       bell nobell sel nosel primary noprimary cursorshape\n"
"                       nocursorshape cursorpos nocursorpos cursor show_cursor\n"
"                       noshow_cursor nocursor arrow xfixes noxfixes xdamage\n"
"                       noxdamage xd_area xd_mem alphacut alphafrac alpharemove\n"
"                       noalpharemove alphablend noalphablend xwarp xwarppointer\n"
"                       noxwarp noxwarppointer buttonmap dragging nodragging\n"
"                       pointer_mode pm input_skip input client_input speeds\n"
"                       debug_pointer dp nodebug_pointer nodp debug_keyboard dk\n"
"                       nodebug_keyboard nodk deferupdate defer wait rfbwait\n"
"                       nap nonap sb screen_blank fs gaps grow fuzz snapfb\n"
"                       nosnapfb progressive rfbport http nohttp httpport\n"
"                       httpdir enablehttpproxy noenablehttpproxy alwaysshared\n"
"                       noalwaysshared nevershared noalwaysshared dontdisconnect\n"
"                       nodontdisconnect desktop noremote\n"
"\n"
"                       aro=  display vncdisplay desktopname http_url auth\n"
"                       users rootshift clipshift scale_str scaled_x scaled_y\n"
"                       scale_numer scale_denom scale_fac scaling_noblend\n"
"                       scaling_nomult4 scaling_pad scaling_interpolate inetd\n"
"                       safer unsafe passwdfile using_shm logfile o rc norc\n"
"                       h help V version lastmod bg sigpipe threads clients\n"
"                       client_count pid ext_xtest ext_xkb ext_xshm ext_xinerama\n"
"                       ext_overlay ext_xfixes ext_xdamage ext_xrandr rootwin\n"
"                       num_buttons button_mask mouse_x mouse_y bpp depth\n"
"                       indexed_color dpy_x dpy_y wdpy_x wdpy_y off_x off_y\n"
"                       cdpy_x cdpy_y coff_x coff_y rfbauth passwd\n"
"\n"
"-sync                  By default -remote commands are run asynchronously, that\n"
"                       is, the request is posted and the program immediately\n"
"                       exits.  Use -sync to have the program wait for an\n"
"                       acknowledgement from the x11vnc server that command\n"
"                       was processed.  On the other hand -query requests are\n"
"                       always processed synchronously because they have wait\n"
"                       for the result.\n"
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
"\n"
"                       A note about security wrt remote control commands.\n"
"                       If someone can connect to the X display and change the\n"
"                       property VNC_CONNECT, then they can remotely control\n"
"                       x11vnc.  Normally access to the X display is protected.\n"
"                       Note that if they can modify VNC_CONNECT, they could\n"
"                       also run their own x11vnc and have complete control\n"
"                       of the desktop.  If the  \"-connect /path/to/file\"\n"
"                       channel is being used, obviously anyone who can\n"
"                       write to /path/to/file can remotely control x11vnc.\n"
"                       So be sure to protect the X display and that file's\n"
"                       write permissions.\n"
"\n"
"                       To disable the VNC_CONNECT property channel completely\n"
"                       use -novncconnect.\n"
"\n"
"-unsafe                If x11vnc is running as root (e.g. inetd or Xsetup for\n"
"                       a display manager) a few remote commands are disabled\n"
"                       (currently: id:pick, accept:<cmd>, and gone:<cmd>)\n"
"                       because they are associated with running external\n"
"                       programs.  If you specify -unsafe, then these remote\n"
"                       control commands are allowed when running as root.\n"
"                       When running as non-root all commands are allowed.\n"
"                       See -safer below.\n"
"-safer                 Even if not running as root, disable the above unsafe\n"
"                       remote control commands.\n"
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
		no_autorepeat ? "-norepeat":"-repeat",
		alt_arrow_max, alt_arrow,
		alpha_threshold,
		alpha_frac,
		cursor_pos_updates ? "-cursorpos":"-nocursorpos",
		pointer_mode_max, pointer_mode,
		ui_skip,
		defer_update,
		waitms,
		take_naps ? "take naps":"no naps",
		screen_blank,
		xdamage_max_area, NSCAN, xdamage_memory,
		use_threads ? "-threads":"-nothreads",
		fs_frac,
		gaps_fill,
		grow_fill,
		tile_fuzz,
		""
	);

	rfbUsage();
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
			;	/* should not occur (port) */
		} else {
			fprintf(stdout, "PORT=%d\n", screen->port);
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
	if (subwin && valid_window(subwin)) {
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

	int i, len;
	int ev, er, maj, min;
	char *arg;
	int remote_sync = 0;
	char *remote_cmd = NULL;
	char *query_cmd  = NULL;
	char *gui_str = NULL;
	int pw_loc = -1;
	int vpw_loc = -1;
	int dt = 0, bg = 0;
	int got_rfbwait = 0, got_deferupdate = 0, got_defer = 0;
	int got_noxdamage = 0;

	/* used to pass args we do not know about to rfbGetScreen(): */
	int argc_vnc = 1; char *argv_vnc[128];

	/* if we are root limit some remote commands, etc: */
	if (!getuid() || !geteuid()) {
		safe_remote_only = 1;
		started_as_root = 1;

		/* check for '-users =bob' */
		immediate_switch_user(argc, argv);
	}

	argv_vnc[0] = strdup(argv[0]);
	program_name = strdup(argv[0]);

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
				if (safe_remote_only) {
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
		} else if (!strcmp(arg, "-isolevel3")) {
			use_iso_level3 = 1;
		} else if (!strcmp(arg, "-xkb")) {
			use_xkb_modtweak = 1;
		} else if (!strcmp(arg, "-xkbcompat")) {
			xkbcompat = 1;
		} else if (!strcmp(arg, "-skip_keycodes")) {
			CHECK_ARGC
			skip_keycodes = strdup(argv[++i]);
		} else if (!strcmp(arg, "-add_keysyms")) {
			add_keysyms++;
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
		} else if (!strcmp(arg, "-nosel")) {
			watch_selection = 0;
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
		} else if (!strcmp(arg, "-nocursorpos")) {
			cursor_pos_updates = 0;
		} else if (!strcmp(arg, "-xwarppointer")) {
			use_xwarppointer = 1;
		} else if (!strcmp(arg, "-buttonmap")) {
			CHECK_ARGC
			pointer_remap = strdup(argv[++i]);
		} else if (!strcmp(arg, "-nodragging")) {
			show_dragging = 0;
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
		} else if (!strcmp(arg, "-nap")) {
			take_naps = 1;
		} else if (!strcmp(arg, "-nonap")) {
			take_naps = 0;
		} else if (!strcmp(arg, "-sb")) {
			CHECK_ARGC
			screen_blank = atoi(argv[++i]);
		} else if (!strcmp(arg, "-noxdamage")) {
			using_xdamage = 0;
			use_xdamage_hints = 0;
			got_noxdamage = 1;
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
		} else if (!strcmp(arg, "-snapfb")) {
			use_snapfb = 1;
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
		    || !strcmp(arg, "-r")) {
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
		} else if (!strcmp(arg, "-unsafe")) {
			safe_remote_only = 0;
		} else if (!strcmp(arg, "-safer")) {
			safe_remote_only = 1;
		} else if (!strcmp(arg, "-deny_all")) {
			deny_all = 1;
		} else if (!strcmp(arg, "-httpdir")) {
			CHECK_ARGC
			http_dir = strdup(argv[++i]);
		} else {
			if (!strcmp(arg, "-desktop") && i < argc-1) {
				dt = 1;
				rfb_desktop_name = strdup(argv[i+1]);
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
	if (viewonly_passwd && pw_loc < 0) {
		rfbLog("-passwd must be supplied when using -viewpasswd\n");
		exit(1);
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

	/* increase rfbwait if threaded */
	if (use_threads && ! got_rfbwait) {
		if (0) {
			/* -rfbwait = rfbScreen->maxClientWait is not used */
			argv_vnc[argc_vnc++] = strdup("-rfbwait");
			argv_vnc[argc_vnc++] = strdup("604800000");
		} else {
			/* set the global in sockets.c instead: */
			rfbMaxClientWait = 604800000;
		}
	}

	/* no framebuffer (Win2VNC) mode */

	if (nofb) {
		/* disable things that do not make sense with no fb */
		using_shm = 0;
		flash_cmap = 0;
		show_cursor = 0;
		show_multiple_cursors = 0;
		overlay = 0;
		overlay_cursor = 0;
		if (! quiet) {
			rfbLog("disabling -cursor, fb, shm, etc. in "
			   "-nofb mode.\n");
		}

		if (! got_deferupdate && ! got_defer) {
			/* reduce defer time under -nofb */
			defer_update = defer_update_nofb;
		}
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
		fprintf(stderr, " xrandr:     %d\n", xrandr);
		fprintf(stderr, " xrandrmode: %s\n", xrandr_mode ? xrandr_mode
		    : "null");
		fprintf(stderr, " padgeom:    %s\n", pad_geometry
		    ? pad_geometry : "null");
		fprintf(stderr, " logfile:    %s\n", logfile ? logfile
                    : "null");
		fprintf(stderr, " logappend:  %d\n", logfile_append);
		fprintf(stderr, " rc_file:    \"%s\"\n", rc_rcfile ? rc_rcfile
                    : "null");
		fprintf(stderr, " norc:       %d\n", rc_norc);
		fprintf(stderr, " bg:         %d\n", bg);
		fprintf(stderr, " mod_tweak:  %d\n", use_modifier_tweak);
		fprintf(stderr, " isolevel3:  %d\n", use_iso_level3);
		fprintf(stderr, " xkb:        %d\n", use_xkb_modtweak);
		fprintf(stderr, " skipkeys:   %s\n",
		    skip_keycodes ? skip_keycodes : "null");
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
		fprintf(stderr, " ptr_mode:   %d\n", pointer_mode);
		fprintf(stderr, " inputskip:  %d\n", ui_skip);
		fprintf(stderr, " speeds:     %s\n", speeds_str
		    ? speeds_str : "null");
		fprintf(stderr, " debug_ptr:  %d\n", debug_pointer);
		fprintf(stderr, " debug_key:  %d\n", debug_keyboard);
		fprintf(stderr, " defer:      %d\n", defer_update);
		fprintf(stderr, " waitms:     %d\n", waitms);
		fprintf(stderr, " take_naps:  %d\n", take_naps);
		fprintf(stderr, " sb:         %d\n", screen_blank);
		fprintf(stderr, " xdamage:    %d\n", !got_noxdamage);
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
		fprintf(stderr, " gui:        %d\n", launch_gui);
		fprintf(stderr, " gui_mode:   %s\n", gui_str
		    ? gui_str : "null");
		fprintf(stderr, " noremote:   %d\n", !accept_remote_cmds);
		fprintf(stderr, " safemode:   %d\n", safe_remote_only);
		fprintf(stderr, " deny_all:   %d\n", deny_all);
		fprintf(stderr, "\n");
		rfbLog("x11vnc version: %s\n", lastmod);
	} else {
		rfbLogEnable(0);
	}

	/* open the X display: */
	X_INIT;
	if (auth_file) {
		set_env("XAUTHORITY", auth_file);
	}
	if (watch_bell || use_xkb_modtweak) {
		/* we need XKEYBOARD for these: */
		use_xkb = 1;
	}
	if (xkbcompat) {
		use_xkb = 0;
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
	use_xkb = 0;
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

	if (remote_cmd || query_cmd) {
		int rc = do_remote_query(remote_cmd, query_cmd, remote_sync);
		XFlush(dpy);
		fflush(stderr);
		fflush(stdout);
		usleep(30 * 1000);	/* still needed? */
		XCloseDisplay(dpy);
		exit(rc);
	}

	if (! dt) {
		static char str[] = "-desktop";
		argv_vnc[argc_vnc++] = str;
		argv_vnc[argc_vnc++] = choose_title(use_dpy);
		rfb_desktop_name = strdup(argv_vnc[argc_vnc-1]);
	}

#if LIBVNCSERVER_HAVE_LIBXFIXES
	if (! XFixesQueryExtension(dpy, &xfixes_base_event_type, &er)) {
		if (! quiet) {
			rfbLog("Disabling XFIXES mode: display does not "
			    "support it.\n");
		}
		xfixes_present = 0;
	} else {
		xfixes_present = 1;
	}
#endif

#if LIBVNCSERVER_HAVE_LIBXDAMAGE
	if (! XDamageQueryExtension(dpy, &xdamage_base_event_type, &er)) {
		if (! quiet) {
			rfbLog("Disabling X DAMAGE mode: display does not "
			    "support it.\n");
		}
		xdamage_present = 0;
	} else {
		xdamage_present = 1;
	}
#endif
	if (! quiet && xdamage_present && ! got_noxdamage) {
		rfbLog("X DAMAGE available on display, using it for"
		    " polling hints\n");
		rfbLog("  to disable this behavior use: "
		    "'-noxdamage'\n");
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
	}
	/*
	 * Window managers will often grab the display during resize, etc.
	 * To avoid deadlock (our user resize input is not processed)
	 * we tell the server to process our requests during all grabs:
	 */
	XTestGrabControl_wr(dpy, True);

	/* set OS struct UT */
	uname(&UT);

	/* check for OS with small shm limits */
	if (using_shm && ! single_copytile) {
		if (limit_shm()) {
			single_copytile = 1;
		}
	}

	single_copytile_orig = single_copytile;

	/* check for MIT-SHM */
	if (! nofb && ! XShmQueryExtension_wr(dpy)) {
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
	if (use_xkb) {
		initialize_xkb();
	}
	initialize_watch_bell();
	if (!use_xkb && use_xkb_modtweak) {
		if (! quiet) {
			rfbLog("warning: disabling xkb modtweak."
			    " XKEYBOARD ext. not present.\n");
		}
		use_xkb_modtweak = 0;
	}
#endif

#if LIBVNCSERVER_HAVE_LIBXRANDR
	if (! XRRQueryExtension(dpy, &xrandr_base_event_type, &er)) {
		if (xrandr && ! quiet) {
			rfbLog("Disabling -xrandr mode: display does not"
			    " support X RANDR.\n");
		}
		xrandr = 0;
		xrandr_present = 0;
	} else {
		xrandr_present = 1;
	}
#endif

	/*
	 * Create the XImage corresponding to the display framebuffer.
	 */

	fb0 = initialize_xdisplay_fb();

	/*
	 * n.b. we do not have to X_LOCK any X11 calls until watch_loop()
	 * is called since we are single-threaded until then.
	 */

	initialize_screen(&argc_vnc, argv_vnc, fb0);

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

