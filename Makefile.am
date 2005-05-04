INCLUDES = $(all_includes)

noinst_LTLIBRARIES = libvncserver.la

libvncserver_la_SOURCES = main.c rfbserver.c sraRegion.c auth.c sockets.c \
	stats.c corre.c hextile.c rre.c translate.c cutpaste.c \
	zlib.c tight.c httpd.c cursor.c font.c \
	draw.c selbox.c d3des.c vncauth.c cargs.c

EXTRA_DIST = CHANGES COPYING README TODO

AM_CPPFLAGS = -DHAVE_PTHREADS -DALLOW24BPP
AM_CFLAGS = -Wno-unused

