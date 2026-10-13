#include <rfb/rfb.h>

#include <errno.h>
#include <string.h>

static int peekCalled;
static int longPeekCalled;

static int failIfPeeked(rfbClientPtr cl, char *buf, int len)
{
  (void)cl;
  (void)buf;
  (void)len;
  peekCalled = 1;
  errno = EAGAIN;
  return -1;
}

static int slowRfbPeek(rfbClientPtr cl, char *buf, int len)
{
  (void)cl;

  if (len == 1) {
    buf[0] = 'R';
    return 1;
  }

  longPeekCalled = 1;
  errno = EAGAIN;
  return -1;
}

static int testRfbModeSkipsPeek(void)
{
  rfbScreenInfo screen;
  rfbClientRec client;

  memset(&screen, 0, sizeof(screen));
  memset(&client, 0, sizeof(client));

  screen.webSocketsHandshakeMode = rfbWebSocketsHandshakeRfb;
  client.screen = &screen;
  client.peekAtSocket = failIfPeeked;
  client.sock = RFB_INVALID_SOCKET;

  peekCalled = 0;
  if (!webSocketsCheck(&client))
    return 1;

  return peekCalled ? 1 : 0;
}

static int testAutoModeAcceptsSlowRfbPrefix(void)
{
  rfbScreenInfo screen;
  rfbClientRec client;

  memset(&screen, 0, sizeof(screen));
  memset(&client, 0, sizeof(client));

  screen.webSocketsHandshakeMode = rfbWebSocketsHandshakeAuto;
  client.screen = &screen;
  client.peekAtSocket = slowRfbPeek;
  client.sock = 0;

  longPeekCalled = 0;
  if (!webSocketsCheck(&client))
    return 1;

  return longPeekCalled ? 1 : 0;
}

int main(void)
{
  if (testRfbModeSkipsPeek())
    return 1;

  if (testAutoModeAcceptsSlowRfbPrefix())
    return 1;

  return 0;
}
