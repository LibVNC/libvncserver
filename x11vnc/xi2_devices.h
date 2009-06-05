#ifndef _X11VNC_XI2_DEVICES
#define _X11VNC_XI2_DEVICES

#include <X11/extensions/XInput2.h> 
#include <X11/Xcursor/Xcursor.h> 

extern int xinput2_present;
extern int use_multipointer;
extern int xi2_device_creation_in_progress;


/*
  create xi2 master device with given name
  returns device_id, -1 on error
*/
extern int createMD(Display* dpy, char* name);

/*
  remove master device 
  returns 1 on success, 0 on failure
*/
extern int removeMD(Display* dpy, int dev_id);

/*
  gets the paired pointer/keyboard id to dev_id
  returns -1 on error
*/
extern int getPairedMD(Display* dpy, int dev_id);


/* 
   set cursor of pointer dev
   return 1 on success, 0 on failure
*/
extern XcursorImage* setPointerShape(Display *dpy, int dev_id, float r, float g, float b, char *label);




#endif /* _X11VNC_XI2_DEVICES */
