/*
 * websockets.h - constants, macros and structures for websocket implementation
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

#ifndef _WS_DECODE_H_
#define _WS_DECODE_H_

#include <stdint.h>
#include <rfb/rfb.h>
#ifndef _MSC_VER
#include <resolv.h> /* __b64_ntop */
#endif

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

#define B64LEN(__x) ((((__x) + 2) / 3) * 12 / 3)
#define B64_ENCODABLE_WITH_BUF_SIZE(__x) (((__x) / 4) * 3)

#define WS_HYBI_MASK_LEN 4
#define WS_HYBI_HEADER_LEN_SHORT_MASKED 2 + WS_HYBI_MASK_LEN
#define WS_HYBI_HEADER_LEN_EXTENDED_MASKED 4 + WS_HYBI_MASK_LEN
#define WS_HYBI_HEADER_LEN_LONG_MASKED 10 + WS_HYBI_MASK_LEN
#define WS_HYBI_HEADER_LEN_SHORT_NOTMASKED 2 
#define WS_HYBI_HEADER_LEN_EXTENDED_NOTMASKED 4 
#define WS_HYBI_HEADER_LEN_LONG_NOTMASKED 10 

#define WSHLENMAX WS_HYBI_HEADER_LEN_LONG_MASKED /* 2 + sizeof(uint64_t) + sizeof(uint32_t) */

#define ARRAYSIZE(a) ((sizeof(a) / sizeof((a[0]))) / (size_t)(!(sizeof(a) % sizeof((a[0])))))

struct ws_ctx_s;
typedef struct ws_ctx_s ws_ctx_t;

typedef int (*wsEncodeFunc)(ws_ctx_t *wsctx, const char *src, int len);
typedef int (*wsDecodeFunc)(ws_ctx_t *wsctx, char *dst, int len);

typedef size_t (*wsReadFunc)(void *ctx, char *dst, size_t len);
typedef size_t (*wsWriteFunc)(void *ctx, char *dst, size_t len);

typedef struct ctxInfo_s{
  void *ctxPtr;
  wsReadFunc readFunc;
  wsWriteFunc writeFunc;
} ctxInfo_t;

enum {
  /* header not yet received completely */
  WS_STATE_DECODING_HEADER_PENDING,
  /* data available */
  WS_STATE_DECODING_DATA_AVAILABLE,
  WS_STATE_DECODING_DATA_NEEDED,
  /* received a complete frame */
  WS_STATE_DECODING_FRAME_COMPLETE,
  /* received part of a 'close' frame */
  WS_STATE_DECODING_CLOSE_REASON_PENDING,
  /* */
  WS_STATE_ERR,
  /* clean state, no frame in transition */
  WS_STATE_ENCODING_IDLE,
  /* started a frame, underlying socket did not transmit everything */
  WS_STATE_ENCODING_FRAME_PENDING,
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
  int nDone;
  /** mask value */
  ws_mask_t mask;
  /** length of frame header including payload len, but without mask */
  int headerLen;
  /** length of the payload data */
  uint64_t payloadLen;
  /** opcode */
  unsigned char opcode;
  /** fin bit */
  unsigned char fin;
} ws_header_data_t;

typedef struct ws_encoding_ctx_s {
    /* encoding state */
    char codeBufEncode[B64LEN(UPDATE_BUF_SIZE) + WSHLENMAX]; /* base64 + maximum frame header length */
    int state;
    char *readPos;
    int nToWrite;
    ws_header_data_t header;
} ws_encoding_ctx_t; 

typedef struct ws_decoing_ctx_s {
    char codeBufDecode[2048 + WSHLENMAX]; /* base64 + maximum frame header length */
    char *writePos;
    unsigned char *readPos;
    int readlen;
    int state;
    char carryBuf[3];                      /* For base64 carry-over */
    int carrylen;
    ws_header_data_t header;
    uint64_t nReadPayload;
    unsigned char continuation_opcode;
} ws_decoding_ctx_t;

typedef struct ws_ctx_s {
    ws_decoding_ctx_t dec;
    ws_encoding_ctx_t enc;
    int base64;
    ctxInfo_t ctxInfo;
} ws_ctx_t;

enum
{
    WS_OPCODE_CONTINUATION = 0x00,
    WS_OPCODE_TEXT_FRAME = 0x01,
    WS_OPCODE_BINARY_FRAME = 0x02,
    WS_OPCODE_CLOSE = 0x08,
    WS_OPCODE_PING = 0x09,
    WS_OPCODE_PONG = 0x0A,
    WS_OPCODE_INVALID = 0xFF
};

int webSocketsDecode(ws_ctx_t *wsctx, char *dst, int len);

int webSocketsEncode(ws_ctx_t *ctx, const char *src, int len);

void wsDecodeCleanupComplete(ws_decoding_ctx_t *wsctx);

void wsEncodeCleanup(ws_encoding_ctx_t *wsctx);

void wsHeaderCleanup(ws_header_data_t* header);
#endif
