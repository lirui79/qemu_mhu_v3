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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "encoder_utils.h"
#include "va_log.h"
#include "va_vdata_internal.h"
#include "va_version.h"

#include "jpeg_encoder.h"
#include "video_encoder.h"
#include "vmpp_enc_api.h"

// For Q level
#define DEFAULT_Q_LEVEL     50

static vmppVersion s_encoder_version = {LIBENC_VERSION_MAJOR, LIBENC_VERSION_MINOR,
                                        LIB_VERSION_BUILD, (int8_t *)LIB_VERSION_STRING};

vmppVersion *vmppEncGetVersion(void) { return &s_encoder_version; }


void vmppEncSetVfMode(vmppEncVfMode vfMode) {
    (void)(vfMode);
}

vmppResult vmppInitEncoder(vmppConfiguration *cfg)
{
    if (!cfg) {
        LOG_ERROR(ENC, "Invalid parameters for decoder initialization!");
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    if (cfg->logCtx.enableCustomLog)
        registerLogContext(ENC, &cfg->logCtx);

    LOG(ENC, vmpp_LOG_INFO, COLOR_YELLOW, "VMPP Encoder Version: %s", s_encoder_version.versionString);

    return vmpp_RSLT_OK;
}

vmppResult vmppDeInitEncoder()
{
    return vmpp_RSLT_OK;
}

static void setRateControlParams(vmppEncVideoConfiguration *vInCfg, encVideoConfiguration *vParams)
{
    if (vInCfg->rcMode > vmpp_ENC_RC_CQP || vInCfg->rcMode < vmpp_ENC_RC_DEFAULT) {
        vInCfg->rcMode = vmpp_ENC_RC_DEFAULT;
    }

    if (vInCfg->rcMode == vmpp_ENC_RC_DEFAULT) {
        if (vParams->crf >= 0) {
            vInCfg->rcMode = vmpp_ENC_RC_CRF;
            if (vParams->vbvMaxRate != 0) {
                vInCfg->rcMode = vmpp_ENC_RC_CAPPED_CRF;
            }
        }
        else if (vParams->cqp != 0) {
            vInCfg->rcMode = vmpp_ENC_RC_CQP;
        } else if (vParams->vbvBufSize == 0 && vParams->vbvMaxRate == 0) {
            vInCfg->rcMode = vmpp_ENC_RC_VBR;
        } else {
            vInCfg->rcMode = vmpp_ENC_RC_CBR;
        }
    }

    if (vInCfg->rcMode == vmpp_ENC_RC_CBR) {
        if (vParams->vbvBufSize == 0)
            vParams->vbvBufSize = vParams->bitRate;
        if (vParams->vbvMaxRate == 0)
            vParams->vbvMaxRate = vParams->bitRate;
        vParams->cqp = 0;
        vParams->crf = -1;
        vParams->crfFracInt = 0;
    }

    if (vInCfg->rcMode == vmpp_ENC_RC_VBR) {
        vParams->vbvBufSize = 0;
        vParams->vbvMaxRate = 0;
        vParams->cqp = 0;
        vParams->crf = -1;
        vParams->crfFracInt = 0;
    }

    if (vInCfg->rcMode == vmpp_ENC_RC_CQP) {
        vParams->cqp = 1;
        if (vParams->initQp == VMPP_ENC_DEFAULT_PAR) {
            vParams->initQp = 26;
            LOG_WARN(ENC, "rcMode vmpp_ENC_RC_CQP not set initQp, set initQp to be 26!");
        }
        vParams->qpMinI = 0;
        vParams->qpMaxI = 51;
        vParams->qpMinPB = 0;
        vParams->qpMaxPB = 51;
    }

    if (vInCfg->rcMode == vmpp_ENC_RC_CRF) {
        if (vParams->crf < 0) {
            vParams->crf = 23;
            vParams->crfFracInt = 0;
            LOG_WARN(ENC, "rcMode vmpp_ENC_RC_CRF not set crf, set crf to be 23!");
        }
        vParams->vbvBufSize = 0;
        vParams->vbvMaxRate = 0;
        vParams->cqp = 0;
        vParams->llRc = 0;
        vParams->qpMinI = 0;
        vParams->qpMaxI = 51;
        vParams->qpMinPB = 0;
        vParams->qpMaxPB = 51;
        vParams->hrd = 0;
    }

    if (vInCfg->rcMode == vmpp_ENC_RC_CAPPED_CRF) {
        if (vParams->crf < 0) {
            vParams->crf = 23;
            vParams->crfFracInt = 0;
            LOG_WARN(ENC, "rcMode vmpp_ENC_RC_CAPPED_CRF not set crf, set crf to be 23!");
        }
        if (vParams->vbvMaxRate == 0) {
            LOG_WARN(ENC, "rcMode vmpp_ENC_RC_CAPPED_CRF not set vbvMaxRate, set to be vmpp_ENC_RC_CRF!");
        }
        vParams->cqp = 0;
        vParams->llRc = 0;
        vParams->hrd = 0;
    }
}

static void setDefaultVParams(vmppEncVideoConfiguration *vInCfg, encVideoConfiguration *vParams)
{

    if (vParams->lookaheadDepth == VMPP_ENC_DEFAULT_PAR) {
        vParams->lookaheadDepth = 0;
    }

    if (vParams->gopSize == VMPP_ENC_DEFAULT_PAR) {
        if (vParams->lookaheadDepth > 0) {
            vParams->gopSize = 0;
        } else {
            vParams->gopSize = 1;
        }
    }

    if (vParams->keyInt == VMPP_ENC_DEFAULT_PAR)
        vParams->keyInt = 250;

    if (vParams->crf == (int32_t)VMPP_ENC_DEFAULT_PAR) {
        vParams->crf = -1;
        vParams->crfFracInt = 0;
    }

    if (vParams->cqp == VMPP_ENC_DEFAULT_PAR)
        vParams->cqp = 0;

    if (vParams->bitRate == VMPP_ENC_DEFAULT_PAR || vParams->bitRate == 0)
        vParams->bitRate = 1000000;

    if (vParams->vbvBufSize == VMPP_ENC_DEFAULT_PAR)
        vParams->vbvBufSize = 0;

    if (vParams->vbvMaxRate == VMPP_ENC_DEFAULT_PAR)
        vParams->vbvMaxRate = 0;

    if (vInCfg->qualityMode == vmpp_GOLD_QUALITY)
        vParams->preset = 4;
    else if (vInCfg->qualityMode == vmpp_SILVER_QUALITY)
        vParams->preset = 3;
    else if (vInCfg->qualityMode == vmpp_SILVERPLUS_QUALITY)
        vParams->preset = 1;
    else if (vInCfg->qualityMode == vmpp_BRONZE_QUALITY)
        vParams->preset = 2;
    else
        vParams->preset = 2;

    if (vParams->maxFrameSizeMultiple == VMPP_ENC_DEFAULT_PAR) {
        vParams->maxFrameSizeMultiple = -1;
    }

    if (vParams->maxFrameSize == VMPP_ENC_DEFAULT_PAR) {
        vParams->maxFrameSize = 0;
    }

    if(vParams->gopSize != 0 || vParams->maxBFrames == VMPP_ENC_DEFAULT_PAR){
        vParams->maxBFrames = 7;
    }

    if (vParams->profile == vmpp_VIDEO_PRFL_H264_BASELINE && vParams->gopSize != 1) {
        LOG_WARN(ENC, "gopSize set to 1 for H264 Baseline Profile");
        vParams->gopSize = 1;
    }

    if (vParams->profile  == vmpp_VIDEO_PRFL_HEVC_MAIN_STILL_PICTURE && vParams->keyInt != 1) {
        LOG_WARN(ENC, "keyInt set to 1 for HEVC Main Still Picture Profile");
        vParams->keyInt = 1;
    }

    if (vParams->gdrDuration == VMPP_ENC_DEFAULT_PAR) {
        vParams->gdrDuration = 0;
    }

    if (vParams->cqp == VMPP_ENC_DEFAULT_PAR) {
        vParams->cqp = 0;
    }

    if (vParams->bitDepthLuma == VMPP_ENC_DEFAULT_PAR) {
        vParams->bitDepthLuma = 8;
    }

    if (vParams->bitDepthChroma == VMPP_ENC_DEFAULT_PAR) {
        vParams->bitDepthChroma = 8;
    }

    if (vParams->llRc == VMPP_ENC_DEFAULT_PAR) {
        vParams->llRc = 0;
    }

    if (vParams->vbvBufSize == VMPP_ENC_DEFAULT_PAR) {
        vParams->vbvBufSize = 0;
    }

    if (vParams->vbvMaxRate == VMPP_ENC_DEFAULT_PAR) {
        vParams->vbvMaxRate = 0;
    }

    if (vParams->vbr == VMPP_ENC_DEFAULT_PAR) {
        vParams->vbr = 0;
    }

    if (vParams->bBPyramid == VMPP_ENC_DEFAULT_PAR) {
        vParams->bBPyramid = 1;
    }

    if (vParams->hrd == VMPP_ENC_DEFAULT_PAR) {
        vParams->hrd = 0;
    }

    if (vParams->pictureSkip == VMPP_ENC_DEFAULT_PAR) {
        vParams->pictureSkip = 0;
    }

    if (vParams->vfr == VMPP_ENC_DEFAULT_PAR) {
        vParams->vfr = 0;
    }

    if (vParams->svcTLayers == VMPP_ENC_DEFAULT_PAR) {
        vParams->svcTLayers = 0;
    }

    if (vParams->alignmentEnable == VMPP_ENC_DEFAULT_PAR) {
        vParams->alignmentEnable = 0;
    }

    if (vParams->sliceSize == VMPP_ENC_DEFAULT_PAR) {
        vParams->sliceSize = 0;
    }

    if (vParams->ltrInterval == VMPP_ENC_DEFAULT_PAR) {
        vParams->ltrInterval = 0;
    }

    if (vParams->ltrQpDelta == VMPP_ENC_DEFAULT_PAR) {
        vParams->ltrQpDelta = 0;
    }

    if (vParams->ltrRefGap == VMPP_ENC_DEFAULT_PAR) {
        vParams->ltrRefGap = 0;
    }

    if (vParams->openGop == VMPP_ENC_DEFAULT_PAR) {
        vParams->openGop = 0;
    }

    if (vParams->smartEnc == VMPP_ENC_DEFAULT_PAR) {
        vParams->smartEnc = 0;
    }

    if (vParams->disableMMCO == VMPP_ENC_DEFAULT_PAR) {
        vParams->disableMMCO = 0;
    }

    if (vParams->bitRateBalanceLevel == VMPP_ENC_DEFAULT_PAR) {
        vParams->bitRateBalanceLevel = 0;
    }

    if (vParams->enableOutputCuInfo == VMPP_ENC_DEFAULT_PAR) {
        vParams->enableOutputCuInfo = 0;
    }

    setRateControlParams(vInCfg, vParams);
}

vmppResult vmppEncCreateChannel(vmppChannel *chn, vmppEncChannelParameters *param)
{
    if (!chn || !param) {
        LOG_ERROR(ENC, "Invalid parameters : chn %p, param %p.", chn, param);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    vmppEncJPEGConfiguration *jInCfg;
    vmppEncVideoConfiguration *vInCfg;
    encJPEGConfiguration *jParams;
    encVideoConfiguration *vParams;
    vmppResult ret = vmpp_RSLT_OK;
    *chn = NULL;

    if (!param) {
        LOG_ERROR(ENC, "Channel parameter is null.");
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    if (param->codecType != vmpp_CODEC_ENC_JPEG && param->codecType != vmpp_CODEC_ENC_HEVC &&
        param->codecType != vmpp_CODEC_ENC_H264 && param->codecType != vmpp_CODEC_ENC_AV1) {
        LOG_ERROR(ENC, "Create encoder channel failed: NOT SUPPORT, type %d.", param->codecType);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    struct va_enc_channel *echn = malloc(sizeof(struct va_enc_channel));
    if (!echn) {
        LOG_ERROR(ENC, "Fail to malloc channel instance.");
        return vmpp_RSLT_ERR_NO_MEMORY;
    }

    memset(echn, 0, sizeof(struct va_enc_channel));
    atomic_set_u32(&echn->state, vmpp_ST_NONE);

    echn->params.encDevice   = param->encDevice;
    echn->params.memDevice   = param->memDevice;
    echn->params.codecType = param->codecType;
    echn->params.outbufNum = param->outbufNum;
    echn->params.enProfiling = param->enProfiling;

    if (param->codecType == vmpp_CODEC_ENC_JPEG) {
        jInCfg = &param->jpegConfig;
        jParams = &echn->params.jpegConfig;

        jParams->codingWidth = jInCfg->codingWidth;
        jParams->codingHeight = jInCfg->codingHeight;
        jParams->frameType = jInCfg->frameType;
        jParams->comLength = jInCfg->comLength;
        jParams->losslessEn = jInCfg->losslessEn;
        jParams->predictMode = jInCfg->predictMode;
        jParams->ptransValue = jInCfg->ptransValue;
        jParams->qLevel = DEFAULT_Q_LEVEL;

		// set q level
        if (jInCfg->qLevel > USER_DEFINED_QTABLE) {
            LOG_WARN(ENC, "Qlevel %d is out of range(1~100), sdk will set to 50(default).", jInCfg->qLevel);
        } else if (jInCfg->qLevel == USER_DEFINED_QTABLE) {
            for (uint32_t i = 0; i < 64; i++) {
                jParams->qTableLuma[i] = jInCfg->qTableLuma[i];
                jParams->qTableChroma[i] = jInCfg->qTableChroma[i];
            }
            jParams->qLevel = USER_DEFINED_QTABLE;
        } else if (jInCfg->qLevel == 0) {
            // do nothing
        } else {
            jParams->qLevel = jInCfg->qLevel;
        }

        jParams->rotation = jInCfg->preProcess.rotation;

        if (echn->params.jpegConfig.comLength) {
            echn->params.jpegConfig.pCom = (uint8_t *)malloc(echn->params.jpegConfig.comLength);
            if (!echn->params.jpegConfig.pCom) {
                LOG_ERROR(ENC, "Fail to malloc pCom, size %d.", echn->params.jpegConfig.comLength);
                free(echn);
                return vmpp_RSLT_ERR_NO_MEMORY;
            }
            memcpy(echn->params.jpegConfig.pCom, param->jpegConfig.pCom,
                   param->jpegConfig.comLength);
        }
        ret = jpeg_encoder_create_chn(echn, &echn->params);
        if (ret != vmpp_RSLT_OK) {
            jpeg_encoder_destory_chn(echn);
            if (echn->params.jpegConfig.pCom)
                free(echn->params.jpegConfig.pCom);
            free(echn);
            LOG_ERROR(ENC, "Fail to create jpeg encode channel, err %d", ret);
            if (ret == vmpp_RSLT_ERR_ALLOC_CHANNEL) {
                return vmpp_RSLT_ERR_ALLOC_CHANNEL;
            }
            return vmpp_RSLT_ERR_ENC_INIT;
        }
    } else if ((param->codecType == vmpp_CODEC_ENC_H264) ||
               (param->codecType == vmpp_CODEC_ENC_HEVC) || param->codecType == vmpp_CODEC_ENC_AV1) {
        vInCfg = &param->videoConfig;
        vParams = &echn->params.videoConfig;

        vParams->profile = vInCfg->profile;
        vParams->level = vInCfg->level;
        vParams->width = vInCfg->width;
        vParams->height = vInCfg->height;
        vParams->frameRate.denominator = vInCfg->frameRate.denominator;
        vParams->frameRate.numerator = vInCfg->frameRate.numerator;
        vParams->bitDepthLuma = vInCfg->bitDepthLuma;
        vParams->bitDepthChroma = vInCfg->bitDepthChroma;
        vParams->gopSize = vInCfg->gopSize;
        vParams->gdrDuration = vInCfg->gdrDuration;
        vParams->lookaheadDepth = vInCfg->lookaheadDepth;
        vParams->tune = vInCfg->tune;
        vParams->keyInt = vInCfg->keyInt;

        vParams->crf = (int16_t)(vInCfg->crf & 0xFFFF);//The lower 16 bits represent the integer part of crf.
        vParams->crfFracInt = CLIP3(0, 9, (vInCfg->crf >> 16) & 0xFFFF);//The upper 16 bits represent the fractional part of crf.
        vParams->cqp = vInCfg->cqp;
        vParams->llRc = vInCfg->llRc;

        vParams->bitRate = vInCfg->bitRate;
        vParams->initQp = vInCfg->initQp;
        vParams->vbvBufSize = vInCfg->vbvBufSize;
        vParams->vbvMaxRate = vInCfg->vbvMaxRate;
        vParams->intraQpDelta = vInCfg->intraQpDelta;
        vParams->qpMinI = vInCfg->qpMinI;
        vParams->qpMaxI = vInCfg->qpMaxI;
        vParams->qpMinPB = vInCfg->qpMinPB;
        vParams->qpMaxPB = vInCfg->qpMaxPB;
        vParams->vbr = vInCfg->vbr;
        vParams->aq_strength = vInCfg->aqStrength;
        // vParams->enableROI = vInCfg->enableROI;
        vParams->P2B = vInCfg->P2B;
        vParams->bBPyramid = vInCfg->bBPyramid;
        vParams->maxFrameSizeMultiple = vInCfg->maxFrameSizeMultiple;
        vParams->maxFrameSize = vInCfg->maxFrameSize;
        vParams->maxBFrames = vInCfg->maxBFrames;
        vParams->smartEnc = vInCfg->smartEnc;
        vParams->bitRateBalanceLevel = vInCfg->bitRateBalanceLevel;

        vParams->enableOutputCuInfo = vInCfg->enableOutputCuInfo;
        vParams->roiType = vInCfg->roiType;
        vParams->roiMapDeltaQpEnable =
            vInCfg->roiType == vmpp_ENC_ROI_MAP; // vInCfg->roiMapDeltaQpEnable;
        vParams->roiMapDeltaQpBlockUnit = vInCfg->roiMapDeltaQpBlockUnit;
        vParams->roiMapQpDeltaVersion = vInCfg->roiMapQpDeltaVersion;
        vParams->roiCuCtrlVersion = 1; // vInCfg->roiCuCtrlVersion;
        vParams->hrd = vInCfg->hrd;
        vParams->pictureSkip = vInCfg->pictureSkip;
        vParams->vfr = vInCfg->vfr;
        vParams->svcTLayers = vInCfg->svcTLayers;
        vParams->alignmentEnable = vInCfg->alignmentEnable;
        vParams->sliceSize = vInCfg->sliceSize;

        vParams->ltrInterval = vInCfg->ltrInterval;
        vParams->ltrQpDelta = vInCfg->ltrQpDelta;
        vParams->ltrRefGap = vInCfg->ltrRefGap;
        vParams->rotation = vInCfg->preProcess.rotation;

        vParams->openGop = vInCfg->openGop;
        vParams->disableMMCO = vInCfg->disableMMCO;

        vParams->inLoopDSRatio = vInCfg->inLoopDSRatio;
        vParams->aq_mode = vInCfg->aqMode;
        vParams->psyFactor = vInCfg->psyFactor;
        vParams->rdoLevel = vInCfg->rdoLevel;
        vParams->enableRdoQuant = vInCfg->enableRdoQuant;
        vParams->qCompress = vInCfg->qCompress;
        vParams->iQpFactor = VMPP_ENC_DEFAULT_PAR;

        setDefaultVParams(vInCfg, vParams);

        ret = video_encoder_create_chn(echn, &echn->params);
        LOG_INFO(ENC, "Encode channel %p created, ret %d.", echn, ret);
        if (ret != vmpp_RSLT_OK) {
            video_encoder_destory_chn(echn);
            free(echn);
            LOG_ERROR(ENC, "Fail to create video encode channel, err %d", ret);
            if (ret == vmpp_RSLT_ERR_ALLOC_CHANNEL) {
                return vmpp_RSLT_ERR_ALLOC_CHANNEL;
            }
            return vmpp_RSLT_ERR_ENC_INIT;
        }
    }

    atomic_set_u32(&echn->state, vmpp_ST_READY);
    pthread_mutex_init(&echn->enc_out_buffer_mutex, NULL);
    *chn = echn;
    return vmpp_RSLT_OK;
}

vmppResult vmppEncDestroyChannel(vmppChannel *chn)
{
    if (chn == NULL || !*chn) {
        LOG_ERROR(ENC, "NULL channel pointer.");
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    struct va_enc_channel *inst = (struct va_enc_channel *)(*chn);
    vmppResult ret = vmpp_RSLT_OK;
    vmppState state = (vmppState)atomic_get_u32(&inst->state);
    if (state == vmpp_ST_RUNNING /* || state == vmpp_ST_STOPPING*/) {
        LOG_WARN(ENC, "Can not destroy encode channel due to incorrect state: %d.", state);
        return vmpp_RSLT_ERR_INVALID_STATE;
    }

    switch (inst->params.codecType) {
    case vmpp_CODEC_ENC_JPEG:
        ret = jpeg_encoder_destory_chn(inst);
        break;
    case vmpp_CODEC_ENC_H264:
    case vmpp_CODEC_ENC_HEVC:
    case vmpp_CODEC_ENC_AV1:
        ret = video_encoder_destory_chn(inst);
        break;
    default:
        break;
    }

    if (ret == vmpp_RSLT_OK) {
        pthread_mutex_destroy(&inst->enc_out_buffer_mutex);
        if (inst->params.codecType == vmpp_CODEC_ENC_JPEG && inst->params.jpegConfig.pCom)
            free(inst->params.jpegConfig.pCom);
        free(inst);
        LOG_INFO(ENC, "Encode channel %p destroyed.", inst);
        *chn = NULL;
    } else {
        // TODO: some error handling
    }

    return ret;
}

vmppResult vmppEncAllocFrame(vmppChannel chn, vmppFrame *frame) {
    if (!chn || !frame) {
        LOG_ERROR(ENC, "Invalid parameter(s): chn %p, frame %p.", chn, frame);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    vmppResult ret = vmpp_RSLT_OK;
    struct va_enc_channel *inst = (struct va_enc_channel *)chn;
    switch (inst->params.codecType) {
    case vmpp_CODEC_ENC_JPEG:
        ret = jpeg_encoder_alloc_frame(chn, frame);
        break;
    case vmpp_CODEC_ENC_H264:
    case vmpp_CODEC_ENC_HEVC:
    case vmpp_CODEC_ENC_AV1:
        ret = video_encoder_alloc_frame(chn, frame);
        break;
    default:
        /* Never return OK here, otherwise the caller gets a frame without any buffer. */
        LOG_ERROR(ENC, "Unsupported codec type %d for input frame allocation.",
                  inst->params.codecType);
        ret = vmpp_RSLT_ERR_INVALID_PARAMS;
        break;
    }

    return ret;
}

VMPP_API vmppResult vmppEncFreeFrame(vmppChannel chn, vmppFrame *frame) {
    if (!chn || !frame) {
        LOG_ERROR(ENC, "Invalid parameter(s): chn %p, frame %p.", chn, frame);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }
    
    vmppResult ret = vmpp_RSLT_OK;
    struct va_enc_channel *inst = (struct va_enc_channel *)chn;
    switch (inst->params.codecType) {
    case vmpp_CODEC_ENC_JPEG:
        ret = jpeg_encoder_free_frame(chn, frame);
        break;
    case vmpp_CODEC_ENC_H264:
    case vmpp_CODEC_ENC_HEVC:
    case vmpp_CODEC_ENC_AV1:
        ret = video_encoder_free_frame(chn, frame);
        break;
    default:
        break;
    }
    
    return ret;
}

vmppResult vmppEncEncodeFrame(vmppChannel chn, vmppFrame *frame, vmppEncExtendedParams *extParams,
                              vmppStream *stream, uint32_t timeout)
{
    if (!chn || !frame || !stream) {
        LOG_ERROR(ENC, "Invalid parameter(s): chn %p, frame %p.", chn, frame);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    vmppResult ret = vmpp_RSLT_OK;
    struct va_enc_channel *inst = (struct va_enc_channel *)chn;

    if (atomic_get_u32(&inst->state) == vmpp_ST_READY) {
        if (frame->memoryType == vmpp_MEM_FLUSH)
            return vmpp_RSLT_WARN_EOS;

        if (inst->params.codecType == vmpp_CODEC_ENC_H264 ||
            inst->params.codecType == vmpp_CODEC_ENC_HEVC || inst->params.codecType == vmpp_CODEC_ENC_AV1) {
            ret = video_encoder_initialize_chn(chn, &inst->params, frame);
            if (ret != vmpp_RSLT_OK) {
                LOG_ERROR(ENC, "Fail to initialize video encode channel, err %d", ret);
                return vmpp_RSLT_ERR_ENC_INIT;
            }
        }
        atomic_set_u32(&inst->state, vmpp_ST_RUNNING);
    }

    if (frame->memoryType == vmpp_MEM_FLUSH) {
        atomic_set_u32(&inst->state, vmpp_ST_STOPPING);
        if (inst->params.codecType == vmpp_CODEC_ENC_JPEG) {
            LOG_DEBUG(ENC, "JPEG ENCODER FLUSH.");
            return vmpp_RSLT_WARN_EOS;
        }
    }

    vmppState state = atomic_get_u32(&inst->state);
    if (state != vmpp_ST_RUNNING && state != vmpp_ST_STOPPING) {
        LOG_ERROR(ENC, "Invalid state: %d.", state);
        return vmpp_RSLT_ERR_INVALID_STATE;
    }

    if (timeout < VMPP_MIN_TIMEOUT_MS) {
        LOG_WARN(ENC, "Timeout(%d) is too small, using default minimum value(%d).", timeout,
                 VMPP_MIN_TIMEOUT_MS);
        timeout = VMPP_MIN_TIMEOUT_MS;
    }

    switch (inst->params.codecType) {
    case vmpp_CODEC_ENC_JPEG:
        ret = jpeg_encode_frame(inst, frame, stream, timeout);
        break;
    case vmpp_CODEC_ENC_H264:
    case vmpp_CODEC_ENC_HEVC:
    case vmpp_CODEC_ENC_AV1:
        ret = video_encode_frame(inst, frame, extParams, stream, timeout);
        break;
    default:
        break;
    }

#if 0
    if (ret < 0)
        atomic_set_u32(&inst->state, vmpp_ST_ERROR);
#endif

    return ret;
}

vmppResult vmppEncReleaseStream(vmppChannel chn, vmppStream *stream)
{
    if (!chn || !stream) {
        LOG_ERROR(ENC, "Invalid parameter(s): chn %p, stream %p.", chn, stream);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    vmppResult ret = vmpp_RSLT_OK;
    struct va_enc_channel *inst = (struct va_enc_channel *)chn;
    vmppState state = atomic_get_u32(&inst->state);
    if (state != vmpp_ST_RUNNING && state != vmpp_ST_STOPPING) {
        LOG_ERROR(ENC, "Invalid state: %d.", state);
        return vmpp_RSLT_ERR_INVALID_STATE;
    }

    switch (inst->params.codecType) {
    case vmpp_CODEC_ENC_JPEG:
        ret = jpeg_encoder_release_stream(inst, stream);
        break;
    case vmpp_CODEC_ENC_H264:
    case vmpp_CODEC_ENC_HEVC:
    case vmpp_CODEC_ENC_AV1:
        ret = video_encoder_release_stream(inst, stream);
        break;
    default:
        break;
    }
    return ret;
}

void vmppEncGetVideoCaps(vmppCodecType type, vmppEncVideoCapability *caps)
{
    if (caps) {
        caps->bitDepth = 8;
        caps->minWidth = 176;
        caps->minHeight = 144;
        caps->maxWidth = 8192;
        caps->maxHeight = 8192;
        if (type == vmpp_CODEC_ENC_H264) {
            caps->maxProFile = vmpp_VIDEO_PRFL_H264_HIGH_10;
            caps->maxLevel = vmpp_VIDEO_LVL_H264_6_2;
        } else if (type == vmpp_CODEC_ENC_HEVC) {
            caps->maxProFile = vmpp_VIDEO_PRFL_HEVC_MAIN_REXT;
            caps->maxLevel = vmpp_VIDEO_LVL_HEVC_6_2;
        } else if (type == vmpp_CODEC_ENC_AV1) {
            caps->maxWidth = 4096;
            caps->maxHeight = 2304;
            caps->maxProFile = vmpp_VIDEO_PRFL_AV1_MAIN;
            caps->maxLevel = vmpp_VIDEO_LVL_AV1_5_1;
        } else {
            // Do nothing
        }

        caps->pixelFormats[0] = vmpp_PIX_FMT_NV12;
        caps->pixelFormats[1] = vmpp_PIX_FMT_NV21;
        caps->pixelFormats[2] = vmpp_PIX_FMT_YUV420P;
        caps->pixelFormats[3] = vmpp_PIX_FMT_YUV420_PLANAR_10BIT_LE;
        caps->pixelFormats[4] = vmpp_PIX_FMT_YUV420_PLANAR_10BIT_P010;
        caps->pixelFormats[5] = vmpp_PIX_FMT_NONE;
    }
}

vmppResult vmppEncParseCuInformation(vmppChannel chn, vmppEncOutData *outData, vmppEncOutInfo *encInfo)
{
    if (!chn || !outData || !encInfo) {
        LOG_ERROR(ENC, "Invalid parameter(s): chn %p, outData %p encInfo %p.", chn, outData, encInfo);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    return video_parse_cu_info(chn, outData, encInfo);
}
