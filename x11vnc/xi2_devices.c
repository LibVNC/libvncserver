
#include <string.h>
#include <X11/extensions/XInput.h> 
#include <cairo.h>
#include <X11/Xproto.h> 
#include <X11/keysym.h> 

#include "cleanup.h"
#include "xi2_devices.h" 



/* does the X version we're running on support XI2? */
int xinput2_present;
int use_multipointer;
int xi2_device_creation_in_progress;


/*
  create MD with given name
  returns device pointer
*/
XDevice* createMD(Display* dpy, char* name)
{
  XDevice *dev = NULL;
  XErrorHandler old_handler;
  XCreateMasterInfo c;
  
  c.type = CH_CreateMasterDevice;
  c.name = name;
  c.sendCore = 1;
  c.enable = 1;

  trapped_xerror = 0;
  old_handler = XSetErrorHandler(trap_xerror);
	
  XChangeDeviceHierarchy(dpy, (XAnyHierarchyChangeInfo*)&c, 1);

  XSync(dpy, False);
  if(trapped_xerror)
    {
      XSetErrorHandler(old_handler);
      trapped_xerror = 0;
      return NULL;
    }

  XSetErrorHandler(old_handler);
  trapped_xerror = 0;

  /* find newly created dev by name
     FIXME: is there a better way? */
  char handle[256];
  snprintf(handle, 256, "%s pointer", name);

  XDeviceInfo	*devices;
  int		num_devices;
  devices = XListInputDevices(dpy, &num_devices); 
  int i;
  for(i = 0; i < num_devices; ++i) /* seems the InputDevices List is already chronologically reversed */
    if(strcmp(devices[i].name, handle) == 0)
      dev = XOpenDevice(dpy, devices[i].id);
 
  XFreeDeviceList(devices);

  return dev;
}



/*
  remove device 
  return 1 on success, 0 on failure
*/
int removeMD(Display* dpy, XDevice* dev)
{
  XRemoveMasterInfo r;
  int found = 0;

  if(!dev)
    return 0;

  /* find id of newly created dev by id */
  XDeviceInfo	*devices;
  int		num_devices;
  devices = XListInputDevices(dpy, &num_devices); 
  int i;
  for(i = 0; i < num_devices; ++i)
    if(devices[i].id == dev->device_id)
      found = 1;
 
  XFreeDeviceList(devices);

  if(!found)
    return 0;

  /* we can go on safely */
  r.type = CH_RemoveMasterDevice;
  r.device = dev;
  r.returnMode = Floating;

  return (XChangeDeviceHierarchy(dpy, (XAnyHierarchyChangeInfo*)&r, 1) == Success) ? 1 : 0;
}





XDevice* getPairedMD(Display* dpy, XDevice* dev)
{
  XDevice* paired = NULL;
  XDeviceInfo* devices;
  int devicecount;

  if(!dev)
    return paired;

  devices = XListInputDevices(dpy, &devicecount);

  while(devicecount)
    {
      XDeviceInfo* currDevice;

      currDevice = &devices[--devicecount];
      /* ignore slave devices, only masters are interesting */
      if ((currDevice->use == IsXKeyboard || currDevice->use == IsXPointer) && currDevice->id == dev->device_id)
        {
	  /* run through classes, find attach class to get the
	     paried pointer.*/
	  XAnyClassPtr any = currDevice->inputclassinfo;
	  int i;
	  for (i = 0; i < currDevice->num_classes; i++) 
	    {
	      if(any->class == AttachClass)
		{
		  XAttachInfoPtr att = (XAttachInfoPtr)any;
		  paired = XOpenDevice(dpy, att->attached);
		}
	      any = (XAnyClassPtr) ((char *) any + any->length);
	    }
        }
    }
  XFreeDeviceList(devices);

  return paired;
}






/*
  set cursor of pointer dev
  returns the shape as an XCursorImage 
*/
XcursorImage *setPointerShape(Display *dpy, XDevice* dev, float r, float g, float b, char *label)
{
  /* label setup */
  const int idFontSize = 18;
  const int idXOffset = 11;
  const int idYOffset = 25;
  const size_t textsz = 64;
  char text[textsz];
  int total_width, total_height;
  XcursorImage *cursor_image = NULL;

  if(!dev)
    return NULL;

  if(label)
    snprintf(text, textsz, "%s", label);
  else
    snprintf(text, textsz, "%i", (int) dev->device_id);
 
  
  cairo_surface_t* main_surface;
  cairo_surface_t* dummy_surface;
  cairo_surface_t* barecursor_surface;
  cairo_t* cr;

  
  /* simple cursor w/o label */
  barecursor_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 24, 24);
  cr = cairo_create(barecursor_surface);
  cairo_move_to (cr, 1, 1);
  cairo_line_to (cr, 12, 8);
  cairo_line_to (cr, 5, 15);
  cairo_close_path (cr);
  cairo_set_source_rgba(cr, r, g, b, 0.9);
  cairo_fill_preserve (cr);
  cairo_set_source_rgba(cr, 0, 0, 0, 0.8);
  cairo_set_line_width (cr, 0.8);
  cairo_stroke (cr);

    
  /* get estimated text extents */
  cairo_text_extents_t est;
  dummy_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 500, 10);/* ah well, but should fit */
  cr = cairo_create(dummy_surface);
  cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size (cr, idFontSize);
  cairo_text_extents(cr, text, &est);

  /* an from these calculate our final size */
  total_width = (int)(idXOffset + est.width + est.x_bearing);	
  total_height = (int)(idYOffset + est.height + est.y_bearing);	

  /* draw evrything */
  main_surface = cairo_image_surface_create( CAIRO_FORMAT_ARGB32, total_width, total_height );
  cr = cairo_create(main_surface);
  cairo_set_source_surface(cr, barecursor_surface, 0, 0);
  cairo_paint (cr);
  cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size (cr, idFontSize);
  cairo_set_source_rgba (cr, r, g, b, 0.8);
  cairo_move_to(cr, idXOffset, idYOffset);
  cairo_show_text(cr,text);
    
 
  /* copy cairo surface to cursor image */
  cursor_image = XcursorImageCreate(total_width, total_height);
  /* this is important! otherwise we get badmatch, badcursor xerrrors galore... */
  cursor_image->xhot = cursor_image->yhot = 0; 
  memcpy(cursor_image->pixels, cairo_image_surface_get_data (main_surface), sizeof(CARD32) * total_width * total_height);
 
  /* this is another way of doing it... */
  /*
    cairo_surface_write_to_png(main_surface,"/tmp/.mpwm_pointer.png");
    system("echo \"24 0 0 /tmp/.mpwm_pointer.png \" > /tmp/.mpwm_pointer.cfg");
    system("xcursorgen /tmp/.mpwm_pointer.cfg /tmp/.mpwm_pointer.cur");
    Cursor cursor = XcursorFilenameLoadCursor(dpy, "/tmp/.mpwm_pointer.cur");
  */

  /* and display  */
  Cursor cursor = XcursorImageLoadCursor(dpy, cursor_image);

  if(XDefineDeviceCursor(dpy, dev, RootWindow(dpy, DefaultScreen(dpy)), cursor) != Success)
    {
      XcursorImageDestroy(cursor_image);
      cursor_image = NULL;
    }


  /* clean up */
  cairo_destroy(cr);
  cairo_surface_destroy(dummy_surface);
  cairo_surface_destroy(main_surface);
  cairo_surface_destroy(barecursor_surface);
  
  return cursor_image;
}

