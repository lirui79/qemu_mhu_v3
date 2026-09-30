/*
 * Copyright (c) 2022, Vastai Tech. All rights reserved
 *
 * The information contained herein is confidential
 * property of Company. The user, copying, transfer or
 * disclosure of such information is prohibited except
 * by express written agreement with VASTAITECH.
 */

#ifndef __VIDEO_DECODER_H__
#define __VIDEO_DECODER_H__

#include "va_vdata_internal.h"
#include "vmpp_dec_defs.h"

// vsi headers
#include "basetype.h"
#include "dectypes.h"
#include "dwl.h"

struct video_decoder_private_context {
    enum DecCodec codec;
    uint32_t pic_decode_number;
    uint32_t headers_ready;
    uint32_t min_buffer_number;
    uint32_t guard_size;
    uint32_t pp_enabled;
    uint32_t prev_width;
    uint32_t prev_height;
    uint32_t prev_buf_width;
    uint32_t prev_buf_height;
    uint32_t buffer_size;
    uint32_t ext_buffer_number;
    PpUnitConfig ppu_cfg[DEC_MAX_PPU_COUNT];
    struct DecConfig dec_cfg;
    struct DecSequenceInfo dec_info;
    struct DWLLinearMem ext_buffers[MAX_PIC_BUFFERS];
    volatile uint32_t buffer_consumed[MAX_PIC_BUFFERS];
    pthread_mutex_t buffer_mutex;
    uint32_t resolution_changed;
    /* Number of consecutive times the same stream buffer had to be sent again. */
    uint32_t input_again_number;
};

vmppResult video_decoder_create_chn(struct va_dec_channel *chn);

vmppResult video_decoder_destory_chn(struct va_dec_channel *chn);

vmppResult video_decoder_send_stream(struct va_dec_channel *chn, vmppStream *stream,
                                    uint32_t timeout, vaSendStreamCb send_cb, uint8_t* SEI_flag);

vmppResult video_decoder_receive_frame(struct va_dec_channel *chn, vmppFrame *frame,
                                      vmppDecOutputOptions *out_opt);

vmppResult video_decoder_transfer_frame(struct va_dec_channel *chn, vmppFrame *frame);

vmppResult video_decoder_release_frame(struct va_dec_channel *chn, vmppFrame *frame);

vmppResult video_decoder_get_stream_info(struct va_dec_channel *chn, vmppDecStreamInfo *info);

vmppResult video_decoder_end_of_stream(struct va_dec_channel *chn);

vmppResult video_decoder_get_video_info(vmppStream *stream, vmppCodecType codecType, vmppDecVideoInfo *info);

uint32_t   video_get_idle_dpb_count(struct va_dec_channel *chn);

#endif //__VIDEO_DECODER_H__