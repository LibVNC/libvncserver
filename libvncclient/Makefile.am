AM_CFLAGS=-g -I $(top_srcdir) -I. -Wall

libvncclient_la_SOURCES=cursor.c listen.c rfbproto.c sockets.c vncviewer.c minilzo.c

noinst_HEADERS=lzoconf.h minilzo.h

rfbproto.o: rfbproto.c corre.c hextile.c rre.c tight.c zlib.c zrle.c ultra.c

EXTRA_DIST=corre.c hextile.c rre.c tight.c zlib.c zrle.c ultra.c

$(libvncclient_la_OBJECTS): ../rfb/rfbclient.h

lib_LTLIBRARIES=libvncclient.la


