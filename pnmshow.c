#include <stdio.h>
#include "rfb.h"
#include "keysym.h"

void HandleKey(Bool down,KeySym key,rfbClientPtr cl)
{
  if(down && (key==XK_Escape || key=='q' || key=='Q'))
    rfbCloseClient(cl);
}

int main(int argc,char** argv)
{
  FILE* in=stdin;
  int i,width,height;
  unsigned char buffer[1024];
  rfbScreenInfoPtr rfbScreen;

  if(argc>1) {
    in=fopen(argv[1],"rb");
    if(!in) {
      printf("Couldn't find file %s.\n",argv[1]);
      exit(1);
    }
  }

  fgets(buffer,1024,in);
  if(strncmp(buffer,"P6",2)) {
    printf("Not a ppm.\n");
    exit(2);
  }

  /* skip comments */
  do {
    fgets(buffer,1024,in);
  } while(buffer[0]=='#');

  /* get width & height */
  sscanf(buffer,"%d %d",&width,&height);
  fprintf(stderr,"Got width %d and height %d (%s).\n",width,height,buffer);
  fgets(buffer,1024,in);

  /* initialize data for vnc server */
  rfbScreen = rfbDefaultScreenInit(argc,argv,width,height,8,3,4);
  if(argc>1)
    rfbScreen->desktopName = argv[1];
  else
    rfbScreen->desktopName = "Picture";
  rfbScreen->rfbAlwaysShared = TRUE;
  rfbScreen->kbdAddEvent = HandleKey;

  /* allocate picture and read it */
  rfbScreen->frameBuffer = (char*)malloc(width*height*4);
  fread(rfbScreen->frameBuffer,width*3,height,in);
  fclose(in);

  /* correct the format to 4 bytes instead of 3 */
  for(i=width*height-1;i>=0;i--) {
    rfbScreen->frameBuffer[i*4+2]=rfbScreen->frameBuffer[i*3+0];
    rfbScreen->frameBuffer[i*4+1]=rfbScreen->frameBuffer[i*3+1];
    rfbScreen->frameBuffer[i*4+0]=rfbScreen->frameBuffer[i*3+2];
  }

  for(i=0;i<200;i++) {
    rfbScreen->frameBuffer[i*4+i*width*4]=0;
    rfbScreen->frameBuffer[i*4+i*width*4+1]=0;
    rfbScreen->frameBuffer[i*4+i*width*4+2]=0;
    rfbScreen->frameBuffer[i*4+i*width*4+3]=0;
  }

  /* run event loop */
  runEventLoop(rfbScreen,40000,FALSE);

  return(0);
}
