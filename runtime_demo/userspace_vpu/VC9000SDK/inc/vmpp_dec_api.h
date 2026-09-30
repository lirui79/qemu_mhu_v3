/*
 * Copyright (C) 2022-2023 VASTAI Technologies Co., Ltd. All Rights Reserved.
 */

/**
 * !@file   vmpp_dec_api.h
 * !@date   2022-06-02
 * !@brief  This file contains the function prototypes used for decoding.
 */
#ifndef __VMPP_DEC_API_H__
#define __VMPP_DEC_API_H__

#include "vmpp_dec_defs.h"

/* clang-format off */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @brief  Do global configuration for decoder
 * @param  [in] cfg  -  configuration parameters, see @vmppConfiguration
 * @return vmpp_RSLT_OK on success, otherwise negative error code: @vmppResult
 */
VMPP_API vmppResult vmppInitDecoder(vmppConfiguration *cfg);

/**
 * @brief  Do global clean job for decoder
 * @return vmpp_RSLT_OK on success, otherwise negative error code: @vmppResult
 */
VMPP_API vmppResult vmppDeInitDecoder();

/**
 * @brief  Create video decoder channel
 * @param  [in] param  -  parameters for decoder channel, see @vmppDecChannelParameters
 * @param  [out] chn   -  An vmppChannel initialized with channel parameters or NULL on failure
 * @return vmpp_RSLT_OK on success, otherwise negative error code: @vmppResult
 */
VMPP_API vmppResult vmppDecCreateChannel(vmppChannel *chn, vmppDecChannelParameters *param);

/**
 * @brief  Destory video decoder channel
 * @param  [in] chn  -  pointer for decoder channel to be destroyed
 * @return vmpp_RSLT_OK on success, otherwise negative error code: @vmppResult
 */
VMPP_API vmppResult vmppDecDestroyChannel(vmppChannel *chn);

/**
 * @brief  Destory video decoder channel forcedly
 * @param  [in] chn  -  pointer for decoder channel to be destroyed
 * @return vmpp_RSLT_OK on success, otherwise negative error code: @vmppResult
 */
VMPP_API vmppResult vmppDecDestroyChannelForced(vmppChannel *chn);

/**
 * @brief  start decoder
 * @param  [in] chn  -  decoder channel
 * @return vmpp_RSLT_OK on success, otherwise negative error code: @vmppResult
 */
VMPP_API vmppResult vmppDecStart(vmppChannel chn);

/**
 * @brief  stop decoder
 * @param  [in] chn  -  decoder channel to be destroyed
 * @return vmpp_RSLT_OK on success, otherwise negative error code: @vmppResult
 */
VMPP_API vmppResult vmppDecStop(vmppChannel chn);

/**
 * @brief  Send stream data to decoder
 * @param  [in] chn      -  decoder channel context
 * @param  [in] stream   -  input stream data structure, see @vmppStream
 * @param  [in] timeout  -  timeout, ms
 * @return vmpp_RSLT_OK on success, otherwise negative error code: @vmppResult
 */
VMPP_API vmppResult vmppDecSendStream(vmppChannel chn, vmppStream *stream, uint32_t timeout);

/**
 * @brief  Send stream data to decoder version 2
 * @param  [in]  chn         -  decoder channel context
 * @param  [in]  stream      -  input stream data structure, see @vmppStream
 * @param  [out] sliceInfo   -  slice info, see @vmppSliceInfo, now only support h264 decode
 * @param  [in]  timeout     -  timeout, ms
 * @return vmpp_RSLT_OK on success, otherwise negative error code: @vmppResult
 */
VMPP_API vmppResult vmppDecSendStreamV2(vmppChannel chn, vmppStream *stream, vmppSliceInfo *sliceInfo, uint32_t timeout);

/**
 * @brief  Retrieve decoded frames from decoder
 * @param  [in]  chn      -  decoder channel context
 * @param  [out] frame    -  decoded video frame received from decoder
 * @param  [in]  opt      -  options used for user to choose mem type and if need crop info.
 * @param  [in]  timeout  -  reserved
 * @return vmpp_RSLT_OK on success, otherwise negative error code: @vmppResult
 */
VMPP_API vmppResult vmppDecReceiveFrame(vmppChannel chn, vmppFrame *frame, vmppDecOutputOptions *opt, uint32_t timeout);

/**
 * @brief  Transfer frame data to HOST memory from DEVICE memory
 * @param  [in]       chn    -  decoder channel context
 * @param  [in & out] frame  -  frame (has data located on Device) to transfer
 *                              !!! Note: Must be a frame decoded with @vmppDecReceiveFrame and memory type is vmpp_MEM_DEVICE
 *                              !!!       see @vmppMemoryType and @vmppDecReceiveFrame
 * @param  [in]       crop   -  if need crop info
 * @return vmpp_RSLT_OK on success, otherwise negative error code: @vmppResult
 */
VMPP_API vmppResult vmppDecTransferFrame(vmppChannel chn, vmppFrame *frame, uint32_t crop);

/**
 * @brief  Release current frame
 * @param  [in] chn      -  decoder channel context
 * @param  [in] frame    -  frame to be released
 * @param  [in] timeout  -  reserved
 * @return vmpp_RSLT_OK on success, otherwise negative error code: @vmppResult
 */
VMPP_API vmppResult vmppDecReleaseFrame(vmppChannel chn, vmppFrame *frame, uint32_t timeout);

/**
 * @brief  Get current stream info
 * @param [in]  chn   -  decoder channel context
 * @param [out] info  -  stream info structure
 * @return vmpp_RSLT_OK on success, otherwise negative error code: @vmppResult
 */
VMPP_API vmppResult vmppDecGetStreamInfo(vmppChannel chn, vmppDecStreamInfo *info);

/**
 * @brief  Get jpeg info
 * @param [in]  stream  -  input stream
 * @param [out] info    -  jpeg info structure
 * @return vmpp_RSLT_OK on success, otherwise negative error code: @vmppResult
 */
VMPP_API vmppResult vmppDecGetJpegInfo(vmppStream *stream, vmppDecJpegInfo *info);

/**
 * @brief  Get video info
 * @param [in]  stream     -  input stream
 * @param [in]  codecType  -  video codec type
 * @param [out] info       -  video info structure
 * @return vmpp_RSLT_OK on success, otherwise negative error code: @vmppResult
 */
VMPP_API vmppResult vmppDecGetVideoInfo(vmppStream *stream, vmppCodecType codecType, vmppDecVideoInfo *info);

/**
 * @brief  Get status of current decode channel
 * @param  [in]  chn     -  decoder channel context
 * @param  [out] status  -  status information structure pointer
 * @return vmpp_RSLT_OK on success, otherwise negative error code: @vmppResult
 */
VMPP_API vmppResult vmppDecGetStatus(vmppChannel chn, vmppDecStatus *status);

/**
 * @brief  Get jpeg decode capability
 * @param  [out] caps  -  capability pointer
 * @return void
 */
VMPP_API void vmppDecGetJpegCaps(vmppDecJpegCapability *caps);

/**
 * @brief  Get video decode capability
 * @param  [in]  type  -  decoder type
 * @param  [out] caps  -  capability pointer
 * @return void
 */
VMPP_API void vmppDecGetVideoCaps(vmppCodecType type, vmppDecVideoCapability *caps);

/**
 * @brief  Get version of current decoder SDK
 * @return An vmppVersion information pointer or NULL on failure.
 */
VMPP_API vmppVersion *vmppDecGetVersion(void);

/**
 * @brief  Get the number of idle dpb buffer
 * @param  [in]  chn     -  decoder channel context
 * @return idle dpb buffer count
 */
VMPP_API int32_t vmppDecGetIdleDpbBufferCount(vmppChannel chn);


#ifdef __cplusplus
}
#endif /* __cplusplus */

/* clang-format on */

#endif  /* __VMPP_DEC_API_H__ */