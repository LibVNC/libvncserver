CFLAGS=-I..

if HAVE_LIBPTHREAD
BACKGROUND_TEST=blooptest
endif

if OSX
MAC=mac
mac_LDFLAGS=-framework ApplicationServices -framework Carbon -framework IOKit
endif

noinst_HEADERS=radon.h

noinst_PROGRAMS=example pnmshow regiontest pnmshow24 fontsel \
	vncev storepasswd colourmaptest $(BACKGROUND_TEST) $(MAC)

LDADD = ../libvncserver.a

