#ifndef _X11VNC_XI2_DEVICES
#define _X11VNC_XI2_DEVICES


#include <X11/Xcursor/Xcursor.h> 

extern int xinput2_present;
extern int use_multipointer;
extern int xi2_device_creation_in_progress;


/*
  create xi2 master device with given name
  returns device pointer
*/
extern XDevice* createMD(Display* dpy, char* name);

/*
  remove master device 
  return 1 on success, 0 on failure
*/
extern int removeMD(Display* dpy, XDevice* dev);

/*
  gets the paired pointer/keyboard to dev
*/
extern  XDevice* getPairedMD(Display* dpy, XDevice* dev);


/* 
   set cursor of pointer dev
   return 1 on success, 0 on failure
*/
extern XcursorImage* setPointerShape(Display *dpy, XDevice* dev, float r, float g, float b, char *label);




#endif /* _X11VNC_XI2_DEVICES */
