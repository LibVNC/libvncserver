if HAVE_LIBPTHREAD
BACKGROUND_TEST=blooptest
endif

noinst_HEADERS=radon.h

noinst_PROGRAMS=example pnmshow sratest pnmshow24 fontsel \
	vncev storepasswd $(BACKGROUND_TEST)

LDADD = -L.. -lvncserver

