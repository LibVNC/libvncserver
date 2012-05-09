#include <rfb/keysym.h>
#include "VNConsole.h"
#include "vga.h"
#include <fcntl.h>
#include <sys/ioctl.h>

static int tty=2;
static int tty_inject_device;

void do_key(rfbBool down,rfbKeySym keySym,rfbClientPtr cl)
{
  static char isControl=0;

  if(down) {
    /* if(keySym==XK_Escape)
      rfbCloseClient(cl);
    else */ if(keySym==XK_Control_L || keySym==XK_Control_R)
      isControl++;
    else if(tty_inject_device>=0) {
      if(keySym==XK_Escape)
	keySym=27;
      if(isControl) {
	if(keySym>='a' && keySym<='z')
	  keySym-='a'-1;
	else if(keySym>='A' && keySym<='Z')
	  keySym-='A'-1;
	else
	  keySym=0xffff;
      }

      if(keySym==XK_Tab)
	keySym='\t';
      else if(keySym==XK_Return)
	keySym='\r';
      else if(keySym==XK_BackSpace)
	keySym=8;
      else if(keySym==XK_Home || keySym==XK_KP_Home)
	keySym=1;
      else if(keySym==XK_End || keySym==XK_KP_End)
	keySym=5;
      else if(keySym==XK_Up || keySym==XK_KP_Up)
	keySym=16;
      else if(keySym==XK_Down || keySym==XK_KP_Down)
	keySym=14;
      else if(keySym==XK_Right || keySym==XK_KP_Right)
	keySym=6;
      else if(keySym==XK_Left || keySym==XK_KP_Left)
	keySym=2;

      if(keySym<0x100) {
	int ret;
	ret=ioctl(tty_inject_device,TIOCSTI,&keySym);
	if(ret<0) {
	  static char device[64];
	  close(tty_inject_device);
	  sprintf(device,"/dev/tty%d",tty);
	  tty_inject_device=open(device,O_WRONLY);
	  ret=ioctl(tty_inject_device,TIOCSTI,&keySym);
	  if(ret<0)
	    rfbErr("Couldn't reopen device %s!\n",device);
	}
      }
    }
  } else if(keySym==XK_Control_L || keySym==XK_Control_R)
    if(isControl>0)
      isControl--;
}

/* these colours are from linux kernel drivers/char/console.c */
unsigned char color_table[] = { 0, 4, 2, 6, 1, 5, 3, 7,
				       8,12,10,14, 9,13,11,15 };
/* the default colour table, for VGA+ colour systems */
int default_red[] = {0x00,0xaa,0x00,0xaa,0x00,0xaa,0x00,0xaa,
    0x55,0xff,0x55,0xff,0x55,0xff,0x55,0xff};
int default_grn[] = {0x00,0x00,0xaa,0x55,0x00,0x00,0xaa,0xaa,
    0x55,0x55,0xff,0xff,0x55,0x55,0xff,0xff};
int default_blu[] = {0x00,0x00,0x00,0x00,0xaa,0xaa,0xaa,0xaa,
    0x55,0x55,0x55,0x55,0xff,0xff,0xff,0xff};

int main(int argc,char **argv)
{
  int width=80,height=25;
  char *buffer;
  vncConsolePtr console;
  char tty_device[64],title[128];
  int i;
  FILE* tty_file;
  struct winsize dimensions;

  if(argc>1) {
    if((tty=atoi(argv[1]))<1) {
      rfbErr("Usage: %s [tty_number [vnc args]]\n",argv[0]);
      exit(1);
    } else {
      argv++;
      argc--;
    }
  }

  /* getopt goes here! */

  sprintf(tty_device,"/dev/tty%d",tty);
  if((tty_inject_device=open(tty_device,O_WRONLY))<0) {
    rfbErr("Couldn't open tty device %s!\n",tty_device);
    exit(1);
  }
  rfbLog("Using device %s.\n",tty_device);

  if(ioctl(tty_inject_device,TIOCGWINSZ,&dimensions)>=0) {
    width=dimensions.ws_col;
    height=dimensions.ws_row;
  }

  sprintf(title,"LinuxVNC: /dev/tty%d",tty);

  /* console init */
  if(!(console=vcGetConsole(&argc,argv,width,height,&vgaFont,TRUE)))
    exit(1);

  for(i=0;i<16;i++) {
    console->screen->colourMap.data.bytes[i*3+0]=default_red[color_table[i]];
    console->screen->colourMap.data.bytes[i*3+1]=default_grn[color_table[i]];
    console->screen->colourMap.data.bytes[i*3+2]=default_blu[color_table[i]];
  }
  console->screen->desktopName=title;
  console->screen->kbdAddEvent=do_key;
  console->selectTimeOut=100000;
  console->wrapBottomToTop=TRUE;
#ifdef USE_OLD_VCS
  buffer=malloc(width*height);
  console->cursorActive=FALSE;
#else
  buffer=malloc(width*height*2+4);
  console->cursorActive=TRUE;
#endif
  /* memcpy(buffer,console->screenBuffer,width*height); */

#ifdef USE_OLD_VCS
  sprintf(tty_device,"/dev/vcs%d",tty);
#else
  sprintf(tty_device,"/dev/vcsa%d",tty);
#endif

  while(rfbIsActive(console->screen)) {
    if(!console->currentlyMarking) {
      tty_file=fopen(tty_device,"rb");
      if(!tty_file) {
	rfbErr("cannot open device \"%s\"\n",
		tty_device);
	exit(1);
      }
#ifdef USE_OLD_VCS
      fread(buffer,width,height,tty_file);
#else
      if(fread(buffer,width*height*2+4,1,tty_file) != 1) {
	rfbErr("Error reading framebuffer\n");
	exit(1);
      }
      vcHideCursor(console);
#endif
      fclose(tty_file);

      for(i=0;i<console->width*console->height;i++) {
	if
#ifdef USE_OLD_VCS
	 (buffer[i]!=console->screenBuffer[i])
#else
	 (buffer[4+2*i]!=console->screenBuffer[i] ||
	  buffer[5+2*i]!=console->attributeBuffer[i])
#endif
	  {
	    console->x=(i%console->width);
	    console->y=(i/console->width);
	    /*
	      rfbLog("changes: %d,%d (%d!=%d || %d!=%d)\n",
	      console->x,console->y,
	      buffer[4+2*i],console->screenBuffer[i],
	      buffer[5+2*i],console->attributeBuffer[i]);
	    */
	    
#ifdef USE_OLD_VCS
	    vcPutChar(console,buffer[i]);
#else
	    vcPutCharColour(console,buffer[4+i*2],buffer[5+i*2]&0x7,buffer[5+i*2]>>4);
#endif
	  }
      }
      console->x=buffer[2];
      console->y=buffer[3];
    }
    vcProcessEvents(console);
  }
  return(0);
}
