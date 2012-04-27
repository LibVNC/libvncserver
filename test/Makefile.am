
if HAVE_LIBJPEG
# TurboJPEG wrapper tests
noinst_PROGRAMS=tjunittest tjbench
tjunittest_SOURCES=tjunittest.c ../common/turbojpeg.c ../common/turbojpeg.h \
	tjutil.c tjutil.h
tjbench_SOURCES=tjbench.c ../common/turbojpeg.c ../common/turbojpeg.h \
	tjutil.c tjutil.h bmp.c bmp.h
tjbench_LDADD=$(LDADD) -lm
endif

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/common
LDADD = ../libvncserver/libvncserver.la ../libvncclient/libvncclient.la @WSOCKLIB@

if HAVE_LIBPTHREAD
BACKGROUND_TEST=blooptest
ENCODINGS_TEST=encodingstest
endif

copyrecttest_LDADD=$(LDADD) -lm

check_PROGRAMS=$(ENCODINGS_TEST) cargstest copyrecttest $(BACKGROUND_TEST) \
	cursortest

test: encodingstest$(EXEEXT) cargstest$(EXEEXT) copyrecttest$(EXEEXT)
	./encodingstest && ./cargstest

