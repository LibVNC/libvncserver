INCLUDES=-I.
VNCSERVERLIB=-L. -lvncserver -L/usr/local/lib -lz -ljpeg

# Uncomment these two lines to enable use of PThreads
#PTHREADDEF = -DHAVE_PTHREADS
#PTHREADLIB = -lpthread

# Comment the following line to disable the use of 3 Bytes/Pixel.
# The code for 3 Bytes/Pixel is not very efficient!
FLAG24 = -DALLOW24BPP

#OPTFLAGS=-g # -Wall
OPTFLAGS=-O2 -Wall
CFLAGS=$(OPTFLAGS) $(PTHREADDEF) $(FLAG24) $(INCLUDES)
RANLIB=ranlib

LIBS=$(LDFLAGS) $(VNCSERVERLIB) $(PTHREADLIB)

# for Mac OS X
OSX_LIBS = -framework ApplicationServices -framework Carbon

# for Example

SOURCES=main.c rfbserver.c sraRegion.c auth.c sockets.c \
	stats.c corre.c hextile.c rre.c translate.c cutpaste.c \
	zlib.c tight.c httpd.c cursor.c font.c \
	draw.c selbox.c d3des.c vncauth.c cargs.c
OBJS=main.o rfbserver.o sraRegion.o auth.o sockets.o \
	stats.o corre.o hextile.o rre.o translate.o cutpaste.o \
	zlib.o tight.o httpd.o cursor.o font.o \
	draw.o selbox.o d3des.o vncauth.o cargs.o
INSTALLHEADER=rfb.h rfbproto.h sraRegion.h keysym.h

all: example pnmshow storepasswd

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

fontsel: fontsel.o libvncserver.a
	$(CC) -o fontsel fontsel.o -L. -lvncserver -lz -ljpeg

vncev: vncev.o libvncserver.a
	$(CC) -o vncev vncev.o -L. -lvncserver -lz -ljpeg

# Example from Justin
zippy: zippy.o libvncserver.a
	$(CC) -o zippy zippy.o -L. -lvncserver -lz -ljpeg

clean:
	rm -f $(OBJS) *~ core "#"* *.bak *.orig storepasswd.o \
	     	mac.o example.o pnmshow.o pnmshow24.o sratest.o \
		blooptest.o $(OBJS)

realclean: clean
	rm -f OSXvnc-server storepasswd example pnmshow libvncserver.a

depend:
	$(CC) -M $(INCLUDES) $(SOURCES) >.depend

#include .depend
