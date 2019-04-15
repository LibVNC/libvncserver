#ifndef _RFB_CRYPTO_H
#define _RFB_CRYPTO_H 1

#include "rfb/rfbconfig.h"

#define SHA1_HASH_SIZE 20
#define MD5_HASH_SIZE 16

#ifdef LIBVNCSERVER_HAVE_SYS_UIO_H
#include <sys/uio.h>

void digestmd5(const struct iovec *iov, int iovcnt, void *dest);
void digestsha1(const struct iovec *iov, int iovcnt, void *dest);
#endif

#endif
