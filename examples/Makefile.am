CFLAGS=-I.. -g -Wall
LDADD = ../libvncserver/libvncserver.a

if HAVE_LIBPTHREAD
BACKGROUND_TEST=blooptest
endif

if OSX
MAC=mac
mac_LDFLAGS=-framework ApplicationServices -framework Carbon -framework IOKit
endif

copyrecttest_LDADD=$(LDADD) -lm

noinst_HEADERS=radon.h

noinst_PROGRAMS=example pnmshow regiontest pnmshow24 fontsel \
	vncev storepasswd colourmaptest simple simple15 copyrecttest \
	$(BACKGROUND_TEST) $(MAC)


