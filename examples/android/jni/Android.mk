LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LIBVNCSERVER_ROOT:=../../..

HAVE_LIBZ=1
#HAVE_LIBJPEG=1

ifdef HAVE_LIBZ
ZLIBSRCS := \
	$(LIBVNCSERVER_ROOT)/libvncserver/zlib.c \
	$(LIBVNCSERVER_ROOT)/libvncserver/zrle.c \
	$(LIBVNCSERVER_ROOT)/libvncserver/zrleoutstream.c \
	$(LIBVNCSERVER_ROOT)/libvncserver/zrlepalettehelper.c \
	$(LIBVNCSERVER_ROOT)/common/zywrletemplate.c
ifdef HAVE_LIBJPEG
TIGHTSRCS := $(LIBVNCSERVER_ROOT)/libvncserver/tight.c
endif
endif

LOCAL_SRC_FILES:= \
	fbvncserver.c \
	$(LIBVNCSERVER_ROOT)/libvncserver/main.c \
	$(LIBVNCSERVER_ROOT)/libvncserver/rfbserver.c \
	$(LIBVNCSERVER_ROOT)/libvncserver/rfbregion.c \
	$(LIBVNCSERVER_ROOT)/libvncserver/auth.c \
	$(LIBVNCSERVER_ROOT)/libvncserver/sockets.c \
	$(LIBVNCSERVER_ROOT)/libvncserver/stats.c \
	$(LIBVNCSERVER_ROOT)/libvncserver/corre.c \
	$(LIBVNCSERVER_ROOT)/libvncserver/hextile.c \
	$(LIBVNCSERVER_ROOT)/libvncserver/rre.c \
	$(LIBVNCSERVER_ROOT)/libvncserver/translate.c \
	$(LIBVNCSERVER_ROOT)/libvncserver/cutpaste.c \
	$(LIBVNCSERVER_ROOT)/libvncserver/httpd.c \
	$(LIBVNCSERVER_ROOT)/libvncserver/cursor.c \
	$(LIBVNCSERVER_ROOT)/libvncserver/font.c \
	$(LIBVNCSERVER_ROOT)/libvncserver/draw.c \
	$(LIBVNCSERVER_ROOT)/libvncserver/selbox.c \
	$(LIBVNCSERVER_ROOT)/common/d3des.c \
	$(LIBVNCSERVER_ROOT)/common/vncauth.c \
	$(LIBVNCSERVER_ROOT)/libvncserver/cargs.c \
	$(LIBVNCSERVER_ROOT)/common/minilzo.c \
	$(LIBVNCSERVER_ROOT)/libvncserver/ultra.c \
	$(LIBVNCSERVER_ROOT)/libvncserver/scale.c \
	$(ZLIBSRCS) \
	$(TIGHTSRCS)

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH) \
	$(LOCAL_PATH)/$(LIBVNCSERVER_ROOT)/libvncserver \
	$(LOCAL_PATH)/$(LIBVNCSERVER_ROOT)/common \
	$(LOCAL_PATH)/$(LIBVNCSERVER_ROOT) \
	external/jpeg

ifdef HAVE_LIBZ
LOCAL_SHARED_LIBRARIES := libz
LOCAL_LDLIBS := -lz
endif
ifdef HAVE_LIBJPEG
LOCAL_STATIC_LIBRARIES := libjpeg
endif

LOCAL_MODULE:= androidvncserver

include $(BUILD_EXECUTABLE)
