#ifndef _X11VNC_SSLCMDS_H
#define _X11VNC_SSLCMDS_H

/* -- sslcmds.h -- */

extern void check_stunnel(void);
extern int start_stunnel(int stunnel_port, int x11vnc_port);
extern void stop_stunnel(void);
extern void setup_stunnel(int rport, int *argc, char **argv);


#endif /* _X11VNC_SSLCMDS_H */
