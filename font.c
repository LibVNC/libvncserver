#include "rfb.h"

int rfbDrawChar(rfbScreenInfoPtr rfbScreen,rfbFontDataPtr font,
		 int x,int y,char c,CARD32 colour)
{
  int i,j,k,width,height;
  unsigned char d;
  unsigned char* data=font->data+font->metaData[c*5];
  int rowstride=rfbScreen->paddedWidthInBytes;
  int bpp=rfbScreen->rfbServerFormat.bitsPerPixel/8;

  width=font->metaData[c*5+1];
  height=font->metaData[c*5+2];
  x+=font->metaData[c*5+3];
  y+=font->metaData[c*5+4]-height+1;

  for(j=0;j<height;j++) {
    for(i=0;i<width;i++) {
      if((i&7)==0) {
	d=*data;
	data++;
      }
      if(d&0x80) {
	for(k=0;k<bpp;k++)
	  rfbScreen->frameBuffer[(y+j)*rowstride+(x+i)*bpp+k]=
	    ((colour>>(8*bpp))&0xff);
      }
      d<<=1;
    }
    if((i&7)==0)
      data++;
  }
  return(width);
}

void rfbDrawString(rfbScreenInfoPtr rfbScreen,rfbFontDataPtr font,
		   int x,int y,char* string,CARD32 colour)
{
  while(*string) {
    x+=rfbDrawChar(rfbScreen,font,x,y,*string,colour);
    string++;
  }
}

int rfbWidth(rfbFontDataPtr font,char* string)
{
  int i=0;
  while(*string) {
    i+=font->metaData[*string*5+1];
    string++;
  }
  return(i);
}

void rfbFontBBox(rfbFontDataPtr font,char c,int* x1,int* y1,int* x2,int* y2)
{
  *x1+=font->metaData[c*5+3];
  *y1+=font->metaData[c*5+4]-font->metaData[c*5+2]+1;
  *x2=*x1+font->metaData[c*5+1];
  *y2=*y1+font->metaData[c*5+2];
}

