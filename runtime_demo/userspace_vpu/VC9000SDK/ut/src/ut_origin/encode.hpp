#define MCUDEMO
// #include "va_utils.h"
#include "vmpp_enc_defs.h"
#include <pthread.h>
// #include "hevcencapi.h"

#define MAX_CUTREE_DEPTH 64
#define MAX_GOP_SIZE 16
#define MAX_DELAY_NUM (MAX_CORE_NUM + MAX_CUTREE_DEPTH)
#define MAX_SEI_BUFFER_NUM (MAX_DELAY_NUM * 2)
#define MAX_EWL_MEM_NUM (MAX_DELAY_NUM * 3)

#define VA_ENC_MAX_OUTPUT_BUFFER 32

#define ALLIGN(size, alignment) (((size) + ((alignment)-1)) & (~((alignment)-1)))

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


// typedef struct {
//     EWLLinearMem_t mem;
//     uint32_t used;
// } ewlMemory;

typedef struct EncSEIBuffer {
    uint8_t *data;
    uint32_t size;
    uint32_t used;
} EncSEIBuffer;

// typedef struct EncInputBuffer {
//     EWLLinearMem_t mem;
//     vmppMemoryType memType;
//     uint32_t used;
//     int32_t index;
//     int32_t number;
//     int64_t pts;
//     int64_t timebaseNum;
//     int64_t timebaseDen;
//     uint32_t sent2Encoder;
//     vmppEncPictureROI roi[VMPP_ENC_MAX_ROI_NUM];
//     uint32_t extSEICount;
//     ExternalSEI *extSEI;
//     uint32_t extSEIBufferSize;
//     uint8_t *encodedSEI;
//     uint32_t encodedSEIBufferSize;
//     uint32_t prefixSeiSize;
//     uint32_t suffixSeiSize;
//     uint32_t forceIDR;
//     uint32_t gopChangeIdr; // for insertIDR
//     uint32_t width;
//     uint32_t height;
//     vmppPixelFormat format;
//     uint32_t newResolution;
//     uint32_t lumaSize;
//     uint32_t chromaSize;
//     vmppEncROIType roiType;
//     EWLLinearMem_t *roiMapDeltaQpMem;
//     uint32_t roiMapDeltaQpSize;
//     EWLLinearMem_t *roimapCuCtrlInfoMem;
//     uint32_t roimapCuCtrlInfoSize;
//     EWLLinearMem_t *roimapCuCtrlIndexMem;
//     uint32_t roimapCuCtrlIndexSize;
//     uint32_t updateBitRate;
//     vmppRational updateFrameRate;
//     uint32_t resetStream;
//     uint32_t svcTemporalId;
// } EncInputBuffer;

// struct video_encoder_private_context {
//     VCEncConfig cfg;
//     VCEncCodingCtrl codingCfg;
//     VCEncRateCtrl rcCfg;
//     VCEncPreProcessingCfg preProcCfg;
//     VCEncIn *encIn;
//     VCEncOut *encOut;
//     EWLLinearMem_t mcuParamMem;
//     VCEncGopPicConfig gopPicCfg_tmp[MAX_GOP_PIC_CONFIG_NUM];
//     VCEncGopPicConfig gopPicCfgPass2_tmp[MAX_GOP_PIC_CONFIG_NUM];
//     VCEncGopPicSpecialConfig gopPicSpecialCfg_tmp[MAX_GOP_SPIC_CONFIG_NUM];
//     VCEncGopPicConfig *gopPicCfg;
//     VCEncGopPicConfig *gopPicCfgPass2;
//     VCEncGopPicSpecialConfig *gopPicSpecialCfg;
//     EncInputBuffer pictureMem[MAX_DELAY_NUM];
// #ifdef ROIMAP_4_HEVC2PASS_WORKAROUND
//     EWLLinearMem_t roiMapDeltaQpMemFactory[MAX_DELAY_NUM];
// #endif
//     ewlMemory roiMemFactory[MAX_EWL_MEM_NUM];
//     EWLLinearMem_t outbufMemFactory[MAX_CORE_NUM];
//     uint32_t parametersSetReady;
//     uint32_t parametersSetOutputed;
//     uint8_t *parametersSet;
//     uint32_t parametersSetSize;
//     // uint32_t lumaSize;
//     // uint32_t chromaSize;
//     int32_t nextGopSize;
//     VCEncPictureCodingType nextCodingType;
//     adapGopCtr agop;
//     int32_t gopLowdelay;
//     uint32_t roiMapDeltaQpEnable;
//     uint32_t roiMapDeltaQpBlockUnit;
//     int32_t inputPictureCount;
//     int32_t outputPictureCount;
//     uint32_t flushing;
//     int32_t streamBufNum;    // tb.streamBufNum = cml->streamBufChain ? 2 : 1;
//     int32_t pictureEncCount; // tb->picture_enc_cnt
//     int32_t frameDelay;      // tb->frame_delay
//     int32_t parallelCoreNum; // tb.parallelCoreNum
//     int32_t bufferCnt;       // tb.buffer_cnt
//     int32_t currInsertedNum; // current inserted number
//     EWLLinearMem_t extSRAMMemFactory[MAX_CORE_NUM];
//     uint32_t extSramLumBwdSize;
//     uint32_t extSramLumFwdSize;
//     uint32_t extSramChrBwdSize;
//     uint32_t extSramChrFwdSize;
//     VCEncVideoCodecFormat codecFormat;
//     int32_t vFrames;
//     uint32_t eos;
//     VCEncRet lastVRet;
//     vmppFrame lastInputFrame;
//     vmppEncPictureROI lastROI[VMPP_ENC_MAX_ROI_NUM];
//     EncSEIBuffer seiBuffer[MAX_SEI_BUFFER_NUM];
//     double psnr_total[3];
//     int32_t curIPFramePoc;       // for insertIDR
//     int32_t lastIPFramePoc;      // for insertIDR
//     int32_t insertIdrPicCnt;     // for insertIDR
//     VCEncIn encInLast;           // for insertIDR
//     vmppFrame lastEncDummyFrame; // for dynamic resolution, only used to mark width & height of last
//                                  // encoded frame
//     int32_t newResPicCnt;        // for dynamic resolution
//     vmppRuntimeInstance rt;
//     int64_t numberBase;
//     uint32_t internalFlushing;
//     int32_t firstFrameNumberOfNewRes;

//     uint32_t hashLenMismatch;
//     // vmppEncROIType lastROIType;
//     uint32_t roiMapVersion;
//     vmppEncROIType roiType;

//     int32_t longterm_enable;
// };



// ### for va_enc_channel ###
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
    vmppDevice device;       // hardware device handle
    vmppCodecType codecType; // @vmppCodecType
    uint32_t outbufNum;
    uint32_t enProfiling;
    vmppCoreMode coreMode;
    union {
        encJPEGConfiguration jpegConfig;
        encVideoConfiguration videoConfig;
    };
} encChannelParameters;

struct va_enc_buf {
    uint8_t *data;
    uint32_t size;
    uint32_t used;
};

struct va_enc_channel {
    void *codec_inst; /* codec instance, decoder/encoder for jpeg/h264/hevc */
    volatile uint32_t state;
    encChannelParameters params;
    void *private_context; /* private context for decoder/encoder */

    uint32_t outbufNum;
    struct va_enc_buf enc_out_buffer[VA_ENC_MAX_OUTPUT_BUFFER];
    uint32_t outbufIdleNum;
    uint32_t outbufMallocNum;
    pthread_mutex_t enc_out_buffer_mutex;
};