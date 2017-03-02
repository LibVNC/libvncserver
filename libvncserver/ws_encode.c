/*
 * ws_encode.c - encoding of a raw data byte stream into websocket frames.
 *
 * This code should be independent of any changes in the RFB protocol. It is
 * an additional handshake and framing of normal sockets:
 *   http://www.whatwg.org/specs/web-socket-protocol/
 *
 */

/*
 *  Copyright (C) 2010-2017 Joel Martin, Andreas Weigel
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


#include "websockets.h"
#include <errno.h>
#include <string.h>

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


void
wsEncodeCleanup(ws_encoding_ctx_t *ctx)
{
    wsHeaderCleanup(&(ctx->header));
    ctx->state = WS_STATE_ENCODING_IDLE;
    ctx->readPos = ctx->codeBufEncode;
    ctx->nToWrite = 0;
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
int
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


