#include <assert.h>
#include <rfb/rfb.h>
#include <rfb/rfbclient.h>

#include "nacro.h"

/* for visual grepping */
typedef struct image_t {
	int width,height;
	char* buffer;
} image_t;

/* this is a VNC connection */
typedef struct private_resource_t {
	int listen_port;
	rfbScreenInfo* server;
	rfbClient* client;

	uint32_t keysym;
	rfbBool keydown;

	int x,y;
	int buttons;

	image_t* grep_image;
	int x_origin,y_origin;

	enum { SLEEP,VISUALGREP,WAITFORUPDATE } state;
	result_t result;
} private_resource_t;

/* resource management */

#define MAX_RESOURCE_COUNT 20

static private_resource_t resource_pool[MAX_RESOURCE_COUNT];
static int resource_count=0;

private_resource_t* get_resource(int resource)
{
	if(resource>=MAX_RESOURCE_COUNT || resource<0 || resource_pool[resource].client==0)
		return 0;
	return resource_pool+resource;
}

private_resource_t* get_next_resource()
{
	if(resource_count<MAX_RESOURCE_COUNT) {
		memset(resource_pool+resource_count,0,sizeof(private_resource_t));
		resource_count++;
		return resource_pool+resource_count-1;
	} else {
		int i;

		for(i=0;i<MAX_RESOURCE_COUNT && resource_pool[i].client;i++);
		if(i<MAX_RESOURCE_COUNT)
			return resource_pool+i;
	}
	return 0;
}

void free_resource(int resource)
{
	private_resource_t* res=get_resource(resource);
	if(res)
		res->client=0;
}

/* hooks */

void got_key(rfbBool down,rfbKeySym keysym,rfbClientRec* cl)
{
	private_resource_t* res=(private_resource_t*)cl->screen->screenData;
	
	res->keydown=down;
	res->keysym=keysym;
	res->result|=RESULT_KEY;
}

void got_mouse(int buttons,int x,int y,rfbClientRec* cl)
{
	private_resource_t* res=(private_resource_t*)cl->screen->screenData;
	
	res->buttons=buttons;
	res->x=x;
	res->y=y;
	res->result|=RESULT_MOUSE;
}

rfbBool malloc_frame_buffer(rfbClient* cl)
{
	private_resource_t* res=(private_resource_t*)cl->clientData;

	if(!res->server) {
		int w=cl->width,h=cl->height;
		
		res->client->frameBuffer=malloc(w*4*h);
		
		res->server=rfbGetScreen(0,0,w,h,8,3,4);
		res->server->screenData=res;
		res->server->port=res->listen_port;
		res->server->frameBuffer=res->client->frameBuffer;
		res->server->kbdAddEvent=got_key;
		res->server->ptrAddEvent=got_mouse;
		rfbInitServer(res->server);
	} else {
		/* TODO: realloc if necessary */
		/* TODO: resolution change: send NewFBSize */
		/* TODO: if the origin is out of bounds, reset to 0 */
	}
}

void got_frame_buffer(rfbClient* cl,int x,int y,int w,int h)
{
	private_resource_t* res=(private_resource_t*)cl->clientData;
	
	assert(res->server);
	
	if(res->grep_image) {
		/* TODO: find image and set x_origin,y_origin if found */
	} else {
		res->state=RESULT_SCREEN;
	}
	if(res->server) {
		rfbMarkRectAsModified(res->server,x,y,x+w,y+h);
	}

	res->result|=RESULT_SCREEN;
}

/* init/shutdown functions */

resource_t initvnc(const char* server,int server_port,int listen_port)
{
	private_resource_t* res=get_next_resource();
	int dummy=0;

	if(res==0)
		return -1;

	/* remember for later */
	res->listen_port=listen_port;
	
	res->client=rfbGetClient(8,3,4);
	res->client->clientData=res;
	res->client->GotFrameBufferUpdate=got_frame_buffer;
	res->client->MallocFrameBuffer=malloc_frame_buffer;
	res->client->serverHost=strdup(server);
	res->client->serverPort=server_port;
	res->client->appData.encodingsString="raw";
	if(!rfbInitClient(res->client,&dummy,0)) {
		res->client=0;
		return -1;
	}
	return res-resource_pool;
}

void closevnc(resource_t resource)
{
	private_resource_t* res=get_resource(resource);
	if(res==0)
		return;

	if(res->server)
		rfbScreenCleanup(res->server);

	assert(res->client);

	rfbClientCleanup(res->client);

	res->client=0;
}

/* PNM (image) helpers */

bool_t savepnm(resource_t resource,const char* filename,int x1,int y1,int x2,int y2)
{
	private_resource_t* res=get_resource(resource);
	int i,j,w,h;
	uint32_t* buffer;
	FILE* f;
	
	assert(res->client);
	assert(res->client->format.depth==24);

	w=res->client->width;
	h=res->client->height;
	buffer=(uint32_t*)res->client->frameBuffer;

	if(res==0 || x1>x2 || y1>y2 || x1<0 || x2>=w || y1<0 || y2>=h)
		return FALSE;

	f=fopen(filename,"wb");
	
	if(f==0)
		return FALSE;

	fprintf(f,"P6\n%d %d\n255\n",1+x2-x1,1+y2-y1);
	for(j=y1;j<=y2;j++)
		for(i=x1;i<=x2;i++) {
			fwrite(buffer+i+j*w,3,1,f);
		}
	if(fclose(f))
		return FALSE;
	return TRUE;
}

image_t* loadpnm(const char* filename)
{
	FILE* f=fopen(filename,"rb");
	char buffer[1024];
	int i,j,w,h;
	image_t* image;
	
	if(f==0)
		return 0;
	
	if(!fgets(buffer,1024,f) || strcmp("P6\n",buffer)) {
		fclose(f);
		return 0;
	}

	do {
		fgets(buffer,1024,f);
		if(feof(f)) {
			fclose(f);
			return 0;
		}
	} while(buffer[0]=='#');

	if(!fgets(buffer,1024,f) || sscanf(buffer,"%d %d",&w,&h)!=2
			|| !fgets(buffer,1024,f) || strcmp("255\n",buffer)) {
		fclose(f);
		return 0;
	}
		
	image=(image_t*)malloc(sizeof(image_t));
	image->width=w;
	image->height=h;
	image->buffer=malloc(w*4*h);
	if(!image->buffer) {
		fclose(f);
		free(image);
		return 0;
	}
	
	for(j=0;j<h;j++)
		for(i=0;i<w;i++)
			if(fread(image->buffer+4*(i+w*j),3,1,f)!=3) {
				fclose(f);
				free(image->buffer);
				free(image);
				return 0;
			}

	fclose(f);
	
	return image;
}

void free_image(image_t* image)
{
	if(image->buffer)
		free(image->buffer);
	free(image);
}

/* process() and friends */

/* this function returns only if res->result in return_mask */
result_t private_process(resource_t resource,timeout_t timeout_in_seconds,result_t return_mask)
{
	private_resource_t* res=get_resource(resource);
	fd_set fds;
	struct timeval tv,tv_start,tv_end;
	unsigned long timeout=(unsigned long)(timeout_in_seconds*1000000UL);
	int count,max_fd;

	if(res==0)
		return 0;

	assert(res->client);

	gettimeofday(&tv_start,0);
	res->result=0;

	do {
		unsigned long timeout_done;

		if(res->server) {
			rfbBool loop;
			do {
				loop=rfbProcessEvents(res->server,res->server->deferUpdateTime);
			} while(loop && res->result&return_mask==0);

			if(res->result&return_mask!=0)
				return res->result;

			memcpy((char*)&fds,(const char*)&(res->server->allFds),sizeof(fd_set));
			max_fd=res->server->maxFd;
		} else {
			FD_ZERO(&fds);
			max_fd=0;
		}
		FD_SET(res->client->sock,&fds);
		if(res->client->sock>max_fd)
			max_fd=res->client->sock;

		gettimeofday(&tv_end,0);
		timeout_done=tv_end.tv_usec-tv_start.tv_usec+
			1000000L*(tv_end.tv_sec-tv_start.tv_sec);
		if(timeout_done>=timeout)
			return RESULT_TIMEOUT;

		tv.tv_usec=((timeout-timeout_done)%1000000);
		tv.tv_sec=(timeout-timeout_done)/1000000;

		count=select(max_fd+1,&fds,0,0,&tv);
		if(count<0)
			return 0;

		if(count>0) {
			if(FD_ISSET(res->client->sock,&fds)) {
				if(!HandleRFBServerMessage(res->client))
					return 0;
				if(res->result&return_mask!=0)
					return res->result;
			}
		} else {
			res->result|=RESULT_TIMEOUT;
			return RESULT_TIMEOUT;
		}
	} while(1);

	return RESULT_TIMEOUT;
}

result_t process(resource_t res,timeout_t timeout)
{
	return private_process(res,timeout,RESULT_TIMEOUT);
}

result_t waitforanything(resource_t res,timeout_t timeout)
{
	return private_process(res,timeout,-1);
}

result_t waitforinput(resource_t res,timeout_t timeout)
{
	return private_process(res,timeout,RESULT_KEY|RESULT_MOUSE|RESULT_TIMEOUT);
}

result_t waitforupdate(resource_t res,timeout_t timeout)
{
	return private_process(res,timeout,RESULT_SCREEN|RESULT_TIMEOUT);
}

result_t visualgrep(resource_t res,const char* filename,timeout_t timeout)
{
	/* TODO: load filename and set res->grep_image to this image */
	return private_process(res,timeout,RESULT_FOUNDIMAGE|RESULT_TIMEOUT);
}

/* this is an overlay which is shown for a certain time */

result_t alert(resource_t resource,const char* message,timeout_t timeout)
{
	private_resource_t* res=get_resource(resource);
	char* fake_frame_buffer;
	char* backup;
	int w,h;
	result_t result;
	
	if(res->server==0)
		return -1;

	w=res->server->width;
	h=res->server->height;
	
	fake_frame_buffer=malloc(w*4*h);
	if(!fake_frame_buffer)
		return -1;
	memcpy(fake_frame_buffer,res->server->frameBuffer,w*4*h);
	/* TODO: draw message */
	
	backup=res->server->frameBuffer;
	res->server->frameBuffer=fake_frame_buffer;

	result=private_process(resource,timeout,-1);
	
	res->server->frameBuffer=backup;
	/* TODO: rfbMarkRectAsModified() */

	return result;
}
/* inspect last events */

keysym_t getkeysym(resource_t res)
{
	private_resource_t* r=get_resource(res);
	return r->keysym;
}

bool_t getkeydown(resource_t res)
{
	private_resource_t* r=get_resource(res);
	return r->keydown;
}

coordinate_t getx(resource_t res)
{
	private_resource_t* r=get_resource(res);
	return r->x;
}

coordinate_t gety(resource_t res)
{
	private_resource_t* r=get_resource(res);
	return r->y;
}

buttons_t getbuttons(resource_t res)
{
	private_resource_t* r=get_resource(res);
	return r->buttons;
}

/* send events to the server */

bool_t sendkey(resource_t res,keysym_t keysym,bool_t keydown)
{
	private_resource_t* r=get_resource(res);
	return SendKeyEvent(r->client,keysym,keydown);
}

bool_t sendmouse(resource_t res,coordinate_t x,coordinate_t y,buttons_t buttons)
{
	private_resource_t* r=get_resource(res);
	return SendPointerEvent(r->client,x,y,buttons);
}

/* for visual grepping */

coordinate_t getoriginx(resource_t res)
{
	private_resource_t* r=get_resource(res);
	return r->x_origin;
}

coordinate_t getoriginy(resource_t res)
{
	private_resource_t* r=get_resource(res);
	return r->y_origin;
}

