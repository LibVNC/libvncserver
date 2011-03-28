#include <assert.h>
#include <string.h>
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

	char* text_client;
	char* text_server;

	image_t* grep_image;
	int x_origin,y_origin;

	enum { SLEEP,VISUALGREP,WAITFORUPDATE } state;
	result_t result;
} private_resource_t;

/* resource management */

#define MAX_RESOURCE_COUNT 20

static private_resource_t resource_pool[MAX_RESOURCE_COUNT];
static int resource_count=0;

static private_resource_t* get_resource(int resource)
{
	if(resource>=MAX_RESOURCE_COUNT || resource<0 || resource_pool[resource].client==0)
		return NULL;
	return resource_pool+resource;
}

static private_resource_t* get_next_resource(void)
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
	return NULL;
}

static void free_resource(int resource)
{
	private_resource_t* res=get_resource(resource);
	if(res)
		res->client=NULL;
}

/* hooks */

static void got_key(rfbBool down,rfbKeySym keysym,rfbClientRec* cl)
{
	private_resource_t* res=(private_resource_t*)cl->screen->screenData;
	
	res->keydown=down;
	res->keysym=keysym;
	res->result|=RESULT_KEY;
}

static void got_mouse(int buttons,int x,int y,rfbClientRec* cl)
{
	private_resource_t* res=(private_resource_t*)cl->screen->screenData;
	
	res->buttons=buttons;
	res->x=x;
	res->y=y;
	res->result|=RESULT_MOUSE;
}

static void got_text(char* str,int len,rfbClientRec* cl)
{
	private_resource_t* res=(private_resource_t*)cl->screen->screenData;

	if (res->text_client)
		free(res->text_client);
	res->text_client=strdup(str);
	res->result|=RESULT_TEXT_CLIENT;
}

static void got_text_from_server(rfbClient* cl, const char *str, int textlen)
{
	private_resource_t* res=(private_resource_t*)cl->clientData;

	if (res->text_server)
		free(res->text_server);
	res->text_server=strdup(str);
	res->result|=RESULT_TEXT_SERVER;
}

static rfbBool malloc_frame_buffer(rfbClient* cl)
{
	private_resource_t* res=(private_resource_t*)cl->clientData;

	if(!res->server) {
		int w=cl->width,h=cl->height;
		
		res->client->frameBuffer=malloc(w*4*h);
		
		res->server=rfbGetScreen(NULL,NULL,w,h,8,3,4);
		if(!res->server)
                  return FALSE;
		res->server->screenData=res;
		res->server->port=res->listen_port;
		res->server->frameBuffer=res->client->frameBuffer;
		res->server->kbdAddEvent=got_key;
		res->server->ptrAddEvent=got_mouse;
		res->server->setXCutText=got_text;
		rfbInitServer(res->server);
	} else {
		/* TODO: realloc if necessary */
		/* TODO: resolution change: send NewFBSize */
		/* TODO: if the origin is out of bounds, reset to 0 */
	}
}

static bool_t do_visual_grep(private_resource_t* res,int x,int y,int w,int h)
{
	rfbClient* cl;
	image_t* image;
	int x_start,y_start,x_end=x+w-1,y_end=y+h-1;
	bool_t found=0;

	if(res==0 || (cl=res->client)==0 || (image=res->grep_image)==0)
		return 0;

	x_start=x-image->width;
	y_start=y-image->height;
	if(x_start<0) x_start=0;
	if(y_start<0) y_start=0;
	if(x_end+image->width>cl->width) x_end=cl->width-image->width;
	if(y_end+image->height>cl->height) y_end=cl->height-image->height;
	
	/* find image and set x_origin,y_origin if found */
	for(y=y_start;y<y_end;y++)
		for(x=x_start;x<x_end;x++) {
			bool_t matching=1;
			int i,j;
			for(j=0;matching && j<image->height;j++)
				for(i=0;matching && i<image->width;i++)
					if(memcmp(cl->frameBuffer+4*(x+i+cl->width*(y+j)),image->buffer+4*(i+image->width*j),3))
						matching=0;
			if(matching) {
				private_resource_t* res=(private_resource_t*)cl->clientData;
				res->x_origin=x;
				res->y_origin=y;
				return -1;
			}
		}
	return 0;
}

static void got_frame_buffer(rfbClient* cl,int x,int y,int w,int h)
{
	private_resource_t* res=(private_resource_t*)cl->clientData;
	
	assert(res->server);
	
	if(res->grep_image && do_visual_grep(res,x,y,w,h)) {
		res->result|=RESULT_FOUNDIMAGE;
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

	res->text_client = NULL;
	res->text_server = NULL;

	res->client=rfbGetClient(8,3,4);
	res->client->clientData=(void*)res;
	res->client->GotFrameBufferUpdate=got_frame_buffer;
	res->client->MallocFrameBuffer=malloc_frame_buffer;
	res->client->GotXCutText=got_text_from_server;
	res->client->serverHost=strdup(server);
	res->client->serverPort=server_port;
	res->client->appData.encodingsString="raw";
	if(!rfbInitClient(res->client,&dummy,NULL)) {
		res->client=NULL;
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

	res->client=NULL;
}

/* PNM (image) helpers */

bool_t savepnm(resource_t resource,const char* filename,int x1,int y1,int x2,int y2)
{
	private_resource_t* res=get_resource(resource);
	int i,j,w,h;
	uint32_t* buffer;
	FILE* f;
	
	if(res==0 || res->client==0)
		return 0;
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

static image_t* loadpnm(const char* filename)
{
	FILE* f=fopen(filename,"rb");
	char buffer[1024];
	int i,j,w,h;
	image_t* image;
	
	if(f==0)
		return NULL;
	
	if(!fgets(buffer,1024,f) || strcmp("P6\n",buffer)) {
		fclose(f);
		return NULL;
	}

	do {
		fgets(buffer,1024,f);
		if(feof(f)) {
			fclose(f);
			return NULL;
		}
	} while(buffer[0]=='#');

	if( sscanf(buffer,"%d %d",&w,&h)!=2
			|| !fgets(buffer,1024,f) || strcmp("255\n",buffer)) {
		fclose(f);
		return NULL;
	}
		
	image=(image_t*)malloc(sizeof(image_t));
	image->width=w;
	image->height=h;
	image->buffer=malloc(w*4*h);
	if(!image->buffer) {
		fclose(f);
		free(image);
		return NULL;
	}
	
	for(j=0;j<h;j++)
		for(i=0;i<w;i++)
			if(fread(image->buffer+4*(i+w*j),3,1,f)!=1) {
				fprintf(stderr,"Could not read 3 bytes at %d,%d\n",i,j);
				fclose(f);
				free(image->buffer);
				free(image);
				return NULL;
			}

	fclose(f);
	
	return image;
}

static void free_image(image_t* image)
{
	if(image->buffer)
		free(image->buffer);
	free(image);
}

static void copy_line(rfbScreenInfo *dest, char *backup,
		int x0, int y0, int x1, int y1, int color_offset)
{
	uint8_t *d = (uint8_t *)dest->frameBuffer, *s = (uint8_t *)backup;
	int i;
	int steps0 = x1 > x0 ? x1 - x0 : x0 - x1;
	int steps1 = y1 > y0 ? y1 - y0 : y0 - y1;

	if (steps1 > steps0)
		steps0 = steps1;
	else if (steps0 == 0)
		steps0 = 1;

	for (i = 0; i <= steps0; i++) {
		int j, index = 4 * (x0 + i * (x1 - x0) / steps0
				+ dest->width * (y0 + i * (y1 - y0) / steps0));
		for (j = 0; j < 4; j++)
			d[index + j] = s[index + j] + color_offset;
	}

	rfbMarkRectAsModified(dest, x0 - 5, y0 - 5, x1 + 1, y1 + 2);
}

result_t displaypnm(resource_t resource, const char *filename,
		coordinate_t x, coordinate_t y, bool_t border,
		timeout_t timeout_in_seconds)
{
	private_resource_t* res = get_resource(resource);
	image_t *image;
	char* fake_frame_buffer;
	char* backup;
	int w, h, i, j, w2, h2;
	result_t result;

	if (res == NULL || res->server == NULL ||
			(image = loadpnm(filename)) == NULL)
		return 0;
	
	w = res->server->width;
	h = res->server->height;
	fake_frame_buffer = malloc(w * 4 * h);
	if(!fake_frame_buffer)
		return 0;
	memcpy(fake_frame_buffer, res->server->frameBuffer, w * 4 * h);
	
	backup = res->server->frameBuffer;
	res->server->frameBuffer = fake_frame_buffer;

	w2 = image->width;
	if (x + w2 > w)
		w2 = w - x;
	h2 = image->height;
	if (y + h2 > h)
		h2 = h - y;
	for (j = 0; j < h2; j++)
		memcpy(fake_frame_buffer + 4 * (x + (y + j) * w),
			image->buffer + j * 4 * image->width, 4 * w2);
	free(image);
	if (border) {
		copy_line(res->server, backup, x, y, x + w2, y, 0x80);
		copy_line(res->server, backup, x, y, x, y + h2, 0x80);
		copy_line(res->server, backup, x + w2, y, x + w2, y + h2, 0x80);
		copy_line(res->server, backup, x, y + h2, x + w2, y + h2, 0x80);
	}
	rfbMarkRectAsModified(res->server,
			x - 1, y - 1, x + w2 + 1, y + h2 + 1);

	result = waitforinput(resource, timeout_in_seconds);

	res->server->frameBuffer=backup;
	free(fake_frame_buffer);
	rfbMarkRectAsModified(res->server,
			x - 1, y - 1, x + w2 + 1, y + h2 + 1);

	return result;
}

/* process() and friends */

/* this function returns only if res->result in return_mask */
static result_t private_process(resource_t resource,timeout_t timeout_in_seconds,result_t return_mask)
{
	private_resource_t* res=get_resource(resource);
	fd_set fds;
	struct timeval tv,tv_start,tv_end;
	unsigned long timeout=(unsigned long)(timeout_in_seconds*1000000UL);
	int count,max_fd;

	if(res==0)
		return 0;

	assert(res->client);

	gettimeofday(&tv_start,NULL);
	res->result=0;

	do {
		unsigned long timeout_done;

		if(res->server) {
			rfbBool loop;
			do {
				loop=rfbProcessEvents(res->server,res->server->deferUpdateTime);
			} while(loop && (res->result&return_mask)==0
				&& rfbIsActive(res->server));

			if(!rfbIsActive(res->server))
				return RESULT_SHUTDOWN;

			if((res->result&return_mask)!=0)
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

		gettimeofday(&tv_end,NULL);
		timeout_done=tv_end.tv_usec-tv_start.tv_usec+
			1000000L*(tv_end.tv_sec-tv_start.tv_sec);
		if(timeout_done>=timeout)
			return RESULT_TIMEOUT;

		tv.tv_usec=((timeout-timeout_done)%1000000);
		tv.tv_sec=(timeout-timeout_done)/1000000;

		count=select(max_fd+1,&fds,NULL,NULL,&tv);
		if(count<0)
			return 0;

		if(count>0) {
			if(FD_ISSET(res->client->sock,&fds)) {
				if(!HandleRFBServerMessage(res->client)) {
					closevnc(resource);
					return 0;
				}
				if((res->result&return_mask)!=0)
					return res->result;
			}
		} else {
			res->result|=RESULT_TIMEOUT;
			return res->result;
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

result_t visualgrep(resource_t resource,const char* filename,timeout_t timeout)
{
	private_resource_t* res=get_resource(resource);
	image_t* image;
	result_t result;

	if(res==0 || res->client==0)
		return 0;

	/* load filename and set res->grep_image to this image */
	image=loadpnm(filename);
	if(image==0)
		return 0;
	if(res->grep_image)
		free_image(res->grep_image);
	res->grep_image=image;

	if(do_visual_grep(res,0,0,res->client->width,res->client->height))
		return RESULT_FOUNDIMAGE;

	result=private_process(resource,timeout,RESULT_FOUNDIMAGE|RESULT_TIMEOUT);

	/* free image */
	if(res->grep_image) {
		free_image(res->grep_image);
		res->grep_image=NULL;
	}

	return result;
}

/* auxiliary function for alert */

#include "default8x16.h"

static void center_text(rfbScreenInfo* screen,const char* message,int* x,int* y,int* w,int* h)
{
	rfbFontData* font=&default8x16Font;
	const char* pointer;
	int j,x1,y1,x2,y2,line_count=0;
	if(message==0 || screen==0)
		return;
	rfbWholeFontBBox(font,&x1,&y1,&x2,&y2);
	for(line_count=1,pointer=message;*pointer;pointer++)
		if(*pointer=='\n')
			line_count++;
	
	*h=(y2-y1)*line_count;
	assert(*h>0);

	if(*h>screen->height)
		*h=screen->height;

	*x=0; *w=screen->width; *y=(screen->height-*h)/2;

	rfbFillRect(screen,*x,*y,*x+*w,*y+*h,0xff0000);

	for(pointer=message,j=0;j<line_count;j++) {
		const char* eol;
		int x_cur,y_cur=*y-y1+j*(y2-y1),width;
		
		for(width=0,eol=pointer;*eol && *eol!='\n';eol++)
			width+=rfbWidthOfChar(font,*eol);
		if(width>screen->width)
			width=screen->width;

		x_cur=(screen->width-width)/2;
		for(;pointer!=eol;pointer++)
			x_cur+=rfbDrawCharWithClip(screen,font,
					x_cur,y_cur,*pointer,
					0,0,screen->width,screen->height,
					0xffffffff,0xffffffff);
		pointer++;
	}
	rfbMarkRectAsModified(screen,*x,*y,*x+*w,*y+*h);
}

/* this is an overlay which is shown for a certain time */

result_t alert(resource_t resource,const char* message,timeout_t timeout)
{
	private_resource_t* res=get_resource(resource);
	char* fake_frame_buffer;
	char* backup;
	int x,y,w,h;
	result_t result;
	
	if(res == NULL || res->server==NULL)
		return -1;

	w=res->server->width;
	h=res->server->height;
	
	fake_frame_buffer=malloc(w*4*h);
	if(!fake_frame_buffer)
		return -1;
	memcpy(fake_frame_buffer,res->server->frameBuffer,w*4*h);
	
	backup=res->server->frameBuffer;
	res->server->frameBuffer=fake_frame_buffer;
	center_text(res->server,message,&x,&y,&w,&h);
	fprintf(stderr,"%s\n",message);

	result=waitforinput(resource,timeout);

	res->server->frameBuffer=backup;
	free(fake_frame_buffer);
	rfbMarkRectAsModified(res->server,x,y,x+w,y+h);

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

const char *gettext_client(resource_t res)
{
	private_resource_t* r=get_resource(res);
	return r->text_client;
}

result_t rubberband(resource_t resource, coordinate_t x0, coordinate_t y0)
{
	private_resource_t* res=get_resource(resource);
	char* fake_frame_buffer;
	char* backup;
	int w, h, x, y;
	
	if(res == NULL || res->server==NULL)
		return -1;

	x = res->x;
	y = res->y;
	w = res->server->width;
	h = res->server->height;
	fake_frame_buffer = malloc(w * 4 * h);
	if(!fake_frame_buffer)
		return 0;
	memcpy(fake_frame_buffer, res->server->frameBuffer, w * 4 * h);
	
	backup = res->server->frameBuffer;
	res->server->frameBuffer = fake_frame_buffer;

	while (res->buttons) {
		result_t r = waitforinput(resource, 1000000L);
		if (x == res->x && y == res->y)
				continue;
		copy_line(res->server, backup, x0, y0, x, y0, 0);
		copy_line(res->server, backup, x0, y0, x0, y, 0);
		copy_line(res->server, backup, x, y0, x, y, 0);
		copy_line(res->server, backup, x0, y, x, y, 0);
		x = res->x;
		y = res->y;
		copy_line(res->server, backup, x0, y0, x, y0, 0x80);
		copy_line(res->server, backup, x0, y0, x0, y, 0x80);
		copy_line(res->server, backup, x, y0, x, y, 0x80);
		copy_line(res->server, backup, x0, y, x, y, 0x80);
	}

	copy_line(res->server, backup, x0, y0, x, y0, 0);
	copy_line(res->server, backup, x0, y0, x0, y, 0);
	copy_line(res->server, backup, x, y0, x, y, 0);
	copy_line(res->server, backup, x0, y, x, y, 0);

	res->server->frameBuffer=backup;
	free(fake_frame_buffer);

	return RESULT_MOUSE;
}

const char *gettext_server(resource_t res)
{
	private_resource_t* r=get_resource(res);
	return r->text_server;
}

/* send events to the server */

bool_t sendkey(resource_t res,keysym_t keysym,bool_t keydown)
{
	private_resource_t* r=get_resource(res);
	if(r==NULL)
		return 0;
	return SendKeyEvent(r->client,keysym,keydown);
}

bool_t sendascii(resource_t res,const char *string)
{
	timeout_t delay = 0.1;
	private_resource_t* r=get_resource(res);
	int i;
	if(r==NULL)
		return 0;
	while (*string) {
		int keysym = *string;
		int need_shift = 0;

		if (keysym >= 8 && keysym < ' ')
			keysym += 0xff00;
		else if (keysym >= 'A' && keysym <= 'Z')
			need_shift = 1;
		else if (keysym > '~') {
			fprintf(stderr, "String contains non-ASCII "
					"character 0x%02x\n", *string);
			return FALSE;
		}

		if (need_shift) {
			if (!SendKeyEvent(r->client,0xffe1,1))
				return FALSE;
			waitforinput(r,delay);
		}
		for (i = 1; i >= 0; i--) {
			if (!SendKeyEvent(r->client,keysym,i))
				return FALSE;
			waitforinput(r,delay);
		}
		if (need_shift) {
			if (!SendKeyEvent(r->client,0xffe1,0))
				return FALSE;
			waitforinput(r,delay);
		}
		string++;
	}
	return TRUE;
}

bool_t sendmouse(resource_t res,coordinate_t x,coordinate_t y,buttons_t buttons)
{
	private_resource_t* r=get_resource(res);
	if(r==NULL)
		return 0;
	return SendPointerEvent(r->client,x,y,buttons);
}

bool_t sendtext(resource_t res, const char *string)
{
	private_resource_t* r=get_resource(res);
	if(r==NULL)
		return 0;
	return SendClientCutText(r->client, (char *)string, (int)strlen(string));
}

bool_t sendtext_to_server(resource_t res, const char *string)
{
	private_resource_t* r=get_resource(res);
	if(r==NULL)
		return 0;
	rfbSendServerCutText(r->server, (char *)string, (int)strlen(string));
	return 1;
}

/* for visual grepping */

coordinate_t getxorigin(resource_t res)
{
	private_resource_t* r=get_resource(res);
	return r->x_origin;
}

coordinate_t getyorigin(resource_t res)
{
	private_resource_t* r=get_resource(res);
	return r->y_origin;
}

