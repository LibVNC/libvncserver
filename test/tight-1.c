#include <time.h>
#include <rfb/rfb.h>
#include <rfb/rfbclient.h>

#ifndef LIBVNCSERVER_HAVE_LIBTHREAD
//#error This test need pthread support (otherwise the client blocks the client)
#endif

MUTEX(frameBufferMutex);

int testEncodings[]={
	rfbEncodingRaw,
	rfbEncodingRRE,
	rfbEncodingCoRRE,
	rfbEncodingHextile,
#ifdef LIBVNCSERVER_HAVE_LIBZ
	rfbEncodingZlib,
	rfbEncodingZlibHex,
	rfbEncodingZRLE,
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
	rfbEncodingTight,
#endif
#endif
	0
};

#define NUMBER_OF_ENCODINGS_TO_TEST (sizeof(testEncodings)/sizeof(int)-1)

/* Here come the variables/functions to handle the test output */

struct { int x1,y1,x2,y2; } lastUpdateRect;
unsigned int statistics[NUMBER_OF_ENCODINGS_TO_TEST];
unsigned int totalFailed;
unsigned int countGotUpdate;
MUTEX(statisticsMutex);

void initStatistics() {
	memset(statistics,0,sizeof(int)*NUMBER_OF_ENCODINGS_TO_TEST);
	totalFailed=0;
	INIT_MUTEX(statisticsMutex);
}

void updateServerStatistics(int x1,int y1,int x2,int y2) {
	LOCK(statisticsMutex);
	countGotUpdate=0;
	lastUpdateRect.x1=x1;
	lastUpdateRect.y1=y1;
	lastUpdateRect.x2=x2;
	lastUpdateRect.y2=y2;
	UNLOCK(statisticsMutex);
}

void updateStatistics(int encodingIndex,rfbBool failed) {
	LOCK(statisticsMutex);
	if(failed) {
		statistics[encodingIndex]++;
		totalFailed++;
	}
	countGotUpdate++;
	UNLOCK(statisticsMutex);
}

	

/* Here begin the functions for the client. They will be called in a
 * pthread. */

/* maxDelta=0 means they are expected to match exactly;
 * maxDelta>0 means that the average difference must be lower than maxDelta */
rfbBool doFramebuffersMatch(rfbScreenInfo* server,rfbClient* client,
		int maxDelta)
{
	int i,j,k;
	unsigned int total=0,diff=0;
	if(server->width!=client->width || server->height!=client->height)
		return FALSE;
	LOCK(frameBufferMutex);
	/* TODO: write unit test for colour transformation, use here, too */
	for(i=0;i<server->width;i++)
		for(j=0;j<server->height;j++)
			for(k=0;k<server->serverFormat.bitsPerPixel/8;k++) {
				unsigned char s=server->frameBuffer[k+i*4+j*server->paddedWidthInBytes];
				unsigned char cl=client->frameBuffer[k+i*4+j*client->width];
				
				if(maxDelta==0 && s!=cl) {
					UNLOCK(frameBufferMutex);
					return FALSE;
				} else {
					total++;
					diff+=(s>cl?s-cl:cl-s);
				}
			}
	UNLOCK(frameBufferMutex);
	if(maxDelta>0 && diff/total>=maxDelta)
		return FALSE;
	return TRUE;
}

static rfbBool resize(rfbClient* cl) {
	if(cl->frameBuffer)
		free(cl->frameBuffer);
	cl->frameBuffer=(char*)malloc(cl->width*cl->height*cl->format.bitsPerPixel/8);
}

typedef struct clientData {
	int encodingIndex;
	rfbScreenInfo* server;
} clientData;

static void update(rfbClient* client,int x,int y,int w,int h) {
	clientData* cd=(clientData*)client->clientData;
	int maxDelta=0;

	if(testEncodings[cd->encodingIndex]==rfbEncodingTight)
		maxDelta=5;
	
	/* TODO: check if dimensions match with marked rectangle */

	/* only check if this was the last update */
	if(x+w!=lastUpdateRect.x2 || y+h!=lastUpdateRect.y2)
		return;
	updateStatistics(cd->encodingIndex,
			!doFramebuffersMatch(cd->server,client,maxDelta));
}

static void* clientLoop(void* data) {
	rfbClient* client=(rfbClient*)data;
	int argc=2;
	char* argv[2]={"client",":0"};
	if(!rfbInitClient(client,&argc,argv))
		return 0;
	while(1) {
		if(WaitForMessage(client,50)>=0)
			if(!HandleRFBServerMessage(client))
				break;
	}
	if(client->frameBuffer)
		free(client->frameBuffer);
	rfbClientCleanup(client);
	return 0;
}

static void startClient(int encodingIndex,rfbScreenInfo* server) {
	rfbClient* client=rfbGetClient(8,3,4);
	clientData* cd;
	pthread_t clientThread;
	
	client->clientData=malloc(sizeof(clientData));
	client->MallocFrameBuffer=resize;
	client->GotFrameBufferUpdate=update;

	cd=(clientData*)client->clientData;
	cd->encodingIndex=encodingIndex;
	cd->server=server;
	
	pthread_create(&clientThread,NULL,clientLoop,(void*)client);
}

/* Here begin the server functions */

static void idle(rfbScreenInfo* server)
{
	int c;
	rfbBool goForward;

	LOCK(statisticsMutex);
	goForward=(countGotUpdate==NUMBER_OF_ENCODINGS_TO_TEST);
	UNLOCK(statisticsMutex);
	if(!goForward)
		return;

	LOCK(frameBufferMutex);
	for(c=0;c<3;c++) {
		int x1=(rand()%server->width),x2=(rand()%server->width),
		y1=(rand()%server->height),y2=(rand()%server->height);
		int i,j;
		if(x1>x2) { i=x1; x1=x2; x2=i; }
		if(y1>y2) { i=y1; y1=y2; y2=i; }
		for(i=x1;i<=x2;i++)
			for(j=y1;j<=y2;j++)
				server->frameBuffer[i*4+c+j*server->paddedWidthInBytes]=255*(i-x1+j-y1+1)/(x2-x1+y2-y1+1);
		rfbMarkRectAsModified(server,x1,y1,x2+1,y2+1);
	}
	UNLOCK(frameBufferMutex);
}

//TODO: pthread'ize the client. otherwise server and client block each
//other
int main(int argc,char** argv)
{                                                                
	int i,j;
	time_t t;

	/* Initialize server */
	rfbScreenInfoPtr server=rfbGetScreen(&argc,argv,400,300,8,3,4);

	server->frameBuffer=malloc(400*300*4);
	for(j=0;j<400*300*4;j++)
		server->frameBuffer[j]=j;
	rfbInitServer(server);
	rfbProcessEvents(server,50);

	/* Initialize clients */
	for(i=0;i<NUMBER_OF_ENCODINGS_TO_TEST;i++)
		startClient(i,server);
	
	t=time(0);
	/* test 20 seconds */
	while(time(0)-t<20) {

		idle(server);

		rfbProcessEvents(server,50);
	}

	free(server->frameBuffer);
	rfbScreenCleanup(server);

	printf("Statistics: %d failed\n",totalFailed);
	if(totalFailed)
		return 1;
	return(0);
}

