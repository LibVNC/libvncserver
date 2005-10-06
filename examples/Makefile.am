AM_CFLAGS=-I.. -g -Wall
LDADD = ../libvncserver/libvncserver.a @WSOCKLIB@

if OSX
MAC=mac
mac_LDFLAGS=-framework ApplicationServices -framework Carbon -framework IOKit
endif

if WITH_TIGHTVNC_FILETRANSFER
FILETRANSFER=filetransfer
endif

noinst_HEADERS=radon.h

noinst_PROGRAMS=example pnmshow regiontest pnmshow24 fontsel \
	vncev storepasswd colourmaptest simple simple15 $(MAC) \
	$(FILETRANSFER) backchannel


