AM_CFLAGS=-I $(top_srcdir) -g -Wall
LDADD = ../libvncclient/libvncclient.la @WSOCKLIB@

if WITH_FFMPEG
FFMPEG_HOME=@with_ffmpeg@

if HAVE_MP3LAME
MP3LAME_LIB=-lmp3lame
endif

vnc2mpg_CFLAGS=-I$(FFMPEG_HOME)/libavformat -I$(FFMPEG_HOME)/libavcodec -I$(FFMPEG_HOME)/libavutil
vnc2mpg_LDADD=$(LDADD) $(FFMPEG_HOME)/libavformat/libavformat.a $(FFMPEG_HOME)/libavcodec/libavcodec.a $(MP3LAME_LIB) -lm

FFMPEG_CLIENT=vnc2mpg
endif

if HAVE_LIBSDL
SDLVIEWER=SDLvncviewer

SDLvncviewer_CFLAGS=$(SDL_CFLAGS)

# thanks to autoconf, this looks ugly
SDLvncviewer_LDADD=$(LDADD) $(SDL_LIBS)
endif

noinst_PROGRAMS=ppmtest $(SDLVIEWER) $(FFMPEG_CLIENT) backchannel



