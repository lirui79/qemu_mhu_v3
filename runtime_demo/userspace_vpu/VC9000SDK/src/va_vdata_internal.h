/*
 * Copyright (c) 2022, Vastai Tech. All rights reserved
 *
 * The information contained herein is confidential
 * property of Company. The user, copying, transfer or
 * disclosure of such information is prohibited except
 * by express written agreement with VASTAITECH.
 */

#ifndef VA_VDATA_INTERNAL_H_
#define VA_VDATA_INTERNAL_H_

#include "va_utils.h"
#include "vmpp_dec_defs.h"
#include "vmpp_enc_defs.h"
#include <pthread.h>

#ifndef VA_MIN
#define VA_MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef VA_MAX
#define VA_MAX(a, b) (((a) < (b)) ? (b) : (a))
#endif
#define VA_MAX_OUTPUT_BUFFER (64+8)
#define VA_MAX_PTS_BUFFER (VA_MAX_OUTPUT_BUFFER * 2)
#define VA_MAX_SEI_BUFFER (VA_MAX_OUTPUT_BUFFER * 4)

#define VA_ENC_MAX_OUTPUT_BUFFER 32
#define VA_ENC_DEF_OUTPUT_BUFFER 4
#define VA_DEC_DEFAULT_WIDTH 1920
#define VA_DEC_DEFAULT_HEIGHT 1080

typedef unsigned long long U64;

typedef int vaSendStreamCb(void* chn, uint64_t pts_index);

struct va_priv_buf {
    uint8_t *private_data;
    uint32_t used;
};

struct va_enc_buf {
    uint8_t *data;
    uint32_t size;
    uint32_t used;
};

struct va_pts_buf {
    int64_t pts;
    uint64_t flag;
};

struct va_sei_buf {
    uint8_t *data;
    uint32_t size;
    uint32_t used;
};

struct va_sei_params {
    vmppSEI *sei_data;
    uint32_t used;
    uint64_t pts;
    void *privateData;
};

/**
 * struct for video codec channel context
 */
struct va_dec_channel {
    vmppVersion version;
    void *codec_inst; /* codec instance, decoder/encoder for jpeg/h264/hevc */
    volatile uint32_t state;
    const void *cwl;       /* codec wrapper layer, dwl/ewl */
    void *private_context; /* private context for decoder/encoder */

    vmppDecChannelParameters params;

    uint32_t extraBufferused; /* extra buffer used */
    uint32_t is_ringbuffer;   /* ring buffer mode by default */
    uint32_t tile_by_tile;
    uint32_t frame_struct_size;

    uint32_t max_buf_num;
    struct va_priv_buf private_buffer[VA_MAX_OUTPUT_BUFFER];
    struct va_pts_buf frame_pts_buf[VA_MAX_PTS_BUFFER];
    pthread_mutex_t private_buffer_mutex;

    struct va_sei_buf sei_buffer[VA_MAX_SEI_BUFFER];
    struct va_sei_params va_sei_parameters[VA_MAX_SEI_BUFFER];
    pthread_mutex_t sei_buffer_mutex;

    volatile uint32_t receive_frame_cnt;
    volatile uint32_t release_frame_cnt;
};

typedef struct encJPEGConfiguration {
    /** extertal accessable parameters **/
    uint32_t codingWidth;      // Width of encoded image
    uint32_t codingHeight;     // Height of encoded image
    vmppPixelFormat frameType; // Input frame YUV / RGB format

    /* for user data */
    uint32_t comLength; // Length of COM header
    uint8_t *pCom;      // Comment header pointer

    /* lossless mode */
    uint32_t losslessEn;

    /** internal accessable parameters **/
    /* for q table */
    uint32_t qLevel;                // Quantization level (1 - 100)
    uint8_t qTableLuma[64];       // Quantization table for luminance [64], overrides quantization
                                  // level, zigzag order
    uint8_t qTableChroma[64];       // Quantization table for chrominance [64], overrides
                                  // quantization level, zigzag order

    uint32_t predictMode;
    uint32_t ptransValue;

    uint32_t qpmin;
    uint32_t qpmax;
    int32_t fixedQP; // fix qp no RC
    //    vmppEncRateControlMode rcMode;
    uint32_t rotation;
} encJPEGConfiguration;

/* -------------------- Structure -------------------- */
typedef struct encGopPicConfig {
    uint32_t poc;          // picture order count within a GOP
    int32_t qpOffset;      // QP offset
    double qpFactor;       // QP Factor
    int32_t temporalID;    // temporal layer ID
    uint32_t nonReference; // frame not used for future reference
    uint32_t numRefPics;   // the number of reference pictures kept for this picture [0,
                           // VMPP_MAX_REF_FRAMES]
} encGopPicConfig;

typedef struct encGopPicSpecialConfig {
    uint32_t poc;          // picture order count within a GOP
    int32_t qpOffset;      // QP offset
    double qpFactor;       // QP Factor
    int32_t temporalID;    // temporal layer ID
    uint32_t nonReference; // frame not used for future reference
    uint32_t numRefPics;   // the number of reference pictures kept for this picture [0,
                           // VMPP_MAX_REF_FRAMES]
    int32_t ltr; // index of the long-term referencr frame [0, VMPP_MAX_LTR_FRAMES]. 0 for not LTR
    int32_t offset;      // offset of the special pics, relative to start of ltrInterval
    int32_t interval;    // interval between two pictures using LTR as referencr picture or interval
                         // between two pictures coded as special frame
    int32_t shortChange; // only change short term coding parameter. 0 - not change, 1 - change
} encGopPicSpecialConfig;

typedef struct encGopConfig {
    encGopPicConfig *pGopPicCfg; // Pointer to an array containing all used VCEncGopPicConfig
    uint8_t
        size; // the number of VCEncGopPicConfig pointed by pGopPicCfg [0, MAX_GOP_PIC_CONFIG_NUM]
    uint8_t vasPlaceholderSize[3];
    uint8_t id; // the index of VCEncGopPicConfig in pGopPicCfg used by current picture [0, size-1]
    uint8_t vasPlaceholderID[3];
    uint8_t idNext; // the index of VCEncGopPicConfig in pGopPicCfg used by next picture [0, size-1]
    uint8_t vasPlaceholderIDNext[3];
    uint8_t specialSize; // number of special configuration [0, MAX_GOP_PIC_CONFIG_NUM]
    uint8_t vaPlaceholderSpecialSize[3];
    int32_t deltaPocToNext; // the difference between poc of next picture and current picture
    encGopPicSpecialConfig
        *pGopPicSpecialCfg; // Pointer to an array containing all used VCEncGopPicSpecialConfig
    uint8_t ltrcnt;         // Number of long-term ref pics used [0, VMPP_MAX_LTR_FRAMES]
    uint8_t vasPlaceholderLtrcnt[3];
    uint32_t ltrIdx[VMPP_ENC_MAX_LTR_FRAMES];
    int32_t idrInterval;
    int32_t gdrDuration;
    int32_t firstPic;
    int32_t lastPic;
    vmppRational outputRate; // Output frame rate
    vmppRational inputRate;  // Input frame rate
    int32_t gopLowdelay;

    /** !!!NOTEs: interlace Interlace Supporting for HEVC
     *
     * Interlace is supported for HEVC by adding proper SEI information to indicate the field
     * structures.
     *
     * To encode as interlace, need to set VCEncGopConfig.interlacedFrame as 1. and when feed
     * input for each field, the VCEncCodingCtrl.fieldOrder will be used to check which field
     * of current picture is belong to. When fieldOrder is 0 (bottom field first), the even
     * number of VCEncIn.poc is bottom field.
     *
     * Currently, only support when gopSize=1.
     */
    int32_t interlacedFrame;
    uint8_t gopCfgOffset[VMPP_ENC_MAX_GOP_SIZE + 1];
    uint8_t vasPlaceholderGopCfgOffset[3];
    encGopPicConfig
        *pGopPicCfgPass1; // Pointer to an array containing all used encGopPicConfig, used for pass1
    encGopPicConfig
        *pGopPicCfgPass2; // Pointer to an array containing all used encGopPicConfig, used for pass2
} encGopConfig;

typedef struct encVideoConfiguration {
    /** extertal accessable parameters **/
    vmppVideoProfile profile;

    /** !!!NOTEs: Automatically Level Detection
     *
     * When level in VCEncConfig.level is specified as 0, the actual level encoded in the SPS
     * will be selected according to the picture size, frame rate, bit rate and cpb size. The
     * smallest level which can fit current setting will be used. Note that only initial bit rate /
     * cpb size set before VCEncStrmStart takes effect. When no level fits, just select highest
     * level (6.2), emitting a warning. This is to support resolution like 8192x8192, 8192x8640,
     * etc.
     *
     * Currently, only HEVC and H264 are valid for automatically level selection. */
    vmppVideoLevel level;
    uint32_t width;  // Encoded picture width in pixels, multiple of 2
    uint32_t height; // Encoded picture height in pixels, multiple of 2

    /** !!!NOTEs: frameRate
     *
     * The frameRate.numerator is within stream time scale, [1~1048575]
     * Maximum frame rate is frameRate.numerator/frameRate.denominator in frames/second.
     * The actual frame rate will be defined by timeIncrement of encoded pictures,
     * [1~frameRate.numerator] */
    vmppRational frameRate;
    uint32_t bitDepthLuma;
    uint32_t bitDepthChroma;
    uint32_t gopSize;        // sequence level GOP size, set gopSize=0 for adaptive GOP size.
    uint32_t gdrDuration;    // canada no_mcu commit 174aeb4d4a3c0ef09660f9c9dda0b1534c653e6a
    uint32_t lookaheadDepth; // for two pass
    // vmppEncRateControlMode rcMode;
    // vmppEncQualityMode  qualityMode;
    vmppEncTuneType tune;
    uint32_t keyInt; // IDR interval

    int32_t crf;
    uint32_t crfFracInt;
    uint32_t cqp;
    uint32_t llRc;

    uint32_t bitRate;
    uint32_t initQp;
    uint32_t vbvBufSize;
    uint32_t vbvMaxRate;
    int32_t intraQpDelta;
    uint32_t qpMinI;
    uint32_t qpMaxI;
    uint32_t qpMinPB;
    uint32_t qpMaxPB;
    uint32_t vbr;
    float aq_strength;
    // uint32_t enableROI;
    uint32_t P2B; // canada no_mcu commit 174aeb4d4a3c0ef09660f9c9dda0b1534c653e6a
    uint32_t bBPyramid;
    float maxFrameSizeMultiple; // maximum multiple to average target frame size
    int32_t  maxFrameSize;           /* max frame size, only valid in llrc mode*/

    /** internal accessable parameters **/

    /** !!!NOTEs: Amount of reference frame buffers, [0..8]
     *
     * 0 = only I frames are encoded.
     * 1 = gop size is 1 and interlacedFrame =0,
     * 2 = gop size is 1 and interlacedFrame =1,
     * 2 = gop size is 2 or 3,
     * 3 = gop size is 4,5,6, or 7,
     * 4 = gop size is 8
     * 8 = gop size is 8 svct hirach 7B+1p 4 layer, only libva support this config*/
    uint32_t refFrameAmount;
    uint32_t strongIntraSmoothing; // 0 = Normal smoothing, 1 = Strong smoothing

    /** !!!NOTEs: rfc Reference Frame Compression
     *
     * Reference frame compression can save bandwidth by saving the reference frame in a lossless
     * compressed format. Consider the worst case for compression ratio and keep the data lossless,
     * the storage memeory is not saved. Intead, some small buffer to save the compression meta data
     * will be used.
     *
     * Currently, only mode 0 (disable) and 3 (enable for all components) are supported.
     * - 0 = Disable Compression
     * - 1 = Only Enable Luma Compression (not support)
     * - 2 = Only Enable Chroma Compression (not support)
     * - 3 = Enable Both Luma and Chroma Compression */
    uint32_t compressor;

    /**
     * only HEVC supported interlace encoding by insert proper SEI information.
     * 0 = progressive frame
     * 1 = interlace frame
     */
    uint32_t interlacedFrame;

    uint32_t enableOutputCuInfo;
    uint32_t enableOutputCtbBits; // 1 to enable CTB bits output
    uint32_t enableSSIM;          // Enable/Disable SSIM calculation
    uint32_t enablePSNR;          // Enable/Disable PSNR calculation
    uint32_t maxTLayers;          // max number Temporal layers

    /** !!!NOTEs: rdoLevel Control RDO Level
     *
     * RDO Level can balance the quality and throughput. Currently only HEVC/AV1 support this
     * feature.
     *
     * - EWLHwConfig_t.progRdoEnable indicate if current configure support this feature or not.
     * - VCEncConfig.rdoLevel control how many effort will be used to mode selection;
     * - EWLHwConfig_t.dynamicRdoSupport indicate if dynamic RDO is supported or not.
     * - VCEncCodingCtrl.enableDynamicRdo control if enable dynamic RDO Level selection;
     *
     * control RDO hw runtime effort level, balence between quality and throughput performance,
     * [0..2] 0 = RDO run 1x cadidates 1 = RDO run 2x cadidates 2 = RDO run 3x cadidates
     */
    uint32_t rdoLevel;
    uint32_t p010RefEnable; // enable P010 tile-raster format for reference frame buffer
    uint32_t picOrderCntType;
    uint32_t log2MaxPicOrderCntLsb;
    uint32_t log2MaxFrameNum;

    vmppChromaFormat codedChromaFmt;
    uint32_t enableRdoQuant;
    uint32_t userCoreID;

    uint32_t preset;

    vmppEncROIType roiType;
    uint32_t roiMapDeltaQpEnable;
    uint32_t roiMapDeltaQpBlockUnit;
    uint32_t roiMapQpDeltaVersion;
    uint32_t roiCuCtrlVersion;
    uint32_t maxBFrames;
    uint32_t hrd;
    uint32_t pictureSkip;
    uint32_t vfr;
    uint32_t svcTLayers;
    uint32_t alignmentEnable;
    uint32_t sliceSize;

    /**
     * There are for LongTerm parameters
     * Must confirm that gopsize is 1
     */
    uint32_t ltrInterval;
    int ltrQpDelta;
    uint32_t ltrRefGap;

    uint32_t rotation;
    uint32_t openGop;
    uint32_t smartEnc;
    uint32_t disableMMCO;

    uint32_t inLoopDSRatio;
    uint32_t aq_mode;
    float psyFactor;
    double qCompress;
    uint32_t bitRateBalanceLevel;
    double iQpFactor;
} encVideoConfiguration;

typedef struct encChannelParameters {
    char*                           encDevice;                    // video encoder device node name
    char*                           memDevice;                    // memory device node name
    vmppCodecType                   codecType; // @vmppCodecType
    uint32_t                        outbufNum;
    uint32_t                        enProfiling;
    union {
        encJPEGConfiguration        jpegConfig;
        encVideoConfiguration       videoConfig;
    };
} encChannelParameters;

/**
 * struct for video codec channel context
 */
struct va_enc_channel {
    void                          *codec_inst; /* codec instance, decoder/encoder for jpeg/h264/hevc */
    volatile uint32_t              state;
    encChannelParameters           params;
    void                          *private_context; /* private context for decoder/encoder */

    uint32_t                       outbufNum;
    struct va_enc_buf              enc_out_buffer[VA_ENC_MAX_OUTPUT_BUFFER];
    uint32_t                       outbufIdleNum;
    uint32_t                       outbufMallocNum;
    pthread_mutex_t                enc_out_buffer_mutex;
};

#endif /* VA_VDATA_INTERNAL_H_ */
