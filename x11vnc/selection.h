#ifndef _X11VNC_SELECTION_H
#define _X11VNC_SELECTION_H

/* -- selection.h -- */

extern char *xcut_str;
extern int own_selection;
extern int set_cutbuffer;
extern int sel_waittime;
extern Window selwin;

extern void selection_request(XEvent *ev);
extern int check_sel_direction(char *dir, char *label, char *sel, int len);
extern void cutbuffer_send(void);
extern void selection_send(XEvent *ev);

#endif /* _X11VNC_SELECTION_H */
