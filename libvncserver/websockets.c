/*
 * websockets.c - deal with WebSockets clients.
 *
 * This code should be independent of any changes in the RFB protocol. It is
 * an additional handshake and framing of normal sockets:
 *   http://www.whatwg.org/specs/web-socket-protocol/
 *
 */

/*
 *  Copyright (C) 2010 Joel Martin
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

#ifdef __STRICT_ANSI__
#define _BSD_SOURCE
#endif

#include <rfb/rfb.h>
/* errno */
#include <errno.h>

#ifndef _MSC_VER
#include <resolv.h> /* __b64_ntop */
#endif

#ifdef LIBVNCSERVER_HAVE_ENDIAN_H
#include <endian.h>
#elif LIBVNCSERVER_HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#endif

#ifdef LIBVNCSERVER_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <string.h>
#if LIBVNCSERVER_UNISTD_H
#include <unistd.h>
#endif
#include "rfb/rfbconfig.h"
#include "rfbssl.h"
#include "rfbcrypto.h"

#if defined(__APPLE__)

#include <libkern/OSByteOrder.h>
#define WS_NTOH64(n) OSSwapBigToHostInt64(n)
#define WS_NTOH32(n) OSSwapBigToHostInt32(n)
#define WS_NTOH16(n) OSSwapBigToHostInt16(n)
#define WS_HTON64(n) OSSwapHostToBigInt64(n)
#define WS_HTON16(n) OSSwapHostToBigInt16(n)

#else

#define WS_NTOH64(n) htobe64(n)
#define WS_NTOH32(n) htobe32(n)
#define WS_NTOH16(n) htobe16(n)
#define WS_HTON64(n) htobe64(n)
#define WS_HTON16(n) htobe16(n)

#endif

#define B64LEN(__x) (((__x + 2) / 3) * 12 / 3)
#define WSHLENMAX 14  /* 2 + sizeof(uint64_t) + sizeof(uint32_t) */
#define WS_HYBI_MASK_LEN 4

#define ARRAYSIZE(a) ((sizeof(a) / sizeof((a[0]))) / (size_t)(!(sizeof(a) % sizeof((a[0])))))

enum {
  WEBSOCKETS_VERSION_HIXIE,
  WEBSOCKETS_VERSION_HYBI
};

#if 0
#include <sys/syscall.h>
static int gettid() {
    return (int)syscall(SYS_gettid);
}
#endif

typedef int (*wsEncodeFunc)(rfbClientPtr cl, const char *src, int len, char **dst);
typedef int (*wsDecodeFunc)(rfbClientPtr cl, char *dst, int len);


enum {
  /* header not yet received completely */
  WS_HYBI_STATE_HEADER_PENDING,
  /* data available */
  WS_HYBI_STATE_DATA_AVAILABLE,
  WS_HYBI_STATE_DATA_NEEDED,
  /* received a complete frame */
  WS_HYBI_STATE_FRAME_COMPLETE,
  /* received part of a 'close' frame */
  WS_HYBI_STATE_CLOSE_REASON_PENDING,
  /* */
  WS_HYBI_STATE_ERR
};

typedef union ws_mask_s {
  char c[4];
  uint32_t u;
} ws_mask_t;

/* XXX: The union and the structs do not need to be named.
 *      We are working around a bug present in GCC < 4.6 which prevented
 *      it from recognizing anonymous structs and unions.
 *      See http://gcc.gnu.org/bugzilla/show_bug.cgi?id=4784
 */
typedef struct 
#if __GNUC__
__attribute__ ((__packed__)) 
#endif
ws_header_s {
  unsigned char b0;
  unsigned char b1;
  union {
    struct 
#if __GNUC__
    __attribute__ ((__packed__)) 
#endif
           {
      uint16_t l16;
      ws_mask_t m16;
    } s16;
    struct
#if __GNUC__
__attribute__ ((__packed__)) 
#endif
           {
      uint64_t l64;
      ws_mask_t m64;
    } s64;
    ws_mask_t m;
  } u;
} ws_header_t;

typedef struct ws_header_data_s {
  ws_header_t *data;
  /** bytes read */
  int nRead;
  /** mask value */
  ws_mask_t mask;
  /** length of frame header including payload len, but without mask */
  int headerLen;
  /** length of the payload data */
  int payloadLen;
  /** opcode */
  unsigned char opcode;
} ws_header_data_t;

typedef struct ws_ctx_s {
    char codeBufDecode[B64LEN(UPDATE_BUF_SIZE) + WSHLENMAX]; /* base64 + maximum frame header length */
    char codeBufEncode[B64LEN(UPDATE_BUF_SIZE) + WSHLENMAX]; /* base64 + maximum frame header length */
    char *writePos;
    unsigned char *readPos;
    int readlen;
    int hybiDecodeState;
    char carryBuf[3];                      /* For base64 carry-over */
    int carrylen;
    int version;
    int base64;
    ws_header_data_t header;
    int nReadRaw;
    int nToRead;
    wsEncodeFunc encode;
    wsDecodeFunc decode;
} ws_ctx_t;

enum
{
    WS_OPCODE_CONTINUATION = 0x0,
    WS_OPCODE_TEXT_FRAME,
    WS_OPCODE_BINARY_FRAME,
    WS_OPCODE_CLOSE = 0x8,
    WS_OPCODE_PING,
    WS_OPCODE_PONG
};

#define FLASH_POLICY_RESPONSE "<cross-domain-policy><allow-access-from domain=\"*\" to-ports=\"*\" /></cross-domain-policy>\n"
#define SZ_FLASH_POLICY_RESPONSE 93

/*
 * draft-ietf-hybi-thewebsocketprotocol-10
 * 5.2.2. Sending the Server's Opening Handshake
 */
#define GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

#define SERVER_HANDSHAKE_HIXIE "HTTP/1.1 101 Web Socket Protocol Handshake\r\n\
Upgrade: WebSocket\r\n\
Connection: Upgrade\r\n\
%sWebSocket-Origin: %s\r\n\
%sWebSocket-Location: %s://%s%s\r\n\
%sWebSocket-Protocol: %s\r\n\
\r\n%s"

#define SERVER_HANDSHAKE_HYBI "HTTP/1.1 101 Switching Protocols\r\n\
Upgrade: websocket\r\n\
Connection: Upgrade\r\n\
Sec-WebSocket-Accept: %s\r\n\
Sec-WebSocket-Protocol: %s\r\n\
\r\n"

#define SERVER_HANDSHAKE_HYBI_NO_PROTOCOL "HTTP/1.1 101 Switching Protocols\r\n\
Upgrade: websocket\r\n\
Connection: Upgrade\r\n\
Sec-WebSocket-Accept: %s\r\n\
\r\n"

#define WEBSOCKETS_CLIENT_CONNECT_WAIT_MS 100
#define WEBSOCKETS_CLIENT_SEND_WAIT_MS 100
#define WEBSOCKETS_MAX_HANDSHAKE_LEN 4096

#if defined(__linux__) && defined(NEED_TIMEVAL)
struct timeval
{
   long int tv_sec,tv_usec;
}
;
#endif

static rfbBool webSocketsHandshake(rfbClientPtr cl, char *scheme);
void webSocketsGenMd5(char * target, char *key1, char *key2, char *key3);

static int webSocketsEncodeHybi(rfbClientPtr cl, const char *src, int len, char **dst);
static int webSocketsEncodeHixie(rfbClientPtr cl, const char *src, int len, char **dst);
static int webSocketsDecodeHybi(rfbClientPtr cl, char *dst, int len);
static int webSocketsDecodeHixie(rfbClientPtr cl, char *dst, int len);

static void hybiDecodeCleanup(ws_ctx_t *wsctx);

static int
min (int a, int b) {
    return a < b ? a : b;
}

static void webSocketsGenSha1Key(char *target, int size, char *key)
{
    struct iovec iov[2];
    unsigned char hash[20];

    iov[0].iov_base = key;
    iov[0].iov_len = strlen(key);
    iov[1].iov_base = GUID;
    iov[1].iov_len = sizeof(GUID) - 1;
    digestsha1(iov, 2, hash);
    if (-1 == b64_ntop(hash, sizeof(hash), target, size))
	rfbErr("b64_ntop failed\n");
}

/*
 * rfbWebSocketsHandshake is called to handle new WebSockets connections
 */

rfbBool
webSocketsCheck (rfbClientPtr cl)
{
    char bbuf[4], *scheme;
    int ret;

    ret = rfbPeekExactTimeout(cl, bbuf, 4,
                                   WEBSOCKETS_CLIENT_CONNECT_WAIT_MS);
    if ((ret < 0) && (errno == ETIMEDOUT)) {
      rfbLog("Normal socket connection\n");
      return TRUE;
    } else if (ret <= 0) {
      rfbErr("webSocketsHandshake: unknown connection error\n");
      return FALSE;
    }

    if (strncmp(bbuf, "<", 1) == 0) {
        rfbLog("Got Flash policy request, sending response\n");
        if (rfbWriteExact(cl, FLASH_POLICY_RESPONSE,
                          SZ_FLASH_POLICY_RESPONSE) < 0) {
            rfbErr("webSocketsHandshake: failed sending Flash policy response");
        }
        return FALSE;
    } else if (strncmp(bbuf, "\x16", 1) == 0 || strncmp(bbuf, "\x80", 1) == 0) {
        rfbLog("Got TLS/SSL WebSockets connection\n");
        if (-1 == rfbssl_init(cl)) {
	  rfbErr("webSocketsHandshake: rfbssl_init failed\n");
	  return FALSE;
	}
	ret = rfbPeekExactTimeout(cl, bbuf, 4, WEBSOCKETS_CLIENT_CONNECT_WAIT_MS);
        scheme = "wss";
    } else {
        scheme = "ws";
    }

    if (strncmp(bbuf, "GET ", 4) != 0) {
      rfbErr("webSocketsHandshake: invalid client header\n");
      return FALSE;
    }

    rfbLog("Got '%s' WebSockets handshake\n", scheme);

    if (!webSocketsHandshake(cl, scheme)) {
        return FALSE;
    }
    /* Start WebSockets framing */
    return TRUE;
}

static rfbBool
webSocketsHandshake(rfbClientPtr cl, char *scheme)
{
    char *buf, *response, *line;
    int n, linestart = 0, len = 0, llen, base64 = TRUE;
    char prefix[5], trailer[17];
    char *path = NULL, *host = NULL, *origin = NULL, *protocol = NULL;
    char *key1 = NULL, *key2 = NULL, *key3 = NULL;
    char *sec_ws_origin = NULL;
    char *sec_ws_key = NULL;
    char sec_ws_version = 0;
    ws_ctx_t *wsctx = NULL;

    buf = (char *) malloc(WEBSOCKETS_MAX_HANDSHAKE_LEN);
    if (!buf) {
        rfbLogPerror("webSocketsHandshake: malloc");
        return FALSE;
    }
    response = (char *) malloc(WEBSOCKETS_MAX_HANDSHAKE_LEN);
    if (!response) {
        free(buf);
        rfbLogPerror("webSocketsHandshake: malloc");
        return FALSE;
    }

    while (len < WEBSOCKETS_MAX_HANDSHAKE_LEN-1) {
        if ((n = rfbReadExactTimeout(cl, buf+len, 1,
                                     WEBSOCKETS_CLIENT_SEND_WAIT_MS)) <= 0) {
            if ((n < 0) && (errno == ETIMEDOUT)) {
                break;
            }
            if (n == 0)
                rfbLog("webSocketsHandshake: client gone\n");
            else
                rfbLogPerror("webSocketsHandshake: read");
            free(response);
            free(buf);
            return FALSE;
        }

        len += 1;
        llen = len - linestart;
        if (((llen >= 2)) && (buf[len-1] == '\n')) {
            line = buf+linestart;
            if ((llen == 2) && (strncmp("\r\n", line, 2) == 0)) {
                if (key1 && key2) {
                    if ((n = rfbReadExact(cl, buf+len, 8)) <= 0) {
                        if ((n < 0) && (errno == ETIMEDOUT)) {
                            break;
                        }
                        if (n == 0)
                            rfbLog("webSocketsHandshake: client gone\n");
                        else
                            rfbLogPerror("webSocketsHandshake: read");
                        free(response);
                        free(buf);
                        return FALSE;
                    }
                    rfbLog("Got key3\n");
                    key3 = buf+len;
                    len += 8;
                } else {
                    buf[len] = '\0';
                }
                break;
            } else if ((llen >= 16) && ((strncmp("GET ", line, min(llen,4))) == 0)) {
                /* 16 = 4 ("GET ") + 1 ("/.*") + 11 (" HTTP/1.1\r\n") */
                path = line+4;
                buf[len-11] = '\0'; /* Trim trailing " HTTP/1.1\r\n" */
                cl->wspath = strdup(path);
                /* rfbLog("Got path: %s\n", path); */
            } else if ((strncasecmp("host: ", line, min(llen,6))) == 0) {
                host = line+6;
                buf[len-2] = '\0';
                /* rfbLog("Got host: %s\n", host); */
            } else if ((strncasecmp("origin: ", line, min(llen,8))) == 0) {
                origin = line+8;
                buf[len-2] = '\0';
                /* rfbLog("Got origin: %s\n", origin); */
            } else if ((strncasecmp("sec-websocket-key1: ", line, min(llen,20))) == 0) {
                key1 = line+20;
                buf[len-2] = '\0';
                /* rfbLog("Got key1: %s\n", key1); */
            } else if ((strncasecmp("sec-websocket-key2: ", line, min(llen,20))) == 0) {
                key2 = line+20;
                buf[len-2] = '\0';
                /* rfbLog("Got key2: %s\n", key2); */
            /* HyBI */

	    } else if ((strncasecmp("sec-websocket-protocol: ", line, min(llen,24))) == 0) {
                protocol = line+24;
                buf[len-2] = '\0';
                rfbLog("Got protocol: %s\n", protocol);
            } else if ((strncasecmp("sec-websocket-origin: ", line, min(llen,22))) == 0) {
		sec_ws_origin = line+22;
                buf[len-2] = '\0';
            } else if ((strncasecmp("sec-websocket-key: ", line, min(llen,19))) == 0) {
		sec_ws_key = line+19;
                buf[len-2] = '\0';
            } else if ((strncasecmp("sec-websocket-version: ", line, min(llen,23))) == 0) {
		sec_ws_version = strtol(line+23, NULL, 10);
                buf[len-2] = '\0';
	    }

            linestart = len;
        }
    }

    if (!(path && host && (origin || sec_ws_origin))) {
        rfbErr("webSocketsHandshake: incomplete client handshake\n");
        free(response);
        free(buf);
        return FALSE;
    }

    if ((protocol) && (strstr(protocol, "binary"))) {
        if (! sec_ws_version) {
            rfbErr("webSocketsHandshake: 'binary' protocol not supported with Hixie\n");
            free(response);
            free(buf);
            return FALSE;
        }
        rfbLog("  - webSocketsHandshake: using binary/raw encoding\n");
        base64 = FALSE;
        protocol = "binary";
    } else {
        rfbLog("  - webSocketsHandshake: using base64 encoding\n");
        base64 = TRUE;
        if ((protocol) && (strstr(protocol, "base64"))) {
            protocol = "base64";
        } else {
            protocol = "";
        }
    }

    /*
     * Generate the WebSockets server response based on the the headers sent
     * by the client.
     */

    if (sec_ws_version) {
	char accept[B64LEN(SHA1_HASH_SIZE) + 1];
	rfbLog("  - WebSockets client version hybi-%02d\n", sec_ws_version);
	webSocketsGenSha1Key(accept, sizeof(accept), sec_ws_key);
        if(strlen(protocol) > 0)
            len = snprintf(response, WEBSOCKETS_MAX_HANDSHAKE_LEN,
	                   SERVER_HANDSHAKE_HYBI, accept, protocol);
        else
            len = snprintf(response, WEBSOCKETS_MAX_HANDSHAKE_LEN,
                           SERVER_HANDSHAKE_HYBI_NO_PROTOCOL, accept);
    } else {
	/* older hixie handshake, this could be removed if
	 * a final standard is established */
	if (!(key1 && key2 && key3)) {
	    rfbLog("  - WebSockets client version hixie-75\n");
	    prefix[0] = '\0';
	    trailer[0] = '\0';
	} else {
	    rfbLog("  - WebSockets client version hixie-76\n");
	    snprintf(prefix, 5, "Sec-");
	    webSocketsGenMd5(trailer, key1, key2, key3);
	}
	len = snprintf(response, WEBSOCKETS_MAX_HANDSHAKE_LEN,
		 SERVER_HANDSHAKE_HIXIE, prefix, origin, prefix, scheme,
		 host, path, prefix, protocol, trailer);
    }

    if (rfbWriteExact(cl, response, len) < 0) {
        rfbErr("webSocketsHandshake: failed sending WebSockets response\n");
        free(response);
        free(buf);
        return FALSE;
    }
    /* rfbLog("webSocketsHandshake: %s\n", response); */
    free(response);
    free(buf);


    wsctx = calloc(1, sizeof(ws_ctx_t));
    if (sec_ws_version) {
	wsctx->version = WEBSOCKETS_VERSION_HYBI;
	wsctx->encode = webSocketsEncodeHybi;
	wsctx->decode = webSocketsDecodeHybi;
    } else {
	wsctx->version = WEBSOCKETS_VERSION_HIXIE;
	wsctx->encode = webSocketsEncodeHixie;
	wsctx->decode = webSocketsDecodeHixie;
    }
    wsctx->base64 = base64;
    hybiDecodeCleanup(wsctx);
    cl->wsctx = (wsCtx *)wsctx;
    return TRUE;
}

void
webSocketsGenMd5(char * target, char *key1, char *key2, char *key3)
{
    unsigned int i, spaces1 = 0, spaces2 = 0;
    unsigned long num1 = 0, num2 = 0;
    unsigned char buf[17];
    struct iovec iov[1];

    for (i=0; i < strlen(key1); i++) {
        if (key1[i] == ' ') {
            spaces1 += 1;
        }
        if ((key1[i] >= 48) && (key1[i] <= 57)) {
            num1 = num1 * 10 + (key1[i] - 48);
        }
    }
    num1 = num1 / spaces1;

    for (i=0; i < strlen(key2); i++) {
        if (key2[i] == ' ') {
            spaces2 += 1;
        }
        if ((key2[i] >= 48) && (key2[i] <= 57)) {
            num2 = num2 * 10 + (key2[i] - 48);
        }
    }
    num2 = num2 / spaces2;

    /* Pack it big-endian */
    buf[0] = (num1 & 0xff000000) >> 24;
    buf[1] = (num1 & 0xff0000) >> 16;
    buf[2] = (num1 & 0xff00) >> 8;
    buf[3] =  num1 & 0xff;

    buf[4] = (num2 & 0xff000000) >> 24;
    buf[5] = (num2 & 0xff0000) >> 16;
    buf[6] = (num2 & 0xff00) >> 8;
    buf[7] =  num2 & 0xff;

    strncpy((char *)buf+8, key3, 8);
    buf[16] = '\0';

    iov[0].iov_base = buf;
    iov[0].iov_len = 16;
    digestmd5(iov, 1, target);
    target[16] = '\0';

    return;
}

static int
webSocketsEncodeHixie(rfbClientPtr cl, const char *src, int len, char **dst)
{
    int sz = 0;
    ws_ctx_t *wsctx = (ws_ctx_t *)cl->wsctx;

    wsctx->codeBufEncode[sz++] = '\x00';
    len = b64_ntop((unsigned char *)src, len, wsctx->codeBufEncode+sz, sizeof(wsctx->codeBufEncode) - (sz + 1));
    if (len < 0) {
        return len;
    }
    sz += len;

    wsctx->codeBufEncode[sz++] = '\xff';
    *dst = wsctx->codeBufEncode;
    return sz;
}

static int
ws_read(rfbClientPtr cl, char *buf, int len)
{
    int n;
    if (cl->sslctx) {
	n = rfbssl_read(cl, buf, len);
    } else {
	n = read(cl->sock, buf, len);
    }
    return n;
}

static int
ws_peek(rfbClientPtr cl, char *buf, int len)
{
    int n;
    if (cl->sslctx) {
	n = rfbssl_peek(cl, buf, len);
    } else {
	while (-1 == (n = recv(cl->sock, buf, len, MSG_PEEK))) {
	    if (errno != EAGAIN)
		break;
	}
    }
    return n;
}

static int
webSocketsDecodeHixie(rfbClientPtr cl, char *dst, int len)
{
    int retlen = 0, n, i, avail, modlen, needlen;
    char *buf, *end = NULL;
    ws_ctx_t *wsctx = (ws_ctx_t *)cl->wsctx;

    buf = wsctx->codeBufDecode;

    n = ws_peek(cl, buf, len*2+2);

    if (n <= 0) {
        /* save errno because rfbErr() will tamper it */
        int olderrno = errno;
        rfbErr("%s: peek (%d) %m\n", __func__, errno);
        errno = olderrno;
        return n;
    }


    /* Base64 encoded WebSockets stream */

    if (buf[0] == '\xff') {
        i = ws_read(cl, buf, 1); /* Consume marker */
        buf++;
        n--;
    }
    if (n == 0) {
        errno = EAGAIN;
        return -1;
    }
    if (buf[0] == '\x00') {
        i = ws_read(cl, buf, 1); /* Consume marker */
        buf++;
        n--;
    }
    if (n == 0) {
        errno = EAGAIN;
        return -1;
    }

    /* end = memchr(buf, '\xff', len*2+2); */
    end = memchr(buf, '\xff', n);
    if (!end) {
        end = buf + n;
    }
    avail = end - buf;

    len -= wsctx->carrylen;

    /* Determine how much base64 data we need */
    modlen = len + (len+2)/3;
    needlen = modlen;
    if (needlen % 4) {
        needlen += 4 - (needlen % 4);
    }

    if (needlen > avail) {
        /* rfbLog("Waiting for more base64 data\n"); */
        errno = EAGAIN;
        return -1;
    }

    /* Any carryover from previous decode */
    for (i=0; i < wsctx->carrylen; i++) {
        /* rfbLog("Adding carryover %d\n", wsctx->carryBuf[i]); */
        dst[i] = wsctx->carryBuf[i];
        retlen += 1;
    }

    /* Decode the rest of what we need */
    buf[needlen] = '\x00';  /* Replace end marker with end of string */
    /* rfbLog("buf: %s\n", buf); */
    n = b64_pton(buf, (unsigned char *)dst+retlen, 2+len);
    if (n < len) {
        rfbErr("Base64 decode error\n");
        errno = EIO;
        return -1;
    }
    retlen += n;

    /* Consume the data from socket */
    i = ws_read(cl, buf, needlen);

    wsctx->carrylen = n - len;
    retlen -= wsctx->carrylen;
    for (i=0; i < wsctx->carrylen; i++) {
        /* rfbLog("Saving carryover %d\n", dst[retlen + i]); */
        wsctx->carryBuf[i] = dst[retlen + i];
    }

    /* rfbLog("<< webSocketsDecode, retlen: %d\n", retlen); */
    return retlen;
}

static int
hybiRemaining(ws_ctx_t *wsctx)
{
  return wsctx->nToRead - wsctx->nReadRaw;
}

static void
hybiDecodeCleanup(ws_ctx_t *wsctx)
{
  wsctx->header.payloadLen = 0;
  wsctx->header.mask.u = 0;
  wsctx->nReadRaw = 0;
  wsctx->nToRead= 0;
  wsctx->carrylen = 0;
  wsctx->readPos = (unsigned char *)wsctx->codeBufDecode;
  wsctx->readlen = 0;
  wsctx->hybiDecodeState = WS_HYBI_STATE_HEADER_PENDING;
  wsctx->writePos = NULL;
  rfbLog("cleaned up wsctx\n");
}

/**
 * Return payload data that has been decoded/unmasked from
 * a websocket frame.
 *
 * @param[out]     dst destination buffer
 * @param[in]      len bytes to copy to destination buffer
 * @param[in,out]  wsctx internal state of decoding procedure
 * @param[out]     number of bytes actually written to dst buffer
 * @return next hybi decoding state
 */
static int
hybiReturnData(char *dst, int len, ws_ctx_t *wsctx, int *nWritten)
{
  int nextState = WS_HYBI_STATE_ERR;

  /* if we have something already decoded copy and return */
  if (wsctx->readlen > 0) {
    /* simply return what we have */
    if (wsctx->readlen > len) {
      rfbLog("copy to %d bytes to dst buffer; readPos=%p, readLen=%d\n", len, wsctx->readPos, wsctx->readlen);
      memcpy(dst, wsctx->readPos, len);
      *nWritten = len;
      wsctx->readlen -= len;
      wsctx->readPos += len;
      nextState = WS_HYBI_STATE_DATA_AVAILABLE;
    } else {
      rfbLog("copy to %d bytes to dst buffer; readPos=%p, readLen=%d\n", wsctx->readlen, wsctx->readPos, wsctx->readlen);
      memcpy(dst, wsctx->readPos, wsctx->readlen);
      *nWritten = wsctx->readlen;
      wsctx->readlen = 0;
      wsctx->readPos = NULL;
      if (hybiRemaining(wsctx) == 0) {
        nextState = WS_HYBI_STATE_FRAME_COMPLETE;
      } else {
        nextState = WS_HYBI_STATE_DATA_NEEDED;
      }
    }
    rfbLog("after copy: readPos=%p, readLen=%d\n", wsctx->readPos, wsctx->readlen);
  } else if (wsctx->hybiDecodeState == WS_HYBI_STATE_CLOSE_REASON_PENDING) {
    nextState = WS_HYBI_STATE_CLOSE_REASON_PENDING;
  }
  return nextState;
}

/**
 * Read an RFC 6455 websocket frame (IETF hybi working group).
 *
 * Internal state is updated according to bytes received and the
 * decoding of header information.
 *
 * @param[in]   cl client ptr with ptr to raw socket and ws_ctx_t ptr
 * @param[out]  sockRet emulated recv return value
 * @return next hybi decoding state; WS_HYBI_STATE_HEADER_PENDING indicates
 *         that the header was not received completely.
 */
static int
hybiReadHeader(rfbClientPtr cl, int *sockRet)
{
  int ret;
  ws_ctx_t *wsctx = (ws_ctx_t *)cl->wsctx;
  char *headerDst = wsctx->codeBufDecode + wsctx->nReadRaw;
  int n = WSHLENMAX - wsctx->nReadRaw;

  rfbLog("header_read to %p with len=%d\n", headerDst, n);
  ret = ws_read(cl, headerDst, n);
  rfbLog("read %d bytes from socket\n", ret);
  if (ret <= 0) {
    if (-1 == ret) {
      /* save errno because rfbErr() will tamper it */
      int olderrno = errno;
      rfbErr("%s: peek; %m\n", __func__);
      errno = olderrno;
      *sockRet = -1;
    } else {
      *sockRet = 0;
    }
    return WS_HYBI_STATE_ERR;
  }

  wsctx->nReadRaw += ret;
  if (wsctx->nReadRaw < 2) {
    /* cannot decode header with less than two bytes */
    errno = EAGAIN;
    *sockRet = -1;
    return WS_HYBI_STATE_HEADER_PENDING;
  }

  /* first two header bytes received; interpret header data and get rest */
  wsctx->header.data = (ws_header_t *)wsctx->codeBufDecode;

  wsctx->header.opcode = wsctx->header.data->b0 & 0x0f;

  /* fin = (header->b0 & 0x80) >> 7; */ /* not used atm */
  wsctx->header.payloadLen = wsctx->header.data->b1 & 0x7f;
  rfbLog("first header bytes received; opcode=%d lenbyte=%d\n", wsctx->header.opcode, wsctx->header.payloadLen);

  /*
   * 4.3. Client-to-Server Masking
   *
   * The client MUST mask all frames sent to the server.  A server MUST
   * close the connection upon receiving a frame with the MASK bit set to 0.
  **/
  if (!(wsctx->header.data->b1 & 0x80)) {
    rfbErr("%s: got frame without mask ret=%d\n", __func__, ret);
    errno = EIO;
    *sockRet = -1;
    return WS_HYBI_STATE_ERR;
  }

  if (wsctx->header.payloadLen < 126 && wsctx->nReadRaw >= 6) {
    wsctx->header.headerLen = 2 + WS_HYBI_MASK_LEN;
    wsctx->header.mask = wsctx->header.data->u.m;
  } else if (wsctx->header.payloadLen == 126 && 8 <= wsctx->nReadRaw) {
    wsctx->header.headerLen = 4 + WS_HYBI_MASK_LEN;
    wsctx->header.payloadLen = WS_NTOH16(wsctx->header.data->u.s16.l16);
    wsctx->header.mask = wsctx->header.data->u.s16.m16;
  } else if (wsctx->header.payloadLen == 127 && 14 <= wsctx->nReadRaw) {
    wsctx->header.headerLen = 10 + WS_HYBI_MASK_LEN;
    wsctx->header.payloadLen = WS_NTOH64(wsctx->header.data->u.s64.l64);
    wsctx->header.mask = wsctx->header.data->u.s64.m64;
  } else {
    /* Incomplete frame header, try again */
    rfbErr("%s: incomplete frame header; ret=%d\n", __func__, ret);
    errno = EAGAIN;
    *sockRet = -1;
    return WS_HYBI_STATE_HEADER_PENDING;
  }

  /* absolute length of frame */
  wsctx->nToRead = wsctx->header.headerLen + wsctx->header.payloadLen;

  /* set payload pointer just after header */
  wsctx->writePos = wsctx->codeBufDecode + wsctx->nReadRaw;

  wsctx->readPos = (unsigned char *)(wsctx->codeBufDecode + wsctx->header.headerLen);

  rfbLog("header complete: state=%d flen=%d writeTo=%p\n", wsctx->hybiDecodeState, wsctx->nToRead, wsctx->writePos);

  return WS_HYBI_STATE_DATA_NEEDED;
}

static int
hybiWsFrameComplete(ws_ctx_t *wsctx)
{
  return wsctx != NULL && hybiRemaining(wsctx) == 0;
}

static char *
hybiPayloadStart(ws_ctx_t *wsctx)
{
  return wsctx->codeBufDecode + wsctx->header.headerLen;
}


/**
 * Read the remaining payload bytes from associated raw socket.
 *
 *  - try to read remaining bytes from socket
 *  - unmask all multiples of 4
 *  - if frame incomplete but some bytes are left, these are copied to
 *      the carry buffer
 *  - if opcode is TEXT: Base64-decode all unmasked received bytes
 *  - set state for reading decoded data
 *  - reset write position to begin of buffer (+ header)
 *      --> before we retrieve more data we let the caller clear all bytes
 *          from the reception buffer
 *  - execute return data routine
 *
 *  Sets errno corresponding to what it gets from the underlying
 *  socket or EIO if some internal sanity check fails.
 *
 *  @param[in]  cl client ptr with raw socket reference
 *  @param[out] dst  destination buffer
 *  @param[in]  len  size of destination buffer
 *  @param[out] sockRet emulated recv return value
 *  @return next hybi decode state
 */
static int
hybiReadAndDecode(rfbClientPtr cl, char *dst, int len, int *sockRet)
{
  int n;
  int i;
  int toReturn;
  int toDecode;
  int bufsize;
  int nextRead;
  unsigned char *data;
  uint32_t *data32;
  ws_ctx_t *wsctx = (ws_ctx_t *)cl->wsctx;

  /* if data was carried over, copy to start of buffer */
  memcpy(wsctx->writePos, wsctx->carryBuf, wsctx->carrylen);
  wsctx->writePos += wsctx->carrylen;

  /* -1 accounts for potential '\0' terminator for base64 decoding */
  bufsize = wsctx->codeBufDecode + ARRAYSIZE(wsctx->codeBufDecode) - wsctx->writePos - 1;
  if (hybiRemaining(wsctx) > bufsize) {
    nextRead = bufsize;
  } else {
    nextRead = hybiRemaining(wsctx);
  }

  rfbLog("calling read with buf=%p and len=%d (decodebuf=%p headerLen=%d\n)", wsctx->writePos, nextRead, wsctx->codeBufDecode, wsctx->header.headerLen);

  if (wsctx->nReadRaw < wsctx->nToRead) {
    /* decode more data */
    if (-1 == (n = ws_read(cl, wsctx->writePos, nextRead))) {
      int olderrno = errno;
      rfbErr("%s: read; %m", __func__);
      errno = olderrno;
      *sockRet = -1;
      return WS_HYBI_STATE_ERR;
    } else if (n == 0) {
      *sockRet = 0;
      return WS_HYBI_STATE_ERR;
    }
    wsctx->nReadRaw += n;
    rfbLog("read %d bytes from socket; nRead=%d\n", n, wsctx->nReadRaw);
  } else {
    n = 0;
  }

  wsctx->writePos += n;

  if (wsctx->nReadRaw >= wsctx->nToRead) {
    if (wsctx->nReadRaw > wsctx->nToRead) {
      rfbErr("%s: internal error, read past websocket frame", __func__);
      errno=EIO;
      *sockRet = -1;
      return WS_HYBI_STATE_ERR;
    }
  }

  toDecode = wsctx->writePos - hybiPayloadStart(wsctx);
  rfbLog("toDecode=%d from n=%d carrylen=%d headerLen=%d\n", toDecode, n, wsctx->carrylen, wsctx->header.headerLen);
  if (toDecode < 0) {
    rfbErr("%s: internal error; negative number of bytes to decode: %d", __func__, toDecode);
    errno=EIO;
    *sockRet = -1;
    return WS_HYBI_STATE_ERR;
  }

  /* for a possible base64 decoding, we decode multiples of 4 bytes until
   * the whole frame is received and carry over any remaining bytes in the carry buf*/
  data = (unsigned char *)hybiPayloadStart(wsctx);
  data32= (uint32_t *)data;

  for (i = 0; i < (toDecode >> 2); i++) {
    data32[i] ^= wsctx->header.mask.u;
  }
  rfbLog("mask decoding; i=%d toDecode=%d\n", i, toDecode);

  if (wsctx->hybiDecodeState == WS_HYBI_STATE_FRAME_COMPLETE) {
    /* process the remaining bytes (if any) */
    for (i*=4; i < toDecode; i++) {
      data[i] ^= wsctx->header.mask.c[i % 4];
    }

    /* all data is here, no carrying */
    wsctx->carrylen = 0;
  } else {
    /* carry over remaining, non-multiple-of-four bytes */
    wsctx->carrylen = toDecode - (i * 4);
    if (wsctx->carrylen < 0 || wsctx->carrylen > ARRAYSIZE(wsctx->carryBuf)) {
      rfbErr("%s: internal error, invalid carry over size: carrylen=%d, toDecode=%d, i=%d", __func__, wsctx->carrylen, toDecode, i);
      *sockRet = -1;
      errno = EIO;
      return WS_HYBI_STATE_ERR;
    }
    rfbLog("carrying over %d bytes from %p to %p\n", wsctx->carrylen, wsctx->writePos + (i * 4), wsctx->carryBuf);
    memcpy(wsctx->carryBuf, data + (i * 4), wsctx->carrylen);
  }

  toReturn = toDecode - wsctx->carrylen;

  switch (wsctx->header.opcode) {
    case WS_OPCODE_CLOSE:

      /* this data is not returned as payload data */
      if (hybiWsFrameComplete(wsctx)) {
        rfbLog("got closure, reason %d\n", WS_NTOH16(((uint16_t *)data)[0]));
        errno = ECONNRESET;
        *sockRet = -1;
        return WS_HYBI_STATE_FRAME_COMPLETE;
      } else {
        rfbErr("%s: close reason with long frame not supported", __func__);
        errno = EIO;
        *sockRet = -1;
        return WS_HYBI_STATE_ERR;
      }
      break;
    case WS_OPCODE_TEXT_FRAME:
      data[toReturn] = '\0';
      rfbLog("Initiate Base64 decoding in %p with max size %d and '\\0' at %p\n", data, bufsize, data + toReturn);
      if (-1 == (wsctx->readlen = b64_pton((char *)data, data, bufsize))) {
        rfbErr("Base64 decode error in %s; data=%p bufsize=%d", __func__, data, bufsize);
        rfbErr("%s: Base64 decode error; %m\n", __func__);
      }
      wsctx->writePos = hybiPayloadStart(wsctx);
      break;
    case WS_OPCODE_BINARY_FRAME:
      wsctx->readlen = toReturn;
      wsctx->writePos = hybiPayloadStart(wsctx);
      break;
    default:
      rfbErr("%s: unhandled opcode %d, b0: %02x, b1: %02x\n", __func__, (int)wsctx->header.opcode, wsctx->header.data->b0, wsctx->header.data->b1);
  }
  wsctx->readPos = data;

  return hybiReturnData(dst, len, wsctx, sockRet);
}

/**
 * Read function for websocket-socket emulation.
 *
 *    0                   1                   2                   3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-------+-+-------------+-------------------------------+
 *   |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
 *   |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
 *   |N|V|V|V|       |S|             |   (if payload len==126/127)   |
 *   | |1|2|3|       |K|             |                               |
 *   +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
 *   |     Extended payload length continued, if payload len == 127  |
 *   + - - - - - - - - - - - - - - - +-------------------------------+
 *   |                               |Masking-key, if MASK set to 1  |
 *   +-------------------------------+-------------------------------+
 *   | Masking-key (continued)       |          Payload Data         |
 *   +-------------------------------- - - - - - - - - - - - - - - - +
 *   :                     Payload Data continued ...                :
 *   + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
 *   |                     Payload Data continued ...                |
 *   +---------------------------------------------------------------+
 *
 * Using the decode buffer, this function:
 *  - reads the complete header from the underlying socket
 *  - reads any remaining data bytes
 *  - unmasks the payload data using the provided mask
 *  - decodes Base64 encoded text data
 *  - copies len bytes of decoded payload data into dst
 *
 * Emulates a read call on a socket.
 */
static int
webSocketsDecodeHybi(rfbClientPtr cl, char *dst, int len)
{
    int result = -1;
    ws_ctx_t *wsctx = (ws_ctx_t *)cl->wsctx;
    /* int fin; */ /* not used atm */

    /* rfbLog(" <== %s[%d]: %d cl: %p, wsctx: %p-%p (%d)\n", __func__, gettid(), len, cl, wsctx, (char *)wsctx + sizeof(ws_ctx_t), sizeof(ws_ctx_t)); */
    rfbLog("%s_enter: len=%d; "
                      "CTX: readlen=%d readPos=%p "
                      "writeTo=%p "
                      "state=%d toRead=%d remaining=%d "
                      " nReadRaw=%d carrylen=%d carryBuf=%p\n",
                      __func__, len,
                      wsctx->readlen, wsctx->readPos,
                      wsctx->writePos,
                      wsctx->hybiDecodeState, wsctx->nToRead, hybiRemaining(wsctx),
                      wsctx->nReadRaw, wsctx->carrylen, wsctx->carryBuf);

    switch (wsctx->hybiDecodeState){
      case WS_HYBI_STATE_HEADER_PENDING:
        wsctx->hybiDecodeState = hybiReadHeader(cl, &result);
        if (wsctx->hybiDecodeState == WS_HYBI_STATE_ERR) {
          goto spor;
        }
        if (wsctx->hybiDecodeState != WS_HYBI_STATE_HEADER_PENDING) {

          /* when header is complete, try to read some more data */
          wsctx->hybiDecodeState = hybiReadAndDecode(cl, dst, len, &result);
        }
        break;
      case WS_HYBI_STATE_DATA_AVAILABLE:
        wsctx->hybiDecodeState = hybiReturnData(dst, len, wsctx, &result);
        break;
      case WS_HYBI_STATE_DATA_NEEDED:
        wsctx->hybiDecodeState = hybiReadAndDecode(cl, dst, len, &result);
        break;
      case WS_HYBI_STATE_CLOSE_REASON_PENDING:
        wsctx->hybiDecodeState = hybiReadAndDecode(cl, dst, len, &result);
        break;
      default:
        /* invalid state */
        rfbErr("%s: called with invalid state %d\n", wsctx->hybiDecodeState);
        result = -1;
        errno = EIO;
        wsctx->hybiDecodeState = WS_HYBI_STATE_ERR;
    }

    /* single point of return, if someone has questions :-) */
spor:
    /* rfbLog("%s: ret: %d/%d\n", __func__, result, len); */
    if (wsctx->hybiDecodeState == WS_HYBI_STATE_FRAME_COMPLETE) {
      rfbLog("frame received successfully, cleaning up: read=%d hlen=%d plen=%d\n", wsctx->header.nRead, wsctx->header.headerLen, wsctx->header.payloadLen);
      /* frame finished, cleanup state */
      hybiDecodeCleanup(wsctx);
    } else if (wsctx->hybiDecodeState == WS_HYBI_STATE_ERR) {
      hybiDecodeCleanup(wsctx);
    }
    rfbLog("%s_exit: len=%d; "
                      "CTX: readlen=%d readPos=%p "
                      "writePos=%p "
                      "state=%d toRead=%d remaining=%d "
                      "nRead=%d carrylen=%d carryBuf=%p "
                      "result=%d\n",
                      __func__, len,
                      wsctx->readlen, wsctx->readPos,
                      wsctx->writePos,
                      wsctx->hybiDecodeState, wsctx->nToRead, hybiRemaining(wsctx),
                      wsctx->nReadRaw, wsctx->carrylen, wsctx->carryBuf,
                      result);
    return result;
}

static int
webSocketsEncodeHybi(rfbClientPtr cl, const char *src, int len, char **dst)
{
    int blen, ret = -1, sz = 0;
    unsigned char opcode = '\0'; /* TODO: option! */
    ws_header_t *header;
    ws_ctx_t *wsctx = (ws_ctx_t *)cl->wsctx;


    /* Optional opcode:
     *   0x0 - continuation
     *   0x1 - text frame (base64 encode buf)
     *   0x2 - binary frame (use raw buf)
     *   0x8 - connection close
     *   0x9 - ping
     *   0xA - pong
    **/
    if (!len) {
	  /* nothing to encode */
	  return 0;
    }

    header = (ws_header_t *)wsctx->codeBufEncode;

    if (wsctx->base64) {
	opcode = WS_OPCODE_TEXT_FRAME;
	/* calculate the resulting size */
	blen = B64LEN(len);
    } else {
	opcode = WS_OPCODE_BINARY_FRAME;
	blen = len;
    }

    header->b0 = 0x80 | (opcode & 0x0f);
    if (blen <= 125) {
      header->b1 = (uint8_t)blen;
      sz = 2;
    } else if (blen <= 65536) {
      header->b1 = 0x7e;
      header->u.s16.l16 = WS_HTON16((uint16_t)blen);
      sz = 4;
    } else {
      header->b1 = 0x7f;
      header->u.s64.l64 = WS_HTON64(blen);
      sz = 10;
    }

    if (wsctx->base64) {
        if (-1 == (ret = b64_ntop((unsigned char *)src, len, wsctx->codeBufEncode + sz, sizeof(wsctx->codeBufEncode) - sz))) {
	  rfbErr("%s: Base 64 encode failed\n", __func__);
	} else {
	  if (ret != blen)
	    rfbErr("%s: Base 64 encode; something weird happened\n", __func__);
	  ret += sz;
	}
    } else {
      memcpy(wsctx->codeBufEncode + sz, src, len);
      ret =  sz + len;
    }

    *dst = wsctx->codeBufEncode;

    return ret;
}

int
webSocketsEncode(rfbClientPtr cl, const char *src, int len, char **dst)
{
    return ((ws_ctx_t *)cl->wsctx)->encode(cl, src, len, dst);
}

int
webSocketsDecode(rfbClientPtr cl, char *dst, int len)
{
    return ((ws_ctx_t *)cl->wsctx)->decode(cl, dst, len);
}


/* returns TRUE if client sent a close frame or a single 'end of frame'
 * marker was received, FALSE otherwise
 *
 * Note: This is a Hixie-only hack!
 **/
rfbBool
webSocketCheckDisconnect(rfbClientPtr cl)
{
    ws_ctx_t *wsctx = (ws_ctx_t *)cl->wsctx;
    /* With Base64 encoding we need at least 4 bytes */
    char peekbuf[4];
    int n;

    if (wsctx->version == WEBSOCKETS_VERSION_HYBI)
	return FALSE;

    if (cl->sslctx)
	n = rfbssl_peek(cl, peekbuf, 4);
    else
	n = recv(cl->sock, peekbuf, 4, MSG_PEEK);

    if (n <= 0) {
	if (n != 0)
	    rfbErr("%s: peek; %m", __func__);
	rfbCloseClient(cl);
	return TRUE;
    }

    if (peekbuf[0] == '\xff') {
	int doclose = 0;
	/* Make sure we don't miss a client disconnect on an end frame
	 * marker. Because we use a peek buffer in some cases it is not
	 * applicable to wait for more data per select(). */
	switch (n) {
	    case 3:
		if (peekbuf[1] == '\xff' && peekbuf[2] == '\x00')
		    doclose = 1;
		break;
	    case 2:
		if (peekbuf[1] == '\x00')
		    doclose = 1;
		break;
	    default:
		return FALSE;
	}

	if (cl->sslctx)
	    n = rfbssl_read(cl, peekbuf, n);
	else
	    n = read(cl->sock, peekbuf, n);

	if (doclose) {
	    rfbErr("%s: websocket close frame received\n", __func__);
	    rfbCloseClient(cl);
	}
	return TRUE;
    }
    return FALSE;
}

/* returns TRUE if there is data waiting to be read in our internal buffer
 * or if is there any pending data in the buffer of the SSL implementation
 */
rfbBool
webSocketsHasDataInBuffer(rfbClientPtr cl)
{
    ws_ctx_t *wsctx = (ws_ctx_t *)cl->wsctx;

    if (wsctx && wsctx->readlen)
      return TRUE;

    return (cl->sslctx && rfbssl_pending(cl) > 0);
}
