/**
 * @file test_shutdown_no_client.c
 * Regression test for Issue #704:
 * Crash on rfbShutdownServer() when screen->httpDir is NULL and no client connected.
 */

#include <stdio.h>
#include <stdlib.h>
#include <rfb/rfb.h>

int main(int argc, char **argv)
{
    rfbScreenInfoPtr screen = rfbGetScreen(&argc, argv, 100, 100, 8, 3, 4);
    if (!screen) {
        fprintf(stderr, "Failed to initialize rfbScreen\n");
        return 1;
    }

    screen->frameBuffer = (char *)calloc(100 * 100 * 4, 1);
    if (!screen->frameBuffer) {
        rfbScreenCleanup(screen);
        return 1;
    }

    screen->httpDir = NULL; /* Explicitly NULL as reported in issue #704 */
    rfbInitServer(screen);

    /* Verify safe shutdown with no client connected */
    rfbShutdownServer(screen, TRUE);
    rfbScreenCleanup(screen);
    free(screen->frameBuffer);

    printf("Issue #704 regression test passed successfully.\n");
    return 0;
}
