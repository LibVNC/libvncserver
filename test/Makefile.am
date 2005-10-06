AM_CFLAGS=-I.. -g -Wall
LDADD = ../libvncserver/libvncserver.a ../libvncclient/libvncclient.a @WSOCKLIB@

if HAVE_LIBPTHREAD
BACKGROUND_TEST=blooptest
ENCODINGS_TEST=encodingstest
endif

copyrecttest_LDADD=$(LDADD) -lm

noinst_PROGRAMS=$(ENCODINGS_TEST) cargstest copyrecttest $(BACKGROUND_TEST) \
	cursortest

test: encodingstest$(EXEEXT) cargstest$(EXEEXT) copyrecttest$(EXEEXT)
	./encodingstest && ./cargstest

