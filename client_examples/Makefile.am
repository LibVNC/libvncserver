DEFINES=-I.. -g -Wall
LDADD = ../libvncclient/libvncclient.a

if WITH_FFMPEG
FFMPEG_HOME=@with_ffmpeg@

vnc2mpg_CFLAGS=-I$(FFMPEG_HOME)/libavformat -I$(FFMPEG_HOME)/libavcodec
vnc2mpg_LDADD=$(LDADD) $(FFMPEG_HOME)/libavformat/libavformat.a $(FFMPEG_HOME)/libavcodec/libavcodec.a -lmp3lame -lm

FFMPEG_CLIENT=vnc2mpg
endif

if HAVE_LIBSDL
SDLVIEWER=SDLvncviewer

SDLvncviewer_CFLAGS=$(SDL_CFLAGS)

# thanks to autoconf, this looks ugly
SDLvncviewer_LDADD=$(LDADD) $(SDL_LIBS)
endif

noinst_PROGRAMS=ppmtest $(SDLVIEWER) $(FFMPEG_CLIENT)



