/*
 * rfbSendServerCutTextUTF8() must release the sendMutex of every client it
 * sends to, including a client without Extended Clipboard when no Latin-1
 * fallback is given. Nothing else can be sent to a client whose sendMutex
 * stays locked.
 */

#include <rfb/rfb.h>

#if defined(LIBVNCSERVER_HAVE_LIBPTHREAD) && defined(LIBVNCSERVER_HAVE_LIBZ) && !defined(WIN32)

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

static int checkUnlocked(rfbClientPtr cl, const char *what)
{
    if (pthread_mutex_trylock(&cl->sendMutex) != 0) {
        fprintf(stderr, "%s: sendMutex left locked\n", what);
        return 1;
    }
    pthread_mutex_unlock(&cl->sendMutex);
    return 0;
}

int main(int argc, char **argv)
{
    rfbScreenInfoPtr screen;
    rfbClientPtr cl;
    int sv[2], failed = 0;

    rfbLogEnable(FALSE);
    screen = rfbGetScreen(&argc, argv, 32, 32, 8, 3, 4);
    if (!screen)
        return 1;
    screen->frameBuffer = (char *)calloc(32 * 32, 4);
    screen->port = 0;     /* no listening socket */
    screen->ipv6port = 0;
    rfbInitServer(screen);

    /* a client without Extended Clipboard, no thread serves it */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0 || (cl = rfbNewClient(screen, sv[0])) == NULL)
        return 1;

    rfbSendServerCutTextUTF8(screen, "hello", 5, "hello", 5);
    failed += checkUnlocked(cl, "with Latin-1 fallback");
    rfbSendServerCutTextUTF8(screen, "hello", 5, NULL, 0);
    failed += checkUnlocked(cl, "without Latin-1 fallback");
    return failed != 0;
}

#else

#include <stdio.h>

int main(void)
{
    printf("cuttexttest needs pthreads, zlib and socketpair(), skipped\n");
    return 0;
}

#endif
