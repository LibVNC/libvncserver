/*
 * h264.c - handle H.264 encoding.
 *
 * This file shouldn't be compiled directly. It is included multiple times by
 * rfbclient.c, each time with a different definition of the macro BPP.
 */

#ifdef LIBVNCSERVER_HAVE_LIBAVCODEC

#ifndef LIBVNCSERVER_HAVE_LIBSWSCALE
#error "H.264 support requires libswscale"
#endif

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/log.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>

#ifndef LIBVNCCLIENT_H264_COMMON
#define LIBVNCCLIENT_H264_COMMON

#define RFB_CLIENT_H264_MAX_CONTEXTS 64


typedef struct {
    rfbPixelFormat format;
    int destBpp;
    rfbBool valid;
    uint32_t redContribution[256];
    uint32_t greenContribution[256];
    uint32_t blueContribution[256];
    uint8_t greyContribution[256];
} rfbClientH264PackState;

typedef struct rfbClientH264Context {
    struct rfbClientH264Context *prev;
    struct rfbClientH264Context *next;
    int rx;
    int ry;
    int rw;
    int rh;
    struct AVCodecContext *decoder;
    struct AVFrame *frame;
    struct AVPacket *packet;
#ifdef LIBVNCSERVER_HAVE_LIBSWSCALE
    struct SwsContext *sws;
#endif
    uint8_t *bgraBuffer;
    size_t bgraBufferSize;
    rfbClientH264PackState packState;
    int lastFormat;
} rfbClientH264Context;

static void rfbClientH264InitLogging(void);
static void rfbClientH264AppendContext(rfbClient *client, rfbClientH264Context *ctx);
static void rfbClientH264DetachContext(rfbClient *client, rfbClientH264Context *ctx);
static rfbClientH264Context *rfbClientH264FindContext(rfbClient *client, int rx, int ry, int rw, int rh);
static rfbClientH264Context *rfbClientH264AcquireContext(rfbClient *client, int rx, int ry, int rw, int rh, rfbBool *created);
static void rfbClientH264ReleaseContext(rfbClient *client, rfbClientH264Context *ctx);
static void rfbClientH264ReleaseAllContexts(rfbClient *client);
static void rfbClientH264MoveContextToTail(rfbClient *client, rfbClientH264Context *ctx);
static void rfbClientH264TrimContexts(rfbClient *client);

static rfbBool rfbClientH264EnsureDecoder(rfbClient *client, rfbClientH264Context *ctx);
static rfbBool rfbClientH264EnsureBgraBuffer(rfbClientH264Context *ctx, int width, int height);
static rfbBool rfbClientH264EnsureRawBuffer(rfbClient *client, int width, int height, int destBpp);
static rfbBool rfbClientH264BlitFrame(rfbClient *client, rfbClientH264Context *ctx, AVFrame *frame,
                                      int rx, int ry, int rw, int rh, int destBpp);
static void rfbClientH264PackPixel(const rfbPixelFormat *fmt, uint8_t *dst,
                                   uint8_t r, uint8_t g, uint8_t b);
static void rfbClientH264PreparePackState(rfbClientH264Context *ctx,
                                          const rfbPixelFormat *fmt,
                                          int destBpp);

static void
rfbClientH264InitLogging(void)
{
    static rfbBool initialized = FALSE;
    if (!initialized) {
        av_log_set_level(AV_LOG_QUIET);
        initialized = TRUE;
    }
}

void
rfbClientH264ReleaseDecoder(rfbClient *client)
{
    if (client == NULL) {
        return;
    }

    rfbClientH264ReleaseAllContexts(client);
}

static void
rfbClientH264AppendContext(rfbClient *client, rfbClientH264Context *ctx)
{
    ctx->prev = client->h264ContextsTail;
    ctx->next = NULL;
    if (client->h264ContextsTail != NULL) {
        client->h264ContextsTail->next = ctx;
    } else {
        client->h264ContextsHead = ctx;
    }
    client->h264ContextsTail = ctx;
}

static void
rfbClientH264DetachContext(rfbClient *client, rfbClientH264Context *ctx)
{
    if (ctx->prev != NULL) {
        ctx->prev->next = ctx->next;
    } else {
        client->h264ContextsHead = ctx->next;
    }
    if (ctx->next != NULL) {
        ctx->next->prev = ctx->prev;
    } else {
        client->h264ContextsTail = ctx->prev;
    }
    ctx->prev = NULL;
    ctx->next = NULL;
}

static rfbClientH264Context *
rfbClientH264FindContext(rfbClient *client, int rx, int ry, int rw, int rh)
{
    rfbClientH264Context *ctx;

    for (ctx = client->h264ContextsHead; ctx != NULL; ctx = ctx->next) {
        if (ctx->rx == rx && ctx->ry == ry && ctx->rw == rw && ctx->rh == rh) {
            return ctx;
        }
    }

    return NULL;
}

static void
rfbClientH264ReleaseContext(rfbClient *client, rfbClientH264Context *ctx)
{
    if (ctx == NULL) {
        return;
    }

    rfbClientH264DetachContext(client, ctx);

    if (client->h264ContextCount > 0) {
        client->h264ContextCount--;
    }

    if (ctx->decoder != NULL) {
        avcodec_free_context(&ctx->decoder);
    }
    av_frame_free(&ctx->frame);
    av_packet_free(&ctx->packet);
#ifdef LIBVNCSERVER_HAVE_LIBSWSCALE
    if (ctx->sws != NULL) {
        sws_freeContext(ctx->sws);
    }
#endif
    free(ctx->bgraBuffer);

    free(ctx);
}

static void
rfbClientH264ReleaseAllContexts(rfbClient *client)
{
    rfbClientH264Context *ctx = client->h264ContextsHead;

    while (ctx != NULL) {
        rfbClientH264Context *next = ctx->next;
        rfbClientH264ReleaseContext(client, ctx);
        ctx = next;
    }

    client->h264ContextsHead = NULL;
    client->h264ContextsTail = NULL;
    client->h264ContextCount = 0;
}

static void
rfbClientH264MoveContextToTail(rfbClient *client, rfbClientH264Context *ctx)
{
    if (ctx == NULL || client->h264ContextsTail == ctx) {
        return;
    }

    rfbClientH264DetachContext(client, ctx);
    rfbClientH264AppendContext(client, ctx);
}

static void
rfbClientH264TrimContexts(rfbClient *client)
{
    while (client->h264ContextCount > RFB_CLIENT_H264_MAX_CONTEXTS && client->h264ContextsHead != NULL) {
        rfbClientH264ReleaseContext(client, client->h264ContextsHead);
    }
}

static rfbClientH264Context *
rfbClientH264AcquireContext(rfbClient *client, int rx, int ry, int rw, int rh, rfbBool *created)
{
    rfbClientH264Context *ctx = rfbClientH264FindContext(client, rx, ry, rw, rh);

    if (ctx != NULL) {
        if (created != NULL) {
            *created = FALSE;
        }
        rfbClientH264MoveContextToTail(client, ctx);
        return ctx;
    }

    ctx = (rfbClientH264Context *)calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        rfbClientLog("Failed to allocate H.264 context\n");
        return NULL;
    }

    ctx->rx = rx;
    ctx->ry = ry;
    ctx->rw = rw;
    ctx->rh = rh;
    ctx->lastFormat = -1;

    rfbClientH264AppendContext(client, ctx);
    client->h264ContextCount++;
    rfbClientH264TrimContexts(client);

    if (created != NULL) {
        *created = TRUE;
    }

    return ctx;
}

static rfbBool
rfbClientH264EnsureDecoder(rfbClient *client, rfbClientH264Context *ctx)
{
    rfbClientH264InitLogging();

    if (ctx->decoder == NULL) {
        const AVCodec *decoder = avcodec_find_decoder(AV_CODEC_ID_H264);
        if (decoder == NULL) {
            rfbClientLog("H.264 decoder not found\n");
            return FALSE;
        }

        ctx->decoder = avcodec_alloc_context3(decoder);
        if (ctx->decoder == NULL) {
            rfbClientLog("Failed to allocate H.264 decoder context\n");
            return FALSE;
        }

        ctx->decoder->thread_count = 1; /* Disable frame threading to avoid delayed output */
#ifdef FF_THREAD_SLICE
        ctx->decoder->thread_type = FF_THREAD_SLICE;
#else
        ctx->decoder->thread_type = 0;
#endif
        ctx->decoder->flags |= AV_CODEC_FLAG_LOW_DELAY;

        if (avcodec_open2(ctx->decoder, decoder, NULL) < 0) {
            rfbClientLog("Failed to open H.264 decoder\n");
            avcodec_free_context(&ctx->decoder);
            return FALSE;
        }
    }

    if (ctx->frame == NULL) {
        ctx->frame = av_frame_alloc();
        if (ctx->frame == NULL) {
            rfbClientLog("Failed to allocate H.264 frame\n");
            return FALSE;
        }
    }

    if (ctx->packet == NULL) {
        ctx->packet = av_packet_alloc();
        if (ctx->packet == NULL) {
            rfbClientLog("Failed to allocate H.264 packet\n");
            return FALSE;
        }
    }

    return TRUE;
}

static rfbBool
rfbClientH264EnsureBgraBuffer(rfbClientH264Context *ctx, int width, int height)
{
    if (width <= 0 || height <= 0) {
        return FALSE;
    }

    size_t pixels = (size_t)width * (size_t)height;
    if (pixels == 0 || pixels > SIZE_MAX / 4) {
        rfbClientLog("H.264 frame too large (%dx%d)\n", width, height);
        return FALSE;
    }

    size_t required = pixels * 4;
    if (ctx->bgraBufferSize < required) {
        uint8_t *newBuffer = (uint8_t *)realloc(ctx->bgraBuffer, required);
        if (newBuffer == NULL) {
            rfbClientLog("Failed to allocate %zu bytes for H.264 conversion buffer\n", required);
            return FALSE;
        }
        ctx->bgraBuffer = newBuffer;
        ctx->bgraBufferSize = required;
    }

    return TRUE;
}

static rfbBool
rfbClientH264EnsureRawBuffer(rfbClient *client, int width, int height, int destBpp)
{
    if (width <= 0 || height <= 0 || destBpp <= 0) {
        rfbClientLog("Invalid framebuffer dimensions for H.264 blit: %dx%d @ %d bpp\n",
                     width, height, destBpp);
        return FALSE;
    }

    size_t bytesPerPixel = (size_t)destBpp / 8;
    size_t pixels = (size_t)width * (size_t)height;

    if (pixels == 0 || bytesPerPixel == 0 || pixels > SIZE_MAX / bytesPerPixel) {
        rfbClientLog("Requested framebuffer update too large for H.264 (%dx%d)\n",
                     width, height);
        return FALSE;
    }

    size_t required = pixels * bytesPerPixel;
    if (required > (size_t)INT_MAX) {
        rfbClientLog("H.264 framebuffer update exceeds maximum buffer size\n");
        return FALSE;
    }

    if (client->raw_buffer_size < (int)required) {
        char *newBuffer = (char *)realloc(client->raw_buffer, required);
        if (newBuffer == NULL) {
            rfbClientLog("Failed to allocate %zu bytes for framebuffer buffer\n", required);
            return FALSE;
        }
        client->raw_buffer = newBuffer;
        client->raw_buffer_size = (int)required;
    }

    return TRUE;
}

static inline uint32_t
rfbClientH264ScaleComponent(uint8_t value, uint16_t max)
{
    if (max == 0) {
        return 0;
    }
    return (uint32_t)((value * (uint32_t)max + 127) / 255);
}


static void
rfbClientH264PreparePackState(rfbClientH264Context *ctx,
                              const rfbPixelFormat *fmt,
                              int destBpp)
{
    rfbClientH264PackState *state = &ctx->packState;

    if (state->valid && state->destBpp == destBpp &&
        memcmp(&state->format, fmt, sizeof(*fmt)) == 0) {
        return;
    }

    state->format = *fmt;
    state->destBpp = destBpp;

    int v;
    for (v = 0; v < 256; ++v) {
        uint8_t component = (uint8_t)v;
        uint32_t rScaled = rfbClientH264ScaleComponent(component, fmt->redMax);
        uint32_t gScaled = rfbClientH264ScaleComponent(component, fmt->greenMax);
        uint32_t bScaled = rfbClientH264ScaleComponent(component, fmt->blueMax);

        state->redContribution[v] = rScaled << fmt->redShift;
        state->greenContribution[v] = gScaled << fmt->greenShift;
        state->blueContribution[v] = bScaled << fmt->blueShift;
        state->greyContribution[v] = (uint8_t)rfbClientH264ScaleComponent(component, fmt->redMax);
    }

    state->valid = TRUE;
}

static void
rfbClientH264PackPixel(const rfbPixelFormat *fmt, uint8_t *dst,
                       uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t rScaled = rfbClientH264ScaleComponent(r, fmt->redMax);
    uint32_t gScaled = rfbClientH264ScaleComponent(g, fmt->greenMax);
    uint32_t bScaled = rfbClientH264ScaleComponent(b, fmt->blueMax);

    switch (fmt->bitsPerPixel) {
    case 32: {
        uint32_t pixel = (rScaled << fmt->redShift) |
                         (gScaled << fmt->greenShift) |
                         (bScaled << fmt->blueShift);
        if (fmt->bigEndian) {
            dst[0] = (uint8_t)((pixel >> 24) & 0xFF);
            dst[1] = (uint8_t)((pixel >> 16) & 0xFF);
            dst[2] = (uint8_t)((pixel >> 8) & 0xFF);
            dst[3] = (uint8_t)(pixel & 0xFF);
        } else {
            dst[0] = (uint8_t)(pixel & 0xFF);
            dst[1] = (uint8_t)((pixel >> 8) & 0xFF);
            dst[2] = (uint8_t)((pixel >> 16) & 0xFF);
            dst[3] = (uint8_t)((pixel >> 24) & 0xFF);
        }
        break;
    }
    case 16: {
        uint16_t pixel = (uint16_t)((rScaled << fmt->redShift) |
                                    (gScaled << fmt->greenShift) |
                                    (bScaled << fmt->blueShift));
        if (fmt->bigEndian) {
            dst[0] = (uint8_t)((pixel >> 8) & 0xFF);
            dst[1] = (uint8_t)(pixel & 0xFF);
        } else {
            dst[0] = (uint8_t)(pixel & 0xFF);
            dst[1] = (uint8_t)((pixel >> 8) & 0xFF);
        }
        break;
    }
    case 8:
        *dst = (uint8_t)rfbClientH264ScaleComponent(r, fmt->redMax);
        break;
    default:
        break;
    }
}


static rfbBool
rfbClientH264BlitFrame(rfbClient *client, rfbClientH264Context *ctx, AVFrame *frame,
                       int rx, int ry, int rw, int rh, int destBpp)
{
    if (!rfbClientH264EnsureBgraBuffer(ctx, frame->width, frame->height)) {
        return FALSE;
    }

#ifdef LIBVNCSERVER_HAVE_LIBSWSCALE
    struct SwsContext *sws = sws_getCachedContext(ctx->sws,
                                                 frame->width, frame->height,
                                                 (enum AVPixelFormat)frame->format,
                                                 frame->width, frame->height,
                                                 AV_PIX_FMT_BGRA,
                                                 SWS_BILINEAR, NULL, NULL, NULL);
    if (sws == NULL) {
        rfbClientLog("Failed to create H.264 colorspace converter\n");
        return FALSE;
    }
    ctx->sws = sws;

    uint8_t *destData[4] = { ctx->bgraBuffer, NULL, NULL, NULL };
    int destLinesize[4] = { frame->width * 4, 0, 0, 0 };

    int scaled = sws_scale(sws, (const uint8_t * const *)frame->data,
                           frame->linesize, 0, frame->height,
                           destData, destLinesize);
    if (scaled < frame->height) {
        rfbClientLog("H.264 conversion produced %d lines (expected %d)\n",
                     scaled, frame->height);
        return FALSE;
    }
#else
    (void)ctx;
#endif

    if (!rfbClientH264EnsureRawBuffer(client, frame->width, frame->height, destBpp)) {
        return FALSE;
    }

    if (!client->format.trueColour) {
        rfbClientLog("H.264 encoding requires true-colour pixel format\n");
        return FALSE;
    }

    if (frame->width != rw || frame->height != rh) {
        rfbClientLog("H.264 frame size (%dx%d) differs from rectangle (%dx%d)\n",
                     frame->width, frame->height, rw, rh);
    }

    int bytesPerPixel = destBpp / 8;
    if (bytesPerPixel <= 0) {
        rfbClientLog("Unsupported destination bpp %d for H.264\n", destBpp);
        return FALSE;
    }

    const int width = frame->width;
    const int height = frame->height;
    size_t srcStride = (size_t)width * 4;
    size_t dstStride = (size_t)width * (size_t)bytesPerPixel;
    uint8_t *dst = (uint8_t *)client->raw_buffer;
#ifdef LIBVNCSERVER_HAVE_LIBSWSCALE
    const uint8_t *src = ctx->bgraBuffer;
#else
    const uint8_t *src = frame->data[0];
#endif

    rfbClientH264PreparePackState(ctx, &client->format, destBpp);
    const rfbClientH264PackState *pack = &ctx->packState;
    const rfbPixelFormat *fmt = &pack->format;

    int x, y;
    switch (destBpp) {
    case 32:
        if (fmt->bigEndian) {
            for (y = 0; y < height; ++y) {
                const uint8_t *srcRow = src + (size_t)y * srcStride;
                uint8_t *dstRow = dst + (size_t)y * dstStride;
                const uint8_t *srcPixel = srcRow;
                uint8_t *dstPixel = dstRow;
                for (x = 0; x < width; ++x) {
                    uint32_t pixel = pack->redContribution[srcPixel[2]] |
                                     pack->greenContribution[srcPixel[1]] |
                                     pack->blueContribution[srcPixel[0]];
                    dstPixel[0] = (uint8_t)((pixel >> 24) & 0xFF);
                    dstPixel[1] = (uint8_t)((pixel >> 16) & 0xFF);
                    dstPixel[2] = (uint8_t)((pixel >> 8) & 0xFF);
                    dstPixel[3] = (uint8_t)(pixel & 0xFF);
                    srcPixel += 4;
                    dstPixel += 4;
                }
            }
        } else {
            for (y = 0; y < height; ++y) {
                const uint8_t *srcRow = src + (size_t)y * srcStride;
                uint8_t *dstRow = dst + (size_t)y * dstStride;
                const uint8_t *srcPixel = srcRow;
                uint8_t *dstPixel = dstRow;
                for (x = 0; x < width; ++x) {
                    uint32_t pixel = pack->redContribution[srcPixel[2]] |
                                     pack->greenContribution[srcPixel[1]] |
                                     pack->blueContribution[srcPixel[0]];
                    dstPixel[0] = (uint8_t)(pixel & 0xFF);
                    dstPixel[1] = (uint8_t)((pixel >> 8) & 0xFF);
                    dstPixel[2] = (uint8_t)((pixel >> 16) & 0xFF);
                    dstPixel[3] = (uint8_t)((pixel >> 24) & 0xFF);
                    srcPixel += 4;
                    dstPixel += 4;
                }
            }
        }
        break;
    case 16:
        if (fmt->bigEndian) {
            for (y = 0; y < height; ++y) {
                const uint8_t *srcRow = src + (size_t)y * srcStride;
                uint8_t *dstRow = dst + (size_t)y * dstStride;
                const uint8_t *srcPixel = srcRow;
                uint8_t *dstPixel = dstRow;
                for (x = 0; x < width; ++x) {
                    uint32_t pixel = pack->redContribution[srcPixel[2]] |
                                     pack->greenContribution[srcPixel[1]] |
                                     pack->blueContribution[srcPixel[0]];
                    uint16_t value = (uint16_t)pixel;
                    dstPixel[0] = (uint8_t)((value >> 8) & 0xFF);
                    dstPixel[1] = (uint8_t)(value & 0xFF);
                    srcPixel += 4;
                    dstPixel += 2;
                }
            }
        } else {
            for (y = 0; y < height; ++y) {
                const uint8_t *srcRow = src + (size_t)y * srcStride;
                uint8_t *dstRow = dst + (size_t)y * dstStride;
                const uint8_t *srcPixel = srcRow;
                uint8_t *dstPixel = dstRow;
                for (x = 0; x < width; ++x) {
                    uint32_t pixel = pack->redContribution[srcPixel[2]] |
                                     pack->greenContribution[srcPixel[1]] |
                                     pack->blueContribution[srcPixel[0]];
                    uint16_t value = (uint16_t)pixel;
                    dstPixel[0] = (uint8_t)(value & 0xFF);
                    dstPixel[1] = (uint8_t)((value >> 8) & 0xFF);
                    srcPixel += 4;
                    dstPixel += 2;
                }
            }
        }
        break;
    case 8:
        for (y = 0; y < height; ++y) {
            const uint8_t *srcRow = src + (size_t)y * srcStride;
            uint8_t *dstRow = dst + (size_t)y * dstStride;
            const uint8_t *srcPixel = srcRow;
            uint8_t *dstPixel = dstRow;
            for (x = 0; x < width; ++x) {
                *dstPixel++ = pack->greyContribution[srcPixel[2]];
                srcPixel += 4;
            }
        }
        break;
    default:
        for (y = 0; y < height; ++y) {
            const uint8_t *srcRow = src + (size_t)y * srcStride;
            uint8_t *dstRow = dst + (size_t)y * dstStride;
            const uint8_t *srcPixel = srcRow;
            uint8_t *dstPixel = dstRow;
            for (x = 0; x < width; ++x) {
                rfbClientH264PackPixel(&client->format, dstPixel, srcPixel[2], srcPixel[1], srcPixel[0]);
                srcPixel += 4;
                dstPixel += bytesPerPixel;
            }
        }
        break;
    }

    client->GotBitmap(client, (uint8_t *)client->raw_buffer,
                      rx, ry, frame->width, frame->height);

    ctx->lastFormat = frame->format;

    return TRUE;
}

#endif /* LIBVNCCLIENT_H264_COMMON */

#define HandleH264BPP CONCAT2E(HandleH264, BPP)

static rfbBool
HandleH264BPP(rfbClient *client, int rx, int ry, int rw, int rh)
{
    rfbH264Header header;
    if (!ReadFromRFBServer(client, (char *)&header, sz_rfbH264Header)) {
        return FALSE;
    }

    uint32_t dataBytes = rfbClientSwap32IfLE(header.length);
    uint32_t flags = rfbClientSwap32IfLE(header.flags);
    const uint32_t knownFlags = rfbH264FlagResetContext | rfbH264FlagResetAllContexts;
    uint32_t unknownFlags = flags & ~knownFlags;
    if (unknownFlags) {
        rfbClientLog("Unknown H.264 flag value %u\n", unknownFlags);
        flags &= knownFlags;
    }

    rfbClientH264Context *ctx = NULL;
    rfbBool created = FALSE;

    if (flags & rfbH264FlagResetAllContexts) {
        rfbClientH264ReleaseAllContexts(client);
        flags &= ~rfbH264FlagResetAllContexts;
        if (dataBytes == 0) {
            return TRUE;
        }
    } else {
        ctx = rfbClientH264FindContext(client, rx, ry, rw, rh);
    }

    if (ctx != NULL && (flags & rfbH264FlagResetContext)) {
        rfbClientH264ReleaseContext(client, ctx);
        ctx = NULL;
    }
    flags &= ~rfbH264FlagResetContext;

    if (ctx == NULL && dataBytes == 0) {
        return TRUE;
    }

    if (ctx == NULL) {
        ctx = rfbClientH264AcquireContext(client, rx, ry, rw, rh, &created);
        if (ctx == NULL) {
            return FALSE;
        }
    }

    if (!rfbClientH264EnsureDecoder(client, ctx)) {
        rfbClientH264ReleaseContext(client, ctx);
        return FALSE;
    }

    int ret;
    if (dataBytes == 0) {
        ret = avcodec_send_packet(ctx->decoder, NULL);
    } else {
        if (av_new_packet(ctx->packet, dataBytes) < 0) {
            rfbClientLog("Failed to allocate H.264 packet of %u bytes\n", dataBytes);
            return FALSE;
        }

        if (!ReadFromRFBServer(client, (char *)ctx->packet->data, dataBytes)) {
            av_packet_unref(ctx->packet);
            return FALSE;
        }

        ret = avcodec_send_packet(ctx->decoder, ctx->packet);
        av_packet_unref(ctx->packet);
    }

    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
        rfbClientLog("H.264 decoder rejected packet (%d)\n", ret);
        return FALSE;
    }

    rfbBool gotFrame = FALSE;

    while ((ret = avcodec_receive_frame(ctx->decoder, ctx->frame)) >= 0) {
        gotFrame = TRUE;
        if (!rfbClientH264BlitFrame(client, ctx, ctx->frame, rx, ry, rw, rh, BPP)) {
            av_frame_unref(ctx->frame);
            return FALSE;
        }
        av_frame_unref(ctx->frame);
    }

    if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
        rfbClientLog("H.264 decoder failed (%d)\n", ret);
        return FALSE;
    }

    if (!gotFrame && dataBytes == 0 && created) {
        /* Decoder flushed without producing output; nothing to blit. */
    }

    rfbClientH264MoveContextToTail(client, ctx);
    return TRUE;
}


#undef HandleH264BPP

#endif /* LIBVNCSERVER_HAVE_LIBAVCODEC */
