/**
 * @example filetransfer.c
 * A small LibVNCClient example for legacy UltraVNC/TightVNC-1.3.x
 * FileTransfer messages.
 *
 * This example intentionally uses the legacy rfbFileTransfer message type.
 * TightVNC-2.x file transfer uses a separate protocol extension and is not
 * implemented here.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <rfb/rfbclient.h>

#ifdef WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

typedef enum {
  FT_OP_LIST,
  FT_OP_DOWNLOAD
} FileTransferOperation;

typedef struct {
  FileTransferOperation operation;
  const char *remotePath;
  const char *localPath;
  FILE *downloadFile;
  uint32_t expectedSize;
  uint32_t receivedSize;
  rfbBool accessGranted;
  rfbBool requestSent;
  rfbBool done;
  rfbBool failed;
} FileTransferState;

static void usage(const char *program)
{
  fprintf(stderr,
          "Usage:\n"
          "  %s -ft-list <server-path> <host[:display]>\n"
          "  %s -ft-download <server-path> <local-file> <host[:display]>\n"
          "\n"
          "Paths use the legacy UltraVNC/TightVNC-1.3.x file-transfer format.\n"
          "On Unix LibVNCServer examples, C:/tmp/file maps to /tmp/file.\n",
          program, program);
}

static void removeArg(int *argc, char **argv, int index, int count)
{
  int i;

  for (i = index; i + count < *argc; i++)
    argv[i] = argv[i + count];
  *argc -= count;
}

static rfbBool parseFileTransferArgs(int *argc, char **argv, FileTransferState *state)
{
  int i;

  memset(state, 0, sizeof(*state));
  state->operation = FT_OP_LIST;
  state->remotePath = "C:/";

  for (i = 1; i < *argc;) {
    if (strcmp(argv[i], "-ft-list") == 0) {
      if (i + 1 >= *argc)
        return FALSE;
      state->operation = FT_OP_LIST;
      state->remotePath = argv[i + 1];
      removeArg(argc, argv, i, 2);
      continue;
    }

    if (strcmp(argv[i], "-ft-download") == 0) {
      if (i + 2 >= *argc)
        return FALSE;
      state->operation = FT_OP_DOWNLOAD;
      state->remotePath = argv[i + 1];
      state->localPath = argv[i + 2];
      removeArg(argc, argv, i, 3);
      continue;
    }

    i++;
  }

  if (state->operation == FT_OP_DOWNLOAD && state->localPath == NULL)
    return FALSE;

  return TRUE;
}

static void printDirectoryEntry(const char *data, uint32_t length)
{
  /* RFB_FIND_DATA.cFileName starts at byte 44 in the legacy wire layout. */
  const uint32_t fileNameOffset = 44;

  if (length == 0)
    return;

  if (length > fileNameOffset && data[fileNameOffset] != '\0')
    printf("  %s\n", data + fileNameOffset);
  else
    printf("Directory: %.*s\n", (int)length, data);
}

static rfbBool sendFileTransferRequest(rfbClient *client, FileTransferState *state)
{
  uint32_t length = (uint32_t)strlen(state->remotePath);

  state->requestSent = TRUE;

  if (state->operation == FT_OP_LIST) {
    rfbClientLog("Requesting remote directory listing: %s\n", state->remotePath);
    return FileTransferSend(client, rfbDirContentRequest, rfbRDirContent,
                            0, length, state->remotePath);
  }

  rfbClientLog("Requesting remote file download: %s -> %s\n",
               state->remotePath, state->localPath);
  return FileTransferSend(client, rfbFileTransferRequest, 0,
                          0, length, state->remotePath);
}

static void handleFileTransfer(rfbClient *client, uint8_t contentType,
                               uint8_t contentParam, uint32_t size,
                               const char *data, uint32_t length)
{
  FileTransferState *state = (FileTransferState *)rfbClientGetClientData(client, handleFileTransfer);

  if (state == NULL)
    return;

  switch (contentType) {
  case rfbFileTransferAccess:
    state->accessGranted = (size == 1);
    if (!state->accessGranted) {
      rfbClientErr("File transfer access denied by server\n");
      state->failed = TRUE;
      state->done = TRUE;
      return;
    }
    if (!state->requestSent && !sendFileTransferRequest(client, state)) {
      state->failed = TRUE;
      state->done = TRUE;
    }
    return;

  case rfbDirPacket:
    if (state->operation != FT_OP_LIST)
      return;

    if (length == 0) {
      state->done = TRUE;
      return;
    }

    if (contentParam == rfbADirectory || contentParam == rfbAFile)
      printDirectoryEntry(data, length);
    return;

  case rfbFileHeader:
    if (state->operation != FT_OP_DOWNLOAD)
      return;

    if (size == 0xFFFFFFFFU) {
      rfbClientErr("Server reported file download error for %s\n", state->remotePath);
      state->failed = TRUE;
      state->done = TRUE;
      return;
    }

    state->downloadFile = fopen(state->localPath, "wb");
    if (state->downloadFile == NULL) {
      rfbClientErr("Cannot open %s for writing: %s\n", state->localPath, strerror(errno));
      FileTransferSend(client, rfbFileHeader, 0, 0xFFFFFFFFU, 0, NULL);
      state->failed = TRUE;
      state->done = TRUE;
      return;
    }

    state->expectedSize = size;
    state->receivedSize = 0;
    FileTransferSend(client, rfbFileHeader, 0, size, 0, NULL);
    return;

  case rfbFilePacket:
    if (state->operation != FT_OP_DOWNLOAD || state->downloadFile == NULL)
      return;

    if (size != 0) {
      rfbClientErr("Compressed file packets are not supported by this example\n");
      FileTransferSend(client, rfbAbortFileTransfer, 0, 0, 0, NULL);
      state->failed = TRUE;
      state->done = TRUE;
      return;
    }

    if (length > 0 && fwrite(data, 1, length, state->downloadFile) != length) {
      rfbClientErr("Failed to write %s: %s\n", state->localPath, strerror(errno));
      FileTransferSend(client, rfbAbortFileTransfer, 0, 0, 0, NULL);
      state->failed = TRUE;
      state->done = TRUE;
      return;
    }

    state->receivedSize += length;
    return;

  case rfbEndOfFile:
    if (state->downloadFile != NULL) {
      fclose(state->downloadFile);
      state->downloadFile = NULL;
    }
    state->done = TRUE;
    return;

  default:
    rfbClientLog("FileTransfer message: type=%u param=%u size=%u length=%u\n",
                 contentType, contentParam, size, length);
    return;
  }
}

int main(int argc, char **argv)
{
  rfbClient *client;
  FileTransferState state;
  time_t deadline;

  if (!parseFileTransferArgs(&argc, argv, &state)) {
    usage(argv[0]);
    return 1;
  }

  client = rfbGetClient(8, 3, 4);
  if (client == NULL)
    return 1;

  client->HandleFileTransfer = handleFileTransfer;
  rfbClientSetClientData(client, handleFileTransfer, &state);

  if (!rfbInitClient(client, &argc, argv))
    return 1;

  if (!FileTransferSend(client, rfbAbortFileTransfer, 1, 0, 0, NULL)) {
    rfbClientCleanup(client);
    return 1;
  }

  deadline = time(NULL) + 30;
  while (!state.done && time(NULL) < deadline) {
    int n = WaitForMessage(client, 100000);
    if (n < 0)
      break;
    if (n > 0 && !HandleRFBServerMessage(client))
      break;
  }

  if (state.downloadFile != NULL)
    fclose(state.downloadFile);

  if (!state.done && !state.failed) {
    rfbClientErr("Timed out waiting for file transfer response\n");
    state.failed = TRUE;
  }

  if (state.operation == FT_OP_DOWNLOAD && !state.failed)
    rfbClientLog("Downloaded %u/%u bytes to %s\n",
                 state.receivedSize, state.expectedSize, state.localPath);

  rfbClientCleanup(client);
  return state.failed ? 1 : 0;
}
