#include <SDL/SDL.h>
#include <rfb/rfbclient.h>

static rfbBool resize(rfbClient* client) {
	static char first=TRUE;
	int flags=SDL_HWSURFACE|SDL_ASYNCBLIT|SDL_HWACCEL;
	int width=client->width,height=client->height,
		depth=client->format.bitsPerPixel;
	rfbBool okay=SDL_VideoModeOK(width,height,depth,flags);
	if(!okay)
		for(depth=24;!okay && depth>4;depth/=2)
			okay=SDL_VideoModeOK(width,height,depth,flags);
	if(okay) {
		SDL_Surface* sdl=SDL_SetVideoMode(width,height,depth,flags);
		client->clientData=sdl;
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
		SDL_Surface* sdl=client->clientData;
		fprintf(stderr,"Could not set resolution %dx%d!\n",
				client->width,client->height);
		if(sdl) {
			client->width=sdl->w;
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

rfbKeySym SDL_keysym2rfbKeySym(int keysym) {
	switch(keysym) {
	case SDLK_BACKSPACE: return XK_BackSpace;
	case SDLK_TAB: return XK_ISO_Left_Tab;
	case SDLK_CLEAR: return XK_Clear;
	case SDLK_RETURN: return XK_Return;
	case SDLK_PAUSE: return XK_Pause;
	case SDLK_ESCAPE: return XK_Escape;
	case SDLK_SPACE: return XK_space;
	case SDLK_EXCLAIM: return XK_exclam;
	case SDLK_QUOTEDBL: return XK_quotedbl;
	case SDLK_HASH: return XK_numbersign;
	case SDLK_DOLLAR: return XK_dollar;
	case SDLK_AMPERSAND: return XK_ampersand;
	case SDLK_QUOTE: return XK_apostrophe;
	case SDLK_LEFTPAREN: return XK_parenleft;
	case SDLK_RIGHTPAREN: return XK_parenright;
	case SDLK_ASTERISK: return XK_asterisk;
	case SDLK_PLUS: return XK_plus;
	case SDLK_COMMA: return XK_comma;
	case SDLK_MINUS: return XK_minus;
	case SDLK_PERIOD: return XK_period;
	case SDLK_SLASH: return XK_slash;
	case SDLK_0: return XK_0;
	case SDLK_1: return XK_1;
	case SDLK_2: return XK_2;
	case SDLK_3: return XK_3;
	case SDLK_4: return XK_4;
	case SDLK_5: return XK_5;
	case SDLK_6: return XK_6;
	case SDLK_7: return XK_7;
	case SDLK_8: return XK_8;
	case SDLK_9: return XK_9;
	case SDLK_COLON: return XK_colon;
	case SDLK_SEMICOLON: return XK_semicolon;
	case SDLK_LESS: return XK_less;
	case SDLK_EQUALS: return XK_equal;
	case SDLK_GREATER: return XK_greater;
	case SDLK_QUESTION: return XK_question;
	case SDLK_AT: return XK_at;
	case SDLK_LEFTBRACKET: return XK_bracketleft;
	case SDLK_BACKSLASH: return XK_backslash;
	case SDLK_RIGHTBRACKET: return XK_bracketright;
	case SDLK_CARET: return XK_asciicircum;
	case SDLK_UNDERSCORE: return XK_underscore;
	case SDLK_BACKQUOTE: return XK_grave;
	case SDLK_a: return XK_a;
	case SDLK_b: return XK_b;
	case SDLK_c: return XK_c;
	case SDLK_d: return XK_d;
	case SDLK_e: return XK_e;
	case SDLK_f: return XK_f;
	case SDLK_g: return XK_g;
	case SDLK_h: return XK_h;
	case SDLK_i: return XK_i;
	case SDLK_j: return XK_j;
	case SDLK_k: return XK_k;
	case SDLK_l: return XK_l;
	case SDLK_m: return XK_m;
	case SDLK_n: return XK_n;
	case SDLK_o: return XK_o;
	case SDLK_p: return XK_p;
	case SDLK_q: return XK_q;
	case SDLK_r: return XK_r;
	case SDLK_s: return XK_s;
	case SDLK_t: return XK_t;
	case SDLK_u: return XK_u;
	case SDLK_v: return XK_v;
	case SDLK_w: return XK_w;
	case SDLK_x: return XK_x;
	case SDLK_y: return XK_y;
	case SDLK_z: return XK_z;
	case SDLK_DELETE: return XK_Delete;
	case SDLK_KP0: return XK_KP_0;
	case SDLK_KP1: return XK_KP_1;
	case SDLK_KP2: return XK_KP_2;
	case SDLK_KP3: return XK_KP_3;
	case SDLK_KP4: return XK_KP_4;
	case SDLK_KP5: return XK_KP_5;
	case SDLK_KP6: return XK_KP_6;
	case SDLK_KP7: return XK_KP_7;
	case SDLK_KP8: return XK_KP_8;
	case SDLK_KP9: return XK_KP_9;
	case SDLK_KP_PERIOD: return XK_KP_Decimal;
	case SDLK_KP_DIVIDE: return XK_KP_Divide;
	case SDLK_KP_MULTIPLY: return XK_KP_Multiply;
	case SDLK_KP_MINUS: return XK_KP_Subtract;
	case SDLK_KP_PLUS: return XK_KP_Add;
	case SDLK_KP_ENTER: return XK_KP_Enter;
	case SDLK_KP_EQUALS: return XK_KP_Equal;
	case SDLK_UP: return XK_Up;
	case SDLK_DOWN: return XK_Down;
	case SDLK_RIGHT: return XK_Right;
	case SDLK_LEFT: return XK_Left;
	case SDLK_INSERT: return XK_Insert;
	case SDLK_HOME: return XK_Home;
	case SDLK_END: return XK_End;
	case SDLK_PAGEUP: return XK_Page_Up;
	case SDLK_PAGEDOWN: return XK_Page_Down;
	case SDLK_F1: return XK_F1;
	case SDLK_F2: return XK_F2;
	case SDLK_F3: return XK_F3;
	case SDLK_F4: return XK_F4;
	case SDLK_F5: return XK_F5;
	case SDLK_F6: return XK_F6;
	case SDLK_F7: return XK_F7;
	case SDLK_F8: return XK_F8;
	case SDLK_F9: return XK_F9;
	case SDLK_F10: return XK_F10;
	case SDLK_F11: return XK_F11;
	case SDLK_F12: return XK_F12;
	case SDLK_F13: return XK_F13;
	case SDLK_F14: return XK_F14;
	case SDLK_F15: return XK_F15;
	case SDLK_NUMLOCK: return XK_Num_Lock;
	case SDLK_CAPSLOCK: return XK_Caps_Lock;
	case SDLK_SCROLLOCK: return XK_Scroll_Lock;
	case SDLK_RSHIFT: return XK_Shift_R;
	case SDLK_LSHIFT: return XK_Shift_L;
	case SDLK_RCTRL: return XK_Control_R;
	case SDLK_LCTRL: return XK_Control_L;
	case SDLK_RALT: return XK_Alt_R;
	case SDLK_LALT: return XK_Alt_L;
	case SDLK_RMETA: return XK_Meta_R;
	case SDLK_LMETA: return XK_Meta_L;
	//case SDLK_LSUPER: return XK_LSuper;		/* left "windows" key */
	//case SDLK_RSUPER: return XK_RSuper;		/* right "windows" key */
	case SDLK_MODE: return XK_Mode_switch;
	//case SDLK_COMPOSE: return XK_Compose;
	case SDLK_HELP: return XK_Help;
	case SDLK_PRINT: return XK_Print;
	case SDLK_SYSREQ: return XK_Sys_Req;
	case SDLK_BREAK: return XK_Break;
	default: fprintf(stderr,"Unknown keysym: %d\n",keysym);
	}
}

#define main main1
#include "ppmtest.c"
#undef main

void update(rfbClient* cl,int x,int y,int w,int h) {
	SDL_UpdateRect(cl->clientData, x, y, w, h);
	SaveFramebufferAsPPM(cl,x,y,w,h);
}

int main(int argc,char** argv) {
	rfbClient* cl;
	int i,buttonMask=0;
	SDL_Event e;

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);

	cl=rfbGetClient(5,3,2);
	//cl=rfbGetClient(8,3,4);
	cl->MallocFrameBuffer=resize;
	cl->GotFrameBufferUpdate=update;
	if(!rfbInitClient(cl,&argc,argv))
		return 1;

	while(1) {
		if(SDL_PollEvent(&e))
			switch(e.type) {
				case SDL_VIDEOEXPOSE:
					SendFramebufferUpdateRequest(cl,0,0,cl->width,cl->height,FALSE);
					break;
				case SDL_MOUSEBUTTONUP: case SDL_MOUSEBUTTONDOWN:
				case SDL_MOUSEMOTION: {
						int x,y;
						int state=SDL_GetMouseState(&x,&y);
						struct { int sdl; int rfb; } buttonMapping[]={
							{SDL_BUTTON_LEFT, rfbButton1Mask},
							{SDL_BUTTON_RIGHT, rfbButton2Mask},
							{SDL_BUTTON_MIDDLE, rfbButton3Mask},
							{0,0}
						};
						int i;
						for(buttonMask=0,i=0;buttonMapping[i].sdl;i++)
							if(state&SDL_BUTTON(buttonMapping[i].sdl))
								buttonMask|=buttonMapping[i].rfb;
						SendPointerEvent(cl,x,y,buttonMask);
					}
					break;
				case SDL_KEYUP: case SDL_KEYDOWN:
					SendKeyEvent(cl,SDL_keysym2rfbKeySym(e.key.keysym.sym),(e.type==SDL_KEYDOWN)?TRUE:FALSE);
					break;
				case SDL_QUIT:
					rfbClientCleanup(cl);
					return 0;
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

