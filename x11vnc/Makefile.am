AM_CFLAGS = -I ..
LDADD = ../libvncserver/libvncserver.a @WSOCKLIB@

SUBDIRS = misc
DIST_SUBDIRS = misc

man_MANS=x11vnc.1
EXTRA_DIST=ChangeLog README tkx11vnc $(man_MANS)

if CYGIPC
LD_CYGIPC=-lcygipc
endif

if HAVE_X
bin_PROGRAMS=x11vnc
x11vnc_SOURCES=x11vnc.c tkx11vnc.h
INCLUDES=@X_CFLAGS@
x11vnc_LDADD=@X_LIBS@ $(LD_CYGIPC) $(LDADD)
endif

