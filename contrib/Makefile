CFLAGS=-I. -I../include
VNCSERVERLIB=-L.. -lvncserver -L/usr/local/lib -lz -ljpeg -lpthread
LDFLAGS=$(VNCSERVERLIB)
ifdef CXX
LINK=$(CXX)
else
LINK=$(CC)
endif

# for x11vnc
#XLIBS =  -L/usr/X11R6/lib -lXtst -lXext -lX11
XLIBS =  -L/usr/X11R6/lib -L/usr/lib32 -lXtst -lXext -lX11

OBJS=*.o

all: x11vnc zippy

x11vnc.o: x11vnc.c ../include/rfb.h ../examples/1instance.c Makefile
	$(CC) $(CFLAGS) -I. -c -o x11vnc.o x11vnc.c

x11vnc: x11vnc.o ../libvncserver.a
	$(LINK) -g -o x11vnc x11vnc.o $(LDFLAGS) $(XLIBS)

# Example from Justin
zippy: zippy.o ../libvncserver.a
	$(LINK) -o zippy zippy.o $(LDFLAGS)

clean:
	rm -f $(OBJS) *~ core "#"* *.bak *.orig

