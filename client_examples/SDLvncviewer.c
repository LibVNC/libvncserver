/**
 * @example SDLvncviewer.c
 */

#include <SDL.h>
#include <signal.h>
#include <rfb/rfbclient.h>
#include "scrap.h"

struct { int sdl; int rfb; } buttonMapping[]={
	{1, rfbButton1Mask},
	{2, rfbButton2Mask},
	{3, rfbButton3Mask},
	{4, rfbButton4Mask},
	{5, rfbButton5Mask},
	{0,0}
};

static int enableResizable = 1, viewOnly, listenLoop, buttonMask;
#ifdef SDL_ASYNCBLIT
	int sdlFlags = SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL;
#else
	int sdlFlags = SDL_HWSURFACE | SDL_HWACCEL;
#endif
static int realWidth, realHeight, bytesPerPixel, rowStride;
static char *sdlPixels;

static int rightAltKeyDown, leftAltKeyDown;

static rfbBool resize(rfbClient* client) {
	int width=client->width,height=client->height,
		depth=client->format.bitsPerPixel;

	if (enableResizable)
		sdlFlags |= SDL_RESIZABLE;

	client->updateRect.x = client->updateRect.y = 0;
	client->updateRect.w = width; client->updateRect.h = height;
	rfbBool okay=SDL_VideoModeOK(width,height,depth,sdlFlags);
	if(!okay)
		for(depth=24;!okay && depth>4;depth/=2)
			okay=SDL_VideoModeOK(width,height,depth,sdlFlags);
	if(okay) {
		SDL_Surface* sdl=SDL_SetVideoMode(width,height,depth,sdlFlags);
		rfbClientSetClientData(client, SDL_Init, sdl);
		client->width = sdl->pitch / (depth / 8);
		if (sdlPixels) {
			free(client->frameBuffer);
			sdlPixels = NULL;
		}
		client->frameBuffer=sdl->pixels;

		client->format.bitsPerPixel=depth;
		client->format.redShift=sdl->format->Rshift;
		client->format.greenShift=sdl->format->Gshift;
		client->format.blueShift=sdl->format->Bshift;
		client->format.redMax=sdl->format->Rmask>>client->format.redShift;
		client->format.greenMax=sdl->format->Gmask>>client->format.greenShift;
		client->format.blueMax=sdl->format->Bmask>>client->format.blueShift;
		SetFormatAndEncodings(client);

	} else {
		SDL_Surface* sdl=rfbClientGetClientData(client, SDL_Init);
		rfbClientLog("Could not set resolution %dx%d!\n",
				client->width,client->height);
		if(sdl) {
			client->width=sdl->pitch / (depth / 8);
			client->height=sdl->h;
		} else {
			client->width=0;
			client->height=0;
		}
		return FALSE;
	}
	SDL_WM_SetCaption(client->desktopName, "SDL");
	return TRUE;
}

static rfbKeySym SDL_key2rfbKeySym(SDL_KeyboardEvent* e) {
	rfbKeySym k = 0;
	SDLKey sym = e->keysym.sym;

	switch (sym) {
	case SDLK_BACKSPACE: k = XK_BackSpace; break;
	case SDLK_TAB: k = XK_Tab; break;
	case SDLK_CLEAR: k = XK_Clear; break;
	case SDLK_RETURN: k = XK_Return; break;
	case SDLK_PAUSE: k = XK_Pause; break;
	case SDLK_ESCAPE: k = XK_Escape; break;
	case SDLK_SPACE: k = XK_space; break;
	case SDLK_DELETE: k = XK_Delete; break;
	case SDLK_KP0: k = XK_KP_0; break;
	case SDLK_KP1: k = XK_KP_1; break;
	case SDLK_KP2: k = XK_KP_2; break;
	case SDLK_KP3: k = XK_KP_3; break;
	case SDLK_KP4: k = XK_KP_4; break;
	case SDLK_KP5: k = XK_KP_5; break;
	case SDLK_KP6: k = XK_KP_6; break;
	case SDLK_KP7: k = XK_KP_7; break;
	case SDLK_KP8: k = XK_KP_8; break;
	case SDLK_KP9: k = XK_KP_9; break;
	case SDLK_KP_PERIOD: k = XK_KP_Decimal; break;
	case SDLK_KP_DIVIDE: k = XK_KP_Divide; break;
	case SDLK_KP_MULTIPLY: k = XK_KP_Multiply; break;
	case SDLK_KP_MINUS: k = XK_KP_Subtract; break;
	case SDLK_KP_PLUS: k = XK_KP_Add; break;
	case SDLK_KP_ENTER: k = XK_KP_Enter; break;
	case SDLK_KP_EQUALS: k = XK_KP_Equal; break;
	case SDLK_UP: k = XK_Up; break;
	case SDLK_DOWN: k = XK_Down; break;
	case SDLK_RIGHT: k = XK_Right; break;
	case SDLK_LEFT: k = XK_Left; break;
	case SDLK_INSERT: k = XK_Insert; break;
	case SDLK_HOME: k = XK_Home; break;
	case SDLK_END: k = XK_End; break;
	case SDLK_PAGEUP: k = XK_Page_Up; break;
	case SDLK_PAGEDOWN: k = XK_Page_Down; break;
	case SDLK_F1: k = XK_F1; break;
	case SDLK_F2: k = XK_F2; break;
	case SDLK_F3: k = XK_F3; break;
	case SDLK_F4: k = XK_F4; break;
	case SDLK_F5: k = XK_F5; break;
	case SDLK_F6: k = XK_F6; break;
	case SDLK_F7: k = XK_F7; break;
	case SDLK_F8: k = XK_F8; break;
	case SDLK_F9: k = XK_F9; break;
	case SDLK_F10: k = XK_F10; break;
	case SDLK_F11: k = XK_F11; break;
	case SDLK_F12: k = XK_F12; break;
	case SDLK_F13: k = XK_F13; break;
	case SDLK_F14: k = XK_F14; break;
	case SDLK_F15: k = XK_F15; break;
	case SDLK_NUMLOCK: k = XK_Num_Lock; break;
	case SDLK_CAPSLOCK: k = XK_Caps_Lock; break;
	case SDLK_SCROLLOCK: k = XK_Scroll_Lock; break;
	case SDLK_RSHIFT: k = XK_Shift_R; break;
	case SDLK_LSHIFT: k = XK_Shift_L; break;
	case SDLK_RCTRL: k = XK_Control_R; break;
	case SDLK_LCTRL: k = XK_Control_L; break;
	case SDLK_RALT: k = XK_Alt_R; break;
	case SDLK_LALT: k = XK_Alt_L; break;
	case SDLK_RMETA: k = XK_Meta_R; break;
	case SDLK_LMETA: k = XK_Meta_L; break;
	case SDLK_LSUPER: k = XK_Super_L; break;
	case SDLK_RSUPER: k = XK_Super_R; break;
#if 0
	case SDLK_COMPOSE: k = XK_Compose; break;
#endif
	case SDLK_MODE: k = XK_Mode_switch; break;
	case SDLK_HELP: k = XK_Help; break;
	case SDLK_PRINT: k = XK_Print; break;
	case SDLK_SYSREQ: k = XK_Sys_Req; break;
	case SDLK_BREAK: k = XK_Break; break;
	default: break;
	}
	/* both SDL and X11 keysyms match ASCII in the range 0x01-0x7f */
	if (k == 0 && sym > 0x0 && sym < 0x100) {
		k = sym;
		if (e->keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT)) {
			if (k >= '1' && k <= '9')
				k &= ~0x10;
			else if (k >= 'a' && k <= 'f')
				k &= ~0x20;
		}
	}
	if (k == 0) {
		if (e->keysym.unicode < 0x100)
			k = e->keysym.unicode;
		else
			rfbClientLog("Unknown keysym: %d\n", sym);
	}

	return k;
}

static uint32_t get(rfbClient *cl, int x, int y)
{
	switch (bytesPerPixel) {
	case 1: return ((uint8_t *)cl->frameBuffer)[x + y * cl->width];
	case 2: return ((uint16_t *)cl->frameBuffer)[x + y * cl->width];
	case 4: return ((uint32_t *)cl->frameBuffer)[x + y * cl->width];
	default:
		rfbClientErr("Unknown bytes/pixel: %d", bytesPerPixel);
		exit(1);
	}
}

static void put(int x, int y, uint32_t v)
{
	switch (bytesPerPixel) {
	case 1: ((uint8_t *)sdlPixels)[x + y * rowStride] = v; break;
	case 2: ((uint16_t *)sdlPixels)[x + y * rowStride] = v; break;
	case 4: ((uint32_t *)sdlPixels)[x + y * rowStride] = v; break;
	default:
		rfbClientErr("Unknown bytes/pixel: %d", bytesPerPixel);
		exit(1);
	}
}

static void resizeRectangleToReal(rfbClient *cl, int x, int y, int w, int h)
{
	int i0 = x * realWidth / cl->width;
	int i1 = ((x + w) * realWidth - 1) / cl->width + 1;
	int j0 = y * realHeight / cl->height;
	int j1 = ((y + h) * realHeight - 1) / cl->height + 1;
	int i, j;

	for (j = j0; j < j1; j++)
		for (i = i0; i < i1; i++) {
			int x0 = i * cl->width / realWidth;
			int x1 = ((i + 1) * cl->width - 1) / realWidth + 1;
			int y0 = j * cl->height / realHeight;
			int y1 = ((j + 1) * cl->height - 1) / realHeight + 1;
			uint32_t r = 0, g = 0, b = 0;

			for (y = y0; y < y1; y++)
				for (x = x0; x < x1; x++) {
					uint32_t v = get(cl, x, y);
#define REDSHIFT cl->format.redShift
#define REDMAX cl->format.redMax
#define GREENSHIFT cl->format.greenShift
#define GREENMAX cl->format.greenMax
#define BLUESHIFT cl->format.blueShift
#define BLUEMAX cl->format.blueMax
					r += (v >> REDSHIFT) & REDMAX;
					g += (v >> GREENSHIFT) & GREENMAX;
					b += (v >> BLUESHIFT) & BLUEMAX;
				}
			r /= (x1 - x0) * (y1 - y0);
			g /= (x1 - x0) * (y1 - y0);
			b /= (x1 - x0) * (y1 - y0);

			put(i, j, (r << REDSHIFT) | (g << GREENSHIFT) |
				(b << BLUESHIFT));
		}
}

static void update(rfbClient* cl,int x,int y,int w,int h) {
	if (sdlPixels) {
		resizeRectangleToReal(cl, x, y, w, h);
		w = ((x + w) * realWidth - 1) / cl->width + 1;
		h = ((y + h) * realHeight - 1) / cl->height + 1;
		x = x * realWidth / cl->width;
		y = y * realHeight / cl->height;
		w -= x;
		h -= y;
	}
	SDL_UpdateRect(rfbClientGetClientData(cl, SDL_Init), x, y, w, h);
}

static void setRealDimension(rfbClient *client, int w, int h)
{
	SDL_Surface* sdl;

	if (w < 0) {
		const SDL_VideoInfo *info = SDL_GetVideoInfo();
		w = info->current_h;
		h = info->current_w;
	}

	if (w == realWidth && h == realHeight)
		return;

	if (!sdlPixels) {
		int size;

		sdlPixels = (char *)client->frameBuffer;
		rowStride = client->width;

		bytesPerPixel = client->format.bitsPerPixel / 8;
		size = client->width * bytesPerPixel * client->height;
		client->frameBuffer = malloc(size);
		if (!client->frameBuffer) {
			rfbClientErr("Could not allocate %d bytes", size);
			exit(1);
		}
		memcpy(client->frameBuffer, sdlPixels, size);
	}

	sdl = rfbClientGetClientData(client, SDL_Init);
	if (sdl->w != w || sdl->h != h) {
		int depth = sdl->format->BitsPerPixel;
		sdl = SDL_SetVideoMode(w, h, depth, sdlFlags);
		rfbClientSetClientData(client, SDL_Init, sdl);
		sdlPixels = sdl->pixels;
		rowStride = sdl->pitch / (depth / 8);
	}

	realWidth = w;
	realHeight = h;
	update(client, 0, 0, client->width, client->height);
}

static void kbd_leds(rfbClient* cl, int value, int pad) {
	/* note: pad is for future expansion 0=unused */
	fprintf(stderr,"Led State= 0x%02X\n", value);
	fflush(stderr);
}

/* trivial support for textchat */
static void text_chat(rfbClient* cl, int value, char *text) {
	switch(value) {
	case rfbTextChatOpen:
		fprintf(stderr,"TextChat: We should open a textchat window!\n");
		TextChatOpen(cl);
		break;
	case rfbTextChatClose:
		fprintf(stderr,"TextChat: We should close our window!\n");
		break;
	case rfbTextChatFinished:
		fprintf(stderr,"TextChat: We should close our window!\n");
		break;
	default:
		fprintf(stderr,"TextChat: Received \"%s\"\n", text);
		break;
	}
	fflush(stderr);
}

#ifdef __MINGW32__
#define LOG_TO_FILE
#endif

#ifdef LOG_TO_FILE
#include <stdarg.h>
static void
log_to_file(const char *format, ...)
{
    FILE* logfile;
    static char* logfile_str=0;
    va_list args;
    char buf[256];
    time_t log_clock;

    if(!rfbEnableClientLogging)
      return;

    if(logfile_str==0) {
	logfile_str=getenv("VNCLOG");
	if(logfile_str==0)
	    logfile_str="vnc.log";
    }

    logfile=fopen(logfile_str,"a");

    va_start(args, format);

    time(&log_clock);
    strftime(buf, 255, "%d/%m/%Y %X ", localtime(&log_clock));
    fprintf(logfile,buf);

    vfprintf(logfile, format, args);
    fflush(logfile);

    va_end(args);
    fclose(logfile);
}
#endif


static void cleanup(rfbClient* cl)
{
  /*
    just in case we're running in listenLoop:
    close viewer window by restarting SDL video subsystem
  */
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
  SDL_InitSubSystem(SDL_INIT_VIDEO);
  if(cl)
    rfbClientCleanup(cl);
}


static rfbBool handleSDLEvent(rfbClient *cl, SDL_Event *e)
{
	switch(e->type) {
#if SDL_MAJOR_VERSION > 1 || SDL_MINOR_VERSION >= 2
	case SDL_VIDEOEXPOSE:
		SendFramebufferUpdateRequest(cl, 0, 0,
					cl->width, cl->height, FALSE);
		break;
#endif
	case SDL_MOUSEBUTTONUP:
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEMOTION:
	{
		int x, y, state, i;
		if (viewOnly)
			break;

		if (e->type == SDL_MOUSEMOTION) {
			x = e->motion.x;
			y = e->motion.y;
			state = e->motion.state;
		}
		else {
			x = e->button.x;
			y = e->button.y;
			state = e->button.button;
			for (i = 0; buttonMapping[i].sdl; i++)
				if (state == buttonMapping[i].sdl) {
					state = buttonMapping[i].rfb;
					if (e->type == SDL_MOUSEBUTTONDOWN)
						buttonMask |= state;
					else
						buttonMask &= ~state;
					break;
				}
		}
		if (sdlPixels) {
			x = x * cl->width / realWidth;
			y = y * cl->height / realHeight;
		}
		SendPointerEvent(cl, x, y, buttonMask);
		buttonMask &= ~(rfbButton4Mask | rfbButton5Mask);
		break;
	}
	case SDL_KEYUP:
	case SDL_KEYDOWN:
		if (viewOnly)
			break;
		SendKeyEvent(cl, SDL_key2rfbKeySym(&e->key),
			e->type == SDL_KEYDOWN ? TRUE : FALSE);
		if (e->key.keysym.sym == SDLK_RALT)
			rightAltKeyDown = e->type == SDL_KEYDOWN;
		if (e->key.keysym.sym == SDLK_LALT)
			leftAltKeyDown = e->type == SDL_KEYDOWN;
		break;
	case SDL_QUIT:
                if(listenLoop)
		  {
		    cleanup(cl);
		    return FALSE;
		  }
		else
		  {
		    rfbClientCleanup(cl);
		    exit(0);
		  }
	case SDL_ACTIVEEVENT:
		if (!e->active.gain && rightAltKeyDown) {
			SendKeyEvent(cl, XK_Alt_R, FALSE);
			rightAltKeyDown = FALSE;
			rfbClientLog("released right Alt key\n");
		}
		if (!e->active.gain && leftAltKeyDown) {
			SendKeyEvent(cl, XK_Alt_L, FALSE);
			leftAltKeyDown = FALSE;
			rfbClientLog("released left Alt key\n");
		}

		if (e->active.gain && lost_scrap()) {
			static char *data = NULL;
			static int len = 0;
			get_scrap(T('T', 'E', 'X', 'T'), &len, &data);
			if (len)
				SendClientCutText(cl, data, len);
		}
		break;
	case SDL_SYSWMEVENT:
		clipboard_filter(e);
		break;
	case SDL_VIDEORESIZE:
		setRealDimension(cl, e->resize.w, e->resize.h);
		break;
	default:
		rfbClientLog("ignore SDL event: 0x%x\n", e->type);
	}
	return TRUE;
}

static void got_selection(rfbClient *cl, const char *text, int len)
{
	put_scrap(T('T', 'E', 'X', 'T'), len, text);
}


#ifdef mac
#define main SDLmain
#endif

int main(int argc,char** argv) {
	rfbClient* cl;
	int i, j;
	SDL_Event e;

#ifdef LOG_TO_FILE
	rfbClientLog=rfbClientErr=log_to_file;
#endif

	for (i = 1, j = 1; i < argc; i++)
		if (!strcmp(argv[i], "-viewonly"))
			viewOnly = 1;
		else if (!strcmp(argv[i], "-resizable"))
			enableResizable = 1;
		else if (!strcmp(argv[i], "-no-resizable"))
			enableResizable = 0;
		else if (!strcmp(argv[i], "-listen")) {
		        listenLoop = 1;
			argv[i] = "-listennofork";
                        ++j;
		}
		else {
			if (i != j)
				argv[j] = argv[i];
			j++;
		}
	argc = j;

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);
	SDL_EnableUNICODE(1);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,
			SDL_DEFAULT_REPEAT_INTERVAL);
	atexit(SDL_Quit);
	signal(SIGINT, exit);

	do {
	  /* 16-bit: cl=rfbGetClient(5,3,2); */
	  cl=rfbGetClient(8,3,4);
	  cl->MallocFrameBuffer=resize;
	  cl->canHandleNewFBSize = TRUE;
	  cl->GotFrameBufferUpdate=update;
	  cl->HandleKeyboardLedState=kbd_leds;
	  cl->HandleTextChat=text_chat;
	  cl->GotXCutText = got_selection;
	  cl->listenPort = LISTEN_PORT_OFFSET;
	  cl->listen6Port = LISTEN_PORT_OFFSET;
	  if(!rfbInitClient(cl,&argc,argv))
	    {
	      cl = NULL; /* rfbInitClient has already freed the client struct */
	      cleanup(cl);
	      break;
	    }

	  init_scrap();

	  while(1) {
	    if(SDL_PollEvent(&e)) {
	      /*
		handleSDLEvent() return 0 if user requested window close.
		In this case, handleSDLEvent() will have called cleanup().
	      */
	      if(!handleSDLEvent(cl, &e))
		break;
	    }
	    else {
	      i=WaitForMessage(cl,500);
	      if(i<0)
		{
		  cleanup(cl);
		  break;
		}
	      if(i)
		if(!HandleRFBServerMessage(cl))
		  {
		    cleanup(cl);
		    break;
		  }
	    }
	  }
	}
	while(listenLoop);

	return 0;
}

