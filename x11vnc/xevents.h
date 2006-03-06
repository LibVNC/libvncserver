#ifndef _X11VNC_XEVENTS_H
#define _X11VNC_XEVENTS_H

/* -- xevents.h -- */

extern int grab_buster;
extern int sync_tod_delay;

extern void initialize_vnc_connect_prop(void);
extern void initialize_x11vnc_remote_prop(void);
extern void spawn_grab_buster(void);
extern void sync_tod_with_servertime(void);
extern void check_keycode_state(void);
extern void check_autorepeat(void);
extern void check_xevents(void);
extern void xcut_receive(char *text, int len, rfbClientPtr cl);

#endif /* _X11VNC_XEVENTS_H */
