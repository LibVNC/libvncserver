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

#include <stdio.h>
#include <netinet/in.h>
#ifdef __IRIX__
#include <netdb.h>
#endif
#define XK_MISCELLANY
#include "rfb.h"
#include "keysymdef.h"

const int maxx=640, maxy=480, bpp=4;

/* This initializes a nice (?) background */

void initBuffer(unsigned char* buffer)
{
  int i,j;
  for(i=0;i<maxx;++i)
    for(j=0;j<maxy;++j) {
      buffer[(j*maxx+i)*bpp+1]=(i+j)*128/(maxx+maxy); /* red */
      buffer[(j*maxx+i)*bpp+2]=i*128/maxx; /* green */
      buffer[(j*maxx+i)*bpp+3]=j*256/maxy; /* blue */
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

void newclient(rfbClientPtr cl)
{
  cl->clientData = (void*)calloc(sizeof(ClientData),1);
  cl->clientGoneHook = clientgone;
}

/* aux function to draw a line */
void drawline(unsigned char* buffer,int rowstride,int bpp,int x1,int y1,int x2,int y2)
{
  int i,j;
  i=x1-x2; j=y1-y2;
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
   if(buttonMask && x>=0 && y>=0 && x<maxx && y<maxy) {
      int i,j,x1,x2,y1,y2;

      if(cd->oldButton==buttonMask) { /* draw a line */
	drawline(cl->screen->frameBuffer,cl->screen->paddedWidthInBytes,bpp,
		 x,y,cd->oldx,cd->oldy);
	rfbMarkRectAsModified(cl->screen,x,y,cd->oldx,cd->oldy);
      } else { /* draw a point (diameter depends on button) */
	x1=x-buttonMask; if(x1<0) x1=0;
	x2=x+buttonMask; if(x2>maxx) x2=maxx;
	y1=y-buttonMask; if(y1<0) y1=0;
	y2=y+buttonMask; if(y2>maxy) y2=maxy;

	for(i=x1*bpp;i<x2*bpp;i++)
	  for(j=y1;j<y2;j++)
	    cl->screen->frameBuffer[j*cl->screen->paddedWidthInBytes+i]=0xff;
	rfbMarkRectAsModified(cl->screen,x1,y1,x2-1,y2-1);
      }

      /* we could get a selection like that:
	 rfbGotXCutText(cl->screen,"Hallo",5);
      */
   } else
     cd->oldButton=0;

   cd->oldx=x; cd->oldy=y; cd->oldButton=buttonMask;
}

/* aux function to draw a character to x, y */

#include "radon.h"
int drawchar(unsigned char* buffer,int rowstride,int bpp,int x,int y,char c)
{
  int i,j,k,width,height;
  unsigned char d;
  unsigned char* data=bdffontdata+bdffontmetadata[c*5];
  width=bdffontmetadata[c*5+1];
  height=bdffontmetadata[c*5+2];
  x+=bdffontmetadata[c*5+3];
  y+=bdffontmetadata[c*5+4]-height+1;

  for(j=0;j<height;j++) {
    for(i=0;i<width;i++) {
      if((i&7)==0) {
	d=*data;
	data++;
      }
      if(d&0x80) {
	for(k=0;k<bpp;k++)
	  buffer[(y+j)*rowstride+(x+i)*bpp+k]=0xff;
      }
      d<<=1;
    }
    if((i&7)==0)
      data++;
  }
  return(width);
}

void drawstring(unsigned char* buffer,int rowstride,int bpp,int x,int y,char* string)
{
  while(*string) {
    x+=drawchar(buffer,rowstride,bpp,x,y,*string);
    string++;
  }
}

int bdflength(char* string)
{
  int i;
  while(*string) {
    i+=bdffontmetadata[*string*5+1];
    string++;
  }
  return(i);
}

void bdfbbox(char c,int* x1,int* y1,int* x2,int* y2)
{
  *x1+=bdffontmetadata[c*5+3];
  *y1+=bdffontmetadata[c*5+4]-bdffontmetadata[c*5+2]+1;
  *x2=*x1+bdffontmetadata[c*5+1];
  *y2=*y1+bdffontmetadata[c*5+2];
}

/* Here the key events are handled */

void dokey(Bool down,KeySym key,rfbClientPtr cl)
{
  if(down) {
    if(key==XK_Escape)
      rfbCloseClient(cl);
    else if(key==XK_Page_Up) {
      initBuffer(cl->screen->frameBuffer);
      rfbMarkRectAsModified(cl->screen,0,0,maxx,maxy);
    } else if(key>=' ' && key<0x100) {
      ClientData* cd=cl->clientData;
      int x1=cd->oldx,y1=cd->oldy,x2,y2;
      cd->oldx+=drawchar(cl->screen->frameBuffer,
			 cl->screen->paddedWidthInBytes,bpp,cd->oldx,cd->oldy,
			 key);
      bdfbbox(key,&x1,&y1,&x2,&y2);
      rfbMarkRectAsModified(cl->screen,x1,y1,x2-1,y2-1);
    }
  }
}

/* Initialization */

int main(int argc,char** argv)
{
  rfbScreenInfoPtr rfbScreen =
    rfbDefaultScreenInit(argc,argv,maxx,maxy,8,3,bpp);
  rfbScreen->desktopName = "LibVNCServer Example";
  rfbScreen->frameBuffer = (char*)malloc(maxx*maxy*bpp);
  rfbScreen->rfbAlwaysShared = TRUE;
  rfbScreen->ptrAddEvent = doptr;
  rfbScreen->kbdAddEvent = dokey;
  rfbScreen->newClientHook = newclient;

  initBuffer(rfbScreen->frameBuffer);

  drawstring(rfbScreen->frameBuffer,maxx*bpp,bpp,20,100,"Hallo, Welt!");

  /* this is the blocking event loop, i.e. it never returns */
  /* 40000 are the microseconds, i.e. 0.04 seconds */
  runEventLoop(rfbScreen,40000,FALSE);

  /* this is the non-blocking event loop; a background thread is started */
  runEventLoop(rfbScreen,40000,TRUE);

  /* now we could do some cool things like rendering */
  while(1);
   
  return(0);
}
