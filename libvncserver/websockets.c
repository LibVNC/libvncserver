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
#include "ws_decode.h"


#if 0
#include <sys/syscall.h>
static int gettid() {
    return (int)syscall(SYS_gettid);
}
#endif

#define FLASH_POLICY_RESPONSE "<cross-domain-policy><allow-access-from domain=\"*\" to-ports=\"*\" /></cross-domain-policy>\n"
#define SZ_FLASH_POLICY_RESPONSE 93

/*
 * draft-ietf-hybi-thewebsocketprotocol-10
 * 5.2.2. Sending the Server's Opening Handshake
 */
#define GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

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

static int webSocketsEncodeHybi(ws_ctx_t *ctx, const char *src, int len);

static size_t ws_read(void *cl, char *buf, size_t len);

static size_t ws_write(void *cl, char *buf, size_t len);

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


void
wsEncodeCleanup(ws_encoding_ctx_t *ctx)
{
    wsHeaderCleanup(&(ctx->header));
    ctx->state = WS_STATE_ENCODING_IDLE;
    ctx->readPos = ctx->codeBufEncode;
    ctx->nToWrite = 0;
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
            if (n == 0) {
                rfbLog("webSocketsHandshake: client gone\n");
            } else {
                rfbLogPerror("webSocketsHandshake: read");
            }
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
                        if (n == 0) {
                            rfbLog("webSocketsHandshake: client gone\n");
                        } else {
                            rfbLogPerror("webSocketsHandshake: read");
                        }
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

    /* older hixie handshake, this could be removed if
     * a final standard is established -- removed now */
    if (!sec_ws_version) {
        rfbErr("Hixie no longer supported\n");
        free(response);
        free(buf);
        return FALSE;
    }

    if (!(path && host && (origin || sec_ws_origin))) {
        rfbErr("webSocketsHandshake: incomplete client handshake\n");
        free(response);
        free(buf);
        return FALSE;
    }

    if ((protocol) && (strstr(protocol, "binary"))) {
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
    char accept[B64LEN(SHA1_HASH_SIZE) + 1];
    rfbLog("  - WebSockets client version hybi-%02d\n", sec_ws_version);
    webSocketsGenSha1Key(accept, sizeof(accept), sec_ws_key);

    if(strlen(protocol) > 0) {
        len = snprintf(response, WEBSOCKETS_MAX_HANDSHAKE_LEN,
                 SERVER_HANDSHAKE_HYBI, accept, protocol);
    } else {
        len = snprintf(response, WEBSOCKETS_MAX_HANDSHAKE_LEN,
                       SERVER_HANDSHAKE_HYBI_NO_PROTOCOL, accept);
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
    wsctx->encode = webSocketsEncodeHybi;
    wsctx->decode = webSocketsDecodeHybi;
    wsctx->ctxInfo.readFunc = ws_read;
    wsctx->base64 = base64;
    hybiDecodeCleanupComplete(&(wsctx->dec));
    wsEncodeCleanup(&(wsctx->enc));
    cl->wsctx = (wsCtx *)wsctx;
    return TRUE;
}

static size_t
ws_read(void *ctxPtr, char *buf, size_t len)
{
    int n;
    rfbClientPtr cl = ctxPtr;
    if (cl->sslctx) {
        n = rfbssl_read(cl, buf, len);
    } else {
        n = read(cl->sock, buf, len);
    }
    return n;
}

static size_t
ws_write(void *ctxPtr, char *buf, size_t len)
{
  int n;
  rfbClientPtr cl = ctxPtr;
  if (cl->sslctx) {
    n = rfbssl_write(cl, buf, len);
  } else {
    n = write(cl->sock, buf, len);
  }
  return n;
}

static size_t
encodeSockTotal(ws_encoding_ctx_t *ctx)
{
    return ctx->header.headerLen + ctx->header.payloadLen;
}

static size_t
encodeSockRemaining(ws_encoding_ctx_t *ctx)
{
    return encodeSockTotal(ctx) - (ctx->readPos - ctx->codeBufEncode);
}

static size_t
encodeSockWritten(ws_encoding_ctx_t *ctx)
{
  return ctx->readPos - ctx->codeBufEncode;
}

static size_t
encodeWritten(ws_encoding_ctx_t *ctx, int base64)
{
    size_t nSockWritten = encodeSockWritten(ctx);
    rfbLog("%s: nSockWritten=%d\n", __func__, nSockWritten);
    if (nSockWritten <= ctx->header.headerLen) {
      return 0;
    } else {
        if (base64) {
            size_t ret = B64_ENCODABLE_WITH_BUF_SIZE(nSockWritten - ctx->header.headerLen);
            /* the last 4 bytes may encode 1, 2 or 3 bytes paylaod;
             * check with the original number of bytes in that case */
            ret = ret > ctx->nToWrite ? ctx->nToWrite : ret;
            return ret;
        } else {
            return nSockWritten - ctx->header.headerLen;
        }
    }
}


static size_t
encodeRemaining(ws_encoding_ctx_t *ctx, int base64)
{
  size_t nSockWritten = encodeSockTotal(ctx) - encodeSockRemaining(ctx);
  if (nSockWritten <= ctx->header.headerLen) {
    if (base64) {
        return B64_ENCODABLE_WITH_BUF_SIZE(ctx->header.payloadLen);
    } else {
        return ctx->header.payloadLen;
    }
  } else {
    if (base64) {
        return ctx->nToWrite - encodeWritten(ctx, base64);
    } else {
        return encodeSockRemaining(ctx);
    }
  }
}
/**
 * We encode a write request as a single websocket frame,
 * as long as our buffer can handle it. When the buffer is too large,
 * we take as many bytes as we can handle, put them into a websocket frame
 * and return the length of the raw payload data written to the underlying
 * socket.
 *
 * If the underlying socket suspends writing in the middle of a b64 encoding,
 * we return the completely written bytes and remember the position we stopped.
 * The next call continues writing bytes at this position.
 */
static int
webSocketsEncodeHybi(ws_ctx_t *wsctx, const char *src, int len)
{
    int framePayloadLen;
    int nSock = -1;
    int n = 0;
    int ret = 0;
    size_t nWritten = 0;
    int toEncode = len;
    unsigned char opcode = '\0'; /* TODO: option! */
    ws_encoding_ctx_t *enc_ctx = &(wsctx->enc);

    rfbLog("%s: src=%p len=%d\n", __func__, src, len);
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

    if (enc_ctx->state == WS_STATE_ENCODING_IDLE) {
        int nMax = 0;
        /* create websocket frame header */
        enc_ctx->header.data = (ws_header_t *)enc_ctx->codeBufEncode;
        ws_header_t *header = enc_ctx->header.data;

        if (wsctx->base64) {
            opcode = WS_OPCODE_TEXT_FRAME;

            /* for simplicity, assume maximum header length here */
            nMax = B64_ENCODABLE_WITH_BUF_SIZE(ARRAYSIZE(enc_ctx->codeBufEncode) - WS_HYBI_HEADER_LEN_LONG_NOTMASKED);

            /* calculate the resulting size, but make sure it fits the buffer */
            if (len > nMax) {
                framePayloadLen = B64LEN(nMax);
                toEncode = nMax;
            } else {
                framePayloadLen = B64LEN(len);
                toEncode = len;
            }
        } else {
            opcode = WS_OPCODE_BINARY_FRAME;
            nMax = ARRAYSIZE(enc_ctx->codeBufEncode) - WS_HYBI_HEADER_LEN_LONG_NOTMASKED;
            toEncode = len > nMax ? nMax : len;
            framePayloadLen = toEncode;
        }

        enc_ctx->nToWrite = toEncode;
        enc_ctx->header.payloadLen = framePayloadLen;
        header->b0 = 0x80 | (opcode & 0x0f);
        if (framePayloadLen <= 125) {
          header->b1 = (uint8_t)framePayloadLen;
          enc_ctx->header.headerLen = WS_HYBI_HEADER_LEN_SHORT_NOTMASKED;
        } else if (framePayloadLen < 65536) {
          header->b1 = 0x7e;
          header->u.s16.l16 = WS_HTON16((uint16_t)framePayloadLen);
          enc_ctx->header.headerLen = WS_HYBI_HEADER_LEN_EXTENDED_NOTMASKED;
        } else {
          header->b1 = 0x7f;
          header->u.s64.l64 = WS_HTON64(framePayloadLen);
          enc_ctx->header.headerLen = WS_HYBI_HEADER_LEN_LONG_NOTMASKED;
        }

        if (wsctx->base64) {
            rfbLog("%s: trying to encode %d bytes into encode buffer of size %d, framePayloadLen=%d\n", __func__, toEncode, ARRAYSIZE(enc_ctx->codeBufEncode), framePayloadLen);
            if (-1 == (nSock = b64_ntop((unsigned char *)src, toEncode, enc_ctx->codeBufEncode + enc_ctx->header.headerLen, ARRAYSIZE(enc_ctx->codeBufEncode) - enc_ctx->header.headerLen))) {
                rfbErr("%s: Base 64 encode failed\n", __func__);
            } else {
              if (nSock != framePayloadLen) {
                rfbErr("%s: Base 64 encode; something weird happened\n", __func__);
              }
              rfbLog("%s: encoded %d source bytes to %d b64 bytes\n", __func__, toEncode, nSock);
              nSock += enc_ctx->header.headerLen;
            }
        } else {
            memcpy(enc_ctx->codeBufEncode + enc_ctx->header.headerLen, src, framePayloadLen);
            nSock =  enc_ctx->header.headerLen + framePayloadLen;
        }

        while (nWritten < enc_ctx->header.headerLen + B64LEN(1)) {
            n = wsctx->ctxInfo.writeFunc(wsctx->ctxInfo.ctxPtr, enc_ctx->codeBufEncode + nWritten, nSock - nWritten);
            if (n < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                  int olderrno = errno;
                  rfbErr("%s: writing to sock caused err; returning it\n", __func__);
                  errno = olderrno;
                  return -1;
                }
            } else {
                nWritten += n;
            }
            rfbLog("%s: written %d bytes to sock; nWritten=%d, ret=%d, remaining=%d\n", __func__, n, nWritten, nSock, nSock - nWritten);
        }
        enc_ctx->readPos = enc_ctx->codeBufEncode + nWritten;
        ret = encodeWritten(enc_ctx, wsctx->base64);
        rfbLog("%s: write in state %d (IDLE); nWritten=%d ret=%d\n", __func__, enc_ctx->state, nWritten, ret);
    } else if (enc_ctx->state == WS_STATE_ENCODING_FRAME_PENDING) {
        int nRemaining = encodeRemaining(enc_ctx, wsctx->base64);

        do {
          /* write from where we left until the end of a frame */
          n = wsctx->ctxInfo.writeFunc(wsctx->ctxInfo.ctxPtr, enc_ctx->readPos, encodeSockRemaining(enc_ctx));
          if (n < 0) {
            int olderrno = errno;
            rfbErr("%s: failed writing to socket\n");
            errno = olderrno;
            return -1;
          }
          enc_ctx->readPos += n;
          ret = nRemaining - encodeRemaining(enc_ctx, wsctx->base64);
          rfbLog("%s: wrote %d bytes to socket; ret=%d nRemaining=%d encodeRemaining=%d\n", __func__, n, ret, nRemaining, encodeRemaining(enc_ctx, wsctx->base64));
        } while (ret < 1);
        rfbLog("%s: write in state %d; nRemaining=%d n=%d ret=%d\n", __func__, enc_ctx->state, nRemaining, n, ret);
    } else {
        rfbErr("%s: invalid state (%d)\n", __func__, enc_ctx->state);
        errno = EIO;
        return -1;
    }
    int bytesRemaining = encodeSockRemaining(enc_ctx);
    /* check if we are finished tranmitting the whole frame */
    if (bytesRemaining == 0) {
      rfbLog("%s: transmission finished; cleaning up\n", __func__);
      wsEncodeCleanup(enc_ctx);
    } else {
      rfbLog("%s: %d bytes remaining\n", __func__, bytesRemaining);
      enc_ctx->state = WS_STATE_ENCODING_FRAME_PENDING;
    }

    rfbLog("%s: returning %d nextState=%d\n", __func__, ret, enc_ctx->state);
    return ret;
}

int
webSocketsEncode(rfbClientPtr cl, const char *src, int len)
{
    ws_ctx_t *wsctx = (ws_ctx_t *)cl->wsctx;
    if (wsctx == NULL) {
      rfbErr("%s: websocket used uninitialized\n", __func__);
      errno = EIO;
      return -1;
    }
    wsctx->ctxInfo.ctxPtr = cl;
    wsctx->ctxInfo.writeFunc = ws_write;
    return webSocketsEncodeHybi(wsctx, src, len);
}

int
webSocketsDecode(rfbClientPtr cl, char *dst, int len)
{
    ws_ctx_t *wsctx = (ws_ctx_t *)cl->wsctx;
    wsctx->ctxInfo.ctxPtr = cl;
    wsctx->ctxInfo.readFunc = ws_read;
    return webSocketsDecodeHybi(wsctx, dst, len);
}

/* returns TRUE if there is data waiting to be read in our internal buffer
 * or if is there any pending data in the buffer of the SSL implementation
 */
rfbBool
webSocketsHasDataInBuffer(rfbClientPtr cl)
{
    ws_ctx_t *wsctx = (ws_ctx_t *)cl->wsctx;

    if (wsctx && wsctx->dec.readlen)
        return TRUE;

    return (cl->sslctx && rfbssl_pending(cl) > 0);
}
