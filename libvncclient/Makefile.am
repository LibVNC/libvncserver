CFLAGS=-g -I.. -I. -Wall

libvncclient_a_SOURCES=cursor.c listen.c rfbproto.c sockets.c vncviewer.c

rfbproto.o: rfbproto.c corre.c hextile.c rre.c tight.c zlib.c

EXTRA_DIST=corre.c hextile.c rre.c tight.c zlib.c

$(libvncclient_a_OBJECTS): ../rfb/rfbclient.h

lib_LIBRARIES=libvncclient.a

client_test_SOURCES=client_test.c

noinst_PROGRAMS=client_test

#client_test_LDADD=libvncclient.a ../libvncserver.a
client_test_LDADD=libvncclient.a
