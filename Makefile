INCLUDES=-I.
VNCSERVERLIB=-L. -lvncserver -L/usr/local/lib -lz -ljpeg

# Uncomment these two lines to enable use of PThreads
PTHREADDEF = -DHAVE_PTHREADS
PTHREADLIB = -lpthread

# Comment the following line to disable the use of 3 Bytes/Pixel.
# The code for 3 Bytes/Pixel is not very efficient!
FLAG24 = -DALLOW24BPP

#CC=cc
CFLAGS=-g -Wall $(PTHREADDEF) $(FLAG24) $(INCLUDES)
#CFLAGS=-O2 -Wall
RANLIB=ranlib

LIBS=$(LDFLAGS) $(VNCSERVERLIB) $(PTHREADLIB)

# for Mac OS X
OSX_LIBS = -framework ApplicationServices -framework Carbon

# for Example

SOURCES=main.c rfbserver.c sraRegion.c auth.c sockets.c \
	stats.c corre.c hextile.c rre.c translate.c cutpaste.c \
	zlib.c tight.c httpd.c cursor.o \
	d3des.c vncauth.c
OBJS=main.o rfbserver.o sraRegion.o auth.o sockets.o \
	stats.o corre.o hextile.o rre.o translate.o cutpaste.o \
	zlib.o tight.o httpd.o cursor.o \
	d3des.o vncauth.o

all: example pnmshow storepasswd

install_OSX: OSXvnc-server
	cp OSXvnc-server storepasswd ../OSXvnc/build/OSXvnc.app/Contents/MacOS

.c.o:
	$(CC) $(CFLAGS) -c $<

$(OBJS): Makefile rfb.h

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

sratest: sratest.o
	$(CC) -o sratest sratest.o

sratest.o: sraRegion.c
	$(CC) $(CFLAGS) -DSRA_TEST -c -o sratest.o sraRegion.c

blooptest: blooptest.o libvncserver.a
	$(CC) -o blooptest blooptest.o $(LIBS)

blooptest.o: example.c rfb.h
	$(CC) $(CFLAGS) -DBACKGROUND_LOOP_TEST -c -o blooptest.o example.c

pnmshow24: pnmshow24.o libvncserver.a
	$(CC) -o pnmshow24 pnmshow24.o $(LIBS)

pnmshow24.o: Makefile

clean:
	rm -f $(OBJS) *~ core "#"* *.bak *.orig storepasswd.o \
	     	mac.o example.o pnmshow.o sratest.o $(OBJS)

realclean: clean
	rm -f OSXvnc-server storepasswd example pnmshow libvncserver.a

depend:
	$(CC) -M $(INCLUDES) $(SOURCES) >.depend

#include .depend
