CC=gcc
CFLAGS=-g -Wall
#CFLAGS=-O2 -Wall
RANLIB=ranlib

INCLUDES=-I. -Ilibvncauth -Iinclude -Iinclude/X11 -Iinclude/Xserver
VNCAUTHLIB=-Llibvncauth -lvncauth
VNCSERVERLIB=-L. -lvncserver -lz -ljpeg

# These two lines enable useage of PThreads
CFLAGS += -DHAVE_PTHREADS
VNCSERVERLIB += -lpthread

LIBS=$(VNCSERVERLIB) $(VNCAUTHLIB)

# for Mac OS X
OSX_LIBS = -framework ApplicationServices -framework Carbon

# for Example
PTHREAD_LIBS = -lpthread

SOURCES=main.c rfbserver.c miregion.c auth.c sockets.c xalloc.c \
	stats.c corre.c hextile.c rre.c translate.c cutpaste.c \
	zlib.c tight.c
OBJS=main.o rfbserver.o miregion.o auth.o sockets.o xalloc.o \
	stats.o corre.o hextile.o rre.o translate.o cutpaste.o \
	zlib.o tight.o

all: example storepasswd

install_OSX: OSXvnc-server
	cp OSXvnc-server storepasswd ../OSXvnc/build/OSXvnc.app/Contents/MacOS

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

libvncserver.a: $(OBJS)
	$(AR) cru $@ $(OBJS)
	$(RANLIB) $@

example: example.o libvncauth/libvncauth.a libvncserver.a
	$(CC) -o example example.o $(LIBS) $(PTHREAD_LIBS)

OSXvnc-server: mac.o libvncauth/libvncauth.a libvncserver.a
	$(CC) -o OSXvnc-server mac.o $(LIBS) $(OSX_LIBS)

storepasswd: storepasswd.o libvncauth/libvncauth.a
	$(CC) -o storepasswd storepasswd.o $(VNCAUTHLIB)

libvncauth/libvncauth.a:
	(cd libvncauth; make)

clean:
	rm -f $(OBJS) *~ core "#"* *.bak *.orig storepasswd.o *.a example.o \
		libvncauth/*.o libvncauth/*~ libvncauth/*.a

realclean: clean
	rm -f OSXvnc-server storepasswd

depend:
	$(CC) -M $(INCLUDES) $(SOURCES) >.depend

#include .depend
