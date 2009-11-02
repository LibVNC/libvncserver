
/*
 * a simple MulticastVNC server
 *
 *
 */


#include <rfb/rfb.h>


int main(int argc,char** argv)
{                                                                
  rfbScreenInfoPtr server = rfbGetScreen(&argc,argv,400,300,8,3,4);
  server->frameBuffer=(char*)malloc(400*300*4);

  /* enable MulticastVNC */
  server->multicastVNC = TRUE;
  /* 
     if we said TRUE above, we can supply the address for the multicast group,
     port and TTL, otherwise libvncserver will use its defaults.
  */
  server->multicastAddr = "ff01::1";
  /*server->multicastPort = 6666;
  server->multicastTTL = 128;
  */
  rfbInitServer(server);           
  rfbRunEventLoop(server,-1,FALSE);
  return(0);
}
