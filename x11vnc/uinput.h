#ifndef _X11VNC_UINPUT_H
#define _X11VNC_UINPUT_H

/* -- uinput.h -- */

extern int check_uinput(void);
extern int initialize_uinput(void);
extern int set_uinput_accel(char *str);
extern void set_uinput_reset(int ms);
extern char *get_uinput_accel();
extern int get_uinput_reset();
extern void parse_uinput_str(char *str);
extern void uinput_pointer_command(int mask, int x, int y, rfbClientPtr client);
extern void uinput_key_command(int down, int keysym, rfbClientPtr client);



#endif /* _X11VNC_UINPUT_H */
