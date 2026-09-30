/*
 * Copyright (c) 2022, Vastai Tech. All rights reserved
 *
 * The information contained herein is confidential
 * property of Company. The user, copying, transfer or
 * disclosure of such information is prohibited except
 * by express written agreement with VASTAITECH.
 */

#include "jpeg_encoder.h"

#include "va_log.h"
#include "va_vdata_internal.h"
#include "vmpp_enc_defs.h"

// vsi headers
#include "ewl.h"
#include "jpegencapi.h"

/* Default quantization level, see JpegEncCfg.qLevel. */
#define DEFAULT_Q_LEVEL 1
/* Quantization level selecting the external quantization table. */
#define QLEVEL_USER_TABLE 10
/* Input stride alignment used by the encoder, 1 << 7 = 128 bytes. */
#define DEFAULT_INPUT_ALIGNMENT_EXP 7
/* Worst case stream size is 2 bytes per pixel. */
#define STREAM_SIZE_PER_PIXEL 2
/* Valid range of the JPEG lossless prediction mode required by the VSI encoder. */
#define JPEG_LOSSLESS_PREDICT_MODE_MIN 1
#define JPEG_LOSSLESS_PREDICT_MODE_MAX 7
/* Point transform is written to the stream with 4 bits. */
#define JPEG_LOSSLESS_PTRANS_VALUE_MAX 15

static void EncStreamSegmentReady(void *cb_data) { UNUSED_PARAMETER(cb_data); }

static void jpeg_cfg_default_init(JpegEncCfg *cfg);

static void clear_out_buffer_list(struct va_enc_channel *chn) {
    pthread_mutex_lock(&chn->enc_out_buffer_mutex);
    for (uint32_t i = 0; i < chn->outbufNum; i++) {
        if (chn->enc_out_buffer[i].data) {
            LOG_DEBUG(ENC, "clear_data_buffer %d, %p", i, chn->enc_out_buffer[i].data);
            free(chn->enc_out_buffer[i].data);
            chn->enc_out_buffer[i].data = NULL;
            chn->enc_out_buffer[i].size = 0;
        }
        chn->enc_out_buffer[i].used = 0;
    }
    pthread_mutex_unlock(&chn->enc_out_buffer_mutex);
}

static uint8_t *get_idle_out_buffer(struct va_enc_channel *chn, uint32_t size)
{
    uint8_t *priv_buf = NULL;
    uint32_t i = 0;
    pthread_mutex_lock(&chn->enc_out_buffer_mutex);
    for (i = 0; i < chn->outbufNum; i++) {
        if (!chn->enc_out_buffer[i].used)
            break;
    }
    if (i >= chn->outbufNum) {
        LOG_ERROR(ENC, "No idle private buffer avaliable.");
        pthread_mutex_unlock(&chn->enc_out_buffer_mutex);
        return NULL;
    }

    if (!chn->enc_out_buffer[i].data) {
        chn->enc_out_buffer[i].data = (uint8_t *)malloc(size);
        if (!chn->enc_out_buffer[i].data) {
            LOG_ERROR(ENC, "Fail to malloc private buffer.");
            pthread_mutex_unlock(&chn->enc_out_buffer_mutex);
            return NULL;
        }
        chn->enc_out_buffer[i].size = size;
    } else {
        if (size > chn->enc_out_buffer[i].size) {
            free(chn->enc_out_buffer[i].data);
            chn->enc_out_buffer[i].data = (uint8_t *)malloc(size);
            if (!chn->enc_out_buffer[i].data) {
                LOG_ERROR(ENC, "Fail to malloc private buffer.");
                pthread_mutex_unlock(&chn->enc_out_buffer_mutex);
                return NULL;
            }
            chn->enc_out_buffer[i].size = size;
        }
    }

    priv_buf = chn->enc_out_buffer[i].data;
    chn->enc_out_buffer[i].used = 1;

    pthread_mutex_unlock(&chn->enc_out_buffer_mutex);
    return priv_buf;
}

static void set_out_buffer_idle(struct va_enc_channel *chn, uint8_t *data)
{
    pthread_mutex_lock(&chn->enc_out_buffer_mutex);
    for (uint32_t i = 0; i < chn->outbufNum; i++) {
        if (chn->enc_out_buffer[i].data == data) {
            chn->enc_out_buffer[i].used = 0;
            break;
        }
    }
    pthread_mutex_unlock(&chn->enc_out_buffer_mutex);
}

static JpegEncFrameType toVSIFrameType(vmppPixelFormat type)
{
    JpegEncFrameType jType = JPEGENC_YUV420_SEMIPLANAR;
    switch (type) {
    case vmpp_PIX_FMT_YUV420P:
        jType = JPEGENC_YUV420_PLANAR;
        break;
    case vmpp_PIX_FMT_NV12:
        jType = JPEGENC_YUV420_SEMIPLANAR;
        break;
    case vmpp_PIX_FMT_NV21:
        jType = JPEGENC_YUV420_SEMIPLANAR_VU;
        break;
    case vmpp_PIX_FMT_RGB24:
        jType = JPEGENC_RGB888;
        break;
    case vmpp_PIX_FMT_BGR24:
        jType = JPEGENC_BGR888;
        break;
    case vmpp_PIX_FMT_YUV420_PLANAR_10BIT_P010:
        jType = JPEGENC_YVU420_PLANAR_10BIT_P010;
        break;
    case vmpp_PIX_FMT_GRAY8:
    case vmpp_PIX_FMT_ARGB:
    case vmpp_PIX_FMT_RGBA:
    case vmpp_PIX_FMT_ABGR:
    case vmpp_PIX_FMT_BGRA:
    default:
        break;
    }
    return jType;
}

/*------------------------------------------------------------------------------
    Calculate the luma/chroma/total size of one input picture, the strides are
    the ones required by the hardware. Same as getAlignedPicSizebyFormat() of
    JpegTestBench.c.
------------------------------------------------------------------------------*/
static void getAlignedPicSizebyFormat(JpegEncFrameType type, uint32_t width, uint32_t height,
                                      uint32_t alignment, uint64_t *lumaSize, uint64_t *chromaSize,
                                      uint64_t *pictureSize, uint32_t *lumaStride,
                                      uint32_t *chromaStride, uint32_t scanType)
{
    uint32_t luma_stride = 0, chroma_stride = 0;
    uint64_t luma_size = 0, chroma_size = 0;

    JpegEncGetAlignedStride((int)width, (i32)type, &luma_stride, &chroma_stride, alignment,
                            scanType);

    switch (type) {
    case JPEGENC_Y8b:
    case JPEGENC_Y10bWL:
    case JPEGENC_Y10bWH:
        luma_size = (uint64_t)luma_stride * height;
        chroma_size = 0;
        break;
    case JPEGENC_YUV420_PLANAR:
    case JPEGENC_YVU420_PLANAR:
    case JPEGENC_YUV420_I010:
        luma_size = (uint64_t)luma_stride * height;
        chroma_size = (uint64_t)chroma_stride * height / 2 * 2;
        break;
    case JPEGENC_YUV420_SEMIPLANAR:
    case JPEGENC_YUV420_SEMIPLANAR_VU:
    case JPEGENC_YUV420_MS_P010:
    case JPEGENC_YVU420_PLANAR_10BIT_P010:
        luma_size = (uint64_t)luma_stride * height;
        chroma_size = (uint64_t)chroma_stride * height / 2;
        break;
    case JPEGENC_YUV422SP_888:
    case JPEGENC_YVU422SP_888:
        luma_size = (uint64_t)luma_stride * height;
        chroma_size = (uint64_t)chroma_stride * height;
        break;
    case JPEGENC_RGB888:
    case JPEGENC_BGR888:
    case JPEGENC_RGB101010:
    case JPEGENC_BGR101010:
    case JPEGENC_RGBX8888:
    case JPEGENC_BGRX8888:
    case JPEGENC_RGBX1010102:
    case JPEGENC_BGRX1010102:
    case JPEGENC_YUV422_INTERLEAVED_YUYV:
    case JPEGENC_YUV422_INTERLEAVED_UYVY:
    case JPEGENC_YUV422_INTERLEAVED_YVYU:
    case JPEGENC_YUV422_INTERLEAVED_VYUY:
    case JPEGENC_RGB565:
    case JPEGENC_BGR565:
    case JPEGENC_RGB555:
    case JPEGENC_BGR555:
    case JPEGENC_RGB444:
    case JPEGENC_BGR444:
    case JPEGENC_RGB888_24BIT:
    case JPEGENC_BGR888_24BIT:
    case JPEGENC_RBG888_24BIT:
    case JPEGENC_GBR888_24BIT:
    case JPEGENC_BRG888_24BIT:
    case JPEGENC_GRB888_24BIT:
        /* Packed formats, one plane only. For the super tile scan the height is
           aligned to 64, same as in getAlignedPicSizebyFormat() of
           JpegTestBench.c. */
        luma_size = (uint64_t)luma_stride * height;
        if (scanType == JPEG_SUPERTILEX_SCAN)
            luma_size = (uint64_t)luma_stride * ((height + 63) / 64);
        chroma_size = 0;
        break;
    default:
        /* Unsupported format, the caller detects it through pictureSize == 0. */
        luma_size = 0;
        chroma_size = 0;
        break;
    }

    if (lumaSize != NULL)
        *lumaSize = luma_size;
    if (chromaSize != NULL)
        *chromaSize = chroma_size;
    if (pictureSize != NULL)
        *pictureSize = luma_size + chroma_size;
    if (lumaStride != NULL)
        *lumaStride = luma_stride;
    if (chromaStride != NULL)
        *chromaStride = chroma_stride;
}

static EWLLinearMem_t *getIdleOutputBuffer(struct jpeg_encoder_private_context *ctx)
{
    pthread_mutex_lock(&ctx->outbufMemMutex);
    for (int i = 0; i < MAX_STRM_BUF_NUM; i++) {
        if (!ctx->outbufMem[i].used) {
            ctx->outbufMem[i].used = 1;
            pthread_mutex_unlock(&ctx->outbufMemMutex);
            return &(ctx->outbufMem[i].mem);
        }
    }
    pthread_mutex_unlock(&ctx->outbufMemMutex);
    return NULL;
}

static void setOutputBufferIdle(struct jpeg_encoder_private_context *ctx, EWLLinearMem_t *buf)
{
    if (!ctx || !buf)
        return;

    pthread_mutex_lock(&ctx->outbufMemMutex);
    for (int i = 0; i < MAX_STRM_BUF_NUM; i++) {
        if (ctx->outbufMem[i].mem.virtualAddress == buf->virtualAddress) {
            ctx->outbufMem[i].used = 0;
        }
    }
    pthread_mutex_unlock(&ctx->outbufMemMutex);
}

/*------------------------------------------------------------------------------
    Copy one plane of the input frame into the input picture buffer, the source
    and the destination may have different strides.
------------------------------------------------------------------------------*/
static void copyPlane(uint8_t *dst, uint32_t dstStride, const uint8_t *src, uint32_t srcStride,
                      uint32_t rows)
{
    uint32_t bytes = (dstStride < srcStride) ? dstStride : srcStride;

    for (uint32_t row = 0; row < rows; row++)
        memcpy(dst + (uint64_t)row * dstStride, src + (uint64_t)row * srcStride, bytes);
}

/*------------------------------------------------------------------------------
    Copy one frame located in host memory into the input picture buffer which is
    visible for the hardware. Same function as ReadPic() of JpegTestBench.c.
------------------------------------------------------------------------------*/
static vmppResult fillInputPicture(struct va_enc_channel *chn, vmppFrame *frame, uint64_t lumaSize,
                                   uint64_t chromaSize)
{
    struct jpeg_encoder_private_context *ctx =
        (struct jpeg_encoder_private_context *)chn->private_context;
    uint32_t alignment = 1u << ctx->cfg.exp_of_input_alignment;
    uint32_t width = (frame->width + 1) & (~1u);
    uint32_t height = (frame->height + 1) & (~1u);
    uint32_t lumaStride = 0, chromaStride = 0;
    uint32_t srcLumaStride = frame->stride[0] ? frame->stride[0] : width;
    uint8_t *dst = (uint8_t *)ctx->pictureMem.virtualAddress;

    if (dst == NULL) {
        LOG_ERROR(ENC, "Input picture buffer is not allocated.");
        return vmpp_RSLT_ERR_NO_MEMORY;
    }

    if (frame->data[0] == NULL) {
        LOG_ERROR(ENC, "Invalid frame data.");
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    JpegEncGetAlignedStride((int)width, (i32)ctx->cfg.frameType, &lumaStride, &chromaStride,
                            alignment, ctx->cfg.scanType);

    switch (ctx->cfg.frameType) {
    case JPEGENC_YUV420_PLANAR:
    case JPEGENC_YVU420_PLANAR:
    case JPEGENC_YUV420_I010: {
        uint32_t srcChromaStride = frame->stride[1] ? frame->stride[1] : chromaStride;
        if (frame->data[1] == NULL || frame->data[2] == NULL) {
            LOG_ERROR(ENC, "Planar format needs 3 planes, but data[1] %p, data[2] %p.",
                      frame->data[1], frame->data[2]);
            return vmpp_RSLT_ERR_INVALID_PARAMS;
        }
        copyPlane(dst, lumaStride, frame->data[0], srcLumaStride, height);
        copyPlane(dst + lumaSize, chromaStride, frame->data[1], srcChromaStride, height / 2);
        copyPlane(dst + lumaSize + chromaSize / 2, chromaStride, frame->data[2], srcChromaStride,
                  height / 2);
        break;
    }
    case JPEGENC_YUV420_SEMIPLANAR:
    case JPEGENC_YUV420_SEMIPLANAR_VU:
    case JPEGENC_YUV420_MS_P010:
    case JPEGENC_YVU420_PLANAR_10BIT_P010: {
        /* The chroma plane may be a part of the buffer of the luma plane. */
        const uint8_t *srcChroma = frame->data[1] ? frame->data[1]
                                                  : frame->data[0] + (uint64_t)srcLumaStride * height;
        uint32_t srcChromaStride = frame->stride[1] ? frame->stride[1] : srcLumaStride;
        copyPlane(dst, lumaStride, frame->data[0], srcLumaStride, height);
        copyPlane(dst + lumaSize, chromaStride, srcChroma, srcChromaStride, height / 2);
        break;
    }
    case JPEGENC_YUV422SP_888:
    case JPEGENC_YVU422SP_888: {
        const uint8_t *srcChroma = frame->data[1] ? frame->data[1]
                                                  : frame->data[0] + (uint64_t)srcLumaStride * height;
        uint32_t srcChromaStride = frame->stride[1] ? frame->stride[1] : srcLumaStride;
        copyPlane(dst, lumaStride, frame->data[0], srcLumaStride, height);
        copyPlane(dst + lumaSize, chromaStride, srcChroma, srcChromaStride, height);
        break;
    }
    default:
        /* One plane only, e.g. the RGB formats. */
        copyPlane(dst, lumaStride, frame->data[0], srcLumaStride, height);
        break;
    }

    (void)EWLSyncMemData(&ctx->pictureMem, 0, (u32)(lumaSize + chromaSize), HOST_TO_DEVICE);

    return vmpp_RSLT_OK;
}

/*------------------------------------------------------------------------------
    Allocate the resources needed by one picture: the input picture buffer for
    host memory input and one output stream buffer. buf is the hardware output
    buffer, *userBuf receives the output buffer owned by the channel.
------------------------------------------------------------------------------*/
static vmppResult allocRes(struct va_enc_channel *chn, vmppFrame *frame, EWLLinearMem_t *buf,
                           uint8_t **userBuf, uint32_t timeout)
{
    i32 ret;
    uint64_t start, tick;
    uint64_t lumaSize = 0, chromaSize = 0, pictureSize = 0;
    uint32_t width = (frame->width + 1) & (~1u);
    uint32_t height = (frame->height + 1) & (~1u);
    uint32_t streamBufTotalSize;
    struct jpeg_encoder_private_context *ctx =
        (struct jpeg_encoder_private_context *)chn->private_context;
    uint32_t inputAlignment = 1u << ctx->cfg.exp_of_input_alignment;
    const void *ewlInst = JpegEncGetEwl((JpegEncInst)chn->codec_inst);

    if (ewlInst == NULL) {
        LOG_ERROR(ENC, "Failed to get the EWL instance.");
        return vmpp_RSLT_ERR_ENC_EWL;
    }

    /* Two bytes per pixel is enough for the worst case, plus the JPEG header. */
    streamBufTotalSize =
        width * height * STREAM_SIZE_PER_PIXEL + JPEGENC_STREAM_MIN_BUF0_SIZE;

    if (frame->memoryType == vmpp_MEM_HOST) {
        getAlignedPicSizebyFormat(ctx->cfg.frameType, width, height, inputAlignment, &lumaSize,
                                  &chromaSize, &pictureSize, NULL, NULL, ctx->cfg.scanType);
        if (pictureSize == 0) {
            LOG_ERROR(ENC, "Unsupported input format: %d.", frame->pixelFormat);
            return vmpp_RSLT_ERR_INVALID_PARAMS;
        }

        if (ctx->pictureMem.virtualAddress == NULL || ctx->pictureMem.size < pictureSize) {
            if (ctx->pictureMem.virtualAddress != NULL) {
                EWLFreeLinear(ewlInst, &ctx->pictureMem);
                memset(&ctx->pictureMem, 0, sizeof(ctx->pictureMem));
            }
            ret = EWLMallocLinear(ewlInst, (u32)pictureSize, 0, &ctx->pictureMem);
            if (ret != EWL_OK) {
                LOG_ERROR(ENC, "Failed to allocate input picture: ERR %d, SIZE %u", ret,
                          (uint32_t)pictureSize);
                memset(&ctx->pictureMem, 0, sizeof(ctx->pictureMem));
                return vmpp_RSLT_ERR_ENC_EWL;
            }
        }
    } else {
        /* The input picture is provided by the caller, only the sizes are needed. */
        uint32_t stride = frame->stride[0] ? frame->stride[0] : frame->width;
        lumaSize = (uint64_t)stride * height;
        chromaSize = lumaSize / 2;
        pictureSize = lumaSize + chromaSize;
    }

    JpegSetLumaSize((JpegEncInst)chn->codec_inst, lumaSize, 0);
    JpegSetChromaSize((JpegEncInst)chn->codec_inst, chromaSize, 0);

    /* One buffer of the channel is used to hand the stream over to the caller. */
    start = va_gettime_ns();
    *userBuf = get_idle_out_buffer(chn, streamBufTotalSize);
    while (*userBuf == NULL) {
        tick = va_gettime_ns();
        if ((float)(tick - start) / 1000000.0 > timeout) {
            LOG_WARN(ENC, "Timeout for JPEG encoder: No Output User Buffer");
            return vmpp_RSLT_ERR_NO_BUFFER;
        }
        sched_yield();
        *userBuf = get_idle_out_buffer(chn, streamBufTotalSize);
    }

    if (buf->virtualAddress != NULL && buf->size < streamBufTotalSize) {
        EWLFreeLinear(ewlInst, buf);
        memset(buf, 0, sizeof(EWLLinearMem_t));
    }

    if (buf->virtualAddress == NULL) {
        memset(buf, 0, sizeof(EWLLinearMem_t));
        ret = EWLMallocLinear(ewlInst, streamBufTotalSize, 0, buf);
        if (ret != EWL_OK) {
            LOG_ERROR(ENC, "Failed to allocate output buffer: ERR %d, SIZE %u", ret,
                      streamBufTotalSize);
            memset(buf, 0, sizeof(EWLLinearMem_t));
            set_out_buffer_idle(chn, *userBuf);
            *userBuf = NULL;
            return vmpp_RSLT_ERR_ENC_EWL;
        }
    }

    return vmpp_RSLT_OK;
}

static void freeRes(struct va_enc_channel *chn)
{
    struct jpeg_encoder_private_context *ctx =
        (struct jpeg_encoder_private_context *)chn->private_context;
    const void *ewlInst = NULL;

    if (ctx == NULL)
        return;

    ewlInst = JpegEncGetEwl((JpegEncInst)chn->codec_inst);
    if (ewlInst == NULL)
        return;

    if (ctx->pictureMem.virtualAddress != NULL) {
        EWLFreeLinear(ewlInst, &ctx->pictureMem);
        memset(&ctx->pictureMem, 0, sizeof(ctx->pictureMem));
    }

    pthread_mutex_lock(&ctx->outbufMemMutex);
    for (int i = 0; i < MAX_STRM_BUF_NUM; i++) {
        if (ctx->outbufMem[i].mem.virtualAddress != NULL) {
            EWLFreeLinear(ewlInst, &ctx->outbufMem[i].mem);
            memset(&ctx->outbufMem[i].mem, 0, sizeof(EWLLinearMem_t));
        }
        ctx->outbufMem[i].used = 0;
    }
    pthread_mutex_unlock(&ctx->outbufMemMutex);
}

vmppResult jpeg_encoder_create_chn(struct va_enc_channel *chn, encChannelParameters *param) {
    JpegEncRet ret = JPEGENC_OK;
    JpegEncCfg *cfg = NULL;
    struct jpeg_encoder_private_context *ctx = NULL;
    uint64_t lumaSize = 0, chromaSize = 0, pictureSize = 0;

    if (!param || !chn) {
        LOG_ERROR(ENC, "Invalid parameters: param %p, chn %p", param, chn);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    ctx = malloc(sizeof(struct jpeg_encoder_private_context));
    if (!ctx) {
        LOG_ERROR(ENC, "Fail to malloc private context for JPEG encoder.");
        return vmpp_RSLT_ERR_NO_MEMORY;
    }

    memset(ctx, 0, sizeof(struct jpeg_encoder_private_context));
    pthread_mutex_init(&ctx->outbufMemMutex, NULL);
    chn->private_context = ctx;
    cfg = &ctx->cfg;
    jpeg_cfg_default_init(cfg);

    /* Lossless mode */
    if (param->jpegConfig.losslessEn) {
        /* The VSI encoder asserts (aborts the process) when lossless coding is requested
         * without a valid prediction mode, so reject such a configuration up front. */
        if (param->jpegConfig.predictMode < JPEG_LOSSLESS_PREDICT_MODE_MIN ||
            param->jpegConfig.predictMode > JPEG_LOSSLESS_PREDICT_MODE_MAX ||
            param->jpegConfig.ptransValue > JPEG_LOSSLESS_PTRANS_VALUE_MAX) {
            LOG_ERROR(ENC,
                      "Invalid lossless configuration: predictMode %u (valid [%u, %u]), "
                      "ptransValue %u (valid [0, %u]).",
                      param->jpegConfig.predictMode, JPEG_LOSSLESS_PREDICT_MODE_MIN,
                      JPEG_LOSSLESS_PREDICT_MODE_MAX, param->jpegConfig.ptransValue,
                      JPEG_LOSSLESS_PTRANS_VALUE_MAX);
            pthread_mutex_destroy(&ctx->outbufMemMutex);
            free(ctx);
            chn->private_context = NULL;
            return vmpp_RSLT_ERR_INVALID_PARAMS;
        }
    }

    cfg->losslessEn = param->jpegConfig.losslessEn;
    cfg->predictMode = param->jpegConfig.predictMode;
    cfg->ptransValue = param->jpegConfig.ptransValue;
    cfg->rotation = (JpegEncPictureRotation)param->jpegConfig.rotation;

    cfg->inputWidth = (param->jpegConfig.codingWidth + 15) & (~15);
    cfg->inputHeight = (param->jpegConfig.codingHeight + 1) & (~0x1);
    if (cfg->rotation && cfg->rotation != JPEGENC_ROTATE_180) {
        cfg->codingWidth = param->jpegConfig.codingHeight;
        cfg->codingHeight = param->jpegConfig.codingWidth;
    } else {
        cfg->codingWidth = param->jpegConfig.codingWidth;
        cfg->codingHeight = param->jpegConfig.codingHeight;
    }

    /* Quantization level, level 10 selects the user defined quantization table. */
    if (param->jpegConfig.qLevel == USER_DEFINED_QTABLE) {
        cfg->qTableLuma = param->jpegConfig.qTableLuma;
        cfg->qTableChroma = param->jpegConfig.qTableChroma;
        cfg->qLevel = QLEVEL_USER_TABLE;
    } else {
        cfg->qLevel = (param->jpegConfig.qLevel > QLEVEL_USER_TABLE) ? QLEVEL_USER_TABLE
                                                                    : param->jpegConfig.qLevel;
    }

    cfg->restartInterval = 0;
    cfg->codingType = JPEGENC_WHOLE_FRAME;
    cfg->frameType = toVSIFrameType(param->jpegConfig.frameType);
    cfg->unitsType = JPEGENC_NO_UNITS;
    cfg->markerType = JPEGENC_SINGLE_MARKER;
    cfg->codingMode = JPEGENC_420_MODE;

    cfg->streamMultiSegCbFunc = &EncStreamSegmentReady;
    cfg->streamMultiSegCbData = &ctx->streamSegCtl;

    /* Rate control */
    cfg->frameRateNum = 1;
    cfg->frameRateDenom = 1;
    cfg->qpmin = param->jpegConfig.qpmin;
    cfg->qpmax = param->jpegConfig.qpmax ? param->jpegConfig.qpmax : 51;
    cfg->fixedQP = param->jpegConfig.fixedQP;
    cfg->rcMode = JPEGENC_CBR;
    cfg->picQpDeltaMax = 3;
    cfg->picQpDeltaMin = -2;
    cfg->exp_of_input_alignment = DEFAULT_INPUT_ALIGNMENT_EXP;

    cfg->comLength = param->jpegConfig.comLength;
    cfg->pCom = param->jpegConfig.pCom;
    cfg->inputLineBufDepth = 1;
    cfg->constCb = 128;
    cfg->constCr = 128;
    cfg->enc_dev = param->encDevice;
    cfg->mem_dev = param->memDevice;

    ret = JpegEncInit(cfg, (JpegEncInst *)&(chn->codec_inst), NULL);
    if (ret != JPEGENC_OK) {
        LOG_ERROR(ENC, "Failed to initialize the encoder. Error code: %8i\n", ret);
        pthread_mutex_destroy(&ctx->outbufMemMutex);
        free(ctx);
        chn->private_context = NULL;
        return vmpp_RSLT_ERR_ENC_INIT;
    }

    /* The input sizes must be known before the internal memories are allocated. */
    getAlignedPicSizebyFormat(cfg->frameType, cfg->inputWidth, cfg->inputHeight,
                              1u << cfg->exp_of_input_alignment, &lumaSize, &chromaSize,
                              &pictureSize, NULL, NULL, cfg->scanType);
    JpegSetLumaSize((JpegEncInst)chn->codec_inst, lumaSize, 0);
    JpegSetChromaSize((JpegEncInst)chn->codec_inst, chromaSize, 0);

    ret = JpegEncSetPictureSize((JpegEncInst)chn->codec_inst, cfg);
    if (ret != JPEGENC_OK) {
        LOG_ERROR(ENC, "JpegEncSetPictureSize failed: %d", ret);
        JpegEncRelease((JpegEncInst)chn->codec_inst);
        chn->codec_inst = NULL;
        pthread_mutex_destroy(&ctx->outbufMemMutex);
        free(ctx);
        chn->private_context = NULL;
        return vmpp_RSLT_ERR_ENC_SET_PIC_SIZE;
    }

    if (param->outbufNum == 0)
        chn->outbufNum = VA_ENC_DEF_OUTPUT_BUFFER;
    else if (param->outbufNum > VA_ENC_MAX_OUTPUT_BUFFER)
        chn->outbufNum = VA_ENC_MAX_OUTPUT_BUFFER;
    else
        chn->outbufNum = param->outbufNum;

    return vmpp_RSLT_OK;
}

vmppResult jpeg_encoder_destory_chn(struct va_enc_channel *chn) {
    struct jpeg_encoder_private_context *ctx = NULL;

    if (!chn)
        return vmpp_RSLT_ERR_INVALID_PARAMS;

    ctx = (struct jpeg_encoder_private_context *)chn->private_context;
    if (ctx != NULL) {
        /* The EWL instance of the encoder is needed to free the memories. */
        freeRes(chn);
        pthread_mutex_destroy(&ctx->outbufMemMutex);
        free(ctx);
        chn->private_context = NULL;
    }

    if (chn->codec_inst) {
        JpegEncRelease((JpegEncInst)chn->codec_inst);
        chn->codec_inst = NULL;
    }

    clear_out_buffer_list(chn);

    return vmpp_RSLT_OK;
}

vmppResult jpeg_encoder_alloc_frame(struct va_enc_channel *chn, vmppFrame *frame) {
    struct jpeg_encoder_private_context *ctx = NULL;
    uint32_t width, height, alignment;
    uint32_t lumaStride = 0, chromaStride = 0;
    uint64_t lumaSize = 0, chromaSize = 0, pictureSize = 0;
    uint8_t *buffer = NULL;

    if (!chn || !frame || !chn->private_context) {
        LOG_ERROR(ENC, "Invalid parameters: chn %p, frame %p", chn, frame);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    if (frame->width == 0 || frame->height == 0) {
        LOG_ERROR(ENC, "Invalid frame size: %ux%u.", frame->width, frame->height);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    ctx = (struct jpeg_encoder_private_context *)chn->private_context;
    width = (frame->width + 1) & (~1u);
    height = (frame->height + 1) & (~1u);
    alignment = 1u << ctx->cfg.exp_of_input_alignment;

    getAlignedPicSizebyFormat(ctx->cfg.frameType, width, height, alignment, &lumaSize, &chromaSize,
                              &pictureSize, &lumaStride, &chromaStride, ctx->cfg.scanType);
    if (pictureSize == 0) {
        LOG_ERROR(ENC, "Unsupported input format: %d.", frame->pixelFormat);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    frame->dataSize = (uint32_t)pictureSize;
    frame->stride[0] = lumaStride;
    frame->stride[1] = chromaStride;
    frame->stride[2] = chromaStride;

    if (frame->memoryType == vmpp_MEM_HOST && frame->data[0] == NULL) {
        buffer = (uint8_t *)malloc((size_t)pictureSize);
        if (!buffer) {
            LOG_ERROR(ENC, "Fail to malloc input frame buffer, size %u.", (uint32_t)pictureSize);
            return vmpp_RSLT_ERR_NO_MEMORY;
        }
        memset(buffer, 0, (size_t)pictureSize);
        frame->data[0] = buffer;
        switch (ctx->cfg.frameType) {
        case JPEGENC_YUV420_PLANAR:
        case JPEGENC_YVU420_PLANAR:
        case JPEGENC_YUV420_I010:
            frame->data[1] = buffer + lumaSize;
            frame->data[2] = buffer + lumaSize + chromaSize / 2;
            break;
        case JPEGENC_YUV420_SEMIPLANAR:
        case JPEGENC_YUV420_SEMIPLANAR_VU:
        case JPEGENC_YUV420_MS_P010:
        case JPEGENC_YVU420_PLANAR_10BIT_P010:
        case JPEGENC_YUV422SP_888:
        case JPEGENC_YVU422SP_888:
            frame->data[1] = buffer + lumaSize;
            frame->data[2] = NULL;
            break;
        default:
            frame->data[1] = NULL;
            frame->data[2] = NULL;
            break;
        }
    }

    /* The caller expects a host buffer, make sure it really has one. */
    if (frame->memoryType == vmpp_MEM_HOST && frame->data[0] == NULL) {
        LOG_ERROR(ENC, "No input frame buffer allocated: memoryType %d, data[0] %p, size %u.",
                  frame->memoryType, frame->data[0], frame->dataSize);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    return vmpp_RSLT_OK;
}

vmppResult jpeg_encoder_free_frame(struct va_enc_channel *chn, vmppFrame *frame) {
    if (!chn || !frame)
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    if (frame->memoryType == vmpp_MEM_HOST && frame->data[0]) {
        free(frame->data[0]);
        frame->data[0] = NULL;
    }
    return vmpp_RSLT_OK;
}

vmppResult jpeg_encode_frame(struct va_enc_channel *chn, vmppFrame *frame, vmppStream *stream, uint32_t timeout) {
    struct jpeg_encoder_private_context *ctx = NULL;
    EWLLinearMem_t *outputBuffer = NULL;
    uint8_t *userBuf = NULL;
    uint64_t lumaSize = 0, chromaSize = 0;
    uint64_t dec400LumaTblSize = 0, dec400ChrTblSize = 0;
    JpegEncIn encIn;
    JpegEncOut encOut;
    JpegEncRet encRet = JPEGENC_OK;
    vmppResult ret = vmpp_RSLT_OK;

    if (!chn || !frame || !stream || !chn->private_context || !chn->codec_inst) {
        LOG_ERROR(ENC, "Invalid parameters: chn %p, frame %p, stream %p", chn, frame, stream);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    ctx = (struct jpeg_encoder_private_context *)chn->private_context;
    memset(&encIn, 0, sizeof(encIn));
    memset(&encOut, 0, sizeof(encOut));

    outputBuffer = getIdleOutputBuffer(ctx);
    if (!outputBuffer) {
        LOG_ERROR(ENC, "No available output buffer.");
        return vmpp_RSLT_ERR_NO_BUFFER;
    }

    ret = allocRes(chn, frame, outputBuffer, &userBuf, timeout);
    if (ret != vmpp_RSLT_OK) {
        setOutputBufferIdle(ctx, outputBuffer);
        return ret;
    }

    JpegGetLumaSize((JpegEncInst)chn->codec_inst, &lumaSize, &dec400LumaTblSize);
    JpegGetChromaSize((JpegEncInst)chn->codec_inst, &chromaSize, &dec400ChrTblSize);

    /* Output stream buffer, the header is written by the software and the
       entropy coded data is written by the hardware. */
    encIn.frameHeader = 1;
    encIn.pOutBuf[0] = (u8 *)outputBuffer->virtualAddress;
    encIn.busOutBuf[0] = outputBuffer->busAddress;
    encIn.outBufSize[0] = outputBuffer->size;

    /* Input picture */
    if (frame->memoryType == vmpp_MEM_DEVICE) {
        encIn.busLum = frame->busAddress[0];
        encIn.busCb = frame->busAddress[1] ? frame->busAddress[1] : encIn.busLum + lumaSize;
        encIn.busCr = frame->busAddress[2] ? frame->busAddress[2] : encIn.busCb + chromaSize / 2;
        if (chromaSize == 0)
            encIn.busCb = encIn.busCr = encIn.busLum;
        /* Device memory is not accessible by the CPU. */
        encIn.pLum = encIn.pCb = encIn.pCr = NULL;
    } else {
        ret = fillInputPicture(chn, frame, lumaSize, chromaSize);
        if (ret != vmpp_RSLT_OK)
            goto end;

        encIn.busLum = ctx->pictureMem.busAddress;
        encIn.busCb = encIn.busLum + lumaSize;
        encIn.busCr = encIn.busCb + chromaSize / 2;
        /* Virtual addresses of the input, used by the software encoder. */
        encIn.pLum = (const u8 *)ctx->pictureMem.virtualAddress;
        encIn.pCb = encIn.pLum + lumaSize;
        encIn.pCr = encIn.pCb + chromaSize / 2;
        if (chromaSize == 0) {
            encIn.busCb = encIn.busCr = encIn.busLum;
            encIn.pCb = encIn.pCr = encIn.pLum;
        }
    }

    encRet = JpegEncEncode((JpegEncInst)chn->codec_inst, &encIn, &encOut);
    switch (encRet) {
    case JPEGENC_FRAME_READY:
        if (encOut.jfifSize > outputBuffer->size) {
            LOG_ERROR(ENC, "Output buffer overflow: %u > %u.", encOut.jfifSize,
                      outputBuffer->size);
            ret = vmpp_RSLT_ERR_ENC_SEND_FRAME;
            break;
        }

        /* Make the stream visible for the CPU and hand it over to the caller. */
        if (encOut.jfifSize > encOut.headerSize)
            (void)EWLSyncMemData(outputBuffer, encOut.headerSize,
                                 encOut.jfifSize - encOut.headerSize, DEVICE_TO_HOST);
        if (encOut.jfifSize > 0)
            memcpy(userBuf, outputBuffer->virtualAddress, encOut.jfifSize);

        stream->stream = userBuf;
        stream->len = encOut.jfifSize;
        stream->pts = frame->pts;
        stream->inputBusAddress = frame->busAddress[0];
        ret = vmpp_RSLT_OK;
        break;
    case JPEGENC_RESTART_INTERVAL:
        /* Not reached with JPEGENC_WHOLE_FRAME coding, kept consistent with the
           frame ready case: the entropy coded data is written by the hardware,
           it has to be synced before it is handed over to the caller. */
        if (encOut.jfifSize > outputBuffer->size) {
            LOG_ERROR(ENC, "Output buffer overflow: %u > %u.", encOut.jfifSize,
                      outputBuffer->size);
            ret = vmpp_RSLT_ERR_ENC_SEND_FRAME;
            break;
        }
        if (encOut.jfifSize > encOut.headerSize)
            (void)EWLSyncMemData(outputBuffer, encOut.headerSize,
                                 encOut.jfifSize - encOut.headerSize, DEVICE_TO_HOST);
        if (encOut.jfifSize > 0)
            memcpy(userBuf, outputBuffer->virtualAddress, encOut.jfifSize);

        stream->stream = userBuf;
        stream->len = encOut.jfifSize;
        stream->pts = frame->pts;
        stream->inputBusAddress = frame->busAddress[0];
        ret = vmpp_RSLT_OK;
        break;
    case JPEGENC_OUTPUT_BUFFER_OVERFLOW:
        LOG_ERROR(ENC, "Output buffer overflow: %u bytes.", encOut.jfifSize);
        ret = vmpp_RSLT_ERR_ENC_SEND_FRAME;
        break;
    default:
        LOG_ERROR(ENC, "JpegEncEncode failed: %d", encRet);
        ret = vmpp_RSLT_ERR_ENC_SEND_FRAME;
        break;
    }

end:
    /* The hardware output buffer is only used as the target of the encoder, the
       stream is copied into the channel buffer, so it can be released here for
       every outcome. Returning it to the pool on the success path as well is
       required, otherwise the pool is drained after MAX_STRM_BUF_NUM frames. */
    setOutputBufferIdle(ctx, outputBuffer);

    if (ret != vmpp_RSLT_OK) {
        if (userBuf)
            set_out_buffer_idle(chn, userBuf);
    }

    return ret;
}

vmppResult jpeg_encoder_release_stream(struct va_enc_channel *chn, vmppStream *stream) {
    if (!chn || !stream) {
        LOG_ERROR(ENC, "Invalid parameters: chn %p, stream %p", chn, stream);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    if (stream->stream)
        set_out_buffer_idle(chn, (uint8_t *)stream->stream);

    return vmpp_RSLT_OK;
}

static void jpeg_cfg_default_init(JpegEncCfg *cfg) {
    uint32_t i;

    memset(cfg, 0, sizeof(JpegEncCfg));

    /* Picture size */
    cfg->inputWidth = 0;
    cfg->inputHeight = 0;
    cfg->xOffset = 0;
    cfg->yOffset = 0;
    cfg->codingWidth = 0;
    cfg->codingHeight = 0;
    cfg->xDensity = 1;
    cfg->yDensity = 1;

    /* lossless mode */
    cfg->losslessEn = 0;
    cfg->predictMode = 0;
    cfg->ptransValue = 0;

    cfg->rotation = JPEGENC_ROTATE_0;
    cfg->mirror = 0;
    cfg->qLevel = DEFAULT_Q_LEVEL;
    cfg->quality = -1;

    cfg->restartInterval = 0;
    cfg->codingType = JPEGENC_WHOLE_FRAME;
    cfg->frameType = JPEGENC_YUV420_SEMIPLANAR;
    cfg->unitsType = JPEGENC_NO_UNITS;
    cfg->markerType = JPEGENC_SINGLE_MARKER;
    cfg->colorConversion.type = JPEGENC_RGBTOYUV_BT601;
    cfg->codingMode = JPEGENC_420_MODE;
    cfg->scanType = JPEG_RASTER_SCAN;

    /* low latency */
    cfg->inputLineBufEn = 0;
    cfg->inputLineBufLoopBackEn = 0;
    cfg->inputLineBufDepth = 1;
    cfg->inputLineBufHwModeEn = 0;
    cfg->amountPerLoopBack = 0;
    cfg->inputLineBufCbFunc = NULL;
    cfg->inputLineBufCbData = NULL;
    cfg->hashType = 0;
    cfg->lowlatGatingDisable = 1;

    /* flexa sbi */
    cfg->sbi_id_0 = 0;
    cfg->sbi_id_1 = 1;
    cfg->sbi_id_2 = 2;
    cfg->segmentUnitHeight = 16;

    /* stream multi-segment */
    cfg->streamMultiSegmentMode = 0;
    cfg->streamMultiSegmentAmount = 0;
    cfg->streamMultiSegmentSize = 0;
    cfg->streamMultiSegCbFunc = NULL;
    cfg->streamMultiSegCbData = NULL;

    /* constant chroma control */
    cfg->constChromaEn = 0;
    cfg->constCb = 0x80;
    cfg->constCr = 0x80;

    /* jpeg rc */
    cfg->targetBitPerSecond = 0;
    cfg->frameRateNum = 1;
    cfg->frameRateDenom = 1;
    cfg->qpmin = 0;
    cfg->qpmax = 51;
    cfg->fixedQP = -1;
    cfg->rcMode = JPEGENC_CBR;
    cfg->picQpDeltaMax = 3;
    cfg->picQpDeltaMin = -2;

    /* stride */
    cfg->exp_of_input_alignment = 4;

    /* overlay control */
    for (i = 0; i < MAX_OVERLAY_NUM; i++) {
        cfg->olEnable[i] = 0;
        cfg->olFormat[i] = 0;
        cfg->olAlpha[i] = 0;
        cfg->olWidth[i] = 0;
        cfg->olCropWidth[i] = 0;
        cfg->olHeight[i] = 0;
        cfg->olCropHeight[i] = 0;
        cfg->olXoffset[i] = 0;
        cfg->olCropXoffset[i] = 0;
        cfg->olYoffset[i] = 0;
        cfg->olCropYoffset[i] = 0;
        cfg->olYStride[i] = 0;
        cfg->olUVStride[i] = 0;
        cfg->olBitmapY[i] = 0;
        cfg->olBitmapU[i] = 0;
        cfg->olBitmapV[i] = 0;
        cfg->olSuperTile[i] = 0;
        cfg->olScaleWidth[i] = 0;
        cfg->olScaleHeight[i] = 0;
    }

    /* mosaic controls */
    for (i = 0; i < MAX_MOSAIC_NUM; i++) {
        cfg->mosEnable[i] = 0;
        cfg->mosWidth[i] = 0;
        cfg->mosHeight[i] = 0;
        cfg->mosXoffset[i] = 0;
        cfg->mosYoffset[i] = 0;
    }
    cfg->mosSizeIndex = 1;

    /* OSD map parameters */
    cfg->osdMapEnable = 0;
    cfg->osdMapStride = 0;
    cfg->osdMapBlockSize = 8;
    for (i = 0; i < MAX_OSDMAP_COLOR_NUM; i++) {
        cfg->osdMapAlpha[i] = 0;
        cfg->osdMapY[i] = 0;
        cfg->osdMapU[i] = 0;
        cfg->osdMapV[i] = 0;
    }

    /* SRAM power down mode disable */
    cfg->sramPowerdownDisable = 0;
    cfg->sramPowerdownMode = 0;
    cfg->sramPowerdownTimerDiv32 = 96;

    /* dump Registers enable or not */
    cfg->dumpRegister = 0;

    cfg->priority = 0;
    cfg->core_mask = 0;

#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
    cfg->sliceinfoEn = 0;
#endif

    cfg->AXIAlignment = 0;
    cfg->irqTypeMask = 0x1f4;
    cfg->burstMaxLength = 0;

    cfg->ufbcParam.mode = 0;

    cfg->enc_dev = "/dev/hantroenc";
    cfg->mem_dev = "/dev/memalloc";
    cfg->useVcmd = 0;
}
