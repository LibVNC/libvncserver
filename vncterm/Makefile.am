CONSOLE_SRCS=VNConsole.c

noinst_HEADERS=VNConsole.h vga.h

CFLAGS=-I..

LDADD=../libvncserver.a
INCLUDES=-I. -I../include

if LINUX
bin_PROGRAMS=LinuxVNC
LinuxVNC_SOURCES=LinuxVNC.c $(CONSOLE_SRCS)
endif

noinst_PROGRAMS=VNCommand example
VNCommand_SOURCES=VNCommand.c $(CONSOLE_SRCS)
example_SOURCES=example.c $(CONSOLE_SRCS)

