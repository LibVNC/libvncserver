#ifndef _X11VNC_SOLID_H
#define _X11VNC_SOLID_H

/* -- solid.h -- */

extern char *guess_desktop(void);
extern void solid_bg(int restore);
extern XImage *solid_root(char *color);

#endif /* _X11VNC_SOLID_H */
