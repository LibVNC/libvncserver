CFLAGS=-g -Wall

SUBDIRS=libvncserver examples contrib x11vnc vncterm classes libvncclient client_examples test
DIST_SUBDIRS=libvncserver examples contrib x11vnc vncterm classes libvncclient client_examples test

bin_SCRIPTS = libvncserver-config

includedir=$(prefix)/include/rfb
#include_HEADERS=rfb.h rfbconfig.h rfbint.h rfbproto.h keysym.h rfbregion.h

include_HEADERS=rfb/rfb.h rfb/rfbconfig.h rfb/rfbint.h rfb/rfbproto.h \
	rfb/keysym.h rfb/rfbregion.h rfb/rfbclient.h

if HAVE_RPM
$(PACKAGE)-$(VERSION).tar.gz: dist

# Rule to build RPM distribution package
rpm: $(PACKAGE)-$(VERSION).tar.gz $(PACKAGE).spec
	cp $(PACKAGE)-$(VERSION).tar.gz @RPMSOURCEDIR@
	rpmbuild -ba $(PACKAGE).spec
endif

all: make_config_executable

make_config_executable:
	chmod a+x libvncserver-config


