/*
 * Copyright (C) 2022-2023 VASTAI Technologies Co., Ltd. All Rights Reserved.
 */

/**
 * !@file   vmpp_dec_defs.h
 * !@date   2022-06-09
 * !@brief  This file contains the constant, enumeration and structure definitions for decoder.
 */

#ifndef __VMPP_DEC_DEFS_H__
#define __VMPP_DEC_DEFS_H__

#include "vmpp_common.h"

/* clang-format off */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* -------------------- Macro -------------------- */
#define VMPP_DEC_MAX_PIX_FMT_NUM        23
#define VMPP_DEC_MAX_CODING_MODE_NUM    23

/* -------------------- Enumeration -------------------- */
/**
 * @brief decode mode
 */
typedef enum vmppDecMode {
    vmpp_DEC_NORMAL,
    vmpp_DEC_INTRA_ONLY,
    vmpp_DEC_SKIP_NONREF,
    vmpp_DEC_LOW_DELAY,
    vmpp_DEC_NO_BFRAME, // same as vmpp_DEC_LOW_DELAY and also decoding as no B-frame exists
} vmppDecMode;

/**
 * @brief coding mode for JPEG
 */
typedef enum vmppJpegCodingMode {
    vmpp_JPEG_NONE,
    vmpp_JPEG_BASELINE,
    vmpp_JPEG_PROGRESSIVE,
    vmpp_JPEG_NONINTERLEAVED,
} vmppJpegCodingMode;

/**
 * @brief memory mode for decoder
 */
typedef enum vmppDecMemoryMode {
    vmpp_DEC_MEM_NORMAL,                    /* default mode.
                                             * The output buffers are all allocated inside SDK. The frame data will be output in different ways based on vmppDecOutputOptions.memoryType when calling vmppDecReceiveFrame.
                                             *
                                             * for memoryType=vmpp_MEM_HOST:   The decoded data will be output into vmppFrame.data[*] and with vmppFrame.memoryType=vmpp_MEM_HOST
                                             * for memoryType=vmpp_MEM_DEVICE: The decoded data will be output into vmppFrame.busAddress[*] and with vmppFrame.memoryType=vmpp_MEM_DEVICE */

    vmpp_DEC_MEM_USER_OUT_BUF_HOST,         /* user provide output buffer (HOST).
                                             * for VIDEO: User needs to provide a HOST buffer with sufficient size (through vmppFrame.data[0]) to call vmppDecReceiveFrame.
                                             *            And the decoded data will be output into this buffer.
                                             *            The output vmppFrame.memoryType will be vmpp_MEM_HOST.
                                             *
                                             * for JPEG:  This memory mode is not supported */

    vmpp_DEC_MEM_USER_OUT_BUF_DEV,          /* user provide output buffer (DEVICE).
                                             * for VIDEO: User needs to provide a DEVICE buffer with sufficient size (through vmppFrame.busAddress[0]) to call vmppDecReceiveFrame.
                                             *            And the decoded data will be output into this buffer.
                                             *            The output vmppFrame.memoryType will be vmpp_MEM_DEVICE.
                                             *
                                             * for JPEG:  This memory mode is not supported.
                                             *
                                             *  NOTE!!! 
                                             *   This mode only supported on certain version of driver, it will set width alignment to 64-bit, set height alignment to 16 bit,
                                             *   and transfer data from video mem to graphic mem by M2M function implemented by driver. */

    vmpp_DEC_MEM_LESS_DEV_MEM,              /* use less device memory as much as possible (only effective for video decoder).
                                             * for VIDEO: The decoded data will be output through vmppFrame.data[0] after calling vmppDecReceiveFrame,
                                             *            The output vmppFrame.memoryType will be vmpp_MEM_HOST.
                                             *
                                             * for JPEG:  This memory mode is not supported.
                                             *
                                             *  NOTE!!! 
                                             *   The decoded frame data will only stored in host memory (vmpp_MEM_HOST), 
                                             *   NOT recommended for transcoding applications. */

    vmpp_DEC_MEM_USER_AS_HWOUT,             /* user provide output buffer for HW to output data into it directly (DEVICE).
                                             * for H.264/HEVC/JPEG: User needs to provide a DEVICE buffer with sufficient size (through vmppStream.outputBusAddress[0/1]) to call vmppDecSendStream.
                                             *                      And the buffer with decoded data will be returned through vmppFrame.busAddress[0/1] after calling vmppDecReceiveFrame,
                                             *                      The output vmppFrame.memoryType will be vmpp_MEM_DEVICE
                                             *
                                             * for AV1/AVS2/VP9:    This memory mode is not supported.
                                             *
                                             *  NOTE!!!
                                             *   This mode only supported on certain version of driver, 
                                             *   Default width alignment requirements apply:
                                             *      H264: 16-pixel alignment
                                             *      HEVC: 8-pixel alignment
                                             *      JPEG: 128-pixel alignment
                                             *   This buffers will set as pp buffer not need M2M. */
} vmppDecMemoryMode;

/**
 * @brief api mode for decoder
 */
typedef enum vmppDecApiMode {
    vmpp_DEC_API_MODE_PARALLEL,    // vmppDecSendStream and vmppDecReceiveFrame will be called from two different threads.
    vmpp_DEC_API_MODE_SERIAL,      // vmppDecSendStream and vmppDecReceiveFrame will be called from one thread.
} vmppDecApiMode;

/* -------------------- Structure -------------------- */
/**
 * @brief parameters for channel creation
 */
typedef struct vmppDecChannelParameters{
    char*               decDevice;          // video decoder device node name
    char*               memDevice;          // memory device node name
    vmppCodecType       codecType;          // @vmppCodecType
    vmppSourceMode      sourceMode;         // currently only support 'frame' mode
    vmppDecMode         decodeMode;         // @vmppDecMode
    vmppPixelFormat     pixelFormat;        // output yuv pixel format, currently only support NV12
    uint32_t            maxWidth;           // reserved, max input picture width
    uint32_t            maxHeight;          // reserved, max input picture height
    uint32_t            streamBufferSize;   // reserved, input buffer size
    uint32_t            extraBufferNumber;  // extra buffer number, must be set big enough when run transcoding cases if using vmpp_MEM_DEVICE memory type
    uint32_t            enProfiling;        // enable profiling
    vmppCoreMode        coreMode;           // core mode
    vmppDecMemoryMode   memoryMode;         // memory mode for video decoder
    uint32_t            outputAlign;        // alignment of output buffer set by user
    vmppDecApiMode      apiMode;            // api mode for video decoder
    uint32_t            enSEIParser;        // enable SEI Parser, 0: disable, 1: enable
    vmppCropInfo        cropInfo;           // crop config
    uint32_t            noOutputReordering; // no output reordering, only for sv100
    uint32_t            bufSlimMode;        // buf slim mode, 0: disable, 1: enable, only for sv100
    uint32_t            extDevId;           // external device ID, default is 0, only for sg100
} vmppDecChannelParameters;

/**
 * @brief stream information
 */
typedef struct vmppDecStreamInfo {
	uint32_t            width;
	uint32_t            height;
	uint32_t            fps;
	uint32_t            pixelSize;
} vmppDecStreamInfo;

/**
 * @brief jpeg information
 */
typedef struct vmppDecJpegInfo {
    uint32_t            width;              // Number of pixels/line in the image
    uint32_t            height;             // Number of lines in in the image
    uint32_t            xDensity;
    uint32_t            yDensity;
    vmppChromaFormat    outputFormat;
    vmppJpegCodingMode  codingMode;
} vmppDecJpegInfo;

/**
 * @brief video information
 */
typedef struct vmppDecVideoInfo {
    uint32_t            width;              // Number of pixels/line in the image
    uint32_t            height;             // Number of lines in in the image
    uint32_t            cropFlag;
    uint32_t            cropWidth;
    uint32_t            cropHeight; 
    uint32_t            xOffset;
    uint32_t            yOffset;
    vmppRational        fps;
    vmppPixelFormat     pixelFormat;
    uint32_t            frameOnlyFlag;      // sps frame_mbs_only_flag, only for h264 and hevc
    uint32_t            requiredBufNum;     // buffer number required for video decoder
    uint32_t            reorderNum;         // reorder frames number
    uint32_t            reserved[16];
} vmppDecVideoInfo;

/**
 * @brief output option for receiving data from decoder
 */
typedef struct vmppDecOutputOptions {
    vmppMemoryType      memoryType;         // host memory or device memory, see @vmppMemoryType
    uint32_t            enableCrop;         // mark if need to do crop
} vmppDecOutputOptions;

/**
 * @brief hardware decoder status
 */
typedef struct vmppDecStatus {
    vmppState           state;
    vmppHardwareID      hardwareID;
    vmppResult          result;
    uint32_t            runningFrames;
    uint32_t            reorderedFrames;
    uint32_t            bufferedFrames;
    uint32_t            droppedFrames;
} vmppDecStatus;

/**
 * @brief Jpeg decoder capability
 */
typedef struct vmppDecJpegCapability {
    uint32_t            maxWidth;
    uint32_t            maxHeight;
    uint32_t            minWidth;
    uint32_t            minHeight;
    vmppJpegCodingMode  codingMode[VMPP_DEC_MAX_CODING_MODE_NUM];
    vmppPixelFormat     pixelFormats[VMPP_DEC_MAX_PIX_FMT_NUM];
} vmppDecJpegCapability;

/**
 * @brief video decoder capability
 */
typedef struct vmppDecVideoCapability {
    uint32_t            bitDepth;
    uint32_t            maxWidth;
    uint32_t            maxHeight;
    uint16_t            minWidth;
    uint16_t            minHeight;
    vmppVideoProfile    maxProFile;
    vmppVideoLevel      maxLevel;
    vmppPixelFormat     pixelFormats[VMPP_DEC_MAX_PIX_FMT_NUM];
} vmppDecVideoCapability;

/**
 * @brief video slice info
 */
typedef struct vmppSliceInfo {
    vmppVideoField      picStruct;
    uint32_t            isFirstField;
    int32_t             picOrderCnt;
    uint32_t            reserved[16];
} vmppSliceInfo;

#ifdef __cplusplus
}
#endif /* __cplusplus */

/* clang-format on */

#endif /* __VMPP_DEC_DEFS_H__ */