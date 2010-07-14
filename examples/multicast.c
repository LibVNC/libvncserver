
/*
 * a simple MulticastVNC example server
 * 
 * some code taken from the camera example.
 * 
 */

#include <rfb/rfb.h>
#include "radon.h"

#define WIDTH  640
#define HEIGHT 480
#define BYTESPERPIXEL 4

/* 15 frames per second (if we can) */
#define FPS 15
#define PICTURE_TIMEOUT (1.0/FPS)




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
  static uint32_t last_line, fps, fcount;
  static uint32_t fps_avg, fps_sum, fps_nr_samples; 
  int line=0;
  int i,j;
  struct timeval now;
  char fps_string[256];
  unsigned char* buffer = (unsigned char*)rfbScreen->frameBuffer;
  rfbClientIteratorPtr it;
  int clients=0, mc_clients=0;
  rfbClientPtr cl;

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
  if(last_line > line) /* a new frame */
    {
      fps = fcount;
      fcount = 0;
      /* now calculate the average */
      ++fps_nr_samples;
      fps_sum += fps;
      fps_avg = fps_sum/fps_nr_samples;
    }
  last_line = line;

  /* number of clients */
  it = rfbGetClientIterator(rfbScreen);
  while((cl=rfbClientIteratorNext(it)) != NULL)
    {
      if(cl->useMulticastVNC)
	++mc_clients;
      else
	++clients;
    }
  rfbReleaseClientIterator(it);

  snprintf(fps_string, 256, "Frame %04d/%04d - Srv FPS: %03d now, %03d avg - Clients: %d unicast, %d multicast\r", 
	   line, HEIGHT, fps, fps_avg, clients, mc_clients);
  rfbDrawString(rfbScreen, &radonFont, 10, 100, fps_string, 0xffffff);
  fprintf(stderr, "%s", fps_string);
}




int main(int argc,char** argv)
{                                                                
  rfbScreenInfoPtr server;
  if(!(server = rfbGetScreen(&argc,argv,WIDTH,HEIGHT,8,3,BYTESPERPIXEL)))
    {
      rfbErr("Could not get server.\n");
      return EXIT_FAILURE;
    }
  server->frameBuffer=(char*)malloc(WIDTH*HEIGHT*BYTESPERPIXEL);

  server->desktopName = "MulticastVNC example";

  /* enable MulticastVNC */
  server->multicastVNC = TRUE;
  /* enable NACK as well */
  server->multicastVNCdoNACK = TRUE;
  /* 
     If we said TRUE above, we can supply the address for the multicast group,
     port, TTL and a time interval in miliseconds by which to defer updates.
     Otherwise, libvncserver will use its defaults.
  */
  /*
  server->multicastAddr = "ff00::e000:2a8a"; 
  server->multicastPort = 5901;
  server->multicastTTL = 32;
  server->multicastDeferUpdateTime = 50;
  */

  /* Initialize the server */
  rfbInitServer(server);           

  rfbLog("Doing %dx%d @%d FPS, sending %dkB/s (raw)\n",
	 WIDTH, HEIGHT, FPS, (WIDTH*HEIGHT*BYTESPERPIXEL*FPS)/1024);

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
