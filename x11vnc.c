/* This file is part of LibVNCServer. It is a small clone of x0rfbserver by HexoNet, demonstrating the
   capabilities of LibVNCServer.
*/

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#ifndef NO_SHM
#include <X11/extensions/XShm.h>
#include <sys/shm.h>
#endif
#define KEYSYM_H
#include "rfb.h"

Display *dpy = 0;
int c=0,blockLength = 32;
Bool gotInput = FALSE;

Bool disconnectAfterFirstClient = TRUE;

/* keyboard handling */

char modifiers[0x100];
KeyCode keycodes[0x100],leftShiftCode,rightShiftCode,altGrCode;

void init_keycodes()
{
  KeySym key,*keymap;
  int i,j,minkey,maxkey,syms_per_keycode;

  memset(modifiers,-1,sizeof(modifiers));

  XDisplayKeycodes(dpy,&minkey,&maxkey);
  keymap=XGetKeyboardMapping(dpy,minkey,(maxkey - minkey + 1),&syms_per_keycode);

  for (i = minkey; i <= maxkey; i++)
    for(j=0;j<syms_per_keycode;j++) {
      key=keymap[(i-minkey)*syms_per_keycode+j];
      if(key>=' ' && key<0x100 && i==XKeysymToKeycode(dpy,key)) {
	keycodes[key]=i;
	modifiers[key]=j;
      }
    }

  leftShiftCode=XKeysymToKeycode(dpy,XK_Shift_L);
  rightShiftCode=XKeysymToKeycode(dpy,XK_Shift_R);
  altGrCode=XKeysymToKeycode(dpy,XK_Mode_switch);

  XFree ((char *) keymap);
}

/* the hooks */

void clientGone(rfbClientPtr cl)
{
  exit(0);
}

void newClient(rfbClientPtr cl)
{
  if(disconnectAfterFirstClient)
    cl->clientGoneHook = clientGone;
}

#define LEFTSHIFT 1
#define RIGHTSHIFT 2
#define ALTGR 4
char ModifierState = 0;

/* this function adjusts the modifiers according to mod (as from modifiers) and ModifierState */

void tweakModifiers(char mod,Bool down)
{
  Bool isShift=ModifierState&(LEFTSHIFT|RIGHTSHIFT);
  if(mod<0) return;
  if(isShift && mod!=1) {
    if(ModifierState&LEFTSHIFT)
      XTestFakeKeyEvent(dpy,leftShiftCode,!down,CurrentTime);
    if(ModifierState&RIGHTSHIFT)
      XTestFakeKeyEvent(dpy,rightShiftCode,!down,CurrentTime);
  }
  if(!isShift && mod==1)
    XTestFakeKeyEvent(dpy,leftShiftCode,down,CurrentTime);

  if(ModifierState&ALTGR && mod!=2)
    XTestFakeKeyEvent(dpy,altGrCode,!down,CurrentTime);
  if(!(ModifierState&ALTGR) && mod==2)
    XTestFakeKeyEvent(dpy,altGrCode,down,CurrentTime);
}

void keyboard(Bool down,KeySym keySym,rfbClientPtr cl)
{
#define ADJUSTMOD(sym,state) \
  if(keySym==sym) { if(down) ModifierState|=state; else ModifierState&=~state; }

  ADJUSTMOD(XK_Shift_L,LEFTSHIFT)
  ADJUSTMOD(XK_Shift_R,RIGHTSHIFT)
  ADJUSTMOD(XK_Mode_switch,ALTGR)

  if(keySym>=' ' && keySym<0x100) {
    KeyCode k;
    /* if(down)
       tweakModifiers(modifiers[keySym],True); */
    tweakModifiers(modifiers[keySym],down);
    XTestFakeKeyEvent(dpy,XK_Shift_R,True,CurrentTime);
    k = XKeysymToKeycode( dpy,keySym );
    if(k!=NoSymbol) {
      XTestFakeKeyEvent(dpy,k,down,CurrentTime);
      gotInput = TRUE;
    }
    /*XTestFakeKeyEvent(dpy,keycodes[keySym],down,CurrentTime);*/
    /*if(down)
      tweakModifiers(modifiers[keySym],False);*/
    gotInput = TRUE;
  } else {
    KeyCode k = XKeysymToKeycode( dpy,keySym );
    if(k!=NoSymbol) {
      XTestFakeKeyEvent(dpy,k,down,CurrentTime);
      gotInput = TRUE;
    }
  }
}

int oldButtonMask = 0;

void mouse(int buttonMask,int x,int y,rfbClientPtr cl)
{
  int i=0;
  //fprintf(stderr,"/");
  XTestFakeMotionEvent(dpy,0,x,y,CurrentTime );
  while(i<5) {
    if ((oldButtonMask&(1<<i))!=(buttonMask&(1<<i)))
      XTestFakeButtonEvent(dpy,i+1,(buttonMask&(1<<i))?True:False,CurrentTime);
    i++;
  }
  oldButtonMask = buttonMask;
  //fprintf(stderr,"-");
  gotInput = TRUE;
}

/* the X11 interaction */

#ifndef NO_SHM
Bool useSHM = TRUE;
XShmSegmentInfo shminfo;
#else
Bool useSHM = FALSE;
#endif

void getImage(int bpp,Display *dpy,int xscreen,XImage **i)
{
  if(useSHM && bpp>0) {
    static Bool firstTime = TRUE;
    if(firstTime) {
      firstTime = FALSE;
      *i = XShmCreateImage( dpy,
			    DefaultVisual( dpy, xscreen ),
			    bpp,
			    ZPixmap,
			    NULL,
			    &shminfo,
			    DisplayWidth(dpy,xscreen),
			    DisplayHeight(dpy,xscreen));

      if(*i == 0) {
	useSHM = FALSE;
	getImage(bpp,dpy,xscreen,i);
	return;
      }

      shminfo.shmid = shmget( IPC_PRIVATE,
			      (*i)->bytes_per_line * (*i)->height,
			      IPC_CREAT | 0777 );
      shminfo.shmaddr = (*i)->data = (char *) shmat( shminfo.shmid, 0, 0 );
      shminfo.readOnly = False;
      XShmAttach( dpy, &shminfo );
    }

    XShmGetImage(dpy,RootWindow(dpy,xscreen),*i,0,0,AllPlanes);
  } else {
    *i = XGetImage(dpy,RootWindow(dpy,xscreen),0,0,DisplayWidth(dpy,xscreen),DisplayHeight(dpy,xscreen),
		    AllPlanes,ZPixmap );
  }
}

void checkForImageUpdates(rfbScreenInfoPtr s,char *b)
{
   Bool changed;
   int i,j,k,l,x1,y1;
   for(j=0;j<s->height;j+=blockLength)
     for(i=0;i<s->width;i+=blockLength) {
	y1=j+blockLength; if(y1>s->height) y1=s->height;
	x1=i+blockLength; if(x1>s->width) x1=s->width;
	y1*=s->paddedWidthInBytes;
	x1*=s->bitsPerPixel/8;
	changed=FALSE;
	for(l=j*s->paddedWidthInBytes;l<y1;l+=s->paddedWidthInBytes)
	  for(k=i*s->bitsPerPixel/8;k<x1;k++)
	    if(s->frameBuffer[l+k]!=b[l+k]) {
	      //	       fprintf(stderr,"changed: %d, %d\n",k,l);
	       changed=TRUE;
	       goto changed_p;
	    }
	if(changed) {
	   changed_p:
	  for(l+=i*s->bitsPerPixel/8;l<y1;l+=s->paddedWidthInBytes)
	     memcpy(/*b+l,*/s->frameBuffer+l,b+l,x1-i*s->bitsPerPixel/8);
	   rfbMarkRectAsModified(s,i,j,i+blockLength,j+blockLength);
	}
     }
}

/* the main program */

int main(int argc,char** argv)
{
  //Screen *sc;
  //Colormap cm;
  XImage *framebufferImage;
  char *backupImage;
  int xscreen,i;
  rfbScreenInfoPtr screen;
  int maxMsecsToConnect = 5000; /* a maximum of 5 seconds to connect */
  int updateCounter = 20; /* about every 50 ms a screen update should be made. */

  for(i=argc-1;i>0;i--)
    if(i<argc-1 && strcmp(argv[i],"-display")==0) {
      fprintf(stderr,"Using display %s\n",argv[i+1]);
      dpy = XOpenDisplay(argv[i+1]);
      if(dpy==0) {
	fprintf(stderr,"Couldn't connect to display \"%s\".\n",argv[i+1]);
	exit(1);
      }
    } else if(strcmp(argv[i],"-noshm")==0) {
      useSHM = FALSE;
    } else if(strcmp(argv[i],"-runforever")==0) {
      disconnectAfterFirstClient = FALSE;
    } else if(i<argc-1 && strcmp(argv[i],"-wait4client")==0) {
      maxMsecsToConnect = atoi(argv[i+1]);
    } else if(i<argc-1 && strcmp(argv[i],"-update")==0) {
      updateCounter = atoi(argv[i+1]);
    }

  if(dpy==0)
    dpy = XOpenDisplay("");
  if(dpy==0) {
    fprintf(stderr,"Couldn't open display!\n");
    exit(2);
  }

  XTestGrabControl(dpy,True);

  xscreen = DefaultScreen(dpy);

  init_keycodes();

  getImage(0,dpy,xscreen,&framebufferImage);

  screen = rfbGetScreen(&argc,argv,framebufferImage->width,
			framebufferImage->height,
			framebufferImage->bits_per_pixel,
			8,
			framebufferImage->bits_per_pixel/8);
   
  screen->paddedWidthInBytes = framebufferImage->bytes_per_line;

  screen->rfbServerFormat.bitsPerPixel = framebufferImage->bits_per_pixel;
  screen->rfbServerFormat.depth = framebufferImage->depth;
  //rfbEndianTest = framebufferImage->bitmap_bit_order != MSBFirst;
  screen->rfbServerFormat.trueColour = TRUE;

  if ( screen->rfbServerFormat.bitsPerPixel == 8 ) {
     if(CellsOfScreen(ScreenOfDisplay(dpy,xscreen))) {
	XColor color[256];
	int i;
        screen->colourMap.count = 256;
	screen->rfbServerFormat.trueColour = FALSE;
	screen->colourMap.is16 = TRUE;
        for(i=0;i<256;i++)
	  color[i].pixel=i;
	XQueryColors(dpy,DefaultColormap(dpy,xscreen),color,256);
	screen->colourMap.data.shorts = (short*)malloc(3*sizeof(short)*screen->colourMap.count);
	for(i=0;i<screen->colourMap.count;i++) {
	   screen->colourMap.data.shorts[i*3+0] = color[i].red;
	   screen->colourMap.data.shorts[i*3+1] = color[i].green;
	   screen->colourMap.data.shorts[i*3+2] = color[i].blue;
	}
     } else {
	screen->rfbServerFormat.redShift = 0;
	screen->rfbServerFormat.greenShift = 2;
	screen->rfbServerFormat.blueShift = 5;
	screen->rfbServerFormat.redMax   = 3;
	screen->rfbServerFormat.greenMax = 7;
	screen->rfbServerFormat.blueMax  = 3;
     }
  } else {
    screen->rfbServerFormat.redShift = 0;
    if ( framebufferImage->red_mask )
      while ( ! ( framebufferImage->red_mask & (1 << screen->rfbServerFormat.redShift) ) )
        screen->rfbServerFormat.redShift++;
    screen->rfbServerFormat.greenShift = 0;
    if ( framebufferImage->green_mask )
      while ( ! ( framebufferImage->green_mask & (1 << screen->rfbServerFormat.greenShift) ) )
        screen->rfbServerFormat.greenShift++;
    screen->rfbServerFormat.blueShift = 0;
    if ( framebufferImage->blue_mask )
      while ( ! ( framebufferImage->blue_mask & (1 << screen->rfbServerFormat.blueShift) ) )
      screen->rfbServerFormat.blueShift++;
    screen->rfbServerFormat.redMax   = framebufferImage->red_mask   >> screen->rfbServerFormat.redShift;
    screen->rfbServerFormat.greenMax = framebufferImage->green_mask >> screen->rfbServerFormat.greenShift;
    screen->rfbServerFormat.blueMax  = framebufferImage->blue_mask  >> screen->rfbServerFormat.blueShift;
  }

  backupImage = malloc(screen->height*screen->paddedWidthInBytes);
  memcpy(backupImage,framebufferImage->data,screen->height*screen->paddedWidthInBytes);

  screen->frameBuffer = backupImage;
  screen->cursor = 0;
  screen->newClientHook = newClient;
  screen->kbdAddEvent = keyboard;
  screen->ptrAddEvent = mouse;

  screen->rfbDeferUpdateTime = 1;
  updateCounter /= screen->rfbDeferUpdateTime;

  rfbInitServer(screen);

  c=0;
  while(1) {
    if(screen->rfbClientHead)
      maxMsecsToConnect = 5000;
    maxMsecsToConnect -= screen->rfbDeferUpdateTime;
    if(maxMsecsToConnect<0) {
      fprintf(stderr,"Maximum time to connect reached. Exiting.\n");
      XTestDiscard(dpy);
      exit(2);
    }

    rfbProcessEvents(screen,-1);

    if(gotInput) {
      gotInput = FALSE;
      c=updateCounter;
    } else if(screen->rfbClientHead && c++>updateCounter) {
      c=0;
      //fprintf(stderr,"*");
      if(!useSHM)
	framebufferImage->f.destroy_image(framebufferImage);
      getImage(screen->rfbServerFormat.bitsPerPixel,dpy,xscreen,&framebufferImage);
      checkForImageUpdates(screen,framebufferImage->data);
      //fprintf(stderr,"+");
    }
#ifdef WRITE_SNAPS
       {
	  int i,j,r,g,b;
	  FILE* f=fopen("test.pnm","wb");
	  fprintf(f,"P6\n%d %d\n255\n",screen->width,screen->height);
	  for(j=0;j<screen->height;j++)
	    for(i=0;i<screen->width;i++) {
	      //r=screen->frameBuffer[j*screen->paddedWidthInBytes+i*2];
	      r=framebufferImage->data[j*screen->paddedWidthInBytes+i*2];
	       fputc(((r>>screen->rfbServerFormat.redShift)&screen->rfbServerFormat.redMax)*255/screen->rfbServerFormat.redMax,f);
	       fputc(((r>>screen->rfbServerFormat.greenShift)&screen->rfbServerFormat.greenMax)*255/screen->rfbServerFormat.greenMax,f);
	       fputc(((r>>screen->rfbServerFormat.blueShift)&screen->rfbServerFormat.blueMax)*255/screen->rfbServerFormat.blueMax,f);
	    }
	  fclose(f);
       }
#endif
  }
#ifndef NO_SHM
  //XShmDetach(dpy,framebufferImage);
#endif

  return(0);
}
