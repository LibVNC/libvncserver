LDADD = ../libvncserver.a

noinst_PROGRAMS=zippy

if HAVE_X
bin_PROGRAMS=x11vnc
x11vnc_SOURCES=x11vnc.c
INCLUDES=@X_CFLAGS@
x11vnc_LDADD=@X_LIBS@ $(LDADD)
endif

zippy_SOURCES=zippy.c

