AM_CFLAGS = -I $(top_srcdir)
LDADD = ../libvncserver/libvncserver.a @WSOCKLIB@

if OSX
x11vnc_LDFLAGS = -framework ApplicationServices -framework Carbon -framework IOKit -framework Cocoa
x11vnc_CFLAGS = -ObjC
endif

SUBDIRS = misc
DIST_SUBDIRS = misc

man_MANS=x11vnc.1
EXTRA_DIST=ChangeLog README tkx11vnc $(man_MANS)

if CYGIPC
LD_CYGIPC=-lcygipc
endif

bin_PROGRAMS=x11vnc
x11vnc_SOURCES = 8to24.c cleanup.c connections.c cursor.c gui.c help.c inet.c keyboard.c linuxfb.c macosx.c macosxCG.c macosxCGP.c macosxCGS.c options.c pm.c pointer.c rates.c remote.c scan.c screen.c selection.c solid.c sslcmds.c sslhelper.c uinput.c unixpw.c user.c userinput.c util.c v4l.c win_utils.c x11vnc.c x11vnc_defs.c xdamage.c xevents.c xinerama.c xkb_bell.c xrandr.c xrecord.c xwrappers.c 8to24.h allowed_input_t.h blackout_t.h cleanup.h connections.h cursor.h enums.h gui.h help.h inet.h keyboard.h linuxfb.h macosx.h macosxCG.h macosxCGP.h macosxCGS.h nox11.h nox11_funcs.h options.h params.h pm.h pointer.h rates.h remote.h scan.h screen.h scrollevent_t.h selection.h solid.h sslcmds.h sslhelper.h ssltools.h tkx11vnc.h uinput.h unixpw.h user.h userinput.h util.h v4l.h win_utils.h winattr_t.h x11vnc.h xdamage.h xevents.h xinerama.h xkb_bell.h xrandr.h xrecord.h xwrappers.h
if HAVE_X
INCLUDES=@X_CFLAGS@
x11vnc_LDADD=$(LDADD) @SSL_LIBS@ @CRYPT_LIBS@ @X_LIBS@ $(LD_CYGIPC)
else
x11vnc_LDADD=$(LDADD) @SSL_LIBS@ @CRYPT_LIBS@ $(LD_CYGIPC)
endif

