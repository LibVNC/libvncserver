/*
  Fuzzing client (server->client RFB decoder) for LibVNCClient.

  This complements the existing server-side fuzzer (test/fuzz_server.c). It
  drives the client-side message handler HandleRFBServerMessage(), which
  dispatches to the framebuffer decoders in libvncclient/ (tight, zrle, hextile,
  rre, corre, ultra, zlib, cursor, ...) that parse untrusted server->client data.

  Build and run it locally exactly like test/fuzz_server.c, then execute
  build/fuzz_client.

 */


#include <rfb/rfbclient.h>

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>

#define FUZZ_FB_WIDTH     64
#define FUZZ_FB_HEIGHT    64
#define FUZZ_FB_BPP       4
#define FUZZ_MAX_INPUT    65536
#define FUZZ_MAX_MESSAGES 512

static void noop_log(const char *format, ...) { (void)format; }

/* Clamp the geometry back to the fixed framebuffer on every resize, otherwise a
   tiny rect could drive the decoders over an attacker-controlled width/height. */
static rfbBool keep_framebuffer(rfbClient *client) {
    client->width = FUZZ_FB_WIDTH;
    client->height = FUZZ_FB_HEIGHT;
    return TRUE;
}

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    int sv[2];
    size_t off = 0;
    int iterations = 0;
    rfbClient *client;

    if (Size > FUZZ_MAX_INPUT)
        return 0;

    rfbClientLog = noop_log;
    rfbClientErr = noop_log;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
        return 0;

    while (off < Size) {
        ssize_t w = write(sv[0], Data + off, Size - off);
        if (w <= 0)
            break;
        off += (size_t)w;
    }
    shutdown(sv[0], SHUT_WR);

    client = rfbGetClient(8, 3, FUZZ_FB_BPP);
    if (!client) {
        close(sv[0]);
        close(sv[1]);
        return 0;
    }

    client->width = FUZZ_FB_WIDTH;
    client->height = FUZZ_FB_HEIGHT;
    client->canHandleNewFBSize = FALSE;
    client->MallocFrameBuffer = keep_framebuffer;
    client->appData.useRemoteCursor = TRUE;
    client->frameBuffer =
        (uint8_t *)calloc(1, (size_t)FUZZ_FB_WIDTH * FUZZ_FB_HEIGHT * FUZZ_FB_BPP);
    if (!client->frameBuffer) {
        rfbClientCleanup(client);
        close(sv[0]);
        close(sv[1]);
        return 0;
    }
    client->sock = sv[1];

    while (iterations++ < FUZZ_MAX_MESSAGES && HandleRFBServerMessage(client))
        ;

    free(client->frameBuffer);
    client->frameBuffer = NULL;
    rfbClientCleanup(client);
    close(sv[0]);
    return 0;
}
