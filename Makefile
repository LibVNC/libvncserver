CC=cc
CFLAGS=-g -Wall
#CFLAGS=-O2 -Wall
RANLIB=ranlib

INCLUDES=-I. -Iinclude
VNCSERVERLIB=-L. -lvncserver -lz -ljpeg

# These two lines enable useage of PThreads
#CFLAGS += -DHAVE_PTHREADS
#VNCSERVERLIB += -lpthread

LIBS=$(LDFLAGS) $(VNCSERVERLIB)

# for Mac OS X
OSX_LIBS = -framework ApplicationServices -framework Carbon

# for Example

SOURCES=main.c rfbserver.c miregion.c auth.c sockets.c xalloc.c \
	stats.c corre.c hextile.c rre.c translate.c cutpaste.c \
	zlib.c tight.c httpd.c cursor.o \
	d3des.c vncauth.c
OBJS=main.o rfbserver.o miregion.o auth.o sockets.o xalloc.o \
	stats.o corre.o hextile.o rre.o translate.o cutpaste.o \
	zlib.o tight.o httpd.o cursor.o \
	d3des.o vncauth.o

all: example pnmshow storepasswd

install_OSX: OSXvnc-server
	cp OSXvnc-server storepasswd ../OSXvnc/build/OSXvnc.app/Contents/MacOS

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

libvncserver.a: $(OBJS)
	$(AR) cru $@ $(OBJS)
	$(RANLIB) $@

example: example.o libvncserver.a
	$(CC) -o example example.o $(LIBS)

pnmshow: pnmshow.o libvncserver.a
	$(CC) -o pnmshow pnmshow.o $(LIBS)

OSXvnc-server: mac.o libvncserver.a
	$(CC) -o OSXvnc-server mac.o $(LIBS) $(OSX_LIBS)

storepasswd: storepasswd.o d3des.o vncauth.o
	$(CC) -o storepasswd storepasswd.o d3des.o vncauth.o

clean:
	rm -f $(OBJS) *~ core "#"* *.bak *.orig storepasswd.o *.a $(OBJS)


realclean: clean
	rm -f OSXvnc-server storepasswd example pnmshow

depend:
	$(CC) -M $(INCLUDES) $(SOURCES) >.depend

#include .depend
