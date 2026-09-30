/*
 * Copyright (c) 2022, Vastai Tech. All rights reserved
 *
 * The information contained herein is confidential
 * property of Company. The user, copying, transfer or
 * disclosure of such information is prohibited except
 * by express written agreement with VASTAITECH.
 */

#ifndef __VIDEO_ENCODER_H__
#define __VIDEO_ENCODER_H__

#include "encoder_utils.h"
#include "va_vdata_internal.h"
#include "vmpp_enc_defs.h"

// vsi headers
#include "ewl.h"
#include "hevcencapi.h"

#define MAX_CUTREE_DEPTH 64
#define MAX_GOP_SIZE 16
#define MAX_DELAY_NUM (MAX_CORE_NUM + MAX_CUTREE_DEPTH)
#define MAX_SEI_BUFFER_NUM (MAX_DELAY_NUM * 2)
#define MAX_EWL_MEM_NUM (MAX_DELAY_NUM * 3)


typedef struct {
    int gop_frm_num;
    double sum_intra_vs_interskip;
    double sum_skip_vs_interskip;
    double sum_intra_vs_interskipP;
    double sum_intra_vs_interskipB;
    int sum_costP;
    int sum_costB;
    int last_gopsize;
    int frmSize;
} adapGopCtr;

typedef struct {
    EWLLinearMem_t mem;
    uint32_t used;
} ewlMemory;

struct video_encoder_private_context {
    VCEncConfig cfg;
    VCEncCodingCtrl codingCfg;
    VCEncRateCtrl rcCfg;
    VCEncPreProcessingCfg preProcCfg;
    VCEncIn *encIn;
    VCEncOut *encOut;
    EWLLinearMem_t mcuParamMem;
    VCEncGopPicConfig gopPicCfg_tmp[MAX_GOP_PIC_CONFIG_NUM];
    VCEncGopPicConfig gopPicCfgPass2_tmp[MAX_GOP_PIC_CONFIG_NUM];
    VCEncGopPicSpecialConfig gopPicSpecialCfg_tmp[MAX_GOP_SPIC_CONFIG_NUM];
    VCEncGopPicConfig *gopPicCfg;
    VCEncGopPicConfig *gopPicCfgPass2;
    VCEncGopPicSpecialConfig *gopPicSpecialCfg;
    EncInputBuffer pictureMem[MAX_DELAY_NUM];
#ifdef ROIMAP_4_HEVC2PASS_WORKAROUND
    EWLLinearMem_t roiMapDeltaQpMemFactory[MAX_DELAY_NUM];
#endif
    ewlMemory roiMemFactory[MAX_EWL_MEM_NUM];
    EWLLinearMem_t outbufMemFactory[MAX_CORE_NUM];
    uint32_t parametersSetReady;
    uint32_t parametersSetOutputed;
    uint8_t *parametersSet;
    uint32_t parametersSetSize;
    uint8_t *ivfHeader;
    uint32_t ivfHeaderSize;
    uint32_t ivfHeaderReady;
    uint32_t ivfHeaderOutputed;
    uint32_t ivfFrameCnt;
    u8* av1Header[MAX_CORE_NUM];
    // uint32_t lumaSize;
    // uint32_t chromaSize;
    int32_t nextGopSize;
    VCEncPictureCodingType nextCodingType;
    adapGopCtr agop;
    int32_t gopLowdelay;
    uint32_t roiMapDeltaQpEnable;
    uint32_t roiMapDeltaQpBlockUnit;
    int32_t inputPictureCount;
    int32_t outputPictureCount;
    uint32_t flushing;
    int32_t streamBufNum;    // tb.streamBufNum = cml->streamBufChain ? 2 : 1;
    int32_t pictureEncCount; // tb->picture_enc_cnt
    int32_t frameDelay;      // tb->frame_delay
    int32_t parallelCoreNum; // tb.parallelCoreNum
    int32_t bufferCnt;       // tb.buffer_cnt
    int32_t currInsertedNum; // current inserted number
    EWLLinearMem_t extSRAMMemFactory[MAX_CORE_NUM];
    uint32_t extSramLumBwdSize;
    uint32_t extSramLumFwdSize;
    uint32_t extSramChrBwdSize;
    uint32_t extSramChrFwdSize;
    VCEncVideoCodecFormat codecFormat;
    int32_t vFrames;
    uint32_t eos;
    VCEncRet lastVRet;
    vmppFrame lastInputFrame;
    vmppEncPictureROI lastROI[VMPP_ENC_MAX_ROI_NUM];
    EncSEIBuffer seiBuffer[MAX_SEI_BUFFER_NUM];
    double psnr_total[3];
    int32_t curIPFramePoc;       // for insertIDR
    int32_t lastIPFramePoc;      // for insertIDR
    int32_t insertIdrPicCnt;     // for insertIDR
    VCEncIn encInLast;           // for insertIDR
    vmppFrame lastEncDummyFrame; // for dynamic resolution, only used to mark width & height of last
                                 // encoded frame
    int32_t newResPicCnt;        // for dynamic resolution
    int64_t numberBase;
    uint32_t internalFlushing;
    int32_t firstFrameNumberOfNewRes;

    uint32_t hashLenMismatch;
    // vmppEncROIType lastROIType;
    uint32_t roiMapVersion;
    vmppEncROIType roiType;

    int32_t longterm_enable;
    uint32_t workmode;

    uint32_t sliceSize;
    /* EWL instance used to allocate the buffers of this channel. */
    const void *ewlInst;
    /* Coding control values that have to be applied after VCEncInit(). */
    uint32_t preset;
    uint32_t enableRdoQuant;
    uint32_t enProfiling;
    uint32_t gdrDuration;
};

vmppResult video_encoder_create_chn(struct va_enc_channel *chn, encChannelParameters *param);

vmppResult video_encoder_destory_chn(struct va_enc_channel *chn);

vmppResult video_encoder_alloc_frame(struct va_enc_channel *chn, vmppFrame *frame);

vmppResult video_encoder_free_frame(struct va_enc_channel *chn, vmppFrame *frame);

vmppResult video_multicore_flush(struct va_enc_channel *chn, EncInputBuffer **inputBuffer, vmppStream *stream);

vmppResult handle_dynamic_resolution(struct va_enc_channel *chn, EncInputBuffer *inputBuffer,
                                     EWLLinearMem_t *outputBuffer, int *newEncoder, int need_flush);

vmppResult video_find_next_pic(struct va_enc_channel *chn);

void video_insert_idr_gopchange(struct va_enc_channel *chn, EncInputBuffer **inputBuffer,
                                int *inputIndex);

vmppResult video_encode_frame(struct va_enc_channel *chn, vmppFrame *frame,
                              vmppEncExtendedParams *extParams, vmppStream *stream,
                              uint32_t timeout);

vmppResult video_encoder_release_stream(struct va_enc_channel *chn, vmppStream *stream);

vmppResult video_encoder_initialize_chn(struct va_enc_channel *chn, encChannelParameters *param,
                                        const vmppFrame *frame);
vmppResult video_parse_cu_info(struct va_enc_channel *chn, vmppEncOutData *cuOutData, vmppEncOutInfo *encOutInfo);

#endif //__VIDEO_ENCODER_H__