INCLUDES = -I$(top_srcdir)

libvncclient_la_SOURCES=cursor.c listen.c rfbproto.c sockets.c vncviewer.c minilzo.c tls.c packetbuf.c

noinst_HEADERS=lzoconf.h minilzo.h tls.h packetbuf.h

rfbproto.o: rfbproto.c corre.c hextile.c rre.c tight.c zlib.c zrle.c ultra.c

EXTRA_DIST=corre.c hextile.c rre.c tight.c zlib.c zrle.c ultra.c tls.c

$(libvncclient_la_OBJECTS): ../rfb/rfbclient.h

lib_LTLIBRARIES=libvncclient.la


