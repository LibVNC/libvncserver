#include <rfb/rfbclient.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static rfbClient *seenClient;
static int clientLogCalls;
static char clientLogMessage[128];
static int legacyLogCalls;
static char legacyLogMessage[128];

static void
clientAwareLog(rfbClient *client, const char *format, va_list args)
{
    seenClient = client;
    clientLogCalls++;
    vsnprintf(clientLogMessage, sizeof(clientLogMessage), format, args);
}

static void
legacyLog(const char *format, ...)
{
    va_list args;

    legacyLogCalls++;
    va_start(args, format);
    vsnprintf(legacyLogMessage, sizeof(legacyLogMessage), format, args);
    va_end(args);
}

int
main(void)
{
    rfbClientLogProc oldLog = rfbClientLog;
    rfbClientLogProc oldErr = rfbClientErr;
    rfbClientLogProcWithClient oldLogWithClient = rfbClientLogWithClient;
    rfbClientLogProcWithClient oldErrWithClient = rfbClientErrWithClient;
    rfbClient *client = rfbGetClient(8, 3, 4);

    if(!client)
      return 1;

    rfbClientLogWithClient = clientAwareLog;
    rfbClientLogEx(client, "client %s", "message");
    if(clientLogCalls != 1 || seenClient != client || strcmp(clientLogMessage, "client message") != 0)
      return 2;

    rfbClientLogWithClient = NULL;
    rfbClientLog = legacyLog;
    rfbClientLogEx(client, "legacy %d", 42);
    if(legacyLogCalls != 1 || strcmp(legacyLogMessage, "legacy 42") != 0)
      return 3;

    rfbClientErrWithClient = clientAwareLog;
    rfbClientErrEx(client, "err %s", "message");
    if(clientLogCalls != 2 || seenClient != client || strcmp(clientLogMessage, "err message") != 0)
      return 4;

    rfbClientLog = oldLog;
    rfbClientErr = oldErr;
    rfbClientLogWithClient = oldLogWithClient;
    rfbClientErrWithClient = oldErrWithClient;
    rfbClientCleanup(client);
    return 0;
}
