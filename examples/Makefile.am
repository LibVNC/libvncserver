DEFINES=-I.. -g -Wall
LDADD = ../libvncserver/libvncserver.a

if OSX
MAC=mac
mac_LDFLAGS=-framework ApplicationServices -framework Carbon -framework IOKit
endif

noinst_HEADERS=radon.h

noinst_PROGRAMS=example pnmshow regiontest pnmshow24 fontsel \
	vncev storepasswd colourmaptest simple simple15 $(MAC)


