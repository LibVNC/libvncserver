CFLAGS=-g -I.. -I. -Wall
LDLIBS=-lz -ljpeg

OBJS=cursor.o listen.o rfbproto.o sockets.o vncviewer.o ../libvncserver.a

all: libvncclient.a

rfbproto.o: rfbproto.c corre.c hextile.c rre.c tight.c zlib.c

$(OBJS): ../rfb/rfbclient.h

libvncclient.a: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm *.o *.a

