
#include <string.h>
#include <cairo.h>
#include <X11/Xproto.h> 
#include <X11/keysym.h> 

#include "xi2_devices.h" 
#include "cleanup.h"




/* does the X version we're running on support XI2? */
int xinput2_present;
int use_multipointer;
int xi2_device_creation_in_progress;


/*
  create MD with given name
  returns device id, -1 on error
*/
int createMD(Display* dpy, char* name)
{
  int dev_id = -1;
  XErrorHandler old_handler;
  XICreateMasterInfo c;

  c.type = XICreateMaster;
  c.name = name;
  c.sendCore = 1;
  c.enable = 1;

  trapped_xerror = 0;
  old_handler = XSetErrorHandler(trap_xerror);
	
  XIChangeHierarchy(dpy, (XIAnyHierarchyChangeInfo*)&c, 1);
  
  XSync(dpy, False);
  if(trapped_xerror)
    {
      XSetErrorHandler(old_handler);
      trapped_xerror = 0;
      return -1;
    }

  XSetErrorHandler(old_handler);
  trapped_xerror = 0;

  /* find newly created dev by name
     FIXME: better wait for XIHierarchy event here? */
  char handle[256];
  snprintf(handle, 256, "%s pointer", name);

  XIDeviceInfo	*devinfo;
  int		num_devices;
  devinfo = XIQueryDevice(dpy, XIAllMasterDevices, &num_devices);

  int i;
  for(i = num_devices-1; i >= 0; --i) 
    if(strcmp(devinfo[i].name, handle) == 0)
      {
	dev_id = devinfo[i].deviceid;
	break;
      }
 
  XIFreeDeviceInfo(devinfo);
      
  return dev_id;
}



/*
  remove device 
  return 1 on success, 0 on failure
*/
int removeMD(Display* dpy, int dev_id)
{
  XIRemoveMasterInfo r;
  int found = 0;

  if(dev_id < 0)
    return 0;

  /* see if this device exists */
  XIDeviceInfo	*devinfo;
  int		num_devices;
  devinfo = XIQueryDevice(dpy, XIAllMasterDevices, &num_devices);
  int i;
  for(i = 0; i < num_devices; ++i)
    if(devinfo[i].deviceid == dev_id)
      found = 1;
 
  XIFreeDeviceInfo(devinfo);

  if(!found)
    return 0;

  /* we can go on safely */
  r.type = XIRemoveMaster;
  r.device = dev_id;
  r.returnMode = XIFloating;

  return (XIChangeHierarchy(dpy, (XIAnyHierarchyChangeInfo*)&r, 1) == Success) ? 1 : 0;
}





int getPairedMD(Display* dpy, int dev_id)
{
  int paired = -1;
  XIDeviceInfo* devinfo;
  int devicecount = 0;

  if(dev_id < 0)
    return paired;

  devinfo = XIQueryDevice(dpy, dev_id, &devicecount);

  if(devicecount)
    paired = devinfo->attachment;

  
  XIFreeDeviceInfo(devinfo);

  return paired;
}






/*
  set cursor of pointer dev
  returns the shape as an XCursorImage 
*/
XcursorImage *setPointerShape(Display *dpy, int dev_id, float r, float g, float b, char *label)
{
  /* label setup */
  const int idFontSize = 18;
  const int idXOffset = 11;
  const int idYOffset = 25;
  const size_t textsz = 64;
  char text[textsz];
  int total_width, total_height;
  XcursorImage *cursor_image = NULL;

  if(dev_id < 0)
    return NULL;

  if(label)
    snprintf(text, textsz, "%s", label);
  else
    snprintf(text, textsz, "%i", (int) dev_id);
 
  
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
 

  /* and display  */
  Cursor cursor = XcursorImageLoadCursor(dpy, cursor_image);

  if(XIDefineCursor(dpy, dev_id, RootWindow(dpy, DefaultScreen(dpy)), cursor) != Success)
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

