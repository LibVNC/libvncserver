CFLAGS=-I. -I../include
VNCSERVERLIB=-L.. -lvncserver -L/usr/local/lib -lz -ljpeg -lpthread
LDFLAGS=$(VNCSERVERLIB)
ifdef CXX
LINK=$(CXX)
else
LINK=$(CC)
endif

all: example pnmshow sratest blooptest pnmshow24 fontsel vncev storepasswd

example: example.o ../libvncserver.a
	$(LINK) -o example example.o $(LDFLAGS)

pnmshow: pnmshow.o ../libvncserver.a
	$(LINK) -o pnmshow pnmshow.o $(LDFLAGS)

mac.o: mac.c 1instance.c

OSXvnc-server: mac.o ../libvncserver.a
	$(LINK) -o OSXvnc-server mac.o $(LDFLAGS) $(OSX_LIBS)

storepasswd: storepasswd.o
	$(LINK) -o storepasswd storepasswd.o $(LDFLAGS)

sratest: sratest.o
	$(LINK) -o sratest sratest.o

sratest.o: ../sraRegion.c
	$(CC) $(CFLAGS) -DSRA_TEST -c -o sratest.o ../sraRegion.c

blooptest: blooptest.o ../libvncserver.a
	$(LINK) -o blooptest blooptest.o $(LDFLAGS)

blooptest.o: example.c
	$(CC) $(CFLAGS) -DBACKGROUND_LOOP_TEST -c -o blooptest.o example.c

pnmshow24: pnmshow24.o ../libvncserver.a
	$(LINK) -o pnmshow24 pnmshow24.o $(LDFLAGS)

fontsel: fontsel.o ../libvncserver.a
	$(LINK) -o fontsel fontsel.o $(LDFLAGS)

vncev: vncev.o ../libvncserver.a
	$(LINK) -o vncev vncev.o $(LDFLAGS)

pnmshow24.o pnmshow.o example.o mac.o blooptest.o: Makefile ../include/rfb.h

clean:
	rm -f $(OBJS) *~ core "#"* *.bak *.orig
