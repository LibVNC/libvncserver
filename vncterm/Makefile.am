CONSOLE_SRCS=VNConsole.c

noinst_HEADERS=VNConsole.h vga.h

CFLAGS_ADD=-I..

LDADD=../libvncserver/libvncserver.a @WSOCKLIB@
INCLUDES=-I.

if LINUX
bin_PROGRAMS=LinuxVNC
LinuxVNC_SOURCES=LinuxVNC.c $(CONSOLE_SRCS)
endif

if ! MINGW
VNCOMMAND=VNCommand
endif

noinst_PROGRAMS=example $(VNCOMMAND)
example_SOURCES=example.c $(CONSOLE_SRCS)
VNCommand_SOURCES=VNCommand.c $(CONSOLE_SRCS)



