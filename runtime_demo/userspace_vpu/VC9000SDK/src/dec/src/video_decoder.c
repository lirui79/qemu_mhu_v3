/*
 * Copyright (c) 2022, Vastai Tech. All rights reserved
 *
 * The information contained herein is confidential
 * property of Company. The user, copying, transfer or
 * disclosure of such information is prohibited except
 * by express written agreement with VASTAITECH.
 */

#include "video_decoder.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "decoder_utils.h"
#include "sw_util.h"
#include "va_log.h"
#include "va_utils.h"
#include "va_vdata_internal.h"
#include "vmpp_dec_defs.h"

// vsi headers
#include "decapicommon.h"
#include "dwl.h"
#include "vcdecapi.h"

/* The maximum waiting count for the internal buffers of the decoder. */
#define MAX_WAIT_CNT (64)
/* The maximum retry count when the decoder makes no progress on the input stream, e.g. it is
   waiting for the pending pictures to be output (DEC_PENDING_FLUSH). */
#define MAX_NO_PROGRESS_CNT (200)
/* The maximum number of times the same stream buffer may be sent again. */
#define MAX_INPUT_AGAIN_CNT (10)
/* Default picture alignment for the frame buffers. */
#define VIDEO_DEFAULT_ALIGN DEC_ALIGN_128B
/* Buffer size used to capture SEI messages from the decoder. */
#define VIDEO_SEI_BUFFER_SIZE (4096)

static const char *codec_name(enum DecCodec codec)
{
    switch (codec) {
    case DEC_H264:
        return "H264";
    case DEC_HEVC:
        return "HEVC";
    case DEC_VP9:
        return "VP9";
    case DEC_AV1:
        return "AV1";
    case DEC_AVS2:
        return "AVS2";
    default:
        break;
    }
    return "unknown";
}

static enum DecCodec codec_from_vmpp(vmppCodecType codec_type)
{
    switch (codec_type) {
    case vmpp_CODEC_DEC_H264:
        return DEC_H264;
    case vmpp_CODEC_DEC_HEVC:
        return DEC_HEVC;
    case vmpp_CODEC_DEC_VP9:
        return DEC_VP9;
    case vmpp_CODEC_DEC_AV1:
        return DEC_AV1;
    case vmpp_CODEC_DEC_AVS2:
        return DEC_AVS2;
    default:
        break;
    }
    return DEC_FMT_MAX;
}

static enum DWLClientType dwl_client_from_codec(enum DecCodec codec)
{
    switch (codec) {
    case DEC_H264:
        return DWL_CLIENT_TYPE_H264_DEC;
    case DEC_VP9:
        return DWL_CLIENT_TYPE_VP9_DEC;
    case DEC_AV1:
        return DWL_CLIENT_TYPE_AV1_DEC;
    case DEC_AVS2:
        return DWL_CLIENT_TYPE_AVS2_DEC;
    case DEC_HEVC:
    default:
        return DWL_CLIENT_TYPE_HEVC_DEC;
    }
}

static enum DecDecoderMode decoder_mode_from_vmpp(vmppDecMode mode)
{
    switch (mode) {
    case vmpp_DEC_INTRA_ONLY:
        return DEC_INTRA_ONLY;
    case vmpp_DEC_LOW_DELAY:
        return DEC_LOW_LATENCY;
    default:
        break;
    }
    return DEC_NORMAL;
}

static vmppResult result_from_vsi(enum DecRet ret)
{
    switch (ret) {
    case DEC_OK:
    case DEC_PIC_RDY:
    case DEC_PIC_DECODED:
    case DEC_HDRS_RDY:
    case DEC_SLICE_RDY:
    case DEC_END_OF_SEQ:
    case DEC_FLUSHED:
    case DEC_NONREF_PIC_SKIPPED:
    case DEC_RESOLUTION_CHANGED:
        return vmpp_RSLT_OK;
    case DEC_STRM_PROCESSED:
    case DEC_SCAN_PROCESSED:
    case DEC_BUF_EMPTY:
        return vmpp_RSLT_WARN_MORE_DATA;
    case DEC_PENDING_FLUSH:
        /* The decoder consumed nothing, the stream has to be sent again after the pending
           pictures have been output. */
        return vmpp_RSLT_DEC_INPUT_AGAIN;
    case DEC_END_OF_STREAM:
        return vmpp_RSLT_WARN_EOS;
    case DEC_ABORTED:
        return vmpp_RSLT_WARN_ABORTED;
    case DEC_WAITING_FOR_BUFFER:
    case DEC_NO_DECODING_BUFFER:
        return vmpp_RSLT_ERR_NO_BUFFER;
    case DEC_MEMFAIL:
        return vmpp_RSLT_ERR_NO_MEMORY;
    case DEC_INITFAIL:
    case DEC_NOT_INITIALIZED:
        return vmpp_RSLT_ERR_NOT_INITIALIZED;
    case DEC_PARAM_ERROR:
    case DEC_INVALID_INPUT_BUFFER_SIZE:
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    case DEC_STRM_ERROR:
    case DEC_INVALID_STREAM_LENGTH:
    case DEC_STREAM_NOT_SUPPORTED:
        return vmpp_RSLT_ERR_INVALID_DATA;
    case DEC_FORMAT_NOT_SUPPORTED:
    case DEC_UNSUPPORTED:
        return vmpp_RSLT_ERR_UNSUPPORTED;
    case DEC_HW_RESERVED:
    case DEC_HW_BUS_ERROR:
    case DEC_HW_TIMEOUT:
    case DEC_HW_EXT_TIMEOUT:
        return vmpp_RSLT_ERR_DEC_BUS;
    case DEC_SYSTEM_ERROR:
    case DEC_FATAL_SYSTEM_ERROR:
        return vmpp_RSLT_ERR_SYS_ERROR;
    case DEC_EXT_BUFFER_REJECTED:
        return vmpp_RSLT_ERR_INVALID_STATE;
    default:
        break;
    }
    return vmpp_RSLT_ERR_UNKNOWN;
}

static DecPicAlignment get_pic_align(uint32_t align)
{
    if (align >= (1U << DEC_ALIGN_2048B))
        return DEC_ALIGN_2048B;
    if (align >= (1U << DEC_ALIGN_1024B))
        return DEC_ALIGN_1024B;
    if (align >= (1U << DEC_ALIGN_512B))
        return DEC_ALIGN_512B;
    if (align >= (1U << DEC_ALIGN_256B))
        return DEC_ALIGN_256B;
    if (align >= (1U << DEC_ALIGN_128B))
        return DEC_ALIGN_128B;
    if (align >= (1U << DEC_ALIGN_64B))
        return DEC_ALIGN_64B;
    if (align >= (1U << DEC_ALIGN_32B))
        return DEC_ALIGN_32B;
    if (align >= (1U << DEC_ALIGN_16B))
        return DEC_ALIGN_16B;
    if (align >= (1U << DEC_ALIGN_8B))
        return DEC_ALIGN_8B;
    return VIDEO_DEFAULT_ALIGN;
}

static vmppFrameType frame_type_from_vsi(enum DecPicCodingType coding_type)
{
    switch (coding_type) {
    case DEC_PIC_TYPE_I:
        return vmpp_FRM_I;
    case DEC_PIC_TYPE_P:
        return vmpp_FRM_P;
    case DEC_PIC_TYPE_B:
    case DEC_PIC_TYPE_BI:
        return vmpp_FRM_B;
    default:
        break;
    }
    return vmpp_FRM_I;
}

static int32_t find_ext_buffer_index(struct video_decoder_private_context *ctx, const void *address)
{
    uint32_t i;

    if (ctx == NULL || address == NULL)
        return -1;

    for (i = 0; i < MAX_PIC_BUFFERS; i++) {
        if (ctx->ext_buffers[i].virtual_address == (u32 *)address)
            return (int32_t)i;
    }

    return -1;
}

/* Frees the external frame buffers. If @destroy is 0, only the buffer whose bus address equals
 * @bus_address is released, otherwise all of them are released. */
static void release_ext_buffers(const void *dwl, struct video_decoder_private_context *ctx,
                                addr_t bus_address, uint32_t destroy)
{
    uint32_t i;

    for (i = 0; i < MAX_PIC_BUFFERS; i++) {
        if (ctx->ext_buffers[i].bus_address == 0)
            continue;
        if (!destroy && bus_address != ctx->ext_buffers[i].bus_address)
            continue;

        if (ctx->ext_buffers[i].alloc_bus_addr != 0)
            DWLFreeLinear(dwl, &ctx->ext_buffers[i]);
        DWLmemset(&ctx->ext_buffers[i], 0, sizeof(ctx->ext_buffers[i]));
    }

    if (destroy) {
        ctx->ext_buffer_number = 0;
        ctx->buffer_size = 0;
        for (i = 0; i < MAX_PIC_BUFFERS; i++)
            ctx->buffer_consumed[i] = 1;
    }
}

/* Configures the post-processing unit according to the sequence information and the user request.
 * Refer to SetPpConfig() of the test bench: the picture buffers are always allocated with the PP
 * alignment, the crop region comes either from the user or from the SPS/sequence information. */
static void config_pp_params(struct va_dec_channel *chn, struct video_decoder_private_context *ctx)
{
    PpUnitConfig *ppu_cfg = &ctx->ppu_cfg[0];
    DecPicAlignment align = get_pic_align(chn->params.outputAlign);

    ppu_cfg->align = align;
    ppu_cfg->align_h = align;
    ppu_cfg->enabled = 1;
    ctx->pp_enabled = 1;

    if (chn->params.cropInfo.flag) {
        ppu_cfg->crop.enabled = 1;
        ppu_cfg->crop.set_by_user = 1;
        if (chn->params.cropInfo.flag == vmpp_CROP_CUSTOMIZED) {
            ppu_cfg->crop.x = chn->params.cropInfo.xOffset;
            ppu_cfg->crop.y = chn->params.cropInfo.yOffset;
            ppu_cfg->crop.width = chn->params.cropInfo.width;
            ppu_cfg->crop.height = chn->params.cropInfo.height;
        } else {
            /* Use the cropping window signaled by the stream. */
            ppu_cfg->crop.x = ctx->dec_info.crop_params.crop_left_offset;
            ppu_cfg->crop.y = ctx->dec_info.crop_params.crop_top_offset;
            ppu_cfg->crop.width = NEXT_MULTIPLE(ctx->dec_info.crop_params.crop_out_width, 2);
            ppu_cfg->crop.height = NEXT_MULTIPLE(ctx->dec_info.crop_params.crop_out_height, 2);
        }
        LOG_DEBUG(DEC, "Crop flag 0x%x, region [%u, %u, %ux%u]", chn->params.cropInfo.flag,
                  ppu_cfg->crop.x, ppu_cfg->crop.y, ppu_cfg->crop.width, ppu_cfg->crop.height);
    }

    memcpy(ctx->dec_cfg.ppu_cfg, ctx->ppu_cfg, sizeof(ctx->ppu_cfg));
    ctx->dec_cfg.align = align;
}

/* Handles the DEC_HDRS_RDY state: get the sequence information, adjust the PP configuration and
 * install it into the decoder instance, same flow as HeadersDecodedCb() of the test bench. */
static vmppResult handle_headers_ready(struct va_dec_channel *chn,
                                       struct video_decoder_private_context *ctx)
{
    enum DecRet dec_ret = VCDecGetInfo(chn->codec_inst, &ctx->dec_info);
    int i;
    if (dec_ret != DEC_OK) {
        LOG_ERROR(DEC, "VCDecGetInfo failed: %s(%d)", VCDecRetStr(dec_ret), dec_ret);
        return result_from_vsi(dec_ret);
    }

    LOG_INFO(DEC, "Sequence %ux%u, crop %ux%u, bit depth %u, chroma format %u", ctx->dec_info.pic_width,
             ctx->dec_info.pic_height, ctx->dec_info.crop_params.crop_out_width,
             ctx->dec_info.crop_params.crop_out_height, ctx->dec_info.bit_depth_luma,
             ctx->dec_info.chroma_format_idc);

    if(ctx->dec_info.out_bit_depth > 0 &&
       ctx->dec_info.out_bit_depth != ctx->dec_info.bit_depth_luma &&
       ctx->dec_info.out_bit_depth == 8){
        printf("Note TB: The current frame is cast to 8bit output.\n");
        /* Output 8-bit picture even if the decoded pictures are 10 bits deep. */
        for (i = 0; i < DEC_MAX_OUT_COUNT; i++)
            ctx->ppu_cfg[i].out_cut_8bits = 1;
    }

    for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
        if (!ctx->ppu_cfg[i].enabled) continue;
            ctx->ppu_cfg[i].video_range = ctx->dec_info.video_range;
    } // default video_range is 1 (full), should be overwritten by stream information (get from sps)

    /* Ajust user cropping params based on cropping params from sequence info. */
    if (ctx->dec_info.crop_params.crop_left_offset != 0 ||
        ctx->dec_info.crop_params.crop_top_offset != 0 ||
        (ctx->dec_info.crop_params.crop_out_width != ctx->dec_info.pic_width &&
        ctx->dec_info.crop_params.crop_out_width != 0) ||
        (ctx->dec_info.crop_params.crop_out_height != ctx->dec_info.pic_height &&
        ctx->dec_info.crop_params.crop_out_height != 0)) {
        //int i;
        //struct DecConfig *config = &inst->current_command->params.config;
        for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
            if (!ctx->ppu_cfg[i].enabled) continue;

            if (!ctx->ppu_cfg[i].crop.enabled && !ctx->dec_info.dis_comformance_window) {
            ctx->ppu_cfg[i].crop.x = ctx->dec_info.crop_params.crop_left_offset;
            ctx->ppu_cfg[i].crop.y = ctx->dec_info.crop_params.crop_top_offset;
            /*support odd crop*/
            // config->ppu_cfg[i].crop.width = (inst->sequence_info.crop_params.crop_out_width+1) & ~0x1;
            // config->ppu_cfg[i].crop.height = (inst->sequence_info.crop_params.crop_out_height+1) & ~0x1;
            ctx->ppu_cfg[i].crop.width = ctx->dec_info.crop_params.crop_out_width;
            ctx->ppu_cfg[i].crop.height = ctx->dec_info.crop_params.crop_out_height;
            } else if (ctx->ppu_cfg[i].crop.enabled && !ctx->dec_info.dis_comformance_window){
            ctx->ppu_cfg[i].crop.x += ctx->dec_info.crop_params.crop_left_offset;
            ctx->ppu_cfg[i].crop.y += ctx->dec_info.crop_params.crop_top_offset;
            /*support odd crop*/
            // if(!config->ppu_cfg[i].crop.width)
            //   config->ppu_cfg[i].crop.width = (inst->sequence_info.crop_params.crop_out_width+1) & ~0x1;
            // if(!config->ppu_cfg[i].crop.height)
            //   config->ppu_cfg[i].crop.height = (inst->sequence_info.crop_params.crop_out_height+1) & ~0x1;
            if(!ctx->ppu_cfg[i].crop.width)
                ctx->ppu_cfg[i].crop.width = ctx->dec_info.crop_params.crop_out_width;
            if(!ctx->ppu_cfg[i].crop.height)
                ctx->ppu_cfg[i].crop.height = ctx->dec_info.crop_params.crop_out_height;
            }
            ctx->ppu_cfg[i].enabled = 1;
            ctx->ppu_cfg[i].crop.enabled = 1;
        }
    }

    /* Ajust user cropping params based on cropping params from sequence info. */
    /* what is the purpose of the next adjust about the client->test_params.ppu_cfg */
    if (ctx->dec_info.crop_params.crop_left_offset != 0 ||
        ctx->dec_info.crop_params.crop_top_offset != 0 ||
        ctx->dec_info.crop_params.crop_out_width != ctx->dec_info.pic_width ||
        ctx->dec_info.crop_params.crop_out_height != ctx->dec_info.pic_height) {
        for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
            if (!ctx->ppu_cfg[i].enabled)
            continue;

            if (!ctx->ppu_cfg[i].crop.enabled && !ctx->dec_info.dis_comformance_window) {
            ctx->ppu_cfg[i].crop.x = ctx->dec_info.crop_params.crop_left_offset;
            ctx->ppu_cfg[i].crop.y = ctx->dec_info.crop_params.crop_top_offset;
            ctx->ppu_cfg[i].crop.width = (ctx->dec_info.crop_params.crop_out_width +1) & ~0x1;
            ctx->ppu_cfg[i].crop.height = (ctx->dec_info.crop_params.crop_out_height +1) & ~0x1;
            } else if(ctx->ppu_cfg[i].crop.enabled && !ctx->dec_info.dis_comformance_window){
            ctx->ppu_cfg[i].crop.x += ctx->dec_info.crop_params.crop_left_offset;
            ctx->ppu_cfg[i].crop.y += ctx->dec_info.crop_params.crop_top_offset;
            if(!ctx->ppu_cfg[i].crop.width)
                ctx->ppu_cfg[i].crop.width = (ctx->dec_info.crop_params.crop_out_width+1) & ~0x1;
            if(!ctx->ppu_cfg[i].crop.height)
                ctx->ppu_cfg[i].crop.height = (ctx->dec_info.crop_params.crop_out_height+1) & ~0x1;
            }
            ctx->ppu_cfg[i].enabled = 1;
            ctx->ppu_cfg[i].crop.enabled = 1;
            ctx->pp_enabled = 1;
        }
    }

    config_pp_params(chn, ctx);
    ctx->headers_ready = 1;

    dec_ret = VCDecSetInfo(chn->codec_inst, &ctx->dec_cfg);
    if (dec_ret != DEC_OK) {
        LOG_ERROR(DEC, "VCDecSetInfo failed: %s(%d)", VCDecRetStr(dec_ret), dec_ret);
        return result_from_vsi(dec_ret);
    }

    return vmpp_RSLT_OK;
}

/* Allocates the external buffers requested by the decoder, see BufferRequestCb() of the test bench. */
static vmppResult allocate_ext_buffers(struct va_dec_channel *chn,
                                       struct video_decoder_private_context *ctx)
{
    struct DecBufferInfo hbuf = {0};
    vmppResult ret = vmpp_RSLT_OK;
    enum DecRet dec_ret;
    uint32_t i;

    dec_ret = VCDecGetBufferInfo(chn->codec_inst, &hbuf);
    if (dec_ret != DEC_WAITING_FOR_BUFFER && dec_ret != DEC_OK) {
        LOG_ERROR(DEC, "VCDecGetBufferInfo failed: %s(%d)", VCDecRetStr(dec_ret), dec_ret);
        return result_from_vsi(dec_ret);
    }

    if (hbuf.buf_to_free.bus_address != 0)
        release_ext_buffers(chn->cwl, ctx, hbuf.buf_to_free.bus_address, 0);

    if (!hbuf.next_buf_size) {
        LOG_DEBUG(DEC, "No more buffer is requested by the decoder.");
        return vmpp_RSLT_OK;
    }

    ctx->buffer_size = (uint32_t)hbuf.next_buf_size;
    ctx->min_buffer_number = hbuf.buf_num + ctx->guard_size;

    for (i = 0; i < hbuf.buf_num; i++) {
        struct DWLLinearMem mem = {0};

        if (ctx->ext_buffer_number >= MAX_PIC_BUFFERS) {
            LOG_WARN(DEC, "No free slot left to add external buffer, %u already added",
                     ctx->ext_buffer_number);
            break;
        }

        mem.mem_type = DWL_MEM_TYPE_DPB;
        SET_MEM_USAGE(mem.mem_type,
                      ctx->pp_enabled ? DWL_MEM_USAGE_OUT_PP : DWL_MEM_USAGE_OUT_REFERENCE, 0);
        mem.size = hbuf.next_buf_size;
        mem.logical_size = hbuf.next_buf_size;

        if (DWLMallocLinear(chn->cwl, hbuf.next_buf_size, &mem) != DWL_OK) {
            LOG_ERROR(DEC, "Failed to allocate %llu bytes for external buffer",
                      (unsigned long long)hbuf.next_buf_size);
            ret = vmpp_RSLT_ERR_NO_MEMORY;
            break;
        }

        /* NOTE: DEC_WAITING_FOR_BUFFER means the buffer has been accepted but the decoder is
                 still waiting for more buffers, DEC_OK means all the requested buffers are added. */
        dec_ret = VCDecAddBuffer(chn->codec_inst, &mem);
        if (dec_ret != DEC_OK && dec_ret != DEC_WAITING_FOR_BUFFER) {
            LOG_WARN(DEC, "VCDecAddBuffer(%llu bytes) rejected: %s(%d)",
                     (unsigned long long)hbuf.next_buf_size, VCDecRetStr(dec_ret), dec_ret);
            DWLFreeLinear(chn->cwl, &mem);
            ret = result_from_vsi(dec_ret);
            break;
        }
        ctx->ext_buffers[ctx->ext_buffer_number] = mem;
        ctx->buffer_consumed[ctx->ext_buffer_number] = 1;
        ctx->ext_buffer_number++;
    }

    return ret;
}

vmppResult video_decoder_create_chn(struct va_dec_channel *chn)
{
    struct video_decoder_private_context *ctx = NULL;
    struct DWLInitParam dwl_init = {0};
    struct DecInitConfig init_cfg = {0};
    const void *dwl = NULL;
    enum DWLClientType client_type;
    enum DecCodec codec;
    enum DecRet dec_ret;
    uint32_t core_mask = 0, i = 0;

    if (!chn) {
        LOG_ERROR(DEC, "Invalid parameters: chn %p", chn);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    codec = codec_from_vmpp(chn->params.codecType);
    if (codec == DEC_FMT_MAX) {
        LOG_ERROR(DEC, "Unsupported codec type: 0x%x", chn->params.codecType);
        return vmpp_RSLT_ERR_UNSUPPORTED;
    }

    if (chn->params.memoryMode != vmpp_DEC_MEM_NORMAL) {
        LOG_ERROR(DEC, "Memory mode %d is not supported by the generic video decoder",
                  chn->params.memoryMode);
        return vmpp_RSLT_ERR_UNSUPPORTED;
    }

    ctx = (struct video_decoder_private_context *)malloc(sizeof(*ctx));
    if (!ctx) {
        LOG_ERROR(DEC, "Failed to allocate the private context of %s decoder", codec_name(codec));
        return vmpp_RSLT_ERR_NO_MEMORY;
    }
    memset(ctx, 0, sizeof(*ctx));
    chn->private_context = ctx;
    ctx->codec = codec;
    ctx->pp_enabled = 1;
    for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
        ctx->ppu_cfg[i].shaper_enabled = 1;
        ctx->ppu_cfg[i].chroma_format = PP_YUV420;
        if (chn->params.pixelFormat == vmpp_PIX_FMT_YUV420_PLANAR_10BIT_P010) 
            ctx->ppu_cfg[i].out_p010 = 1;
        if (chn->params.pixelFormat == vmpp_PIX_FMT_YUV420_PLANAR_10BIT_I010) 
            ctx->ppu_cfg[i].out_I010 = 1;
        ctx->ppu_cfg[i].source_range = 1;
        ctx->ppu_cfg[i].set_source_range_enable = 0;
        ctx->ppu_cfg[i].target_range = 0;
        ctx->ppu_cfg[i].set_target_range_enable = 0;
        ctx->ppu_cfg[i].dither_enable = 1;
        ctx->ppu_cfg[i].antialias = 1;
        ctx->ppu_cfg[i].enable_3dlut = 0;
        ctx->ppu_cfg[i].crop_id = 1;
        ctx->ppu_cfg[i].align_pixel_w = 1;
        ctx->ppu_cfg[i].enabled = 1;
    }
    ctx->guard_size = chn->params.extraBufferNumber;

    client_type = dwl_client_from_codec(codec);
    dwl_init.client_type = client_type;
    dwl_init.dec_dev     = chn->params.decDevice;
    dwl_init.mem_dev     = chn->params.memDevice;

    dwl = DWLInit(&dwl_init);
    if (!dwl) {
        LOG_ERROR(DEC, "%s: failed to initialize DWL", codec_name(codec));
        free(ctx);
        chn->private_context = NULL;
        return vmpp_RSLT_ERR_DEC_DWL;
    }
    chn->cwl = dwl;
    chn->frame_struct_size = sizeof(struct DecPictures);
    chn->max_buf_num = MAX_PIC_BUFFERS;

    DWLGetHwFeaturesByClientType(dwl, client_type, &core_mask);
    if (core_mask == 0) {
        LOG_ERROR(DEC, "%s decoding is not supported by this hardware", codec_name(codec));
        DWLRelease(dwl);
        chn->cwl = NULL;
        free(ctx);
        chn->private_context = NULL;
        return vmpp_RSLT_ERR_UNSUPPORTED;
    }

    struct DecSwHwBuild dec_build = VCDecGetBuild(dwl, client_type);
    LOG_DEBUG(DEC, "%s decoder SW build %u, ASIC id 0x%x, core mask 0x%x", codec_name(codec),
              dec_build.sw_build, dec_build.asic_id, core_mask);

    init_cfg.dwl_inst = dwl;
    init_cfg.codec = codec;
    init_cfg.decoder_mode = decoder_mode_from_vmpp(chn->params.decodeMode);
    init_cfg.error_handling = DEC_EC_FRAME_NO_ERROR;
    init_cfg.use_video_compressor = 1;
    init_cfg.disable_picture_reordering = chn->params.noOutputReordering;
    init_cfg.guard_size = ctx->guard_size;
    init_cfg.use_adaptive_buffers = 1;
    init_cfg.skip_frame = DEC_SKIP_NONE;
    init_cfg.mc_cfg.mc_enable = 0;

    dec_ret = VCDecInit((const void **)&chn->codec_inst, &init_cfg);
    if (dec_ret != DEC_OK) {
        LOG_ERROR(DEC, "%s decoder initialization failed: %s(%d)", codec_name(codec),
                  VCDecRetStr(dec_ret), dec_ret);
        DWLRelease(dwl);
        chn->cwl = NULL;
        free(ctx);
        chn->private_context = NULL;
        return vmpp_RSLT_ERR_DEC_INIT;
    }

    pthread_mutex_init(&ctx->buffer_mutex, NULL);

    return vmpp_RSLT_OK;
}

vmppResult video_decoder_destory_chn(struct va_dec_channel *chn)
{
    if (!chn)
        return vmpp_RSLT_ERR_INVALID_PARAMS;

    struct video_decoder_private_context *ctx =
        (struct video_decoder_private_context *)chn->private_context;
    if (ctx != NULL) {
        release_ext_buffers(chn->cwl, ctx, 0, 1);
        pthread_mutex_destroy(&ctx->buffer_mutex);
        free(chn->private_context);
        chn->private_context = NULL;
    }

    if (chn->codec_inst != NULL) {
        VCDecRelease(chn->codec_inst);
        chn->codec_inst = NULL;
    }
    if (chn->cwl != NULL) {
        DWLRelease(chn->cwl);
        chn->cwl = NULL;
    }

    LOG_DEBUG(DEC, "Generic video decoder instance destroyed.");
    return vmpp_RSLT_OK;
}

vmppResult video_decoder_send_stream(struct va_dec_channel *chn, vmppStream *stream,
                                     uint32_t timeout, vaSendStreamCb send_cb, uint8_t *SEI_flag)
{
    UNUSED_PARAMETER(send_cb);

    struct video_decoder_private_context *ctx = NULL;
    struct DecInputParameters input = {0};
    struct DWLLinearMem stream_mem = {0};
    struct SEI_buffer sei = {0};
    uint8_t *sei_data = NULL;
    vmppResult ret = vmpp_RSLT_OK;
    enum DecRet dec_ret = DEC_OK;
    uint64_t start = 0, tick = 0;
    uint32_t no_buf_cnt = 0;
    uint32_t no_progress_cnt = 0;

    if (!chn || !chn->codec_inst || !stream || !stream->stream || stream->len == 0) {
        LOG_ERROR(DEC, "Invalid parameters: chn %p, stream %p, len %u", chn, stream,
                  stream ? stream->len : 0);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }
    ctx = (struct video_decoder_private_context *)chn->private_context;
    if (!ctx) {
        LOG_ERROR(DEC, "Video decoder private context is NULL.");
        return vmpp_RSLT_ERR_NOT_INITIALIZED;
    }
    if (chn->params.memoryMode != vmpp_DEC_MEM_NORMAL) {
        LOG_ERROR(DEC, "Memory mode %d is not supported by the generic video decoder",
                  chn->params.memoryMode);
        return vmpp_RSLT_ERR_UNSUPPORTED;
    }
    if (SEI_flag)
        *SEI_flag = 0;

    /* The unified decoder needs the stream accessible from both the host and the hardware. */
    stream_mem.mem_type = DWL_MEM_TYPE_CPU;
#ifdef SUPPORT_DMA
    stream_mem.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
#endif
    SET_MEM_USAGE(stream_mem.mem_type, DWL_MEM_USAGE_IN_STRM, 0);
    if (DWLMallocLinear(chn->cwl, stream->len, &stream_mem) != DWL_OK) {
        LOG_ERROR(DEC, "Failed to allocate a stream buffer of %u bytes", stream->len);
        return vmpp_RSLT_ERR_NO_MEMORY;
    }
    DWLmemcpy(stream_mem.virtual_address, stream->stream, stream->len);

    input.stream_buffer = stream_mem;
    input.stream = (u8 *)stream_mem.virtual_address;
    input.strm_len = stream->len;
    input.pic_id = (u32)stream->pts;
    input.skip_frame = DEC_SKIP_NONE;
    input.p_user_data = chn;

    if (chn->params.enSEIParser) {
        sei_data = (uint8_t *)malloc(VIDEO_SEI_BUFFER_SIZE);
        if (sei_data) {
            memset(sei_data, 0, VIDEO_SEI_BUFFER_SIZE);
            sei.buffer = sei_data;
            sei.total_size = VIDEO_SEI_BUFFER_SIZE;
            sei.available_size = 0;
            input.sei_buffer = &sei;
        } else {
            LOG_WARN(DEC, "Failed to allocate the SEI capture buffer.");
        }
    }

    start = va_gettime_ns();
    do {
        struct DecOutput output = {0};

        dec_ret = VCDecDecode(chn->codec_inst, &output, &input);
        if (output.sei_buffer != NULL && output.sei_buffer->available_size > 0) {
            if (SEI_flag)
                *SEI_flag = 1;
            output.sei_buffer->available_size = 0;
        }

        switch (dec_ret) {
        case DEC_PIC_DECODED:
            ctx->pic_decode_number++;
            break;
        case DEC_STRM_PROCESSED:
        case DEC_SLICE_RDY:
        case DEC_SCAN_PROCESSED:
            break;
        case DEC_HDRS_RDY:
            ret = handle_headers_ready(chn, ctx);
            if (ret != vmpp_RSLT_OK)
                LOG_ERROR(DEC, "Failed to handle the new sequence: %d", ret);
            break;
        case DEC_WAITING_FOR_BUFFER:
            /* Refresh the sequence information, it may have changed. */
            VCDecGetInfo(chn->codec_inst, &ctx->dec_info);
            ret = allocate_ext_buffers(chn, ctx);
            if (ret != vmpp_RSLT_OK)
                LOG_ERROR(DEC, "Failed to allocate external buffers: %d", ret);
            break;
        case DEC_NO_DECODING_BUFFER:
            if (no_buf_cnt > MAX_WAIT_CNT) {
                LOG_WARN(DEC, "Timeout(%u ms) for VCDecDecode: %s(%d)", timeout, VCDecRetStr(dec_ret),
                         dec_ret);
                ret = vmpp_RSLT_ERR_NO_BUFFER;
            } else {
                usleep(1000);
                no_buf_cnt++;
            }
            break;
        case DEC_PENDING_FLUSH:
            /* The decoder is flushing the DPB, it has consumed nothing and expects the pending
               pictures to be output before it can continue, see the no progress handling below. */
            LOG_DEBUG(DEC, "VCDecDecode reports a pending flush, %u bytes left", output.data_left);
            break;
        case DEC_END_OF_STREAM:
            LOG_INFO(DEC, "VCDecDecode reports end of stream, flushing %s decoder",
                     codec_name(ctx->codec));
            break;
        default:
            LOG_ERROR(DEC, "VCDecDecode returns %s(%d)", VCDecRetStr(dec_ret), dec_ret);
            ret = result_from_vsi(dec_ret);
            break;
        }

        if (ret != vmpp_RSLT_OK)
            break;

        tick = va_gettime_ns();
        if (timeout > 0 && (float)(tick - start) / 1000000.0 > (float)timeout) {
            LOG_WARN(DEC, "Timeout(%u ms) for sending stream, %u bytes left", timeout, output.data_left);
            ret = vmpp_RSLT_ERR_NO_BUFFER;
            break;
        }

        if (output.data_left == 0) {
            ctx->input_again_number = 0;
            break;
        }

        if (output.data_left >= input.strm_len) {
            /* The decoder made no progress on this stream buffer, it is waiting for something to
               happen, typically for the pending pictures to be output (DEC_PENDING_FLUSH). Give it
               a chance to move on, otherwise ask the application to send this stream again. */
            if (no_progress_cnt++ >= MAX_NO_PROGRESS_CNT) {
                if (ctx->input_again_number++ >= MAX_INPUT_AGAIN_CNT) {
                    LOG_ERROR(DEC, "%u bytes of stream are still not consumed (%s(%d)) after %u retries",
                              output.data_left, VCDecRetStr(dec_ret), dec_ret, ctx->input_again_number);
                    ret = vmpp_RSLT_ERR_NO_BUFFER;
                    break;
                }
                LOG_WARN(DEC, "%u bytes of stream are not consumed (%s(%d)), they need to be sent again",
                         output.data_left, VCDecRetStr(dec_ret), dec_ret);
                ret = vmpp_RSLT_DEC_INPUT_AGAIN;
                break;
            }
            usleep(1000);
            continue;
        }
        no_progress_cnt = 0;
        ctx->input_again_number = 0;

        /* Continue with the remaining part of the stream. */
        input.stream = output.strm_curr_pos;
        input.strm_len = output.data_left;
    } while (input.strm_len > 0);

    DWLFreeLinear(chn->cwl, &stream_mem);
    if (sei_data)
        free(sei_data);

    return ret;
}

vmppResult video_decoder_receive_frame(struct va_dec_channel *chn, vmppFrame *frame,
                                       vmppDecOutputOptions *out_opt)
{
    struct video_decoder_private_context *ctx = NULL;
    struct DecPictures *dec_picture = NULL;
    struct DecPicture *pic = NULL;
    vmppResult ret;
    enum DecRet dec_ret;
    uint32_t size;
    uint32_t output_height;
    int32_t idx;

    if (!chn || !chn->codec_inst || !frame || !frame->privateData || !out_opt) {
        LOG_ERROR(DEC, "Invalid parameters: chn %p, inst %p, frame %p, options %p", chn,
                  chn ? chn->codec_inst : NULL, frame, out_opt);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }
    ctx = (struct video_decoder_private_context *)chn->private_context;
    if (!ctx) {
        LOG_ERROR(DEC, "Video decoder private context is NULL.");
        return vmpp_RSLT_ERR_NOT_INITIALIZED;
    }

    dec_picture = (struct DecPictures *)frame->privateData;
    pic = &dec_picture->pictures[0];

    dec_ret = VCDecNextPicture(chn->codec_inst, dec_picture);
    if (dec_ret == DEC_PIC_RDY) {
        pthread_mutex_lock(&ctx->buffer_mutex);
        idx = find_ext_buffer_index(ctx, pic->luma.virtual_address);
        if (idx >= 0)
            ctx->buffer_consumed[idx] = 0;
        pthread_mutex_unlock(&ctx->buffer_mutex);

        output_height = chn->params.cropInfo.flag == vmpp_CROP_CUSTOMIZED
                            ? chn->params.cropInfo.height
                            : pic->pic_height;
        /* The height must be even for the 4:2:0 output. */
        output_height = (output_height + 1) & ~0x1;
        size = IS_PIC_MONOCHROME(pic->picture_info.format)
                   ? pic->pic_stride * output_height
                   : pic->pic_stride * output_height * 3 / 2;

        if (chn->params.memoryMode != vmpp_DEC_MEM_NORMAL) {
            LOG_ERROR(DEC, "Memory mode %d is not supported by the generic video decoder",
                      chn->params.memoryMode);
            return vmpp_RSLT_ERR_UNSUPPORTED;
        }

        if (out_opt->memoryType == vmpp_MEM_HOST) {
            frame->data[0] = (uint8_t *)pic->luma.virtual_address;
            frame->data[1] = (uint8_t *)pic->chroma.virtual_address;
        } else {
            frame->data[0] = NULL;
            frame->data[1] = NULL;
        }
        frame->memoryType = out_opt->memoryType;
        frame->busAddress[0] = (vmppDevAddr)(uintptr_t)pic->luma.bus_address;
        frame->busAddress[1] = (vmppDevAddr)(uintptr_t)pic->chroma.bus_address;
        frame->dataSize = size;
        frame->pixelFormat = format_from_vsi(pic->picture_info.format);
        frame->width = pic->pic_width;
        frame->height = output_height;
        frame->stride[0] = pic->pic_stride;
        frame->stride[1] = pic->pic_stride_ch;
        frame->pts = pic->picture_info.pic_id;
        frame->frameType = frame_type_from_vsi(pic->picture_info.pic_coding_type);

        if (chn->params.cropInfo.flag == vmpp_CROP_CUSTOMIZED) {
            frame->cropInfo.flag = 0;
            frame->cropInfo.width = pic->pic_width;
            frame->cropInfo.height = output_height;
            frame->cropInfo.xOffset = 0;
            frame->cropInfo.yOffset = 0;
        } else if (pic->pic_width != pic->sequence_info.crop_params.crop_out_width ||
                   pic->pic_height != pic->sequence_info.crop_params.crop_out_height) {
            frame->cropInfo.flag = 1;
            frame->cropInfo.width = pic->sequence_info.crop_params.crop_out_width;
            frame->cropInfo.height = pic->sequence_info.crop_params.crop_out_height;
            frame->cropInfo.xOffset = pic->sequence_info.crop_params.crop_left_offset;
            frame->cropInfo.yOffset = pic->sequence_info.crop_params.crop_top_offset;
        } else {
            frame->cropInfo.flag = 0;
            frame->cropInfo.width = pic->pic_width;
            frame->cropInfo.height = output_height;
            frame->cropInfo.xOffset = 0;
            frame->cropInfo.yOffset = 0;
        }

        LOG_DEBUG(DEC, "Picture [%ux%u], stride [%ux%u], pts %llu, size %u, format %d", frame->width,
                  frame->height, frame->stride[0], frame->stride[1], (unsigned long long)frame->pts,
                  frame->dataSize, frame->pixelFormat);

        ret = vmpp_RSLT_OK;
    } else if (dec_ret == DEC_OK) {
        /* No picture is available for output for now. */
        LOG_DEBUG(DEC, "VCDecNextPicture reports no picture available.");
        ret = vmpp_RSLT_WARN_MORE_DATA;
    } else if (dec_ret == DEC_FLUSHED) {
        /* The decoder has been flushed, wait for the pending pictures or the end of stream. */
        LOG_INFO(DEC, "VCDecNextPicture reports the decoder is flushed.");
        ret = vmpp_RSLT_WARN_MORE_DATA;
    } else if (dec_ret == DEC_END_OF_STREAM) {
        LOG_INFO(DEC, "VCDecNextPicture reports end of stream.");
        ret = vmpp_RSLT_WARN_EOS;
    } else {
        LOG_ERROR(DEC, "VCDecNextPicture returns %s(%d)", VCDecRetStr(dec_ret), dec_ret);
        ret = result_from_vsi(dec_ret);
        if (ret == vmpp_RSLT_OK)
            ret = vmpp_RSLT_WARN_MORE_DATA;
    }

    return ret;
}

vmppResult video_decoder_transfer_frame(struct va_dec_channel *chn, vmppFrame *frame)
{
    struct DecPictures *dec_picture = NULL;
    struct DecPicture *pic = NULL;

    if (!chn || !chn->codec_inst || !frame || !frame->privateData) {
        LOG_ERROR(DEC, "Invalid parameters: chn %p, inst %p, frame %p", chn,
                  chn ? chn->codec_inst : NULL, frame);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    dec_picture = (struct DecPictures *)frame->privateData;
    pic = &dec_picture->pictures[0];

    if (pic->luma.virtual_address == NULL) {
        LOG_ERROR(DEC, "Picture hasn't been transferred to the host.");
        return vmpp_RSLT_ERR_INVALID_STATE;
    }

    frame->data[0] = (uint8_t *)pic->luma.virtual_address;
    frame->data[1] = (uint8_t *)pic->chroma.virtual_address;
    frame->memoryType = vmpp_MEM_HOST;

    return vmpp_RSLT_OK;
}

vmppResult video_decoder_release_frame(struct va_dec_channel *chn, vmppFrame *frame)
{
    struct video_decoder_private_context *ctx = NULL;
    struct DecPictures *dec_picture = NULL;
    struct DecPicture *pic = NULL;
    vmppResult ret = vmpp_RSLT_OK;
    enum DecRet dec_ret;
    int32_t idx;

    if (!chn || !chn->codec_inst || !frame || !frame->privateData) {
        LOG_ERROR(DEC, "Invalid parameters: chn %p, inst %p, frame %p", chn,
                  chn ? chn->codec_inst : NULL, frame);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }
    ctx = (struct video_decoder_private_context *)chn->private_context;
    if (!ctx) {
        LOG_ERROR(DEC, "Video decoder private context is NULL.");
        return vmpp_RSLT_ERR_NOT_INITIALIZED;
    }

    dec_picture = (struct DecPictures *)frame->privateData;
    pic = &dec_picture->pictures[0];

    pthread_mutex_lock(&ctx->buffer_mutex);
    idx = find_ext_buffer_index(ctx, pic->luma.virtual_address);
    if (idx >= 0)
        ctx->buffer_consumed[idx] = 1;
    pthread_mutex_unlock(&ctx->buffer_mutex);

    dec_ret = VCDecPictureConsumed(chn->codec_inst, dec_picture);
    if (dec_ret != DEC_OK) {
        LOG_ERROR(DEC, "VCDecPictureConsumed returns %s(%d)", VCDecRetStr(dec_ret), dec_ret);
        ret = result_from_vsi(dec_ret);
        if (ret == vmpp_RSLT_OK)
            ret = vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    return ret;
}

vmppResult video_decoder_get_stream_info(struct va_dec_channel *chn, vmppDecStreamInfo *info)
{
    struct video_decoder_private_context *ctx = NULL;

    if (!chn || !chn->codec_inst || !info) {
        LOG_ERROR(DEC, "Invalid parameters: chn %p, inst %p, info %p", chn,
                  chn ? chn->codec_inst : NULL, info);
        return vmpp_RSLT_ERR_NOT_INITIALIZED;
    }
    ctx = (struct video_decoder_private_context *)chn->private_context;
    if (!ctx) {
        LOG_ERROR(DEC, "Video decoder private context is NULL.");
        return vmpp_RSLT_ERR_NOT_INITIALIZED;
    }

    LOG_INFO(DEC, "Stream info: resolution %ux%u", ctx->dec_info.pic_width, ctx->dec_info.pic_height);

    info->width = ctx->dec_info.pic_width;
    info->height = ctx->dec_info.pic_height;
    if (ctx->dec_info.timing_info_present_flag && ctx->dec_info.num_units_in_tick &&
        ctx->dec_info.time_scale)
        info->fps = ctx->dec_info.time_scale / (2 * ctx->dec_info.num_units_in_tick);
    else
        info->fps = 0;
    info->pixelSize = ctx->dec_info.bit_depth_luma > 8 ? 2 : 1;

    return vmpp_RSLT_OK;
}

vmppResult video_decoder_end_of_stream(struct va_dec_channel *chn)
{
    if (!chn || !chn->codec_inst) {
        LOG_ERROR(DEC, "Invalid parameters: chn %p, inst %p", chn, chn ? chn->codec_inst : NULL);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    VCDecEndOfStream(chn->codec_inst);

    return vmpp_RSLT_OK;
}

/* Parses the sequence information of a stream with a temporary decoder instance, no channel nor DWL
 * instance is required, the probing always opens and closes its own ones. */
vmppResult video_decoder_get_video_info(vmppStream *stream, vmppCodecType codecType,
                                       vmppDecVideoInfo *info)
{
    struct DWLInitParam dwl_init = {0};
    struct DecInitConfig init_cfg = {0};
    struct DecInputParameters input = {0};
    struct DWLLinearMem stream_mem = {0};
    struct DecSequenceInfo seq_info = {0};
    const void *dwl = NULL;
    void *inst = NULL;
    vmppResult ret = vmpp_RSLT_ERR_INVALID_DATA;
    enum DWLClientType client_type;
    enum DecCodec codec;
    enum DecRet dec_ret;

    if (!stream || !stream->stream || stream->len == 0 || !info) {
        LOG_ERROR(DEC, "Invalid parameters: stream %p, info %p", stream, info);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    codec = codec_from_vmpp(codecType);
    if (codec == DEC_FMT_MAX) {
        LOG_ERROR(DEC, "Unsupported codec type: 0x%x", codecType);
        return vmpp_RSLT_ERR_UNSUPPORTED;
    }

    client_type = dwl_client_from_codec(codec);
    dwl_init.client_type = client_type;
    dwl_init.dec_dev     = "/dev/hantrodec";
    dwl_init.mem_dev     = "/dev/memalloc";

    dwl = DWLInit(&dwl_init);
    if (!dwl) {
        LOG_ERROR(DEC, "Failed to initialize DWL for %s stream parsing", codec_name(codec));
        return vmpp_RSLT_ERR_DEC_DWL;
    }

    init_cfg.dwl_inst = dwl;
    init_cfg.codec = codec;
    init_cfg.decoder_mode = DEC_NORMAL;
    init_cfg.error_handling = DEC_EC_FRAME_NO_ERROR;
    init_cfg.use_adaptive_buffers = 1;

    dec_ret = VCDecInit((const void **)&inst, &init_cfg);
    if (dec_ret != DEC_OK) {
        LOG_ERROR(DEC, "Failed to initialize the temporary %s decoder: %s(%d)", codec_name(codec),
                  VCDecRetStr(dec_ret), dec_ret);
        VCDecRelease(inst);
        DWLRelease(dwl);
        return vmpp_RSLT_ERR_DEC_INIT;
    }

    stream_mem.mem_type = DWL_MEM_TYPE_CPU;
    SET_MEM_USAGE(stream_mem.mem_type, DWL_MEM_USAGE_IN_STRM, 0);
    if (DWLMallocLinear(dwl, stream->len, &stream_mem) != DWL_OK) {
        LOG_ERROR(DEC, "Failed to allocate %u bytes for stream parsing", stream->len);
        VCDecRelease(inst);
        DWLRelease(dwl);
        return vmpp_RSLT_ERR_NO_MEMORY;
    }
    DWLmemcpy(stream_mem.virtual_address, stream->stream, stream->len);

    input.stream_buffer = stream_mem;
    input.stream = (u8 *)stream_mem.virtual_address;
    input.strm_len = stream->len;
    input.pic_id = (u32)stream->pts;
    input.skip_frame = DEC_SKIP_NONE;

    /* Only the sequence headers are needed, no picture buffer is registered. */
    do {
        struct DecOutput output = {0};

        dec_ret = VCDecDecode(inst, &output, &input);
        if (dec_ret == DEC_HDRS_RDY) {
            dec_ret = VCDecGetInfo(inst, &seq_info);
            if (dec_ret == DEC_OK) {
                ret = vmpp_RSLT_OK;
                break;
            }
            LOG_ERROR(DEC, "VCDecGetInfo failed: %s(%d)", VCDecRetStr(dec_ret), dec_ret);
            ret = result_from_vsi(dec_ret);
            break;
        } else if (dec_ret == DEC_WAITING_FOR_BUFFER || dec_ret == DEC_NO_DECODING_BUFFER) {
            LOG_INFO(DEC, "Picture buffers are needed before the sequence headers are ready");
            ret = vmpp_RSLT_ERR_NO_BUFFER;
            break;
        } else if (dec_ret != DEC_OK && dec_ret != DEC_STRM_PROCESSED && dec_ret != DEC_SLICE_RDY &&
                   dec_ret != DEC_SCAN_PROCESSED && dec_ret != DEC_PIC_DECODED) {
            LOG_ERROR(DEC, "VCDecDecode returns %s(%d)", VCDecRetStr(dec_ret), dec_ret);
            ret = result_from_vsi(dec_ret);
            break;
        }

        if (output.data_left == 0 || output.data_left >= input.strm_len)
            break;
        input.stream = output.strm_curr_pos;
        input.strm_len = output.data_left;
    } while (input.strm_len > 0);

    if (ret == vmpp_RSLT_OK) {
        info->width = seq_info.pic_width;
        info->height = seq_info.pic_height;
        info->cropFlag = seq_info.crop_params.crop_out_width &&
                                 seq_info.crop_params.crop_out_width <= seq_info.pic_width &&
                                 seq_info.crop_params.crop_out_height <= seq_info.pic_height
                             ? 1
                             : 0;
        info->cropWidth = seq_info.crop_params.crop_out_width;
        info->cropHeight = seq_info.crop_params.crop_out_height;
        info->xOffset = seq_info.crop_params.crop_left_offset;
        info->yOffset = seq_info.crop_params.crop_top_offset;
        if (seq_info.timing_info_present_flag && seq_info.num_units_in_tick) {
            info->fps.numerator = seq_info.time_scale;
            info->fps.denominator = seq_info.num_units_in_tick * 2;
        } else {
            info->fps.numerator = 0;
            info->fps.denominator = 1;
        }
        info->pixelFormat = format_from_vsi(seq_info.output_format);
        info->frameOnlyFlag = seq_info.is_interlaced ? 0 : 1;
        info->requiredBufNum = seq_info.num_of_ref_frames + 1;
        info->reorderNum = seq_info.num_of_ref_frames;

        LOG_INFO(DEC, "%s stream: %ux%u, crop %ux%u, required buffers %u", codec_name(codec),
                 info->width, info->height, info->cropWidth, info->cropHeight, info->requiredBufNum);
    }

    DWLFreeLinear(dwl, &stream_mem);
    VCDecRelease(inst);
    DWLRelease(dwl);

    return ret;
}

uint32_t video_get_idle_dpb_count(struct va_dec_channel *chn)
{
    struct video_decoder_private_context *ctx = NULL;
    uint32_t cnt = 0;
    uint32_t i;

    if (!chn || !chn->codec_inst)
        return 0;

    ctx = (struct video_decoder_private_context *)chn->private_context;
    if (!ctx)
        return 0;

    pthread_mutex_lock(&ctx->buffer_mutex);
    for (i = 0; i < MAX_PIC_BUFFERS; i++) {
        if (ctx->ext_buffers[i].bus_address != 0 && ctx->buffer_consumed[i] == 1)
            cnt++;
    }
    pthread_mutex_unlock(&ctx->buffer_mutex);

    LOG_DEBUG(DEC, "Idle DPB buffers: %u/%u", cnt, ctx->ext_buffer_number);

    return cnt;
}
