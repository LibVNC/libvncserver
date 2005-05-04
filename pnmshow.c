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
  int i,j,k,width,height,paddedWidth;
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
  fprintf(stderr,"Got width %d and height %d.\n",width,height);
  fgets(buffer,1024,in);

  /* vncviewers have problems with widths which are no multiple of 4. */
  paddedWidth = width;
  if(width&3)
    paddedWidth+=4-(width&3);

  /* initialize data for vnc server */
  rfbScreen = rfbGetScreen(&argc,argv,paddedWidth,height,8,3,4);
  if(argc>1)
    rfbScreen->desktopName = argv[1];
  else
    rfbScreen->desktopName = "Picture";
  rfbScreen->rfbAlwaysShared = TRUE;
  rfbScreen->kbdAddEvent = HandleKey;

  /* enable http */
  rfbScreen->httpDir = "./classes";

  /* allocate picture and read it */
  rfbScreen->frameBuffer = (char*)malloc(paddedWidth*4*height);
  fread(rfbScreen->frameBuffer,width*3,height,in);
  fclose(in);

  /* correct the format to 4 bytes instead of 3 (and pad to paddedWidth) */
  for(j=height-1;j>=0;j--) {
    for(i=width-1;i>=0;i--)
      for(k=2;k>=0;k--)
	rfbScreen->frameBuffer[(j*paddedWidth+i)*4+k]=
	  rfbScreen->frameBuffer[(j*width+i)*3+k];
    for(i=width*4;i<paddedWidth*4;i++)
      rfbScreen->frameBuffer[j*paddedWidth*4+i]=0;
  }

  /* initialize server */
  rfbInitServer(rfbScreen);

  /* run event loop */
  rfbRunEventLoop(rfbScreen,40000,FALSE);

  return(0);
}
