#include <time.h>
#include <rfb/rfb.h>
#include <rfb/rfbclient.h>

int main(int argc,char** argv)
{                                                                
  int i,j;
  time_t t=time(0);

  rfbScreenInfoPtr server=rfbGetScreen(&argc,argv,400,300,8,3,4);
  rfbClient* client=rfbGetClient(&argc,argv,8,3,4);

  server->frameBuffer=malloc(400*300*4);
  for(j=0;j<400*300*4;j++)
    server->frameBuffer[j]=j;
  //server->maxRectsPerUpdate=-1;
  rfbInitServer(server);           
  while(time(0)-t<20) {

    for(j=0;j<400;j+=10)
      for(i=0;i<300;i+=10)
	rfbMarkRectAsModified(server,i,j,i+5,j+5);

    rfbProcessEvents(server,5000);
  }

  free(server->frameBuffer);
  rfbScreenCleanup(server);
  rfbClientCleanup(client);

  return(0);
}
