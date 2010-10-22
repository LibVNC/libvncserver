/*
  Based on the unclutter utility by Mark M Martin, mmm@cetia.fr  sep 1992.,
  this small (quick&dirty) helper program sets the cursor of a given (or the
  last added) pointer device and afterwards tracks the pointer position and 
  uses unclutters subwindow trick to always show a labeled cursor, even if the 
  window the pointer is in specifies another one.
*/


#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/extensions/XInput2.h> 
#include <X11/extensions/XInput.h> /* for BadDevice() */
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <regex.h>
#include <cairo.h> 



#define CW_SIZE 10 /* width and height of cursor window */
#define SLEEPTIME 50 /* milliseconds to wait in between pointer queries */


char *progname;

char **names = 0;	/* -> argv list of names to avoid */
char **classes = 0;     /* -> argv list of classes to avoid */
regex_t *nc_re = 0;     /* regex for list of classes/names to avoid */

Display *display;

int dev = -1; /* device id */
int numscreens;
Window *realroot;  /* array of root windows */

/* XI error numbers are dynamically generated, 
   we have to get the via macros, e.g. BadDevice(display, &my_errorcode)
*/
int baddevice_err; 


void pexit(char *str)
{
  fprintf(stderr,"%s: %s\n",progname,str);
  exit(EXIT_FAILURE);
}



void usage()
{
  pexit("usage:\n\
	-display <display>\n\
	-dev <device id>	open device with this id instead of last\n\
                                created pointer\n\
	-label <label>		cursor label to apply. if none, use device id\n\
	-color <X11 color>	color of the cursor, given as an X11 color name\n\
                                if none is given, color depends on device id\n\
	-onescreen		apply only to given screen of display\n\
 	-visible       		ignore visibility events\n\
 	-noevents      		don't send pseudo events\n\
	-regex			name or class below is a regular expression\n\
	-not names...		don't apply to windows whose wm-name begins.\n\
				(must be last argument)\n\
	-notname names...	same as -not names...\n\
	-notclass classes...    don't apply to windows whose wm-class begins.\n\
				(must be last argument, cannot be used with\n\
				-not or -notname)");
}






/* Since the small window we create is a child of the window the pointer is
 * in, it can be destroyed by its adoptive parent.  Hence our destroywindow()
 * can return an error, saying it no longer exists.  Similarly, the parent
 * window can disappear while we are trying to create the child. Trap and
 * ignore these errors.
 
 also, it's possible that the device gets removed while xisetcursor is running,
 so ignore XI_BadDevice errors as well.

*/
int (*defaulthandler)();

int errorhandler(Display *display, XErrorEvent *error)
{
  if(error->error_code != BadWindow && error->error_code != baddevice_err) 
    (*defaulthandler)(display,error);
  return 1;
}



/*
 * return true if window has a wm_name (class) and the start of it matches
 * one of the given names (classes) to avoid
 */
int nameinlist(display,window)
     Display *display;
     Window window;
{
  char **cpp;
  char *name = 0;

  if(names)
    XFetchName (display, window, &name);
  else if(classes){
    XClassHint *xch = XAllocClassHint();
    if(XGetClassHint (display, window, xch))
      name = strdup(xch->res_class);
    if(xch)
      XFree(xch);
  }else
    return 0;

  if(name){
    if(nc_re){
      if(!regexec(nc_re, name, 0, 0, 0)) {
	XFree(name);
	return 1;
      }
    }else{
      for(cpp = names!=0 ? names : classes;*cpp!=0;cpp++){
	if(strncmp(*cpp,name,strlen(*cpp))==0)
	  break;
      }
      XFree(name);
      return(*cpp!=0);
    }
  }
  return 0;
}	


void cleanup()
{
  int screen;
  for(screen = 0; screen < numscreens; ++screen)
    {
      realroot[screen] = XRootWindow(display,screen);
      
      /* finally ... */
      if(XIUndefineCursor(display, dev, realroot[screen]) != Success)
	printf("undefine cursor failed\n");  

      XFlush(display);
    }
}



/*
 * 
 */
Cursor createCursor(Display *dpy, int dev, Window root, float r, float g, float b, const char* label)
{
  /* label setup */
  const int idFontSize = 18;
  const int idXOffset = 11;
  const int idYOffset = 25;
  const size_t textsz = 64;
  char text[textsz];
  int total_width, total_height;
  XcursorImage *cursor_image = NULL;

  if(dev < 0)
    return 0;


  if(label)
    snprintf(text, textsz, "%s", label);
  else
    snprintf(text, textsz, "%i", dev);
 
  
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
 
 
  Cursor cursor = XcursorImageLoadCursor(dpy, cursor_image);
  

  /* clean up */
  XcursorImageDestroy(cursor_image);

  cairo_destroy(cr);
  cairo_surface_destroy(dummy_surface);
  cairo_surface_destroy(main_surface);
  cairo_surface_destroy(barecursor_surface);
  
  return cursor;
}





int main(int argc, char** argv)
{
  int xi2opcode;
  int screen;
  int oldx = -99, oldy=-99, dovisible = 1, doevents = 1, onescreen = 0, dev_wanted = -1;
  Cursor *cursor;
  Window root;
  char *displayname = 0;
  char *label = 0;
  XColor color;
  char* colorspec = 0;
  XIDeviceInfo* devinfo;
  int num, ignore;

  
  /* call exit() on interrupt and termination */
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = &exit;
  sigaction(SIGINT, &action, NULL);
  sigaction(SIGTERM, &action, NULL);
  /* this in turn calls cleanup */
  atexit(cleanup);
    

  progname = *argv;
  argc--;
  while(argv++,argc-->0){

    if(strcmp(*argv,"-noevents")==0){
      doevents = 0;
    }else if(strcmp(*argv,"-dev")==0){
      argc--,argv++;
      if( argc<0 || atoi(*argv)<0 )
	usage();
      dev_wanted = atoi(*argv);}
    else if(strcmp(*argv,"-onescreen")==0){
      onescreen = 1;
    }else if(strcmp(*argv,"-visible")==0){
      dovisible = 0;
    }else if(strcmp(*argv,"-regex")==0){
      nc_re = (regex_t *)malloc(sizeof(regex_t));
    }else if(strcmp(*argv,"-not")==0 || strcmp(*argv,"-notname")==0){
      /* take rest of srg list */
      names = ++argv;
      if(*names==0)names = 0;	/* no args follow */
      argc = 0;
    }else if(strcmp(*argv,"-notclass")==0){
      /* take rest of arg list */
      classes = ++argv;
      if(*classes==0)classes = 0;	/* no args follow */
      argc = 0;
    }else if(strcmp(*argv,"-display")==0 || strcmp(*argv,"-d")==0){
      argc--,argv++;
      if(argc<0)usage();
      displayname = *argv;
    }else if(strcmp(*argv,"-label")==0){
      argc--,argv++;
      if(argc<0)usage();
      label = *argv;
    }else if(strcmp(*argv,"-color")==0){
      argc--,argv++;
      if(argc<0)usage();
      colorspec = *argv;
    }else usage();
  }

 

  /* compile a regex from the first name or class */
  if(nc_re){
    if(names || classes){
      if (regcomp(nc_re, (names != 0 ? *names : *classes),
		  REG_EXTENDED | REG_NOSUB)) { /* error */
	free(nc_re);
	names = classes = 0;
	nc_re = 0;
      }
    }else{ /* -regex without -not... ... */
      free(nc_re);
      nc_re = 0;
    }
  }

  display = XOpenDisplay(displayname);
  if(!display)
    pexit("could not open display");


  if(colorspec)
    if(! XParseColor(display, DefaultColormap(display, DefaultScreen(display)), colorspec, &color))
      pexit("could not find color\n");

  /* get error code */
  BadDevice(display, baddevice_err);
  
  // query XI and XI2
  int major = 2, minor = 0;
  if(! XQueryExtension (display, "XInputExtension", &xi2opcode, &ignore, &ignore))
    pexit("No XInput Extension!");
  
  if (XIQueryVersion(display, &major, &minor) != Success)
    pexit("XINPUT 2 not available!\n");

    
  if(dev_wanted >= 0)/* just look for this device */
    {
      devinfo = XIQueryDevice(display, dev_wanted, &num);
      if(!num)
	pexit("Unable to find specified device!\n");
      else
	dev = dev_wanted;
    }
  else /* use the last added pointer dev */
    {
      devinfo = XIQueryDevice(display, XIAllMasterDevices, &num);
      
      int i;
      for(i = num-1; i >= 0; --i) 
        if (devinfo[i].use == XIMasterPointer)
	  {
	    dev = devinfo[i].deviceid;
	    break;
	  }
    }  
  XIFreeDeviceInfo(devinfo);
  

  printf("%s: using device with id: %i\n", progname, dev);



  numscreens = ScreenCount(display);
  cursor = (Cursor*) malloc(numscreens*sizeof(Cursor));
  realroot = (Window*) malloc(numscreens*sizeof(Window));

  /* each screen needs its own empty cursor.
   * note each real root id so can find which screen we are on
   */
  for(screen = 0;screen<numscreens;screen++)
    if(onescreen && screen!=DefaultScreen(display))
      {
	realroot[screen] = -1;
	cursor[screen] = -1;
      }
    else
      {
	realroot[screen] = XRootWindow(display,screen);
	if(colorspec)
	  {
	    float div = 65535; /* XColor stores RBG from 0 to 65535 */
	    cursor[screen] = createCursor(display, dev, realroot[screen], color.red/div, color.green/div, color.blue/div, label);
	  }
	else
	  cursor[screen] = createCursor(display, dev, realroot[screen], 0.4*(dev%3), 0.2*(dev%5), 1*(dev%2), label);
	
	/* finally ... */
	if(XIDefineCursor(display, dev, realroot[screen], cursor[screen]) != Success)
	  printf("define cursor failed\n");  
      }
  
  screen = DefaultScreen(display);
  root = RootWindow(display,screen);

  defaulthandler = XSetErrorHandler(errorhandler);

 

  /*
   * create a small unmapped window on a screen just so xdm can use
   * it as a handle on which to killclient() us.
   */
  XCreateWindow(display, realroot[screen], 0,0,1,1, 0, CopyFromParent,
		InputOutput, CopyFromParent, 0, (XSetWindowAttributes*)0);

 
  /*
    register for device hierarchy changes
  */
  XIEventMask mask;
  unsigned char bits[4] = {0};
  
  mask.mask = bits;
  mask.mask_len = sizeof(bits);
  mask.deviceid = XIAllDevices;
  XISetMask(bits, XI_HierarchyChanged);
  
  XISelectEvents(display, DefaultRootWindow(display), &mask, 1);
	

 
  while(1)
    {
      Window dummywin,windowin,newroot;
      double rootx,rooty,winx,winy;
      XIModifierState modifs;
      XIGroupState group;
      XIButtonState buttons;
      Window lastwindowavoided = None;
	
      /*
       * 
       */
      while(1)
	{
	  if(!XIQueryPointer(display, dev, root, &newroot, &windowin,
			     &rootx, &rooty, &winx, &winy, &buttons, &modifs, &group))
	    {
	      /* window manager with virtual root may have restarted
	       * or we have changed screens */
	      if(!onescreen)
		{
		  for(screen = 0;screen<numscreens;screen++)
		    if(newroot==realroot[screen])break;
		  if(screen>=numscreens)
		    pexit("not on a known screen");
		}
	      root = RootWindow(display,screen);
	    }
	  else 
	    /* check if any bit is set */
	    if((*(buttons.mask)) || !(rootx == oldx && rooty == oldy))
	      oldx = rootx, oldy = rooty;
	  
	    else 
	      if(windowin==None)
		{
		  windowin = root;
		  break;
		}
	      else 
		if(windowin!=lastwindowavoided)
		  {
		    /* descend tree of windows under cursor to bottommost */
		    Window childin;
		    int toavoid = xFalse;
		    lastwindowavoided = childin = windowin;
		    do
		      {
			windowin = childin;
			if(nameinlist (display, windowin))
			  {
			    toavoid = xTrue;
			    break;
			  }
		      }
		    while(XIQueryPointer(display, dev, windowin, &dummywin,
					 &childin, &rootx, &rooty, &winx, &winy, 
					 &buttons, &modifs, &group)
			  && childin!=None);
		    if(!toavoid)
		      {
			lastwindowavoided = None;
			break;
		      }
		  }

	  usleep(1000* SLEEPTIME);
	}


      XSetWindowAttributes attributes;
      XEvent event;
      Window cursorwindow;
  	    
      /* create small input-only window under cursor
       * as a sub window of the window currently under the cursor
       */
      attributes.event_mask = LeaveWindowMask |
	EnterWindowMask |
	StructureNotifyMask |
	FocusChangeMask;
      if(dovisible)
	attributes.event_mask |= VisibilityChangeMask;
      attributes.override_redirect = True;
      attributes.cursor = cursor[screen];

      cursorwindow = XCreateWindow(display, windowin,
				   winx - CW_SIZE/2, winy - CW_SIZE/2,
				   CW_SIZE, CW_SIZE, 0, CopyFromParent,
				   InputOnly, CopyFromParent, 
				   CWOverrideRedirect | CWEventMask | CWCursor,
				   &attributes);

      /* discard old events for previously created windows */
      XSync(display,True);


      XMapWindow(display,cursorwindow);
      /*
       * Dont wait for expose/map cos override and inputonly(?).
       * Check that created window captured the pointer by looking
       * for inevitable EnterNotify event that must follow MapNotify.
       * [Bug fix thanks to Charles Hannum <mycroft@ai.mit.edu>]
       */
      XSync(display,False);
      if(!XCheckTypedWindowEvent(display, cursorwindow, EnterNotify,
				 &event))
	oldx = -1;	/* slow down retry */
      else{
	if(doevents)
	  {
	    /*
	     * send a pseudo EnterNotify event to the parent window
	     * to try to convince application that we didnt really leave it
	     */
	    event.xcrossing.type = EnterNotify;
	    event.xcrossing.display = display;
	    event.xcrossing.window = windowin;
	    event.xcrossing.root = root;
	    event.xcrossing.subwindow = None;
	    event.xcrossing.time = CurrentTime;
	    event.xcrossing.x = winx;
	    event.xcrossing.y = winy;
	    event.xcrossing.x_root = rootx;
	    event.xcrossing.y_root = rooty;
	    event.xcrossing.mode = NotifyNormal;
	    event.xcrossing.same_screen = True;
	    event.xcrossing.focus = True;
	    //event.xcrossing.state = modifs;
	    (void)XSendEvent(display,windowin,
			     True/*propagate*/,EnterWindowMask,&event);
	  }

	/* wait till pointer leaves window */
	while(1)
	  {
	    XNextEvent(display,&event);

	    if (event.xcookie.type == GenericEvent &&
		event.xcookie.extension == xi2opcode &&
		XGetEventData(display, &event.xcookie))
	      {
		if(event.xcookie.evtype == XI_HierarchyChanged)
		  {
		    devinfo = XIQueryDevice(display, dev, &num);
		    if(!devinfo)
		      {
			XDestroyWindow(display, cursorwindow);
		        fprintf(stderr, "device %d removed, ", dev);
			pexit("exiting.\n");
		      }
		    XIFreeDeviceInfo(devinfo);
		  }
	      }
	    XFreeEventData(display, &event.xcookie);
	    
	    
	    if(event.type==LeaveNotify ||
	       event.type==FocusOut ||
	       event.type==UnmapNotify ||
	       event.type==ConfigureNotify ||
	       event.type==CirculateNotify ||
	       event.type==ReparentNotify ||
	       event.type==DestroyNotify ||
	       (event.type==VisibilityNotify && event.xvisibility.state!=VisibilityUnobscured)
	       )
	      break;
	  }

	/* check if a second unclutter is running cos they thrash */
	/* if(event.type==LeaveNotify &&
	   event.xcrossing.window==cursorwindow &&
	   event.xcrossing.detail==NotifyInferior)
	   pexit("someone created a sub-window to my sub-window! giving up");
	*/
      }
      XDestroyWindow(display, cursorwindow);
    }
}
