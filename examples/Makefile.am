if HAVE_LIBPTHREAD
BACKGROUND_TEST=blooptest
endif

if OSX
MAC=mac
endif

noinst_HEADERS=radon.h

noinst_PROGRAMS=example pnmshow regiontest pnmshow24 fontsel \
	vncev storepasswd $(BACKGROUND_TEST) $(MAC)

LDADD = -L.. -lvncserver

