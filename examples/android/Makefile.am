INCLUDES = -I$(top_srcdir)
LDADD = $(top_srcdir)/libvncserver/libvncserver.la @WSOCKLIB@

noinst_PROGRAMS=androidvncserver
androidvncserver_SOURCES=jni/fbvncserver.c

EXTRA_DIST=jni/Android.mk
