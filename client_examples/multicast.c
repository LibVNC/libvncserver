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




int main(int argc, char **argv)
{
  rfbClient* client = rfbGetClient(8,3,4);

  client->GotFrameBufferUpdate = HandleRect;
  client->canHandleMulticastVNC = TRUE;

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

