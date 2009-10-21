/* A simple example of a multicast RFB client */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <rfb/rfbclient.h>


static void HandleRect(rfbClient* client, int x, int y, int w, int h) 
{
}

/*
 * The client part of the multicast vnc extension example.
 *
 */

#define rfbMulticastPseudoEncoding  0xFFFFFCC1





/* returns TRUE if the encoding was handled */
static rfbBool handleMultiCastEncoding(rfbClient* cl, rfbFramebufferUpdateRectHeader* rect)
{
  /* rect.r.w=byte count */
  if (rect->encoding !=  rfbMulticastPseudoEncoding)
    return FALSE;
  
  rfbClientLog("we got multicast encoded msg!!\n");

  rfbClientLog("x: %i\n", rect->r.x);
  rfbClientLog("y: %i\n", rect->r.y);
  rfbClientLog("w: %i\n", rect->r.w);
  rfbClientLog("h: %i\n", rect->r.h);

  char *buffer;
  buffer = malloc(rect->r.w+1);
  if (!ReadFromRFBServer(cl, buffer, rect->r.w))
    {
      free(buffer);
      return FALSE;
    }
  buffer[rect->r.w]=0; /* null terminate, just in case */
  rfbClientLog("got address: \"%s\"\n", buffer);
  free(buffer);
  return TRUE;
}


static int multiCastEncodings[] = { rfbMulticastPseudoEncoding, 0 };

static rfbClientProtocolExtension multicast = {
  multiCastEncodings,		/* encodings */
  handleMultiCastEncoding,	/* handleEncoding */
  NULL,                   	/* handleMessage */
  NULL				/* next extension */
};




int main(int argc, char **argv)
{
  rfbClient* client = rfbGetClient(8,3,4);

  client->GotFrameBufferUpdate = HandleRect;
  rfbClientRegisterExtension(&multicast);

  if (!rfbInitClient(client,&argc,argv))
    return 1;

  int i;
  while(1) 
    {
      i=WaitForMessage(client, 500);

      if(i<0)
	break;

      if(i)
	if(!HandleRFBServerMessage(client))
	  break;
    }

  rfbClientCleanup(client);

  return 0;
}

