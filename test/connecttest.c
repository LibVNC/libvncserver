/* Exercise failure cleanup through the public IPv4 connection functions.
 * ELF symbol interposition lets the test inject a failed mode change without
 * changing the real socket, connect, or close operations. */
#include <rfb/rfbclient.h>
#include "sockets.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fail_mode_change;
static rfbSocket failed_socket = RFB_INVALID_SOCKET;
static int failures;

static void check(int condition, const char *message)
{
  if (!condition) {
    fprintf(stderr, "%s\n", message);
    ++failures;
  }
}

/* Calls from the shared library resolve to this test's definition. */
rfbBool SetNonBlocking(rfbSocket sock)
{
  if (fail_mode_change) {
    failed_socket = sock;
    errno = EIO;
    return FALSE;
  }
  return sock_set_nonblocking(sock, TRUE, rfbClientErr);
}

static void failed_connect(int legacy)
{
  rfbSocket result;
  int stdin_flags = fcntl(STDIN_FILENO, F_GETFL);
  int still_open;
  fail_mode_change = 1;
  failed_socket = RFB_INVALID_SOCKET;
  result = legacy ? ConnectClientToTcpAddr(htonl(INADDR_LOOPBACK), 1)
                  : ConnectClientToTcpAddrWithTimeout(htonl(INADDR_LOOPBACK), 1, 1);
  fail_mode_change = 0;
  check(failed_socket != RFB_INVALID_SOCKET, "mode-change fault was not reached");
  check(result == RFB_INVALID_SOCKET, "setup failure returned a valid descriptor");
  still_open = fcntl(failed_socket, F_GETFD) != -1;
  check(!still_open && errno == EBADF, "setup failure retained its socket");
  check(fcntl(STDIN_FILENO, F_GETFL) == stdin_flags, "legacy wrapper changed stdin flags");
  if (still_open)
    close(failed_socket);
}

static void successful_connect(int legacy)
{
  struct sockaddr_in address;
  socklen_t length = sizeof(address);
  rfbSocket listener, client, accepted;
  int flags;
  char byte;
  listener = socket(AF_INET, SOCK_STREAM, 0);
  if (listener == RFB_INVALID_SOCKET) exit(2);
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(listener, (struct sockaddr *)&address, sizeof(address)) ||
      listen(listener, 1) || getsockname(listener, (struct sockaddr *)&address, &length)) exit(2);
  client = legacy ? ConnectClientToTcpAddr(address.sin_addr.s_addr, ntohs(address.sin_port))
                  : ConnectClientToTcpAddrWithTimeout(address.sin_addr.s_addr, ntohs(address.sin_port), 1);
  check(client != RFB_INVALID_SOCKET, "loopback connect failed");
  if (client == RFB_INVALID_SOCKET) { close(listener); return; }
  flags = fcntl(client, F_GETFL);
  check(flags >= 0 && !!(flags & O_NONBLOCK) == !legacy, "connected socket has wrong blocking mode");
  accepted = accept(listener, NULL, NULL);
  if (accepted == RFB_INVALID_SOCKET) exit(2);
  check(write(client, "x", 1) == 1, "connected socket write failed");
  check(read(accepted, &byte, 1) == 1 && byte == 'x', "connected socket lost data");
  close(client);
  close(accepted);
  close(listener);
}

int main(void)
{
  int i;
  int saved_stdin = dup(STDIN_FILENO);
  int input[2];
  /* Use our own input file description so the legacy wrapper's accidental
   * SetBlocking(0) is observable without affecting the invoking process. */
  if (pipe(input) || dup2(input[0], STDIN_FILENO) < 0) return 2;
  if (input[0] != STDIN_FILENO) close(input[0]);
  close(input[1]);
  if (fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) < 0) return 2;
  for (i = 0; i < 10; ++i) {
    failed_connect(0);
    successful_connect(0);
    failed_connect(1);
    successful_connect(1);
  }
  if (saved_stdin >= 0) {
    dup2(saved_stdin, STDIN_FILENO);
    close(saved_stdin);
  } else {
    close(STDIN_FILENO);
  }
  return failures ? 1 : 0;
}
