DEFINES=-I.. -g -Wall

if HAVE_LIBPTHREAD
BACKGROUND_TEST=blooptest
endif

copyrecttest_LDADD=$(LDADD) -lm

noinst_PROGRAMS=tight-1 cargstest copyrecttest $(BACKGROUND_TEST) 


LDADD = ../libvncserver/libvncserver.a ../libvncclient/libvncclient.a

