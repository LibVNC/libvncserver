#include <rfb/rfbclient.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void enablePointerEvent(rfbClient *client)
{
  memset(&client->supportedMessages, 0, sizeof(client->supportedMessages));
  client->supportedMessages.client2server[(rfbPointerEvent & 0xff) / 8] |=
      (1 << (rfbPointerEvent % 8));
}

static int readExact(int fd, unsigned char *buf, size_t len)
{
  size_t off = 0;
  while (off < len) {
    ssize_t n = read(fd, buf + off, len - off);
    if (n <= 0)
      return 0;
    off += (size_t)n;
  }
  return 1;
}

static int expectNoByte(int fd)
{
  unsigned char ch;
  ssize_t n = recv(fd, &ch, 1, MSG_DONTWAIT);
  if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
    return 1;
  return 0;
}

static int expectBytes(int fd, const unsigned char *expected, size_t len)
{
  unsigned char got[16];
  if (len > sizeof(got))
    return 0;
  if (!readExact(fd, got, len))
    return 0;
  if (memcmp(got, expected, len) != 0) {
    size_t i;
    fprintf(stderr, "unexpected bytes:\n");
    for (i = 0; i < len; ++i)
      fprintf(stderr, "  %zu: got 0x%02x expected 0x%02x\n", i, got[i], expected[i]);
    return 0;
  }
  return 1;
}

int main(void)
{
  int sv[2];
  rfbClient client;
  unsigned char expectedNormalBack[] = {
    rfbPointerEvent, rfbButton8Mask, 0x00, 0x0a, 0x00, 0x14
  };
  unsigned char expectedExtendedBackForward[] = {
    rfbPointerEvent, rfbPointerEventExtendedButtonMask,
    0x00, 0x0a, 0x00, 0x14, 0x03
  };

  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
    return 1;

  memset(&client, 0, sizeof(client));
  client.sock = sv[0];
  client.endianTest = 1;
  enablePointerEvent(&client);

  if (!SendPointerEvent(&client, 10, 20, rfbButton8Mask)) {
    fprintf(stderr, "normal back button event failed\n");
    return 1;
  }
  if (!expectBytes(sv[1], expectedNormalBack, sizeof(expectedNormalBack)))
    return 1;

  if (SendPointerEvent(&client, 10, 20, rfbButton9Mask)) {
    fprintf(stderr, "button 9 succeeded without ExtendedMouseButtons acknowledgement\n");
    return 1;
  }
  if (!expectNoByte(sv[1]))
    return 1;

  client.extendedMouseButtonsEnabled = TRUE;
  if (!SendPointerEvent(&client, 10, 20, rfbButton8Mask | rfbButton9Mask)) {
    fprintf(stderr, "extended back/forward button event failed\n");
    return 1;
  }
  if (!expectBytes(sv[1], expectedExtendedBackForward, sizeof(expectedExtendedBackForward)))
    return 1;

  close(sv[0]);
  close(sv[1]);
  return 0;
}
