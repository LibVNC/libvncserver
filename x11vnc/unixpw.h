#ifndef _X11VNC_UNIXPW_H
#define _X11VNC_UNIXPW_H

/* -- unixpw.h -- */

extern void unixpw_screen(int init);
extern void unixpw_keystroke(rfbBool down, rfbKeySym keysym, int init);
extern void unixpw_accept(char *user);
extern void unixpw_deny(void);
extern int su_verify(char *user, char *pass);

extern int unixpw_in_progress;
extern time_t unixpw_last_try_time;
extern rfbClientPtr unixpw_client;

#endif /* _X11VNC_UNIXPW_H */
