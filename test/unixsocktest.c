#include <rfb/rfb.h>

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static char observedHost[256];
static rfbBool hookWasCalled = FALSE;

static enum rfbNewClientAction
recordAndRefuseClient(rfbClientPtr cl)
{
    hookWasCalled = TRUE;
    if (cl->host)
	snprintf(observedHost, sizeof(observedHost), "%s", cl->host);
    else
	observedHost[0] = '\0';
    return RFB_CLIENT_REFUSE;
}

int
main(int argc, char **argv)
{
    rfbScreenInfoPtr screen;
    int fds[2];
    rfbClientPtr client;
    int failed;

    failed = 0;
    fds[0] = -1;
    fds[1] = -1;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
	perror("socketpair");
	return 1;
    }

    screen = rfbGetScreen(&argc, argv, 8, 8, 8, 3, 4);
    if (!screen) {
	close(fds[0]);
	close(fds[1]);
	return 1;
    }

    screen->newClientHook = recordAndRefuseClient;
    client = rfbNewClient(screen, fds[0]);
    if (client != NULL) {
	fprintf(stderr, "expected refused client to be cleaned up\n");
	rfbCloseClient(client);
	rfbClientConnectionGone(client);
	failed = 1;
    }

    if (!hookWasCalled) {
	fprintf(stderr, "newClientHook was not called\n");
	failed = 1;
    } else if (observedHost[0] == '\0') {
	fprintf(stderr, "AF_UNIX client host must not be empty\n");
	failed = 1;
    }

    rfbScreenCleanup(screen);
    close(fds[1]);

    return failed ? 1 : 0;
}
