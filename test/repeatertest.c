/*
 *  Test connecting a LibVNCServer-based server to an UltraVNC repeater.
 *
 *  This exercises rfbConnectToTcpAddr() -> sock_wait_for_connected(), which has
 *  to notice that a non-blocking connect() completed. A repeater in mode 2 says
 *  nothing until it has read the 250 byte id, so waiting for readability rather
 *  than writability here runs into the full rfbMaxClientWait timeout and the id
 *  never gets sent - see https://github.com/bk138/droidVNC-NG/issues/109.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <rfb/rfb.h>

#define REPEATER_ID "1234"
/* an UltraVNC repeater in mode 2 reads the id in one go, padded to 250 bytes */
#define REPEATER_ID_LEN 250
/* connecting to a repeater on loopback is immediate, anything else is a bug */
#define CONNECT_TIMEOUT 2

static int fails = 0;

#define CHECK(cond, ...) if(!(cond)) { fprintf(stderr, "FAIL: " __VA_ARGS__); fails++; }

/* stand in for the repeater: a listening socket that says nothing by itself */
static int fake_repeater(int *port)
{
  int listener;
  struct sockaddr_in addr;
  socklen_t len = sizeof addr;

  if((listener = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    return -1;

  memset(&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0; /* let the kernel pick a free one */

  if(bind(listener, (struct sockaddr *)&addr, sizeof addr) < 0 ||
     listen(listener, 1) < 0 ||
     getsockname(listener, (struct sockaddr *)&addr, &len) < 0) {
    close(listener);
    return -1;
  }

  *port = ntohs(addr.sin_port);
  return listener;
}

int main(void)
{
  int listener, conn, port;
  char id[REPEATER_ID_LEN];
  char version[13];
  time_t start;
  double elapsed;
  rfbScreenInfoPtr screen;
  rfbClientPtr cl;
  char *framebuffer;

  if((listener = fake_repeater(&port)) < 0) {
    rfbLogPerror("could not set up the fake repeater");
    return 1;
  }

  framebuffer = calloc(320 * 200, 4);
  screen = rfbGetScreen(NULL, NULL, 320, 200, 8, 3, 4);
  if(!screen || !framebuffer)
    return 1;
  screen->frameBuffer = framebuffer;
  /* outward connection only, do not listen for inbound ones */
  screen->port = 0;
  screen->ipv6port = 0;
  screen->autoPort = FALSE;
  rfbInitServer(screen);

  start = time(NULL);
  cl = rfbUltraVNCRepeaterMode2Connection(screen, "127.0.0.1", port, REPEATER_ID);
  elapsed = difftime(time(NULL), start);

  CHECK(cl != NULL, "rfbUltraVNCRepeaterMode2Connection() failed after %.0f s\n", elapsed);
  CHECK(elapsed <= CONNECT_TIMEOUT, "connecting took %.0f s, connect completion is not"
        " detected but waited out\n", elapsed);

  if(cl) {
    if((conn = accept(listener, NULL, NULL)) < 0) {
      rfbLogPerror("accept");
      return 1;
    }

    /* the repeater reads the id first, the ProtocolVersion message second */
    memset(id, 0, sizeof id);
    CHECK(recv(conn, id, sizeof id, MSG_WAITALL) == (ssize_t)sizeof id,
        "did not get %d id bytes\n", REPEATER_ID_LEN);
    CHECK(strcmp(id, "ID:" REPEATER_ID) == 0, "got id \"%s\", expected \"ID:%s\"\n",
        id, REPEATER_ID);

    memset(version, 0, sizeof version);
    CHECK(recv(conn, version, 12, MSG_WAITALL) == 12 && strncmp(version, "RFB 00", 6) == 0,
        "got no ProtocolVersion message, but \"%s\"\n", version);

    close(conn);
  }

  close(listener);
  rfbShutdownServer(screen, TRUE);
  rfbScreenCleanup(screen);
  free(framebuffer);

  return fails ? 1 : 0;
}
