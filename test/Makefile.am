DEFINES=-I.. -g -Wall

if HAVE_LIBPTHREAD
BACKGROUND_TEST=blooptest
endif

copyrecttest_LDADD=$(LDADD) -lm

noinst_PROGRAMS=encodingstest cargstest copyrecttest $(BACKGROUND_TEST) 

LDADD = ../libvncserver/libvncserver.a ../libvncclient/libvncclient.a

test: encodingstest cargstest copyrecttest
	./encodingstest && ./cargstest

