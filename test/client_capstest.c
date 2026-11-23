#include <rfb/rfbclient.h>

int main(void)
{
  rfbClient* client;

  client = rfbGetClient(8, 3, 4);
  if (!client)
    return 1;

  client->screen.width = rfbClientSwap16IfLE(640);
  client->screen.height = rfbClientSwap16IfLE(480);

  if (SupportsClient2Server(client, rfbSetDesktopSize)) {
    rfbClientCleanup(client);
    return 1;
  }

  if (!SendExtDesktopSize(client, 800, 600)) {
    rfbClientCleanup(client);
    return 1;
  }

  rfbClientCleanup(client);
  return 0;
}
