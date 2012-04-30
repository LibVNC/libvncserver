INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/common

if WITH_TIGHTVNC_FILETRANSFER
TIGHTVNCFILETRANSFERHDRS=tightvnc-filetransfer/filelistinfo.h \
	tightvnc-filetransfer/filetransfermsg.h \
	tightvnc-filetransfer/handlefiletransferrequest.h \
	tightvnc-filetransfer/rfbtightproto.h

TIGHTVNCFILETRANSFERSRCS = tightvnc-filetransfer/rfbtightserver.c \
	tightvnc-filetransfer/handlefiletransferrequest.c \
	tightvnc-filetransfer/filetransfermsg.c \
	tightvnc-filetransfer/filelistinfo.c
endif

if WITH_WEBSOCKETS

if HAVE_GNUTLS
WEBSOCKETSSSLSRCS = rfbssl_gnutls.c rfbcrypto_gnutls.c
WEBSOCKETSSSLLIBS = @GNUTLS_LIBS@
else
if HAVE_LIBSSL
WEBSOCKETSSSLSRCS = rfbssl_openssl.c rfbcrypto_openssl.c
WEBSOCKETSSSLLIBS = @SSL_LIBS@ @CRYPT_LIBS@
else
WEBSOCKETSSSLSRCS = rfbssl_none.c rfbcrypto_included.c ../common/md5.c ../common/sha1.c
endif
endif

WEBSOCKETSSRCS = websockets.c $(WEBSOCKETSSSLSRCS)
endif

includedir=$(prefix)/include/rfb
#include_HEADERS=rfb.h rfbconfig.h rfbint.h rfbproto.h keysym.h rfbregion.h

include_HEADERS=../rfb/rfb.h ../rfb/rfbconfig.h ../rfb/rfbint.h \
	../rfb/rfbproto.h ../rfb/keysym.h ../rfb/rfbregion.h ../rfb/rfbclient.h

noinst_HEADERS=../common/d3des.h ../rfb/default8x16.h zrleoutstream.h \
	zrlepalettehelper.h zrletypes.h private.h scale.h rfbssl.h rfbcrypto.h \
	../common/minilzo.h ../common/lzoconf.h ../common/lzodefs.h ../common/md5.h ../common/sha1.h \
	$(TIGHTVNCFILETRANSFERHDRS)

EXTRA_DIST=tableinit24.c tableinittctemplate.c tabletranstemplate.c \
	tableinitcmtemplate.c tabletrans24template.c \
	zrleencodetemplate.c

if HAVE_LIBZ
ZLIBSRCS = zlib.c zrle.c zrleoutstream.c zrlepalettehelper.c ../common/zywrletemplate.c
if HAVE_LIBJPEG
TIGHTSRCS = tight.c ../common/turbojpeg.c
endif
endif

LIB_SRCS = main.c rfbserver.c rfbregion.c auth.c sockets.c $(WEBSOCKETSSRCS) \
	stats.c corre.c hextile.c rre.c translate.c cutpaste.c \
	httpd.c cursor.c font.c \
	draw.c selbox.c ../common/d3des.c ../common/vncauth.c cargs.c ../common/minilzo.c ultra.c scale.c \
	$(ZLIBSRCS) $(TIGHTSRCS) $(TIGHTVNCFILETRANSFERSRCS)

libvncserver_la_SOURCES=$(LIB_SRCS)
libvncserver_la_LIBADD=$(WEBSOCKETSSSLLIBS)

lib_LTLIBRARIES=libvncserver.la

if HAVE_RPM
$(PACKAGE)-$(VERSION).tar.gz: dist

# Rule to build RPM distribution package
rpm: $(PACKAGE)-$(VERSION).tar.gz libvncserver.spec
	cp $(PACKAGE)-$(VERSION).tar.gz @RPMSOURCEDIR@
	rpmbuild -ba libvncserver.spec
endif


