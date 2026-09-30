#ifndef __ENCODER_UTILS_H__
#define __ENCODER_UTILS_H__

#include "vmpp_enc_defs.h"

// vsi headers
#include "ewl.h"
#include "hevcencapi.h"

typedef struct EncOutputBuffer {
    EWLLinearMem_t mem;
    uint32_t used;
} EncOutputBuffer;

typedef struct EncSEIBuffer {
    uint8_t *data;
    uint32_t size;
    uint32_t used;
} EncSEIBuffer;

typedef struct EncInputBuffer {
    EWLLinearMem_t mem;
    vmppMemoryType memType;
    uint32_t used;
    int32_t index;
    int32_t number;
    int64_t pts;
    int64_t timebaseNum;
    int64_t timebaseDen;
    uint32_t sent2Encoder;
    vmppEncPictureROI roi[VMPP_ENC_MAX_ROI_NUM];
    uint32_t extSEICount;
    ExternalSEI *extSEI;
    uint32_t extSEIBufferSize;
    uint8_t *encodedSEI;
    uint32_t encodedSEIBufferSize;
    uint32_t prefixSeiSize;
    uint32_t suffixSeiSize;
    uint32_t forceIDR;
    uint32_t gopChangeIdr; // for insertIDR
    uint32_t width;
    uint32_t height;
    uint32_t stride[VMPP_MAX_VIDEO_PLANE];
    vmppPixelFormat format;
    uint32_t newResolution;
    uint32_t lumaSize;
    uint32_t chromaSize;
    vmppEncROIType roiType;
    EWLLinearMem_t *roiMapDeltaQpMem;
    uint32_t roiMapDeltaQpSize;
    EWLLinearMem_t *roimapCuCtrlInfoMem;
    uint32_t roimapCuCtrlInfoSize;
    EWLLinearMem_t *roimapCuCtrlIndexMem;
    uint32_t roimapCuCtrlIndexSize;
    uint8_t updateTypeMask;
    uint32_t updateBitRate;
    uint32_t updateVbvBufSize;
    uint32_t updateVbvMaxRate;
    vmppRational updateFrameRate;
    uint32_t svcTemporalId;
    int32_t  updateCrf;
    uint32_t updateKeyInt;
    uint32_t updateInitQp;
    uint32_t updateQpMinI;
    uint32_t updateQpMaxI;
    uint32_t updateQpMinPB;
    uint32_t updateQpMaxPB;
    uint32_t orgStreamSize;
} EncInputBuffer;

#endif