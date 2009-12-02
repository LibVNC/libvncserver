
/*
 * a simple MulticastVNC example server
 * 
 * some code taken from the camera example.
 * 
 */

#include <rfb/rfb.h>
#include "radon.h"


#define WIDTH  1280
#define HEIGHT 1024
#define BYTESPERPIXEL      4




/* 15 frames per second (if we can) */
#define PICTURE_TIMEOUT (1.0/15.0)


/*
 * throttle camera updates
*/
int UpdateIntervalOver() 
{
    static struct timeval now={0,0}, then={0,0};
    double elapsed, dnow, dthen;

    gettimeofday(&now,NULL);

    dnow  = now.tv_sec  + (now.tv_usec /1000000.0);
    dthen = then.tv_sec + (then.tv_usec/1000000.0);
    elapsed = dnow - dthen;

    if (elapsed > PICTURE_TIMEOUT)
      memcpy((char *)&then, (char *)&now, sizeof(struct timeval));
    return elapsed > PICTURE_TIMEOUT;
}



/*
 * fills the framebuffer with a coloured pattern plus moving black line,
 * displays current server-side frame rate and number of connected clients
 */
void UpdateFramebuffer(rfbScreenInfoPtr rfbScreen)
{
  static int last_line=0, fps=0, fcount=0;
  int line=0;
  int i,j;
  struct timeval now;
  char fps_string[256];
  unsigned char* buffer = (unsigned char*)rfbScreen->frameBuffer;
  rfbClientIteratorPtr it;
  int clients=0;

  /*
   * simulate grabbing data from a device by updating the entire framebuffer
   */
  for(j=0;j<HEIGHT;++j) {
    for(i=0;i<WIDTH;++i) {
      buffer[(j*WIDTH+i)*BYTESPERPIXEL+0]=(i+j)*128/(WIDTH+HEIGHT); /* red */
      buffer[(j*WIDTH+i)*BYTESPERPIXEL+1]=i*128/WIDTH; /* green */
      buffer[(j*WIDTH+i)*BYTESPERPIXEL+2]=j*256/HEIGHT; /* blue */
    }
    buffer[j*WIDTH*BYTESPERPIXEL+0]=0xff;
    buffer[j*WIDTH*BYTESPERPIXEL+1]=0xff;
    buffer[j*WIDTH*BYTESPERPIXEL+2]=0xff;
  }

  /*
   * simulate the passage of time
   *
   * draw a simple black line that moves down the screen. The faster the
   * client, the more updates it will get, the smoother it will look!
   */
  gettimeofday(&now,NULL);
  line = now.tv_usec / (1000000/HEIGHT);
  if(line>=HEIGHT)
    line=HEIGHT-1;
  
  memset(&buffer[(WIDTH * BYTESPERPIXEL) * line], 0, (WIDTH * BYTESPERPIXEL));

  /* frames per second (informational only) */
  ++fcount;
  if(last_line > line) 
    {
      fps = fcount;
      fcount = 0;
    }
  last_line = line;

  /* number of clients */
  it = rfbGetClientIterator(rfbScreen);
  while(rfbClientIteratorNext(it) != NULL) 
    ++clients;
  
  rfbReleaseClientIterator(it);

  snprintf(fps_string, 256, "Frame %04d of %04d (%03d fps on server side) - %d Clients \r", line, HEIGHT, fps, clients);
  rfbDrawString(rfbScreen, &radonFont, 20, 100, fps_string, 0xffffff);
  fprintf(stderr, "%s", fps_string);
}




int main(int argc,char** argv)
{                                                                
  rfbScreenInfoPtr server = rfbGetScreen(&argc,argv,WIDTH,HEIGHT,8,3,BYTESPERPIXEL);
  server->frameBuffer=(char*)malloc(WIDTH*HEIGHT*BYTESPERPIXEL);

  server->desktopName = "MulticastVNC example";

  /* enable MulticastVNC */
  server->multicastVNC = TRUE;
  /* 
     if we said TRUE above, we can supply the address for the multicast group,
     port and TTL, otherwise libvncserver will use its defaults.
  */
  /*
  server->multicastAddr = "ff00::e000:2a8a"; 
  server->multicastPort = 5901;
  server->multicastTTL = 32;
  */


  /* Initialize the server */
  rfbInitServer(server);           

  /* Loop, updating framebuffer and processing clients */
  while(rfbIsActive(server)) 
    {
      if(UpdateIntervalOver())
	{
	  UpdateFramebuffer(server);
	  rfbMarkRectAsModified(server,0,0,WIDTH,HEIGHT);
	}
      rfbProcessEvents(server, server->deferUpdateTime*1000);
    }

  return EXIT_SUCCESS;
}
