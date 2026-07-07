#include <rfb/rfb.h>

#include <stdlib.h>

int main(int argc, char **argv)
{
    rfbScreenInfoPtr screen;
    char *fb;

    screen = rfbGetScreen(&argc, argv, 16, 16, 8, 3, 4);
    if (!screen)
	return 1;

    fb = (char *)calloc(16 * 16 * 4, 1);
    if (!fb) {
	rfbScreenCleanup(screen);
	return 1;
    }

    screen->frameBuffer = fb;
    screen->port = 0;
    screen->ipv6port = 0;
    screen->httpDir = NULL;

    rfbInitServer(screen);
    rfbShutdownServer(screen, TRUE);
    rfbScreenCleanup(screen);
    free(fb);

    return 0;
}
