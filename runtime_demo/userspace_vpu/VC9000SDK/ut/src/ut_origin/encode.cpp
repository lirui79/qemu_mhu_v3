#include "vmpp_enc_api.h"
#include "vmpp_enc_defs.h"
#include "catch2/catch.hpp"
#include "encode.hpp"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
// #include "log.h"
#include "stream.h"
#include "utils.h"
#ifdef __cplusplus
}
#endif /* __cplusplus */

extern vmppRuntimeInstance rtInstance;
extern vmppDevice encFD;
extern vmppEncChannelParameters defaultEncParams;
extern vmppEncJPEGConfiguration defaultEncParams4JPEG;
extern vmppEncVideoConfiguration defaultEncParams4Video;

int flu_lop = 0;
int gopSize_lop = 0;
int extParams_lop = 0;

int Initialize_Encoder()
{
    vmppResult ret = vmpp_RSLT_OK;
    vmppConfiguration cfg;
    memset(&cfg, 0, sizeof(vmppConfiguration));
    cfg.runtimeInst = rtInstance;

    ret = vmppInitEncoder(NULL);
    REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );

    cfg.runtimeInst.init = NULL;
    ret = vmppInitEncoder(&cfg);
    REQUIRE(ret == vmpp_RSLT_ERR_INVALID_PARAMS);

    cfg.runtimeInst = rtInstance;
    cfg.logCtx.enableCustomLog = 1;
    ret = vmppInitEncoder(&cfg);
    REQUIRE( ret == vmpp_RSLT_OK );

    cfg.runtimeInst = rtInstance;
    cfg.logCtx.enableCustomLog = 0;
    ret = vmppInitEncoder(&cfg);
    REQUIRE(ret == vmpp_RSLT_OK);

    return 0;
}

static void loadDefaultParams(vmppCodecType type, vmppEncChannelParameters *params)
{
    memcpy(params, &defaultEncParams, sizeof(vmppEncChannelParameters));

    params->device = (vmppDevice)encFD;
    params->codecType = type;
    if (type == vmpp_CODEC_ENC_JPEG) {
        memcpy(&params->jpegConfig, &defaultEncParams4JPEG, sizeof(vmppEncJPEGConfiguration));
    } else {
        memcpy(&params->videoConfig, &defaultEncParams4Video, sizeof(vmppEncVideoConfiguration));
        if (type == vmpp_CODEC_ENC_H264) {
            params->videoConfig.profile = vmpp_VIDEO_PRFL_H264_HIGH;
            params->videoConfig.level = vmpp_VIDEO_LVL_H264_5_1;
        } else if (type == vmpp_CODEC_ENC_HEVC) {
            params->videoConfig.profile = vmpp_VIDEO_PRFL_HEVC_MAIN;
            params->videoConfig.level = vmpp_VIDEO_LVL_HEVC_6;
        }
    }
}

int Create_Enc_Channel_before_Init()
{
    vmppResult ret = vmpp_RSLT_OK;
    vmppChannel chn = NULL;

    vmppEncChannelParameters params;

    memset(&params, 0, sizeof(params));
    loadDefaultParams(vmpp_CODEC_ENC_JPEG, &params);
    params.device = (vmppDevice)encFD;
    ret = vmppEncCreateChannel(&chn, &params);
    REQUIRE( ret == vmpp_RSLT_RUNTIME_INVALID );
    return 0;
}

int Create_and_Destroy_Enc_Channel()
{
    vmppResult ret = vmpp_RSLT_OK;
    vmppChannel chn = NULL;

    vmppEncChannelParameters params;

    try {
        memset(&params, 0, sizeof(params));
        loadDefaultParams(vmpp_CODEC_ENC_JPEG, &params);
        params.device = (vmppDevice)encFD;

        /* Invalid parameters cases */
        ret = vmppEncCreateChannel(NULL, &params);
        REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );

        ret = vmppEncCreateChannel(&chn, NULL);
        REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );

        ret = vmppEncCreateChannel(NULL, NULL);
        REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );

        params.codecType = vmpp_CODEC_DEC_JPEG;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );

        params.codecType = vmpp_CODEC_DEC_H264;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );

        params.codecType = vmpp_CODEC_DEC_HEVC;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );

        memset(&params, 0, sizeof(params));
        loadDefaultParams(vmpp_CODEC_ENC_H264, &params);
        params.device = (vmppDevice)encFD;
        params.videoConfig.lookaheadDepth = 41;
        params.videoConfig.llRc = 6;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret < vmpp_RSLT_OK );
        params.videoConfig.llRc = 0;
        params.videoConfig.maxFrameSizeMultiple = 1;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret < vmpp_RSLT_OK );
        params.videoConfig.lookaheadDepth = 3;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret < vmpp_RSLT_OK );
        params.videoConfig.lookaheadDepth = -1;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret < vmpp_RSLT_OK );
        params.device = -1;
        // params.videoConfig.enableROI = 1;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_ERR_ENC_INIT );

        memset(&params, 0, sizeof(params));
        loadDefaultParams(vmpp_CODEC_ENC_JPEG, &params);
        params.device = -1;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_ERR_ENC_INIT );

        memset(&params, 0, sizeof(params));
        loadDefaultParams(vmpp_CODEC_ENC_HEVC, &params);
        
        params.videoConfig.lookaheadDepth = 5;
        params.videoConfig.P2B = VMPP_ENC_DEFAULT_PAR;
        params.videoConfig.rdoLevel = 0;
	    params.videoConfig.maxBFrames = 7;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_ERR_ENC_INIT ); vmppEncDestroyChannel(&chn);
        params.videoConfig.rdoLevel = VMPP_ENC_DEFAULT_PAR;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_OK ); vmppEncDestroyChannel(&chn);
        params.videoConfig.roiType = vmpp_ENC_ROI_RANGE;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_ERR_ENC_INIT );
        params.videoConfig.lookaheadDepth = 0;
        params.videoConfig.gdrDuration = 2;
        params.videoConfig.keyInt = 1;
	    params.videoConfig.maxBFrames = 0;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_OK ); vmppEncDestroyChannel(&chn);
        params.videoConfig.gdrDuration = 3;
        params.videoConfig.keyInt = 2;
        params.videoConfig.maxBFrames = 7;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_OK ); vmppEncDestroyChannel(&chn);
        params.videoConfig.tune = vmpp_ENC_TUNE_SSIM;
        params.videoConfig.lookaheadDepth = 0;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_OK ); vmppEncDestroyChannel(&chn);
        params.videoConfig.tune = vmpp_ENC_TUNE_VISUAL;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_OK ); vmppEncDestroyChannel(&chn);
        params.videoConfig.tune = vmpp_ENC_TUNE_SHARP_VISUAL;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_OK ); vmppEncDestroyChannel(&chn);
        params.videoConfig.tune = (vmppEncTuneType)5;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_OK ); vmppEncDestroyChannel(&chn);
        params.videoConfig.gopSize = 0;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_OK ); vmppEncDestroyChannel(&chn);

        params.videoConfig.qualityMode = vmpp_GOLD_QUALITY;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_OK ); vmppEncDestroyChannel(&chn);
        params.videoConfig.qualityMode = vmpp_SILVER_QUALITY;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_OK ); vmppEncDestroyChannel(&chn);
        params.videoConfig.qualityMode = vmpp_SILVERPLUS_QUALITY;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_OK ); vmppEncDestroyChannel(&chn);
        params.videoConfig.qualityMode = (vmppEncQualityMode)5;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_OK ); vmppEncDestroyChannel(&chn);
        memset(&params, 0, sizeof(params));
        loadDefaultParams(vmpp_CODEC_ENC_H264, &params);
        params.videoConfig.qualityMode = vmpp_SILVER_QUALITY;
        params.videoConfig.maxBFrames = 7;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_OK ); vmppEncDestroyChannel(&chn);
        params.videoConfig.qualityMode = vmpp_SILVERPLUS_QUALITY;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_OK ); vmppEncDestroyChannel(&chn);
        params.videoConfig.qualityMode = vmpp_BRONZE_QUALITY;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_OK ); vmppEncDestroyChannel(&chn);
        params.videoConfig.qualityMode = (vmppEncQualityMode)5;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_OK ); vmppEncDestroyChannel(&chn);
        params.videoConfig.P2B = 1;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_OK ); vmppEncDestroyChannel(&chn);

        memset(&params, 0, sizeof(params));
        loadDefaultParams(vmpp_CODEC_ENC_JPEG, &params);
        params.jpegConfig.frameType = vmpp_PIX_FMT_NV21;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_OK ); vmppEncDestroyChannel(&chn);
        params.jpegConfig.frameType = vmpp_PIX_FMT_RGB24;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_OK ); vmppEncDestroyChannel(&chn);
        params.jpegConfig.frameType = vmpp_PIX_FMT_BGR24;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_OK ); vmppEncDestroyChannel(&chn);
        params.jpegConfig.frameType = vmpp_PIX_FMT_GRAY8;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_OK ); vmppEncDestroyChannel(&chn);
        params.outbufNum = 0;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_OK ); vmppEncDestroyChannel(&chn);
        params.outbufNum = 34;
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_OK ); vmppEncDestroyChannel(&chn);

        memset(&params, 0, sizeof(params));
        loadDefaultParams(vmpp_CODEC_ENC_H264, &params);
        params.device = (vmppDevice)encFD;
        params.videoConfig.gopSize = 9; // 9~15 is not supported
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret < vmpp_RSLT_OK );
        params.videoConfig.gopSize = -2; // Negative numbers
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_ERR_ENC_INIT );
        params.videoConfig.gopSize = 17; // > 16 
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_ERR_ENC_INIT );

        params.videoConfig.gopSize = 0; // -1 == VMPP_ENC_DEFAULT_PAR
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_OK );
        if (ret == vmpp_RSLT_OK) {
            ret = vmppEncDestroyChannel(NULL);
            REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );
            ret = vmppEncDestroyChannel(&chn);
            REQUIRE( ret == vmpp_RSLT_OK );
        }
    } catch (...) {
        return -1;
    }

    return 0;
}

int Create_and_Destroy_Enc_Channels(int channels, vmppCodecType type)
{
    vmppResult ret = vmpp_RSLT_OK;
    vmppChannel chn = NULL;

    vmppEncChannelParameters params;
    memset(&params, 0, sizeof(params));
    loadDefaultParams(type, &params);

    params.device = (vmppDevice)encFD;
    params.videoConfig.maxBFrames = 7;

    for (int i = 0; i < channels; i++) {
        ret = vmppEncCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_OK );
        ret = vmppEncDestroyChannel(&chn);
        REQUIRE( ret == vmpp_RSLT_OK );
    }

    return 0;
}

int Get_Video_Enc_Version()
{
    vmppVersion *ret; 
    for (int i = 0; i < 100; i++) {
        ret = vmppEncGetVersion();
        REQUIRE( ret != NULL );
    }
    return 0;
}
int Get_Video_Enc_Caps()
{
    try {
        vmppEncVideoCapability cap = {0};
        vmppEncGetVideoCaps(vmpp_CODEC_ENC_H264, NULL);
        vmppEncGetVideoCaps(vmpp_CODEC_ENC_HEVC, NULL);
        vmppEncGetVideoCaps(vmpp_CODEC_ENC_JPEG, NULL);
        vmppEncGetVideoCaps(vmpp_CODEC_DEC_JPEG, NULL);
        vmppEncGetVideoCaps(vmpp_CODEC_DEC_H264, NULL);
        vmppEncGetVideoCaps(vmpp_CODEC_DEC_HEVC, NULL);

        vmppEncGetVideoCaps(vmpp_CODEC_ENC_H264, &cap);
        vmppEncGetVideoCaps(vmpp_CODEC_ENC_HEVC, &cap);
        vmppEncGetVideoCaps(vmpp_CODEC_ENC_JPEG, &cap);
        vmppEncGetVideoCaps(vmpp_CODEC_DEC_JPEG, &cap);
        vmppEncGetVideoCaps(vmpp_CODEC_DEC_H264, &cap);
        vmppEncGetVideoCaps(vmpp_CODEC_DEC_HEVC, &cap);
    } catch (...) {
        return -1;
    }
    return 0;
}

int Encoding(vmppCodecType type)
{
    if (type != vmpp_CODEC_ENC_JPEG && type != vmpp_CODEC_ENC_HEVC && type != vmpp_CODEC_ENC_H264) {
        LOG_WARN("Unsupported codec type %d", type);
        return -1;
    }

    vmppResult encRet = vmpp_RSLT_OK;
    vmppChannel chn = NULL;

    vmppEncChannelParameters params;
    memset(&params, 0, sizeof(params));
    loadDefaultParams(type, &params);
    params.device = (vmppDevice)encFD;
    params.videoConfig.maxBFrames = 7;

    {
        struct raw_context raw_ctx = {0};
        char rawPath[MAX_PATH_LEN] = {0};
        int width, height, stride;
        vmppPixelFormat fmt = vmpp_PIX_FMT_NV12;
        // int frameCount = 0;
        switch (type) {
        case vmpp_CODEC_ENC_JPEG:
            sprintf(rawPath, "%s/1280x720_nv12_1.yuv", UT_RES_PATH);
            params.jpegConfig.codingWidth = width = 1280;
            params.jpegConfig.codingHeight = height = 720;
            params.jpegConfig.frameType = fmt = vmpp_PIX_FMT_NV12;
            // frameCount = 1;
            break;
        case vmpp_CODEC_ENC_H264:
        case vmpp_CODEC_ENC_HEVC:
        default:
            sprintf(rawPath, "%s/352x288_nv12_121.yuv", UT_RES_PATH);
            params.videoConfig.width = width = 352;
            params.videoConfig.height = height = 288;
            fmt = vmpp_PIX_FMT_NV12;
            // frameCount = 121;
            break;
        }

        encRet = vmppEncCreateChannel(&chn, &params);
        REQUIRE( encRet == vmpp_RSLT_OK );
        if (encRet != vmpp_RSLT_OK) {
            LOG_WARN("Failed to create channel for %d", type);
            return -1;
        }

        // equal by default
        stride = width;

        int tmpRet = raw_open(rawPath, fmt, width, height, stride, &raw_ctx);
        if (!tmpRet) {
            int comp1_size, comp2_size, comp3_size;
            int size = raw_pic_size(&raw_ctx, &comp1_size, &comp2_size, &comp3_size);

            vmppFrame frame = {0};
            frame.data[0] = new (std::nothrow) uint8_t[size];
            if (!frame.data[0]) {
                printf("frame err\n");
                raw_close(&raw_ctx);
                vmppEncDestroyChannel(&chn);
                return -1;
            }
            frame.data[1] = frame.data[0] + comp1_size;
            if (comp3_size)
                frame.data[2] = frame.data[1] + comp2_size;
            vmppStream stream;
            int64_t send = 0;
            int64_t receive = 0;
            vmppSEI *seiData = NULL;
            uint8_t *payloadData = NULL;
            do {
                memset(&stream, 0, sizeof(vmppStream));
                tmpRet = raw_read_frame(&raw_ctx, &frame);
                if (tmpRet <= 0)
                    break;
                frame.pts = ++send;
                do {
                    encRet = vmppEncEncodeFrame(NULL, &frame, NULL, &stream, 4000);
                    REQUIRE( encRet == vmpp_RSLT_ERR_INVALID_PARAMS );
                    encRet = vmppEncEncodeFrame(chn, NULL, NULL, &stream, 4000);
                    REQUIRE( encRet == vmpp_RSLT_ERR_INVALID_PARAMS );
                    encRet = vmppEncEncodeFrame(chn, &frame, NULL, NULL, 4000);
                    REQUIRE( encRet == vmpp_RSLT_ERR_INVALID_PARAMS );
                    if (frame.pts % 9 == 0 ) {
                        encRet = vmppEncEncodeFrame(chn, &frame, NULL, &stream, 200);//Timeout < 4000 is too small, using default minimum value(4000)
                    }
                    else if (frame.pts % 9 == 1 ) {
                        if (type == vmpp_CODEC_ENC_H264)
                            gopSize_lop++;
                        if (frame.pts == 1 && type == vmpp_CODEC_ENC_H264) {
                            struct va_enc_channel *inst = (struct va_enc_channel *)chn;
                            // struct video_encoder_private_context *ctx = (struct video_encoder_private_context *)        inst->private_context;
                            // ctx->roiType = vmpp_ENC_ROI_RANGE;
                            // ctx->lastROI[0].qpValue = 1;
                            // ctx->lastROI[0].area.bottom = 8000;
                            // ctx->lastROI[0].area.top = 8000;
                            // ctx->lastROI[0].area.left = 8000;
                            // ctx->lastROI[0].area.right = 8000;
                            
                            vmppEncExtendedParams extParams;
                            memset(&extParams, 0, sizeof(extParams));
                            extParams.roi[0].area.enable = 1;
                            extParams.roi[0].area.bottom = 8000;
                            extParams.roi[0].area.top = 8000;
                            extParams.roi[0].area.left = 8000;
                            extParams.roi[0].area.right = 8000;

                            for (int i=1; i<8; i++)
                                extParams.roi[i].area.enable = 1;
                            extParams.roi[1].qpType = vmpp_ENC_QP_DELTA;
                            // ctx->cfg.streamType = VCENC_NAL_UNIT_STREAM;
                            // ctx->lastVRet = VCENC_FRAME_ENQUEUE;

                            inst->params.videoConfig.roiType = vmpp_ENC_ROI_RANGE;
                            
                            inst->params.videoConfig.frameRate.numerator = 8;
                            inst->params.videoConfig.initQp = 0;
                            inst->params.videoConfig.qpMinPB = 0;
                            inst->params.videoConfig.qpMinI = 0;
                            inst->params.videoConfig.qpMaxPB = 0;
                            inst->params.videoConfig.qpMaxI = 0;
                            // inst->params.videoConfig.bitRate = VMPP_ENC_DEFAULT_PAR - 1;
                            inst->params.videoConfig.intraQpDelta = 0;
                            inst->params.videoConfig.vbvBufSize = VMPP_ENC_DEFAULT_PAR - 1;
                            inst->params.videoConfig.vbvMaxRate = VMPP_ENC_DEFAULT_PAR - 1;
                            inst->params.videoConfig.crf = (int32_t)VMPP_ENC_DEFAULT_PAR;

                            int outbufNum = inst->params.outbufNum;
                            inst->params.outbufNum = 0;
                            encRet = vmppEncEncodeFrame(chn, &frame, &extParams, &stream, 4000);
                            inst->params.outbufNum = outbufNum;
                            inst->params.videoConfig.frameRate.numerator = 20;
                            // ctx->cfg.streamType = VCENC_BYTE_STREAM;
                            // ctx->lastVRet = VCENC_FRAME_READY;
                            inst->params.videoConfig.roiType = vmpp_ENC_ROI_NONE;
                            for (int i=1; i<8; i++)
                                extParams.roi[i].area.enable = 0;
                            inst->params.videoConfig.initQp = VMPP_ENC_DEFAULT_PAR;
                            inst->params.videoConfig.qpMinPB = VMPP_ENC_DEFAULT_PAR;
                            inst->params.videoConfig.qpMinI = VMPP_ENC_DEFAULT_PAR;
                            inst->params.videoConfig.qpMaxPB = VMPP_ENC_DEFAULT_PAR;
                            inst->params.videoConfig.qpMaxI = VMPP_ENC_DEFAULT_PAR;
                            // inst->params.videoConfig.bitRate = VMPP_ENC_DEFAULT_PAR;
                            inst->params.videoConfig.intraQpDelta = VMPP_ENC_DEFAULT_PAR;
                            inst->params.videoConfig.vbvBufSize = VMPP_ENC_DEFAULT_PAR;
                            inst->params.videoConfig.vbvMaxRate = VMPP_ENC_DEFAULT_PAR;
                            inst->params.videoConfig.crf = (int32_t)VMPP_ENC_DEFAULT_PAR;
                        } else {
                            if (type == vmpp_CODEC_ENC_JPEG) {
                                frame.pixelFormat = vmpp_PIX_FMT_NV21;
                                encRet = vmppEncEncodeFrame(chn, &frame, NULL, &stream, 4000);
                                printf("============ YUV420P encRet: %d\n", encRet);
                                REQUIRE( encRet == vmpp_RSLT_OK );
                                
                                frame.pixelFormat = vmpp_PIX_FMT_RGB24;
                                encRet = vmppEncEncodeFrame(chn, &frame, NULL, &stream, 4000);
                                printf("============ RGB24 encRet: %d\n", encRet);
                                // REQUIRE( encRet == vmpp_RSLT_ERR_ENC_SET_PIC_SIZE );
                            }
                            frame.pixelFormat = vmpp_PIX_FMT_YUV420P;
                            encRet = vmppEncEncodeFrame(chn, &frame, NULL, &stream, 4000);
                            printf("============ NV21 encRet: %d\n", encRet);
                            encRet = vmpp_RSLT_OK;
                        }
                    } else if (frame.pts % 9 == 2 ) {
                        struct va_enc_channel *inst = (struct va_enc_channel *)chn;
                        inst->params.videoConfig.llRc = 1;

                        frame.width = 352;
                        frame.height = 288;
                        frame.pixelFormat = vmpp_PIX_FMT_NV21;
                        encRet = vmppEncEncodeFrame(chn, &frame, NULL, &stream, 4000);
                        inst->params.videoConfig.llRc = 0;
                    } else if (frame.pts % 9 == 3 ) {
                        struct va_enc_channel *inst = (struct va_enc_channel *)chn;
                        if (frame.pts == 3)
                            inst->params.videoConfig.llRc = 2;
                        else if (frame.pts == 12)
                            inst->params.videoConfig.llRc = 3;
                        else if (frame.pts == 21)
                            inst->params.videoConfig.llRc = 4;
                        else if (frame.pts == 30)
                            inst->params.videoConfig.llRc = 5;
                        seiData = (vmppSEI *)malloc(sizeof(vmppSEI));
                        payloadData = (uint8_t *)malloc(sizeof(uint8_t) * 10);
                        if (type == vmpp_CODEC_ENC_H264) {
                            frame.seiCount = 1;
                            memset(seiData, 0, sizeof(vmppSEI));
                            frame.seiData = &seiData;
                            frame.seiData[0]->nalType = vmpp_SEI_PREFIX;
                            
                            memset(payloadData, 0, sizeof(uint8_t));
                            frame.seiData[0]->payloadData = payloadData;
                            frame.seiData[0]->payloadDataSize = 10;
                            frame.seiData[0]->payloadType = SEI_BUFFERING_PERIOD;
                        }
                        frame.pixelFormat = vmpp_PIX_FMT_RGB24;
                        encRet = vmppEncEncodeFrame(chn, &frame, NULL, &stream, 4000);
                        frame.seiCount = 0;
                        inst->params.videoConfig.llRc = 0;
                    } else if (frame.pts % 9 == 4 ) {
                        if (type == vmpp_CODEC_ENC_HEVC) {
                            struct va_enc_channel *inst = (struct va_enc_channel *)chn;
                            int lookaheadDepth = inst->params.videoConfig.lookaheadDepth;
                            inst->params.videoConfig.lookaheadDepth = 1;
                            frame.pixelFormat = vmpp_PIX_FMT_BGR24;
                            encRet = vmppEncEncodeFrame(chn, &frame, NULL, &stream, 4000);
                            inst->params.videoConfig.lookaheadDepth = lookaheadDepth;
                        } else
                            encRet = vmppEncEncodeFrame(chn, &frame, NULL, &stream, 4000);
                    } else if (frame.pts % 9 == 5 ) {
                        struct va_enc_channel *inst = (struct va_enc_channel *)chn;
                        int gopSize = inst->params.videoConfig.gopSize;
                        int lookaheadDepth = inst->params.videoConfig.lookaheadDepth;
                        if (type == vmpp_CODEC_ENC_H264) {
                            inst->params.videoConfig.gopSize = 0;
                            if (frame.pts == 5)
                            inst->params.videoConfig.lookaheadDepth = 1;
                        }
                        frame.pixelFormat = vmpp_PIX_FMT_YUV420_PLANAR_10BIT_LE;
                        encRet = vmppEncEncodeFrame(chn, &frame, NULL, &stream, 4000);
                        inst->params.videoConfig.gopSize = gopSize;
                        inst->params.videoConfig.lookaheadDepth = lookaheadDepth;
                    } else if (frame.pts % 9 == 6 ) {
                        extParams_lop++;
                        if (extParams_lop == 1 || extParams_lop == 14 ) {
                            struct va_enc_channel *inst = (struct va_enc_channel *)chn;
                            // struct video_encoder_private_context *ctx = (struct video_encoder_private_context *)inst->private_context;
                            
                            vmppEncExtendedParams extParams;
                            memset(&extParams, 0, sizeof(extParams));
                            int8_t roiMapDeltaQp = 1;
                            extParams.roiMap.roiMapDeltaQp = &roiMapDeltaQp;
                            extParams.roiMap.roiMapDeltaQpSize = 1;
                            // ctx->roiType = vmpp_ENC_ROI_MAP;
                            // ctx->roiMemFactory[0].mem.busAddress = 1;
                            frame.pixelFormat = vmpp_PIX_FMT_YUV420_PLANAR_10BIT_P010;

                            inst->params.videoConfig.roiCuCtrlVersion = 6;
                            encRet = vmppEncEncodeFrame(chn, &frame, &extParams, &stream, 4000);
                            inst->params.videoConfig.roiCuCtrlVersion = 1;
                            // ctx->roiType = vmpp_ENC_ROI_RANGE;
                        } else 
                            encRet = vmppEncEncodeFrame(chn, &frame, NULL, &stream, 4000);
                    } else if (frame.pts % 9 == 7 ) {
                        frame.pixelFormat = vmpp_PIX_FMT_GRAY8;
                        encRet = vmppEncEncodeFrame(chn, &frame, NULL, &stream, 4000);
                    } else if (frame.pts % 9 == 8 ) {
                        frame.pixelFormat = vmpp_PIX_FMT_NONE;
                        encRet = vmppEncEncodeFrame(chn, &frame, NULL, &stream, 4000);
                    }
                    LOG_INFO("encRet: %d", encRet);
                    REQUIRE( ( encRet == vmpp_RSLT_ENC_AGAIN || encRet == vmpp_RSLT_ENC_AGAIN_WITH_NO_OUTPUT || encRet == vmpp_RSLT_OK || encRet == vmpp_RSLT_ENC_AGAIN ) );
                    if (encRet < 0) {
                        LOG_ERROR("vmppEncEncodeFrame failed, encRet %d", encRet);
                        raw_close(&raw_ctx);
                        vmppEncDestroyChannel(&chn);
                        return -1;
                    }
                    if (encRet == vmpp_RSLT_ENC_AGAIN ||
                        encRet == vmpp_RSLT_ENC_AGAIN_WITH_NO_OUTPUT) {
                        LOG_WARN("vmpp_RSLT_ENC_AGAIN %ld to %ld", send, send - 1);
                        send--;
                    }
                    if (encRet == vmpp_RSLT_OK || encRet == vmpp_RSLT_ENC_AGAIN) {
                        receive++;
                        encRet = vmppEncReleaseStream(NULL, &stream);
                        REQUIRE( encRet == vmpp_RSLT_ERR_INVALID_PARAMS );
                        encRet = vmppEncReleaseStream(chn, NULL);
                        REQUIRE( encRet == vmpp_RSLT_ERR_INVALID_PARAMS );
                        encRet = vmppEncReleaseStream(NULL, NULL);
                        REQUIRE( encRet == vmpp_RSLT_ERR_INVALID_PARAMS );
                        encRet = vmppEncReleaseStream(chn, &stream);
                        REQUIRE( encRet == vmpp_RSLT_OK );
                        if (encRet < 0) {
                            LOG_ERROR("release stream error %d.", encRet);
                            raw_close(&raw_ctx);
                            vmppEncDestroyChannel(&chn);
                            return -1;
                        }
                    }
                    if (seiData) {
                        free(seiData);
                        seiData = NULL;
                    }
                    if (payloadData) {
                        free(payloadData);
                        payloadData = NULL;
                    }
                } while (encRet == vmpp_RSLT_ENC_AGAIN ||
                         encRet == vmpp_RSLT_ENC_AGAIN_WITH_NO_OUTPUT);
            } while (1);

            // flush
            do {
                memset(&stream, 0, sizeof(vmppStream));
                frame.memoryType = vmpp_MEM_FLUSH;

                flu_lop++;
                if (flu_lop == 2) {
                    struct va_enc_channel *inst = (struct va_enc_channel *)chn;
                    inst->params.videoConfig.lookaheadDepth = 1;
                    // struct video_encoder_private_context *pri_ctx = (struct video_encoder_private_context *)inst->private_context;
                    // pri_ctx->pictureMem[0].used = 1;
                    // pri_ctx->pictureMem[0].sent2Encoder = 0;
                    // pri_ctx->pictureMem[0].number = 121;
                    encRet = vmppEncEncodeFrame(chn, &frame, NULL, &stream, 4000);
                    inst->params.videoConfig.lookaheadDepth = 0;
                    // pri_ctx->pictureMem[0].used = 0;
                } else if (flu_lop == 3) {
                    struct va_enc_channel *inst = (struct va_enc_channel *)chn;
                    // struct video_encoder_private_context *pri_ctx =
                    //     (struct video_encoder_private_context *)inst->private_context;
                    // pri_ctx->parametersSetOutputed = 0;
                    // pri_ctx->inputPictureCount = 123;
                    inst->params.videoConfig.lookaheadDepth = 0;
                    // pri_ctx->pictureMem[0].used = 1;
                    // pri_ctx->pictureMem[0].sent2Encoder = 0;
                    // pri_ctx->pictureMem[0].number = 122;
                    encRet = vmppEncEncodeFrame(chn, &frame, NULL, &stream, 4000);
                }
                else if (flu_lop == 4) {
                    // struct va_enc_channel *inst = (struct va_enc_channel *)chn;
                    // struct video_encoder_private_context *pri_ctx =
                    //     (struct video_encoder_private_context *)inst->private_context;
                    // pri_ctx->pictureMem[0].used = 1;
                    // pri_ctx->pictureMem[0].roiMapDeltaQpMem = 1;
                    encRet = vmppEncEncodeFrame(chn, &frame, NULL, &stream, 4000);
                } else 
                    encRet = vmppEncEncodeFrame(chn, &frame, NULL, &stream, 4000);
                
                REQUIRE(( encRet == vmpp_RSLT_WARN_EOS || encRet == vmpp_RSLT_OK || encRet == vmpp_RSLT_ENC_AGAIN ));
                if (encRet == vmpp_RSLT_WARN_EOS) {
                    if (type != vmpp_CODEC_ENC_JPEG)
                        receive--; // do not count EOF
                    LOG_INFO("Finished");
                    break;
                }

                if (encRet < 0) {
                    LOG_ERROR("vmppEncEncodeFrame failed when flushing, encRet %d", encRet);
                    raw_close(&raw_ctx);
                    vmppEncDestroyChannel(&chn);
                    return -1;
                }

                if (encRet == vmpp_RSLT_OK || encRet == vmpp_RSLT_ENC_AGAIN) {
                    receive++;
                    vmppEncReleaseStream(chn, &stream);
                }
            } while (1);
            // REQUIRE(receive == send);
            // REQUIRE(receive == frameCount);
            raw_close(&raw_ctx);

            LOG_INFO("Encoding details: type %d, send %ld, receive %ld", type, send, receive);

            if (seiData) {
                free(seiData);
                seiData = NULL;
            }
            if (payloadData) {
                free(payloadData);
                payloadData = NULL;
            }
            // 释放内存
            if (frame.data[0]) {
                delete[] frame.data[0];
            frame.data[0] = nullptr;
            }
        }
        vmppEncDestroyChannel(&chn);
    }
    return 0;
}



// --------------------------------------------------------------------------------------
TEST_CASE( "unitest_lowlevel_Initialize_Encoder_simple_normal_enc", "[simple][enc][normal][unitest]" ) {

    REQUIRE( Get_Available_Enc_Channel_Before_Init() == 0 );
    REQUIRE( Create_Enc_Channel_before_Init() == 0 );
    REQUIRE( Initialize_Encoder() == 0 );
    REQUIRE( Create_and_Destroy_Enc_Channel() == 0 );
    REQUIRE( Create_and_Destroy_Enc_Channels(1, vmpp_CODEC_ENC_JPEG) == 0 );
    REQUIRE( Create_and_Destroy_Enc_Channels(1, vmpp_CODEC_ENC_H264) == 0 );
    REQUIRE( Create_and_Destroy_Enc_Channels(1, vmpp_CODEC_ENC_HEVC) == 0 );

    // #define EXECUTE_FULL_UT
    #define TIMES 200 // make this bigger to check long time running cases
    #ifdef EXECUTE_FULL_UT
    REQUIRE( Create_and_Destroy_Enc_Channels(TIMES, vmpp_CODEC_ENC_JPEG) == 0 );
    REQUIRE( Create_and_Destroy_Enc_Channels(TIMES, vmpp_CODEC_ENC_H264) == 0 );
    REQUIRE( Create_and_Destroy_Enc_Channels(TIMES, vmpp_CODEC_ENC_HEVC) == 0 );
    #endif
  
    REQUIRE( Get_Available_Enc_Channel() == 0 );
    REQUIRE( Get_Video_Enc_Version() == 0 );
    REQUIRE( Get_Video_Enc_Caps() == 0 );

    REQUIRE( Encoding(vmpp_CODEC_ENC_JPEG) == 0 );
    REQUIRE( Encoding(vmpp_CODEC_ENC_H264) == 0 );
    REQUIRE( Encoding(vmpp_CODEC_ENC_HEVC) == 0 );
}
// TODO