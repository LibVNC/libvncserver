#ifndef _RFB_RFBCONFIG_H
/* #undef _RFB_RFBCONFIG_H */
 
/* rfb/rfbconfig.h. Generated automatically by cmake. */

/* Enable 24 bit per pixel in native framebuffer */
#define LIBVNCSERVER_ALLOW24BPP  1 

/* work around when write() returns ENOENT but does not mean it */
/* #undef LIBVNCSERVER_ENOENT_WORKAROUND */

/* Define to 1 if you have the <dirent.h> header file. */
#define LIBVNCSERVER_HAVE_DIRENT_H 1

/* Define to 1 if you have the <endian.h> header file. */
#define LIBVNCSERVER_HAVE_ENDIAN_H 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define LIBVNCSERVER_HAVE_FCNTL_H  1 

/* Define to 1 if you have the `gettimeofday' function. */
#define LIBVNCSERVER_HAVE_GETTIMEOFDAY  1 

/* Define to 1 if you have the `ftime' function. */
#define LIBVNCSERVER_HAVE_FTIME  1 

/* Define to 1 if you have the `gethostbyname' function. */
#define LIBVNCSERVER_HAVE_GETHOSTBYNAME  1 

/* Define to 1 if you have the `gethostname' function. */
#define LIBVNCSERVER_HAVE_GETHOSTNAME  1 

/* Define to 1 if you have the `inet_ntoa' function. */
#define LIBVNCSERVER_HAVE_INET_NTOA  1 

/* Define to 1 if you have the `memmove' function. */
#define LIBVNCSERVER_HAVE_MEMMOVE  1 

/* Define to 1 if you have the `memset' function. */
#define LIBVNCSERVER_HAVE_MEMSET  1 

/* Define to 1 if you have the `mkfifo' function. */
#define LIBVNCSERVER_HAVE_MKFIFO  1 

/* Define to 1 if you have the `select' function. */
#define LIBVNCSERVER_HAVE_SELECT  1 

/* Define to 1 if you have the `socket' function. */
#define LIBVNCSERVER_HAVE_SOCKET  1 

/* Define to 1 if you have the `strchr' function. */
#define LIBVNCSERVER_HAVE_STRCHR  1 

/* Define to 1 if you have the `strcspn' function. */
#define LIBVNCSERVER_HAVE_STRCSPN  1 

/* Define to 1 if you have the `strdup' function. */
#define LIBVNCSERVER_HAVE_STRDUP  1 

/* Define to 1 if you have the `strerror' function. */
#define LIBVNCSERVER_HAVE_STRERROR  1 

/* Define to 1 if you have the `strstr' function. */
#define LIBVNCSERVER_HAVE_STRSTR  1 

/* Define to 1 if you have the `jpeg' library (-ljpeg). */
/* #undef LIBVNCSERVER_HAVE_LIBJPEG */

/* Define if you have the `png' library (-lpng). */
/* #undef LIBVNCSERVER_HAVE_LIBPNG */

/* Define to 1 if you have the `pthread' library (-lpthread). */
#define LIBVNCSERVER_HAVE_LIBPTHREAD  1 

/* Define to 1 if you have win32 threads. */
/* #undef LIBVNCSERVER_HAVE_WIN32THREADS */

/* Define to 1 if you have the `z' library (-lz). */
#define LIBVNCSERVER_HAVE_LIBZ  1 

/* Define to 1 if you have the `lzo2' library (-llzo2). */
/* #undef LIBVNCSERVER_HAVE_LZO */

/* Define to 1 if you have the <netinet/in.h> header file. */
#define LIBVNCSERVER_HAVE_NETINET_IN_H  1 

/* Define to 1 if you have the <sys/endian.h> header file. */
/* #undef LIBVNCSERVER_HAVE_SYS_ENDIAN_H */

/* Define to 1 if you have the <sys/socket.h> header file. */
#define LIBVNCSERVER_HAVE_SYS_SOCKET_H  1 

/* Define to 1 if you have the <sys/stat.h> header file. */
#define LIBVNCSERVER_HAVE_SYS_STAT_H  1 

/* Define to 1 if you have the <sys/time.h> header file. */
#define LIBVNCSERVER_HAVE_SYS_TIME_H  1 

/* Define to 1 if you have the <sys/types.h> header file. */
#define LIBVNCSERVER_HAVE_SYS_TYPES_H  1 

/* Define to 1 if you have <sys/wait.h> that is POSIX.1 compatible. */
#define LIBVNCSERVER_HAVE_SYS_WAIT_H  1 

/* Define to 1 if you have <sys/uio.h> */
/* #undef LIBVNCSERVER_HAVE_SYS_UIO_H */

/* Define to 1 if you have <sys/resource.h> */
#define LIBVNCSERVER_HAVE_SYS_RESOURCE_H  1

/* Define to 1 if you have the <unistd.h> header file. */
#define LIBVNCSERVER_HAVE_UNISTD_H  1 

/* Define to 1 if you have the `vfork' function. */
#define LIBVNCSERVER_HAVE_VFORK  1 

/* Define to 1 if you have the <vfork.h> header file. */
/* #undef LIBVNCSERVER_HAVE_VFORK_H */

/* Define to 1 if you have the `vprintf' function. */
#define LIBVNCSERVER_HAVE_VPRINTF  1 

/* Define to 1 if `fork' works. */
/* #undef LIBVNCSERVER_HAVE_WORKING_FORK */

/* Define to 1 if `vfork' works. */
/* #undef LIBVNCSERVER_HAVE_WORKING_VFORK */

/* Define to 1 if `mmap' exists. */
#define LIBVNCSERVER_HAVE_MMAP  1 

/* Define to 1 if `fork' exists. */
#define LIBVNCSERVER_HAVE_FORK  1 

/* Define to 1 if you have the <ws2tcpip.h> header file. */
/* #undef LIBVNCSERVER_HAVE_WS2TCPIP_H */

/* Enable IPv6 support */
#define LIBVNCSERVER_IPv6  1 

/* Need a typedef for in_addr_t */
/* #undef LIBVNCSERVER_NEED_INADDR_T */

/* Define to the full name and version of this package. */
#define LIBVNCSERVER_PACKAGE_STRING  "LibVNCServer 0.9.15"

/* Define to the version of this package. */
#define LIBVNCSERVER_PACKAGE_VERSION  "0.9.15"
#define LIBVNCSERVER_VERSION "0.9.15"
#define LIBVNCSERVER_VERSION_MAJOR "0"
#define LIBVNCSERVER_VERSION_MINOR "9"
#define LIBVNCSERVER_VERSION_PATCHLEVEL "15"

/* Define to 1 if libgcrypt is present */
/* #undef LIBVNCSERVER_HAVE_LIBGCRYPT */

/* Define to 1 if GnuTLS is present */
#define LIBVNCSERVER_HAVE_GNUTLS 1

/* Define to 1 if OpenSSL is present */
#define LIBVNCSERVER_HAVE_LIBSSL 1

/* Define to 1 if Cyrus SASL is present */
/* #undef LIBVNCSERVER_HAVE_SASL */

/* Define to 1 to build with websockets */
#define LIBVNCSERVER_WITH_WEBSOCKETS 1

/* Define to 1 if your processor stores words with the most significant byte
   first (like Motorola and SPARC, unlike Intel and VAX). */
/* #undef LIBVNCSERVER_WORDS_BIGENDIAN */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
/* #ifndef __cplusplus */
/* #undef inline */
/* #endif */

/* Define to `int' if <sys/types.h> does not define. */
#define HAVE_LIBVNCSERVER_PID_T 1
#ifndef HAVE_LIBVNCSERVER_PID_T
typedef int pid_t;
#endif

/* The type for size_t */
#define HAVE_LIBVNCSERVER_SIZE_T 1
#ifndef HAVE_LIBVNCSERVER_SIZE_T
typedef int size_t;
#endif

/* The type for socklen */
#define HAVE_LIBVNCSERVER_SOCKLEN_T 1
#ifndef HAVE_LIBVNCSERVER_SOCKLEN_T
typedef int socklen_t;
#endif

/* once: _RFB_RFBCONFIG_H */
#endif
