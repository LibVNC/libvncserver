/*
 * 
 * This is an example of how to use libvncserver.
 * 
 * libvncserver example
 * Copyright (C) 2001 Johannes E. Schindelin <Johannes.Schindelin@gmx.de>
 * 
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

#ifdef WIN32
#define sleep Sleep
#else
#include <unistd.h>
#endif

#ifdef __IRIX__
#include <netdb.h>
#endif

#include "rfb.h"
#include "keysym.h"

const int maxx=640, maxy=480, bpp=4;
/* TODO: odd maxx doesn't work (vncviewer bug) */

/* This initializes a nice (?) background */

void initBuffer(unsigned char* buffer)
{
  int i,j;
  for(j=0;j<maxy;++j) {
    for(i=0;i<maxx;++i) {
      buffer[(j*maxx+i)*bpp+0]=(i+j)*128/(maxx+maxy); /* red */
      buffer[(j*maxx+i)*bpp+1]=i*128/maxx; /* green */
      buffer[(j*maxx+i)*bpp+2]=j*256/maxy; /* blue */
    }
    buffer[j*maxx*bpp+0]=0xff;
    buffer[j*maxx*bpp+1]=0xff;
    buffer[j*maxx*bpp+2]=0xff;
    buffer[j*maxx*bpp+3]=0xff;
  }
}

/* Here we create a structure so that every client has it's own pointer */

typedef struct ClientData {
  Bool oldButton;
  int oldx,oldy;
} ClientData;

void clientgone(rfbClientPtr cl)
{
  free(cl->clientData);
}

enum rfbNewClientAction newclient(rfbClientPtr cl)
{
  cl->clientData = (void*)calloc(sizeof(ClientData),1);
  cl->clientGoneHook = clientgone;
  return RFB_CLIENT_ACCEPT;
}

/* aux function to draw a line */

void drawline(unsigned char* buffer,int rowstride,int bpp,int x1,int y1,int x2,int y2)
{
  int i,j;
  i=x1-x2; j=y1-y2;
  if(i==0 && j==0) {
     for(i=0;i<bpp;i++)
       buffer[y1*rowstride+x1*bpp+i]=0xff;
     return;
  }
  if(i<0) i=-i;
  if(j<0) j=-j;
  if(i<j) {
    if(y1>y2) { i=y2; y2=y1; y1=i; i=x2; x2=x1; x1=i; }
    if(y2==y1) { if(y2>0) y1--; else y2++; }
    for(j=y1;j<=y2;j++)
      for(i=0;i<bpp;i++)
	buffer[j*rowstride+(x1+(j-y1)*(x2-x1)/(y2-y1))*bpp+i]=0xff;
  } else {
    if(x1>x2) { i=y2; y2=y1; y1=i; i=x2; x2=x1; x1=i; }
    for(i=x1;i<=x2;i++)
      for(j=0;j<bpp;j++)
	buffer[(y1+(i-x1)*(y2-y1)/(x2-x1))*rowstride+i*bpp+j]=0xff;
  }
}
    
/* Here the pointer events are handled */

void doptr(int buttonMask,int x,int y,rfbClientPtr cl)
{
   ClientData* cd=cl->clientData;

   if(cl->screen->cursorIsDrawn)
     rfbUndrawCursor(cl->screen);

   if(x>=0 && y>=0 && x<maxx && y<maxy) {
      if(buttonMask) {
	 int i,j,x1,x2,y1,y2;

	 if(cd->oldButton==buttonMask) { /* draw a line */
	    drawline(cl->screen->frameBuffer,cl->screen->paddedWidthInBytes,bpp,
		     x,y,cd->oldx,cd->oldy);
	    rfbMarkRectAsModified(cl->screen,x,y,cd->oldx,cd->oldy);
	 } else { /* draw a point (diameter depends on button) */
	    int w=cl->screen->paddedWidthInBytes;
	    x1=x-buttonMask; if(x1<0) x1=0;
	    x2=x+buttonMask; if(x2>maxx) x2=maxx;
	    y1=y-buttonMask; if(y1<0) y1=0;
	    y2=y+buttonMask; if(y2>maxy) y2=maxy;

	    for(i=x1*bpp;i<x2*bpp;i++)
	      for(j=y1;j<y2;j++)
		cl->screen->frameBuffer[j*w+i]=(char)0xff;
	    rfbMarkRectAsModified(cl->screen,x1,y1,x2-1,y2-1);
	 }

	 /* we could get a selection like that:
	  rfbGotXCutText(cl->screen,"Hallo",5);
	  */
      } else
	cd->oldButton=0;

      cd->oldx=x; cd->oldy=y; cd->oldButton=buttonMask;
   }
   defaultPtrAddEvent(buttonMask,x,y,cl);
}

/* aux function to draw a character to x, y */

#include "radon.h"

/* Here the key events are handled */

void dokey(Bool down,KeySym key,rfbClientPtr cl)
{
  if(down) {
    if(key==XK_Escape)
      rfbCloseClient(cl);
    else if(key==XK_Page_Up) {
      if(cl->screen->cursorIsDrawn)
	rfbUndrawCursor(cl->screen);
      initBuffer(cl->screen->frameBuffer);
      rfbMarkRectAsModified(cl->screen,0,0,maxx,maxy);
    } else if(key>=' ' && key<0x100) {
      ClientData* cd=cl->clientData;
      int x1=cd->oldx,y1=cd->oldy,x2,y2;
      if(cl->screen->cursorIsDrawn)
	rfbUndrawCursor(cl->screen);
      cd->oldx+=rfbDrawChar(cl->screen,&radonFont,cd->oldx,cd->oldy,(char)key,0x00ffffff);
      rfbFontBBox(&radonFont,(char)key,&x1,&y1,&x2,&y2);
      rfbMarkRectAsModified(cl->screen,x1,y1,x2-1,y2-1);
    }
  }
}

/* Example for an XCursor (foreground/background only) */

int exampleXCursorWidth=9,exampleXCursorHeight=7;
char exampleXCursor[]=
  "         "
  " xx   xx "
  "  xx xx  "
  "   xxx   "
  "  xx xx  "
  " xx   xx "
  "         ";

/* Example for a rich cursor (full-colour) */

void MakeRichCursor(rfbScreenInfoPtr rfbScreen)
{
  int i,j,w=32,h=32;
  rfbCursorPtr c = rfbScreen->cursor;
  char bitmap[]=
    "                                "
    "              xxxxxx            "
    "       xxxxxxxxxxxxxxxxx        "
    "      xxxxxxxxxxxxxxxxxxxxxx    "
    "    xxxxx  xxxxxxxx  xxxxxxxx   "
    "   xxxxxxxxxxxxxxxxxxxxxxxxxxx  "
    "  xxxxxxxxxxxxxxxxxxxxxxxxxxxxx "
    "  xxxxx   xxxxxxxxxxx   xxxxxxx "
    "  xxxx     xxxxxxxxx     xxxxxx "
    "  xxxxx   xxxxxxxxxxx   xxxxxxx "
    " xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx "
    " xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx "
    " xxxxxxxxxxxx  xxxxxxxxxxxxxxx  "
    " xxxxxxxxxxxxxxxxxxxxxxxxxxxx   "
    " xxxxxxxxxxxxxxxxxxxxxxxxxxxx   "
    " xxxxxxxxxxx   xxxxxxxxxxxxxx   "
    " xxxxxxxxxx     xxxxxxxxxxxx    "
    "  xxxxxxxxx      xxxxxxxxx      "
    "   xxxxxxxxxx   xxxxxxxxx       "
    "      xxxxxxxxxxxxxxxxxxx       "
    "       xxxxxxxxxxxxxxxxxxx      "
    "         xxxxxxxxxxxxxxxxxxx    "
    "             xxxxxxxxxxxxxxxxx  "
    "                xxxxxxxxxxxxxxx "
    "   xxxx           xxxxxxxxxxxxx "
    "  xx   x            xxxxxxxxxxx "
    "  xxx               xxxxxxxxxxx "
    "  xxxx             xxxxxxxxxxx  "
    "   xxxxxx       xxxxxxxxxxxx    "
    "    xxxxxxxxxxxxxxxxxxxxxx      "
    "      xxxxxxxxxxxxxxxx          "
    "                                ";
  c=rfbScreen->cursor = rfbMakeXCursor(w,h,bitmap,bitmap);
  c->xhot = 16; c->yhot = 24;

  c->richSource = malloc(w*h*bpp);
  for(j=0;j<h;j++) {
    for(i=0;i<w;i++) {
      c->richSource[j*w*bpp+i*bpp+0]=i*0xff/w;
      c->richSource[j*w*bpp+i*bpp+1]=(i+j)*0xff/(w+h);
      c->richSource[j*w*bpp+i*bpp+2]=j*0xff/h;
      c->richSource[j*w*bpp+i*bpp+3]=0;
    }
  }
}

/* Initialization */

int main(int argc,char** argv)
{
  rfbScreenInfoPtr rfbScreen =
    rfbGetScreen(&argc,argv,maxx,maxy,8,3,bpp);
  rfbScreen->desktopName = "LibVNCServer Example";
  rfbScreen->frameBuffer = (char*)malloc(maxx*maxy*bpp);
  rfbScreen->rfbAlwaysShared = TRUE;
  rfbScreen->ptrAddEvent = doptr;
  rfbScreen->kbdAddEvent = dokey;
  rfbScreen->newClientHook = newclient;
  rfbScreen->httpDir = "./classes";

  initBuffer(rfbScreen->frameBuffer);
  rfbDrawString(rfbScreen,&radonFont,20,100,"Hello, World!",0xffffff);

  /* This call creates a mask and then a cursor: */
  /* rfbScreen->defaultCursor =
       rfbMakeXCursor(exampleCursorWidth,exampleCursorHeight,exampleCursor,0);
  */

  MakeRichCursor(rfbScreen);

  /* initialize the server */
  rfbInitServer(rfbScreen);

#ifndef BACKGROUND_LOOP_TEST
  /* this is the blocking event loop, i.e. it never returns */
  /* 40000 are the microseconds, i.e. 0.04 seconds */
  rfbRunEventLoop(rfbScreen,40000,FALSE);
#elif !defined(HAVE_PTHREADS)
#error "I need pthreads for that."
#endif

  /* this is the non-blocking event loop; a background thread is started */
  rfbRunEventLoop(rfbScreen,-1,TRUE);
  /* now we could do some cool things like rendering */
  while(1) sleep(5); /* render(); */
   
  return(0);
}
