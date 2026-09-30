#define CATCH_CONFIG_RUNNER
#include "catch2/catch.hpp"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#include "log.h"
#include "stream.h"
#include "utils.h"
#ifdef __cplusplus
}
#endif /* __cplusplus */
#include "vmpp_dec_defs.h"
#include "vmpp_enc_defs.h"

#define OUT_BUF_NUM 4
vmppDevice encFD = -1;
vmppDevice decFD = -1;
vmppRuntimeInstance rtInstance;
vmppEncChannelParameters defaultEncParams;
vmppEncJPEGConfiguration defaultEncParams4JPEG;
vmppEncVideoConfiguration defaultEncParams4Video;
vmppDecChannelParameters defaultDecParams;

class utInstance
{
public:
    utInstance()
    {
        setDefaultEncParams();
        setDefaultDecParams();
        LOG_INFO("UT Instance Constructed...");
    }
    virtual ~utInstance()
    {
        // if (rtInstance.runtimeHandle)
        //     close_runtime(&rtInstance);

        // if (encFD != -1)
        //     close(encFD);

        // if (decFD != -1)
        //     close(decFD);

        // LOG_INFO("UT Instance Destructed...");
    }

private:
    void setDefaultEncParams()
    {
        memset(&defaultEncParams, 0, sizeof(vmppEncChannelParameters));
        memset(&defaultEncParams4JPEG, 0, sizeof(vmppEncJPEGConfiguration));
        memset(&defaultEncParams4Video, 0, sizeof(vmppEncVideoConfiguration));

        // common
        defaultEncParams.enProfiling = 1;
        defaultEncParams.codecType = vmpp_CODEC_ENC_JPEG;
        defaultEncParams.outbufNum = 4;

        // for JPEG
        defaultEncParams4JPEG.frameType = vmpp_PIX_FMT_NV12;
        defaultEncParams4JPEG.codingWidth = 1920;
        defaultEncParams4JPEG.codingHeight = 1080;
        defaultEncParams4JPEG.comLength = 7;
        defaultEncParams4JPEG.pCom = (uint8_t *)"vastai";

        // for video
        defaultEncParams4Video.profile = vmpp_VIDEO_PRFL_HEVC_MAIN;
        defaultEncParams4Video.level = vmpp_VIDEO_LVL_HEVC_6;
        defaultEncParams4Video.width = 1920;
        defaultEncParams4Video.height = 1080;
        defaultEncParams4Video.gopSize = VMPP_ENC_DEFAULT_PAR;
        defaultEncParams4Video.lookaheadDepth = 0;
        defaultEncParams4Video.frameRate.numerator = 30;
        defaultEncParams4Video.frameRate.denominator = 1;
        defaultEncParams4Video.bitDepthLuma = 8;
        defaultEncParams4Video.bitDepthChroma = 8;
        defaultEncParams.outbufNum = 4;
        defaultEncParams4Video.bitRate = 0;
        // defaultEncParams4Video.rcMode = vmpp_RC_CBR;
        defaultEncParams4Video.tune = vmpp_ENC_TUNE_PSNR;
        defaultEncParams4Video.keyInt = VMPP_ENC_DEFAULT_PAR;
        defaultEncParams4Video.crf = VMPP_ENC_DEFAULT_PAR;
        defaultEncParams4Video.cqp = 0;
        defaultEncParams4Video.llRc = 0;
        defaultEncParams4Video.initQp = VMPP_ENC_DEFAULT_PAR;
        defaultEncParams4Video.vbvBufSize = VMPP_ENC_DEFAULT_PAR;
        defaultEncParams4Video.vbvMaxRate = VMPP_ENC_DEFAULT_PAR;
        defaultEncParams4Video.intraQpDelta = VMPP_ENC_DEFAULT_PAR;
        defaultEncParams4Video.qpMinI = VMPP_ENC_DEFAULT_PAR;
        defaultEncParams4Video.qpMaxI = VMPP_ENC_DEFAULT_PAR;
        defaultEncParams4Video.qpMinPB = VMPP_ENC_DEFAULT_PAR;
        defaultEncParams4Video.qpMaxPB = VMPP_ENC_DEFAULT_PAR;
        defaultEncParams4Video.aqStrength = VMPP_ENC_DEFAULT_PAR;
        defaultEncParams4Video.vbr = 0;
        defaultEncParams4Video.qualityMode = vmpp_BRONZE_QUALITY;
        defaultEncParams4Video.gdrDuration = 0;
        // defaultEncParams4Video.enableROI = 0;
        defaultEncParams4Video.P2B = VMPP_ENC_DEFAULT_PAR;
        defaultEncParams4Video.bBPyramid = 1;
        defaultEncParams4Video.maxFrameSizeMultiple = VMPP_ENC_DEFAULT_PAR;
        defaultEncParams4Video.maxFrameSize = VMPP_ENC_DEFAULT_PAR;
        defaultEncParams4Video.roiType = (vmppEncROIType)0;
        defaultEncParams4Video.roiMapDeltaQpBlockUnit = 0;
        defaultEncParams4Video.roiMapQpDeltaVersion = 0;
        defaultEncParams4Video.maxBFrames = VMPP_ENC_DEFAULT_PAR;
        defaultEncParams4Video.hrd = 0;
        defaultEncParams4Video.pictureSkip = 0;
        defaultEncParams4Video.vfr = 0;
        defaultEncParams4Video.svcTLayers = 0;
        defaultEncParams4Video.sliceSize = 0;
        defaultEncParams4Video.ltrInterval = 0;
        defaultEncParams4Video.ltrQpDelta = 0;
        defaultEncParams4Video.ltrRefGap = 0;
        defaultEncParams4Video.openGop = 0;
        defaultEncParams4Video.smartEnc = 0;
        defaultEncParams4Video.disableMMCO = 0;
        defaultEncParams4Video.inLoopDSRatio = VMPP_ENC_DEFAULT_PAR;
        defaultEncParams4Video.aqMode = (vmppEncAQMode)VMPP_ENC_DEFAULT_PAR;
        defaultEncParams4Video.psyFactor = VMPP_ENC_DEFAULT_PAR;
        defaultEncParams4Video.rdoLevel = VMPP_ENC_DEFAULT_PAR;
        defaultEncParams4Video.enableRdoQuant = VMPP_ENC_DEFAULT_PAR;
        defaultEncParams4Video.qCompress = VMPP_ENC_DEFAULT_PAR;
        defaultEncParams4Video.bitRateBalanceLevel = 0;
#ifdef ENABLE_DYNAMIC_RES
        // Do not support these features when running dynamic resolution transcoding case
        defaultEncParams4Video.lookaheadDepth = 0;
        defaultEncParams4Video.gopSize = 1;
#endif
    }

    void setDefaultDecParams()
    {
        memset(&defaultDecParams, 0, sizeof(vmppDecChannelParameters));
        defaultDecParams.device = -1;
        defaultDecParams.extraBufferNumber = 2;
#if 1 // these parameter are reserved.
        defaultDecParams.sourceMode = vmpp_SRC_FRAME;
        defaultDecParams.decodeMode = vmpp_DEC_NORMAL;
        defaultDecParams.maxWidth = 32768;
        defaultDecParams.maxHeight = 32768;
        defaultDecParams.streamBufferSize = MAX_STREAM_SIZE;
        defaultDecParams.pixelFormat = vmpp_PIX_FMT_NV12;
#endif

        defaultDecParams.enProfiling = 1;
    }
};

struct MyListener : Catch::TestEventListenerBase {
    using TestEventListenerBase::TestEventListenerBase;  // inherit constructor
    void testCaseStarting(Catch::TestCaseInfo const &testInfo) override {
        utInstance myutInstance;
        encFD = open("/dev/vastai_video0", O_RDWR);
        decFD = open("/dev/vastai_video0", O_RDWR);
        open_runtime(&rtInstance);
        vaccrt_init_t vaccrt_init = (vaccrt_init_t)rtInstance.init;
        rtError_t vaccRet = vaccrt_init(0);
        if (vaccRet)
        {
            LOG_ERROR("[transcode] vaccrt_init failed: err %d", vaccRet);
            exit(-1);
        }
        vaccRet = testInfo.Benchmark;

    }

    void testCaseEnded(Catch::TestCaseStats const &testCaseStats) override {
        if (rtInstance.runtimeHandle)
            close_runtime(&rtInstance);

        if (encFD != -1)
            close(encFD);

        if (decFD != -1)
            close(decFD);
        
        decFD = testCaseStats.aborting;
        LOG_INFO("UT Instance Destructed...");
    }
};

CATCH_REGISTER_LISTENER(MyListener);


int main( int argc, char* argv[] ) {
    
    int result = Catch::Session().run( argc, argv );
    return result;
}

// exclude case by name: ./build/out/dec_ut exclude:multi_thread_video_dec_stress_testing exclude:case_name
// exclude case by tag:  ./build/out/dec_ut ~[threads_120] ~[tag]