/*
 * Copyright (C)2017 Andreas Weigel.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS",
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _WIN32

#include <websockets.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>

/* incoming data frames should not be larger than that */
#define TEST_BUF_SIZE B64LEN(131072) + WSHLENMAX

/* seed is fixed deliberately to get reproducible test cases */
#define RND_SEED 100

enum {
  OK,
  FAIL_DATA,
  FAIL_ERRNO,
  FAIL_CLOSED,
  FAIL_INVALID
};

const char *result_descr[] = {
  "",
  "Data buffers do not match",
  "Wrong errno",
  "Wrongly reported closed socket",
  "Internal test error"
};

struct ws_frame_test {
  char frame[TEST_BUF_SIZE];
  char *pos;
  char rawData[TEST_BUF_SIZE];
  uint64_t n_compare;
  uint64_t frame_len;
  uint64_t raw_payload_len;
  int expected_errno;
  const char *descr;
  int ret_bytes[16];
  int ret_bytes_len;
  int i;
  int simulate_sock_malfunction_at;
  int errno_val;
  int close_sock_at;
  int test_type;
  int enc_opcode;
};

#include "wstestdata.inc"

char el_log[1000000];
char *el_pos;

static void logtest(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  size_t left = el_log + sizeof(el_log) - el_pos;
  size_t off = vsnprintf(el_pos, left, fmt, args);
  el_pos += off;
  va_end(args);
}

static size_t emu_read(void *ctx, char *dst, size_t len);
static size_t emu_write(void *ctx, char *src, size_t len);

static size_t emu_read(void *ctx, char *dst, size_t len)
{
  struct ws_frame_test *ft = (struct ws_frame_test *)ctx;
  ssize_t nret;
  int r;
  ssize_t modu;

  rfbLog("%s: dst=%p and len=%lu\n", __func__, dst, len);
  if (ft->simulate_sock_malfunction_at > 0 && ft->simulate_sock_malfunction_at == ft->i) {
    rfbLog("%s: simulating IO error with errno=%d\n", __func__, ft->errno_val);
    errno = ft->errno_val;
    return -1;
  }

  /* return something */
  r = rand();
  modu = (ft->frame + ft->frame_len) - ft->pos;
  rfbLog("%s: r=%d modu=%ld frame=%p pos=%p\n", __func__, r, modu, ft->frame, ft->pos);
  nret = (r % modu) + 1;
  nret = nret > len ? len : nret;

  rfbLog("%s: copy and return %ld bytes\n", __func__, nret);
  memcpy(dst, ft->pos, nret);
  ft->pos += nret;
  rfbLog("%s: leaving; pos=%p framebuf=%p nret=%ld\n", __func__, ft->pos, ft->frame, nret);
  return nret;
}

static size_t emu_write(void *ctx, char *src, size_t len)
{
  struct ws_frame_test *ft = (struct ws_frame_test *)ctx;

  ssize_t nret;
  int r;
  ssize_t modu;

  rfbLog("%s: with src=%p and len=%lu; dst=%p\n", __func__, src, len, ft->pos);
  if (ft->simulate_sock_malfunction_at > 0 && ft->simulate_sock_malfunction_at == ft->i) {
    rfbLog("%s: simulating sock error with errno=%d\n", __func__, ft->errno_val);
    errno = ft->errno_val;
    return -1;
  }

  r = rand();
  nret = (r % len) + 1;
  rfbLog("%s: write %ld bytes\n", __func__, nret);
  memcpy(ft->pos, src, nret);
  ft->pos += nret;
  rfbLog("%s: leaving; pos=%p src=%p nret=%ld\n", __func__, ft->pos, src, nret);
  return nret;
}

static int run_decode_test(struct ws_frame_test *ft, ws_ctx_t *ctx)
{
  uint64_t nleft = ft->raw_payload_len;
  char dstbuf[ft->raw_payload_len];
  char *dst = dstbuf;
  ssize_t n;

  ft->pos = ft->frame;

  ctx->ctxInfo.ctxPtr = (void *)ft;

  while (nleft > 0) {
    rfbLog("calling ws_decode with dst=%p, len=%lu\n", dst, nleft);
    n = _webSocketsDecode(ctx, dst, nleft);
    rfbLog("read n=%ld\n", n);
    if (n == 0) {
      if (ft->close_sock_at > 0) {
        return OK;
      } else {
        return FAIL_CLOSED;
      }
    } else if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        /* ok, just call again */
      } else {
        if (ft->expected_errno == errno) {
          rfbLog("errno=%d as expected\n", errno);
          return OK;
        } else {
          rfbLog("errno=%d != expected(%d)\n", errno, ft->expected_errno);
          return FAIL_ERRNO;
        }
      }
    } else {
      nleft -= n;
      dst += n;
      rfbLog("read n=%ld from decode; dst=%p, nleft=%lu\n", n, dst, nleft);
    }
  }

  if (memcmp(ft->rawData, dstbuf, ft->raw_payload_len) != 0) {
    ft->rawData[ft->raw_payload_len] = '\0';
    dstbuf[ft->raw_payload_len] = '\0';
    rfbLog("decoded result not equal:\nexpected:\n%s\ngot\n%s\n\n", ft->rawData, dstbuf);
    return FAIL_DATA;
  }

  return OK;
}

static int run_encode_test(struct ws_frame_test *ft, ws_ctx_t *ctx)
{
  int64_t nleft = ft->raw_payload_len;
  char expectedFrame[ft->frame_len];
  ssize_t n;
  char *src = ft->rawData;

  /* remember expected expectedFrameing websocket frame, and clear frame buffer;
   * emu_write  uses this buffer to encode the frame again */
  memcpy(expectedFrame, ft->frame, ft->frame_len);
  memset(ft->frame, 0, ft->frame_len);

  ft->pos = ft->frame;
  ctx->ctxInfo.ctxPtr = (void *)ft;

  if (ft->enc_opcode == 1) {
    ctx->base64 = 1;
  } else {
    ctx->base64 = 0;
  }
  rfbLog("set base64 to %d\n", ctx->base64);

  while (nleft > 0) {
    rfbLog("calling _webSocketsEncode with src=%p, len=%lu\n", src, nleft);
    n = _webSocketsEncode(ctx, src, nleft);
    rfbLog("written n=%ld\n", n);
    if (n == 0) {
      if (ft->close_sock_at > 0) {
        return OK;
      } else {
        return FAIL_CLOSED;
      }
    } else if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        /* ok, just call again */
      } else {
        if (ft->expected_errno == errno) {
          rfbLog("errno=%d as expected\n", errno);
          return OK;
        } else {
          rfbLog("errno=%d != expected(%d)\n", errno, ft->expected_errno);
          return FAIL_ERRNO;
        }
      }
    } else {
      nleft -= n;
      src += n;
      rfbLog("written n=%ld to websocketEncode; src=%p, nleft=%lu\n", n, src, nleft);
    }
  }
  if (nleft < 0) {
    rfbLog("_webSocketsEncode returned more than passed; nleft=%lld\n", nleft);
    return FAIL_INVALID;
  }

  if (memcmp(ft->frame, expectedFrame, ft->frame_len) != 0) {
    ft->frame[ft->frame_len] = '\0';
    expectedFrame[ft->frame_len] = '\0';
    rfbLog("ws frame not equal:\nexpected:\n%s\ngot\n%s\n\n", expectedFrame, ft->frame);
    return FAIL_DATA;
  }

  return OK;
}

static int run_test(struct ws_frame_test *ft, ws_ctx_t *ctx)
{
  if (ft->test_type == 0) {
    /* decode */
    return run_decode_test(ft, ctx);
  } else if (ft->test_type == 1) {
    /* encode */
    return run_encode_test(ft, ctx);
  }
  return FAIL_INVALID;
}


int main()
{
  ws_ctx_t ctx;
  int retall= 0;
  int i;
  srand(RND_SEED);

  wsDecodeCleanupComplete(&(ctx.dec));
  wsEncodeCleanup(&(ctx.enc));
  ctx.ctxInfo.readFunc = emu_read;
  ctx.ctxInfo.writeFunc = emu_write;
  rfbLog = logtest;
  rfbErr = logtest;

  for (i = 0; i < ARRAYSIZE(tests); i++) {
    int ret;

    /* reset output log buffer to begin */
    el_pos = el_log;

    ret = run_test(&tests[i], &ctx);
    printf("%s: \"%s\"\n", ret == 0 ? "PASS" : "FAIL", tests[i].descr);
    if (ret != 0) {
      *el_pos = '\0';
      printf("%s", el_log);
      retall = -1;
    }
  }
  return retall;
}

#else

int main() {
  return 0;
}

#endif
