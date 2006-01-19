/* -- 8to24.c -- */
#include "x11vnc.h"
#include "cleanup.h"
#include "scan.h"
#include "util.h"
#include "win_utils.h"

int multivis_count = 0;
int multivis_24count = 0;

void check_for_multivis(void);
void bpp8to24(int, int, int, int);
void mark_8bpp(void);

static void set_root_cmap(void);
static void check_pointer_in_depth24(void);
static int check_depth(Window win, Window top, int doall);
static int check_depth_win(Window win, Window top, XWindowAttributes attr);
static int get_8pp_region(sraRegionPtr region8bpp, sraRegionPtr rect,
    int validate);
static int get_cmap(int j, Colormap cmap);
static void do_8bpp_region(sraRect rect);

/* struct for keeping info about the 8bpp windows: */
typedef struct window8 {
	Window win;
	Window top;
	int depth;
	int x, y;
	int w, h;
	int map_state;
	Colormap cmap;
	Bool map_installed;
	int fetched;
} window8bpp_t;

static Colormap root_cmap = 0;
static void set_root_cmap(void) {
	static time_t last_set = 0;
	time_t now = time(0);
	XWindowAttributes attr;

	if (now > last_set + 5) {
		root_cmap = 0;
	}
	if (! root_cmap) {
		if (valid_window(window, &attr, 1)) {
			last_set = now;
			root_cmap = attr.colormap;
		}
	}
}

/* fixed size array.  Will primarily hold visible 8bpp windows */
#define MAX_8BPP_WINDOWS 64
static window8bpp_t windows_8bpp[MAX_8BPP_WINDOWS];

static int db24 = 0;
static int xgetimage_8to24 = 0;
static int do_hibits = 0;

static void check_pointer_in_depth24(void) {
	int tries = 0, in_24 = 0;
	XWindowAttributes attr;
	Window c, w;
	double now = dnow();

	c = window;

	if (now > last_keyboard_time + 1.0 && now > last_pointer_time + 1.0) {
		return;
	}

	X_LOCK;
	while (c && tries++ < 3) {
		c = query_pointer(c);
		if (valid_window(c, &attr, 1)) 	{
			if (attr.depth == 24) {
				in_24 = 1;
				break;
			}
		}
	}
	X_UNLOCK;
	if (in_24) {
		int x1, y1, x2, y2;
		X_LOCK;
		xtranslate(c, window, 0, 0, &x1, &y1, &w, 1);
		X_UNLOCK;
		x2 = x1 + attr.width;
		y2 = y1 + attr.height;
		x1 = nfix(x1, dpy_x);
		y1 = nfix(y1, dpy_y);
		x2 = nfix(x2, dpy_x);
		y2 = nfix(y2, dpy_y);
if (db24 > 1) fprintf(stderr, "check_pointer_in_depth24 %d %d %d %d\n", x1, y1, x2, y2);
		mark_rect_as_modified(x1, y1, x2, y2, 0);
	}
}

void check_for_multivis(void) {
	XWindowAttributes attr;
	int doall = 0;
	int k, i, cnt, diff;
	static int first = 1;
	static double last_update = 0.0;
	static double last_clear = 0.0;
	double now = dnow();
	static Window *stack_old = NULL;
	static int stack_old_len = 0;

	if (first) {
		int i;
		/* initialize 8bpp window table: */
		for (i=0; i < MAX_8BPP_WINDOWS; i++) 	{
			windows_8bpp[i].win = None;
			windows_8bpp[i].top = None;
			windows_8bpp[i].map_state = IsUnmapped;
			windows_8bpp[i].cmap = (Colormap) 0;
			windows_8bpp[i].fetched = 0;
		}
		if (getenv("DEBUG_8TO24") != NULL) {
			db24 = atoi(getenv("DEBUG_8TO24"));
		}
		if (getenv("XGETIMAGE_8TO24") != NULL) {
			xgetimage_8to24 = 1;
		}
		if (getenv("HIGHBITS_8TO24") != NULL) {
			do_hibits = 1;
		}
		first = 0;
		doall = 1;	/* fetch everything first time */
	}
	set_root_cmap();

	/*
	 * allocate an "old stack" list of all toplevels.  we compare
	 * this to the current stack to guess stacking order changes.
	 */
	if (!stack_old || stack_old_len < stack_list_len) {
		int n = stack_list_len;
		if (n < 256) {
			n = 256;
		}
		if (stack_old) {
			free(stack_old);
		}
		stack_old = (Window *) malloc(n*sizeof(Window));
		stack_old_len = n;
	}

	/* fill the old stack with visible windows: */
	cnt = 0;
	for (k=0; k < stack_list_num; k++) {
		if (stack_list[k].valid &&
		    stack_list[k].map_state == IsViewable) {
			stack_old[cnt++] = stack_list[k].win;
		}
	}

	/* snapshot + update the current stacking order: */
	snapshot_stack_list(0, 0.25);
	if (doall || now > last_update + 0.25) {
		update_stack_list();
		last_update = now;
	}

	/* look for differences in the visible toplevels: */
	diff = 0;
	cnt = 0;
	for (k=0; k < stack_list_num; k++) {
		if (stack_list[k].valid && stack_list[k].map_state == IsViewable) {
			if (stack_old[cnt] != stack_list[k].win) {
				diff = 1;
				break;
			}
			cnt++;
		}
	}

	/*
	 * if there are 8bpp visible and a stacking order change
	 * refresh vnc with coverage of the 8bpp regions:
	 */
	if (diff && multivis_count) {
if (db24) fprintf(stderr, "check_for_multivis stack diff: mark_all %f\n", now - x11vnc_start);
		mark_8bpp();

	} else if (depth == 8 && multivis_24count) {
		static double last_check = 0.0;
		if (now > last_check + 0.25) {
			last_check = now;
			check_pointer_in_depth24();
		}
	}
	
	multivis_count = 0;
	multivis_24count = 0;

	/*
	 * every 10 seconds we try to clean out and also refresh the window
	 * info in the the 8bpp window table:
	 */
	if (now > last_clear + 10) {
		last_clear = now;
		X_LOCK;
		for (i=0; i < MAX_8BPP_WINDOWS; i++) {
			Window w = windows_8bpp[i].win;
if ((db24) && w != None) fprintf(stderr, " windows_8bpp: 0x%lx i=%02d  ms: %d  dep=%d\n", windows_8bpp[i].win, i, windows_8bpp[i].map_state, windows_8bpp[i].depth);
			if (! valid_window(w, &attr, 1)) {
				/* catch windows that went away: */
				windows_8bpp[i].win = None;
				windows_8bpp[i].top = None;
				windows_8bpp[i].map_state = IsUnmapped;
				windows_8bpp[i].cmap = (Colormap) 0;
				windows_8bpp[i].fetched = 0;
			}
		}
		X_UNLOCK;
	}

	/* loop over all toplevels, both 8 and 24 depths: */
	for (k=0; k < stack_list_num; k++) {
		Window r, parent;
		Window *list0;
		Status rc;
		unsigned int nc0;
		int i1;

		Window win = stack_list[k].win;

		if (win == None) {
			continue;
		}

		if (stack_list[k].map_state != IsViewable) {
			int i;
			/*
			 * if the toplevel became unmapped, mark it
			 * for the children as well...
			 */
			for (i=0; i < MAX_8BPP_WINDOWS; i++) {
				if (windows_8bpp[i].top == win) {
					windows_8bpp[i].map_state =
					    stack_list[k].map_state;
				}
			}
		}

		if (check_depth(win, win, doall)) {
			/*
			 * returns 1 if no need to recurse down e.g. IT
			 * is 8bpp and we assume all lower one are too.
			 */
			continue;
		}

		/* we recurse up to two levels down from stack_list windows */
		X_LOCK;
		rc = XQueryTree(dpy, win, &r, &parent, &list0, &nc0);
		X_UNLOCK;
		if (! rc) {
			continue;
		}

		/* loop over grandchildren of rootwin: */
		for (i1=0; i1 < (int) nc0; i1++) {
			Window win1 = list0[i1];
			Window *list1;
			unsigned int nc1;
			int i2;

			if (check_depth(win1, win, doall)) {
				continue;
			}
			X_LOCK;
			rc = XQueryTree(dpy, win1, &r, &parent, &list1, &nc1);
			X_UNLOCK;
			if (! rc) {
				continue;
			}
			/* loop over great-grandchildren of rootwin: */
			for (i2=0; i2< (int) nc1; i2++) {
				Window win2 = list1[i2];

				if (check_depth(win2, win, doall)) {
					continue;
				}
				/* more? Which wm does this? */
			}
			if (nc1) {
				X_LOCK;
				XFree(list1);
				X_UNLOCK;
			}
		}
		if (nc0) {
			X_LOCK;
			XFree(list0);
			X_UNLOCK;
		}
	}
}

static int check_depth(Window win, Window top, int doall) {
	XWindowAttributes attr;

	X_LOCK;
	/* first see if it is (still) a valid window: */
	if (! valid_window(win, &attr, 1)) {
		X_UNLOCK;
		return 1;	/* indicate done */
	}
	X_UNLOCK;
	if (! doall && attr.map_state != IsViewable) {
		/*
		 * store results anyway...  this may lead to table filling up,
		 * but currently this allows us to update state of onetime mapped
		 * windows.
		 */
		check_depth_win(win, top, attr);
		return 1;	/* indicate done */
	} else if (check_depth_win(win, top, attr)) {
		return 1;	/* indicate done */
	} else {
		return 0;	/* indicate not done */
	}
}

static int check_depth_win(Window win, Window top, XWindowAttributes attr) {
	int store_it = 0;
	/*
	 * only store windows with depth not equal to the default visual's depth
	 * note some windows can have depth == 0 ... (skip them).
	 */
	if (attr.depth > 0) {
		set_root_cmap();
		if (depth == 24 && attr.depth != 24) {
			store_it = 1;
		} else if (depth == 8 && root_cmap && attr.colormap !=
		    root_cmap) {
			store_it = 1;
		}
	}
	if (store_it) {

		int i, j = -1, none = -1, nomap = -1;
		int new = 0;
		if (attr.map_state == IsViewable) {
			/* count the visible ones: */
			multivis_count++;
			if (attr.depth == 24) {
				multivis_24count++;
			}
if (db24 > 1) fprintf(stderr, "multivis: 0x%lx %d\n", win, attr.depth);
		}

		/* try to find a table slot for this window: */
		for (i=0; i < MAX_8BPP_WINDOWS; i++) {
			if (none < 0 && windows_8bpp[i].win == None) {
				/* found first None */
				none = i;
			}
			if (windows_8bpp[i].win == win) {
				/* found myself */
				j = i;
				break;
			}
			if (nomap < 0 && windows_8bpp[i].win != None &&
			    windows_8bpp[i].map_state != IsViewable) {
				/* found first unmapped */
				nomap = i;
			}
		}
		if (j < 0) {
			if (attr.map_state != IsViewable) {
				/* no slot and not visible: not worth keeping */
				return 1;
			} else if (none >= 0) {
				/* put it in the first None slot */
				j = none;
				new = 1;
			} else if (nomap >=0) {
				/* put it in the first unmapped slot */
				j = nomap;
			}
			/* otherwise we cannot store it... */
		}

if (db24 > 1) fprintf(stderr, "multivis: 0x%lx ms: %d j: %d no: %d nm: %d dep=%d\n", win, attr.map_state, j, none, nomap, attr.depth);

		/* store if if we found a slot j: */
		if (j >= 0) {
			Window w;
			int x, y;
			int now_vis = 0;

			if (attr.map_state == IsViewable &&
			    windows_8bpp[j].map_state != IsViewable) {
				now_vis = 1;
			} 
if (db24 > 1) fprintf(stderr, "multivis: STORE 0x%lx j: %3d ms: %d dep=%d\n", win, j, attr.map_state, attr.depth);
			windows_8bpp[j].win = win;
			windows_8bpp[j].top = top;
			windows_8bpp[j].depth = attr.depth;
			windows_8bpp[j].map_state = attr.map_state;
			windows_8bpp[j].cmap = attr.colormap;
			windows_8bpp[j].map_installed = attr.map_installed;
			windows_8bpp[j].w = attr.width;
			windows_8bpp[j].h = attr.height;

			/* translate x y to be WRT the root window (not parent) */
			xtranslate(win, window, 0, 0, &x, &y, &w, 1);
			windows_8bpp[j].x = x;
			windows_8bpp[j].y = y;

			windows_8bpp[j].fetched = 1;

			if (new || now_vis) {
if (db24) fprintf(stderr, "new/now_vis: 0x%lx %d/%d\n", win, new, now_vis);
				/* mark it immediately if a new one: */
				mark_rect_as_modified(x, y, x + attr.width,
				    y + attr.height, 0);
			}
		} else {
			/*
			 * Error: could not find a slot.
			 * perhaps keep age and expire old ones??
			 */
if (db24) fprintf(stderr, "multivis: CANNOT STORE 0x%lx j=%d\n", win, j);
			for (i=0; i < MAX_8BPP_WINDOWS; i++) {
if (db24 > 1) fprintf(stderr, "          ------------ 0x%lx i=%d\n", windows_8bpp[i].win, i);
			}
			
		}
		return 1;
	}
	return 0;
}

#define CMAPMAX 64
Colormap cmaps[CMAPMAX];
int ncmaps;

static int get_8pp_region(sraRegionPtr region8bpp, sraRegionPtr rect,
    int validate) {

	XWindowAttributes attr;
	int i, k, mapcount = 0;

	/* initialize color map list */
	ncmaps = 0;
	for (i=0; i < CMAPMAX; i++) {
		cmaps[i] = (Colormap) 0;
	}

	/* loop over the table of 8bpp windows: */
	for (i=0; i < MAX_8BPP_WINDOWS; i++) {
		sraRegionPtr tmp_reg, tmp_reg2;
		Window c, w = windows_8bpp[i].win;
		int x, y;

		if (wireframe_in_progress) {
			break;	/* skip updates during wireframe drag */
		}

		if (w == None) {
			continue;
		}

if (db24 > 1) fprintf(stderr, "get_8pp_region: 0x%lx ms=%d dep=%d i=%d\n", w, windows_8bpp[i].map_state, windows_8bpp[i].depth, i);
		if (validate) {
			/*
			 * this could be slow: validating 8bpp windows each
			 * time...
			 */

			X_LOCK;
			if (! valid_window(w, &attr, 1)) {
				X_UNLOCK;
				windows_8bpp[i].win = None;
				windows_8bpp[i].top = None;
				windows_8bpp[i].map_state = IsUnmapped;
				windows_8bpp[i].cmap = (Colormap) 0;
				windows_8bpp[i].fetched = 0;
				continue;
			}
			X_UNLOCK;
			if (attr.map_state != IsViewable) {
				continue;
			}

			X_LOCK;
			xtranslate(w, window, 0, 0, &x, &y, &c, 1);
			X_UNLOCK;

		} else {
			/* this will be faster: no call to X server: */
			if (windows_8bpp[i].map_state != IsViewable) {
				continue;
			}
			x =  windows_8bpp[i].x; 
			y =  windows_8bpp[i].y; 
			attr.width = windows_8bpp[i].w;
			attr.height = windows_8bpp[i].h;
			attr.map_state = windows_8bpp[i].map_state;
			attr.colormap = windows_8bpp[i].cmap;
		}

		mapcount++;

		/* tmp region for this 8bpp rectangle: */
		tmp_reg = sraRgnCreateRect(nfix(x, dpy_x), nfix(y, dpy_y),
		    nfix(x + attr.width, dpy_x), nfix(y + attr.height, dpy_y));

		/* find overlap with mark region in rect: */
		sraRgnAnd(tmp_reg, rect);

		if (sraRgnEmpty(tmp_reg)) {
			/* skip if no overlap: */
			sraRgnDestroy(tmp_reg);
			continue;
		}

		/* loop over all toplevels, top to bottom clipping: */
		for (k = stack_list_num - 1; k >= 0; k--) {
			Window swin = stack_list[k].win;
			int sx, sy, sw, sh;

if (db24 > 1 && stack_list[k].map_state == IsViewable) fprintf(stderr, "Stack win: 0x%lx %d iv=%d\n", swin, k, stack_list[k].map_state);

			if (swin == windows_8bpp[i].top) {
				/* found our top level: we clip the rest. */
if (db24 > 1) fprintf(stderr, "found top: 0x%lx %d iv=%d\n", swin, k, stack_list[k].map_state);
				break;
			}
			if (stack_list[k].map_state != IsViewable) {
				/* skip unmapped ones: */
				continue;
			}

			/* make a temp rect for this toplevel: */
			sx = stack_list[k].x;
			sy = stack_list[k].y;
			sw = stack_list[k].width;
			sh = stack_list[k].height;

if (db24 > 1) fprintf(stderr, "subtract:  0x%lx %d -- %d %d %d %d\n", swin, k, sx, sy, sw, sh);

			tmp_reg2 = sraRgnCreateRect(nfix(sx, dpy_x),
			    nfix(sy, dpy_y), nfix(sx + sw, dpy_x),
			    nfix(sy + sh, dpy_y));

			/* subtract it from the 8bpp window region */
			sraRgnSubtract(tmp_reg, tmp_reg2);
			sraRgnDestroy(tmp_reg2);
		}

		if (sraRgnEmpty(tmp_reg)) {
			/* skip this 8bpp if completely clipped away: */
			sraRgnDestroy(tmp_reg);
if (db24 > 1) fprintf(stderr, "Empty tmp_reg\n");
			continue;
		}

		/* otherwise, store any new colormaps: */
		if (ncmaps < CMAPMAX && attr.colormap != (Colormap) 0) {
			int m, sawit = 0;
			for (m=0; m < ncmaps; m++) {
				if (cmaps[m] == attr.colormap) {
					sawit = 1;
					break;
				}
			}
			if (! sawit && attr.depth == 8) {
				/* store only new ones: */
				cmaps[ncmaps++] = attr.colormap;
			}
		}

		/* now include this region with the full 8bpp region:  */
		sraRgnOr(region8bpp, tmp_reg);
		sraRgnDestroy(tmp_reg);
	}

	return mapcount;
}

#define NCOLOR 256
static XColor color[CMAPMAX][NCOLOR];
static unsigned int rgb[CMAPMAX][NCOLOR];
static int cmap_failed[CMAPMAX];
int histo[256];

static int get_cmap(int j, Colormap cmap) {
	int i, ncells;
	XErrorHandler old_handler = NULL;

	if (0) {
		/* not working properly for depth 24... */
		X_LOCK;
		ncells = CellsOfScreen(ScreenOfDisplay(dpy, scr));
		X_UNLOCK;
	} else {
		ncells = NCOLOR;
	}
if (db24 > 1) fprintf(stderr, "get_cmap: %d 0x%x\n", j, (unsigned int) cmap);

	/* ncells should "always" be 256. */
	if (ncells > NCOLOR) {
		ncells = NCOLOR;
	} else if (ncells == 8) {
		/* hmmm. see set_colormap() */
		ncells = NCOLOR;
	}

	/* initialize XColor array: */
	for (i=0; i < ncells; i++) {
		color[j][i].pixel = i;
		color[j][i].pad = 0;
	}

	/* try to query the colormap, trap errors */
	X_LOCK;
	trapped_xerror = 0;
	old_handler = XSetErrorHandler(trap_xerror);
	XQueryColors(dpy, cmap, color[j], ncells);
	XSetErrorHandler(old_handler);
	X_UNLOCK;

	if (trapped_xerror) {
		trapped_xerror = 0;
		return 0;
	}
	trapped_xerror = 0;

	/* now map each index to depth 24 RGB */
	for (i=0; i < ncells; i++) {
		unsigned int red, green, blue;
		/* strip out highest 8 bits of values: */
		red   = (color[j][i].red   & 0xff00) >> 8;
		green = (color[j][i].green & 0xff00) >> 8;
		blue  = (color[j][i].blue  & 0xff00) >> 8;

		/*
		 * the maxes should be at 255 already,
		 * but just in case...
		 */
		red   = (main_red_max   * red  )/255;
		green = (main_green_max * green)/255;
		blue  = (main_blue_max  * blue )/255;

if (db24 > 2) fprintf(stderr, " cmap[%02d][%03d]: %03d %03d %03d  0x%08x \n", j, i, red, green, blue, ( red << main_red_shift | green << main_green_shift | blue << main_blue_shift));

		/* shift them over and or together for value */
		red   = red    << main_red_shift;
		green = green  << main_green_shift;
		blue  = blue   << main_blue_shift;

		rgb[j][i] = red | green | blue;
	}
	return 1;
}

static void do_8bpp_region(sraRect rect) {

	char *src, *dst;
	unsigned int *ui;
	unsigned char *uc;
	int ps, pixelsize = bpp/8;

	int do_getimage = xgetimage_8to24;
	int line, n_off, j, h, w;
	unsigned int hi, idx;
	XWindowAttributes attr;
	XErrorHandler old_handler = NULL;

	double score, max_score = -1.0;
	int m, best, best_depth = 0;
	Window best_win = None;

if (db24 > 1) fprintf(stderr, "ncmaps: %d\n", ncmaps);

	/*
	 * try to pick the "best" colormap to use for
	 * this rectangle (often wrong... let them
	 * iconify or move the trouble windows, etc.)
	 */
	best = -1;

	for (m=0; m < MAX_8BPP_WINDOWS; m++) {
		int mx1, my1, mx2, my2;
		int k, failed = 0;
		if (windows_8bpp[m].win == None) {
			continue;
		}
		if (windows_8bpp[m].map_state != IsViewable) {
			continue;
		}

		/* see if XQueryColors failed: */
		for (k=0; k<ncmaps; k++) {
			if (windows_8bpp[m].cmap == cmaps[k] && cmap_failed[k]) {
				failed = 1;
			}
		}
		if (windows_8bpp[m].depth == 8 && failed) {
			continue;
		}

		/* rectangle coords for this 8bpp win: */
		mx1 = windows_8bpp[m].x;
		my1 = windows_8bpp[m].y;
		mx2 = windows_8bpp[m].x + windows_8bpp[m].w;
		my2 = windows_8bpp[m].y + windows_8bpp[m].h;

		/* use overlap as score: */
		score = rect_overlap(mx1, my1, mx2, my2, rect.x1, rect.y1,
		    rect.x2, rect.y2);

		if (score > max_score) {
			max_score = score;
			best = m;
			best_win = windows_8bpp[m].win;
			best_depth = windows_8bpp[m].depth;
		}
if (db24 > 1) fprintf(stderr, "cmap_score: 0x%x %.3f %.3f\n", (int) windows_8bpp[m].cmap, score, max_score);

	}

	if (best < 0) {
		/* hmmm, use the first one then... */
		best = 0;
	} else {
		int ok = 0;
		/*
		 * find the cmap corresponding to best window
		 * note we reset best from the windows_8bpp
		 * index to the cmaps[].
		 */
		for (m=0; m < ncmaps; m++) {
			if (cmaps[m] == windows_8bpp[best].cmap) {
				ok = 1;
				best = m;
			}
		}
		if (! ok) {
			best = 0;
		}
	}


if (db24 > 1) fprintf(stderr, "transform %d %d %d %d\n", rect.x1, rect.y1, rect.x2, rect.y2);

	/* now tranform the pixels in this rectangle: */
	n_off = main_bytes_per_line * rect.y1 + pixelsize * rect.x1;

	h = rect.y2 - rect.y1;
	w = rect.x2 - rect.x1;

	if (depth == 8) {
		/*
		 * need to fetch depth 24 data. might need for 
		 * best_depth == 8 too... (hi | ... failure).
		 */
		if (best_depth == 24)  {
			do_getimage = 1;
		} else if (! do_hibits) {
			do_getimage = 1;
		}
	}

	if (do_getimage && valid_window(best_win, &attr, 1)) {
		XImage *xi;
		Window c;
		unsigned int wu, hu;
		int xo, yo;

		wu = (unsigned int) w;
		hu = (unsigned int) h;

		X_LOCK;
		xtranslate(best_win, window, 0, 0, &xo, &yo, &c, 1);
		xo = rect.x1 - xo;
		yo = rect.y1 - yo;

if (db24 > 1) fprintf(stderr, "xywh: %d %d %d %d vs. %d %d\n", xo, yo, w, h, attr.width, attr.height);

		if (xo < 0 || yo < 0 || w > attr.width || h > attr.height) {
			X_UNLOCK;
if (db24 > 1) fprintf(stderr, "skipping due to potential bad match...\n");
			return;
		}

		trapped_xerror = 0;
		old_handler = XSetErrorHandler(trap_xerror);
		/* FIXME: XGetSubImage? */
		xi = XGetImage(dpy, best_win, xo, yo, wu, hu,
		    AllPlanes, ZPixmap);
		XSetErrorHandler(old_handler);
		X_UNLOCK;

		if (! xi || trapped_xerror) {
			trapped_xerror = 0;
if (db24 > 1) fprintf(stderr, "xi-fail: 0x%p trap=%d  %d %d %d %d\n", (void *)xi, trapped_xerror, xo, yo, w, h);
			return;
		} else {
if (db24 > 1) fprintf(stderr, "xi: 0x%p  %d %d %d %d -- %d %d\n", (void *)xi, xo, yo, w, h, xi->width, xi->height);
		}
		trapped_xerror = 0;

		if (xi->depth != 8 && xi->depth != 24) {
			X_LOCK;
			XDestroyImage(xi);
			X_UNLOCK;
if (db24) fprintf(stderr, "xi: wrong depth: %d\n", xi->depth);
			return;
		}

		if (xi->depth == 8) {
			int ps1, ps2, fac;

			if (depth == 8) {
				ps1 = 1;
				ps2 = 4;
				fac = 4;
			} else {
				ps1 = 1;
				ps2 = pixelsize;
				fac = 1;
			}

			src = xi->data;
			dst = cmap8to24_fb + fac * n_off;


			/* line by line ... */
			for (line = 0; line < xi->height; line++) {
				/* pixel by pixel... */
				for (j = 0; j < xi->width; j++) {

					uc = (unsigned char *) (src + ps1 * j);
					ui = (unsigned int *)  (dst + ps2 * j);

					idx = (int) (*uc);

					*ui = rgb[best][idx];
				}
				src += xi->bytes_per_line;
				dst += main_bytes_per_line * fac;
			}
		} else if (xi->depth == 24) {
			/* line by line ... */
			int ps1 = 4, fac;
			if (depth == 8) {
				fac = 4;
			} else {
				fac = 1;	/* should not happen */
			}

			src = xi->data;
			dst = cmap8to24_fb + fac * n_off;

			for (line = 0; line < xi->height; line++) {
				memcpy(dst, src, w * ps1);
				src += xi->bytes_per_line;
				dst += main_bytes_per_line * fac;
			}
		}

		X_LOCK;
		XDestroyImage(xi);
		X_UNLOCK;

	} else if (! do_getimage) {
		/* normal mode. */
		int fac;

		if (depth == 8) {
			/* cooked up depth 24 TrueColor  */
			ps = 4;
			fac = 4;
			src = cmap8to24_fb + 4 * n_off;
		} else {
			ps = pixelsize;
			fac = 1;
			src = cmap8to24_fb + n_off;
		}
		
		/* line by line ... */
		for (line = 0; line < h; line++) {
			/* pixel by pixel... */
			for (j = 0; j < w; j++) {

				/* grab 32 bit value */
				ui = (unsigned int *) (src + ps * j);

				/* extract top 8 bits (FIXME: masks?) */
				hi = (*ui) & 0xff000000; 

				/* map to lookup index; rewrite pixel */
				idx = hi >> 24;
				*ui = hi | rgb[best][idx];
			}
			src += main_bytes_per_line * fac;
		}
	}
}

void bpp8to24(int x1, int y1, int x2, int y2) {
	char *src, *dst;
	unsigned char *uc;
	unsigned int *ui;
	unsigned int hi;
	int idx, pixelsize = bpp/8;
	int line, i, j, h, w;
	int n_off;

	sraRegionPtr rect, disp, region8bpp;

	int validate = 1;
	static int last_map_count = 0, call_count = 0;

	if (! cmap8to24 || !cmap8to24_fb) {
		/* hmmm, why were we called? */
		return;
	}

	call_count++;

	/* clip to display just in case: */
	x1 = nfix(x1, dpy_x);
	y1 = nfix(y1, dpy_y);
	x2 = nfix(x2, dpy_x);
	y2 = nfix(y2, dpy_y);

	/* create regions for finding overlap, etc. */
	disp = sraRgnCreateRect(0, 0, dpy_x, dpy_y);
	rect = sraRgnCreateRect(x1, y1, x2, y2);
	region8bpp = sraRgnCreate();

	X_LOCK;
	XFlush(dpy);	/* make sure X server is up to date WRT input, etc */
	X_UNLOCK;

	if (last_map_count > MAX_8BPP_WINDOWS/4) {
		/* table is filling up... skip validating sometimes: */
		int skip = 3;
		if (last_map_count > MAX_8BPP_WINDOWS/2) {
			skip = 6;
		} else if (last_map_count > 3*MAX_8BPP_WINDOWS/4) {
			skip = 12;
		}
		if (call_count % skip != 0) {
			validate = 0;
		}
	}

if (db24 > 2) {for(i=0;i<256;i++){histo[i]=0;}}

	last_map_count = get_8pp_region(region8bpp, rect, validate);

	/* copy from main_fb to cmap8to24_fb regardless of 8bpp windows: */

	h = y2 - y1;
	w = x2 - x1;

	if (depth == 8) {
		/* need to cook up to depth 24 TrueColor  */
		/* pixelsize = 1 */

		n_off = main_bytes_per_line * y1 + pixelsize * x1;

		src = main_fb + n_off;
		dst = cmap8to24_fb + 4 * n_off;

		set_root_cmap();
		if (get_cmap(0, root_cmap)) {
			int ps1 = 1, ps2 = 4;

			/* line by line ... */
			for (line = 0; line < h; line++) {
				/* pixel by pixel... */
				for (j = 0; j < w; j++) {

					uc = (unsigned char *) (src + ps1 * j);
					ui = (unsigned int *)  (dst + ps2 * j);

					idx = (int) (*uc);

					if (do_hibits) {
						hi = idx << 24;
						*ui = hi | rgb[0][idx];
					} else {
						*ui = rgb[0][idx];
					}
if (db24 > 2) histo[idx]++;
				}
				src += main_bytes_per_line;
				dst += main_bytes_per_line * 4;
			}
		}
		
	} else if (depth == 24) {
		/* pixelsize = 4 */
		n_off = main_bytes_per_line * y1 + pixelsize * x1;

		src = main_fb      + n_off;
		dst = cmap8to24_fb + n_off;

		/* otherwise, the pixel data as is */
		for (line = 0; line < h; line++) {
			memcpy(dst, src, w * pixelsize);
			src += main_bytes_per_line;
			dst += main_bytes_per_line;
		}
	}

if (db24 > 1) fprintf(stderr, "bpp8to24 w=%d h=%d m=%p c=%p r=%p ncmaps=%d\n", w, h, main_fb, cmap8to24_fb, rfb_fb, ncmaps);

	/*
	 * now go back and tranform and 8bpp regions to TrueColor in
	 * cmap8to24_fb.  we have to guess the best colormap to use if
	 * there is more than one...
	 */
	if (! sraRgnEmpty(region8bpp) && (ncmaps || depth == 8)) {
		sraRectangleIterator *iter;
		sraRect rect;
		int j;

		/*
		 * first, grab all of the associated colormaps from the
		 * X server.  Hopefully just 1 or 2...
		 */
		for (j=0; j<ncmaps; j++) {
			if (! get_cmap(j, cmaps[j])) {
				cmap_failed[j] = 1;
			} else {
				cmap_failed[j] = 0;
			}
if (db24 > 1) fprintf(stderr, "cmap %d\n", (int) cmaps[j]);
		}

		/* loop over the rectangles making up region8bpp */
		iter = sraRgnGetIterator(region8bpp);
		while (sraRgnIteratorNext(iter, &rect)) {
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

			do_8bpp_region(rect);
		}
		sraRgnReleaseIterator(iter);
	}

if (db24 > 2) {for(i=0; i<256;i++) {fprintf(stderr, " cmap histo[%03d] %d\n", i, histo[i]);}}

	/* cleanup */
	sraRgnDestroy(disp);
	sraRgnDestroy(rect);
	sraRgnDestroy(region8bpp);
}

void mark_8bpp(void) {
	int i, cnt = 0;

	if (! cmap8to24 || !cmap8to24_fb) {
		return;
	}

	/* for each mapped 8bpp window, mark it changed: */

	for (i=0; i < MAX_8BPP_WINDOWS; i++) {
		int x1, y1, x2, y2, w, h, f = 32;

		f = 0;	/* skip fuzz, may bring in other windows... */

		if (windows_8bpp[i].win == None) {
			continue;
		}
		if (windows_8bpp[i].map_state != IsViewable) {
			XWindowAttributes attr;
			int vw;

			X_LOCK;
			vw = valid_window(windows_8bpp[i].win, &attr, 1);
			X_UNLOCK;
			if (vw) {
				if (attr.map_state != IsViewable) {
					continue;
				}
			} else {
				continue;
			}
		}

		x1 = windows_8bpp[i].x;
		y1 = windows_8bpp[i].y;
		w  = windows_8bpp[i].w;
		h  = windows_8bpp[i].h;

		/* apply a fuzz f around each one... constrain to screen */
		x2 = x1 + w;
		y2 = y1 + h;
		x1 = nfix(x1 - f, dpy_x);
		y1 = nfix(y1 - f, dpy_y);
		x2 = nfix(x2 + f, dpy_x);
		y2 = nfix(y2 + f, dpy_y);

if (db24 > 1) fprintf(stderr, "mark_8bpp: 0x%lx %d %d %d %d\n", windows_8bpp[i].win, x1, y1, x2, y2);

		mark_rect_as_modified(x1, y1, x2, y2, 0);
		cnt++;
	}
	if (cnt) {
		/* push it to viewers if possible. */
		rfbPE(-1);
	}
}

