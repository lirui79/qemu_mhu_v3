/*
 * Copyright (C) 2022-2023 VASTAI Technologies Co., Ltd. All Rights Reserved.
 */

/**
 * !@file   vmpp_enc_defs.h
 * !@date   2022-06-09
 * !@brief  This file contains the constant, enumeration and structure definitions for encoder.
 */

#ifndef __VMPP_ENC_DEFS_H__
#define __VMPP_ENC_DEFS_H__

#include "vmpp_common.h"

/* clang-format off */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* -------------------- Macro -------------------- */
#define VMPP_ENC_MAX_STRM_BUF_NUM   2
#define VMPP_ENC_MAX_REF_FRAMES     8
#define VMPP_ENC_MAX_GOP_SIZE       16
#define VMPP_ENC_MAX_LTR_FRAMES     VMPP_ENC_MAX_REF_FRAMES
#define VMPP_ENC_MAX_ROI_NUM        8
#define VMPP_ENC_DEFAULT_PAR        0xFFFFFFF
#define VMPP_ENC_MAX_PIX_FMT_NUM    23
#define VMPP_ENC_MAX_MOSAIC_NUM     12
#define VMPP_ENC_MAX_OVERLAY_NUM    8
#define VMPP_SPECIFY_COREID         0xAAAA
#define VMPP_ENC_UPDATE_NONE        0x00
#define VMPP_ENC_UPDATE_CBR         0x01
#define VMPP_ENC_UPDATE_FRAMERATE   0x02
#define VMPP_ENC_UPDATE_CRF         0x04
#define VMPP_ENC_UPDATE_KEYINT      0x08
#define VMPP_ENC_UPDATE_QP          0x10

/**
 * @brief Picture color space conversion (RGB input) for pre-processing
 */
typedef enum vmppEncColorConversionType {
    vmpp_RGBTOYUV_DEFAULT,                        // Color conversion default mode, sv100: vmpp_RGBTOYUV_BT601_FULL_RANGE, sg100: vmpp_RGBTOYUV_BT601_LIMITED_RANGE
    vmpp_RGBTOYUV_BT601_FULL_RANGE,               // Color conversion to yuv in full range [0,255] according to BT.601
    vmpp_RGBTOYUV_BT601_LIMITED_RANGE,            // Color conversion to yuv in limited range [16,235] according to BT.601, sv100 not support
    vmpp_RGBTOYUV_BT2020,                         // Color conversion to yuv according to BT.2020. It's for 10bit rgba
} vmppEncColorConversionType;

/**
 * @brief vf schedule mode, only for sg100
 */
typedef enum vmppEncVfMode {
    vmpp_VFMODE_NO_CHANGE,
    vmpp_VFMODE_SHARE,
    vmpp_VFMODE_LIMIT,
} vmppEncVfMode;

/**
 * @brief quality mode
 */
typedef enum vmppEncQualityMode {
    vmpp_GOLD_QUALITY,
    vmpp_SILVER_QUALITY,
    vmpp_SILVERPLUS_QUALITY,
    vmpp_BRONZE_QUALITY,
} vmppEncQualityMode;

/**
 * @brief tune type
 */
typedef enum vmppEncTuneType {
    vmpp_ENC_TUNE_PSNR,
    vmpp_ENC_TUNE_SSIM,
    vmpp_ENC_TUNE_VISUAL,
    vmpp_ENC_TUNE_SHARP_VISUAL,
    vmpp_ENC_TUNE_VMAF,
} vmppEncTuneType;

/**
 * @brief QP type
 */
typedef enum vmppEncQPType {
    vmpp_ENC_QP,
    vmpp_ENC_QP_DELTA,
} vmppEncQPType;

/**
 * @brief ROI type
 */
typedef enum vmppEncROIType {
    vmpp_ENC_ROI_NONE,
    vmpp_ENC_ROI_RANGE,
    vmpp_ENC_ROI_MAP,
} vmppEncROIType;

/**
 * @brief AQ MODE type
 */
typedef enum vmppEncAQMode {
    vmpp_AQ_NONE,
    vmpp_AQ_VARIANCE,
    vmpp_AQ_AUTO_VARIANCE,
    vmpp_AQ_AUTO_VARIANCE_BIASED,
} vmppEncAQMode;

/**
 * @brief Picture Rotation type
 */
typedef enum vmppEncPictureRotation {
    vmpp_ROTATE_0,
    vmpp_ROTATE_90R,  /* Rotate 90 degrees clockwise */
    vmpp_ROTATE_90L,  /* Rotate 90 degrees counter-clockwise */
    vmpp_ROTATE_180R, /* Rotate 180 degrees clockwise */
} vmppEncPictureRotation;

/**
 * @brief Rate Contorl Mode type
 */
typedef enum vmppEncRcMode {
    vmpp_ENC_RC_DEFAULT,
    vmpp_ENC_RC_CBR,
    vmpp_ENC_RC_VBR,
    vmpp_ENC_RC_CRF,
    vmpp_ENC_RC_CAPPED_CRF,
    vmpp_ENC_RC_CQP,
} vmppEncRcMode;

/* -------------------- Structure -------------------- */

/**
 * @brief picture area
 */
typedef struct vmppEncPictureArea {
    uint32_t                        enable;                     // whether enable this area
    uint32_t                        top;                        // top of the area [0~height]
    uint32_t                        left;                       // left of the area [0~width]
    uint32_t                        bottom;                     // bottom of the area [top~height]
    uint32_t                        right;                      // right of the area [left~width]
} vmppEncPictureArea;

/**
 * @brief picture ROI area, only effective when roiType is vmpp_ENC_ROI_RANGE and lookaheadDepth is zero
 */
typedef struct vmppEncPictureROI {
    vmppEncPictureArea              area;                       // roi area description
    vmppEncQPType                   qpType;
    int32_t                         qpValue;                    // vmpp_ENC_QP: [0, 51], vmpp_ENC_QP_DELTA: [-51, 51]
} vmppEncPictureROI;

/**
 * @brief ROI map data structure, only effective when roiType is vmpp_ENC_ROI_MAP
 */
typedef struct vmppEncROIMap {
    int8_t *                        roiMapDeltaQp;
    uint32_t                        roiMapDeltaQpSize;
} vmppEncROIMap;

/**
 * @brief Preprocess Params structure
 */
typedef struct vmppEncPreprocessParams {
    vmppEncPictureRotation          rotation;     // rotate input image, default vmpp_ROTATE_0
    uint32_t                        mirror;       // 0:disable mirror 1:enable mirror
    uint32_t                        scaledWidth;  // Optional down-scaled output picture width
    uint32_t                        scaledHeight; // Optional down-scaled output picture height
    /* constant chroma control */
    uint32_t                        constChromaEn;
    uint32_t                        constCb;
    uint32_t                        constCr;
    /* Mosaic region parameters */
    uint32_t                        mosaicEnables; /** Mosaic region enable */
    uint32_t                        mosXoffset[VMPP_ENC_MAX_MOSAIC_NUM]; /** Mosaic region top left horizontal offset */
    uint32_t                        mosYoffset[VMPP_ENC_MAX_MOSAIC_NUM]; /** Mosaic region top left vertical offset */
    uint32_t                        mosWidth[VMPP_ENC_MAX_MOSAIC_NUM]; /** Mosaic region width */
    uint32_t                        mosHeight[VMPP_ENC_MAX_MOSAIC_NUM]; /** Mosaic region height */
    /* Overlay */
    uint32_t                        overlayEnables;
    char*                           olInput[VMPP_ENC_MAX_OVERLAY_NUM];
    uint32_t                        olFormat[VMPP_ENC_MAX_OVERLAY_NUM];
    uint32_t                        olAlpha[VMPP_ENC_MAX_OVERLAY_NUM];
    uint32_t                        olWidth[VMPP_ENC_MAX_OVERLAY_NUM];
    uint32_t                        olCropWidth[VMPP_ENC_MAX_OVERLAY_NUM];
    uint32_t                        olScaleWidth[VMPP_ENC_MAX_OVERLAY_NUM];
    uint32_t                        olHeight[VMPP_ENC_MAX_OVERLAY_NUM];
    uint32_t                        olCropHeight[VMPP_ENC_MAX_OVERLAY_NUM];
    uint32_t                        olScaleHeight[VMPP_ENC_MAX_OVERLAY_NUM];
    uint32_t                        olXoffset[VMPP_ENC_MAX_OVERLAY_NUM];
    uint32_t                        olCropXoffset[VMPP_ENC_MAX_OVERLAY_NUM];
    uint32_t                        olYoffset[VMPP_ENC_MAX_OVERLAY_NUM];
    uint32_t                        olCropYoffset[VMPP_ENC_MAX_OVERLAY_NUM];
    uint32_t                        olYStride[VMPP_ENC_MAX_OVERLAY_NUM];
    uint32_t                        olUVStride[VMPP_ENC_MAX_OVERLAY_NUM];
    uint32_t                        olBitmapY[VMPP_ENC_MAX_OVERLAY_NUM];
    uint32_t                        olBitmapU[VMPP_ENC_MAX_OVERLAY_NUM];
    uint32_t                        olBitmapV[VMPP_ENC_MAX_OVERLAY_NUM];
    uint32_t                        olSuperTile[VMPP_ENC_MAX_OVERLAY_NUM];
} vmppEncPreprocessParams;

/**
 * @brief update qp setting for rate control
 */
typedef struct vmppEncUpdateQp {
    uint32_t                        updateInitQp;               // [0, 51], update initQp
    uint32_t                        updateQpMinI;               // [0, 51], update Min qp for I frame
    uint32_t                        updateQpMaxI;               // [0, 51], update Max qp for I frame
    uint32_t                        updateQpMinPB;              // [0, 51], update Min qp for PB frame
    uint32_t                        updateQpMaxPB;              // [0, 51], update Max qp for PB frame
} vmppEncUpdateQp;

/**
 * @brief extended parameters for encoder
 */
typedef struct vmppEncExtendedParams {
    uint32_t                        forceIDR;                   // force current frame to be IDR frame
    union {
        vmppEncPictureROI           roi[VMPP_ENC_MAX_ROI_NUM];  // roi range informations
        vmppEncROIMap               roiMap;                     // roi map informations
    };
    uint8_t                         updateTypeMask;             // update type mask,
                                                                // updateTypeMask & VMPP_ENC_UPDATE_CBR is true indicates update cbr mode (updateBitRate, updateVbvBufSize, updateVbvMaxRate),
                                                                // updateTypeMask & VMPP_ENC_UPDATE_FRAMERATE is true indicates update framerate (updateFrameRate),
                                                                // updateTypeMask & VMPP_ENC_UPDATE_CRF is true indicates update crf or capped crf mode (updateCrf, updateVbvBufSize, updateVbvMaxRate)
                                                                // updateTypeMask & VMPP_ENC_UPDATE_KEYINT is true indicates update keyint (updateKeyInt)
    uint32_t                        updateBitRate;              // bps, updateTypeMask & VMPP_ENC_UPDATE_CBR is true, bitrate to be updated
    uint32_t                        updateVbvBufSize;           // bits, updateTypeMask & VMPP_ENC_UPDATE_CBR or updateTypeMask & VMPP_ENC_UPDATE_CRF is true, vbvBufSize to be updated
    uint32_t                        updateVbvMaxRate;           // bps, updateTypeMask & VMPP_ENC_UPDATE_CBR or updateTypeMask & VMPP_ENC_UPDATE_CRF is true, vbvMaxRate to be updated
    vmppRational                    updateFrameRate;            // updateTypeMask & VMPP_ENC_UPDATE_CRF is true, framerate to be update
    int32_t                         updateCrf;                  // when crf is [0, 51] and updateTypeMask & VMPP_ENC_UPDATE_CRF is true, update crf
    uint32_t                        forceLTR;                   // force current frame to be LTR frame
    uint32_t                        updateKeyInt;               // updateTypeMask & VMPP_ENC_UPDATE_KEYINT is true, keyint to be updated, only for sv100
    vmppEncUpdateQp                 updateQpSetting;            // @vmppEncUpdateQp, updateTypeMask & VMPP_ENC_UPDATE_QP is true, qp setting for rate control to be updated, only for sg100
} vmppEncExtendedParams;

/**
 * @brief parameters for video encoder
 */
typedef struct vmppEncVideoConfiguration {
    vmppVideoProfile                profile;

    /** !!!NOTEs: Automatically Level Detection
     *
     * When level in level is specified as 0, the actual level encoded in the SPS
     * will be selected according to the picture size, frame rate, bit rate and cpb size. The
     * smallest level which can fit current setting will be used.
     *
     * Currently, only HEVC and H264 are valid for automatically level selection.
     * AV1 only support vmpp_VIDEO_LVL_AV1_5_1. */
    vmppVideoLevel                  level;
    uint32_t                        width;                      // Encoded picture width in pixels, multiple of 2
    uint32_t                        height;                     // Encoded picture height in pixels, multiple of 2

    /** !!!NOTEs: frameRate
     *
     * The frameRate.numerator is within stream time scale, [1~1048575]
     * Maximum frame rate is frameRate.numerator/frameRate.denominator in frames/second.
     * The actual frame rate will be defined by timeIncrement of encoded pictures,
     * [1~frameRate.numerator] */
    vmppRational                    frameRate;
    uint32_t                        bitDepthLuma;
    uint32_t                        bitDepthChroma;
    uint32_t                        gopSize;                    // sequence level GOP size, [0, 8] and 16, 0 for adaptive GOP size.
    uint32_t                        gdrDuration;                // only works with 1 pass and IPPP gop structure, and must be smaller than keyInt
    uint32_t                        lookaheadDepth;             // 0 for 1 pass, [4, 40] for 2 pass
    vmppEncQualityMode              qualityMode;                // [0, 3]
    vmppEncTuneType                 tune;                       // 0 for PSNR, 1 for SSIM, 2 for VISUAL, 3 for SHARP_VISUAL, 4 for VAMF.
    uint32_t                        keyInt;                     // IDR interval, VMPP_ENC_DEFAULT_PAR for auto decided by encoder

    int32_t                         crf;                        // [0, 51], VMPP_ENC_DEFAULT_PAR for auto decided by encoder
    uint32_t                        cqp;                        // mark if using cqp mode, VMPP_ENC_DEFAULT_PAR for auto decided by encoder
    uint32_t                        llRc;                       // lowlantency rate control, [0, 5]

    uint32_t                        bitRate;                    // bps
    uint32_t                        initQp;                     // [0, 51], VMPP_ENC_DEFAULT_PAR for auto decided by encoder
    uint32_t                        vbvBufSize;                 // bits, VMPP_ENC_DEFAULT_PAR for auto decided by encoder
    uint32_t                        vbvMaxRate;                 // bps, VMPP_ENC_DEFAULT_PAR for auto decided by encoder
    int32_t                         intraQpDelta;               // VMPP_ENC_DEFAULT_PAR for auto decided by encoder [-12..12]
    uint32_t                        qpMinI;                     // Min qp for I frame, [0, 51], VMPP_ENC_DEFAULT_PAR for auto decided by encoder
    uint32_t                        qpMaxI;                     // Max qp for I frame, [0, 51], VMPP_ENC_DEFAULT_PAR for auto decided by encoder
    uint32_t                        qpMinPB;                    // Min qp for PB frame, [0, 51], VMPP_ENC_DEFAULT_PAR for auto decided by encoder
    uint32_t                        qpMaxPB;                    // Max qp for PB frame, [0, 51], VMPP_ENC_DEFAULT_PAR for auto decided by encoder
    uint32_t                        vbr;                        // mark if using vbr, VMPP_ENC_DEFAULT_PAR for auto decided by encoder
    float                           aqStrength;                 // Reduces blocking and blurring in flat and textured areas, [0.0, 3.0], VMPP_ENC_DEFAULT_PAR for auto decided by encoder, only support 2pass
    uint32_t                        P2B;                        // VMPP_ENC_DEFAULT_PAR for auto decided by encoder, set by default for hevc and only be valid when lookaheadDepth > 0 or gopSize = 1
    uint32_t                        bBPyramid;                  //  0: non-referece B Frames, 1: reference B Frames
    float                           maxFrameSizeMultiple;       // maximum multiple to average target frame size, VMPP_ENC_DEFAULT_PAR for auto decided by encoder
    int32_t                         maxFrameSize;               // max frame size, bit per pic, only valid in llrc mode, VMPP_ENC_DEFAULT_PAR for auto decided by encoder

    /* for roi range & roi map */
    vmppEncROIType                  roiType;
    uint32_t                        roiMapDeltaQpBlockUnit;     // 0: 64x64, 1: 32x32, 2: 16x16, 3: 8x8
    uint32_t                        roiMapQpDeltaVersion;       // reserved
    uint32_t                        maxBFrames;                 // max B frames for adaptive GOP decision, [0, 7], VMPP_ENC_DEFAULT_PAR for auto decided by encoder, need set gopSize to be 0.
    uint32_t                        hrd;                        // Hypothetical Reference Decoder model, [0, 1], 0 for disable.
    uint32_t                        pictureSkip;                // Frame All Skip Mode When Overflow, [0, 1], 0 for disable.
    uint32_t                        vfr;                        // variable frame rate, [0, 1], 0 for disable.
    uint32_t                        svcTLayers;                 // Temporal Layers for Scalable Video Coding, [0-4], 0 for disable.
    uint32_t                        alignmentEnable;            // input YUV data align 1 enable 0 disable
    vmppEncColorConversionType      colorConversionType;        // define color conversion type for RGB input convert to yuv, vmpp_RGBTOYUV_DEFAULT or VMPP_ENC_DEFAULT_PAR for auto decided by sg100 or sv100.
    uint32_t                        sliceSize;                  // Slice size in CTB/MB rows for multiSlice, 0 for disable, VMPP_ENC_DEFAULT_PAR for auto decided by encoder.

    /**
     * There are for LongTerm parameters
     * Must confirm that gopsize is 1
     */
    uint32_t                        ltrInterval;                // 0: disable ltr, 1~max frame num
    int32_t                         ltrQpDelta;                 // default 0
    uint32_t                        ltrRefGap;                  // the frame gap that references the ltr, 1~ltrInterval
    uint32_t                        openGop;                    // openGop, 1 for enable, 0 for disable, default 0, only for sv100
    uint32_t                        smartEnc;                   // smartEnc Mode, [1,5] for enable, 0 for disable, default 0, only for sv100
    uint32_t                        disableMMCO;                // disable h264 memory management control operation, only for sv100

    uint32_t                        inLoopDSRatio;              // in-loop downsample ratio for first pass, [0,1], 0=1/1 (no downsample), 1=1/2 downsample, VMPP_ENC_DEFAULT_PAR for auto decided by encoder, only 2pass support
    vmppEncAQMode                   aqMode;                     // aq mode, VMPP_ENC_DEFAULT_PAR for auto decided by encoder, only 2pass support
    float                           psyFactor;                  // weight of psycho-visual encoding, [0.0, 4.0], VMPP_ENC_DEFAULT_PAR for auto decided by encoder
    uint32_t                        rdoLevel;                   // rdoLevel can balance the quality and throughput, currently only HEVC/AV1 support, [1, 3], VMPP_ENC_DEFAULT_PAR for auto decided by encoder
    uint32_t                        enableRdoQuant;             // enable RDO quantization, 1 for enable, 0 for disable, VMPP_ENC_DEFAULT_PAR for auto decided by encoder, only HEVC/H264 support
    double                          qCompress;                  // qCompress sets the quantizer curve compression factor for lookahead. [0.0, 1.0], 0.0 => cbr, 1.0 => constant qp, VMPP_ENC_DEFAULT_PAR for auto decided by encoder, only 2pass support
    uint32_t                        bitRateBalanceLevel;        // set the matching degree between the encoding output bitrate and the target bitrate in simple or static scenarios, [0, 4], 0 for match strictly, [1, 4] higher for increasing deviation, default 0, only for sv100
    vmppEncPreprocessParams         preProcess;                 // Preprocess parameters for video encoder, only support rotation
    vmppEncRcMode                   rcMode;                     // Rate control mode, vmpp_ENC_RC_DEFAULT for auto decided by encoder
    uint32_t                        enableOutputCuInfo;         // enable output cu/frame info, 1 for enable, 0 for disable
} vmppEncVideoConfiguration;

/**
 * @brief parameters for JPEG encoder
 */
typedef struct vmppEncJPEGConfiguration {
    uint32_t                        codingWidth;                // Width of encoded image
    uint32_t                        codingHeight;               // Height of encoded image
    vmppPixelFormat                 frameType;                  // Input frame YUV / RGB format

    /* for user data */
    uint32_t                        comLength;                  // Length of COM header
    uint8_t *                       pCom;                       // Comment header pointer

    /* lossless mode */
    uint32_t                        losslessEn;                 // Enable lossless JPEG coding, 0: off, 1: on
    uint32_t                        predictMode;                // Prediction mode for lossless coding,
                                                                // valid range: [1, 7]. Only takes effect
                                                                // when losslessEn is enabled.
    uint32_t                        ptransValue;                // Point transform value for lossless coding,
                                                                // valid range: [0, 15]. Only takes effect
                                                                // when losslessEn is enabled.

    /* for q table */
    uint32_t                        qLevel;                     // Quantization level (1 - 101).
                                                                // 101 for user quantization table, qTableLuma and qTableChroma
                                                                // will take effect, otherwise not.
    uint8_t                         qTableLuma[64];             // Quantization table for luminance [64], overrides quantization
                                                                // level, zigzag order
    uint8_t                         qTableChroma[64];           // Quantization table for chrominance [64], overrides
                                                                // quantization level, zigzag order
    vmppEncPreprocessParams         preProcess;                 // Preprocess parameters for JPEG encoder, only support rotation
} vmppEncJPEGConfiguration;

/**
 * @brief parameters for creating encoder channel
 */
typedef struct vmppEncChannelParameters {
    char*                           encDevice;                  // video encoder device node name
    char*                           memDevice;                  // memory device node name
    vmppCodecType                   codecType;                  // @vmppCodecType
    uint32_t                        outbufNum;                  // output buffer count, (0~32], default is 4
    uint32_t                        enProfiling;                // enable profiling
    union {
        vmppEncJPEGConfiguration    jpegConfig;                 // parameters for jpeg encoder
        vmppEncVideoConfiguration   videoConfig;                // parameters for video encoder
    };
} vmppEncChannelParameters;

/**
 * @brief output option for receiving data from encoder
 */
typedef struct vmppEncOutputOptions {
    uint32_t                        reserved;
} vmppEncOutputOptions;


/**
 * @brief video encoder capability
 */
typedef struct vmppEncVideoCapability {
    uint32_t                        bitDepth;
    uint32_t                        maxWidth;
    uint32_t                        maxHeight;
    uint16_t                        minWidth;
    uint16_t                        minHeight;
    vmppVideoProfile                maxProFile;
    vmppVideoLevel                  maxLevel;
    vmppPixelFormat                 pixelFormats[VMPP_ENC_MAX_PIX_FMT_NUM];
} vmppEncVideoCapability;

typedef struct vmppEncMv {
    uint8_t                         refIdx;          // reference idx in reference list
    int16_t                         mvX;             // horiazontal motion in 1/4 pixel
    int16_t                         mvY;             // vertical motion in 1/4 pixel
} vmppEncMv;

typedef struct vmppEncCuInfo {
    uint8_t                         cuLocationX;     // cu x coordinate relative to CTU
    uint8_t                         cuLocationY;     // cu y coordinate relative to CTU
    uint8_t                         cuSize;          // cu size. 8/16/32/64
    uint8_t                         cuMode;          // cu mode. 0:INTER; 1:INTRA; 2:IPCM
    uint32_t                        costIntraSatd;   // satd cost of intra mode
    uint32_t                        costInterSatd;   // satd cost of inter mode
    uint8_t                         interPredIdc;    // only for INTER cu. prediction direction
                                                     // 0: by list0; 1: by list1; 2: bi-direction
    vmppEncMv                       mv[2];           // only for INTER cu. motion information
                                                     // mv[0] for list0 if it's valid; mv[1] for list1 if it's valid
} vmppEncCuInfo;

typedef struct vmppEncOutInfo {
    vmppEncCuInfo*                  cuInfo;          // buffer need to alloc to store parse cu info
    uint32_t                        frameSatd;       // total SATD cost for whole frame
    uint32_t                        intraMBCount;    // number of Intra MBs in thre encoded frame
    uint32_t                        interMBCount;    // number of Inter MBs in thre encoded frame
    int32_t                         averageMVX;      // average Motion Vector in X direction
    int32_t                         averageMVY;      // average Motion Vector in Y direction
    uint32_t                        totalCuNum;      // total cu count for a frame
} vmppEncOutInfo;

#ifdef __cplusplus
}
#endif /* __cplusplus */

/* clang-format on */

#endif /* __VMPP_ENC_DEFS_H__ */