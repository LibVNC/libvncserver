SUBDIRS=. examples contrib
DIST_SUBDIRS=examples contrib

include_HEADERS=include/rfb.h include/rfbconfig.h include/rfbproto.h \
	include/keysym.h

ZRLE_SRCS=zrle.cxx rdr/FdInStream.cxx rdr/FdOutStream.cxx rdr/InStream.cxx \
	rdr/NullOutStream.cxx rdr/ZlibInStream.cxx rdr/ZlibOutStream.cxx

LIB_SOURCES = main.c rfbserver.c sraRegion.c auth.c sockets.c \
	stats.c corre.c hextile.c rre.c translate.c cutpaste.c \
	zlib.c tight.c httpd.c cursor.c font.c \
	draw.c selbox.c d3des.c vncauth.c cargs.c

if HAVE_CXX
libvncserver_a_SOURCES=$(LIB_SOURCES) $(ZRLE_SRCS)
else
libvncserver_a_SOURCES=$(LIB_SOURCES)
endif

lib_LIBRARIES=libvncserver.a
