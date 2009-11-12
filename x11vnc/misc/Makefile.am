SUBDIRS = turbovnc
DIST_SUBDIRS = turbovnc
EXTRA_DIST=README blockdpy.c dtVncPopup rx11vnc rx11vnc.pl shm_clear ranfb.pl slide.pl vcinject.pl x11vnc_loop Xdummy

if HAVE_XI2
if HAVE_CAIRO
noinst_PROGRAMS=xi2setcursor
xi2setcursor_SOURCES=xi2setcursor.c
xi2setcursor_CFLAGS= @XI2_CFLAGS@ @CAIRO_CFLAGS@ 
xi2setcursor_LDADD= @X_LIBS@ @XI2_LIBS@ @CAIRO_LIBS@
endif
endif


