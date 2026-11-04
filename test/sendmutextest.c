/*
 * Regression tests for client messages handled under cl->sendMutex:
 * ExtendedClipboard Caps, write errors while ExtendedClipboard messages are
 * sent, and SetPixelFormat.
 *
 * Write errors are injected through cl->writeToSocket. The close() callback of
 * a test protocol extension calls rfbSendBell(), which takes sendMutex, so a
 * client closed with sendMutex held deadlocks. Each scenario runs in a child
 * process that alarm() terminates after WATCHDOG_S seconds, so a deadlock
 * fails the test instead of hanging it. Clients are connected through
 * socketpair(), no TCP port is opened.
 *
 * Usage: sendmutextest [scenario...], all scenarios by default.
 * Set SENDMUTEXTEST_VERBOSE to see the library log.
 */

#include <rfb/rfb.h>

#if defined(LIBVNCSERVER_HAVE_LIBPTHREAD) && defined(LIBVNCSERVER_HAVE_LIBZ) && !defined(WIN32)

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define WIDTH 32
#define HEIGHT 32
#define TIMEOUT_MS 3000  /* for each expected message or close */
#define WATCHDOG_S 10    /* for each scenario */

#define ALL_ACTIONS (rfbExtendedClipboard_Caps | rfbExtendedClipboard_Request | rfbExtendedClipboard_Peek | \
                     rfbExtendedClipboard_Notify | rfbExtendedClipboard_Provide)

static rfbScreenInfoPtr screen;
static ClientWriteToSocket defaultWriteToSocket;

/* shared with the server threads */
static pthread_mutex_t stateMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t stateCond = PTHREAD_COND_INITIALIZER;
static int failWrites;          /* server writes to fail: 0 none, -1 all */
static rfbBool closeSendsBell;  /* the extension's close() takes sendMutex */
static int closeCalls, clientsGone;

/* some scenarios write to the client socket from two threads */
static pthread_mutex_t clientWriteMutex = PTHREAD_MUTEX_INITIALIZER;


/* ---------- server side ---------- */

static int faultyWriteToSocket(rfbClientPtr cl, const char *buf, int len)
{
    rfbBool fail;

    pthread_mutex_lock(&stateMutex);
    fail = failWrites != 0;
    if (failWrites > 0)
        failWrites--;
    pthread_mutex_unlock(&stateMutex);
    if (fail) {
        errno = EPIPE;
        return -1;
    }
    return defaultWriteToSocket(cl, buf, len);
}

static void setFaults(int writesToFail, rfbBool sendBellOnClose)
{
    pthread_mutex_lock(&stateMutex);
    failWrites = writesToFail;
    closeSendsBell = sendBellOnClose;
    pthread_mutex_unlock(&stateMutex);
}

static void clientGone(rfbClientPtr cl)
{
    pthread_mutex_lock(&stateMutex);
    clientsGone++;
    pthread_cond_broadcast(&stateCond);
    pthread_mutex_unlock(&stateMutex);
}

static rfbBool waitClientsGone(int n)
{
    struct timespec deadline;
    rfbBool ok;

    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += TIMEOUT_MS / 1000;
    pthread_mutex_lock(&stateMutex);
    while (clientsGone < n && pthread_cond_timedwait(&stateCond, &stateMutex, &deadline) == 0)
        ;
    ok = clientsGone >= n;
    pthread_mutex_unlock(&stateMutex);
    return ok;
}

static rfbBool extensionNewClient(rfbClientPtr cl, void **data)
{
    static int extensionData;

    *data = &extensionData;
    return TRUE;
}

static void extensionClose(rfbClientPtr cl, void *data)
{
    rfbBool sendBell;

    pthread_mutex_lock(&stateMutex);
    sendBell = closeSendsBell && closeCalls == 0;
    closeCalls++;
    pthread_mutex_unlock(&stateMutex);

    /* like an extension saying goodbye to its client */
    if (sendBell)
        rfbSendBell(cl->screen);
}

static rfbProtocolExtension closeExtension = {
    extensionNewClient,  /* newClient */
    NULL,                /* init */
    NULL,                /* pseudoEncodings */
    NULL,                /* enablePseudoEncoding */
    NULL,                /* handleMessage */
    extensionClose,      /* close */
    NULL,                /* usage */
    NULL,                /* processArgument */
    NULL                 /* next */
};

static void setXCutText(char *str, int len, rfbClientPtr cl) {}
static rfbBool xvp(rfbClientPtr cl, uint8_t version, uint8_t code) { return TRUE; }

static rfbBool startServer(void)
{
    int argc = 1;
    char *argv[] = { "sendmutextest", NULL };

    rfbLogEnable(getenv("SENDMUTEXTEST_VERBOSE") != NULL);
    rfbRegisterProtocolExtension(&closeExtension);
    screen = rfbGetScreen(&argc, argv, WIDTH, HEIGHT, 8, 3, 4);
    if (!screen)
        return FALSE;
    screen->frameBuffer = (char *)calloc(WIDTH * HEIGHT, 4);
    screen->port = 0;     /* no listening socket */
    screen->ipv6port = 0;
    screen->deferUpdateTime = 0;
    screen->setXCutText = setXCutText;
    screen->setXCutTextUTF8 = setXCutText;
    screen->xvpHook = xvp;
    rfbInitServer(screen);
    rfbRunEventLoop(screen, 10000, TRUE);
    return TRUE;
}

static void serverCutText(const char *text)
{
    rfbSendServerCutTextUTF8(screen, (char *)text, strlen(text), (char *)text, strlen(text));
}


/* ---------- client side ---------- */

static long long nowMs(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* 0 on success, -1 on error, EOF or timeout */
static int readFull(int fd, void *buf, size_t len)
{
    long long deadline = nowMs() + TIMEOUT_MS;
    char *p = buf;

    while (len > 0) {
        struct pollfd pfd = { fd, POLLIN, 0 };
        long long left = deadline - nowMs();
        ssize_t n;

        if (left <= 0 || poll(&pfd, 1, (int)left) <= 0 || (n = read(fd, p, len)) <= 0)
            return -1;
        p += n;
        len -= n;
    }
    return 0;
}

static int writeFull(int fd, const void *buf, size_t len)
{
    const char *p = buf;
    ssize_t n = 0;

    pthread_mutex_lock(&clientWriteMutex);
    while (len > 0 && (n = write(fd, p, len)) > 0) {
        p += n;
        len -= n;
    }
    pthread_mutex_unlock(&clientWriteMutex);
    return len == 0 ? 0 : -1;
}

/* TRUE once the server closed the connection, data before that is discarded */
static rfbBool expectClosed(int fd)
{
    long long deadline = nowMs() + TIMEOUT_MS;
    char buf[4096];

    for (;;) {
        struct pollfd pfd = { fd, POLLIN, 0 };
        long long left = deadline - nowMs();
        ssize_t n;

        if (left <= 0 || poll(&pfd, 1, (int)left) <= 0)
            break;
        n = read(fd, buf, sizeof(buf));
        if (n == 0 || (n < 0 && errno == ECONNRESET))
            return TRUE;
        if (n < 0)
            break;
    }
    pthread_mutex_lock(&stateMutex);
    fprintf(stderr, "    client not closed, close() callbacks entered: %d\n", closeCalls);
    pthread_mutex_unlock(&stateMutex);
    return FALSE;
}

static void putU16(unsigned char *p, uint16_t v) { p[0] = v >> 8; p[1] = v & 0xff; }
static void putU32(unsigned char *p, uint32_t v) { putU16(p, v >> 16); putU16(p + 2, v & 0xffff); }
static uint16_t getU16(const unsigned char *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static uint32_t getU32(const unsigned char *p) { return (uint32_t)getU16(p) << 16 | getU16(p + 2); }

/* returns the client end of a new connection, after the RFB 3.8 handshake */
static int connectClient(void)
{
    unsigned char buf[3 + 255];
    rfbClientPtr cl;
    int sv[2];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0)
        return -1;
    if ((cl = rfbNewClient(screen, sv[0])) == NULL) {
        close(sv[1]);
        return -1;
    }
    defaultWriteToSocket = cl->writeToSocket;
    cl->writeToSocket = faultyWriteToSocket;
    cl->clientGoneHook = clientGone;
    rfbStartOnHoldClient(cl);

    buf[0] = rfbSecTypeNone;
    buf[1] = 1; /* shared */
    if (readFull(sv[1], buf + 2, 12) < 0 || writeFull(sv[1], "RFB 003.008\n", 12) < 0 ||
        readFull(sv[1], buf + 2, 1) < 0 || buf[2] == 0 || readFull(sv[1], buf + 3, buf[2]) < 0 ||
        writeFull(sv[1], buf, 1) < 0 || readFull(sv[1], buf + 2, 4) < 0 || getU32(buf + 2) != 0 ||
        writeFull(sv[1], buf + 1, 1) < 0 || readFull(sv[1], buf, 24) < 0 ||
        getU32(buf + 20) > sizeof(buf) || readFull(sv[1], buf, getU32(buf + 20)) < 0) {
        close(sv[1]);
        return -1;
    }
    return sv[1];
}

static int sendSetEncodings(int fd, const uint32_t *encodings, int n)
{
    unsigned char buf[4 + 4 * 8];
    int i;

    buf[0] = rfbSetEncodings;
    buf[1] = 0;
    putU16(buf + 2, n);
    for (i = 0; i < n; i++)
        putU32(buf + 4 + 4 * i, encodings[i]);
    return writeFull(fd, buf, 4 + 4 * n);
}

/* Caps with one size per format bit, the text size is textMax; nSizes < 0 sends all of them */
static int sendCaps(int fd, uint32_t flags, uint32_t textMax, int nSizes)
{
    unsigned char buf[8 + 4 + 4 * 16];
    int i, n = 0;

    for (i = 0; i < 16; i++)
        if ((flags & (1 << i)) && (nSizes < 0 || n < nSizes))
            putU32(buf + 12 + 4 * n++, i == 0 ? textMax : 0);
    buf[0] = rfbClientCutText;
    buf[1] = buf[2] = buf[3] = 0;
    putU32(buf + 4, (uint32_t)-(int32_t)(4 + 4 * n));
    putU32(buf + 8, flags);
    return writeFull(fd, buf, 12 + 4 * n);
}

static int sendClipboardAction(int fd, uint32_t action)
{
    unsigned char buf[12] = { rfbClientCutText, 0, 0, 0 };

    putU32(buf + 4, (uint32_t)-4);
    putU32(buf + 8, action | rfbExtendedClipboard_Text);
    return writeFull(fd, buf, sizeof(buf));
}

/* xvp version 2 is answered with an xvp fail message: a round trip through the input thread */
static int sendPing(int fd)
{
    unsigned char buf[4] = { rfbXvp, 0, 2, 0 };

    return writeFull(fd, buf, sizeof(buf));
}

static int sendSetPixelFormat(int fd, int bpp, int trueColour)
{
    unsigned char buf[20] = { rfbSetPixelFormat };

    buf[4] = bpp;
    buf[5] = bpp;
    buf[7] = trueColour;
    putU16(buf + 8, bpp == 16 ? 31 : 255);
    putU16(buf + 10, bpp == 16 ? 63 : 255);
    putU16(buf + 12, bpp == 16 ? 31 : 255);
    buf[14] = bpp == 16 ? 11 : 16;
    buf[15] = bpp == 16 ? 5 : 8;
    return writeFull(fd, buf, sizeof(buf));
}

static int sendUpdateRequest(int fd)
{
    unsigned char buf[10] = { rfbFramebufferUpdateRequest, 0 /* not incremental */ };

    putU16(buf + 6, WIDTH);
    putU16(buf + 8, HEIGHT);
    return writeFull(fd, buf, sizeof(buf));
}

typedef struct {
    int type;
    rfbBool extended;   /* ServerCutText */
    uint32_t flags;     /* extended ServerCutText */
    int32_t length;     /* plain ServerCutText */
    int xvpVersion, xvpCode;
    int colours;        /* SetColourMapEntries */
    int rawBytes;       /* FramebufferUpdate, raw rectangles only */
} ServerMsg;

static int skip(int fd, size_t len)
{
    char buf[4096];

    while (len > 0) {
        size_t n = len < sizeof(buf) ? len : sizeof(buf);
        if (readFull(fd, buf, n) < 0)
            return -1;
        len -= n;
    }
    return 0;
}

static int readServerMsg(int fd, ServerMsg *m, int bytesPerPixel)
{
    unsigned char buf[12];
    int i, rects;

    memset(m, 0, sizeof(*m));
    if (readFull(fd, buf, 1) < 0)
        return -1;
    m->type = buf[0];
    switch (m->type) {
    case rfbServerCutText:
        if (readFull(fd, buf, 7) < 0)
            return -1;
        m->length = (int32_t)getU32(buf + 3);
        m->extended = m->length < 0;
        if (!m->extended)
            return skip(fd, m->length);
        if (-m->length < 4 || readFull(fd, buf, 4) < 0)
            return -1;
        m->flags = getU32(buf);
        return skip(fd, -m->length - 4);
    case rfbXvp:
        if (readFull(fd, buf, 3) < 0)
            return -1;
        m->xvpVersion = buf[1];
        m->xvpCode = buf[2];
        return 0;
    case rfbBell:
        return 0;
    case rfbSetColourMapEntries:
        if (readFull(fd, buf, 5) < 0)
            return -1;
        m->colours = getU16(buf + 3);
        return skip(fd, 6 * m->colours);
    case rfbFramebufferUpdate:
        if (readFull(fd, buf, 3) < 0)
            return -1;
        rects = getU16(buf + 1);
        for (i = 0; i < rects; i++) {
            if (readFull(fd, buf, 12) < 0 || getU32(buf + 8) != rfbEncodingRaw)
                return -1;
            m->rawBytes += getU16(buf + 4) * getU16(buf + 6) * bytesPerPixel;
            if (skip(fd, getU16(buf + 4) * getU16(buf + 6) * bytesPerPixel) < 0)
                return -1;
        }
        return 0;
    default:
        fprintf(stderr, "    unexpected server message type %d\n", m->type);
        return -1;
    }
}

static rfbBool expectExtended(int fd, uint32_t flag)
{
    ServerMsg m;

    if (readServerMsg(fd, &m, 4) < 0 || m.type != rfbServerCutText || !m.extended || !(m.flags & flag)) {
        fprintf(stderr, "    expected an extended ServerCutText with 0x%x, got type %d flags 0x%x\n",
                flag, m.type, m.flags);
        return FALSE;
    }
    return TRUE;
}

static rfbBool expectPong(int fd)
{
    ServerMsg m;

    if (sendPing(fd) < 0 || readServerMsg(fd, &m, 4) < 0 || m.type != rfbXvp || m.xvpVersion != 2) {
        fprintf(stderr, "    no answer to the xvp round trip\n");
        return FALSE;
    }
    return TRUE;
}

static int connectWithExtendedClipboard(void)
{
    uint32_t enc = rfbEncodingExtendedClipboard;
    int fd = connectClient();

    if (fd >= 0 && (sendSetEncodings(fd, &enc, 1) < 0 || !expectExtended(fd, rfbExtendedClipboard_Caps))) {
        close(fd);
        fd = -1;
    }
    return fd;
}

/* reads server messages until the connection ends, counting xvp round trips */
typedef struct {
    int fd;
    pthread_t thread;
    pthread_mutex_t mutex;
    int pongs;
} Drain;

static void *drainRun(void *arg)
{
    Drain *d = arg;
    ServerMsg m;

    while (readServerMsg(d->fd, &m, 4) == 0) {
        pthread_mutex_lock(&d->mutex);
        if (m.type == rfbXvp && m.xvpVersion == 2)
            d->pongs++;
        pthread_mutex_unlock(&d->mutex);
    }
    return NULL;
}

static void startDrain(Drain *d, int fd)
{
    d->fd = fd;
    d->pongs = 0;
    pthread_mutex_init(&d->mutex, NULL);
    pthread_create(&d->thread, NULL, drainRun, d);
}

static void stopDrain(Drain *d)
{
    shutdown(d->fd, SHUT_RDWR);
    pthread_join(d->thread, NULL);
    close(d->fd);
}


/* ---------- write errors with sendMutex held ---------- */

static rfbBool capsWriteError(void)
{
    uint32_t enc = rfbEncodingExtendedClipboard;
    int fd = connectClient();

    setFaults(1, TRUE);
    return fd >= 0 && sendSetEncodings(fd, &enc, 1) == 0 && expectClosed(fd);
}

static rfbBool replyWriteError(uint32_t action)
{
    int fd = connectWithExtendedClipboard();

    if (fd < 0)
        return FALSE;
    serverCutText("hello");
    if (!expectExtended(fd, rfbExtendedClipboard_Provide))
        return FALSE;
    setFaults(1, TRUE);
    return sendClipboardAction(fd, action) == 0 && expectClosed(fd);
}

static rfbBool requestWriteError(void) { return replyWriteError(rfbExtendedClipboard_Request); }
static rfbBool peekWriteError(void) { return replyWriteError(rfbExtendedClipboard_Peek); }

static rfbBool serverCutTextWriteError(void)
{
    int fd = connectWithExtendedClipboard();

    if (fd < 0)
        return FALSE;
    setFaults(1, TRUE);
    serverCutText("hello");
    return expectClosed(fd);
}


/* ---------- SetPixelFormat ---------- */

static rfbBool invalidPixelFormat(int bpp, int trueColour)
{
    int fd = connectClient();

    setFaults(0, TRUE);
    return fd >= 0 && sendSetPixelFormat(fd, bpp, trueColour) == 0 && expectClosed(fd);
}

static rfbBool pixelFormatInvalidBpp(void) { return invalidPixelFormat(12, 1); }
static rfbBool pixelFormatInvalidColourMap(void) { return invalidPixelFormat(16, 0); }

static rfbBool pixelFormatValid(void)
{
    static const struct { int bpp, trueColour; } formats[] = {
        { 16, 1 },
        { 8, 0 },  /* colour map clients get the BGR233 map first */
#ifdef LIBVNCSERVER_ALLOW24BPP
        { 24, 1 },
#endif
        { 32, 1 },
    };
    uint32_t enc = rfbEncodingRaw;
    ServerMsg m;
    size_t i;
    int fd = connectClient();

    if (fd < 0 || sendSetEncodings(fd, &enc, 1) < 0)
        return FALSE;
    for (i = 0; i < sizeof(formats) / sizeof(formats[0]); i++) {
        int bytes = formats[i].bpp / 8;

        if (sendSetPixelFormat(fd, formats[i].bpp, formats[i].trueColour) < 0)
            return FALSE;
        if (!formats[i].trueColour &&
            (readServerMsg(fd, &m, bytes) < 0 || m.type != rfbSetColourMapEntries || m.colours != 256)) {
            fprintf(stderr, "    %d bpp: expected 256 colour map entries, got type %d\n", formats[i].bpp, m.type);
            return FALSE;
        }
        if (sendUpdateRequest(fd) < 0 || readServerMsg(fd, &m, bytes) < 0 ||
            m.type != rfbFramebufferUpdate || m.rawBytes != WIDTH * HEIGHT * bytes) {
            fprintf(stderr, "    %d bpp: got type %d, %d raw bytes\n", formats[i].bpp, m.type, m.rawBytes);
            return FALSE;
        }
    }
    return expectPong(fd);
}


/* ---------- incoming ExtendedClipboard Caps ---------- */

static rfbBool capsMaxSize(void)
{
    uint32_t flags = ALL_ACTIONS | rfbExtendedClipboard_Text;
    int fd = connectWithExtendedClipboard();

    /* up to the announced size the text is provided, above it only announced */
    if (fd < 0 || sendCaps(fd, flags, 16, -1) < 0 || !expectPong(fd))
        return FALSE;
    serverCutText("0123456789abcdef");
    if (!expectExtended(fd, rfbExtendedClipboard_Provide))
        return FALSE;
    serverCutText("0123456789abcdefg");
    if (!expectExtended(fd, rfbExtendedClipboard_Notify))
        return FALSE;

    /* without Provide, any text is only announced */
    if (sendCaps(fd, flags & ~rfbExtendedClipboard_Provide, 1 << 20, -1) < 0 || !expectPong(fd))
        return FALSE;
    serverCutText("abc");
    return expectExtended(fd, rfbExtendedClipboard_Notify);
}

/* Caps without text, or without any format, disable the extension */
static rfbBool capsDisable(void)
{
    static const uint32_t caps[] = { ALL_ACTIONS | rfbExtendedClipboard_RTF, ALL_ACTIONS };
    ServerMsg m;
    size_t i;

    for (i = 0; i < sizeof(caps) / sizeof(caps[0]); i++) {
        int fd = connectWithExtendedClipboard();

        if (fd < 0 || sendCaps(fd, caps[i], 0, -1) < 0 || !expectPong(fd))
            return FALSE;
        serverCutText("hello");
        if (readServerMsg(fd, &m, 4) < 0 || m.type != rfbServerCutText || m.extended || m.length != 5) {
            fprintf(stderr, "    caps 0x%x: expected a plain ServerCutText of 5 bytes, got type %d length %d\n",
                    caps[i], m.type, m.length);
            return FALSE;
        }
        close(fd);
    }
    return TRUE;
}

static rfbBool capsCorrupt(void)
{
    int fd = connectWithExtendedClipboard();

    /* two formats, one size */
    return fd >= 0 &&
        sendCaps(fd, ALL_ACTIONS | rfbExtendedClipboard_Text | rfbExtendedClipboard_RTF, 16, 1) == 0 &&
        expectClosed(fd);
}

static void *sendTexts(void *arg)
{
    int i;

    for (i = 0; i < 2000; i++)
        serverCutText("0123456789abcdef0123456789abcdef");
    return NULL;
}

/* meant for ThreadSanitizer: Caps received while texts are sent */
static rfbBool capsWhileSending(void)
{
    uint32_t flags;
    pthread_t texts;
    long long deadline;
    Drain d;
    int i, pongs = 0, fd = connectWithExtendedClipboard();

    if (fd < 0)
        return FALSE;
    startDrain(&d, fd);
    pthread_create(&texts, NULL, sendTexts, NULL);
    for (i = 0; i < 2000; i++) {
        flags = ALL_ACTIONS | rfbExtendedClipboard_Text;
        if (i & 1)
            flags &= ~rfbExtendedClipboard_Provide;
        if (sendCaps(fd, flags, (i & 2) ? 8 : 1 << 20, -1) < 0)
            break;
    }
    pthread_join(texts, NULL);

    /* the client must still be served */
    sendPing(fd);
    for (deadline = nowMs() + TIMEOUT_MS; pongs == 0 && nowMs() < deadline; usleep(1000)) {
        pthread_mutex_lock(&d.mutex);
        pongs = d.pongs;
        pthread_mutex_unlock(&d.mutex);
    }
    stopDrain(&d);
    if (pongs == 0)
        fprintf(stderr, "    client not served after concurrent caps\n");
    return pongs > 0;
}


/* ---------- concurrent close ---------- */

static void *requestUpdates(void *arg)
{
    int fd = *(int *)arg;

    /* ends when the server closes the connection */
    while (sendUpdateRequest(fd) == 0)
        usleep(200);
    return NULL;
}

/* all writes fail while the output thread sends updates and the input thread a clipboard reply */
static rfbBool concurrentClose(void)
{
    uint32_t encodings[2] = { rfbEncodingRaw, rfbEncodingExtendedClipboard };
    int round;

    for (round = 1; round <= 10; round++) {
        pthread_t requester;
        Drain d;
        int fd = connectClient();

        if (fd < 0 || sendSetEncodings(fd, encodings, 2) < 0 || !expectExtended(fd, rfbExtendedClipboard_Caps))
            return FALSE;
        serverCutText("hello");
        if (!expectExtended(fd, rfbExtendedClipboard_Provide))
            return FALSE;

        startDrain(&d, fd);
        pthread_create(&requester, NULL, requestUpdates, &fd);
        usleep(2000);
        setFaults(-1, FALSE);
        sendClipboardAction(fd, rfbExtendedClipboard_Request);
        if (!waitClientsGone(round)) {
            fprintf(stderr, "    round %d: client not gone\n", round);
            return FALSE;
        }
        pthread_join(requester, NULL);
        stopDrain(&d);
        setFaults(0, FALSE);
    }

    pthread_mutex_lock(&stateMutex);
    round = clientsGone;
    pthread_mutex_unlock(&stateMutex);
    if (round != 10)
        fprintf(stderr, "    %d clients gone for 10 connections\n", round);
    return round == 10;
}


/* ---------- SetEncodings ---------- */

static rfbBool setEncodingsCapsAndXvp(void)
{
    uint32_t encodings[2] = { rfbEncodingExtendedClipboard, rfbEncodingXvp };
    ServerMsg m;
    int fd = connectClient();

    /* the caps are sent while the list is applied, xvp init once sendMutex is released */
    if (fd < 0 || sendSetEncodings(fd, encodings, 2) < 0 || !expectExtended(fd, rfbExtendedClipboard_Caps))
        return FALSE;
    if (readServerMsg(fd, &m, 4) < 0 || m.type != rfbXvp || m.xvpVersion != 1 || m.xvpCode != rfbXvp_Init) {
        fprintf(stderr, "    expected xvp init after the caps, got type %d\n", m.type);
        return FALSE;
    }
    return expectPong(fd);
}


/* ---------- driver ---------- */

typedef struct {
    const char *name;
    rfbBool (*run)(void);
} Scenario;

static const Scenario scenarios[] = {
    { "caps-write-error", capsWriteError },
    { "request-write-error", requestWriteError },
    { "peek-write-error", peekWriteError },
    { "servercuttext-write-error", serverCutTextWriteError },
    { "pixelformat-invalid-bpp", pixelFormatInvalidBpp },
    { "pixelformat-invalid-colourmap", pixelFormatInvalidColourMap },
    { "pixelformat-valid", pixelFormatValid },
    { "caps-max-size", capsMaxSize },
    { "caps-disable", capsDisable },
    { "caps-corrupt", capsCorrupt },
    { "caps-while-sending", capsWhileSending },
    { "concurrent-close", concurrentClose },
    { "setencodings-caps-xvp", setEncodingsCapsAndXvp },
};

/* "PASS", or why the scenario failed */
static const char *runScenario(const Scenario *s)
{
    pid_t pid;
    int status;

    fflush(NULL);
    pid = fork();
    if (pid == 0) {
        signal(SIGPIPE, SIG_IGN);
        alarm(WATCHDOG_S); /* terminates the child if a scenario hangs */
        _exit(startServer() && s->run() ? 0 : 1);
    }
    if (pid < 0 || waitpid(pid, &status, 0) < 0)
        return "FAIL (fork)";
    if (WIFSIGNALED(status))
        return WTERMSIG(status) == SIGALRM ? "FAIL (stuck)" : "FAIL (crashed)";
    return WEXITSTATUS(status) == 0 ? "PASS" : "FAIL";
}

int main(int argc, char **argv)
{
    size_t i;
    int a, failed = 0, ran = 0;

    for (i = 0; i < sizeof(scenarios) / sizeof(scenarios[0]); i++) {
        const char *result;
        int selected = argc == 1;

        for (a = 1; a < argc; a++)
            if (strcmp(argv[a], scenarios[i].name) == 0)
                selected = 1;
        if (!selected)
            continue;
        result = runScenario(&scenarios[i]);
        printf("%-30s %s\n", scenarios[i].name, result);
        fflush(stdout);
        ran++;
        if (strcmp(result, "PASS") != 0)
            failed++;
    }
    printf("%d of %d scenarios failed\n", failed, ran);
    return failed == 0 && ran > 0 ? 0 : 1;
}

#else

#include <stdio.h>

int main(void)
{
    printf("sendmutextest needs pthreads, zlib and POSIX sockets, skipped\n");
    return 0;
}

#endif
