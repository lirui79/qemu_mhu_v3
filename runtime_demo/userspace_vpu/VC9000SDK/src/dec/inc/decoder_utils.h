#ifndef __DECODER_UTILS_H__
#define __DECODER_UTILS_H__

#include "va_log.h"
#include "vmpp_common.h"

// vsi headers
#include "decapicommon.h"

static inline vmppPixelFormat format_from_vsi(enum DecPictureFormat fmt)
{
    vmppPixelFormat format = vmpp_PIX_FMT_NV12;
    switch (fmt) {
    case DEC_OUT_FRM_MONOCHROME:
    case DEC_OUT_FRM_YUV400:
        format = vmpp_PIX_FMT_GRAY8;
        break;
    case DEC_OUT_FRM_RASTER_SCAN:
    case DEC_OUT_FRM_YUV420SP:
        format = vmpp_PIX_FMT_NV12;
        break;
    case DEC_OUT_FRM_PLANAR_420:
    case DEC_OUT_FRM_YUV420P:
        format = vmpp_PIX_FMT_YUV420P;
        break;
    case DEC_OUT_FRM_NV21SP:
        format = vmpp_PIX_FMT_NV21;
        break;
    case DEC_OUT_FRM_YUV420SP_P010:
        format = vmpp_PIX_FMT_YUV420_PLANAR_10BIT_P010;
        break;
    case DEC_OUT_FRM_YUV420SP_I010:
        format = vmpp_PIX_FMT_YUV420_PLANAR_10BIT_I010;
        break;
    default:
        LOG_WARN(DEC, "Unsupported format %d", fmt);
        break;
    }
    return format;
}

#endif // __DECODER_UTILS_H__