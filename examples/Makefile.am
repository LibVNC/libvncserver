AM_CFLAGS=-I $(top_srcdir) -g -Wall
LDADD = ../libvncserver/libvncserver.la @WSOCKLIB@

if OSX
MAC=mac
mac_LDFLAGS=-framework ApplicationServices -framework Carbon -framework IOKit
endif

if WITH_TIGHTVNC_FILETRANSFER
FILETRANSFER=filetransfer
endif

if HAVE_LIBPTHREAD
BLOOPTEST=blooptest
endif

noinst_HEADERS=radon.h rotatetemplate.c

noinst_PROGRAMS=example pnmshow regiontest pnmshow24 fontsel \
	vncev storepasswd colourmaptest simple simple15 $(MAC) \
	$(FILETRANSFER) backchannel $(BLOOPTEST) camera rotate

