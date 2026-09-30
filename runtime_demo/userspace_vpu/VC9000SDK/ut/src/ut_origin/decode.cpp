#include <unistd.h>
#include "vmpp_dec_api.h"
#include "vmpp_dec_defs.h"
#include "catch2/catch.hpp"
#include <string>
#include "decode.hpp"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
// #include "log.h"
#include "md5.h"
#include "stream.h"
#include "utils.h"
#ifdef __cplusplus
}
#endif /* __cplusplus */

extern vmppRuntimeInstance rtInstance;
extern int decFD;
extern vmppDecChannelParameters defaultDecParams;

int Initialize_Decoder()
{
    vmppResult ret = vmpp_RSLT_OK;
    vmppConfiguration cfg;
    memset(&cfg, 0, sizeof(vmppConfiguration));
    cfg.runtimeInst = rtInstance;

    ret = vmppInitDecoder(NULL);
    REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );

    cfg.runtimeInst.mallocVideo = NULL;
    ret = vmppInitDecoder(&cfg);
    REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );

    cfg.runtimeInst = rtInstance;
    cfg.logCtx.enableCustomLog = 1;
    ret = vmppInitDecoder(&cfg);
    REQUIRE( ret == vmpp_RSLT_OK );

    cfg.runtimeInst = rtInstance;
    cfg.logCtx.enableCustomLog = 0;
    ret = vmppInitDecoder(&cfg);
    REQUIRE( ret == vmpp_RSLT_OK );

    return 0;
}

static void loadDefaultParams(vmppCodecType type, vmppDecChannelParameters *params)
{
    memcpy(params, &defaultDecParams, sizeof(vmppDecChannelParameters));
    params->codecType = type;
}

int Create_Dec_Channel_before_Init()
{
    vmppResult ret = vmpp_RSLT_OK;
    vmppChannel chn = NULL;

    vmppDecChannelParameters params;

    memset(&params, 0, sizeof(params));
    loadDefaultParams(vmpp_CODEC_DEC_JPEG, &params);
    params.device = (vmppDevice)decFD;
    ret = vmppDecCreateChannel(&chn, &params);
    REQUIRE( ret == vmpp_RSLT_RUNTIME_INVALID);
    return 0;
}

int Create_and_Destroy_Dec_Channel_with_GetStatus_GetIdleDpbBufferCount(vmppCodecType type)
{
    vmppResult ret = vmpp_RSLT_OK;
    vmppChannel chn = NULL;
    int count = 0;
    vmppDecStatus status;
    memset(&status, 0, sizeof(status));

    vmppDecChannelParameters params;

    try {
        memset(&params, 0, sizeof(params));
        loadDefaultParams(vmpp_CODEC_DEC_JPEG, &params);
        params.device = (vmppDevice)decFD;

        /* Invalid parameters cases */
        ret = vmppDecCreateChannel(NULL, &params);
        REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );

        ret = vmppDecCreateChannel(&chn, NULL);
        REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );

        ret = vmppDecCreateChannel(NULL, NULL);
        REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );

        params.codecType = vmpp_CODEC_ENC_JPEG;
        ret = vmppDecCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_ERR_UNSUPPORTED );

        params.codecType = vmpp_CODEC_ENC_H264;
        ret = vmppDecCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_ERR_UNSUPPORTED );

        params.codecType = vmpp_CODEC_ENC_HEVC;
        ret = vmppDecCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_ERR_UNSUPPORTED );

        memset(&params, 0, sizeof(params));
        loadDefaultParams(type, &params);
        params.device = (vmppDevice)decFD;

        int chns = vmppDecGetAvailableChannels(decFD, params.codecType);
        REQUIRE( chns > 0 );
        if (chns > 0) {
            if (type != vmpp_CODEC_DEC_JPEG) {
                // params.codecType = vmpp_CODEC_ENC_H264;
                params.pixelFormat = vmpp_PIX_FMT_NV21;
                ret = vmppDecCreateChannel(&chn, &params);
                REQUIRE( ret == vmpp_RSLT_ERR_UNSUPPORTED );
                params.pixelFormat = vmpp_PIX_FMT_NV12;
            }
            ret = vmppDecCreateChannel(&chn, &params);
            REQUIRE( ret == vmpp_RSLT_OK ); vmppDecDestroyChannel(&chn);
            params.pixelFormat = vmpp_PIX_FMT_YUV420_PLANAR_10BIT_P010;
            ret = vmppDecCreateChannel(&chn, &params);
            REQUIRE( ret == vmpp_RSLT_OK ); vmppDecDestroyChannel(&chn);
            params.pixelFormat = vmpp_PIX_FMT_YUV420_PLANAR_10BIT_I010;
            ret = vmppDecCreateChannel(&chn, &params);
            REQUIRE( ret == vmpp_RSLT_OK ); vmppDecDestroyChannel(&chn);
            params.decodeMode = vmpp_DEC_INTRA_ONLY;
            ret = vmppDecCreateChannel(&chn, &params);
            REQUIRE( ret == vmpp_RSLT_OK );

            ret = vmppDecGetStatus(NULL, &status); //vmppDecGetStatus
            REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );

            ret = vmppDecGetStatus(chn, NULL);
            REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );

            ret = vmppDecGetStatus(NULL, NULL);
            REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );

            ret = vmppDecGetStatus(chn, &status);
            REQUIRE( ret == vmpp_RSLT_OK );

            count = vmppDecGetIdleDpbBufferCount(NULL); //vmppDecGetIdleDpbBufferCount
            REQUIRE( count == 0 );
            count = vmppDecGetIdleDpbBufferCount(chn); //vmppDecGetIdleDpbBufferCount
            REQUIRE( count >= 0 );

            if (ret == vmpp_RSLT_OK) {
                ret = vmppDecDestroyChannel(&chn);
                REQUIRE( ret == vmpp_RSLT_OK );
            }
        }
    } catch (...) {
        return -1;
    }

    return 0;
}

int Create_and_Destroy_Dec_Channels(int channels, vmppCodecType type)
{
    vmppResult ret = vmpp_RSLT_OK;
    vmppChannel chn = NULL;
    int chns = 0;

    vmppDecChannelParameters params;
    memset(&params, 0, sizeof(params));
    loadDefaultParams(type, &params);

    params.device = (vmppDevice)decFD;

    int chns_start = vmppDecGetAvailableChannels(decFD, params.codecType);
    if (chns_start > 0) {
        for (int i = 0; i < channels; i++) {
            ret = vmppDecCreateChannel(&chn, &params);
            REQUIRE( ret == vmpp_RSLT_OK);

            ret = vmppDecDestroyChannel(&chn);
            REQUIRE( ret == vmpp_RSLT_OK);
            if ((i < chns_start && i % chns_start == (chns_start - 1)) || (i-chns_start) % 180 == (180 - 1)) {
                chns = vmppDecGetAvailableChannels(decFD, params.codecType);
                close(decFD);
                decFD = open("/dev/vastai_video0", O_RDWR);
                chns = vmppDecGetAvailableChannels(decFD, params.codecType);
                REQUIRE( chns == 360);
            }
        }
    }

    return 0;
}

int Get_Available_Dec_Channel_Before_Init()
{
    int chns = vmppDecGetAvailableChannels(decFD, vmpp_CODEC_DEC_H264);
    REQUIRE( chns > 0 );
    return 0;
}


int Get_Video_Dec_Version()
{
    vmppVersion *ret; 
    for (int i = 0; i < 100; i++) {
        ret = vmppDecGetVersion();
        REQUIRE( ret != NULL );
    }
    return 0;
}

int Get_Video_Dec_Caps()
{
    try {
        vmppDecVideoCapability cap_video = {0};
        vmppDecJpegCapability cap_jpeg = {0};
        vmppDecGetVideoCaps(vmpp_CODEC_DEC_H264, NULL);
        vmppDecGetVideoCaps(vmpp_CODEC_DEC_HEVC, NULL);
        vmppDecGetVideoCaps(vmpp_CODEC_DEC_JPEG, NULL);
        vmppDecGetVideoCaps(vmpp_CODEC_ENC_JPEG, NULL);
        vmppDecGetVideoCaps(vmpp_CODEC_ENC_H264, NULL);
        vmppDecGetVideoCaps(vmpp_CODEC_ENC_HEVC, NULL);
        vmppDecGetJpegCaps(NULL);

        vmppDecGetVideoCaps(vmpp_CODEC_DEC_H264, &cap_video);
        vmppDecGetVideoCaps(vmpp_CODEC_DEC_HEVC, &cap_video);
        vmppDecGetVideoCaps(vmpp_CODEC_DEC_JPEG, &cap_video);
        vmppDecGetVideoCaps(vmpp_CODEC_ENC_JPEG, &cap_video);
        vmppDecGetVideoCaps(vmpp_CODEC_ENC_H264, &cap_video);
        vmppDecGetVideoCaps(vmpp_CODEC_ENC_HEVC, &cap_video);
        vmppDecGetJpegCaps(&cap_jpeg);
    } catch (...) {
        return -1;
    }
    return 0;
}

struct dec_thread_params {
    vmppChannel chn;
    int receive;
    uint8_t md5[MD5_HASH_LEN];
};

static void *output_thread(void *arg)
{
    struct dec_thread_params *params = (struct dec_thread_params *)arg;
    vmppFrame out_frame;
    vmppDecOutputOptions out_opt;
    out_opt.memoryType = vmpp_MEM_HOST;
    out_opt.enableCrop = 1;
    int ret, ret_out;
    struct md5_context md5ctx = {0};
    md5_init(&md5ctx);
    while (1) {
        memset(&out_frame, 0, sizeof(out_frame));
        ret = vmppDecReceiveFrame(NULL, &out_frame, &out_opt, 4000);
        REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );
        ret = vmppDecReceiveFrame(params->chn, NULL, &out_opt, 4000);
        REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );
        if (params->receive % 3 == 0) {
            ret = vmppDecReceiveFrame(params->chn, &out_frame, &out_opt, 0);
        } else if (params->receive % 3 == 1) {
            struct va_dec_channel *inst = (struct va_dec_channel *)params->chn;
            struct h264_decoder_private_context *ctx =
                (struct h264_decoder_private_context *)inst->private_context;
                ctx->dec_info.interlaced_sequence = 1;
            ret = vmppDecReceiveFrame(params->chn, &out_frame, &out_opt, 4000);
        } else {
            ret = vmppDecReceiveFrame(params->chn, &out_frame, &out_opt, 4000);
        }
        REQUIRE(( ret == vmpp_RSLT_OK || ret == vmpp_RSLT_WARN_EOS || ret == vmpp_RSLT_WARN_MORE_DATA));
        ret_out = ret;

        if (ret == vmpp_RSLT_OK) {
            // ########## test case vmppDecTransferFrame ##########
            ret = vmppDecTransferFrame(NULL, &out_frame, 0);
            REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );
            ret = vmppDecTransferFrame(NULL, &out_frame, 1);
            REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );
            ret = vmppDecTransferFrame(params->chn, NULL, 0);
            REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );
            ret = vmppDecTransferFrame(params->chn, NULL, 1);
            REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );
            
            if (params->receive % 2 == 0) {
                out_frame.memoryType = vmpp_MEM_DEVICE;
                ret = vmppDecTransferFrame(params->chn, &out_frame, 1);
                REQUIRE( ret == vmpp_RSLT_OK ); 
            } else if (params->receive % 2 == 1) {
                ret = vmppDecTransferFrame(params->chn, &out_frame, 0);
                REQUIRE( ret == vmpp_RSLT_WARN_REPEAT_OPERATION );
            }
        }

        if (ret_out == vmpp_RSLT_OK) {
            params->receive++;
            md5_update(&md5ctx, (const unsigned char *)out_frame.data[0],
                       (unsigned long)out_frame.dataSize);
            ret = vmppDecReleaseFrame(NULL, &out_frame, 4000);
            REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );
            ret = vmppDecReleaseFrame(params->chn, NULL, 4000);
            REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );
            ret = vmppDecReleaseFrame(NULL, NULL, 4000);     //timeout - reserved
            REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );
            ret = vmppDecReleaseFrame(params->chn, &out_frame, 4000);
            REQUIRE( ret == vmpp_RSLT_OK );
            if (ret < 0)
                LOG_WARN("release frame error %d", ret);
        } else if (ret == vmpp_RSLT_WARN_EOS) {
            md5_final(params->md5, &md5ctx);
            break;
        } else if (ret == vmpp_RSLT_WARN_MORE_DATA) {
            usleep(1000);
        } else {
            LOG_ERROR("receive frame error %d", ret);
            break;
        }
    }
    return NULL;
}

static void HexToBytes(const std::string &hex, uint8_t *result)
{
    for (uint32_t index = 0; index < hex.length(); index += 2) {
        std::string byteString = hex.substr(index, 2);
        uint8_t byte = (uint8_t)strtol(byteString.c_str(), NULL, 16);
        result[index / 2] = byte;
    }
}

static int get_stream_info(vmppStream *stream, vmppCodecType codecType, uint32_t *width, uint32_t *height)
{
    vmppDecVideoInfo videoInfo = {0};

    vmppResult ret = vmppDecGetVideoInfo(stream, codecType, &videoInfo);
    if (ret == vmpp_RSLT_OK) {
        *width = videoInfo.width;
        *height = videoInfo.height;
        LOG_INFO(
            "video info(size %d x %d, cropFlag %d, cw %d, ch %d, xoffset %d, yoffset %d, fps_d "
            "%d, fps_n %d, pf %d).\n",
            videoInfo.width, videoInfo.height, videoInfo.cropFlag, videoInfo.cropWidth,
            videoInfo.cropHeight, videoInfo.xOffset, videoInfo.yOffset, videoInfo.fps.denominator,
            videoInfo.fps.numerator, videoInfo.pixelFormat);

        vmppDecVideoInfo info_video;
        memset(&info_video, 0, sizeof(vmppDecVideoInfo));
        // ########## test case vmppDecGetVideoInfo ##########
        ret = vmppDecGetVideoInfo(NULL, codecType, &info_video);
        REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );
        ret = vmppDecGetVideoInfo(stream, (vmppCodecType)-1, &info_video);
        REQUIRE( ret == vmpp_RSLT_ERR_UNSUPPORTED );
        ret = vmppDecGetVideoInfo(stream, codecType, NULL);
        REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );
        ret = vmppDecGetVideoInfo(stream, vmpp_CODEC_ENC_JPEG, &info_video);
        REQUIRE( ret == vmpp_RSLT_ERR_UNSUPPORTED );
        ret = vmppDecGetVideoInfo(stream, vmpp_CODEC_DEC_JPEG, &info_video);
        REQUIRE( ret == vmpp_RSLT_ERR_UNSUPPORTED );
        ret = vmppDecGetVideoInfo(stream, vmpp_CODEC_ENC_H264, &info_video);
        REQUIRE( ret == vmpp_RSLT_ERR_UNSUPPORTED );
        ret = vmppDecGetVideoInfo(stream, vmpp_CODEC_ENC_HEVC, &info_video);
        REQUIRE( ret == vmpp_RSLT_ERR_UNSUPPORTED );
        
        if (codecType == vmpp_CODEC_DEC_HEVC) {
            ret = vmppDecGetVideoInfo(stream, vmpp_CODEC_DEC_HEVC, &info_video);
            REQUIRE( ret == vmpp_RSLT_OK );
            ret = vmppDecGetVideoInfo(stream, vmpp_CODEC_DEC_H264, &info_video);
            REQUIRE( ret == vmpp_RSLT_ERR_GET_INFO );
        } else {
            ret = vmppDecGetVideoInfo(stream, vmpp_CODEC_DEC_HEVC, &info_video);
            CHECK( ret == vmpp_RSLT_ERR_GET_INFO );
            ret = vmppDecGetVideoInfo(stream, vmpp_CODEC_DEC_H264, &info_video);
            REQUIRE( ret == vmpp_RSLT_OK );
            stream->len = 0;
            ret = vmppDecGetVideoInfo(stream, vmpp_CODEC_DEC_H264, &info_video);
            REQUIRE( ret == vmpp_RSLT_ERR_INVALID_DATA );
        }

        return 0;
    }

    return -1;
}

int Decode(vmppCodecType type)
{
    if (type != vmpp_CODEC_DEC_JPEG && type != vmpp_CODEC_DEC_HEVC && type != vmpp_CODEC_DEC_H264) {
        LOG_WARN("Unsupported codec type %d", type);
        return -1;
    }

    vmppResult decRet = vmpp_RSLT_OK;
    vmppChannel chn = NULL;

    vmppDecStreamInfo info;

    vmppDecJpegInfo info_Jpeg;

    vmppDecChannelParameters params;
    memset(&params, 0, sizeof(params));
    loadDefaultParams(type, &params);

    params.device = (vmppDevice)decFD;

    int chns = vmppDecGetAvailableChannels(decFD, params.codecType);
    REQUIRE( chns > 0 );
    if (chns > 0) {

        stream_context_ptr strmctx = NULL;
        char input[MAX_PATH_LEN] = {0};
        std::string yuvMD5String;
        int frameNum = 0;
        int strmtype;
        int32_t tmp_ret = 0;
        switch (type) {
        case vmpp_CODEC_DEC_JPEG:
            sprintf(input, "%s/1920x1080_1.jpg", UT_RES_PATH);
            frameNum = 1;
            strmtype = BIT_STREAM_JPEG;
            break;
        case vmpp_CODEC_DEC_H264:
            sprintf(input, "%s/1920x1088_121.h264", UT_RES_PATH);
            frameNum = 121;
            strmtype = BIT_STREAM_H264;
            yuvMD5String = "F3914240F42C8445E459C8752C713EEF";
            break;
        case vmpp_CODEC_DEC_HEVC:
        default:
            sprintf(input, "%s/1920x1088_121.hevc", UT_RES_PATH);
            frameNum = 121;
            strmtype = BIT_STREAM_HEVC;
            yuvMD5String = "B24FE8A79E98F3A876611EAB74F5D4FB";
            break;
        }

        decRet = vmppDecCreateChannel(&chn, &params);
        if (decRet != vmpp_RSLT_OK || !chn) {
            LOG_ERROR("create channel error %d or chn is null.\n", decRet);
            return -1;
        }

        strmctx = stream_open(input, strmtype);
        if (!strmctx) {
            LOG_ERROR("Unable to open input file <%s>", input);
            vmppDecDestroyChannel(&chn);
            return -1;
        }
        if (strmtype == BIT_STREAM_JPEG)
            strmctx->size = stream_size(strmctx);
        
        decRet = vmppDecStart(NULL);
        REQUIRE( decRet == vmpp_RSLT_ERR_INVALID_PARAMS );
        decRet = vmppDecStart(chn); // set start status.
        REQUIRE( decRet == vmpp_RSLT_OK );
        
        if (decRet < 0) {
            LOG_ERROR("Start recv stream error %d", decRet);
            stream_close(&strmctx);
            vmppDecDestroyChannel(&chn);
            return -1;
        }

        decRet = vmppDecGetStreamInfo(NULL, &info);
        REQUIRE( decRet == vmpp_RSLT_ERR_INVALID_PARAMS );
        decRet = vmppDecGetStreamInfo(chn, NULL);
        REQUIRE( decRet == vmpp_RSLT_ERR_INVALID_PARAMS );
        decRet = vmppDecGetStreamInfo(NULL, NULL);
        REQUIRE( decRet == vmpp_RSLT_ERR_INVALID_PARAMS );
        decRet = vmppDecGetStreamInfo(chn, &info);
        REQUIRE( decRet == vmpp_RSLT_OK );

        pthread_t thread_handle;
        struct dec_thread_params params = {0};
        params.chn = chn;
        tmp_ret = pthread_create(&thread_handle, NULL, output_thread, &params);
        if (tmp_ret != 0) {
            LOG_ERROR("Fail to start output thread %d", tmp_ret);

            decRet = vmppDecStop(chn);
            REQUIRE( decRet == vmpp_RSLT_OK );

            stream_close(&strmctx);
            vmppDecDestroyChannel(&chn);
            return -1;
        }

        uint32_t stream_len = 0;
        uint8_t *stream_p = NULL;
        int64_t pts_cnt = 0;
        vmppStream input_stream = {0};

        if (type != vmpp_CODEC_DEC_JPEG) {
            uint32_t w, h;
            while (1) {
                tmp_ret = stream_read_frame(strmctx, &stream_p);
                if (tmp_ret <= 0) {
                    if (tmp_ret == 0 && stream_eof(strmctx)) {
                        LOG_DEBUG("[decode] stream end");
                        break;
                    }
                    LOG_ERROR("stream_read_frame failed, tmp_ret:%d",tmp_ret);
                    break;
                }
                //  else if (tmp_ret < 0 && input_len < tmp_buf_len) {
                //     /* Insufficient buffer size */
                //     delete[] input_buf;
                //     input_buf = new (std::nothrow) uint8_t[tmp_buf_len];
                //     if (!input_buf) {
                //         LOG_ERROR("Fail to realloc input buffer, size %d", tmp_buf_len);
                //         vmppDecStop(chn);
                //         pthread_join(thread_handle, NULL);
                //         stream_close(&strmctx);
                //         vmppDecDestroyChannel(&chn);
                //         return -1;
                //     }
                //     input_len = tmp_buf_len;
                //     continue;
                // }

                stream_len = (uint32_t)tmp_ret;

                input_stream.len = (uint32_t)tmp_ret;
                input_stream.pts = 0;
                if (get_stream_info(&input_stream, type, &w, &h) == 0)
                    break;
            }
            stream_seek_to_start(strmctx);
        }
        // end for case vmppDecGetVideoInfo

        do {
            tmp_ret = stream_read_frame(strmctx, &stream_p);
            if (tmp_ret <= 0) {
                if (tmp_ret == 0 && stream_eof(strmctx)) {
                    LOG_DEBUG("[decode] stream end");
                    break;
                }
                LOG_ERROR("stream_read_frame failed, tmp_ret:%d",tmp_ret);
                break;
            }

            stream_len = (uint32_t)tmp_ret;

            pts_cnt++;

            input_stream.stream = stream_p;
            input_stream.len = 0;
            input_stream.pts = pts_cnt;
            decRet = vmppDecSendStream(NULL, &input_stream, 4000);
            REQUIRE( decRet == vmpp_RSLT_ERR_INVALID_PARAMS );
            decRet = vmppDecSendStream(chn, NULL, 4000);
            REQUIRE( decRet == vmpp_RSLT_ERR_INVALID_PARAMS );
            decRet = vmppDecSendStream(chn, &input_stream, 4000);
            REQUIRE( decRet == vmpp_RSLT_ERR_INVALID_PARAMS );
            input_stream.len = stream_len;
            if (pts_cnt % 2 == 0) {
                decRet = vmppDecSendStream(chn, &input_stream, 20); // Timeout<4000 is too small, using default minimum value 4000
            } else {
                if (type == vmpp_CODEC_DEC_HEVC) {
                struct va_dec_channel *inst = (struct va_dec_channel *)chn;
                struct hevc_decoder_private_context *ctx =
                    (struct hevc_decoder_private_context *)inst->private_context;
                ctx->prev_width = 1;
                ctx->prev_buf_width = 1;
                decRet = vmppDecSendStream(chn, &input_stream, 20);
                }else
                    decRet = vmppDecSendStream(chn, &input_stream, 4000);
            }
            REQUIRE( decRet >= 0 );

            if (type == vmpp_CODEC_DEC_JPEG) {
                int ret_Jpeg_info = vmppDecGetJpegInfo(NULL, &info_Jpeg);
                REQUIRE(ret_Jpeg_info == vmpp_RSLT_ERR_INVALID_PARAMS);
                ret_Jpeg_info = vmppDecGetJpegInfo(&input_stream, NULL);
                REQUIRE(ret_Jpeg_info == vmpp_RSLT_ERR_INVALID_PARAMS);
                ret_Jpeg_info = vmppDecGetJpegInfo(NULL, NULL);
                REQUIRE(ret_Jpeg_info == vmpp_RSLT_ERR_INVALID_PARAMS);
                ret_Jpeg_info = vmppDecGetJpegInfo(&input_stream, &info_Jpeg);
                REQUIRE(ret_Jpeg_info == vmpp_RSLT_OK);
                input_stream.stream = NULL;
                ret_Jpeg_info = vmppDecGetJpegInfo(&input_stream, &info_Jpeg);
                REQUIRE(ret_Jpeg_info == vmpp_RSLT_ERR_INVALID_DATA);
            }
            
            if (decRet < 0) {
                LOG_ERROR("vmppDecSendStream (%ld), error: %d", pts_cnt, decRet);
                vmppDecStop(chn);
                pthread_join(thread_handle, NULL);
                stream_close(&strmctx);
                vmppDecDestroyChannel(&chn);
                return -1;
            }
        } while (1);

        decRet = vmppDecStop(NULL);
        REQUIRE( decRet == vmpp_RSLT_ERR_INVALID_PARAMS );
        decRet = vmppDecStop(chn);
        REQUIRE( decRet >= vmpp_RSLT_OK );
        if (decRet < 0) {
            LOG_ERROR("stop recv stream error %d", decRet);
        }
        pthread_join(thread_handle, NULL);
        decRet = vmppDecDestroyChannel(&chn);
        if (decRet < 0)
            LOG_ERROR("destroy dec chn error %d", decRet);
        stream_close(&strmctx);
        LOG_INFO("Decoding Finished: type %d, count %d, expected %d", type, params.receive,
                 frameNum);
        REQUIRE( frameNum == params.receive );
        if (strmtype != BIT_STREAM_JPEG) {
            uint8_t yuvMD5[MD5_HASH_LEN] = {0};
            HexToBytes(yuvMD5String, yuvMD5);
            LOG_INFO("Target MD5:");
            print_md5(yuvMD5, MD5_HASH_LEN);
            LOG_INFO("Result MD5:");
            print_md5(params.md5, MD5_HASH_LEN);
            REQUIRE( memcmp(yuvMD5, params.md5, sizeof(yuvMD5)) == 0 );
        }
    }

    return 0;
}

int Get_Available_Dec_Channel(vmppCodecType type)
{
    vmppResult ret = vmpp_RSLT_OK;
    vmppChannel chn = NULL;
    vmppDecChannelParameters params;
    memset(&params, 0, sizeof(params));
    loadDefaultParams(type, &params);

    params.device = (vmppDevice)decFD;

    // invalid parameter
    vmppDevice dummyDev = -1;
    int chns = vmppDecGetAvailableChannels(dummyDev, params.codecType);
    REQUIRE( chns == -1 );

    chns = vmppDecGetAvailableChannels(decFD, params.codecType);
    REQUIRE( chns >= 0 );
    if (chns > 0) {
        ret = vmppDecCreateChannel(&chn, &params);
        REQUIRE( ret == vmpp_RSLT_OK );

        int chns2 = vmppDecGetAvailableChannels(decFD, params.codecType);
        REQUIRE( chns2 == chns - 1 );

        ret = vmppDecDestroyChannel(NULL);
        REQUIRE( ret == vmpp_RSLT_ERR_INVALID_PARAMS );
        ret = vmppDecDestroyChannel(&chn); // decoder can not recovery channel count after destroy
        REQUIRE( ret == vmpp_RSLT_OK );

        close(decFD);
        decFD = open("/dev/vastai_video0", O_RDWR);

        int chns3 = vmppDecGetAvailableChannels(decFD, params.codecType);
        REQUIRE(chns3 == 360 /*chns*/);
    }

    return 0;
}

// --------------------------------------------------------------------------------------
TEST_CASE( "unitest_lowlevel_Initialize_Decoder_simple_normal_dec", "[simple][dec][normal][unitest]" ) {
    
    REQUIRE( Get_Available_Dec_Channel_Before_Init() == 0 );
    REQUIRE( Create_Dec_Channel_before_Init() == 0 );
    REQUIRE( Initialize_Decoder() == 0 );
    REQUIRE( Create_and_Destroy_Dec_Channel_with_GetStatus_GetIdleDpbBufferCount(vmpp_CODEC_DEC_JPEG) == 0 );
    REQUIRE( Create_and_Destroy_Dec_Channel_with_GetStatus_GetIdleDpbBufferCount(vmpp_CODEC_DEC_H264) == 0 );
    REQUIRE( Create_and_Destroy_Dec_Channel_with_GetStatus_GetIdleDpbBufferCount(vmpp_CODEC_DEC_HEVC) == 0 );
    REQUIRE( Create_and_Destroy_Dec_Channels(1, vmpp_CODEC_DEC_JPEG) == 0 );
    REQUIRE( Create_and_Destroy_Dec_Channels(1, vmpp_CODEC_DEC_H264) == 0 );
    REQUIRE( Create_and_Destroy_Dec_Channels(1, vmpp_CODEC_DEC_HEVC) == 0 );

    REQUIRE( Decode(vmpp_CODEC_DEC_JPEG) == 0 );
    REQUIRE( Decode(vmpp_CODEC_DEC_H264) == 0 );
    REQUIRE( Decode(vmpp_CODEC_DEC_HEVC) == 0 );

    // #define EXECUTE_FULL_UT
    #define TIMES 200 // make this bigger to check long time running cases
    #ifdef EXECUTE_FULL_UT
    REQUIRE( Create_and_Destroy_Dec_Channels(TIMES, vmpp_CODEC_DEC_JPEG) == 0 );
    REQUIRE( Create_and_Destroy_Dec_Channels(TIMES, vmpp_CODEC_DEC_H264) == 0 );
    REQUIRE( Create_and_Destroy_Dec_Channels(TIMES, vmpp_CODEC_DEC_HEVC) == 0 );
    #endif

    REQUIRE( Get_Available_Dec_Channel(vmpp_CODEC_DEC_JPEG) == 0 );
    REQUIRE( Get_Available_Dec_Channel(vmpp_CODEC_DEC_H264) == 0 );
    REQUIRE( Get_Available_Dec_Channel(vmpp_CODEC_DEC_HEVC) == 0 );
    REQUIRE( Get_Video_Dec_Version() == 0 );
    REQUIRE( Get_Video_Dec_Caps() == 0 );
    
}

// TODO