/*
 * Copyright (c) 2022, Vastai Tech. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "va_vdata_internal.h"
#include "va_version.h"
#include "vmpp_dec_api.h"

#include "decoder_utils.h"
#include "video_decoder.h"
#include "jpeg_decoder.h"
#include "sei_decoder.h"
#include "va_log.h"
#include "va_utils.h"

// vsi headers
#include "dwl.h"

#include <assert.h>
#include <string.h>
#include <fcntl.h>

static vmppVersion s_decoder_version = {LIBDEC_VERSION_MAJOR, LIBDEC_VERSION_MINOR,
                                        LIB_VERSION_BUILD, (int8_t *)LIB_VERSION_STRING};

static void clear_private_buffer(struct va_dec_channel *chn)
{
    pthread_mutex_lock(&chn->private_buffer_mutex);
    for (uint32_t i = 0; i < VA_MAX_OUTPUT_BUFFER; i++) {
        if (chn->private_buffer[i].private_data) {
            LOG_DEBUG(DEC, "clear_private_buffer %d, %p", i, chn->private_buffer[i].private_data);
            free(chn->private_buffer[i].private_data);
            chn->private_buffer[i].private_data = NULL;
        }
        chn->private_buffer[i].used = 0;
    }
    pthread_mutex_unlock(&chn->private_buffer_mutex);
}

static uint8_t *get_idle_private_buffer(struct va_dec_channel *chn)
{
    uint8_t *priv_buf = NULL;
    uint32_t i = 0;
    pthread_mutex_lock(&chn->private_buffer_mutex);
    for (i = 0; i < VA_MAX_OUTPUT_BUFFER; i++) {
        if (!chn->private_buffer[i].used)
            break;
    }
    if (i >= VA_MAX_OUTPUT_BUFFER) {
        LOG_ERROR(DEC, "No idle private buffer avaliable.");
        pthread_mutex_unlock(&chn->private_buffer_mutex);
        return NULL;
    }

    if (!chn->private_buffer[i].private_data) {
        chn->private_buffer[i].private_data = (uint8_t *)malloc(chn->frame_struct_size);
        if (!chn->private_buffer[i].private_data) {
            LOG_ERROR(DEC, "Fail to malloc private buffer.");
            pthread_mutex_unlock(&chn->private_buffer_mutex);
            return NULL;
        }
    }

    priv_buf = chn->private_buffer[i].private_data;
    chn->private_buffer[i].used = 1;

    pthread_mutex_unlock(&chn->private_buffer_mutex);
    return priv_buf;
}

static void set_private_buffer_idle(struct va_dec_channel *chn, uint8_t *privateData)
{
    pthread_mutex_lock(&chn->private_buffer_mutex);
    for (uint32_t i = 0; i < VA_MAX_OUTPUT_BUFFER; i++) {
        if (chn->private_buffer[i].private_data == privateData) {
            chn->private_buffer[i].used = 0;
            break;
        }
    }
    pthread_mutex_unlock(&chn->private_buffer_mutex);
}

static int check_private_buffer_exist(struct va_dec_channel *chn, uint8_t *privateData)
{
    int is_found = 0;
    pthread_mutex_lock(&chn->private_buffer_mutex);
    for (uint32_t i = 0; i < VA_MAX_OUTPUT_BUFFER; i++) {
        if (chn->private_buffer[i].private_data == privateData) {
            is_found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&chn->private_buffer_mutex);
    return is_found;
}

#define VA_PTS_IDLE 0
#define VA_PTS_STORED 1
#define VA_PTS_SET 2

static void va_init_pts_buf(struct va_dec_channel *chn)
{
    uint32_t i;
    struct va_pts_buf *pts_buf = chn->frame_pts_buf;

    for (i = 0; i < VA_MAX_PTS_BUFFER; i++) {
        (pts_buf + i)->flag = VA_PTS_IDLE;
    }
}

static int32_t va_store_pts(struct va_dec_channel *chn, int64_t pts)
{
    int32_t i;
    struct va_pts_buf *pts_buf = chn->frame_pts_buf;

    for (i = 0; i < VA_MAX_PTS_BUFFER; i++) {
        if ((pts_buf + i)->flag == VA_PTS_IDLE) {
            (pts_buf + i)->pts = pts;
            (pts_buf + i)->flag = VA_PTS_STORED;
            LOG_DEBUG(DEC, "pts 0x%llx, pts index %d", (U64)pts, i);
            return i;
        }
    }
    return -1;
}

static int32_t va_get_pts(struct va_dec_channel *chn, int64_t *pts)
{
    int32_t index;
    struct va_pts_buf *pts_buf;

    index = (int32_t)(*pts);

    if (index >= VA_MAX_PTS_BUFFER || index < 0) {
        return -1;
    }

    pts_buf = &chn->frame_pts_buf[index];
    pts_buf->flag = VA_PTS_IDLE;
    *pts = pts_buf->pts;

    LOG_DEBUG(DEC, "pts 0x%llx, pts index %d", (U64)*pts, index);

    return 0;
}

static int32_t va_set_pts(struct va_dec_channel *chn, int32_t index)
{
    struct va_pts_buf *pts_buf;

    if (index >= VA_MAX_PTS_BUFFER || index < 0) {
        return -1;
    }

    pts_buf = &chn->frame_pts_buf[index];
    if (pts_buf->flag != VA_PTS_STORED)
        LOG_ERROR(DEC, "pts 0x%llx, pts index %d   Status Error!", (U64)pts_buf->pts, index);

    pts_buf->flag = VA_PTS_SET;

    LOG_DEBUG(DEC, "pts 0x%llx, pts index %d", (U64)pts_buf->pts, index);

    return 0;
}

static int32_t va_wait_pts(struct va_dec_channel *chn, int32_t index)
{
    struct va_pts_buf *pts_buf;
    int wait_count = 0;

    if (index >= VA_MAX_PTS_BUFFER || index < 0) {
        return -1;
    }

    pts_buf = &chn->frame_pts_buf[index];
    while (pts_buf->flag != VA_PTS_SET){
        LOG_DEBUG(DEC, "pts 0x%llx, pts index %d Waiting!!!", (U64)pts_buf->pts, index);
        usleep(100);
        wait_count++;
        if (wait_count > 10000)
            return -2; // timeout
    }

    LOG_DEBUG(DEC, "pts 0x%llx, pts index %d", (U64)pts_buf->pts, index);

    return 0;
}

static int send_stream_cb(void* chn, uint64_t pts_index)
{
    int64_t pts = pts_index;
    va_get_pts((struct va_dec_channel *) chn, &pts);

    return 0;
}

static int set_default_slice_info(vmppSliceInfo *slice_info)
{
    slice_info->picStruct = vmpp_FLD_FRAME;
    slice_info->isFirstField = 1;
    slice_info->picOrderCnt = 0;

    return 0;
}

/* Returns 1 when the codec is handled by the generic video decoder (unified VCDec API). */
static int is_video_codec(vmppCodecType codec_type)
{
    switch (codec_type) {
    case vmpp_CODEC_DEC_H264:
    case vmpp_CODEC_DEC_HEVC:
    case vmpp_CODEC_DEC_VP9:
    case vmpp_CODEC_DEC_AV1:
    case vmpp_CODEC_DEC_AVS2:
        return 1;
    default:
        return 0;
    }
}

static const char *codec_type_string(vmppCodecType codec_type)
{
    switch (codec_type) {
    case vmpp_CODEC_DEC_JPEG:
        return "JPEG";
    case vmpp_CODEC_DEC_H264:
        return "H264";
    case vmpp_CODEC_DEC_HEVC:
        return "HEVC";
    case vmpp_CODEC_DEC_VP9:
        return "VP9";
    case vmpp_CODEC_DEC_AV1:
        return "AV1";
    case vmpp_CODEC_DEC_AVS2:
        return "AVS2";
    default:
        break;
    }
    return "UNKNOWN";
}

static enum DWLClientType dwl_client_type(vmppCodecType codec_type)
{
    switch (codec_type) {
    case vmpp_CODEC_DEC_JPEG:
        return DWL_CLIENT_TYPE_JPEG_DEC;
    case vmpp_CODEC_DEC_H264:
        return DWL_CLIENT_TYPE_H264_DEC;
    case vmpp_CODEC_DEC_VP9:
        return DWL_CLIENT_TYPE_VP9_DEC;
    case vmpp_CODEC_DEC_AV1:
        return DWL_CLIENT_TYPE_AV1_DEC;
    case vmpp_CODEC_DEC_AVS2:
        return DWL_CLIENT_TYPE_AVS2_DEC;
    case vmpp_CODEC_DEC_HEVC:
    default:
        return DWL_CLIENT_TYPE_HEVC_DEC;
    }
}

VMPP_API vmppResult vmppInitDecoder(vmppConfiguration *cfg)
{
    if (!cfg) {
        LOG_ERROR(DEC, "Invalid parameters for decoder initialization!");
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    if (cfg->logCtx.enableCustomLog)
        registerLogContext(DEC, &cfg->logCtx);

    LOG(DEC, vmpp_LOG_INFO, COLOR_YELLOW, "VMPP Decoder Version: %s", s_decoder_version.versionString);

    return vmpp_RSLT_OK;
}

vmppResult vmppDeInitDecoder()
{
    return vmpp_RSLT_OK;
}

vmppResult vmppDecCreateChannel(vmppChannel *chn, vmppDecChannelParameters *param)
{
    if (!chn || !param) {
        LOG_ERROR(DEC, "Invalid parameters: chn %p, param %p.", chn, param);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    if (param->cropInfo.flag == vmpp_CROP_CUSTOMIZED) {
        /*odd number is not supported in current version of crop*/
        u32 cropAlign = 1;
        if (param->cropInfo.width == 0 || param->cropInfo.height == 0 || (param->cropInfo.xOffset & cropAlign) ||
            (param->cropInfo.yOffset & cropAlign) || (param->cropInfo.width & cropAlign) ||
            (param->cropInfo.height & cropAlign)) {
            LOG_ERROR(DEC, "Invalid crop info: [%d, %d, %dx%d]", param->cropInfo.xOffset, param->cropInfo.yOffset,
                      param->cropInfo.width, param->cropInfo.height);
            return vmpp_RSLT_ERR_INVALID_PARAMS;
        }
    }

    struct va_dec_channel *dchn = malloc(sizeof(struct va_dec_channel));
    if (!dchn) {
        LOG_ERROR(DEC, "Fail to malloc channel instance.");
        *chn = NULL;
        return vmpp_RSLT_ERR_NO_MEMORY;
    }
#if 0
    FILE* fp = fopen("/data/stream_dump.bin", "wb");
    if (fp != NULL) {
        fclose(fp);
    }else {
        LOG_ERROR(DEC, "failed to open stream_dump.bin");
    }
#endif

    memset(dchn, 0, sizeof(struct va_dec_channel));

    pthread_mutex_init(&dchn->private_buffer_mutex, NULL);
    memcpy(&dchn->params, param, sizeof(vmppDecChannelParameters));

    pthread_mutexattr_t attr;
    int attr_ret = -1;
    if ((attr_ret = pthread_mutexattr_init(&attr)) != 0) {
        LOG_ERROR(DEC, "create mutex attribute error. msg:%s", strerror(attr_ret));
        pthread_mutex_destroy(&dchn->private_buffer_mutex);
        free(dchn);
        *chn = NULL;
        return vmpp_RSLT_ERR_NOT_INITIALIZED;
    }
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
//    memset(&dchn->sei_buffer, 0, sizeof(dchn->sei_buffer));
//    memset(&dchn->va_sei_parameters, 0, sizeof(dchn->va_sei_parameters));
    pthread_mutex_init(&dchn->sei_buffer_mutex, &attr);

    vmppResult ret = vmpp_RSLT_OK;
    if (dchn->params.codecType == vmpp_CODEC_DEC_JPEG) {
        // jpeg do not support LESS_DEV_MEM mode
        if (dchn->params.memoryMode != vmpp_DEC_MEM_USER_AS_HWOUT &&
            dchn->params.memoryMode != vmpp_DEC_MEM_NORMAL) {
            LOG_ERROR(DEC, "memory mode:%d is still not supported by %s decoder!", dchn->params.memoryMode,
                      codec_type_string(dchn->params.codecType));
            ret = vmpp_RSLT_ERR_UNSUPPORTED;
        } else {
            ret = jpeg_decoder_create_chn(dchn);
        }
    } else if (is_video_codec(dchn->params.codecType)) {
        /* Only NORMAL memory mode is supported by the generic video decoder. */
        if (dchn->params.memoryMode != vmpp_DEC_MEM_NORMAL) {
            LOG_ERROR(DEC, "memory mode:%d is still not supported by %s decoder!", dchn->params.memoryMode,
                      codec_type_string(dchn->params.codecType));
            ret = vmpp_RSLT_ERR_UNSUPPORTED;
        } else {
            ret = video_decoder_create_chn(dchn);
        }
    } else {
        ret = vmpp_RSLT_ERR_UNSUPPORTED;
    }

    if (ret == vmpp_RSLT_OK) {
        LOG_INFO(DEC, "Decoder channel for %s is READY.", codec_type_string(dchn->params.codecType));
        atomic_set_u32(&dchn->state, vmpp_ST_READY);
    } else {
        LOG_ERROR(DEC, "Create decoder channel for %s failed: %d.", codec_type_string(dchn->params.codecType),
                  ret);
        pthread_mutex_destroy(&dchn->sei_buffer_mutex);
        pthread_mutex_destroy(&dchn->private_buffer_mutex);
        free(dchn);
        dchn = NULL;
    }

    *chn = dchn;

    LOG_DEBUG(DEC, "chn %p", *chn);

    return ret;
}

static vmppResult vmppDecDestroyChannelInternal(vmppChannel *chn, int is_forced)
{
    if (chn == NULL || !*chn) {
        LOG_ERROR(DEC, "NULL channel pointer.");
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    struct va_dec_channel *inst = (struct va_dec_channel *)(*chn);
    vmppResult ret = vmpp_RSLT_OK;
    vmppState state = (vmppState)atomic_get_u32(&inst->state);

    if ((is_forced == 0) && (state == vmpp_ST_RUNNING || state == vmpp_ST_ERROR || state == vmpp_ST_STOPPING)) {
        LOG_WARN(DEC, "Can not destroy decoder channel due to incorrect state: %d.", state);
        return vmpp_RSLT_ERR_INVALID_STATE;
    }

    if (inst->params.codecType == vmpp_CODEC_DEC_JPEG)
        ret = jpeg_decoder_destory_chn(inst);
    else if (is_video_codec(inst->params.codecType))
        ret = video_decoder_destory_chn(inst);

    if (ret == vmpp_RSLT_OK) {
        uint32_t recv_cnt = atomic_get_u32(&inst->receive_frame_cnt);
        uint32_t release_cnt = atomic_get_u32(&inst->release_frame_cnt);
        if (release_cnt < recv_cnt)
            LOG_WARN(DEC, "release_cnt(%d) is less than recv_cnt(%d) when destrying channel %p", release_cnt, recv_cnt, inst);
        clear_private_buffer(inst);
        pthread_mutex_destroy(&inst->private_buffer_mutex);
        free_sei_parameter(inst);
        pthread_mutex_destroy(&inst->sei_buffer_mutex);
        free(inst);
        LOG_INFO(DEC, "Decode channel %p destroyed (%d/%d)", inst, recv_cnt, release_cnt);
        LOG_DEBUG(DEC, "chn %p", *chn);
        *chn = NULL;
    } else {
        // TODO: some error handling
    }

    return ret;
}

vmppResult vmppDecDestroyChannel(vmppChannel *chn)
{
    return vmppDecDestroyChannelInternal(chn, 0);
}

vmppResult vmppDecDestroyChannelForced(vmppChannel *chn)
{
    return vmppDecDestroyChannelInternal(chn, 1);
}

vmppResult vmppDecStart(vmppChannel chn)
{
    if (chn == NULL) {
        LOG_ERROR(DEC, "NULL channel pointer.");
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    struct va_dec_channel *inst = (struct va_dec_channel *)chn;
    uint32_t state = atomic_get_u32(&inst->state);
    if (state != vmpp_ST_READY) {
        LOG_ERROR(DEC, "Invalid state: %d.", state);
        return vmpp_RSLT_ERR_INVALID_STATE;
    }

    atomic_set_u32(&inst->state, vmpp_ST_RUNNING);

    LOG_DEBUG(DEC, "chn %p", chn);

    return vmpp_RSLT_OK;
}

vmppResult vmppDecStop(vmppChannel chn)
{
    if (chn == NULL) {
        LOG_ERROR(DEC, "NULL channel pointer.");
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    struct va_dec_channel *inst = (struct va_dec_channel *)chn;

    uint32_t state = atomic_get_u32(&inst->state);
    if (state != vmpp_ST_RUNNING && state != vmpp_ST_ERROR) {
        LOG_ERROR(DEC, "Invalid state: %d.", state);
        return vmpp_RSLT_ERR_INVALID_STATE;
    }

    atomic_set_u32(&inst->state, vmpp_ST_STOPPING);

    if (inst->params.codecType == vmpp_CODEC_DEC_JPEG)
        jpeg_decoder_end_of_stream(inst);
    else if (is_video_codec(inst->params.codecType))
        video_decoder_end_of_stream(inst);

    LOG_INFO(DEC, "Decode channel %p stopping.", chn);

    return vmpp_RSLT_OK;
}

vmppResult vmppDecSendStream(vmppChannel chn, vmppStream *stream, uint32_t timeout)
{
    vmppResult ret = vmpp_RSLT_OK;
    ret = vmppDecSendStreamV2(chn, stream, NULL, timeout);
    return ret;
}

vmppResult vmppDecSendStreamV2(vmppChannel chn, vmppStream *stream, vmppSliceInfo *sliceInfo, uint32_t timeout)
{
    int32_t pts_index;
    uint64_t stream_pts;

    if (!chn || !stream) {
        LOG_ERROR(DEC, "Invalid parameter(s): chn %p, stream %p.", chn, stream);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    if (stream->len == 0) {
        LOG_ERROR(DEC, "Empty buffer: chn %p, stream %p, len %d", chn, stream->stream, stream->len);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

#if 0
    FILE* fp = fopen("/data/stream_dump.bin", "ab+");
    if (fp != NULL) {
        fwrite(stream->stream, 1, stream->len, fp);
        fclose(fp);
    }else {
        LOG_ERROR(DEC, "failed to open stream_dump.bin");
    }
#endif

#if 0
    LOG_ERROR(DEC, "Dump stream");
    for (int i = 0; i < stream->len; i+=16){
        int left = (stream->len - i) < 16 ? (stream->len - i) : 16;
        unsigned char* ptr = (char*) stream->stream;
        if (i > 48) break;
        LOG_ERROR(DEC, "bin(%d): %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x", left, ptr[i+0], ptr[i+1],ptr[i+2], ptr[i+3],
        ptr[i+4],ptr[i+5],ptr[i+6],ptr[i+7],
        ptr[i+8],ptr[i+9],ptr[i+10],ptr[i+11],
        ptr[i+12],ptr[i+13],ptr[i+14],ptr[i+15]);
    }
#endif

    stream_pts = stream->pts;
    vmppResult ret = vmpp_RSLT_OK;
    struct va_dec_channel *inst = (struct va_dec_channel *)chn;
    vmppState state = (vmppState)atomic_get_u32(&inst->state);
    if (state != vmpp_ST_RUNNING) {
        LOG_ERROR(DEC, "Invalid state: %d.", state);
        return vmpp_RSLT_ERR_INVALID_STATE;
    }

    if (inst->params.memoryMode == vmpp_DEC_MEM_USER_AS_HWOUT && stream->outputBusAddress[0] == 0) {
        LOG_ERROR(DEC, "Invalid bus address: chn %p", chn);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    } else if ((inst->params.codecType == vmpp_CODEC_DEC_HEVC || inst->params.codecType == vmpp_CODEC_DEC_H264) &&
               stream->outputBusAddress[0] == (vmppDevAddr)-1) {
        // when stream header(SPS/PPS) is sent seperately without IDR frame, output handle is not needed.
        LOG_INFO(DEC, "output handle -1 will be ignored later, chn %p", chn);
    }

    if (timeout < VMPP_MIN_TIMEOUT_MS) {
        LOG_INFO(DEC, "Timeout(%d) is too small, using default minimum value(%d).", timeout,
                 VMPP_MIN_TIMEOUT_MS);
        timeout = VMPP_MIN_TIMEOUT_MS;
    }

    pts_index = va_store_pts(chn, stream->pts);
    if (pts_index == -1) {
        LOG_ERROR(DEC, "No PTS buffer!");
        return vmpp_RSLT_ERR_NO_PTSBUF;
    }
    stream->pts = (uint64_t)pts_index;
    if (sliceInfo) {
        set_default_slice_info(sliceInfo);
    }

    uint8_t SEI_flag = 0;
    if (inst->params.codecType == vmpp_CODEC_DEC_JPEG)
        ret = jpeg_decoder_send_stream(inst, stream, timeout);
    else if (is_video_codec(inst->params.codecType))
        ret = video_decoder_send_stream(inst, stream, timeout, send_stream_cb, &SEI_flag);

    if (inst->params.codecType == vmpp_CODEC_DEC_AV1 || inst->params.codecType == vmpp_CODEC_DEC_VP9) {
        if (ret == vmpp_RSLT_OK || ret == vmpp_RSLT_ERR_SYS_ERROR)
            stream->pts = stream_pts;
        else
            va_get_pts(chn, &stream->pts);
    } else if (ret == vmpp_RSLT_OK || ret == vmpp_RSLT_ERR_SYS_ERROR) {
        if (SEI_flag) {
            LOG_INFO(DEC, "chn %p, SEI_flag %d, enSEIParser %d", chn, SEI_flag, inst->params.enSEIParser);

            if (inst->params.enSEIParser) {
                int res = sei_decoder(inst, stream, stream->pts);
                if (res < 0) {
                    LOG_ERROR(DEC, "sei_decoder failed, res %d", res);
                }
            }
        }

        va_set_pts(inst, (int32_t)stream->pts);

        // restore stream pts
        stream->pts = stream_pts;
    } else {
        va_get_pts(chn, &stream->pts);
    }

#if 0
    if (ret < 0)
        atomic_set_u32(&inst->state, vmpp_ST_ERROR);
#endif

    LOG_DEBUG(DEC, "chn %p, stream %p, len %d, ret %d", chn, stream->stream, stream->len, ret);

    return ret;
}

static void do_cropping(vmppFrame *frame)
{
    uint32_t src_offset = 0;
    uint32_t dst_offset = 0;
    uint32_t uv_offset = 0;
    uint32_t uv_crop_width = 0, uv_crop_height = 0;
    uint32_t jj;
    uint32_t do_crop = 0;

#if 0
    frame->cropInfo.xOffset = 101;
    frame->cropInfo.yOffset = 101;
    frame->cropInfo.width   -= 101;
    frame->cropInfo.height  -= 101;
#endif

    if (frame->cropInfo.xOffset % 2 == 1) {
        frame->cropInfo.xOffset -= 1;
    }

    uv_crop_width =
        (frame->cropInfo.width % 2 == 1) ? (frame->cropInfo.width + 1) : (frame->cropInfo.width);
    uv_crop_height =
        (frame->cropInfo.height % 2 == 1) ? (frame->cropInfo.height + 1) : (frame->cropInfo.height);

    do_crop = frame->stride[0] > frame->cropInfo.width || frame->height > frame->cropInfo.height ||
              frame->cropInfo.xOffset || frame->cropInfo.yOffset;

    if (do_crop) {
        jj = 0;
        // copy Y
        for (uint32_t j = 0; j < frame->height; j++) {
            if (j >= frame->cropInfo.yOffset && (j < (frame->cropInfo.yOffset + frame->cropInfo.height))) {
                memmove(frame->data[0] + dst_offset,
                        frame->data[0] + src_offset + frame->cropInfo.xOffset,
                        frame->cropInfo.width);
                dst_offset += frame->cropInfo.width;
                jj++;
            }
            src_offset += frame->stride[0] /*frame->width*/;
        }

        // copy UV
        if (frame->data[1]) {
            jj = 0;
            uv_offset = dst_offset;
            src_offset = 0;
            for (uint32_t j = 0; j < (frame->height + 1) / 2; j++) {
                if (j >= frame->cropInfo.yOffset / 2) {
                    memmove(frame->data[0] + dst_offset,
                            frame->data[1] + src_offset + frame->cropInfo.xOffset / 2, uv_crop_width);
                    dst_offset += uv_crop_width;
                    jj++;

                    if (jj >= uv_crop_height / 2)
                        break;
                }
                src_offset += frame->stride[1] /*frame->width*/;
            }
            frame->data[1] = frame->data[0] + uv_offset;
        }

        frame->cropInfo.flag = 0;
        frame->dataSize = dst_offset; // frame->cropInfo.width * frame->cropInfo.height * 3 / 2;
    }
}

vmppResult vmppDecReceiveFrame(vmppChannel chn, vmppFrame *frame, vmppDecOutputOptions *opt,
                               uint32_t timeout)
{

    UNUSED_PARAMETER(timeout);
    if (!chn || !frame) {
        LOG_ERROR(DEC, "Invalid parameter(s): chn %p, frame %p.", chn, frame);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    struct va_dec_channel *inst = (struct va_dec_channel *)chn;

    switch (inst->params.memoryMode) {
    case vmpp_DEC_MEM_USER_OUT_BUF_HOST:
    case vmpp_DEC_MEM_LESS_DEV_MEM:
        if (opt->memoryType == vmpp_MEM_DEVICE) {
            LOG_DEBUG(DEC, "WARN!!! memoryType(%d) is incompatible with memoryMode '%d', will be ignored.",
                opt->memoryType, inst->params.memoryMode);
        }
        break;
    case vmpp_DEC_MEM_USER_OUT_BUF_DEV:
    case vmpp_DEC_MEM_USER_AS_HWOUT:
        if (opt->memoryType == vmpp_MEM_HOST) {
            LOG_DEBUG(DEC, "WARN!!! memoryType(%d) is incompatible with memoryMode '%d', will be ignored.",
                opt->memoryType, inst->params.memoryMode);
        }
        break;
    case vmpp_DEC_MEM_NORMAL:
    default:
        break;
    }

    if (inst->params.memoryMode == vmpp_DEC_MEM_USER_OUT_BUF_DEV && frame->busAddress[0] == 0) {
        LOG_ERROR(DEC, "Invalid bus address: chn %p", chn);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    } else if (inst->params.memoryMode == vmpp_DEC_MEM_USER_OUT_BUF_HOST && frame->data[0] == 0) {
        LOG_ERROR(DEC, "Invalid host address: chn %p", chn);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    if (inst->params.memoryMode == vmpp_DEC_MEM_USER_OUT_BUF_HOST || inst->params.memoryMode == vmpp_DEC_MEM_USER_OUT_BUF_DEV) {
        uint8_t *temp1 = frame->data[0];
        vmppDevAddr temp2 = frame->busAddress[0];

        memset(frame, 0, sizeof(vmppFrame));
        frame->data[0] = temp1;
        frame->busAddress[0] = temp2;
    } else {
        memset(frame, 0, sizeof(vmppFrame));
    }

    vmppResult ret = vmpp_RSLT_OK;
    vmppState state = (vmppState)atomic_get_u32(&inst->state);
    if (state == vmpp_ST_STOPPED || state == vmpp_ST_NONE || state == vmpp_ST_READY) {
        LOG_ERROR(DEC, "Invalid state: %d.", state);
        return vmpp_RSLT_ERR_INVALID_STATE;
    }

    frame->privateData = get_idle_private_buffer(inst);
    if (!frame->privateData)
        return vmpp_RSLT_ERR_NO_BUFFER;

    if (inst->params.codecType == vmpp_CODEC_DEC_JPEG)
        ret = jpeg_decoder_receive_frame(inst, frame, opt);
    else if (is_video_codec(inst->params.codecType))
        ret = video_decoder_receive_frame(inst, frame, opt);

    int64_t pts = frame->pts;
    int64_t extra_pts = -1; // for interlaced sequence
    if (ret == vmpp_RSLT_OK) {
        if (inst->params.codecType != vmpp_CODEC_DEC_AV1 && inst->params.codecType != vmpp_CODEC_DEC_VP9) {
            int pts_ret = va_wait_pts(chn, (int32_t)pts);
            if (pts_ret == -1) {
                return vmpp_RSLT_ERR_NO_PTSBUF;
            } else if (pts_ret == -2) {
                return vmpp_RSLT_ERR_HW_TIMEOUT;
            }
        }

        if (va_get_pts(chn, &pts) == -1)
            return vmpp_RSLT_ERR_NO_PTSBUF;

        /* TODO: interlaced sequence handling needs the extra picture information of the decoder. */
    }

    /* to crop picture.*/
    if (ret == vmpp_RSLT_OK && frame->memoryType == vmpp_MEM_HOST && opt->enableCrop == 1)
        do_cropping(frame);

    if ((ret == vmpp_RSLT_WARN_EOS || ret < vmpp_RSLT_OK) &&
        (vmppState)atomic_get_u32(&inst->state) == vmpp_ST_STOPPING) {
        LOG_INFO(DEC, "Decode channel %p stopped.", inst);
        atomic_set_u32(&inst->state, vmpp_ST_STOPPED);
    } else if (ret < 0) {
        atomic_set_u32(&inst->state, vmpp_ST_ERROR);
    }

    if (ret != vmpp_RSLT_OK) {
        set_private_buffer_idle(inst, frame->privateData);
        set_sei_parameter_idle_frame(inst, frame);
        if (vmpp_RSLT_WARN_MORE_DATA != ret && vmpp_RSLT_WARN_EOS != ret) {
            LOG_WARN(DEC, "Error happens %d, set private buffer unused.", ret);
        }
    } else {
        LOG_DEBUG(DEC, "orig: %dx%d, crop: %dx%d, stride: %d %d, pix_fmt: %d, type: %d", frame->width, frame->height,
            frame->cropInfo.width, frame->cropInfo.height, frame->stride[0], frame->stride[1], frame->pixelFormat, frame->frameType);
        get_sei_parameter_for_frame(inst, frame, extra_pts);
    }

    frame->pts = pts;

    if (ret == vmpp_RSLT_OK)
        atomic_add_fetch_u32(&inst->receive_frame_cnt);

    LOG_DEBUG(DEC, "chn %p, frame %p, cnt:%d, ret %d", chn, frame, inst->receive_frame_cnt, ret);

    return ret;
}

vmppResult vmppDecTransferFrame(vmppChannel chn, vmppFrame *frame, uint32_t crop)
{
    if (!chn || !frame) {
        LOG_ERROR(DEC, "Invalid parameter(s): chn %p, frame %p.", chn, frame);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    if (frame->memoryType == vmpp_MEM_HOST) {
        LOG_WARN(DEC, "frame has already been transfered to host.");
        return vmpp_RSLT_WARN_REPEAT_OPERATION;
    }

    vmppResult ret = vmpp_RSLT_OK;
    struct va_dec_channel *inst = (struct va_dec_channel *)chn;
    if ((vmppState)atomic_get_u32(&inst->state) == vmpp_ST_NONE) {
        LOG_ERROR(DEC, "Invalid state: %d.", vmpp_ST_NONE);
        return vmpp_RSLT_ERR_INVALID_STATE;
    }

    if (inst->params.codecType == vmpp_CODEC_DEC_JPEG)
        ret = jpeg_decoder_transfer_frame(inst, frame);
    else if (is_video_codec(inst->params.codecType))
        ret = video_decoder_transfer_frame(inst, frame);

    /* to crop picture.*/
    if (ret == vmpp_RSLT_OK && crop == 1)
        do_cropping(frame);

    LOG_DEBUG(DEC, "chn %p, frame %p, ret %d", chn, frame, ret);

    return ret;
}

vmppResult vmppDecReleaseFrame(vmppChannel chn, vmppFrame *frame, uint32_t timeout)
{
    UNUSED_PARAMETER(timeout);
    if (!chn || !frame) {
        LOG_ERROR(DEC, "Invalid parameter(s): chn %p, frame %p.", chn, frame);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    vmppResult ret = vmpp_RSLT_OK;
    struct va_dec_channel *inst = (struct va_dec_channel *)chn;
    vmppState state = (vmppState)atomic_get_u32(&inst->state);
    if (/*state == vmpp_ST_STOPPED || */ state == vmpp_ST_NONE || state == vmpp_ST_READY) {
        LOG_ERROR(DEC, "Invalid state: %d.", state);
        return vmpp_RSLT_ERR_INVALID_STATE;
    }

    if( !check_private_buffer_exist(inst, frame->privateData)) {
        LOG_WARN(DEC, "frame not belong to this channel!");
        return vmpp_RSLT_WARN_FRAME_NOT_EXIST;
    }

    if (inst->params.codecType == vmpp_CODEC_DEC_JPEG)
        ret = jpeg_decoder_release_frame(inst, frame);
    else if (is_video_codec(inst->params.codecType))
        ret = video_decoder_release_frame(inst, frame);

    set_private_buffer_idle(inst, frame->privateData);
    set_sei_parameter_idle_frame(inst, frame);

    if (ret == vmpp_RSLT_OK)
        atomic_add_fetch_u32(&inst->release_frame_cnt);

    LOG_DEBUG(DEC, "chn %p, frame %p, cnt %d, ret %d", chn, frame, inst->release_frame_cnt, ret);

    return ret;
}

vmppVersion *vmppDecGetVersion(void) { return &s_decoder_version; }

vmppResult vmppDecGetStatus(vmppChannel chn, vmppDecStatus *status)
{
    if (!chn || !status) {
        LOG_ERROR(DEC, "Invalid parameter(s): chn %p, status %p.", chn, status);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    struct va_dec_channel *inst = (struct va_dec_channel *)chn;

    status->state = (vmppState)atomic_get_u32(&inst->state);
    /* The new DWL interface only exposes the ASIC ID, die ID is no longer available here. */
    status->hardwareID.dieID = 0;
    status->hardwareID.coreID = (int32_t)DWLReadAsicID(inst->cwl, dwl_client_type(inst->params.codecType));
    status->result = vmpp_RSLT_OK;
    status->runningFrames = 0;
    status->reorderedFrames = 0;
    status->bufferedFrames = 0;
    status->droppedFrames = 0;

    return vmpp_RSLT_OK;
}

vmppResult vmppDecGetStreamInfo(vmppChannel chn, vmppDecStreamInfo *info)
{
    if (!chn || !info) {
        LOG_ERROR(DEC, "Invalid parameter(s): chn %p, info %p.", chn, info);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    vmppResult ret = vmpp_RSLT_OK;
    struct va_dec_channel *inst = (struct va_dec_channel *)chn;
    vmppState state = (vmppState)atomic_get_u32(&inst->state);
    if (state == vmpp_ST_NONE || state == vmpp_ST_READY) {
        LOG_ERROR(DEC, "Invalid state: %d.", state);
        return vmpp_RSLT_ERR_INVALID_STATE;
    }

    if (inst->params.codecType == vmpp_CODEC_DEC_JPEG)
        ret = jpeg_decoder_get_stream_info(inst, info);
    else if (is_video_codec(inst->params.codecType))
        ret = video_decoder_get_stream_info(inst, info);

    return ret;
}

vmppResult vmppDecGetJpegInfo(vmppStream *stream, vmppDecJpegInfo *info)
{
    if (!stream || !info) {
        LOG_ERROR(DEC, "Invalid parameter(s): stream %p, info %p.", stream, info);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    if (!stream->stream || !stream->len) {
        LOG_ERROR(DEC, "Invalid data: stream %p, len %d.", stream->stream, stream->len);
        return vmpp_RSLT_ERR_INVALID_DATA;
    }

    vmppResult ret = jpeg_decoder_get_jpeg_info(stream, info);
    return ret;
}

vmppResult vmppDecGetVideoInfo(vmppStream *stream, vmppCodecType codecType, vmppDecVideoInfo *info)
{
    if (!stream || !info) {
        LOG_ERROR(DEC, "Invalid parameter(s): stream %p, info %p.", stream, info);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    if (!stream->stream || !stream->len) {
        LOG_ERROR(DEC, "Invalid data: stream %p, len %d.", stream->stream, stream->len);
        return vmpp_RSLT_ERR_INVALID_DATA;
    }

    if (!is_video_codec(codecType))
        return vmpp_RSLT_ERR_UNSUPPORTED;

    return video_decoder_get_video_info(stream, codecType, info);
}

void vmppDecGetJpegCaps(vmppDecJpegCapability *caps)
{
    if (caps) {
        caps->minWidth = 128;
        caps->minHeight = 128;
        caps->maxWidth = 32768;
        caps->maxHeight = 32768;
        caps->codingMode[0] = vmpp_JPEG_BASELINE;
        caps->codingMode[1] = vmpp_JPEG_NONE;
        caps->pixelFormats[0] = vmpp_PIX_FMT_NV12;
        caps->pixelFormats[1] = vmpp_PIX_FMT_NONE;
    }
}

void vmppDecGetVideoCaps(vmppCodecType type, vmppDecVideoCapability *caps)
{
    if (caps) {
        caps->bitDepth = 8;
        caps->minWidth = 176;
        caps->minHeight = 144;
        caps->maxWidth = 8192;
        caps->maxHeight = 8192;
        if (type == vmpp_CODEC_DEC_H264) {
            caps->maxProFile = vmpp_VIDEO_PRFL_H264_HIGH_10;
            caps->maxLevel = vmpp_VIDEO_LVL_H264_6_2;
        } else if (type == vmpp_CODEC_DEC_HEVC) {
            caps->maxProFile = vmpp_VIDEO_PRFL_HEVC_MAIN_REXT;
            caps->maxLevel = vmpp_VIDEO_LVL_HEVC_6_2;
        } else {
            // Do nothing
        }

        caps->pixelFormats[0] = vmpp_PIX_FMT_NV12;
        caps->pixelFormats[1] = vmpp_PIX_FMT_YUV420_PLANAR_10BIT_I010;
        caps->pixelFormats[2] = vmpp_PIX_FMT_YUV420_PLANAR_10BIT_P010;
        caps->pixelFormats[3] = vmpp_PIX_FMT_NONE;
    }
}

int32_t vmppDecGetIdleDpbBufferCount(vmppChannel chn)
{
    uint32_t idle_count = 0;
    if (!chn) {
        LOG_ERROR(DEC, "Invalid parameter(s): chn %p", chn);
        return idle_count;
    }

    struct va_dec_channel *inst = (struct va_dec_channel *)chn;
    if (inst->params.codecType == vmpp_CODEC_DEC_JPEG) {
        LOG_ERROR(DEC, "Invalid codec type:JPEG");
    } else if (is_video_codec(inst->params.codecType)) {
        idle_count = (uint32_t)video_get_idle_dpb_count(inst);
    }

    return idle_count;
}