DEFINES=-g -Wall

includedir=$(prefix)/include/rfb
#include_HEADERS=rfb.h rfbconfig.h rfbint.h rfbproto.h keysym.h rfbregion.h

include_HEADERS=../rfb/rfb.h ../rfb/rfbconfig.h ../rfb/rfbint.h \
	../rfb/rfbproto.h ../rfb/keysym.h ../rfb/rfbregion.h ../rfb/rfbclient.h

noinst_HEADERS=d3des.h ../rfb/default8x16.h zrleoutstream.h \
	zrlepalettehelper.h zrletypes.h

EXTRA_DIST=tableinit24.c tableinittctemplate.c tabletranstemplate.c \
	tableinitcmtemplate.c tabletrans24template.c \
	zrleencodetemplate.c

if HAVE_LIBZ
ZLIBSRCS = zlib.c zrle.c zrleoutstream.c zrlepalettehelper.c
if HAVE_LIBJPEG
JPEGSRCS = tight.c
endif
endif

LIB_SRCS = main.c rfbserver.c rfbregion.c auth.c sockets.c \
	stats.c corre.c hextile.c rre.c translate.c cutpaste.c \
	httpd.c cursor.c font.c \
	draw.c selbox.c d3des.c vncauth.c cargs.c \
	$(ZLIBSRCS) $(JPEGSRCS)

libvncserver_a_SOURCES=$(LIB_SRCS)

lib_LIBRARIES=libvncserver.a

if HAVE_RPM
$(PACKAGE)-$(VERSION).tar.gz: dist

# Rule to build RPM distribution package
rpm: $(PACKAGE)-$(VERSION).tar.gz libvncserver.spec
	cp $(PACKAGE)-$(VERSION).tar.gz @RPMSOURCEDIR@
	rpm -ba libvncserver.spec
endif


