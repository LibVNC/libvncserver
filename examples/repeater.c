/* This example shows how to connect to an UltraVNC repeater. */

#include <rfb/rfb.h>

static void clientGone(rfbClientPtr cl)
{
  rfbShutdownServer(cl->screen, TRUE);
}

int main(int argc,char** argv)
{
  char *repeaterHost;
  int repeaterPort, sock;
  char id[250];
  rfbClientPtr cl;

  int i,j;
  uint16_t* f;

  /* Parse command-line arguments */
  if (argc < 3) {
    fprintf(stderr,
      "Usage: %s <id> <repeater-host> [<repeater-port>]\n", argv[0]);
    exit(1);
  }
  snprintf(id, sizeof(id) - 1, "ID:%s", argv[1]);
  repeaterHost = argv[2];
  repeaterPort = argc < 4 ? 5500 : atoi(argv[3]);

  /* The initialization is identical to simple15.c */
  rfbScreenInfoPtr server=rfbGetScreen(&argc,argv,400,300,5,3,2);
  if(!server)
    return 0;
  server->frameBuffer=(char*)malloc(400*300*2);
  f=(uint16_t*)server->frameBuffer;
  for(j=0;j<300;j++)
    for(i=0;i<400;i++)
      f[j*400+i]=/* red */ ((j*32/300) << 10) |
		 /* green */ (((j+400-i)*32/700) << 5) |
		 /* blue */ ((i*32/400));

  /* Now for the repeater-specific part: */
  server->port = -1; /* do not listen on any port */
  server->ipv6port = -1; /* do not listen on any port */

  sock = rfbConnectToTcpAddr(repeaterHost, repeaterPort);
  if (sock < 0) {
    perror("connect to repeater");
    return 1;
  }
  if (write(sock, id, sizeof(id)) != sizeof(id)) {
    perror("writing id");
    return 1;
  }
  cl = rfbNewClient(server, sock);
  if (!cl) {
    perror("new client");
    return 1;
  }
  cl->reverseConnection = 0;
  cl->clientGoneHook = clientGone;

  /* Run the server */
  rfbInitServer(server);
  rfbRunEventLoop(server,-1,FALSE);

  return 0;
}
