DEFINES=-I.. -g -Wall
LDADD = ../libvncclient/libvncclient.a

if HAVE_LIBSDL
SDLVIEWER=SDLvncviewer

SDLvncviewer_CFLAGS=`sdl-config --cflags`

SDLvncviewer_LDFLAGS=`sdl-config --libs`
endif

noinst_PROGRAMS=ppmtest $(SDLVIEWER)

