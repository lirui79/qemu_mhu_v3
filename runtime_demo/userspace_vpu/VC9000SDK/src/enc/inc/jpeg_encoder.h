/*
 * Copyright (c) 2022, Vastai Tech. All rights reserved
 *
 * The information contained herein is confidential
 * property of Company. The user, copying, transfer or
 * disclosure of such information is prohibited except
 * by express written agreement with VASTAITECH.
 */

#ifndef __JPEG_ENCODER_H__
#define __JPEG_ENCODER_H__

#include "encoder_utils.h"
#include "va_vdata_internal.h"
#include "vmpp_enc_defs.h"

#include <pthread.h>

//#include "dma_trans.h"
#include "ewl.h"
#include "jpegencapi.h"

#define USER_DEFINED_QTABLE 101

typedef struct {
  u32 streamRDCounter;
  u32 streamMultiSegEn;
  u32 streamMultiSegOffset;
  u8 *streamBase;
  u32 segmentSize;
  u32 segmentAmount;
  FILE *outStreamFile;
} SegmentCtl_s;


struct jpeg_encoder_private_context {
    JpegEncCfg       cfg;
    EWLLinearMem_t   pictureMem;
    EncOutputBuffer  outbufMem[MAX_STRM_BUF_NUM];
    pthread_mutex_t  outbufMemMutex;
    SegmentCtl_s     streamSegCtl;
};


vmppResult jpeg_encoder_create_chn(struct va_enc_channel *chn, encChannelParameters *param);

vmppResult jpeg_encoder_destory_chn(struct va_enc_channel *chn);

vmppResult jpeg_encoder_alloc_frame(struct va_enc_channel *chn, vmppFrame *frame);

vmppResult jpeg_encoder_free_frame(struct va_enc_channel *chn, vmppFrame *frame);

vmppResult jpeg_encode_frame(struct va_enc_channel *chn, vmppFrame *frame, vmppStream *stream, uint32_t timeout);

vmppResult jpeg_encoder_release_stream(struct va_enc_channel *chn, vmppStream *stream);

#endif