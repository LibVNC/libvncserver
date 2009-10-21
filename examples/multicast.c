#include <rfb/rfb.h>

/*
 * This is a simple example demonstrating a protocol extension.
 *
 *
 * As suggested in the RFB protocol, the back channel is enabled by asking
 * for a "pseudo encoding", and enabling the back channel on the client side
 * as soon as it gets a back channel message from the server.
 *
 * This implements the server part.
 *
 * Note: If you design your own extension and want it to be useful for others,
 * too, you should make sure that
 *
 * - your server as well as your client can speak to other clients and
 *   servers respectively (i.e. they are nice if they are talking to a
 *   program which does not know about your extension).
 *
 * - if the machine is little endian, all 16-bit and 32-bit integers are
 *   swapped before they are sent and after they are received.
 *
 */

#define rfbMulticastPseudoEncoding 0xFFFFFCC1


rfbBool enableMulticast(rfbClientPtr cl, void** data, int encoding)
{
  if(encoding == rfbMulticastPseudoEncoding) 
    {
      uint16_t port = 5900;
      /*
	FIXME: actually enable multicast
      */
	    
      
      rfbFramebufferUpdateMsg *fu = (rfbFramebufferUpdateMsg *)cl->updateBuf;
      fu->type = rfbFramebufferUpdate;
      fu->nRects = Swap16IfLE((uint16_t)(1));
   
      cl->ublen = sz_rfbFramebufferUpdateMsg;
      
      
      rfbFramebufferUpdateRectHeader rect;
      char buffer[512];

      snprintf(buffer,sizeof(buffer)-1, "%s", "multicast address plus port");
      
      /* flush update buffer before appending if too much inside */
      if (cl->ublen + sz_rfbFramebufferUpdateRectHeader  + (strlen(buffer)+1) > UPDATE_BUF_SIZE) 
	{
	  if (!rfbSendUpdateBuf(cl))
	    return FALSE;
	     
	}
	
      rect.encoding = Swap32IfLE(rfbMulticastPseudoEncoding);
      rect.r.x = 0;
      rect.r.y = Swap16IfLE(port);
      rect.r.w = Swap16IfLE(strlen(buffer)+1);
      rect.r.h = 0;

      /* append the header to client's update buffer */
      memcpy(&cl->updateBuf[cl->ublen], (char *)&rect, sz_rfbFramebufferUpdateRectHeader);
      cl->ublen += sz_rfbFramebufferUpdateRectHeader;

      /* an append our message */
      memcpy(&cl->updateBuf[cl->ublen], buffer, strlen(buffer)+1);
      cl->ublen += strlen(buffer)+1;

      rfbStatRecordEncodingSent(cl, rfbMulticastPseudoEncoding,
				sz_rfbFramebufferUpdateRectHeader+strlen(buffer)+1,
				sz_rfbFramebufferUpdateRectHeader+strlen(buffer)+1);
    
      if (!rfbSendUpdateBuf(cl))
	return FALSE;
	  

      rfbLog("Enabling MultiCast VNC protocol extension\n");
     




      return TRUE;
    }

  return FALSE;
}





static int multicastEncodings[] = {rfbMulticastPseudoEncoding, 0};


static rfbProtocolExtension multicastExtension = {
  NULL,				/* newClient */
  NULL,				/* init */
  multicastEncodings,		/* pseudoEncodings */
  enableMulticast,		/* enablePseudoEncoding */
  NULL,                    	/* handleMessage */
  NULL,				/* close */
  NULL,				/* usage */
  NULL,				/* processArgument */
  NULL				/* next extension */
};



int main(int argc,char** argv)
{                                                                
  rfbScreenInfoPtr server;

  rfbRegisterProtocolExtension(&multicastExtension);

  server=rfbGetScreen(&argc,argv,400,300,8,3,4);
  server->frameBuffer=(char*)malloc(400*300*4);
  rfbInitServer(server);           
  rfbRunEventLoop(server,-1,FALSE);
  return(0);
}
