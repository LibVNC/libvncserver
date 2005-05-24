DEFINES=-g -I.. -I. -Wall

libvncclient_a_SOURCES=cursor.c listen.c rfbproto.c sockets.c vncviewer.c

rfbproto.o: rfbproto.c corre.c hextile.c rre.c tight.c zlib.c zrle.c

EXTRA_DIST=corre.c hextile.c rre.c tight.c zlib.c zrle.c

$(libvncclient_a_OBJECTS): ../rfb/rfbclient.h

lib_LIBRARIES=libvncclient.a


