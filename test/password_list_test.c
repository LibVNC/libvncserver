#include <rfb/rfb.h>

#include <string.h>

int main(int argc, char **argv)
{
    rfbScreenInfoPtr screen;
    rfbClientRec cl;
    char response[CHALLENGESIZE + 1];
    char *passwords[] = { (char *)"password", NULL };

    screen = rfbGetScreen(&argc, argv, 4, 4, 8, 1, 1);
    if(!screen)
	return 1;

    memset(&cl, 0, sizeof(cl));
    memset(response, 'A', sizeof(response));

    cl.screen = screen;
    cl.host = (char *)"test";
    screen->authPasswdData = passwords;

    if(rfbCheckPasswordByList(&cl, response, sizeof(response)) != FALSE) {
	rfbScreenCleanup(screen);
	return 1;
    }

    rfbScreenCleanup(screen);
    return 0;
}
