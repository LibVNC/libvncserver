/*
 * h264.c - server-side H.264 encoding helpers.
 */

#ifdef LIBVNCSERVER_HAVE_LIBAVCODEC

#ifndef LIBVNCSERVER_HAVE_LIBSWSCALE
#error "H.264 support requires libswscale"
#endif

#include <limits.h>
#include <string.h>
#include <stdint.h>

#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>

#include <rfb/rfb.h>

#define H264_LOG_PREFIX "H264: "

typedef struct {
    rfbPixelFormat format;
    rfbBool valid;
    uint8_t *redLut;
    uint8_t *greenLut;
    uint8_t *blueLut;
    size_t redLutSize;
    size_t greenLutSize;
    size_t blueLutSize;
} rfbH264FillState;

typedef struct rfbH264EncoderContext {
    struct rfbH264EncoderContext *prev;
    struct rfbH264EncoderContext *next;
    int x;
    int y;
    int w;
    int h;
    struct AVCodecContext *encoder;
    struct AVFrame *frame;
    struct AVPacket *packet;
#ifdef LIBVNCSERVER_HAVE_LIBSWSCALE
    struct SwsContext *sws;
#endif
    uint8_t *rgbBuffer;
    size_t rgbBufferSize;
    uint8_t *encodeBuffer;
    size_t encodeBufferSize;
    rfbH264FillState fillState;
    int64_t framePts;
    rfbBool forceKeyframe;
    rfbBool sentConfig;
} rfbH264EncoderContext;

#define RFB_H264_MAX_CONTEXTS 64


static rfbH264EncoderContext *rfbClientH264FindContext(rfbClientPtr cl, int x, int y, int w, int h);
static rfbH264EncoderContext *rfbClientH264AcquireContext(rfbClientPtr cl, int x, int y, int w, int h, rfbBool *created);
static void rfbClientH264ReleaseContext(rfbClientPtr cl, rfbH264EncoderContext *ctx);
static void rfbClientH264ReleaseAllContexts(rfbClientPtr cl);
static void rfbClientH264MoveContextToTail(rfbClientPtr cl, rfbH264EncoderContext *ctx);
static void rfbClientH264MarkAllForKeyframe(rfbClientPtr cl);

static rfbBool rfbClientH264EnsureEncoder(rfbClientPtr cl, rfbH264EncoderContext *ctx);
static rfbBool rfbClientH264EnsureRgbBuffer(rfbClientPtr cl, rfbH264EncoderContext *ctx);
static rfbBool rfbClientH264FillBgra(rfbClientPtr cl, rfbH264EncoderContext *ctx);
static rfbBool rfbClientH264EncodeFrame(rfbClientPtr cl, rfbH264EncoderContext *ctx, rfbBool forceKeyframe, size_t *encodedSizeOut);
static rfbBool rfbClientH264EnsureEncodeCapacity(rfbClientPtr cl, rfbH264EncoderContext *ctx, size_t required);
static rfbBool rfbClientH264PrependConfig(rfbClientPtr cl, rfbH264EncoderContext *ctx, rfbBool forceKeyframe, size_t *encodedSize);
static rfbBool rfbClientH264AppendPacket(rfbClientPtr cl, rfbH264EncoderContext *ctx, const AVPacket *packet, size_t *encodedSize);
static rfbBool rfbClientH264ExtradataToAnnexB(const uint8_t *extra, size_t extraSize, uint8_t **out, size_t *outSize);
static rfbBool rfbClientH264PacketNeedsAnnexB(const AVPacket *packet);
static int rfbClientH264GetNalLengthSize(const AVCodecContext *ctx);

static const uint8_t h264StartCode[4] = { 0x00, 0x00, 0x00, 0x01 };

static void
rfbClientH264InitLogging(void)
{
    static rfbBool initialized = FALSE;
    if (!initialized) {
        av_log_set_level(AV_LOG_QUIET);
        initialized = TRUE;
    }
}

static void
rfbClientH264AppendContext(rfbClientPtr cl, rfbH264EncoderContext *ctx)
{
    ctx->prev = cl->h264ContextsTail;
    ctx->next = NULL;
    if (cl->h264ContextsTail != NULL) {
        cl->h264ContextsTail->next = ctx;
    } else {
        cl->h264ContextsHead = ctx;
    }
    cl->h264ContextsTail = ctx;
}

static void
rfbClientH264DetachContext(rfbClientPtr cl, rfbH264EncoderContext *ctx)
{
    if (ctx->prev != NULL) {
        ctx->prev->next = ctx->next;
    } else {
        cl->h264ContextsHead = ctx->next;
    }
    if (ctx->next != NULL) {
        ctx->next->prev = ctx->prev;
    } else {
        cl->h264ContextsTail = ctx->prev;
    }
    ctx->prev = NULL;
    ctx->next = NULL;
}

static rfbH264EncoderContext *
rfbClientH264FindContext(rfbClientPtr cl, int x, int y, int w, int h)
{
    rfbH264EncoderContext *ctx;

    for (ctx = cl->h264ContextsHead; ctx != NULL; ctx = ctx->next) {
        if (ctx->x == x && ctx->y == y && ctx->w == w && ctx->h == h) {
            return ctx;
        }
    }

    return NULL;
}

static void
rfbClientH264ReleaseContext(rfbClientPtr cl, rfbH264EncoderContext *ctx)
{
    if (ctx == NULL) {
        return;
    }

    rfbClientH264DetachContext(cl, ctx);

    if (ctx->encoder != NULL) {
        avcodec_free_context(&ctx->encoder);
    }
    av_frame_free(&ctx->frame);
    av_packet_free(&ctx->packet);
#ifdef LIBVNCSERVER_HAVE_LIBSWSCALE
    if (ctx->sws != NULL) {
        sws_freeContext(ctx->sws);
    }
#endif
    free(ctx->rgbBuffer);
    free(ctx->encodeBuffer);
    free(ctx->fillState.redLut);
    free(ctx->fillState.greenLut);
    free(ctx->fillState.blueLut);

    if (cl->h264ContextCount > 0) {
        cl->h264ContextCount--;
    }

    free(ctx);
}

static void
rfbClientH264ReleaseAllContexts(rfbClientPtr cl)
{
    rfbH264EncoderContext *ctx = cl->h264ContextsHead;

    if (ctx != NULL) {
        cl->h264NeedsResetAll = TRUE;
    }

    while (ctx != NULL) {
        rfbH264EncoderContext *next = ctx->next;
        rfbClientH264ReleaseContext(cl, ctx);
        ctx = next;
    }

    cl->h264ContextsHead = NULL;
    cl->h264ContextsTail = NULL;
    cl->h264ContextCount = 0;
}

static void
rfbClientH264MoveContextToTail(rfbClientPtr cl, rfbH264EncoderContext *ctx)
{
    if (ctx == NULL || cl->h264ContextsTail == ctx) {
        return;
    }

    rfbClientH264DetachContext(cl, ctx);
    rfbClientH264AppendContext(cl, ctx);
}

static void
rfbClientH264MarkAllForKeyframe(rfbClientPtr cl)
{
    rfbH264EncoderContext *ctx;

    for (ctx = cl->h264ContextsHead; ctx != NULL; ctx = ctx->next) {
        ctx->forceKeyframe = TRUE;
        ctx->sentConfig = FALSE;
        ctx->framePts = 0;
    }
}

static rfbH264EncoderContext *
rfbClientH264AcquireContext(rfbClientPtr cl, int x, int y, int w, int h, rfbBool *created)
{
    rfbH264EncoderContext *ctx = rfbClientH264FindContext(cl, x, y, w, h);

    if (ctx != NULL) {
        if (created != NULL) {
            *created = FALSE;
        }
        rfbClientH264MoveContextToTail(cl, ctx);
        return ctx;
    }

    if (cl->h264ContextCount >= RFB_H264_MAX_CONTEXTS && cl->h264ContextsHead != NULL) {
        rfbClientH264ReleaseContext(cl, cl->h264ContextsHead);
    }

    ctx = (rfbH264EncoderContext *)calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        rfbLog(H264_LOG_PREFIX "failed to allocate encoder context\n");
        return NULL;
    }

    ctx->x = x;
    ctx->y = y;
    ctx->w = w;
    ctx->h = h;
    ctx->forceKeyframe = TRUE;
    ctx->sentConfig = FALSE;
    ctx->framePts = 0;

    rfbClientH264AppendContext(cl, ctx);
    cl->h264ContextCount++;

    if (created != NULL) {
        *created = TRUE;
    }

    return ctx;
}

static rfbBool
rfbClientH264EnsureEncodeCapacity(rfbClientPtr cl, rfbH264EncoderContext *ctx, size_t required)
{
    if (required > ctx->encodeBufferSize) {
        uint8_t *buf = (uint8_t *)realloc(ctx->encodeBuffer, required);
        if (buf == NULL) {
            rfbLog(H264_LOG_PREFIX "failed to allocate %zu bytes for encoded buffer\n", required);
            return FALSE;
        }
        ctx->encodeBuffer = buf;
        ctx->encodeBufferSize = required;
    }
    return TRUE;
}

static rfbBool
rfbClientH264AppendBytes(rfbClientPtr cl, rfbH264EncoderContext *ctx, const uint8_t *data, size_t len, size_t *offset)
{
    if (!rfbClientH264EnsureEncodeCapacity(cl, ctx, *offset + len)) {
        return FALSE;
    }
    memcpy(ctx->encodeBuffer + *offset, data, len);
    *offset += len;
    return TRUE;
}

static rfbBool
rfbClientH264AppendStartCode(rfbClientPtr cl, rfbH264EncoderContext *ctx, size_t *offset)
{
    return rfbClientH264AppendBytes(cl, ctx, h264StartCode, sizeof(h264StartCode), offset);
}

static int
rfbClientH264GetNalLengthSize(const AVCodecContext *ctx)
{
    if (ctx->extradata_size >= 5 && ctx->extradata != NULL && ctx->extradata[0] == 1) {
        return (ctx->extradata[4] & 0x03) + 1;
    }
    return 4;
}

void
rfbClientH264ReleaseEncoder(rfbClientPtr cl)
{
    if (cl == NULL) {
        return;
    }

    rfbClientH264ReleaseAllContexts(cl);
    cl->h264ForceKeyframe = FALSE;
    cl->h264NeedsResetAll = FALSE;
}

void
rfbClientH264SetBitrate(rfbClientPtr cl, int64_t bitRate)
{
    if (cl == NULL) {
        return;
    }

    int64_t previous = cl->h264BitRate;
    cl->h264BitRate = bitRate;

    if (previous == bitRate) {
        return;
    }

    rfbClientH264ReleaseAllContexts(cl);
    cl->h264ForceKeyframe = TRUE;
    cl->h264NeedsResetAll = TRUE;
}

static rfbBool
rfbClientH264EnsureEncoder(rfbClientPtr cl, rfbH264EncoderContext *ctx)
{
    const int width = ctx->w;
    const int height = ctx->h;

    rfbClientH264InitLogging();

    if (!cl->scaledScreen->serverFormat.trueColour) {
        rfbLog(H264_LOG_PREFIX "server pixel format must be true-colour for H.264\n");
        return FALSE;
    }

    if (width <= 0 || height <= 0) {
        rfbLog(H264_LOG_PREFIX "invalid frame size %dx%d\n", width, height);
        return FALSE;
    }

    if (ctx->encoder != NULL &&
        (ctx->encoder->width != width || ctx->encoder->height != height)) {
        avcodec_free_context(&ctx->encoder);
#ifdef LIBVNCSERVER_HAVE_LIBSWSCALE
        if (ctx->sws != NULL) {
            sws_freeContext(ctx->sws);
            ctx->sws = NULL;
        }
#endif
    }

    if (ctx->frame != NULL &&
        (ctx->frame->width != width || ctx->frame->height != height)) {
        av_frame_free(&ctx->frame);
    }

    if (ctx->encoder == NULL) {
        const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);

        if (codec == NULL) {
            rfbLog(H264_LOG_PREFIX "H.264 encoder not available\n");
            return FALSE;
        }

        AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
        if (codecCtx == NULL) {
            rfbLog(H264_LOG_PREFIX "failed to allocate encoder context\n");
            return FALSE;
        }

        codecCtx->width = width;
        codecCtx->height = height;
        codecCtx->time_base = (AVRational){1, 30};
        codecCtx->framerate = (AVRational){30, 1};
        codecCtx->gop_size = 30;
        codecCtx->max_b_frames = 0;
        codecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
        codecCtx->thread_count = 1; /* Disable frame threading to avoid delayed output */
#ifdef FF_THREAD_SLICE
        codecCtx->thread_type = FF_THREAD_SLICE;
#endif
        codecCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;

        int64_t bitRate = cl->h264BitRate;
        if (bitRate <= 0) {
            bitRate = (int64_t)width * height * 4; /* ~4 bits per pixel */
        }
        codecCtx->bit_rate = bitRate;

        av_opt_set(codecCtx->priv_data, "preset", "veryfast", 0);
        av_opt_set(codecCtx->priv_data, "tune", "zerolatency", 0);

        if (avcodec_open2(codecCtx, codec, NULL) < 0) {
            rfbLog(H264_LOG_PREFIX "failed to open encoder\n");
            avcodec_free_context(&codecCtx);
            return FALSE;
        }

        ctx->encoder = codecCtx;
        ctx->framePts = 0;
        ctx->forceKeyframe = TRUE;
        ctx->sentConfig = FALSE;
    }

    if (ctx->frame == NULL) {
        ctx->frame = av_frame_alloc();
        if (ctx->frame == NULL) {
            rfbLog(H264_LOG_PREFIX "failed to allocate frame\n");
            avcodec_free_context(&ctx->encoder);
            ctx->encoder = NULL;
            return FALSE;
        }
        ctx->frame->format = ctx->encoder->pix_fmt;
        ctx->frame->width = ctx->encoder->width;
        ctx->frame->height = ctx->encoder->height;

        if (av_frame_get_buffer(ctx->frame, 32) < 0) {
            rfbLog(H264_LOG_PREFIX "failed to allocate frame buffer\n");
            av_frame_free(&ctx->frame);
            avcodec_free_context(&ctx->encoder);
            ctx->encoder = NULL;
            return FALSE;
        }
    }

    if (ctx->packet == NULL) {
        ctx->packet = av_packet_alloc();
        if (ctx->packet == NULL) {
            rfbLog(H264_LOG_PREFIX "failed to allocate packet\n");
            av_frame_free(&ctx->frame);
            avcodec_free_context(&ctx->encoder);
            ctx->encoder = NULL;
            return FALSE;
        }
    }

    return TRUE;
}

static rfbBool
rfbClientH264EnsureRgbBuffer(rfbClientPtr cl, rfbH264EncoderContext *ctx)
{
    size_t required = (size_t)ctx->w * (size_t)ctx->h * 4;

    if (required > ctx->rgbBufferSize) {
        uint8_t *buf = (uint8_t *)realloc(ctx->rgbBuffer, required);
        if (buf == NULL) {
            rfbLog(H264_LOG_PREFIX "failed to allocate RGB buffer (%zu bytes)\n", required);
            return FALSE;
        }
        ctx->rgbBuffer = buf;
        ctx->rgbBufferSize = required;
    }
    return TRUE;
}


static inline uint8_t
rfbClientH264ScaleComponent(uint32_t value, uint32_t max)
{
    if (max == 0) {
        return 0;
    }
    return (uint8_t)((value * 255U + (max / 2U)) / max);
}

static rfbBool
rfbClientH264EnsureComponentLut(uint8_t **lut, size_t *capacity, uint32_t max)
{
    size_t required = (size_t)max + 1;
    if (required == 0) {
        required = 1;
    }

    if (*capacity < required) {
        uint8_t *tmp = (uint8_t *)realloc(*lut, required);
        if (tmp == NULL) {
            return FALSE;
        }
        *lut = tmp;
        *capacity = required;
    }

    return TRUE;
}

static rfbBool
rfbClientH264EnsureFillState(rfbClientPtr cl, rfbH264EncoderContext *ctx)
{
    rfbH264FillState *state = &ctx->fillState;
    const rfbPixelFormat *fmt = &cl->scaledScreen->serverFormat;

    if (state->valid && memcmp(&state->format, fmt, sizeof(*fmt)) == 0) {
        return TRUE;
    }

    if (!rfbClientH264EnsureComponentLut(&state->redLut, &state->redLutSize, fmt->redMax) ||
        !rfbClientH264EnsureComponentLut(&state->greenLut, &state->greenLutSize, fmt->greenMax) ||
        !rfbClientH264EnsureComponentLut(&state->blueLut, &state->blueLutSize, fmt->blueMax)) {
        rfbLog(H264_LOG_PREFIX "failed to allocate H.264 component lookup tables\n");
        return FALSE;
    }

    uint32_t i;
    for (i = 0; i <= fmt->redMax; ++i) {
        state->redLut[i] = rfbClientH264ScaleComponent(i, fmt->redMax);
    }
    for (i = 0; i <= fmt->greenMax; ++i) {
        state->greenLut[i] = rfbClientH264ScaleComponent(i, fmt->greenMax);
    }
    for (i = 0; i <= fmt->blueMax; ++i) {
        state->blueLut[i] = rfbClientH264ScaleComponent(i, fmt->blueMax);
    }

    state->format = *fmt;
    state->valid = TRUE;
    return TRUE;
}



static rfbBool
rfbClientH264PacketNeedsAnnexB(const AVPacket *packet)
{
    if (packet->size >= 4 && packet->data != NULL) {
        if (packet->data[0] == 0x00 && packet->data[1] == 0x00 &&
            packet->data[2] == 0x00 && packet->data[3] == 0x01) {
            return FALSE;
        }
    }
    return TRUE;
}

static rfbBool
rfbClientH264ExtradataToAnnexB(const uint8_t *extra, size_t extraSize, uint8_t **out, size_t *outSize)
{
    *out = NULL;
    *outSize = 0;

    if (extra == NULL || extraSize == 0) {
        return TRUE;
    }

    if (extraSize >= 4 && extra[0] == 0x00 && extra[1] == 0x00 && extra[2] == 0x00 && extra[3] == 0x01) {
        uint8_t *buf = (uint8_t *)malloc(extraSize);
        if (buf == NULL) {
            return FALSE;
        }
        memcpy(buf, extra, extraSize);
        *out = buf;
        *outSize = extraSize;
        return TRUE;
    }

    if (extraSize < 7 || extra[0] != 1) {
        return FALSE;
    }

    size_t pos = 5;
    uint8_t numSps = extra[pos++] & 0x1F;
    size_t capacity = extraSize * 4 + 16;
    size_t offset = 0;
    uint8_t *buf = (uint8_t *)malloc(capacity);
    if (buf == NULL) {
        return FALSE;
    }

    uint8_t i;

    for (i = 0; i < numSps; ++i) {
        if (pos + 2 > extraSize) {
            free(buf);
            return FALSE;
        }
        uint16_t nalSize = ((uint16_t)extra[pos] << 8) | extra[pos + 1];
        pos += 2;
        if (pos + nalSize > extraSize) {
            free(buf);
            return FALSE;
        }
        if (offset + sizeof(h264StartCode) + nalSize > capacity) {
            size_t newCap = capacity * 2 + sizeof(h264StartCode) + nalSize;
            uint8_t *tmp = (uint8_t *)realloc(buf, newCap);
            if (tmp == NULL) {
                free(buf);
                return FALSE;
            }
            buf = tmp;
            capacity = newCap;
        }
        memcpy(buf + offset, h264StartCode, sizeof(h264StartCode));
        offset += sizeof(h264StartCode);
        memcpy(buf + offset, extra + pos, nalSize);
        offset += nalSize;
        pos += nalSize;
    }

    if (pos + 1 > extraSize) {
        free(buf);
        return FALSE;
    }
    uint8_t numPps = extra[pos++];
    for (i = 0; i < numPps; ++i) {
        if (pos + 2 > extraSize) {
            free(buf);
            return FALSE;
        }
        uint16_t nalSize = ((uint16_t)extra[pos] << 8) | extra[pos + 1];
        pos += 2;
        if (pos + nalSize > extraSize) {
            free(buf);
            return FALSE;
        }
        if (offset + sizeof(h264StartCode) + nalSize > capacity) {
            size_t newCap = capacity * 2 + sizeof(h264StartCode) + nalSize;
            uint8_t *tmp = (uint8_t *)realloc(buf, newCap);
            if (tmp == NULL) {
                free(buf);
                return FALSE;
            }
            buf = tmp;
            capacity = newCap;
        }
        memcpy(buf + offset, h264StartCode, sizeof(h264StartCode));
        offset += sizeof(h264StartCode);
        memcpy(buf + offset, extra + pos, nalSize);
        offset += nalSize;
        pos += nalSize;
    }

    if (pos + 1 <= extraSize) {
        uint8_t numSpsExt = extra[pos++];
        for (i = 0; i < numSpsExt; ++i) {
            if (pos + 2 > extraSize) {
                free(buf);
                return FALSE;
            }
            uint16_t nalSize = ((uint16_t)extra[pos] << 8) | extra[pos + 1];
            pos += 2;
            if (pos + nalSize > extraSize) {
                free(buf);
                return FALSE;
            }
            if (offset + sizeof(h264StartCode) + nalSize > capacity) {
                size_t newCap = capacity * 2 + sizeof(h264StartCode) + nalSize;
                uint8_t *tmp = (uint8_t *)realloc(buf, newCap);
                if (tmp == NULL) {
                    free(buf);
                    return FALSE;
                }
                buf = tmp;
                capacity = newCap;
            }
            memcpy(buf + offset, h264StartCode, sizeof(h264StartCode));
            offset += sizeof(h264StartCode);
            memcpy(buf + offset, extra + pos, nalSize);
            offset += nalSize;
            pos += nalSize;
        }
    }

    *out = buf;
    *outSize = offset;
    return TRUE;
}

static rfbBool
rfbClientH264AppendPacket(rfbClientPtr cl, rfbH264EncoderContext *ctx, const AVPacket *packet, size_t *encodedSize)
{
    if (packet->size <= 0) {
        return TRUE;
    }

    if (!rfbClientH264PacketNeedsAnnexB(packet)) {
        if (!rfbClientH264EnsureEncodeCapacity(cl, ctx, *encodedSize + (size_t)packet->size)) {
            return FALSE;
        }
        memcpy(ctx->encodeBuffer + *encodedSize, packet->data, (size_t)packet->size);
        *encodedSize += (size_t)packet->size;
        return TRUE;
    }

    int nalLengthSize = rfbClientH264GetNalLengthSize(ctx->encoder);
    size_t pos = 0;
    const size_t packetSize = (size_t)packet->size;

    while (pos + nalLengthSize <= packetSize) {
        uint32_t nalSize = 0;
        const uint8_t *data = packet->data + pos;

        switch (nalLengthSize) {
        case 1:
            nalSize = data[0];
            break;
        case 2:
            nalSize = ((uint32_t)data[0] << 8) | data[1];
            break;
        case 3:
            nalSize = ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | data[2];
            break;
        default:
            nalSize = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                      ((uint32_t)data[2] << 8) | data[3];
            break;
        }

        pos += nalLengthSize;

        if (nalSize == 0 || pos + nalSize > packetSize) {
            return FALSE;
        }

        if (!rfbClientH264AppendStartCode(cl, ctx, encodedSize)) {
            return FALSE;
        }

        if (!rfbClientH264AppendBytes(cl, ctx, packet->data + pos, nalSize, encodedSize)) {
            return FALSE;
        }

        pos += nalSize;
    }

    return pos == packetSize;
}

static rfbBool
rfbClientH264PrependConfig(rfbClientPtr cl, rfbH264EncoderContext *ctx, rfbBool forceKeyframe, size_t *encodedSize)
{
    if (ctx->sentConfig && !forceKeyframe) {
        return TRUE;
    }

    uint8_t *annexB = NULL;
    size_t annexSize = 0;

    if (!rfbClientH264ExtradataToAnnexB(ctx->encoder->extradata,
                                        ctx->encoder->extradata_size,
                                        &annexB,
                                        &annexSize)) {
        free(annexB);
        return FALSE;
    }

    if (annexSize == 0) {
        ctx->sentConfig = TRUE;
        free(annexB);
        return TRUE;
    }

    if (!rfbClientH264EnsureEncodeCapacity(cl, ctx, *encodedSize + annexSize)) {
        free(annexB);
        return FALSE;
    }

    memmove(ctx->encodeBuffer + annexSize, ctx->encodeBuffer, *encodedSize);
    memcpy(ctx->encodeBuffer, annexB, annexSize);
    *encodedSize += annexSize;
    ctx->sentConfig = TRUE;

    free(annexB);
    return TRUE;
}



static rfbBool
rfbClientH264FillBgra(rfbClientPtr cl, rfbH264EncoderContext *ctx)
{
    const rfbPixelFormat *fmt = &cl->scaledScreen->serverFormat;
    const uint8_t *fb = (const uint8_t *)cl->scaledScreen->frameBuffer;
    const int stride = cl->scaledScreen->paddedWidthInBytes;
    const int width = ctx->w;
    const int height = ctx->h;
    const int originX = ctx->x;
    const int originY = ctx->y;
    const size_t bytesPerPixel = (size_t)fmt->bitsPerPixel / 8;

    if (bytesPerPixel == 0) {
        rfbLog(H264_LOG_PREFIX "unsupported server pixel depth %d\n", fmt->bitsPerPixel);
        return FALSE;
    }

    if (!rfbClientH264EnsureRgbBuffer(cl, ctx)) {
        return FALSE;
    }

    uint8_t *dstBase = ctx->rgbBuffer;

    int row, col;
    if (!fmt->bigEndian &&
        fmt->bitsPerPixel == 32 &&
        fmt->redMax == 255 && fmt->greenMax == 255 && fmt->blueMax == 255 &&
        fmt->redShift == 16 && fmt->greenShift == 8 && fmt->blueShift == 0) {
        for (row = 0; row < height; ++row) {
            const uint8_t *src = fb + (size_t)(originY + row) * stride + (size_t)originX * bytesPerPixel;
            uint8_t *dst = dstBase + (size_t)row * width * 4;
            const uint8_t *srcPixel = src;
            uint8_t *dstPixel = dst;
            for (col = 0; col < width; ++col) {
                dstPixel[0] = srcPixel[0];
                dstPixel[1] = srcPixel[1];
                dstPixel[2] = srcPixel[2];
                dstPixel[3] = 0xFF;
                srcPixel += 4;
                dstPixel += 4;
            }
        }
        return TRUE;
    }

    if (!rfbClientH264EnsureFillState(cl, ctx)) {
        return FALSE;
    }

    const uint8_t *redLut = ctx->fillState.redLut;
    const uint8_t *greenLut = ctx->fillState.greenLut;
    const uint8_t *blueLut = ctx->fillState.blueLut;

    switch (fmt->bitsPerPixel) {
    case 32:
        if (fmt->bigEndian) {
            for (row = 0; row < height; ++row) {
                const uint8_t *srcPixel = fb + (size_t)(originY + row) * stride + (size_t)originX * bytesPerPixel;
                uint8_t *dstPixel = dstBase + (size_t)row * width * 4;
                for (col = 0; col < width; ++col) {
                    uint32_t pixel = ((uint32_t)srcPixel[0] << 24) |
                                     ((uint32_t)srcPixel[1] << 16) |
                                     ((uint32_t)srcPixel[2] << 8) |
                                     (uint32_t)srcPixel[3];
                    uint32_t rawR = (pixel >> fmt->redShift) & fmt->redMax;
                    uint32_t rawG = (pixel >> fmt->greenShift) & fmt->greenMax;
                    uint32_t rawB = (pixel >> fmt->blueShift) & fmt->blueMax;
                    dstPixel[2] = redLut[rawR];
                    dstPixel[1] = greenLut[rawG];
                    dstPixel[0] = blueLut[rawB];
                    dstPixel[3] = 0xFF;
                    srcPixel += 4;
                    dstPixel += 4;
                }
            }
        } else {
            for (row = 0; row < height; ++row) {
                const uint8_t *srcPixel = fb + (size_t)(originY + row) * stride + (size_t)originX * bytesPerPixel;
                uint8_t *dstPixel = dstBase + (size_t)row * width * 4;
                for (col = 0; col < width; ++col) {
                    uint32_t pixel = (uint32_t)srcPixel[0] |
                                     ((uint32_t)srcPixel[1] << 8) |
                                     ((uint32_t)srcPixel[2] << 16) |
                                     ((uint32_t)srcPixel[3] << 24);
                    uint32_t rawR = (pixel >> fmt->redShift) & fmt->redMax;
                    uint32_t rawG = (pixel >> fmt->greenShift) & fmt->greenMax;
                    uint32_t rawB = (pixel >> fmt->blueShift) & fmt->blueMax;
                    dstPixel[2] = redLut[rawR];
                    dstPixel[1] = greenLut[rawG];
                    dstPixel[0] = blueLut[rawB];
                    dstPixel[3] = 0xFF;
                    srcPixel += 4;
                    dstPixel += 4;
                }
            }
        }
        break;
    case 24:
        if (fmt->bigEndian) {
            for (row = 0; row < height; ++row) {
                const uint8_t *srcPixel = fb + (size_t)(originY + row) * stride + (size_t)originX * bytesPerPixel;
                uint8_t *dstPixel = dstBase + (size_t)row * width * 4;
                for (col = 0; col < width; ++col) {
                    uint32_t pixel = ((uint32_t)srcPixel[0] << 16) |
                                     ((uint32_t)srcPixel[1] << 8) |
                                     (uint32_t)srcPixel[2];
                    uint32_t rawR = (pixel >> fmt->redShift) & fmt->redMax;
                    uint32_t rawG = (pixel >> fmt->greenShift) & fmt->greenMax;
                    uint32_t rawB = (pixel >> fmt->blueShift) & fmt->blueMax;
                    dstPixel[2] = redLut[rawR];
                    dstPixel[1] = greenLut[rawG];
                    dstPixel[0] = blueLut[rawB];
                    dstPixel[3] = 0xFF;
                    srcPixel += 3;
                    dstPixel += 4;
                }
            }
        } else {
            for (row = 0; row < height; ++row) {
                const uint8_t *srcPixel = fb + (size_t)(originY + row) * stride + (size_t)originX * bytesPerPixel;
                uint8_t *dstPixel = dstBase + (size_t)row * width * 4;
                for (col = 0; col < width; ++col) {
                    uint32_t pixel = (uint32_t)srcPixel[0] |
                                     ((uint32_t)srcPixel[1] << 8) |
                                     ((uint32_t)srcPixel[2] << 16);
                    uint32_t rawR = (pixel >> fmt->redShift) & fmt->redMax;
                    uint32_t rawG = (pixel >> fmt->greenShift) & fmt->greenMax;
                    uint32_t rawB = (pixel >> fmt->blueShift) & fmt->blueMax;
                    dstPixel[2] = redLut[rawR];
                    dstPixel[1] = greenLut[rawG];
                    dstPixel[0] = blueLut[rawB];
                    dstPixel[3] = 0xFF;
                    srcPixel += 3;
                    dstPixel += 4;
                }
            }
        }
        break;
    case 16:
        if (fmt->bigEndian) {
            for (row = 0; row < height; ++row) {
                const uint8_t *srcPixel = fb + (size_t)(originY + row) * stride + (size_t)originX * bytesPerPixel;
                uint8_t *dstPixel = dstBase + (size_t)row * width * 4;
                for (col = 0; col < width; ++col) {
                    uint32_t pixel = ((uint32_t)srcPixel[0] << 8) | (uint32_t)srcPixel[1];
                    uint32_t rawR = (pixel >> fmt->redShift) & fmt->redMax;
                    uint32_t rawG = (pixel >> fmt->greenShift) & fmt->greenMax;
                    uint32_t rawB = (pixel >> fmt->blueShift) & fmt->blueMax;
                    dstPixel[2] = redLut[rawR];
                    dstPixel[1] = greenLut[rawG];
                    dstPixel[0] = blueLut[rawB];
                    dstPixel[3] = 0xFF;
                    srcPixel += 2;
                    dstPixel += 4;
                }
            }
        } else {
            for (row = 0; row < height; ++row) {
                const uint8_t *srcPixel = fb + (size_t)(originY + row) * stride + (size_t)originX * bytesPerPixel;
                uint8_t *dstPixel = dstBase + (size_t)row * width * 4;
                for (col = 0; col < width; ++col) {
                    uint32_t pixel = (uint32_t)srcPixel[0] | ((uint32_t)srcPixel[1] << 8);
                    uint32_t rawR = (pixel >> fmt->redShift) & fmt->redMax;
                    uint32_t rawG = (pixel >> fmt->greenShift) & fmt->greenMax;
                    uint32_t rawB = (pixel >> fmt->blueShift) & fmt->blueMax;
                    dstPixel[2] = redLut[rawR];
                    dstPixel[1] = greenLut[rawG];
                    dstPixel[0] = blueLut[rawB];
                    dstPixel[3] = 0xFF;
                    srcPixel += 2;
                    dstPixel += 4;
                }
            }
        }
        break;
    case 8:
        for (row = 0; row < height; ++row) {
            const uint8_t *srcPixel = fb + (size_t)(originY + row) * stride + (size_t)originX * bytesPerPixel;
            uint8_t *dstPixel = dstBase + (size_t)row * width * 4;
            for (col = 0; col < width; ++col) {
                uint32_t pixel = (uint32_t)srcPixel[0];
                uint32_t rawR = (pixel >> fmt->redShift) & fmt->redMax;
                uint32_t rawG = (pixel >> fmt->greenShift) & fmt->greenMax;
                uint32_t rawB = (pixel >> fmt->blueShift) & fmt->blueMax;
                dstPixel[2] = redLut[rawR];
                dstPixel[1] = greenLut[rawG];
                dstPixel[0] = blueLut[rawB];
                dstPixel[3] = 0xFF;
                srcPixel += 1;
                dstPixel += 4;
            }
        }
        break;
    default:
        rfbLog(H264_LOG_PREFIX "unsupported server pixel depth %d\n", fmt->bitsPerPixel);
        return FALSE;
    }

    return TRUE;
}

static rfbBool
rfbClientH264EncodeFrame(rfbClientPtr cl, rfbH264EncoderContext *ctx, rfbBool forceKeyframe, size_t *encodedSizeOut)
{
    AVCodecContext *codecCtx = ctx->encoder;
    AVFrame *frame = ctx->frame;
    AVPacket *packet = ctx->packet;
    size_t encodedSize = 0;
    int ret;

    if (codecCtx == NULL || frame == NULL || packet == NULL) {
        return FALSE;
    }

    if (av_frame_make_writable(frame) < 0) {
        rfbLog(H264_LOG_PREFIX "frame not writable");
        return FALSE;
    }

    frame->pict_type = forceKeyframe ? AV_PICTURE_TYPE_I : AV_PICTURE_TYPE_P;
#ifdef AV_FRAME_FLAG_KEY
    if (forceKeyframe)
        frame->flags |= AV_FRAME_FLAG_KEY;
    else
        frame->flags &= ~AV_FRAME_FLAG_KEY;
#endif

    struct SwsContext *sws = sws_getCachedContext(
        ctx->sws,
        codecCtx->width,
        codecCtx->height,
        AV_PIX_FMT_BGRA,
        codecCtx->width,
        codecCtx->height,
        codecCtx->pix_fmt,
        SWS_BILINEAR,
        NULL,
        NULL,
        NULL);
    if (sws == NULL) {
        rfbLog(H264_LOG_PREFIX "failed to create colorspace converter");
        return FALSE;
    }
    ctx->sws = sws;

    uint8_t *srcData[4] = { ctx->rgbBuffer, NULL, NULL, NULL };
    int srcStride[4] = { codecCtx->width * 4, 0, 0, 0 };

    sws_scale(sws, (const uint8_t * const *)srcData, srcStride, 0, codecCtx->height,
              frame->data, frame->linesize);

    frame->pts = ctx->framePts++;

    ret = avcodec_send_frame(codecCtx, frame);
    if (ret < 0) {
        rfbLog(H264_LOG_PREFIX "encoder rejected frame (%d)", ret);
        return FALSE;
    }

    for (;;) {
        ret = avcodec_receive_packet(codecCtx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            rfbLog(H264_LOG_PREFIX "encoder failed (%d)", ret);
            return FALSE;
        }

        if (!rfbClientH264AppendPacket(cl, ctx, packet, &encodedSize)) {
            av_packet_unref(packet);
            return FALSE;
        }
        av_packet_unref(packet);
    }

    if (encodedSize == 0) {
        rfbLog(H264_LOG_PREFIX "encoder produced no output");
        return FALSE;
    }

    *encodedSizeOut = encodedSize;
    return TRUE;
}

rfbBool
rfbSendRectEncodingH264(rfbClientPtr cl,
                        int x,
                        int y,
                        int w,
                        int h)
{
    size_t encodedSize = 0;
    rfbFramebufferUpdateRectHeader rect;
    rfbH264Header hdr;
    int rawEquivalent;
    rfbBool created = FALSE;
    rfbH264EncoderContext *ctx;
    rfbBool forceKeyframe;

    if (cl == NULL || cl->scaledScreen == NULL || cl->scaledScreen->frameBuffer == NULL) {
        return FALSE;
    }

    if (w <= 0 || h <= 0) {
        return TRUE;
    }

    if (cl->h264ForceKeyframe) {
        rfbClientH264MarkAllForKeyframe(cl);
        cl->h264ForceKeyframe = FALSE;
        cl->h264NeedsResetAll = TRUE;
    }

    ctx = rfbClientH264AcquireContext(cl, x, y, w, h, &created);
    if (ctx == NULL) {
        return FALSE;
    }

    if (!rfbClientH264EnsureEncoder(cl, ctx)) {
        rfbClientH264ReleaseContext(cl, ctx);
        return FALSE;
    }

    if (!rfbClientH264FillBgra(cl, ctx)) {
        rfbClientH264ReleaseContext(cl, ctx);
        return FALSE;
    }

    forceKeyframe = ctx->forceKeyframe || created || cl->h264NeedsResetAll;

    if (!rfbClientH264EncodeFrame(cl, ctx, forceKeyframe, &encodedSize)) {
        return FALSE;
    }

    if (!rfbClientH264PrependConfig(cl, ctx, forceKeyframe, &encodedSize)) {
        return FALSE;
    }

    if (encodedSize > UINT32_MAX) {
        rfbLog(H264_LOG_PREFIX "encoded frame too large (%zu bytes)\n", encodedSize);
        return FALSE;
    }

    if (cl->ublen > 0 && !rfbSendUpdateBuf(cl)) {
        return FALSE;
    }

    rect.r.x = Swap16IfLE((uint16_t)x);
    rect.r.y = Swap16IfLE((uint16_t)y);
    rect.r.w = Swap16IfLE((uint16_t)w);
    rect.r.h = Swap16IfLE((uint16_t)h);
    rect.encoding = Swap32IfLE(rfbEncodingH264);

    uint32_t flags = rfbH264FlagNone;
    if (cl->h264NeedsResetAll) {
        flags |= rfbH264FlagResetAllContexts;
    }
    if (forceKeyframe || created || (flags & rfbH264FlagResetAllContexts)) {
        flags |= rfbH264FlagResetContext;
    }

    hdr.length = Swap32IfLE((uint32_t)encodedSize);
    hdr.flags = Swap32IfLE(flags);

    ctx->forceKeyframe = FALSE;

    if (cl->ublen + sz_rfbFramebufferUpdateRectHeader + sz_rfbH264Header > UPDATE_BUF_SIZE) {
        if (!rfbSendUpdateBuf(cl)) {
            return FALSE;
        }
    }

    memcpy(&cl->updateBuf[cl->ublen], &rect, sz_rfbFramebufferUpdateRectHeader);
    cl->ublen += sz_rfbFramebufferUpdateRectHeader;
    memcpy(&cl->updateBuf[cl->ublen], &hdr, sz_rfbH264Header);
    cl->ublen += sz_rfbH264Header;

    if (!rfbSendUpdateBuf(cl)) {
        return FALSE;
    }

    if (rfbWriteExact(cl, (char *)ctx->encodeBuffer, (int)encodedSize) < 0) {
        rfbLogPerror(H264_LOG_PREFIX "write");
        rfbCloseClient(cl);
        return FALSE;
    }

    rawEquivalent = w * h * (cl->format.bitsPerPixel / 8);
    rfbStatRecordEncodingSent(cl, rfbEncodingH264,
                              sz_rfbFramebufferUpdateRectHeader + sz_rfbH264Header + (int)encodedSize,
                              sz_rfbFramebufferUpdateRectHeader + sz_rfbH264Header + rawEquivalent);

    if (flags & rfbH264FlagResetAllContexts) {
        cl->h264NeedsResetAll = FALSE;
    }

    rfbClientH264MoveContextToTail(cl, ctx);

    return TRUE;
}

#endif /* LIBVNCSERVER_HAVE_LIBAVCODEC */
