
#include <string.h>
#include <X11/extensions/XInput.h> 
#include <cairo.h>
#include <X11/Xproto.h> 
#include <X11/keysym.h> 

#include "xi2_devices.h" 



// does the X version we're running on support XI2?
int xinput2_present;
int use_multipointer;
int xi2_device_creation_in_progress;


// create MD with given name
// returns device pointer
XDevice* createMD(Display* dpy, char* name)
{
  XDevice *dev = NULL;

  XCreateMasterInfo c;
  
  c.type = CH_CreateMasterDevice;
  c.name = name;
  c.sendCore = 1;
  c.enable = 1;
  
  XChangeDeviceHierarchy(dpy, (XAnyHierarchyChangeInfo*)&c, 1);


  // find newly created dev by name
  char handle[256];
  snprintf(handle, 256, "%s pointer", name);

  XDeviceInfo	*devices;
  int		num_devices;
  devices = XListInputDevices(dpy, &num_devices); 
  int i;
  for(i = 0; i < num_devices; ++i) // seems the InputDevices List is already chronologically reversed
    if(strcmp(devices[i].name, handle) == 0)
      dev = XOpenDevice(dpy, devices[i].id);
 
  XFreeDeviceList(devices);


#ifdef HACK
  // not-so-nice hack to get the new MD keyboard set up
  // attach the physical keyboard to our new MD keyboard, send shift up, down
  // and put it back
  
  XDevice *kbd = getPairedMD(dpy, dev); 
  XDevice *slavekbd = NULL;
  XDevice *vck =  XOpenDevice(dpy, 1);     // the virtual core keyboard

  // get attached slave keyb - hopefully the right one
  devices = XListInputDevices(dpy, &num_devices);
  int found = 0;
  for(i = 0; i < num_devices && !found; ++i)
    {
      XDeviceInfo* currDevice;
      currDevice = &devices[i];
      if (currDevice->use == IsXExtensionKeyboard)
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
                  if(att->attached == vck->device_id)
                    {
                      slavekbd = XOpenDevice(dpy, currDevice->id);
                      found = 1;
                      break;
                    }
		}
	      any = (XAnyClassPtr) ((char *) any + any->length);
	    }
        }
    }
  XFreeDeviceList(devices);
  
  fprintf(stderr, "slave k: %i\n", slavekbd->device_id);
  fprintf(stderr, "master k: %i\n", kbd->device_id);
  fprintf(stderr, "master p: %i\n", dev->device_id);
   
  XChangeAttachmentInfo ca;
  ca.type = CH_ChangeAttachment; 
  ca.changeMode = AttachToMaster; 
  ca.device = slavekbd;
  ca.newMaster = kbd;
  XChangeDeviceHierarchy(dpy, (XAnyHierarchyChangeInfo*)&ca, 1);

  XTestFakeDeviceKeyEvent(dpy, slavekbd, XKeysymToKeycode(dpy, XK_Shift_L), 1, NULL, 0, 1);
  XTestFakeDeviceKeyEvent(dpy, slavekbd, XKeysymToKeycode(dpy, XK_Shift_L), 0, NULL, 0, 1);

  ca.device = slavekbd;
  ca.newMaster = vck;
  XChangeDeviceHierarchy(dpy, (XAnyHierarchyChangeInfo*)&ca, 1);
  // HACK END
#endif  

  return dev;
}



// remove device 
// return 1 on success, 0 on failure
int removeMD(Display* dpy, XDevice* dev)
{
  XRemoveMasterInfo r;
  int found = 0;

  if(!dev)
    return 0;

  // find id of newly created dev by id
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

  // we can go on safely
  r.type = CH_RemoveMasterDevice;
  r.device = dev;
#ifndef HACK
  r.returnMode = Floating;
#else
  // HACK START
  r.returnMode = AttachToMaster;
  r.returnPointer = XOpenDevice(dpy, 0);
  r.returnKeyboard = XOpenDevice(dpy, 1);
  // HACK END
#endif

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
      // ignore slave devices, only masters are interesting 
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






// set cursor of pointer dev
// returns the shape as an XCursorImage
XcursorImage *setPointerShape(Display *dpy, XDevice* dev, float r, float g, float b, char *label)
{
  // label setup
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

  
  // simple cursor w/o label
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

    
  // get estimated text extents
  cairo_text_extents_t est;
  dummy_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 500, 10);// ah well, but should fit
  cr = cairo_create(dummy_surface);
  cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size (cr, idFontSize);
  cairo_text_extents(cr, text, &est);

  // an from these calculate our final size
  total_width = (int)(idXOffset + est.width + est.x_bearing);	
  total_height = (int)(idYOffset + est.height + est.y_bearing);	

  // draw evrything
  main_surface = cairo_image_surface_create( CAIRO_FORMAT_ARGB32, total_width, total_height );
  cr = cairo_create(main_surface);
  cairo_set_source_surface(cr, barecursor_surface, 0, 0);
  cairo_paint (cr);
  cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size (cr, idFontSize);
  cairo_set_source_rgba (cr, r, g, b, 0.8);
  cairo_move_to(cr, idXOffset, idYOffset);
  cairo_show_text(cr,text);
    
 
  // copy cairo surface to cursor image
  cursor_image = XcursorImageCreate(total_width, total_height);
  cursor_image->xhot = cursor_image->yhot = 0; // this is important! otherwise we get badmatch, badcursor xerrrors galore...
  memcpy(cursor_image->pixels, cairo_image_surface_get_data (main_surface), sizeof(CARD32) * total_width * total_height);
 
  // this is another way of doing it...
  /*
    cairo_surface_write_to_png(main_surface,"/tmp/.mpwm_pointer.png");
    system("echo \"24 0 0 /tmp/.mpwm_pointer.png \" > /tmp/.mpwm_pointer.cfg");
    system("xcursorgen /tmp/.mpwm_pointer.cfg /tmp/.mpwm_pointer.cur");
    Cursor cursor = XcursorFilenameLoadCursor(dpy, "/tmp/.mpwm_pointer.cur");
  */

  // and display 
  Cursor cursor = XcursorImageLoadCursor(dpy, cursor_image);

  if(XDefineDeviceCursor(dpy, dev, RootWindow(dpy, DefaultScreen(dpy)), cursor) != Success)
    {
      XcursorImageDestroy(cursor_image);
      cursor_image = NULL;
    }


  // clean up
  cairo_destroy(cr);
  cairo_surface_destroy(dummy_surface);
  cairo_surface_destroy(main_surface);
  cairo_surface_destroy(barecursor_surface);
  
  return cursor_image;
}

