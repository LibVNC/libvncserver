INCLUDES=-I.
VNCSERVERLIB=-L. -lvncserver -L/usr/local/lib -lz -ljpeg

#CXX=
CXX=g++
CC=gcc
LINK=gcc

# for IRIX
#EXTRALIBS=-L/usr/lib32

# for Solaris
#EXTRALIBS=-lsocket -lnsl -L/usr/X/lib

# for FreeBSD
#EXTRAINCLUDES=-I/usr/X11R6/include

# Uncomment these two lines to enable use of PThreads
#PTHREADDEF = -DHAVE_PTHREADS
#PTHREADLIB = -lpthread

# Comment the following line to disable the use of 3 Bytes/Pixel.
# The code for 3 Bytes/Pixel is not very efficient!
FLAG24 = -DALLOW24BPP

OPTFLAGS=-g -Wall -pedantic
#OPTFLAGS=-O2 -Wall
RANLIB=ranlib

# for Mac OS X
OSX_LIBS = -framework ApplicationServices -framework Carbon -framework IOKit

# for x11vnc
#XLIBS =  -L/usr/X11R6/lib -lXtst -lXext -lX11
XLIBS =  -L/usr/X11R6/lib -L/usr/lib32 -lXtst -lXext -lX11

ifdef CXX

ZRLE_SRCS=zrle.cc rdr/FdInStream.cxx rdr/FdOutStream.cxx rdr/InStream.cxx \
	rdr/NullOutStream.cxx rdr/ZlibInStream.cxx rdr/ZlibOutStream.cxx
ZRLE_OBJS=zrle.o rdr/FdInStream.o rdr/FdOutStream.o rdr/InStream.o \
	rdr/NullOutStream.o rdr/ZlibInStream.o rdr/ZlibOutStream.o
ZRLE_DEF=-DHAVE_ZRLE
LINK=$(CXX)

%.o: %.cxx
	$(CXX) $(CXXFLAGS) -c -o $@ $<

endif

CFLAGS=$(OPTFLAGS) $(PTHREADDEF) $(FLAG24) $(INCLUDES) $(EXTRAINCLUDES) $(ZRLE_DEF) -DBACKCHANNEL
CXXFLAGS=$(OPTFLAGS) $(PTHREADDEF) $(FLAG24) $(INCLUDES) $(EXTRAINCLUDES) $(ZRLE_DEF) -DBACKCHANNEL
LIBS=$(LDFLAGS) $(VNCSERVERLIB) $(PTHREADLIB) $(EXTRALIBS)

SOURCES=main.c rfbserver.c sraRegion.c auth.c sockets.c \
	stats.c corre.c hextile.c rre.c translate.c cutpaste.c \
	zlib.c tight.c httpd.c cursor.c font.c \
	draw.c selbox.c d3des.c vncauth.c cargs.c $(ZRLE_SRCS)
OBJS=main.o rfbserver.o sraRegion.o auth.o sockets.o \
	stats.o corre.o hextile.o rre.o translate.o cutpaste.o \
	zlib.o tight.o httpd.o cursor.o font.o \
	draw.o selbox.o d3des.o vncauth.o cargs.o $(ZRLE_OBJS)
INSTALLHEADER=rfb.h rfbproto.h sraRegion.h keysym.h

all: example pnmshow storepasswd

all_examples: example pnmshow x11vnc x11vnc_static sratest blooptest pnmshow24 fontsel vncev zippy storepasswd

install_OSX: OSXvnc-server
	cp OSXvnc-server storepasswd ../OSXvnc/build/OSXvnc.app/Contents/MacOS

.c.o:
	$(CC) $(CFLAGS) -c $<

$(OBJS) pnmshow24.o pnmshow.o example.o mac.o blooptest.o: Makefile rfb.h

libvncserver.a: $(OBJS)
	$(AR) cru $@ $(OBJS)
	$(RANLIB) $@

translate.o: translate.c tableinit24.c tableinitcmtemplate.c tableinittctemplate.c tabletrans24template.c tabletranstemplate.c

example: example.o libvncserver.a
	$(LINK) -o example example.o $(LIBS)

pnmshow: pnmshow.o libvncserver.a
	$(LINK) -o pnmshow pnmshow.o $(LIBS)

mac.o: mac.c 1instance.c

OSXvnc-server: mac.o libvncserver.a
	$(LINK) -o OSXvnc-server mac.o $(LIBS) $(OSX_LIBS)

x11vnc.o: contrib/x11vnc.c rfb.h 1instance.c Makefile
	$(CC) $(CFLAGS) -I. -c -o x11vnc.o contrib/x11vnc.c

x11vnc: x11vnc.o libvncserver.a
	$(LINK) -g -o x11vnc x11vnc.o $(LIBS) $(XLIBS)

x11vnc_static: x11vnc.o libvncserver.a
	$(LINK) -o x11vnc_static x11vnc.o libvncserver.a /usr/lib/libz.a /usr/lib/libjpeg.a $(XLIBS)
#$(LIBS) $(XLIBS)

storepasswd: storepasswd.o d3des.o vncauth.o
	$(LINK) -o storepasswd storepasswd.o d3des.o vncauth.o

sratest: sratest.o
	$(LINK) -o sratest sratest.o

sratest.o: sraRegion.c
	$(CC) $(CFLAGS) -DSRA_TEST -c -o sratest.o sraRegion.c

blooptest: blooptest.o libvncserver.a
	$(LINK) -o blooptest blooptest.o $(LIBS)

blooptest.o: example.c rfb.h
	$(CC) $(CFLAGS) -DBACKGROUND_LOOP_TEST -c -o blooptest.o example.c

pnmshow24: pnmshow24.o libvncserver.a
	$(LINK) -o pnmshow24 pnmshow24.o $(LIBS)

fontsel: fontsel.o libvncserver.a
	$(LINK) -o fontsel fontsel.o -L. -lvncserver -lz -ljpeg

vncev: vncev.o libvncserver.a
	$(LINK) -o vncev vncev.o -L. -lvncserver -lz -ljpeg

# Example from Justin
zippy: contrib/zippy.o libvncserver.a
	$(LINK) -o zippy contrib/zippy.o -L. -lvncserver -lz -ljpeg

clean:
	rm -f $(OBJS) *~ core "#"* *.bak *.orig storepasswd.o \
	     	x11vnc.o mac.o example.o pnmshow.o pnmshow24.o sratest.o \
		blooptest.o $(OBJS)

realclean: clean
	rm -f OSXvnc-server storepasswd example pnmshow libvncserver.a

depend:
	$(CC) -M $(INCLUDES) $(SOURCES) >.depend

#include .depend
