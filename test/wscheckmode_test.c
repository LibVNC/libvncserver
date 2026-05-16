#include <rfb/rfb.h>

#include <errno.h>
#include <string.h>

static int peekCalled;

static int failIfPeeked(rfbClientPtr cl, char *buf, int len)
{
  (void)cl;
  (void)buf;
  (void)len;
  peekCalled = 1;
  errno = EAGAIN;
  return -1;
}

int main(void)
{
  rfbScreenInfo screen;
  rfbClientRec client;

  memset(&screen, 0, sizeof(screen));
  memset(&client, 0, sizeof(client));

  screen.webSocketsHandshakeMode = rfbWebSocketsHandshakeRfb;
  client.screen = &screen;
  client.peekAtSocket = failIfPeeked;
  client.sock = RFB_INVALID_SOCKET;

  if (!webSocketsCheck(&client))
    return 1;

  return peekCalled ? 1 : 0;
}
