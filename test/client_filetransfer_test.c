#include <rfb/rfbclient.h>

#ifndef _WIN32
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
static int callback_called;
static uint8_t callback_content_type;
static uint8_t callback_content_param;
static uint32_t callback_size;
static uint32_t callback_length;
static char callback_payload[32];

static void handle_file_transfer(rfbClient *client, uint8_t contentType, uint8_t contentParam, uint32_t size, const char *data, uint32_t length)
{
  (void)client;

  callback_called++;
  callback_content_type = contentType;
  callback_content_param = contentParam;
  callback_size = size;
  callback_length = length;

  if (length >= sizeof(callback_payload))
    length = sizeof(callback_payload) - 1;
  if (length > 0 && data != NULL)
    memcpy(callback_payload, data, length);
  callback_payload[length] = '\0';
}

static int test_receive_file_transfer(void)
{
  int sv[2];
  const char payload[] = "payload";
  rfbFileTransferMsg msg;
  rfbClient *client;
  int rc = 1;

  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
    return 1;

  client = rfbGetClient(8, 3, 4);
  if (!client)
    goto out_sockets;

  memset(&msg, 0, sizeof(msg));
  msg.type = rfbFileTransfer;
  msg.contentType = rfbDirPacket;
  msg.contentParam = rfbAFile;
  msg.size = rfbClientSwap32IfLE(1234);
  msg.length = rfbClientSwap32IfLE((uint32_t)strlen(payload));

  client->sock = sv[1];
  client->HandleFileTransfer = handle_file_transfer;

  if (write(sv[0], &msg, sz_rfbFileTransferMsg) != sz_rfbFileTransferMsg)
    goto out_client;
  if (write(sv[0], payload, strlen(payload)) != (ssize_t)strlen(payload))
    goto out_client;

  if (!HandleRFBServerMessage(client))
    goto out_client;

  if (callback_called != 1 ||
      callback_content_type != rfbDirPacket ||
      callback_content_param != rfbAFile ||
      callback_size != 1234 ||
      callback_length != strlen(payload) ||
      strcmp(callback_payload, payload) != 0)
    goto out_client;

  rc = 0;

out_client:
  rfbClientCleanup(client);
out_sockets:
  close(sv[0]);
  if (rc != 0)
    close(sv[1]);
  return rc;
}

static int test_send_file_transfer(void)
{
  int sv[2];
  const char payload[] = "request";
  rfbFileTransferMsg msg;
  char buf[sizeof(payload)];
  rfbClient *client;
  int rc = 1;

  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
    return 1;

  client = rfbGetClient(8, 3, 4);
  if (!client)
    goto out_sockets;

  client->sock = sv[0];
  client->supportedMessages.client2server[rfbFileTransfer / 8] |= (1 << (rfbFileTransfer % 8));

  if (!FileTransferSend(client, rfbDirContentRequest, rfbRDirContent, 42,
                        (uint32_t)strlen(payload), payload))
    goto out_client;

  if (read(sv[1], &msg, sz_rfbFileTransferMsg) != sz_rfbFileTransferMsg)
    goto out_client;
  if (read(sv[1], buf, strlen(payload)) != (ssize_t)strlen(payload))
    goto out_client;

  if (msg.type != rfbFileTransfer ||
      msg.contentType != rfbDirContentRequest ||
      msg.contentParam != rfbRDirContent ||
      rfbClientSwap32IfLE(msg.size) != 42 ||
      rfbClientSwap32IfLE(msg.length) != strlen(payload) ||
      memcmp(buf, payload, strlen(payload)) != 0)
    goto out_client;

  rc = 0;

out_client:
  rfbClientCleanup(client);
out_sockets:
  close(sv[1]);
  if (rc != 0)
    close(sv[0]);
  return rc;
}
#endif

int main(void)
{
#ifdef _WIN32
  return 0;
#else
  if (test_receive_file_transfer() != 0) {
    fprintf(stderr, "receive file transfer callback test failed\n");
    return 1;
  }

  if (test_send_file_transfer() != 0) {
    fprintf(stderr, "send file transfer helper test failed\n");
    return 1;
  }

  return 0;
#endif
}
