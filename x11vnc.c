/* This file (x11vnc.c) is part of LibVNCServer.
   It is a small clone of x0rfbserver by HexoNet, demonstrating the
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
#undef Bool
#define KeySym RFBKeySym
#include "rfb.h"

Display *dpy = 0;
int window;
int c=0,blockLength = 32;
int tileX=0,tileY=0,tileWidth=32,tileHeight=32*2,dontTile=TRUE;
Bool gotInput = FALSE;
Bool viewOnly = FALSE;
Bool sharedMode = FALSE;

Bool disconnectAfterFirstClient = TRUE;

/* keyboard handling */
#define KBDDEBUG

char modifiers[0x100];
KeyCode keycodes[0x100],leftShiftCode,rightShiftCode,altGrCode;

void init_keycodes()
{
  KeySym key,*keymap;
  int i,j,minkey,maxkey,syms_per_keycode;

  memset(modifiers,-1,sizeof(modifiers));

  XDisplayKeycodes(dpy,&minkey,&maxkey);
  keymap=XGetKeyboardMapping(dpy,minkey,(maxkey - minkey + 1),&syms_per_keycode);

#ifdef KBDDEBUG
  fprintf(stderr,"minkey=%d, maxkey=%d, syms_per_keycode=%d\n",
	  minkey,maxkey,syms_per_keycode);
#endif
  for (i = minkey; i <= maxkey; i++)
    for(j=0;j<syms_per_keycode;j++) {
      key=keymap[(i-minkey)*syms_per_keycode+j];
#ifdef KBDDEBUG
      fprintf(stderr,"keymap(i=0x%x,j=%d)==0x%lx\n",i,j,key);
#endif
      if(key>=' ' && key<0x100 && i==XKeysymToKeycode(dpy,key)) {
	keycodes[key]=i;
	modifiers[key]=j;
#ifdef KBDDEBUG
	fprintf(stderr,"key 0x%lx (%c): keycode=0x%x, modifier=%d\n",
		key,(char)key,i,j);
#endif
      }
    }

  leftShiftCode=XKeysymToKeycode(dpy,XK_Shift_L);
  rightShiftCode=XKeysymToKeycode(dpy,XK_Shift_R);
  altGrCode=XKeysymToKeycode(dpy,XK_Mode_switch);

#ifdef KBDDEBUG
  fprintf(stderr,"leftShift=0x%x, rightShift=0x%x, altGr=0x%x\n",
	  leftShiftCode,rightShiftCode,altGrCode);
#endif

  XFree ((char *) keymap);
}

static Bool shutDownServer=0;

/* the hooks */

void clientGone(rfbClientPtr cl)
{
  shutDownServer=-1;
}

enum rfbNewClientAction newClient(rfbClientPtr cl)
{
  if(disconnectAfterFirstClient)
    cl->clientGoneHook = clientGone;
  if(viewOnly)
    cl->clientData = (void*)-1;
  else
    cl->clientData = (void*)0;
  return(RFB_CLIENT_ACCEPT);
}

#define LEFTSHIFT 1
#define RIGHTSHIFT 2
#define ALTGR 4
char ModifierState = 0;

/* this function adjusts the modifiers according to mod (as from modifiers) and ModifierState */

void tweakModifiers(char mod,Bool down)
{
  Bool isShift=ModifierState&(LEFTSHIFT|RIGHTSHIFT);
#ifdef KBDDEBUG
  fprintf(stderr,"tweakModifiers: 0x%x %s\n",
	  mod,down?"down":"up");
#endif
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
  if(((int)cl->clientData)==-1) return; /* viewOnly */

#define ADJUSTMOD(sym,state) \
  if(keySym==sym) { if(down) ModifierState|=state; else ModifierState&=~state; }

  ADJUSTMOD(XK_Shift_L,LEFTSHIFT)
  ADJUSTMOD(XK_Shift_R,RIGHTSHIFT)
  ADJUSTMOD(XK_Mode_switch,ALTGR)

#ifdef KBDDEBUG
    fprintf(stderr,"keyboard: down=%s, keySym=0x%lx (%s), ModState=0x%x\n",
	    down?"down":"up",keySym,XKeysymToString(keySym),ModifierState);
#endif

  if(keySym>=' ' && keySym<0x100) {
    KeyCode k;
    if(down)
       tweakModifiers(modifiers[keySym],True);
    //tweakModifiers(modifiers[keySym],down);
    //k = XKeysymToKeycode( dpy,keySym );
    k = keycodes[keySym];
    if(k!=NoSymbol) {
      XTestFakeKeyEvent(dpy,k,down,CurrentTime);
      gotInput = TRUE;
    }
    if(down)
      tweakModifiers(modifiers[keySym],False);
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

  if(((int)cl->clientData)==-1) return; /* viewOnly */

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

void getImage(int bpp,Display *dpy,int xscreen,XImage **i,int x,int y,int width,int height)
{
  if(width<=0) width=DisplayWidth(dpy,xscreen);
  if(height<=0) height=DisplayHeight(dpy,xscreen);
  if(useSHM && bpp>0) {
    static Bool firstTime = TRUE;
    if(firstTime) {
      firstTime = FALSE;
      *i = XShmCreateImage(dpy,
			   DefaultVisual( dpy, xscreen ),
			   bpp,
			   ZPixmap,
			   NULL,
			   &shminfo,
			   width,height);

      if(*i == 0) {
	useSHM = FALSE;
	getImage(bpp,dpy,xscreen,i,x,y,width,height);
	return;
      }

      shminfo.shmid = shmget( IPC_PRIVATE,
			      (*i)->bytes_per_line * (*i)->height,
			      IPC_CREAT | 0777 );
      shminfo.shmaddr = (*i)->data = (char *) shmat( shminfo.shmid, 0, 0 );
      shminfo.readOnly = False;
      XShmAttach( dpy, &shminfo );
    }

    if(x==0 && y==0 && width==DisplayWidth(dpy,xscreen) && height==DisplayHeight(dpy,xscreen))
      XShmGetImage(dpy,window,*i,0,0,AllPlanes);
    else
      XGetSubImage(dpy,window,x,y,width,height,AllPlanes,ZPixmap,*i,0,0);
  } else {
    *i = XGetImage(dpy,window,x,y,width,height,AllPlanes,ZPixmap );
  }
}

void checkForImageUpdates(rfbScreenInfoPtr s,char *b,int rowstride,int x,int y,int width,int height)
{
   Bool changed;
   int i,j,k,l1,l2,x1,y1;
   int bpp=s->bitsPerPixel/8;

   for(j=0;j<height;j+=blockLength)
     for(i=0;i<width;i+=blockLength) {
	y1=j+blockLength; if(y1>height) y1=height;
	x1=i+blockLength; if(x1>width) x1=width;
	y1*=rowstride;
	x1*=bpp;
	changed=FALSE;
	for(l1=j*rowstride,l2=(j+y)*s->paddedWidthInBytes+x*bpp;l1<y1;l1+=rowstride,l2+=s->paddedWidthInBytes)
	  for(k=i*bpp;k<x1;k++)
	    if(s->frameBuffer[l2+k]!=b[l1+k]) {
	      //	       fprintf(stderr,"changed: %d, %d\n",k,l);
	       changed=TRUE;
	       goto changed_p;
	    }
	if(changed) {
	   changed_p:
	  for(l1+=i*bpp,l2+=i*bpp;l1<y1;l1+=rowstride,l2+=s->paddedWidthInBytes)
	    memcpy(/*b+l,*/s->frameBuffer+l2,b+l1,x1-i*bpp);
	  rfbMarkRectAsModified(s,x+i,y+j,x+i+blockLength,y+j+blockLength);
	}
     }
}

int probeX=0,probeY=0;

void probeScreen(rfbScreenInfoPtr s,int xscreen)
{
  int i,j,/*pixel,i1,*/j1,
    bpp=s->rfbServerFormat.bitsPerPixel/8,/*mask=(1<<bpp)-1,*/
    rstride=s->paddedWidthInBytes;
  XImage* im;
  //fprintf(stderr,"/%d,%d",probeX,probeY);
#if 0
  probeX++;
  if(probeX>=tileWidth) {
    probeX=0;
    probeY++;
    if(probeY>=tileHeight)
      probeY=0;
  }
#else
  probeX=(rand()%tileWidth);
  probeY=(rand()%tileHeight);
#endif

  for(j=probeY;j<s->height;j+=tileHeight)
    for(i=0/*probeX*/;i<s->width;i+=tileWidth) {
      im=XGetImage(dpy,window,i,j,tileWidth/*1*/,1,AllPlanes,ZPixmap);
      /*      for(i1=0;i1<bpp && im->data[i1]==(s->frameBuffer+i*bpp+j*rstride)[i1];i1++);
	      if(i1<bpp) { */
      if(memcmp(im->data,s->frameBuffer+i*bpp+j*rstride,tileWidth*bpp)) {
	/* do update */
	int x=i/*-probeX*/,w=(x+tileWidth>s->width)?s->width-x:tileWidth,
	  y=j-probeY,h=(y+tileHeight>s->height)?s->height-y:tileHeight;

	XDestroyImage(im);
	//getImage(bpp,dpy,xscreen,&im,x,y,w,h);
	//fprintf(stderr,"GetImage(%d,%d,%d,%d)",x,y,w,h);
	im = XGetImage(dpy,window,x,y,w,h,AllPlanes,ZPixmap );
	for(j1=0;j1<h;j1++)
	  memcpy(s->frameBuffer+x*bpp+(y+j1)*rstride,
		 im->data+j1*im->bytes_per_line,bpp*w);
	//checkForImageUpdates(s,im->data,rstride,x,y,w,h);
	//if(0 && !useSHM)
	  XDestroyImage(im);
	//memcpy(s->frameBuffer+i*bpp+j*rstride,&pixel,bpp);
	rfbMarkRectAsModified(s,x,y,x+w,y+h);
	//fprintf(stderr,"%d:%d:%x\n",i,j,pixel);
	//fprintf(stderr,"*");
      } else
	XDestroyImage(im);
    }
}

#define LOCAL_CONTROL

#ifdef LOCAL_CONTROL
#include "1instance.c"
#endif

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
  int updateCounter; /* about every 50 ms a screen update should be made. */

#ifdef LOCAL_CONTROL
  char message[1024];
  single_instance_struct single_instance = { "/tmp/x11vnc_control" };

  open_control_file(&single_instance);
#endif

  for(i=argc-1;i>0;i--)
#ifdef LOCAL_CONTROL
    if(i<argc-1 && !strcmp(argv[i],"-toggleviewonly")) {
      snprintf(message, sizeof(message), "t%s",argv[i+1]);
      send_message(&single_instance,message);
      exit(0);
    } else if(!strcmp(argv[i],"-listclients")) {
      fprintf(stderr,"list clients\n");
      send_message(&single_instance,"l");
      exit(0);
    } else
#ifdef BACKCHANNEL
    if(i<argc-1 && !strcmp(argv[i],"-backchannel")) {
      snprintf(message, sizeof(message), "b%s",argv[i+1]);
      send_message(&single_instance,message);
      exit(0);
    } else
#endif
#endif
    if(i<argc-1 && strcmp(argv[i],"-display")==0) {
      fprintf(stderr,"Using display %s\n",argv[i+1]);
      dpy = XOpenDisplay(argv[i+1]);
      if(dpy==0) {
	fprintf(stderr,"Couldn't connect to display \"%s\".\n",argv[i+1]);
	exit(1);
      }
    } else if(i<argc-1 && strcmp(argv[i],"-wait4client")==0) {
      maxMsecsToConnect = atoi(argv[i+1]);
    } else if(i<argc-1 && strcmp(argv[i],"-update")==0) {
      updateCounter = atoi(argv[i+1]);
    } else if(strcmp(argv[i],"-noshm")==0) {
      useSHM = FALSE;
    } else if(strcmp(argv[i],"-runforever")==0) {
      disconnectAfterFirstClient = FALSE;
    } else if(strcmp(argv[i],"-tile")==0) {
      dontTile=FALSE;
    } else if(strcmp(argv[i],"-viewonly")==0) {
      viewOnly=TRUE;
    } else if(strcmp(argv[i],"-shared")==0) {
      sharedMode=TRUE;
    }

  updateCounter = dontTile?20:1;

  if(dpy==0)
    dpy = XOpenDisplay("");
  if(dpy==0) {
    fprintf(stderr,"Couldn't open display!\n");
    exit(2);
  }

  xscreen = DefaultScreen(dpy);
  window = RootWindow(dpy,xscreen);
  //XTestGrabControl(dpy,True);

  init_keycodes();

  getImage(0,dpy,xscreen,&framebufferImage,0,0,-1,-1);

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

  if(sharedMode) {
    screen->rfbAlwaysShared = TRUE;
  }

  screen->rfbDeferUpdateTime = 1;
  updateCounter /= screen->rfbDeferUpdateTime;

  rfbInitServer(screen);

  c=0;
  while(1) {
    if(screen->rfbClientHead)
      maxMsecsToConnect = 1<<16;
    else {
      maxMsecsToConnect -= screen->rfbDeferUpdateTime;
      if(maxMsecsToConnect<0) {
	fprintf(stderr,"Maximum time to connect reached. Exiting.\n");
	XTestDiscard(dpy);
	exit(2);
      }
    }

#ifdef LOCAL_CONTROL
    if(get_next_message(message,1024,&single_instance,50)) {
      if(message[0]=='l' && message[1]==0) {
	rfbClientPtr cl;
	int i;
	for(i=0,cl=screen->rfbClientHead;cl;cl=cl->next,i++)
	  fprintf(stderr,"%02d: %s\n",i,cl->host);
      } else if(message[0]=='t') {
	rfbClientPtr cl;
	for(cl=screen->rfbClientHead;cl;cl=cl->next)
	  if(!strcmp(message+1,cl->host)) {
	    cl->clientData=(void*)((cl->clientData==0)?-1:0);
	    break;
	  }
      }
#ifdef BACKCHANNEL
      else if(message[0]=='b')
	rfbSendBackChannel(screen,message+1,strlen(message+1));
#endif
    }
#endif

    rfbProcessEvents(screen,-1);
    if(shutDownServer) {
      free(backupImage);
      rfbScreenCleanup(screen);
      XFree(dpy);
#ifndef NO_SHM
      XShmDetach(dpy,framebufferImage);
#endif
      exit(0);
    }

    if(dontTile) {
      if(gotInput) {
	gotInput = FALSE;
	c=updateCounter;
      } else if(screen->rfbClientHead && c++>updateCounter) {
	c=0;
	//fprintf(stderr,"*");
	if(!useSHM)
	  framebufferImage->f.destroy_image(framebufferImage);
	if(dontTile) {
	  getImage(screen->rfbServerFormat.bitsPerPixel,dpy,xscreen,&framebufferImage,0,0,screen->width,screen->height);
	  checkForImageUpdates(screen,framebufferImage->data,framebufferImage->bytes_per_line,
			       0,0,screen->width,screen->height);
	} else {
	  /* old tile code. Eventually to be removed (TODO) */
	  char isRightEdge = tileX+tileWidth>=screen->width;
	  char isLowerEdge = tileY+tileHeight>=screen->height;
	  getImage(screen->rfbServerFormat.bitsPerPixel,dpy,xscreen,&framebufferImage,tileX,tileY,
		   isRightEdge?screen->width-tileX:tileWidth,
		   isLowerEdge?screen->height-tileY:tileHeight);
	  checkForImageUpdates(screen,framebufferImage->data,framebufferImage->bytes_per_line,
			       tileX,tileY,
			       isRightEdge?screen->width-tileX:tileWidth,
			       isLowerEdge?screen->height-tileY:tileHeight);
	  if(isRightEdge) {
	    tileX=0;
	    if(isLowerEdge)
	      tileY=0;
	    else
	      tileY+=tileHeight;
	  } else
	    tileX+=tileWidth;
	}
      }
    } else if(c++>updateCounter) {
      c=0;
      probeScreen(screen,xscreen);
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
