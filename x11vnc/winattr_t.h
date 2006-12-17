#ifndef _X11VNC_WINATTR_T_H
#define _X11VNC_WINATTR_T_H

/* -- winattr_t.h -- */

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
	double bs_time;
	double su_time;
	int bs_x, bs_y, bs_w, bs_h;
	int su_x, su_y, su_w, su_h;
	int selectinput;
} winattr_t;

#endif /* _X11VNC_WINATTR_T_H */
