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

void doptr(int buttonMask,int x,int y,rfbClientPtr cl)
{
   if(buttonMask && x>=0 && y>=0 && x<maxx && y<maxy) {
      int i;
      for(i=0;i<bpp;i++)
	cl->screen->frameBuffer[y*cl->screen->paddedWidthInBytes+x*bpp+i]^=0xff;
      rfbMarkRectAsModified(cl->screen,x,y,x+1,y+1);
      rfbGotXCutText(cl->screen,"Hallo",5);
   }
}

void dokey(Bool down,KeySym key,rfbClientPtr cl)
{
  if(down && key==XK_Escape)
    rfbCloseClient(cl);
}

int main(int argc,char** argv)
{
  int i,j;
  rfbScreenInfoPtr rfbScreen = rfbDefaultScreenInit(argc,argv);
  rfbScreen->desktopName="LibVNCServer Example";
  rfbScreen->frameBuffer = (char*)malloc(maxx*maxy*bpp);
  rfbScreen->width=maxx;
  rfbScreen->height=maxy;
  rfbScreen->paddedWidthInBytes=maxx*bpp;
  rfbScreen->ptrAddEvent=doptr;
  rfbScreen->kbdAddEvent=dokey;

  for(i=0;i<maxx;++i)
    for(j=0;j<maxy;++j) {
      rfbScreen->frameBuffer[(j*maxx+i)*bpp]=i*256/maxx;
      rfbScreen->frameBuffer[(j*maxx+i)*bpp+1]=j*256/maxy;
      rfbScreen->frameBuffer[(j*maxx+i)*bpp+2]=(i+j)*256/(maxx*maxy);
    }

  runEventLoop(rfbScreen,40000,FALSE);
  runEventLoop(rfbScreen,40000,TRUE);
  while(1);
   
  return(0);
}
