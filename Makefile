INCLUDES=-I. -Iinclude
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
#PTHREADLIB = -lpthread

# Comment the following line to disable the use of 3 Bytes/Pixel.
# The code for 3 Bytes/Pixel is not very efficient!

OPTFLAGS=-g -Wall -pedantic
#OPTFLAGS=-O2 -Wall
RANLIB=ranlib

# for Mac OS X
OSX_LIBS = -framework ApplicationServices -framework Carbon -framework IOKit

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

CFLAGS=$(OPTFLAGS) $(INCLUDES) $(EXTRAINCLUDES)
CXXFLAGS=$(OPTFLAGS) $(INCLUDES) $(EXTRAINCLUDES)
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

all: libvncserver.a all_examples

all_examples:
	cd examples && make

all_contrib:
	cd contrib && make

install_OSX: OSXvnc-server
	cp OSXvnc-server storepasswd ../OSXvnc/build/OSXvnc.app/Contents/MacOS

.c.o:
	$(CC) $(CFLAGS) -c $<

$(OBJS): Makefile include/rfb.h

libvncserver.a: $(OBJS)
	$(AR) cru $@ $(OBJS)
	$(RANLIB) $@

translate.o: translate.c tableinit24.c tableinitcmtemplate.c tableinittctemplate.c tabletrans24template.c tabletranstemplate.c

clean:
	rm -f $(OBJS) *~ core "#"* *.bak *.orig
	cd examples && make clean
	cd contrib && make clean

realclean: clean
	rm -f OSXvnc-server storepasswd example pnmshow libvncserver.a

depend:
	$(CC) -M $(INCLUDES) $(SOURCES) >.depend

#include .depend
