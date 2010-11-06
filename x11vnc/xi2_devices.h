/*
  XInput2 device handling routines for x11vnc.  

  Copyright (C) 2009-2010 Christian Beier <dontmind@freeshell.org>
  All rights reserved.

  This file is part of x11vnc.

  x11vnc is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or (at
  your option) any later version.

  x11vnc is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with x11vnc; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA
  or see <http://www.gnu.org/licenses/>.
*/

#ifndef _X11VNC_XI2_DEVICES
#define _X11VNC_XI2_DEVICES

#ifdef LIBVNCSERVER_HAVE_XI2
#include <X11/extensions/XInput2.h> 
#endif

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
*/
extern rfbCursorPtr setClientCursor(Display *dpy, int dev_id, float r, float g, float b, char *label);


#endif /* _X11VNC_XI2_DEVICES */
