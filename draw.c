#include "rfb.h"

void rfbFillRect(rfbScreenInfoPtr s,int x1,int y1,int x2,int y2,Pixel col)
{
  int rowstride = s->paddedWidthInBytes, bpp = s->bitsPerPixel>>3;
  int i,j;
  char* colour=(char*)&col;

  if(!rfbEndianTest)
    colour += 4-bpp;
  for(j=y1;j<y2;j++)
    for(i=x1;i<x2;i++)
      memcpy(s->frameBuffer+j*rowstride+i*bpp,colour,bpp);
  rfbMarkRectAsModified(s,x1,y1,x2,y2);
}

