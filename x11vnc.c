#include <X11/Xlib.h>
#include <X11/keysym.h>
#define KEYSYM_H
#include "rfb.h"

int c=0,blockLength = 32;

void getImage(Display *dpy,int xscreen,XImage **i)
{
  *i = XGetImage( dpy,
		  RootWindow(dpy,xscreen),
		  0,0,
		  DisplayWidth(dpy,xscreen),
		  DisplayHeight(dpy,xscreen),
		  AllPlanes,
		  ZPixmap );
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
	for(l=j*s->paddedWidthInBytes;!changed&&l<y1;l+=s->paddedWidthInBytes)
	  for(k=i*s->bitsPerPixel/8;k<x1;k++)
	    if(s->frameBuffer[l+k]!=b[l+k]) {
//	       fprintf(stderr,"changed: %d, %d\n",k,l);
	       changed=TRUE;
	       goto changed_p;
	    }
	if(changed) {
	   changed_p:
	  for(;l<0*y1;l++)
	     memcpy(/*b+l,*/s->frameBuffer+l,b+l,x1-l);
	   rfbMarkRectAsModified(s,i,j,i+blockLength,j+blockLength);
	}
     }
}

int main(int argc,char** argv)
{
  XImage *framebufferImage;
  char *backupImage;
  Display *dpy;
  int xscreen;
  rfbScreenInfoPtr screen;

  dpy = XOpenDisplay("");
  xscreen = DefaultScreen(dpy);

  getImage(dpy,xscreen,&framebufferImage);

  screen = rfbGetScreen(&argc,argv,framebufferImage->width,
			framebufferImage->height,
			framebufferImage->bits_per_pixel,
			8,
			framebufferImage->bits_per_pixel/8);
   
  screen->paddedWidthInBytes = framebufferImage->bytes_per_line;

  screen->rfbServerFormat.bitsPerPixel = framebufferImage->bits_per_pixel;
  screen->rfbServerFormat.depth = framebufferImage->depth;
  rfbEndianTest = framebufferImage->bitmap_bit_order != MSBFirst;
  screen->rfbServerFormat.trueColour = TRUE;

  if ( screen->rfbServerFormat.bitsPerPixel == 8 ) {
    screen->rfbServerFormat.redShift = 0;
    screen->rfbServerFormat.greenShift = 2;
    screen->rfbServerFormat.blueShift = 5;
    screen->rfbServerFormat.redMax   = 3;
    screen->rfbServerFormat.greenMax = 7;
    screen->rfbServerFormat.blueMax  = 3;
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
  //memcpy(backupImage,framebufferImage->data,screen->height*screen->paddedWidthInBytes);
  screen->frameBuffer = backupImage;
  screen->rfbDeferUpdateTime = 500;
  screen->cursor = 0;

  rfbInitServer(screen);
   
  while(1) {
    rfbProcessEvents(screen,-1);
    if(1 || /*c++>7 &&*/ (!screen->rfbClientHead || !FB_UPDATE_PENDING(screen->rfbClientHead))) {
       c=0;
    framebufferImage->f.destroy_image(framebufferImage);
    getImage(dpy,xscreen,&framebufferImage);
    //checkForImageUpdates(screen,framebufferImage->data);
    }
     fprintf(stderr,"%x\n%x\n---\n",screen->frameBuffer,framebufferImage->data);
   memcpy(screen->frameBuffer,framebufferImage->data,screen->height/10*screen->paddedWidthInBytes);
   rfbMarkRectAsModified(screen,0,0,screen->width,screen->height);
#if 0
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

  return(0);
}
