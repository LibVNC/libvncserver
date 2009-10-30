#include <rfb/rfb.h>


/*
 * a simple MulticastVNC server
 *
 *
 */



int main(int argc,char** argv)
{                                                                
  rfbScreenInfoPtr server = rfbGetScreen(&argc,argv,400,300,8,3,4);
  server->frameBuffer=(char*)malloc(400*300*4);

  /* enable MulticastVNC */
  server->multicastVNC = TRUE;
  /* 
     if we said TRUE above, we have to supply a valid multicast address plus port,
     otherwise rfbInitServer will fail
  */
  server->multicastAddr = "192.168.42.123";
  server->multicastPort = 6666;
  
  rfbInitServer(server);           
  rfbRunEventLoop(server,-1,FALSE);
  return(0);
}
