DEFINES = -I ..
LDADD = ../libvncserver/libvncserver.a

EXTRA_DIST=ChangeLog

if CYGIPC
LD_CYGIPC=-lcygipc
endif

if HAVE_X
bin_PROGRAMS=x11vnc
x11vnc_SOURCES=x11vnc.c
INCLUDES=@X_CFLAGS@
x11vnc_LDADD=@X_LIBS@ $(LD_CYGIPC) $(LDADD)
endif

