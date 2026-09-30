/*
 * Copyright (C) 2022-2023 VASTAI Technologies Co., Ltd. All Rights Reserved.
 */

/**
 * !@file   vmpp_enc_api.h
 * !@date   2022-06-02
 * !@brief  This file contains the function prototypes used for encoding.
 */
#ifndef __VMPP_ENC_API_H__
#define __VMPP_ENC_API_H__

#include "vmpp_enc_defs.h"

/* clang-format off */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @brief  Setting global vf schedule mode, only for sg100
 * @param  The global vf schedule mode, see @vmppEncVfMode
 * @return Null
 */
VMPP_API void vmppEncSetVfMode(vmppEncVfMode vfMode);

/**
 * @brief  Do global configuration for encoder
 * @param  [in] cfg  -  configuration parameters, see @vmppConfiguration
 * @return vmpp_RSLT_OK on success, otherwise negative error code: @vmppResult
 */
VMPP_API vmppResult vmppInitEncoder(vmppConfiguration *cfg);

/**
 * @brief  Do global clean job for encoder
 * @return vmpp_RSLT_OK on success, otherwise negative error code: @vmppResult
 */
VMPP_API vmppResult vmppDeInitEncoder();

/**
 * @brief  Create video encoder channel
 * @param  [in] param  -  parameters for encoder channel, see @vmppEncChannelParameters
 * @param  [out] chn   -  An vmppChannel initialized with channel parameters or NULL on failure
 * @return vmpp_RSLT_OK on success, otherwise negative error code: @vmppResult
 */
VMPP_API vmppResult vmppEncCreateChannel(vmppChannel *chn, vmppEncChannelParameters *param);

/**
 * @brief  Destory video encoder channel
 * @param  [in] chn  -  pointer for encoder channel to be destroyed
 * @return vmpp_RSLT_OK on success, otherwise negative error code: @vmppResult
 */
VMPP_API vmppResult vmppEncDestroyChannel(vmppChannel *chn);

/**
 * @brief  Allocate frame buffer for encoder
 * @param  [in]  chn     -  encoder channel context
 * @param  [out] frame   -  frame data structure, see @vmppFrame
 * @return vmpp_RSLT_OK on success, otherwise negative error code: @vmppResult  
*/
VMPP_API vmppResult vmppEncAllocFrame(vmppChannel chn, vmppFrame *frame);

/**
 * @brief  Free frame buffer for encoder
 * @param  [in]  chn     -  encoder channel context
 * @param  [out] frame   -  frame data structure, see @vmppFrame
 * @return vmpp_RSLT_OK on success, otherwise negative error code: @vmppResult  
*/
VMPP_API vmppResult vmppEncFreeFrame(vmppChannel chn, vmppFrame *frame);

/**
 * @brief  Send frame data to encoder
 * @param  [in]  chn        -  encoder channel context
 * @param  [in]  frame      -  input frame data structure, see @vmppFrame
 * @param  [in]  extParams  -  extended parameters structure, see @vmppEncExtendedParams
 * @param  [out] stream     -  output stream pointer, see @vmppStream
 * @param  [in]  timeout    -  timeout, ms
 * @return vmpp_RSLT_OK on success, otherwise negative error code: @vmppResult
 */
VMPP_API vmppResult vmppEncEncodeFrame(vmppChannel chn, vmppFrame *frame, vmppEncExtendedParams *extParams, vmppStream* stream, uint32_t timeout);

/**
 * @brief  Return stream buffer back to encoder
 * @param  [in] chn     -  encoder channel context
 * @param  [in] stream  -  output stream pointer, see @vmppStream
 * @return vmpp_RSLT_OK on success, otherwise negative error code: @vmppResult
 * @note   Must release stream after every successful calling for vmppEncEncodeFrame
 */
VMPP_API vmppResult vmppEncReleaseStream(vmppChannel chn, vmppStream* stream);

/**
 * @brief  get version of current encoder SDK
 * @return An vmppVersion information pointer or NULL on failure.
 */
VMPP_API vmppVersion *vmppEncGetVersion(void);

/**
 * @brief  Get video encoder capability
 * @param  [in]  type  -  encoder type
 * @param  [out] caps  -  capability pointer
 * @return void
 */
VMPP_API void vmppEncGetVideoCaps(vmppCodecType type, vmppEncVideoCapability *caps);

/**
 * @brief  Return stream buffer back to encoder
 * @param  [in]  chn      -  encoder channel context
 * @param  [in]  cuData   -  encode output data pointer, see @vmppEncOutData
 * @param  [out] encInfo  -  parse cuinfo data output, see @vmppEncOutInfo
 * @return vmpp_RSLT_OK on success, otherwise negative error code: @vmppResult
 */
VMPP_API vmppResult vmppEncParseCuInformation(vmppChannel chn, vmppEncOutData *cuData, vmppEncOutInfo *encInfo);


#ifdef __cplusplus
}
#endif /* __cplusplus */

/* clang-format on */

#endif /* __VMPP_ENC_API_H__ */