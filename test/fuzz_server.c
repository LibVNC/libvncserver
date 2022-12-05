#include <rfb/rfb.h>

static int initialized = 0;
rfbScreenInfoPtr server;
char *fakeargv[] = {"fuzz_server"};

extern size_t fuzz_offset;
extern size_t fuzz_size;
extern const uint8_t *fuzz_data;


int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    if (initialized == 0) {
        int fakeargc=1;
        server=rfbGetScreen(&fakeargc,fakeargv,400,300,8,3,4);
        server->frameBuffer=malloc(400*300*4);
        rfbInitServer(server);
        initialized = 1;
    }
    rfbClientPtr cl = rfbNewClient(server, RFB_INVALID_SOCKET - 1);

    fuzz_data = Data;
    fuzz_offset = 0;
    fuzz_size = Size;
    while (cl->sock != RFB_INVALID_SOCKET) {
        rfbProcessClientMessage(cl);
    }
    rfbClientConnectionGone(cl);
    return 0;
}

