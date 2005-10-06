AM_CFLAGS=-g -Wall

includedir=$(prefix)/include/rfb

noinst_HEADERS=filelistinfo.h  filetransfermsg.h \
	handlefiletransferrequest.h  rfbtightproto.h

LIB_SRCS = rfbtightserver.c handlefiletransferrequest.c filetransfermsg.c \
	filelistinfo.c

tightvnc_filetransfer_a_SOURCES=$(LIB_SRCS)

lib_LIBRARIES=tightvnc-filetransfer.a


