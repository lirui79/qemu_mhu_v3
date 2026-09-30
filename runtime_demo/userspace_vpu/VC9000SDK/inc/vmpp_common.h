/*
 * Copyright (C) 2022-2023 VASTAI Technologies Co., Ltd. All Rights Reserved.
 */

/**
 * !@file   vmpp_common.h
 * !@date   2022-06-09
 * !@brief  This file contains the constant, enumeration and structure definitions for decoder &
 * encoder.
 */

#ifndef __VMPP_COMMON_H__
#define __VMPP_COMMON_H__

#include <stddef.h>
#include <stdint.h>

/* clang-format off */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* -------------------- Macro -------------------- */
#if defined _WIN32
#define VMPP_DLL_IMPORT         __declspec(dllimport)
#define VMPP_DLL_EXPORT         __declspec(dllexport)
#else
#if __GNUC__ >= 4
#define VMPP_DLL_IMPORT         __attribute__((visibility("default")))
#define VMPP_DLL_EXPORT         __attribute__((visibility("default")))
#else
#define VMPP_DLL_IMPORT
#define VMPP_DLL_EXPORT
#endif
#endif
#define VMPP_API                VMPP_DLL_EXPORT
#define VMPP_API_IMPORT         VMPP_DLL_IMPORT
#define VMPP_MAX_VIDEO_PLANE    3
#define VMPP_MIN_TIMEOUT_MS     4000
#define VMPP_MAX_ENC_NALS       16
#define VMPP_NORM_ENCODER 760320 //(176*144*30)
#define VMPP_SILVER_QUALITY 50
#define VMPP_PRE_OCCUPY_LOAD ((uint64_t)1920*1080*30*VMPP_SILVER_QUALITY/(VMPP_NORM_ENCODER))

/* -------------------- Function Pointer -------------------- */
typedef void    (*vmppLogCallback)(const void *pUser, int level, const char *module, const char *file, const char *func, int line, const char *msg);

/* -------------------- Type -------------------- */
typedef size_t    vmppAddr;
typedef uint64_t  vmppDevAddr;

typedef void *  vmppChannel;
typedef void *  vmppFuncPtr;
typedef void *  vmppHandle;
#ifdef _MSC_VER
typedef void *  vmppDevice;
#else
typedef int     vmppDevice;
#endif

/* -------------------- Enumeration -------------------- */
/**
 * @brief result definition
 */
typedef enum vmppResult {
    vmpp_RSLT_OK                            = 0,

    vmpp_RSLT_DEC_INPUT_AGAIN               = 10,   // Result from 'vmppDecSendStream' : current stream should be input again, while NO output frame is ready
    vmpp_RSLT_DEC_AV1_NOTSHOW               = 11,   // Result from 'vmppDecSendStream' : current stream decode frame is not show now for av1 decoder
    vmpp_RSLT_DEC_DISCARD_FRAME             = 12,   // Result from 'vmppDecSendStream' : current stream decode frame need to be discarded

    vmpp_RSLT_ENC_INPUT_INSERTED            = 20,   // Result from 'vmppEncEncodeFrame' : current frame has been accepted by encoder , but NO output stream is ready
    vmpp_RSLT_ENC_FLUSH                     = 21,   // Result from 'vmppEncEncodeFrame' : NO real output stream is ready, but user may need to release frame according to busAddress in this stream
    vmpp_RSLT_ENC_AGAIN                     = 22,   // Result from 'vmppEncEncodeFrame' : current frame should be input again, but an output stream is ready
    vmpp_RSLT_ENC_AGAIN_WITH_NO_OUTPUT      = 23,   // Result from 'vmppEncEncodeFrame' : current frame should be input again, while NO output stream is ready

    /* warning */
    vmpp_RSLT_WARN_MORE_DATA                = 100,
    vmpp_RSLT_WARN_EOS                      = 101,
    vmpp_RSLT_WARN_ABORTED                  = 102,
    vmpp_RSLT_WARN_REPEAT_OPERATION         = 103,
    vmpp_RSLT_WARN_FRAME_SKIPPED            = 104,
    vmpp_RSLT_WARN_STREAM_PROCESSED         = 105,
    vmpp_RSLT_WARN_FRAME_NOT_EXIST          = 106,
    vmpp_RSLT_WARN_STREAM_NOT_EXIST         = 107,

    /* common error */
    vmpp_RSLT_ERR_INVALID_PARAMS            = -1,
    vmpp_RSLT_ERR_NOT_INITIALIZED           = -2,
    vmpp_RSLT_ERR_INVALID_DATA              = -3,
    vmpp_RSLT_ERR_NO_MEMORY                 = -4,
    vmpp_RSLT_ERR_UNSUPPORTED               = -5,
    vmpp_RSLT_ERR_INVALID_STATE             = -6,
    vmpp_RSLT_ERR_HW_TIMEOUT                = -7,
    vmpp_RSLT_ERR_SYS_ERROR                 = -8,
    vmpp_RSLT_ERR_UNKNOWN                   = -9,
    vmpp_RSLT_ERR_NO_BUFFER                 = -10,
    vmpp_RSLT_ERR_NO_PTSBUF                 = -11,
    vmpp_RSLT_RUNTIME_INVALID               = -12,
    vmpp_RSLT_MC_NO_RESOURCE                = -13,
    vmpp_RSLT_ERR_DMA                       = -14,

    /* decoder error */
    vmpp_RSLT_ERR_DEC_BUS                   = -100,
    vmpp_RSLT_ERR_DEC_DWL                   = -101,
    vmpp_RSLT_ERR_DEC_RUNTIME               = -102,
    vmpp_RSLT_ERR_DEC_INIT                  = -103,
    vmpp_RSLT_ERR_DEC_ABORTED               = -104,
    vmpp_RSLT_ERR_GET_INFO                  = -105,
    vmpp_RSLT_ERR_ALLOC_CHANNEL             = -106,
    vmpp_RSLT_ERR_INVALID_PARAM_SET         = -107,
    vmpp_RSLT_ERR_DEC_SHARED_DMABUF         = -108,

    /* encoder error */
    vmpp_RSLT_ERR_ENC_BUS                   = -200,
    vmpp_RSLT_ERR_ENC_EWL                   = -201,
    vmpp_RSLT_ERR_ENC_INIT                  = -202,
    vmpp_RSLT_ERR_ENC_SET_PIC_SIZE          = -203,
    vmpp_RSLT_ERR_ENC_SEND_FRAME            = -204,
    vmpp_RSLT_ERR_ENC_INIT_GOP              = -205,
    vmpp_RSLT_ERR_ENC_CODING_CTRL           = -206,
    vmpp_RSLT_ERR_ENC_DRIVER_MISMATCH       = -207,
    vmpp_RSLT_ERR_ENC_RECOVERY              = -208,
    vmpp_RSLT_ERR_ENC_SHARED_DMABUF         = -209,
} vmppResult;

/**
 * @brief log level
 */
typedef enum vmppLogLevel {
    vmpp_LOG_TRACE,
    vmpp_LOG_DEBUG,
    vmpp_LOG_INFO,
    vmpp_LOG_WARN,
    vmpp_LOG_ERROR,
    vmpp_LOG_FATAL
} vmppLogLevel;

/**
 * @brief decoder/encoder state
 */
typedef enum vmppState {
    vmpp_ST_NONE,
    vmpp_ST_READY,
    vmpp_ST_RUNNING,
    vmpp_ST_ERROR,
    vmpp_ST_STOPPING,
    vmpp_ST_STOPPED
} vmppState;

/**
 * @brief codec type
 */
typedef enum vmppCodecType {
    vmpp_CODEC_DEC_JPEG                     = 0,            // Decoder for JPEG
    vmpp_CODEC_DEC_H264                     = 1,            // Decoder for H264
    vmpp_CODEC_DEC_HEVC                     = 2,            // Decoder for HEVC
    vmpp_CODEC_DEC_AV1                      = 3,            // Decoder for AV1
    vmpp_CODEC_DEC_VP9                      = 4,            // Decoder for VP9
    vmpp_CODEC_DEC_AVS2                     = 5,            // Decoder for AVS2

    vmpp_CODEC_ENC_JPEG                     = 100,          // Encoder for JPEG
    vmpp_CODEC_ENC_H264                     = 101,          // Encoder for H264
    vmpp_CODEC_ENC_HEVC                     = 102,          // Encoder for HEVC
    vmpp_CODEC_ENC_AV1                      = 103           // Encoder for HEVC
} vmppCodecType;

/**
 * @brief source mode, currently, only 'frame' mode is supported
 */
typedef enum vmppSourceMode {
    vmpp_SRC_FRAME,
} vmppSourceMode;

/**
 * @brief memory type
 */
typedef enum vmppMemoryType {
    vmpp_MEM_DEVICE,
    vmpp_MEM_HOST,
    vmpp_MEM_FLUSH,
} vmppMemoryType;

/**
 * @brief pixel format
 */
typedef enum vmppPixelFormat {
    vmpp_PIX_FMT_NONE = -1,
    vmpp_PIX_FMT_YUV420P,                                   // planar YUV 4:2:0,    12bpp, 1 Cr & Cb sample per 2x2 Y samples
    vmpp_PIX_FMT_YUV444P,                                   // planar YUV 4:4:4,    24bpp, 1 Cr & Cb sample per 1x1 Y samples
    vmpp_PIX_FMT_YUV422P,                                   // planar YUV 4:2:2,    16bpp, 1 Cr & Cb sample per 2x1 Y samples
    vmpp_PIX_FMT_YUV420P9,                                  // planar YUV 4:2:0,    13.5bpp, 1 Cr & Cb sample per 2x2 Y samples
    vmpp_PIX_FMT_YUV422P9,                                  // planar YUV 4:2:2,    18bpp, 1 Cr & Cb sample per 2x1 Y samples
    vmpp_PIX_FMT_YUV444P9,                                  // planar YUV 4:4:4,    27bpp, 1 Cr & Cb sample per 1x1 Y samples
    vmpp_PIX_FMT_YUV420P10,                                 // planar YUV 4:2:0,    15bpp, 1 Cr & Cb sample per 2x2 Y samples
    vmpp_PIX_FMT_YUV422P10,                                 // planar YUV 4:2:2,    20bpp, 1 Cr & Cb sample per 2x1 Y samples
    vmpp_PIX_FMT_YUV444P10,                                 // planar YUV 4:4:4,    30bpp, 1 Cr & Cb sample per 1x1 Y samples
    vmpp_PIX_FMT_YUV420P12,                                 // planar YUV 4:2:0,    18bpp, 1 Cr & Cb sample per 2x2 Y samples
    vmpp_PIX_FMT_YUV422P12,                                 // planar YUV 4:2:2,    24bpp, 1 Cr & Cb sample per 2x1 Y samples
    vmpp_PIX_FMT_YUV444P12,                                 // planar YUV 4:4:4,    36bpp, 1 Cr & Cb sample per 1x1 Y samples
    vmpp_PIX_FMT_NV12,                                      // planar YUV 4:2:0,    12bpp, 1 plane for Y and 1 plane for the UV components, which are interleaved (first byte U and the following byte V)
    vmpp_PIX_FMT_NV21,                                      // as above, but U and V bytes are swapped
    vmpp_PIX_FMT_GRAY8,                                     // Y, a.k.a. YUV400,    8bpp
    vmpp_PIX_FMT_GRAY9,                                     // Y, a.k.a. YUV400,    9bpp
    vmpp_PIX_FMT_GRAY10,                                    // Y, a.k.a. YUV400,    10bpp
    vmpp_PIX_FMT_GRAY12,                                    // Y, a.k.a. YUV400,    12bpp
    vmpp_PIX_FMT_RGB24,                                     // packed RGB 8:8:8,    24bpp, RGBRGB...
    vmpp_PIX_FMT_BGR24,                                     // packed RGB 8:8:8,    24bpp, BGRBGR...
    vmpp_PIX_FMT_ARGB,                                      // packed ARGB 8:8:8:8, 32bpp, ARGBARGB...
    vmpp_PIX_FMT_RGBA,                                      // packed RGBA 8:8:8:8, 32bpp, RGBARGBA...
    vmpp_PIX_FMT_ABGR,                                      // packed ABGR 8:8:8:8, 32bpp, ABGRABGR...
    vmpp_PIX_FMT_BGRA,                                      // packed BGRA 8:8:8:8, 32bpp, BGRABGRA...

    vmpp_PIX_FMT_YUV420_PLANAR_10BIT_LE,                    // YUV420 10 bit (1 pixel in 2 bytes) planar CbCr YYYY... UUUU... VVVV..., only support enc.
    vmpp_PIX_FMT_YUV420_PLANAR_10BIT_I010,                  // YUV420 10 bit (1 pixel in 2 bytes) semiplanar CbCr YYYY... UVUVUV..., different with P010, only support dec.
    vmpp_PIX_FMT_YUV420_PLANAR_10BIT_P010,                  // YUV420 10 bit (1 pixel in 2 bytes) semiplanar CbCr YYYY... UVUVUV...
    vmpp_PIX_FMT_RGBA10                                     // packed RGBA 10:10:10:2, 32bpp, RGBARGBA..., only for sg100
} vmppPixelFormat;

/**
 * @brief chroma format
 */
typedef enum vmppChromaFormat {
    vmpp_CHROMA_FMT_NONE = -1,
    vmpp_CHROMA_FMT_400,
    vmpp_CHROMA_FMT_411,
    vmpp_CHROMA_FMT_420,
    vmpp_CHROMA_FMT_422,
    vmpp_CHROMA_FMT_440,
    vmpp_CHROMA_FMT_444,
} vmppChromaFormat;

/**
 * @brief video profile
 */
typedef enum vmppVideoProfile {
    /* For HEVC */
    vmpp_VIDEO_PRFL_HEVC_MAIN               = 0,
    vmpp_VIDEO_PRFL_HEVC_MAIN_STILL_PICTURE = 1,
    vmpp_VIDEO_PRFL_HEVC_MAIN_10            = 2,
    vmpp_VIDEO_PRFL_HEVC_MAIN_REXT          = 3,

    /* For H264 */
    vmpp_VIDEO_PRFL_H264_BASELINE           = 9,
    vmpp_VIDEO_PRFL_H264_MAIN               = 10,
    vmpp_VIDEO_PRFL_H264_HIGH               = 11,
    vmpp_VIDEO_PRFL_H264_HIGH_10            = 12,

    /* Reserved For AV1 */
    vmpp_VIDEO_PRFL_AV1_MAIN                = 0,
    vmpp_VIDEO_PRFL_AV1_HIGH                = 1,
    vmpp_VIDEO_PRFL_AV1_PROFESSIONAL        = 2,
} vmppVideoProfile;

/**
 * @brief video level
 */
typedef enum vmppVideoLevel {
    /* For HEVC */
    vmpp_VIDEO_LVL_HEVC_1                   = 30,
    vmpp_VIDEO_LVL_HEVC_2                   = 60,
    vmpp_VIDEO_LVL_HEVC_2_1                 = 63,
    vmpp_VIDEO_LVL_HEVC_3                   = 90,
    vmpp_VIDEO_LVL_HEVC_3_1                 = 93,
    vmpp_VIDEO_LVL_HEVC_4                   = 120,
    vmpp_VIDEO_LVL_HEVC_4_1                 = 123,
    vmpp_VIDEO_LVL_HEVC_5                   = 150,
    vmpp_VIDEO_LVL_HEVC_5_1                 = 153,
    vmpp_VIDEO_LVL_HEVC_5_2                 = 156,
    vmpp_VIDEO_LVL_HEVC_6                   = 180,
    vmpp_VIDEO_LVL_HEVC_6_1                 = 183,
    vmpp_VIDEO_LVL_HEVC_6_2                 = 186,

    /* For H264 */
    vmpp_VIDEO_LVL_H264_1                   = 10,
    vmpp_VIDEO_LVL_H264_1_b                 = 99,
    vmpp_VIDEO_LVL_H264_1_1                 = 11,
    vmpp_VIDEO_LVL_H264_1_2                 = 12,
    vmpp_VIDEO_LVL_H264_1_3                 = 13,
    vmpp_VIDEO_LVL_H264_2                   = 20,
    vmpp_VIDEO_LVL_H264_2_1                 = 21,
    vmpp_VIDEO_LVL_H264_2_2                 = 22,
    vmpp_VIDEO_LVL_H264_3                   = 30,
    vmpp_VIDEO_LVL_H264_3_1                 = 31,
    vmpp_VIDEO_LVL_H264_3_2                 = 32,
    vmpp_VIDEO_LVL_H264_4                   = 40,
    vmpp_VIDEO_LVL_H264_4_1                 = 41,
    vmpp_VIDEO_LVL_H264_4_2                 = 42,
    vmpp_VIDEO_LVL_H264_5                   = 50,
    vmpp_VIDEO_LVL_H264_5_1                 = 51,
    vmpp_VIDEO_LVL_H264_5_2                 = 52,
    vmpp_VIDEO_LVL_H264_6                   = 60,
    vmpp_VIDEO_LVL_H264_6_1                 = 61,
    vmpp_VIDEO_LVL_H264_6_2                 = 62,

    /* For AV1 */
    vmpp_VIDEO_LVL_AV1_2_0                  = 0,
    vmpp_VIDEO_LVL_AV1_2_1                  = 1,
    vmpp_VIDEO_LVL_AV1_2_2                  = 2,
    vmpp_VIDEO_LVL_AV1_2_3                  = 3,
    vmpp_VIDEO_LVL_AV1_3_0                  = 4,
    vmpp_VIDEO_LVL_AV1_3_1                  = 5,
    vmpp_VIDEO_LVL_AV1_3_2                  = 6,
    vmpp_VIDEO_LVL_AV1_3_3                  = 7,
    vmpp_VIDEO_LVL_AV1_4_0                  = 8,
    vmpp_VIDEO_LVL_AV1_4_1                  = 9,
    vmpp_VIDEO_LVL_AV1_4_2                  = 10,
    vmpp_VIDEO_LVL_AV1_4_3                  = 11,
    vmpp_VIDEO_LVL_AV1_5_0                  = 12,
    vmpp_VIDEO_LVL_AV1_5_1                  = 13,
    vmpp_VIDEO_LVL_AV1_5_2                  = 14,
    vmpp_VIDEO_LVL_AV1_5_3                  = 15,
    vmpp_VIDEO_LVL_AV1_6_0                  = 16,
    vmpp_VIDEO_LVL_AV1_6_1                  = 17,
    vmpp_VIDEO_LVL_AV1_6_2                  = 18,
    vmpp_VIDEO_LVL_AV1_6_3                  = 19,
    vmpp_VIDEO_LVL_AV1_7_0                  = 20,
    vmpp_VIDEO_LVL_AV1_7_1                  = 21,
    vmpp_VIDEO_LVL_AV1_7_2                  = 22,
    vmpp_VIDEO_LVL_AV1_7_3                  = 23,
    vmpp_VIDEO_LVL_AV1_8_0                  = 24,
    vmpp_VIDEO_LVL_AV1_8_1                  = 25,
    vmpp_VIDEO_LVL_AV1_8_2                  = 26,
    vmpp_VIDEO_LVL_AV1_8_3                  = 27
} vmppVideoLevel;

/**
 * @brief video field type
 */
typedef enum vmppVideoField {
    vmpp_FLD_FRAME,
    vmpp_FLD_TOP,
    vmpp_FLD_BOTTOM,
} vmppVideoField;

/**
 * @brief frame type
 */
typedef enum vmppFrameType {
    vmpp_FRM_I,
    vmpp_FRM_P,
    vmpp_FRM_B
} vmppFrameType;

/**
 * @brief SEI Nal Unit Type
 */
typedef enum vmppSEINalType {
    vmpp_SEI_PREFIX = 39,
    vmpp_SEI_SUFFIX = 40
} vmppSEINalType;

/**
 * @brief SEI payload type
 */
typedef enum vmppSEIPayloadType {
    SEI_BUFFERING_PERIOD                    = 0,
    SEI_USER_DATA_UNREGISTERED              = 5
} vmppSEIPayloadType;

/**
 * @brief core mode for decoder and encoder
 */
typedef enum vmppCoreMode {
    vmpp_CORE_AUTO,                                         // auto mode: get core mode from driver
    vmpp_CORE_SINGLE,                                       // single core to decode one stream or encode frames
    vmpp_CORE_MULTI,                                        // multicores to decode one stream or encode frames
} vmppCoreMode;

/**
 * @brief Nal type
 */
typedef enum vmppNalType {
    vmpp_NAL_NONE                           = -1,
    vmpp_NAL_SPS                            = 0,
    vmpp_NAL_PPS                            = 1,
    vmpp_NAL_VPS                            = 2,
    vmpp_NAL_IVF_HEADER                     = 3,
    vmpp_NAL_PREFIX_SEI                     = 4,
    vmpp_NAL_SUFFIX_SEI                     = 5,
    vmpp_NAL_FILLER_DATA                    = 6,
    vmpp_NAL_I                              = 7,
    vmpp_NAL_B                              = 8,
    vmpp_NAL_P                              = 9,
} vmppNalType;

/**
 * @brief Crop flags
 */
typedef enum vmppCropFlag {
    vmpp_CROP_NONE                          = 0,
    vmpp_CROP_ENABLE                        = 1,
    vmpp_CROP_CUSTOMIZED                    = vmpp_CROP_ENABLE << 1,
    vmpp_CROP_SPS_INFO                      = vmpp_CROP_ENABLE << 2,
} vmppCropFlag;

/**
 * @brief context for registering external log callback
 * @note  The log callback is set through vmppInitDecoder/vmppInitEncoder.
 *        see @vmppConfiguration
 */
typedef struct vmppLogContext {
    uint32_t            enableCustomLog;                    // if using custom log context
    vmppLogCallback     logCallback;                        // log callback functions
    vmppLogLevel        logLevel;                           // ignore logs whose level < this
    const void *        usrParameters;                      // user defined parameter for log
} vmppLogContext;

/**
 * @brief stream encoded nals , used for encoder.
 */
typedef struct vmppEncedNals {
    uint32_t            cnt;
    vmppNalType         nals[VMPP_MAX_ENC_NALS];
} vmppEncedNals;

typedef struct vmppEncOutData {
   int32_t              cuInfoVersion;                      // specify which format of stats data will be output when encoding
                                                            // when cuInfoVersion is 1, the output CU statistics information is organized in decoding order, support for sv100
                                                            // when cuInfoVersion is 2, the information for the fixed 16x16 blocks is organized in raster-scan order, support for sg100
   uint8_t *            cuData;                             // point to a memory containing the CU information of a picture output
   uint32_t             cuDataTotalSize;
   uint32_t             ctuPerCol;
   uint32_t             ctuPerRow;
   uint32_t             maxCuNum;
   vmppFrameType        frameType;                          // frame type of the encoded picture
   uint32_t             frameAvgQP;                         // average qp of the frame, only support lookaheadDepth = 0
} vmppEncOutData;

/**
 * @brief input stream data
 */
typedef struct vmppStream {
    void *              stream;                             // input data pointer
    uint32_t            len;                                // input data length
    int64_t             pts;                                // pts for current frame
    uint32_t            svcTemporalId;                      // svc temporal layer id;
    vmppDevAddr         inputBusAddress;                    // encoder output, used for releasing of device memory from decoder
    vmppEncedNals       encedNals;                          // stream encoded nals, used for encoder.
    double              psnrInfo[6];                        // 0:MSE-Y 1:MSE-U 2:MSE-V 3:SSIM-Y 4:SSIM-U 5:SSIM-V
    vmppDevAddr         outputBusAddress[3];                // decoder hardware output with this bus address
    vmppEncOutData      encOutData;                         // encode output data for encoder
} vmppStream;

/**
 * @brief SEI
 */
typedef struct vmppSEI {
    /** !!!NOTEs: SEI NAL type,
     * for HEVC, this should be vmpp_SEI_PREFIX or vmpp_SEI_SUFFIX
     * for H264, this will always be treated as vmpp_SEI_PREFIX */
    vmppSEINalType      nalType;

    /** !!!NOTEs: SEI payload type, e.g. 1 for picture timing.
     * for HEVC, please see ITU-T Rec. H.265 (11/2019), Annex D.2.1 for reference
     * for H264, please see ITU-T Rec. H.264 (06/2019), Annex D.1.1 for reference */
    vmppSEIPayloadType  payloadType;

    uint32_t            payloadDataSize;                    // SEI payload data length
    uint8_t *           payloadData;                        // SEI payload data
} vmppSEI;

typedef struct vmppRational {
    uint32_t            numerator;
    uint32_t            denominator;
} vmppRational;

/**
 * @brief crop information
 */
typedef struct vmppCropInfo {
    /** !!!NOTEs: 
     * flag used in the following scenarios (ref to @vmppCropFlag)
     *  - vmppCropInfo (as input) used for vmppDecCreateChannel:
     *      [flag == 'vmpp_CROP_NONE']       No cropping through PP
     *      [flag == 'vmpp_CROP_ENABLE']     Do cropping through PP based on information from parameters set
     *      [flag == 'vmpp_CROP_CUSTOMIZED'] Do cropping through PP based on user defined information
     *  - vmppCropInfo (as output) used for vmppDecReceiveFrame:
     *      [flag == 'vmpp_CROP_NONE']       No futher cropping is needed for YUV data
     *      [flag != 'vmpp_CROP_NONE']       Futher cropping maybe needed
     *  - vmppCropInfo (as input) used for vmppEncEncodeFrame:
     *      [flag == 'vmpp_CROP_ENABLE']     Customized cropping info will be set to encoder
     *      [flag == 'vmpp_CROP_SPS_INFO']   SPS crop info will be set to encoder
     */
    uint32_t        flag;

    /* User defined crop info should never be odd number */
    uint32_t        width;
    uint32_t        height;
    uint32_t        xOffset;
    uint32_t        yOffset;
} vmppCropInfo;

/**
 * @brief frame data used as output from decoder or input for filter or encoder
 */
typedef struct vmppFrame {
    uint8_t *           data[VMPP_MAX_VIDEO_PLANE];         // frame data buffer, used when memoryType==vmpp_MEM_HOST
    vmppDevAddr         busAddress[VMPP_MAX_VIDEO_PLANE];   // hardware address for frame data, used when memoryType==vmpp_MEM_DEVICE
    uint32_t            stride[VMPP_MAX_VIDEO_PLANE];
    uint32_t            dataSize;                           // frame data size
    uint32_t            width;
    uint32_t            height;
    int64_t             pts;
    vmppRational        timebase;
    vmppMemoryType      memoryType;                         // mark where frame data is located
    vmppVideoField      field;                              // only used for decoder
    vmppPixelFormat     pixelFormat;
    vmppFrameType       frameType;
    vmppCropInfo        cropInfo;
    void *              privateData;                        // picture information, used for decoder, not for encoder.
    uint32_t            seiCount;                           // SEI array size
    vmppSEI **          seiData;                            // SEI array
} vmppFrame;

/**
 * @brief SDK version
 */
typedef struct vmppVersion {
    uint32_t            major;
    uint32_t            minor;
    uint32_t            build;
    int8_t *            versionString;
} vmppVersion;

/**
 * @brief hardware physical channel info
 */
typedef struct vmppHardwareID {
    int32_t             dieID;
    int32_t             coreID;
} vmppHardwareID;

/**
 * @brief global configuration for decoder and encoder
 */
typedef struct vmppConfiguration {
    vmppLogContext      logCtx;                             // context for registering external log callback, please check note of @vmppLogContext
} vmppConfiguration;

#ifdef __cplusplus
}
#endif /* __cplusplus */

/* clang-format on */

#endif /* __VMPP_COMMON_H__ */
