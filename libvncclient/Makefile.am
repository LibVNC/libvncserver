INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/common

if HAVE_GNUTLS
TLSSRCS = tls_gnutls.c
TLSLIBS = @GNUTLS_LIBS@
else
if HAVE_LIBSSL
TLSSRCS = tls_openssl.c 
TLSLIBS = @SSL_LIBS@ @CRYPT_LIBS@
else
TLSSRCS = tls_none.c
endif
endif


libvncclient_la_SOURCES=cursor.c listen.c rfbproto.c sockets.c vncviewer.c ../common/minilzo.c $(TLSSRCS)
libvncclient_la_LIBADD=$(TLSLIBS)

noinst_HEADERS=../common/lzodefs.h ../common/lzoconf.h ../common/minilzo.h tls.h

rfbproto.o: rfbproto.c corre.c hextile.c rre.c tight.c zlib.c zrle.c ultra.c

EXTRA_DIST=corre.c hextile.c rre.c tight.c zlib.c zrle.c ultra.c tls_gnutls.c tls_openssl.c tls_none.c

$(libvncclient_la_OBJECTS): ../rfb/rfbclient.h

lib_LTLIBRARIES=libvncclient.la


