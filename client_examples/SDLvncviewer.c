#include <SDL.h>
#include <rfb/rfbclient.h>

struct { int sdl; int rfb; } buttonMapping[]={
	{1, rfbButton1Mask},
	{2, rfbButton2Mask},
	{3, rfbButton3Mask},
	{0,0}
};

static rfbBool resize(rfbClient* client) {
	static char first=TRUE;
#ifdef SDL_ASYNCBLIT
	int flags=SDL_HWSURFACE|SDL_ASYNCBLIT|SDL_HWACCEL;
#else
	int flags=SDL_HWSURFACE|SDL_HWACCEL;
#endif
	int width=client->width,height=client->height,
		depth=client->format.bitsPerPixel;
	client->updateRect.x = client->updateRect.y = 0;
	client->updateRect.w = width; client->updateRect.h = height;
	rfbBool okay=SDL_VideoModeOK(width,height,depth,flags);
	if(!okay)
		for(depth=24;!okay && depth>4;depth/=2)
			okay=SDL_VideoModeOK(width,height,depth,flags);
	if(okay) {
		SDL_Surface* sdl=SDL_SetVideoMode(width,height,depth,flags);
		rfbClientSetClientData(client, SDL_Init, sdl);
		client->width = sdl->pitch / (depth / 8);
		client->frameBuffer=sdl->pixels;
		if(first || depth!=client->format.bitsPerPixel) {
			first=FALSE;
			client->format.bitsPerPixel=depth;
			client->format.redShift=sdl->format->Rshift;
			client->format.greenShift=sdl->format->Gshift;
			client->format.blueShift=sdl->format->Bshift;
			client->format.redMax=sdl->format->Rmask>>client->format.redShift;
			client->format.greenMax=sdl->format->Gmask>>client->format.greenShift;
			client->format.blueMax=sdl->format->Bmask>>client->format.blueShift;
			SetFormatAndEncodings(client);
		}
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
	switch(e->keysym.sym) {
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
#if 0
	/* TODO: find out keysyms */
	case SDLK_LSUPER: k = XK_LSuper; break;		/* left "windows" key */
	case SDLK_RSUPER: k = XK_RSuper; break;		/* right "windows" key */
	case SDLK_COMPOSE: k = XK_Compose; break;
#endif
	case SDLK_MODE: k = XK_Mode_switch; break;
	case SDLK_HELP: k = XK_Help; break;
	case SDLK_PRINT: k = XK_Print; break;
	case SDLK_SYSREQ: k = XK_Sys_Req; break;
	case SDLK_BREAK: k = XK_Break; break;
	default: break;
	}
	if (k == 0 && e->keysym.sym >= SDLK_a && e->keysym.sym <= SDLK_z) {
		k = XK_a + e->keysym.sym - SDLK_a;
		if (e->keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT))
			k &= ~0x20;
	}
	if (k == 0) {
		if (e->keysym.unicode < 0x100)
			k = e->keysym.unicode;
		else
			rfbClientLog("Unknown keysym: %d\n",e->keysym.sym);
	}

	return k;
}

static void update(rfbClient* cl,int x,int y,int w,int h) {
	SDL_UpdateRect(rfbClientGetClientData(cl, SDL_Init), x, y, w, h);
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

#ifdef mac
#define main SDLmain
#endif

int main(int argc,char** argv) {
	rfbClient* cl;
	int i, j, buttonMask = 0, viewOnly = 0;
	SDL_Event e;

#ifdef LOG_TO_FILE
	rfbClientLog=rfbClientErr=log_to_file;
#endif

	for (i = 1, j = 1; i < argc; i++)
		if (!strcmp(argv[1], "-viewonly"))
			viewOnly = 1;
		else {
			if (i != j)
				argv[j] = argv[i];
			j++;
		}
	argc = j;

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);
	SDL_EnableUNICODE(1);

	/* 16-bit: cl=rfbGetClient(5,3,2); */
	cl=rfbGetClient(8,3,4);
	cl->MallocFrameBuffer=resize;
	cl->canHandleNewFBSize = TRUE;
	cl->GotFrameBufferUpdate=update;
	cl->HandleKeyboardLedState=kbd_leds;
	cl->HandleTextChat=text_chat;
	if(!rfbInitClient(cl,&argc,argv))
		return 1;

	while(1) {
		if(SDL_PollEvent(&e))
			switch(e.type) {
#if SDL_MAJOR_VERSION>1 || SDL_MINOR_VERSION>=2
				case SDL_VIDEOEXPOSE:
					SendFramebufferUpdateRequest(cl,0,0,cl->width,cl->height,FALSE);
					break;
#endif
				case SDL_MOUSEBUTTONUP: case SDL_MOUSEBUTTONDOWN:
				case SDL_MOUSEMOTION: {
						int x,y;
						if (viewOnly)
							break;
						int state=SDL_GetMouseState(&x,&y);
						int i;
						for(buttonMask=0,i=0;buttonMapping[i].sdl;i++)
							if(state&SDL_BUTTON(buttonMapping[i].sdl))
								buttonMask|=buttonMapping[i].rfb;
						SendPointerEvent(cl,x,y,buttonMask);
					}
					break;
				case SDL_KEYUP: case SDL_KEYDOWN:
					if (viewOnly)
						break;
					SendKeyEvent(cl,SDL_key2rfbKeySym(&e.key),(e.type==SDL_KEYDOWN)?TRUE:FALSE);
					break;
				case SDL_QUIT:
					rfbClientCleanup(cl);
					return 0;
				case SDL_ACTIVEEVENT:
					break;
				default:
					rfbClientLog("ignore SDL event: 0x%x\n",e.type);
			}
		else {
			i=WaitForMessage(cl,500);
			if(i<0)
				return 0;
			if(i)
		    		if(!HandleRFBServerMessage(cl))
					return 0;
		}
	}

	return 0;
}

