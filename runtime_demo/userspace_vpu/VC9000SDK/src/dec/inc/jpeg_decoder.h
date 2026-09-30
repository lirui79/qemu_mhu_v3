/*
 * Copyright (c) 2022, Vastai Tech. All rights reserved
 *
 * The information contained herein is confidential
 * property of Company. The user, copying, transfer or
 * disclosure of such information is prohibited except
 * by express written agreement with VASTAITECH.
 */

#ifndef __JPEG_DECODER_H__
#define __JPEG_DECODER_H__

#include "va_vdata_internal.h"
#include "vmpp_dec_defs.h"

// vsi headers
#include "decapicommon.h"
#include "dectypes.h"
#include "dwl.h"
#include "vcdecapi.h"

#define JPEG_MAX_BUFFERS 2

struct jpeg_decoder_private_context {
    uint32_t pp_enabled;
    PpUnitConfig ppu_cfg[DEC_MAX_OUT_COUNT];
    struct DecSequenceInfo image_info;
    struct DWLLinearMem ext_buffers[JPEG_MAX_BUFFERS];
    uint32_t ext_buffers_number;
    uint32_t guard_size;
};

vmppResult jpeg_decoder_create_chn(struct va_dec_channel *chn);

vmppResult jpeg_decoder_destory_chn(struct va_dec_channel *chn);

vmppResult jpeg_decoder_send_stream(struct va_dec_channel *chn, vmppStream *stream,
                                    uint32_t timeout);

vmppResult jpeg_decoder_receive_frame(struct va_dec_channel *chn, vmppFrame *frame,
                                      vmppDecOutputOptions *out_opt);

vmppResult jpeg_decoder_transfer_frame(struct va_dec_channel *chn, vmppFrame *frame);

vmppResult jpeg_decoder_release_frame(struct va_dec_channel *chn, vmppFrame *frame);

vmppResult jpeg_decoder_get_stream_info(struct va_dec_channel *chn, vmppDecStreamInfo *info);

vmppResult jpeg_decoder_get_jpeg_info(vmppStream *stream, vmppDecJpegInfo *info);

vmppResult jpeg_decoder_end_of_stream(struct va_dec_channel *chn);

#endif //__JPEG_DECODER_H__