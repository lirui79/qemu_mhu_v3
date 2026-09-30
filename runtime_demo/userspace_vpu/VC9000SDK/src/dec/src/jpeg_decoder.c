/*
 * Copyright (c) 2022, Vastai Tech. All rights reserved
 *
 * The information contained herein is confidential
 * property of Company. The user, copying, transfer or
 * disclosure of such information is prohibited except
 * by express written agreement with VASTAITECH.
 */

#include "jpeg_decoder.h"

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

/* Default picture alignment used for the output buffers. */
#define JPEG_DEFAULT_ALIGN DEC_ALIGN_128B

/* The decoder only programs the real stream length into HW once it has reported
 * the input buffer as empty, so the very first VCDecDecode() call of a picture
 * ends with DEC_STRM_PROCESSED without any decoded picture. The hardware has to
 * be kicked again with the same input (this is what the reference testbench
 * does) until a picture is ready. Bounded retries keep a truncated stream from
 * spinning forever. */
#define JPEG_STRM_PROCESSED_RETRY_MAX 3

static vmppResult result_from_vsi(enum DecRet ret)
{
    switch (ret) {
    case DEC_OK:
    case DEC_PIC_RDY:
    case DEC_PIC_DECODED:
    case DEC_HDRS_RDY:
    case DEC_DP_HDRS_RDY:
    case DEC_END_OF_SEQ:
    case DEC_PARAM_SET_PARSED:
    case DEC_SEI_PARSED:
    case DEC_ADVANCED_TOOLS:
    case DEC_NONREF_PIC_SKIPPED:
    case DEC_PB_PIC_SKIPPED:
    case DEC_FLUSHED:
    case DEC_VOS_END:
    case DEC_RESOLUTION_CHANGED:
    case DEC_DISCARD_INTERNAL:
        return vmpp_RSLT_OK;
    case DEC_END_OF_STREAM:
        return vmpp_RSLT_WARN_EOS;
    case DEC_STRM_PROCESSED:
    case DEC_SCAN_PROCESSED:
    case DEC_BUF_EMPTY:
        return vmpp_RSLT_WARN_MORE_DATA;
    case DEC_ABORTED:
        return vmpp_RSLT_WARN_ABORTED;
    case DEC_SLICE_RDY:
    case DEC_PENDING_FLUSH:
        return vmpp_RSLT_DEC_INPUT_AGAIN;
    case DEC_WAITING_FOR_BUFFER:
    case DEC_NO_DECODING_BUFFER:
        return vmpp_RSLT_ERR_NO_BUFFER;
    case DEC_MEMFAIL:
        return vmpp_RSLT_ERR_NO_MEMORY;
    case DEC_INITFAIL:
        return vmpp_RSLT_ERR_DEC_INIT;
    case DEC_NOT_INITIALIZED:
        return vmpp_RSLT_ERR_NOT_INITIALIZED;
    case DEC_PARAM_ERROR:
    case DEC_INFOPARAM_ERROR:
    case DEC_EXT_BUFFER_REJECTED:
    case DEC_INCREASE_INPUT_BUFFER:
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    case DEC_HDRS_NOT_RDY:
        return vmpp_RSLT_ERR_GET_INFO;
    case DEC_STRM_ERROR:
    case DEC_INVALID_STREAM_LENGTH:
    case DEC_INVALID_INPUT_BUFFER_SIZE:
    case DEC_STREAM_ERROR_DEDECTED:
    case DEC_METADATA_FAIL:
    case DEC_PARAM_SET_ERROR:
    case DEC_SEI_ERROR:
    case DEC_SLICE_HDR_ERROR:
    case DEC_REF_PICS_ERROR:
    case DEC_NALUNIT_ERROR:
    case DEC_AU_BOUNDARY_ERROR:
    case DEC_NO_REFERENCE:
    case DEC_ERROR:
        return vmpp_RSLT_ERR_INVALID_DATA;
    case DEC_STREAM_NOT_SUPPORTED:
    case DEC_UNSUPPORTED:
    case DEC_FORMAT_NOT_SUPPORTED:
    case DEC_SLICE_MODE_UNSUPPORTED:
        return vmpp_RSLT_ERR_UNSUPPORTED;
    case DEC_HW_RESERVED:
    case DEC_HW_BUS_ERROR:
        return vmpp_RSLT_ERR_DEC_BUS;
    case DEC_HW_TIMEOUT:
    case DEC_HW_EXT_TIMEOUT:
    case DEC_DWL_ERROR:
        return vmpp_RSLT_ERR_DEC_DWL;
    case DEC_SYSTEM_ERROR:
    case DEC_FATAL_SYSTEM_ERROR:
        return vmpp_RSLT_ERR_SYS_ERROR;
    case DEC_EVALUATION_LIMIT_EXCEEDED:
        return vmpp_RSLT_ERR_INVALID_STATE;
    default:
        break;
    }
    return vmpp_RSLT_ERR_UNKNOWN;
}

static vmppChromaFormat chroma_fmt_from_vsi(enum DecPictureFormat fmt)
{
    switch (fmt) {
    case DEC_OUT_FRM_MONOCHROME:
    case DEC_OUT_FRM_YUV400:
        return vmpp_CHROMA_FMT_400;
    case DEC_OUT_FRM_RASTER_SCAN:
    case DEC_OUT_FRM_YUV420SP:
    case DEC_OUT_FRM_PLANAR_420:
    case DEC_OUT_FRM_YUV420P:
    case DEC_OUT_FRM_NV21SP:
    case DEC_OUT_FRM_NV21P:
        return vmpp_CHROMA_FMT_420;
    case DEC_OUT_FRM_YUV411SP:
        return vmpp_CHROMA_FMT_411;
    case DEC_OUT_FRM_YUV422SP:
    case DEC_OUT_FRM_YUV422P:
        return vmpp_CHROMA_FMT_422;
    case DEC_OUT_FRM_YUV440:
        return vmpp_CHROMA_FMT_440;
    case DEC_OUT_FRM_YUV444SP:
    case DEC_OUT_FRM_YUV444P:
        return vmpp_CHROMA_FMT_444;
    default:
        break;
    }
    return vmpp_CHROMA_FMT_NONE;
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
    return JPEG_DEFAULT_ALIGN;
}

static void ReleaseExtBuffers(const void *dwl, struct jpeg_decoder_private_context *ctx)
{
    uint32_t i;

    for (i = 0; i < JPEG_MAX_BUFFERS; i++) {
        if (ctx->ext_buffers[i].bus_address == 0)
            continue;

        /* The external buffers of vmpp_DEC_MEM_USER_AS_HWOUT mode are only
         * placeholders, they are never really allocated. */
        if (ctx->ext_buffers[i].alloc_bus_addr != 0)
            DWLFreeLinear(dwl, &ctx->ext_buffers[i]);
        DWLmemset(&ctx->ext_buffers[i], 0, sizeof(ctx->ext_buffers[i]));
    }
    ctx->ext_buffers_number = 0;
}

void stream_buffer_comsumed(void *stream, void *p_user_data)
{
    // TODO:
    UNUSED_PARAMETER(stream);
    UNUSED_PARAMETER(p_user_data);
}

vmppResult jpeg_decoder_create_chn(struct va_dec_channel *chn)
{
    if (!chn) {
        LOG_ERROR(DEC, "Invalid parameters: chn %p", chn);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    struct jpeg_decoder_private_context *ctx =
        malloc(sizeof(struct jpeg_decoder_private_context));
    if (!ctx) {
        LOG_ERROR(DEC, "Fail to malloc private context for JPEG decoder.");
        return vmpp_RSLT_ERR_NO_MEMORY;
    }
    memset(ctx, 0, sizeof(struct jpeg_decoder_private_context));
    chn->private_context = ctx;

    ctx->pp_enabled = 1;
    ctx->ppu_cfg[0].enabled = 1;
    ctx->guard_size = VA_MIN(JPEG_MAX_BUFFERS, VA_MAX(chn->params.extraBufferNumber, 1));

    struct DWLInitParam dwl_init = {0};
    dwl_init.client_type = DWL_CLIENT_TYPE_JPEG_DEC;
    dwl_init.dec_dev     = chn->params.decDevice;
    dwl_init.mem_dev     = chn->params.memDevice;

    const void *dwl = DWLInit(&dwl_init);
    if (dwl == NULL) {
        LOG_ERROR(DEC, "DWLInit# ERROR: DWL Init failed.");
        free(ctx);
        chn->private_context = NULL;
        return vmpp_RSLT_ERR_DEC_DWL;
    }
    chn->cwl = dwl;
    chn->frame_struct_size = sizeof(struct DecPictures);

    uint32_t core_mask = 0;
    DWLGetHwFeaturesByClientType(dwl, DWL_CLIENT_TYPE_JPEG_DEC, &core_mask);
    if (core_mask == 0) {
        LOG_ERROR(DEC, "JPEG is not supported by this hardware.");
        DWLRelease(dwl);
        chn->cwl = NULL;
        free(ctx);
        chn->private_context = NULL;
        return vmpp_RSLT_ERR_UNSUPPORTED;
    }

    struct DecSwHwBuild dec_build = VCDecGetBuild(dwl, DWL_CLIENT_TYPE_JPEG_DEC);
    LOG_DEBUG(DEC, "JPEG decoder SW build %u, ASIC id 0x%x.", dec_build.sw_build,
              dec_build.asic_id);

    struct DecInitConfig init_config = {0};
    init_config.codec = DEC_JPEG;
    init_config.decoder_mode = DEC_NORMAL;
    init_config.error_handling = DEC_EC_FRAME_NO_ERROR;
    init_config.dwl_inst = dwl;
    init_config.guard_size = ctx->guard_size;
    init_config.use_adaptive_buffers = 1;
    init_config.mc_cfg.mc_enable = 0;
    init_config.mc_cfg.stream_consumed_callback = stream_buffer_comsumed;

    enum DecRet dec_ret = VCDecInit((const void **)&chn->codec_inst, &init_config);
    if (dec_ret != DEC_OK) {
        LOG_ERROR(DEC, "JPEG decoder init failed, ret %d.", dec_ret);
        DWLRelease(chn->cwl);
        chn->cwl = NULL;
        free(ctx);
        chn->private_context = NULL;
        return vmpp_RSLT_ERR_DEC_INIT;
    }

    return vmpp_RSLT_OK;
}

vmppResult jpeg_decoder_destory_chn(struct va_dec_channel *chn)
{
    if (!chn)
        return vmpp_RSLT_ERR_INVALID_PARAMS;

    if (chn->private_context != NULL) {
        struct jpeg_decoder_private_context *private_ctx =
            (struct jpeg_decoder_private_context *)chn->private_context;
        ReleaseExtBuffers(chn->cwl, private_ctx);
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

    LOG_DEBUG(DEC, "JPEG decoder instance destroy.");
    return vmpp_RSLT_OK;
}

static vmppResult allocate_buffer(struct va_dec_channel *chn, int waiting_for_buffer)
{
    struct DecBufferInfo hbuf = {0};
    uint32_t new_buf_count;
    uint32_t i;

    void *inst = chn->codec_inst;
    struct jpeg_decoder_private_context *ctx =
        (struct jpeg_decoder_private_context *)chn->private_context;

    enum DecRet dec_ret = VCDecGetBufferInfo(inst, &hbuf);
    if (dec_ret != DEC_WAITING_FOR_BUFFER && dec_ret != DEC_OK) {
        LOG_ERROR(DEC, "VCDecGetBufferInfo failed: %d", dec_ret);
        return result_from_vsi(dec_ret);
    }

    if (hbuf.buf_to_free.bus_address != 0) {
        for (i = 0; i < JPEG_MAX_BUFFERS; i++) {
            if (ctx->ext_buffers[i].bus_address == hbuf.buf_to_free.bus_address) {
                if (ctx->ext_buffers[i].alloc_bus_addr != 0)
                    DWLFreeLinear(chn->cwl, &ctx->ext_buffers[i]);
                DWLmemset(&ctx->ext_buffers[i], 0, sizeof(ctx->ext_buffers[i]));
                ctx->ext_buffers_number--;
                break;
            }
        }
    }

    if (!hbuf.next_buf_size)
        return vmpp_RSLT_OK;
    if (ctx->ext_buffers_number >= ctx->guard_size)
        return waiting_for_buffer ? vmpp_RSLT_ERR_NO_BUFFER : vmpp_RSLT_OK;

    new_buf_count = VA_MIN(ctx->guard_size - ctx->ext_buffers_number, hbuf.buf_num);
    for (i = 0; i < new_buf_count; i++) {
        struct DWLLinearMem mem = {0};
        mem.size = hbuf.next_buf_size;
        mem.logical_size = hbuf.next_buf_size;

        if (chn->params.memoryMode == vmpp_DEC_MEM_USER_AS_HWOUT) {
            /* For this mode the output buffer is provided by user when sending
             * the stream, only a placeholder is recorded here. */
            mem.virtual_address = NULL;
            mem.bus_address = i + 1;
        } else {
            mem.mem_type = DWL_MEM_TYPE_DPB;
            SET_MEM_USAGE(mem.mem_type,
                          ctx->pp_enabled ? DWL_MEM_USAGE_OUT_PP : DWL_MEM_USAGE_OUT_REFERENCE, 0);
            if (DWLMallocLinear(chn->cwl, hbuf.next_buf_size, &mem)) {
                LOG_ERROR(DEC, "Malloc buffer failed.");
                return vmpp_RSLT_ERR_NO_BUFFER;
            }
        }

        dec_ret = VCDecAddBuffer(inst, &mem);
        /* NOTE: DEC_WAITING_FOR_BUFFER means the buffer has been accepted but the decoder is
                 still waiting for more buffers. */
        if (dec_ret != DEC_OK && dec_ret != DEC_WAITING_FOR_BUFFER) {
            LOG_WARN(DEC, "VCDecAddBuffer rejected: %s(%d)", VCDecRetStr(dec_ret), dec_ret);
            if (mem.alloc_bus_addr != 0)
                DWLFreeLinear(chn->cwl, &mem);
            return result_from_vsi(dec_ret);
        }
        ctx->ext_buffers[ctx->ext_buffers_number++] = mem;
    }

    return vmpp_RSLT_OK;
}

static void config_pp_params(struct va_dec_channel *chn, struct DecSequenceInfo *image_info,
                             struct DecConfig *dec_cfg)
{
    struct jpeg_decoder_private_context *ctx =
        (struct jpeg_decoder_private_context *)chn->private_context;
    PpUnitConfig *ppu_cfg = &ctx->ppu_cfg[0];
    uint32_t mode = image_info->jpeg_input_info.dec_image_type == JPEGDEC_THUMBNAIL ? 1 : 0;
    uint32_t display_width = (image_info->scaled_width + 1) & ~0x1;
    uint32_t display_height = (image_info->scaled_height + 1) & ~0x1;
    uint32_t display_width_thumb = (image_info->scaled_width_thumb + 1) & ~0x1;
    uint32_t display_height_thumb = (image_info->scaled_height_thumb + 1) & ~0x1;
    uint32_t crop_w, crop_h;

    if (chn->params.cropInfo.flag) {
        ppu_cfg->crop.enabled = 1;
        ppu_cfg->crop.set_by_user = 1;
        if (chn->params.cropInfo.flag == vmpp_CROP_CUSTOMIZED) {
            ppu_cfg->crop.x = chn->params.cropInfo.xOffset;
            ppu_cfg->crop.y = chn->params.cropInfo.yOffset;
            ppu_cfg->crop.width = chn->params.cropInfo.width;
            ppu_cfg->crop.height = chn->params.cropInfo.height;
        } else {
            ppu_cfg->crop.x = 0;
            ppu_cfg->crop.y = 0;
            ppu_cfg->crop.width = display_width;
            ppu_cfg->crop.height = display_height;
        }
        LOG_DEBUG(DEC, "Crop Info: flag 0x%x, [%d, %d, %dx%d]", chn->params.cropInfo.flag,
                  ppu_cfg->crop.x, ppu_cfg->crop.y, ppu_cfg->crop.width, ppu_cfg->crop.height);
        crop_w = ppu_cfg->crop.width;
        crop_h = ppu_cfg->crop.height;
    } else if (mode) {
        /* The thumbnail image is scaled to the thumbnail size only. */
        ppu_cfg->crop.enabled = 0;
        ppu_cfg->crop.set_by_user = 0;
        crop_w = display_width_thumb;
        crop_h = display_height_thumb;
    } else {
        ppu_cfg->crop.enabled = 1;
        ppu_cfg->crop.set_by_user = 0;
        ppu_cfg->crop.x = 0;
        ppu_cfg->crop.y = 0;
        crop_w = display_width;
        crop_h = display_height;
    }

    /* No real down scaling here, PP only outputs the cropped region. */
    ppu_cfg->scale.enabled = 1;
    ppu_cfg->scale.width = NEXT_MULTIPLE(crop_w - 1, 2);
    ppu_cfg->scale.height = NEXT_MULTIPLE(crop_h - 1, 2);
    ppu_cfg->enabled = 1;
    ppu_cfg->align = dec_cfg->align;
    ppu_cfg->align_h = dec_cfg->align;
    ctx->pp_enabled = 1;

    memcpy(dec_cfg->ppu_cfg, ctx->ppu_cfg, sizeof(ctx->ppu_cfg));
    dec_cfg->dec_image_type = image_info->jpeg_input_info.dec_image_type;
    dec_cfg->chroma_format = mode ? image_info->output_format_thumb : image_info->output_format;
}

vmppResult jpeg_decoder_send_stream(struct va_dec_channel *chn, vmppStream *stream,
                                    uint32_t timeout)
{
    vmppResult ret = vmpp_RSLT_OK;
    struct DecInputParameters jpeg_in = {0};
    struct DecSequenceInfo image_info = {0};
    struct DecConfig dec_cfg = {0};
    struct DecOutput jpeg_out = {0};
    struct DWLLinearMem stream_mem = {0};
    enum DecRet dec_ret = DEC_OK;
    uint32_t again;
    uint32_t strm_retry = 0;
    uint64_t start, tick;

    void *inst = chn->codec_inst;
    struct jpeg_decoder_private_context *ctx =
        (struct jpeg_decoder_private_context *)chn->private_context;
    if (!inst || !ctx || !stream || !stream->stream || stream->len == 0) {
        LOG_ERROR(DEC, "Invalid decoder parameters: JPEG inst %p, private context %p", inst, ctx);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    stream_mem.mem_type = DWL_MEM_TYPE_CPU;
#ifdef SUPPORT_DMA
    stream_mem.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
#endif
    SET_MEM_USAGE(stream_mem.mem_type, DWL_MEM_USAGE_IN_STRM, 0);
    if (DWLMallocLinear(chn->cwl, stream->len, &stream_mem) != DWL_OK) {
        LOG_ERROR(DEC, "Failed to allocate JPEG input buffer of %u bytes", stream->len);
        return vmpp_RSLT_ERR_NO_MEMORY;
    }
    DWLmemcpy(stream_mem.virtual_address, stream->stream, stream->len);

    jpeg_in.stream_buffer = stream_mem;
    jpeg_in.stream = (uint8_t *)stream_mem.virtual_address;
    jpeg_in.strm_len = stream->len;
    jpeg_in.pic_id = (uint32_t)stream->pts;
    jpeg_in.dec_image_type = JPEGDEC_IMAGE;

    image_info.jpeg_input_info = jpeg_in;
    dec_ret = VCDecGetInfo(inst, &image_info);
    if (dec_ret != DEC_OK) {
        LOG_ERROR(DEC, "VCDecGetInfo failed: %d", dec_ret);
        DWLFreeLinear(chn->cwl, &stream_mem);
        return vmpp_RSLT_ERR_INVALID_DATA;
    }

    LOG_DEBUG(DEC, "JPEG image: %dx%d, scaled: %dx%d, coding mode %u", image_info.pic_width,
              image_info.pic_height, image_info.scaled_width, image_info.scaled_height,
              image_info.coding_mode);

    dec_cfg.align = get_pic_align(chn->params.outputAlign);
    image_info.pic_width = NEXT_MULTIPLE(image_info.pic_width, ALIGN(dec_cfg.align));
    image_info.pic_width_thumb = NEXT_MULTIPLE(image_info.pic_width_thumb, ALIGN(dec_cfg.align));

    config_pp_params(chn, &image_info, &dec_cfg);

    LOG_DEBUG(DEC, "JPEG decoder SetInfo: align %d, type %d", dec_cfg.align, dec_cfg.dec_image_type);
    dec_ret = VCDecSetInfo(inst, &dec_cfg);
    if (dec_ret != DEC_OK) {
        LOG_ERROR(DEC, "VCDecSetInfo failed: %d", dec_ret);
        DWLFreeLinear(chn->cwl, &stream_mem);
        return dec_ret == DEC_MEMFAIL ? vmpp_RSLT_ERR_NO_MEMORY : vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    if ((jpeg_in.dec_image_type == JPEGDEC_THUMBNAIL &&
         image_info.coding_mode_thumb == JPEG_PROGRESSIVE) ||
        (jpeg_in.dec_image_type == JPEGDEC_IMAGE && image_info.coding_mode == JPEG_PROGRESSIVE))
        jpeg_in.slice_mb_set = 0;

    memcpy(&(ctx->image_info), &image_info, sizeof(ctx->image_info));

    vmppResult alloc_ret = allocate_buffer(chn, 0);
    if (alloc_ret != vmpp_RSLT_OK)
        LOG_WARN(DEC, "allocate_buffer: %d", alloc_ret);

    if (chn->params.memoryMode == vmpp_DEC_MEM_USER_AS_HWOUT) {
        /* Install the user provided hardware buffer as the output of this picture. */
        jpeg_in.picture_buffer_y.mem_type = DWL_MEM_TYPE_DPB;
        jpeg_in.picture_buffer_y.bus_address = (addr_t)stream->outputBusAddress;
        jpeg_in.picture_buffer_y.virtual_address = NULL;
        jpeg_in.picture_buffer_y.logical_size =
            ctx->image_info.pic_width * ctx->image_info.pic_height * 3 / 2;
        jpeg_in.picture_buffer_y.size = jpeg_in.picture_buffer_y.logical_size;
        LOG_DEBUG(DEC, "User output bus address 0x%llx, size 0x%llx",
                  (unsigned long long)jpeg_in.picture_buffer_y.bus_address,
                  (unsigned long long)jpeg_in.picture_buffer_y.size);
    }

    start = va_gettime_ns();
    again = 1;
    do {
        dec_ret = VCDecDecode(inst, &jpeg_out, &jpeg_in);
        switch (dec_ret) {
        case DEC_PIC_RDY:
        case DEC_PIC_DECODED:
            again = 0;
            ret = vmpp_RSLT_OK;
            break;
        case DEC_STRM_PROCESSED:
        case DEC_SCAN_PROCESSED:
            /* Not a real end of the picture: feed the same input again so the
             * hardware restarts with the stream length programmed. */
            if (++strm_retry > JPEG_STRM_PROCESSED_RETRY_MAX) {
                LOG_WARN(DEC, "VCDecDecode keeps returning %s(%d), no picture decoded",
                         VCDecRetStr(dec_ret), dec_ret);
                ret = vmpp_RSLT_WARN_MORE_DATA;
                again = 0;
            } else {
                LOG_DEBUG(DEC, "VCDecDecode returns %s(%d), kick HW again (retry %u)",
                          VCDecRetStr(dec_ret), dec_ret, strm_retry);
            }
            break;
        case DEC_SLICE_RDY:
        case DEC_PENDING_FLUSH:
            break;
        case DEC_WAITING_FOR_BUFFER:
            alloc_ret = allocate_buffer(chn, 1);
            if (alloc_ret != vmpp_RSLT_OK)
                LOG_WARN(DEC, "allocate_buffer: %d", alloc_ret);
            break;
        case DEC_NO_DECODING_BUFFER:
            tick = va_gettime_ns();
            if ((float)(tick - start) / 1000000.0 > timeout) {
                LOG_WARN(DEC, "Timeout for VCDecDecode: %s", STRING(DEC_NO_DECODING_BUFFER));
                ret = vmpp_RSLT_ERR_NO_BUFFER;
                again = 0;
                break;
            }
            usleep(1000);
            break;
        default:
            LOG_ERROR(DEC, "VCDecDecode return error: %d", dec_ret);
            ret = result_from_vsi(dec_ret);
            again = 0;
            break;
        }

        if (again) {
            tick = va_gettime_ns();
            if ((float)(tick - start) / 1000000.0 > timeout) {
                LOG_WARN(DEC, "Timeout(%u ms) for VCDecDecode: %d", timeout, dec_ret);
                ret = vmpp_RSLT_ERR_NO_BUFFER;
                again = 0;
            }
        }
    } while (again);

    DWLFreeLinear(chn->cwl, &stream_mem);
    return ret;
}

vmppResult jpeg_decoder_receive_frame(struct va_dec_channel *chn, vmppFrame *frame,
                                      vmppDecOutputOptions *out_opt)
{
    uint32_t size;
    uint32_t output_height;
    vmppResult ret;

    if (!chn || !frame || !out_opt) {
        LOG_ERROR(DEC, "Invalid parameters");
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    struct jpeg_decoder_private_context *ctx =
        (struct jpeg_decoder_private_context *)chn->private_context;
    if (!ctx) {
        LOG_ERROR(DEC, "JPEG private context null.");
        return vmpp_RSLT_ERR_NOT_INITIALIZED;
    }

    struct DecPictures *dec_picture = (struct DecPictures *)frame->privateData;
    if (!dec_picture) {
        LOG_ERROR(DEC, "Invalid private data of frame %p", frame);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    enum DecRet dec_ret = VCDecNextPicture(chn->codec_inst, dec_picture);
    if (dec_ret == DEC_PIC_RDY) {
        struct DecPicture *pic = &dec_picture->pictures[0];
        output_height =
            chn->params.cropInfo.flag == vmpp_CROP_CUSTOMIZED
                ? chn->params.cropInfo.height
                : (chn->params.cropInfo.flag ? ctx->image_info.scaled_height : pic->pic_height);
        /* (output_height + 1) & ~0x1 for odd size */
        output_height = (output_height + 1) & ~0x1;
        size = IS_PIC_MONOCHROME(ctx->image_info.output_format)
                   ? pic->pic_stride * output_height
                   : pic->pic_stride * output_height * 3 / 2;
        frame->dataSize = size;
        switch (chn->params.memoryMode) {
        case vmpp_DEC_MEM_USER_OUT_BUF_HOST:
        case vmpp_DEC_MEM_LESS_DEV_MEM:
        case vmpp_DEC_MEM_USER_OUT_BUF_DEV:
            LOG_ERROR(DEC, "memory mode %d is not supported yet!", chn->params.memoryMode);
            return vmpp_RSLT_ERR_UNSUPPORTED;
        default:
            if (chn->params.memoryMode != vmpp_DEC_MEM_USER_AS_HWOUT &&
                out_opt->memoryType == vmpp_MEM_HOST) {
                frame->data[0] = (uint8_t *)pic->luma.virtual_address;
                frame->data[1] = (uint8_t *)pic->chroma.virtual_address;
                frame->busAddress[0] = (vmppDevAddr)NULL;
                frame->busAddress[1] = (vmppDevAddr)NULL;
                frame->memoryType = vmpp_MEM_HOST;
            } else {
                frame->data[0] = NULL;
                frame->data[1] = NULL;
                frame->memoryType = vmpp_MEM_DEVICE;
            }
            break;
        }
        frame->busAddress[0] = (vmppDevAddr)(uintptr_t)pic->luma.bus_address;
        frame->busAddress[1] = (vmppDevAddr)(uintptr_t)pic->chroma.bus_address;
        frame->pixelFormat = format_from_vsi(ctx->image_info.output_format);
        frame->width = pic->pic_width;
        frame->height = output_height;
        frame->stride[0] = pic->pic_stride;
        frame->stride[1] = pic->pic_stride_ch;

        LOG_DEBUG(DEC,
                  "CropFlag 0x%x, cropInfo[%d,%d,%dx%d], pic[%dx%d], display[%dx%d], "
                  "stride[%dx%d], dataSize %d",
                  chn->params.cropInfo.flag, chn->params.cropInfo.xOffset,
                  chn->params.cropInfo.yOffset, chn->params.cropInfo.width,
                  chn->params.cropInfo.height, pic->pic_width, pic->pic_height,
                  ctx->image_info.scaled_width, ctx->image_info.scaled_height, pic->pic_stride,
                  pic->pic_stride_ch, frame->dataSize);

        if (chn->params.cropInfo.flag == vmpp_CROP_CUSTOMIZED) {
            frame->cropInfo.flag = 0;
            frame->cropInfo.width = chn->params.cropInfo.width;
            frame->cropInfo.height = chn->params.cropInfo.height;
            frame->cropInfo.xOffset = 0;
            frame->cropInfo.yOffset = 0;
        } else {
            if (chn->params.cropInfo.flag) {
                frame->cropInfo.flag = 0;
            } else if ((pic->pic_width != ctx->image_info.scaled_width) ||
                       (pic->pic_height != ctx->image_info.scaled_height)) {
                frame->cropInfo.flag = 1;
            } else {
                frame->cropInfo.flag = 0;
            }

            frame->cropInfo.width = ctx->image_info.scaled_width;
            frame->cropInfo.height = ctx->image_info.scaled_height;
            frame->cropInfo.xOffset = 0;
            frame->cropInfo.yOffset = 0;
        }
        frame->pts = pic->picture_info.pic_id;

        ret = vmpp_RSLT_OK;
    } else if (dec_ret == DEC_END_OF_STREAM) {
        LOG_INFO(DEC, "VCDecNextPicture return DEC_END_OF_STREAM: %d", dec_ret);
        ret = vmpp_RSLT_WARN_EOS;
    } else if (dec_ret == DEC_PARAM_ERROR) {
        LOG_WARN(DEC, "VCDecNextPicture return DEC_PARAM_ERROR: %d", dec_ret);
        ret = vmpp_RSLT_ERR_INVALID_PARAMS;
    } else if (dec_ret == DEC_ABORTED) {
        LOG_WARN(DEC, "VCDecNextPicture return DEC_ABORTED: %d", dec_ret);
        ret = vmpp_RSLT_ERR_DEC_ABORTED;
    } else {
        LOG_INFO(DEC, "VCDecNextPicture return: %d, treated as again", dec_ret);
        ret = vmpp_RSLT_WARN_MORE_DATA;
    }

    return ret;
}

vmppResult jpeg_decoder_transfer_frame(struct va_dec_channel *chn, vmppFrame *frame)
{
    if (!chn || !chn->codec_inst || !frame || !frame->privateData) {
        LOG_ERROR(DEC, "Invalid parameters.");
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    struct jpeg_decoder_private_context *ctx =
        (struct jpeg_decoder_private_context *)chn->private_context;
    if (!ctx) {
        LOG_ERROR(DEC, "JPEG private context null.");
        return vmpp_RSLT_ERR_NOT_INITIALIZED;
    }

    struct DecPictures *dec_picture = (struct DecPictures *)frame->privateData;
    struct DecPicture *pic = &dec_picture->pictures[0];

    frame->data[0] = (uint8_t *)pic->luma.virtual_address;
    frame->data[1] = (uint8_t *)pic->chroma.virtual_address;
    frame->memoryType = vmpp_MEM_HOST;

    return vmpp_RSLT_OK;
}

vmppResult jpeg_decoder_release_frame(struct va_dec_channel *chn, vmppFrame *frame)
{
    vmppResult ret;
    struct DecPictures *dec_picture;

    if (!chn || !chn->codec_inst || !frame || !frame->privateData) {
        LOG_ERROR(DEC, "Invalid parameters.");
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    dec_picture = (struct DecPictures *)frame->privateData;
    enum DecRet dec_ret = VCDecPictureConsumed(chn->codec_inst, dec_picture);
    if (dec_ret == DEC_PARAM_ERROR) {
        LOG_ERROR(DEC, "VCDecPictureConsumed failed: %d", dec_ret);
        ret = vmpp_RSLT_ERR_INVALID_PARAMS;
    } else {
        ret = vmpp_RSLT_OK;
    }
    return ret;
}

vmppResult jpeg_decoder_get_stream_info(struct va_dec_channel *chn, vmppDecStreamInfo *info)
{
    if (!chn || !chn->codec_inst || !info) {
        LOG_ERROR(DEC, "Invalid parameters: chn %p, codec_inst %p, info %p", chn, chn->codec_inst,
                  info);
        return vmpp_RSLT_ERR_NOT_INITIALIZED;
    }

    struct jpeg_decoder_private_context *ctx =
        (struct jpeg_decoder_private_context *)chn->private_context;
    if (!ctx) {
        LOG_ERROR(DEC, "JPEG private context null.");
        return vmpp_RSLT_ERR_NOT_INITIALIZED;
    }

    LOG_INFO(DEC, "scaled:%dx%d, output:%dx%d", ctx->image_info.scaled_width,
             ctx->image_info.scaled_height, ctx->image_info.pic_width,
             ctx->image_info.pic_height);

    info->width = ctx->image_info.scaled_width;
    info->height = ctx->image_info.scaled_height;

    return vmpp_RSLT_OK;
}

vmppResult jpeg_decoder_get_jpeg_info(vmppStream *stream, vmppDecJpegInfo *info)
{
    if (!stream || !stream->stream || stream->len == 0 || !info) {
        LOG_ERROR(DEC, "Invalid parameters: stream %p, info %p", stream, info);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    struct DWLInitParam dwl_init = {0};
    dwl_init.client_type = DWL_CLIENT_TYPE_JPEG_DEC;
    dwl_init.dec_dev     = "/dev/hantrodec";
    dwl_init.mem_dev     = "/dev/memalloc";

    /* The information is parsed with a temporary instance, no DWL is created for
     * the channel itself. */
    const void *dwl = DWLInit(&dwl_init);
    if (dwl == NULL) {
        LOG_ERROR(DEC, "Fail to init DWL for JPEG header parsing.");
        return vmpp_RSLT_ERR_DEC_DWL;
    }

    struct DecInitConfig init_config = {0};
    init_config.codec = DEC_JPEG;
    init_config.decoder_mode = DEC_NORMAL;
    init_config.error_handling = DEC_EC_FRAME_NO_ERROR;
    init_config.dwl_inst = dwl;
    init_config.use_adaptive_buffers = 1;

    void *inst = NULL;
    vmppResult ret = vmpp_RSLT_OK;
    if (VCDecInit((const void **)&inst, &init_config) != DEC_OK) {
        LOG_ERROR(DEC, "Fail to init JPEG decoder instance for header parsing.");
        VCDecRelease(inst);
        DWLRelease(dwl);
        return vmpp_RSLT_ERR_DEC_INIT;
    }

    struct DecInputParameters jpeg_in = {0};
    jpeg_in.stream_buffer.virtual_address = (u32 *)stream->stream;
    jpeg_in.stream_buffer.logical_size = stream->len;
    jpeg_in.stream = (u8 *)stream->stream;
    jpeg_in.strm_len = stream->len;
    jpeg_in.dec_image_type = JPEGDEC_IMAGE;

    struct DecSequenceInfo image_info = {0};
    image_info.jpeg_input_info = jpeg_in;

    enum DecRet dec_ret = VCDecGetInfo(inst, &image_info);
    if (dec_ret != DEC_OK) {
        LOG_ERROR(DEC, "VCDecGetInfo failed: %d", dec_ret);
        ret = vmpp_RSLT_ERR_INVALID_DATA;
    } else {
        info->width = image_info.scaled_width;
        info->height = image_info.scaled_height;
        info->xDensity = image_info.exif_info.ifd0_info.XResolution;
        info->yDensity = image_info.exif_info.ifd0_info.YResolution;
        info->outputFormat = chroma_fmt_from_vsi(image_info.output_format);
        switch (image_info.coding_mode) {
        case JPEG_BASELINE:
            info->codingMode = vmpp_JPEG_BASELINE;
            break;
        case JPEG_PROGRESSIVE:
            info->codingMode = vmpp_JPEG_PROGRESSIVE;
            break;
        case JPEG_NONINTERLEAVED:
            info->codingMode = vmpp_JPEG_NONINTERLEAVED;
            break;
        default:
            info->codingMode = vmpp_JPEG_NONE;
            break;
        }
        ret = vmpp_RSLT_OK;
    }

    VCDecRelease(inst);
    DWLRelease(dwl);

    return ret;
}

vmppResult jpeg_decoder_end_of_stream(struct va_dec_channel *chn)
{
    if (!chn || !chn->codec_inst) {
        LOG_ERROR(DEC, "Invalid JPEG decoder instance.");
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    VCDecEndOfStream(chn->codec_inst);
    return vmpp_RSLT_OK;
}
