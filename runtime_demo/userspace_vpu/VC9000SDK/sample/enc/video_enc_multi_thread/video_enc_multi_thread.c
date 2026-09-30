#include <assert.h>
#include <ctype.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "ffmpeg-wrapper.h"
#include "stream.h"
#include "utils.h"

#include "defs.h"
#include "option.h"
#include "queue.h"
#include "vmpp_enc_api.h"
#include "vmpp_enc_defs.h"

#define OUT_BUF_NUM (4)
//#define RELEASE_THREAD_TEST

#define MAX_ENC_CHANNEL_COUNT (24)

// endof  the va_dec.h

static struct option_t ops[] = {
    {"help", 'H', 2},
    {"input", 'i', 1},
    {"output", 'o', 1},
    {"width", 'w', 1},
    {"height", 'h', 1},
    {"stride", 't', 1}, /* Input image format */
    {"thread", 'T', 1}, /* thread count */
    {"encDevice", 'e', 1},
    {"memDevice", 'm', 1},
    {"pixelFormat", 'f', 1},
    {"loop", 'l', 1},
    {"save", 's', 1},
    {"codecFormat", 'c', 1},
    {"auto devices", 'a', 1},
    {"vframes", 'n', 1},
    /* Only long option can be used for all the following parameters because
     * we have no more letters to use. All shortOpt=0 will be identified by
     * long option. */
    {"profile", '0', 1},
    {"level", '0', 1},
    {"frameRateNum", '0', 1},
    {"frameRateDen", '0', 1},
    {"bitDepthLuma", '0', 1},
    {"bitDepthChroma", '0', 1},
    {"gopSize", '0', 1},
    {"gdrDuration", '0', 1},
    {"lookaheadDepth", '0', 1},
    {"qualityMode", '0', 1},
    {"keyInt", '0', 1},
    {"crf", '0', 1},
    {"cqp", '0', 1},
    {"llRc", '0', 1},
    {"bitRate", '0', 1},
    {"initQp", '0', 1},
    {"vbvBufSize", '0', 1},
    {"vbvMaxRate", '0', 1}, /* max bitrate for CPB VBR/CBR */
    {"intraQpDelta", '0', 1},
    {"qpMinI", '0', 1},
    {"qpMaxI", '0', 1},
    {"qpMinPB", '0', 1},
    {"qpMaxPB", '0', 1},
    {"tolCtbRcInter", '0', 1},
    {"aqStrength", '0', 1},
    {"tune", '0', 1},
    {"P2B", '0', 1},
    {"bBPyramid", '0', 1},
    {"maxFrameSizeMultiple", '0', 1},
    {"maxFrameSize", '0', 1},
    {"outbufNum",'0',1},
    {"roiType", '0', 1},
    {"roiInt", '0', 1},
    {"roiParam", '0', 1},
    {"extSEIInt", '0', 1},
    {"forceIDRInt", '0', 1},
    {"logLevel", '0', 1},
    {"roiMapDeltaQpBlockUnit", '0', 1},
    {"roiMapQpDeltaVersion", '0', 1},
    {"enableDynamicBitrate", '0', 1},
    {"enableDynamicFrameRate", '0', 1},
    {"maxBFrames", '0', 1},
    {"hrd", '0', 1},
    {"picSkip", '0', 1},
    {"vfr", '0', 1},
    {"svcTLayers", '0', 1},
    {"svcExtractMaxTLayer", '0', 1},
    {"sliceSize", '0', 1},
    {"enableDynamicCrf", '0', 1},
    {"psnr", '0', 1},
    {"ssim", '0', 1},
    {"ltrInterval", '0', 1},
    {"ltrQpDelta", '0', 1},
    {"ltrRefGap", '0', 1},
    {"ltrInsertTest", '0', 1},
    {"rotation", '0', 1},
    {"coreID", '0', 1},
    {"openGop", '0', 1},
    {"smartEnc", '0', 1},
    {"enableSpsCropInfo", '0', 1},
    {"cropRect", '0', 1},
    {"enableDynamicKeyInt", '0', 1},
    {"disableMMCO", '0', 1},
    {"inLoopDSRatio", '0', 1},
    {"aqMode", '0', 1},
    {"psyFactor", '0', 1},
    {"rdoLevel", '0', 1},
    {"enableRdoQuant", '0', 1},
    {"qCompress", '0', 1},
    {"bitRateBalanceLevel", '0', 1},
    {NULL, 0, 0} /* Format of last line */
};

//static char *default_dev_str = "/dev/vastai_video0";
typedef int RET_TYPE;
typedef void *task_handle;
typedef void *(*task_func)(void *);
#define X_FRAMES 30
uint64_t time_start = 0;
uint64_t time_end = 0;
uint64_t time_tick = 0;
uint64_t time_total_frames = 0;
uint64_t total_bits = 0;
uint64_t last_total_bits = 0;
static int thread_count = 2;

// vmppStream stream[OUT_BUF_NUM];

typedef struct {
    int argc;
    char **argv;
} MainArgs;

// options struct
typedef struct {
    char*                           encDevice;                  // video encoder device node name
    char*                           memDevice;                  // memory device node name
    char *input_file;
    char *output_file;
    int width;
    int height;
    int stride;
    enum vmppPixelFormat pixel_format;
    int loop;
    int store;
    int codec; // 0 for h264, 1 for hevc, 2 for av1
    int vframes;
    int auto_devices;

    unsigned int profile;
    unsigned int level;

    unsigned int frameRateNum;
    unsigned int frameRateDen;
    unsigned int bitDepthLuma;
    unsigned int bitDepthChroma;
    unsigned int gopSize;     // sequence level GOP size, set gopSize=0 for adaptive GOP size.
    unsigned int gdrDuration; // canada no_mcu commit 174aeb4d4a3c0ef09660f9c9dda0b1534c653e6a
    unsigned int lookaheadDepth;

    unsigned int qualityMode;
    unsigned int tune;
    unsigned int keyInt; // IDR interval

    int crf;
    unsigned int cqp;
    unsigned int llRc;

    unsigned int bitRate;
    unsigned int initQp;
    unsigned int vbvBufSize;
    unsigned int vbvMaxRate;
    signed int intraQpDelta;
    unsigned int qpMinI;
    unsigned int qpMaxI;
    unsigned int qpMinPB;
    unsigned int qpMaxPB;
    unsigned int vbr;
    float aqStrength;
    unsigned int P2B;
    unsigned int bBPyramid;
    float maxFrameSizeMultiple; /**< deprecated! maximum multiple to average target frame size */
    signed int maxFrameSize;    /* max frame size, only valid in llrc mode*/

    unsigned int outbufNum;

    /* For roi, 0 for none, 1 for roi range, 2 for roi map.*/
    unsigned int roiType;
    /* For roi range*/
    unsigned int roiInt;      // ROI interval
    char *roiParam;           // ROI param
    unsigned int extSEIInt;   // extSEI interval
    unsigned int forceIDRInt; // forceIDR interval
    unsigned int logLevel;    // log level for SDK

    /* For roi map*/
    unsigned int roiMapDeltaQpBlockUnit;
    unsigned int roiMapQpDeltaVersion;
    unsigned int enableDynamicBitrate;
    unsigned int enableDynamicFrameRate;
    unsigned int maxBFrames;
    unsigned int hrd;
    unsigned int pictureSkip;
    unsigned int vfr;
    unsigned int svcTLayers;
    unsigned int svcExtractMaxTLayer;
    unsigned int sliceSize;
    unsigned int enableDynamicCrf;

    int enableCalcPSNR;
    int enableCalcSSIM;

    /**
     * There are for LongTerm parameters
     * Must confirm that gopsize is 1
     */
    unsigned int ltrInterval;                // 0: disable ltr, 1~max frame num
    int          ltrQpDelta;                 // default 0
    unsigned int ltrRefGap;                  // the frame gap that references the ltr, 1~ltrInterval,
    unsigned int ltrInsertTest;
    unsigned int rotation;
    unsigned int coreID;
    unsigned int openGop;
    unsigned int smartEnc;
    unsigned int enableSpsCropInfo;
    char* cropRect;
    unsigned int  enableDynamicKeyInt;
    unsigned int disableMMCO;

    unsigned int inLoopDSRatio;
    unsigned int aqMode;
    float psyFactor;
    unsigned int rdoLevel;
    unsigned int enableRdoQuant;
    double qCompress;
    unsigned int bitRateBalanceLevel;
} enc_options;

typedef struct {
    void *enc_ch;
    enc_options *options;
} thread_param_t;



static enum vmppPixelFormat get_pixel_format(const char *pf)
{
    enum vmppPixelFormat pixel_format = vmpp_PIX_FMT_NONE;
    if (!strcmp(pf, "nv12")) {
        pixel_format = vmpp_PIX_FMT_NV12;
    } else if (!strcmp(pf, "nv21")) {
        pixel_format = vmpp_PIX_FMT_NV21;
    } else if (!strcmp(pf, "yuv420p")) {
        pixel_format = vmpp_PIX_FMT_YUV420P;
    } else if (!strcmp(pf, "yuv420p_10bit")) {
        pixel_format = vmpp_PIX_FMT_YUV420_PLANAR_10BIT_LE;
    } else if (!strcmp(pf, "p010le")) {
        pixel_format = vmpp_PIX_FMT_YUV420_PLANAR_10BIT_P010;
    } else if (!strcmp(pf, "rgba")) {
        pixel_format = vmpp_PIX_FMT_RGBA;
    } else {
        pixel_format = vmpp_PIX_FMT_NONE;
    }
    return pixel_format;
}

static void usage(const char *program)
{
    printf("Usage: %s [options]\n", program);
    printf("  -i                 url for input file\n");
    printf("  -o                 url for output folder\n");
    printf("  -w                 width\n");
    printf("  -h                 height\n");
    printf("  -t                 stride\n");
    printf("  -T                 thread count, [1, 24]\n");
    printf("  -e                 video encoder device name\n");
    printf("  -m                 memory device  name\n");
    printf("  -f                 pixel format\n");
    printf("  -l                 loop time, if loop = 0, it will loop forever\n");
    printf("  -C                 core mode, default 0, 0-auto, 1-single core, 2-multicore\n");
    printf("  -s                 stored in output file or not, 0/1\n");
    printf("  -c                 codec format: 0 for h264, 1 for hevc, 2 for av1, default is 0\n");
    printf("  -n                 yuv frame numbers to be encoded\n");
    printf("  -a                 auto select devices\n");
    printf("  --profile          main profile or main still picture profile\n");
    printf("  --level            main profile level\n");
    printf("  --frameRateNum     frame rate numerator\n");
    printf("  --frameRateDen     frame rate denominator\n");
    printf("  --bitDepthLuma     luma bit depth\n");
    printf("  --bitDepthChroma   chroma bit depth\n");
    printf("  --gopSize          gop size\n");
    printf("  --gdrDuration      gdr duration\n");
    printf("  --lookaheadDepth   lookahead depth\n");
    printf("  --qualityMode      quality mode\n");
    printf("  --tune             tune\n");
    printf("  --keyInt           IDR interval\n");
    printf("  --crf              CRF constant\n");
    printf("  --cqp              cqp\n");
    printf("  --llRc             llRc\n");
    printf("  --bitRate          bps, bit rate,\n");
    printf("  --cqp              cqp\n");
    printf("  --initQp           init qp\n");
    printf("  --vbvBufSize       kb, vbv Buffer size\n");
    printf("  --vbvMaxRate       kbps, vbv max rate\n");
    printf("  --intraQpDelta     intra qp delta\n");
    printf("  --qpMinI           min qp value of I frame\n");
    printf("  --qpMaxI           max qp value of I frame\n");
    printf("  --qpMinPB          min qp value of B/P frame\n");
    printf("  --qpMaxPB          max qp value of B/P frame\n");
    printf("  --aqStrength       aq strength\n");
    printf("  --P2B              P2B\n");
    printf("  --bBPyramid        bBPyramid\n");
    printf("  --maxFrameSizeMultiple        maxFrameSizeMultiple\n");
    printf("  --maxFrameSize     max frame size\n");
    printf("  --outbufNum        max output buf number\n");
    printf("  --roiType           ROI type, 0 for none, 1 for roi range, 2 for roi map\n");
    printf("  --roiInt           ROI interval\n");
    printf("  --roiParam           ROI param\n");
    printf("  --extSEIInt        extSEI interval\n");
    printf("  --forceIDRInt      forceIDR interval\n");
    printf("  --logLevel         log level for SDK, default: 2(INFO),  1-DEBUG 2-INFO 3-WARN 4-ERROR\n");
    printf("  --roiMapDeltaQpBlockUnit      roi map delta qp block unit\n");
    printf("  --roiMapQpDeltaVersion        roi map delta qp version\n");
    printf("  --enableDynamicBitrate        enable dynamic bitrate or not(1/0)\n");
    printf("  --enableDynamicFrameRate      enable dynamic framerate or not(1/0)\n");
    printf("  --maxBFrames       max B frames control for adaptive GOP decision\n");
    printf("  --hrd              Hypothetical Reference Decoder model\n");
    printf("  --picSkip          Frame All Skip Mode When Overflow\n");
    printf("  --vfr              variable frame rate\n");
    printf("  --svcTLayers              Temporal Layers for Scalable Video Coding\n");
    printf("  --svcExtractMaxTLayer     Max Temporal Layer to Extract for Scalable Video Coding\n");
    printf("  --sliceSize        Slice size in CTB/MB rows for multislice\n");
    printf("  --enableDynamicCrf        enable dynamic crf or not(1/0)\n");
    printf("  --psnr        enable caculation of PSNR or not, default: 0\n");
    printf("  --ssim        enable caculation of SSIM or not, default: 0\n");
    printf("  --ltrInterval        enable LTR, default: 0\n");
    printf("  --ltrQpDelta        set LTR frame QpDelta, default: 0\n");
    printf("  --ltrRefGap        set frame gap that references the LTR, default: 0\n");
    printf("  --ltrInsertTest    Enable test insert LTR: 0\n");
    printf("  --rotation    Rotate input image, 0-Disabled (Default), 1-90 degrees right, 2-90 degrees left, 3-180 degrees right\n");
    printf("  --coreID           Enable specify core id: 0-3\n");
    printf("  --openGop           openGop\n");
    printf("  --smartEnc          smartEnc Mode\n");
    printf("  --enableSpsCropInfo     enable sps crop rect info\n");
    printf("  --cropRect           sps crop rect info\n");
    printf("  --enableDynamicKeyInt        enable dynamic keyInt or not(1/0)\n");
    printf("  --disableMMCO      disable h264 memory management control operation or not(1/0)\n");
    printf("  --inLoopDSRatio    in-loop downsample ratio for first pass(1/0)\n");
    printf("  --aqMode           aq mode:0-3\n");
    printf("  --psyFactor        weight of psycho-visual encoding:0.0-4.0\n");
    printf("  --rdoLevel         RDO Level can balance the quality and throughput:1-3\n");
    printf("  --enableRdoQuant   enable RDO quantization or not(1/0)\n");
    printf("  --qCompress        qCompress sets the quantizer curve compression factor:0.0-1.0\n");
    printf("  --bitRateBalanceLevel        set the matching degree between the encoding output bitrate and the target bitrate in simple or static scenarios:0-4\n");
    printf("example: ./video_enc_mt -i /home/stone/workspace/YUV/akiyo_352x288_300.yuv -o output -w 352 -h 288 -t 352 -e /dev/hantroenc -m /dev/memalloc  -f yuv420p -c 0 -T 4\n");
    printf("        ./video_enc_mt -i /home/stone/workspace/YUV/akiyo_352x288_300.yuv -o output -w 352 -h 288 -t 352 -e /dev/hantroenc -m /dev/memalloc  -f yuv420p -c 1 -T 4\n");

}

static int parse_options(int argc, char **argv, enc_options *options)
{
    struct parameter prm;
    int ret;
    char *optarg;
    prm.cnt = 1;
    while ((ret = get_option(argc, argv, ops, &prm)) != -1) {
        if  (ret == -2 && prm.enable == 1) {
            LOG_ERROR("Unassigned value,please check the parameters");
            usage(argv[0]);
            return -1;
        }
        optarg = prm.argument;
        switch (prm.short_opt) {
        case 'H':
            usage(argv[0]);
            return -1;
        case 'i':
            options->input_file = optarg;
            break;
        case 'o':
            options->output_file = optarg;
            break;
        case 'w':
            options->width = atoi(optarg);
            break;
        case 'h':
            options->height = atoi(optarg);
            break;
        case 't':
            options->stride = atoi(optarg);
            break;
        case 'e':
            options->encDevice = optarg;
            break;
        case 'm':
            options->memDevice = optarg;
            break;
        case 'f':
            options->pixel_format = get_pixel_format(optarg);
            break;
        case 'l':
            options->loop = atoi(optarg);
            break;
        case 's':
            options->store = atoi(optarg);
            break;
        case 'T':
            thread_count = atoi(optarg);
            if (thread_count < 1 || thread_count > MAX_ENC_CHANNEL_COUNT) {
                LOG_ERROR("thread_count is invalid, please check the parameters");
                return -1;
            }
            break;
        case 'c':
            options->codec = atoi(optarg);
            break;
        case 'a':
            options->auto_devices = atoi(optarg);
            break;
        case 'n':
            options->vframes = atoi(optarg);
            break;
        case '0':
            if (strcmp(prm.longOpt, "profile") == 0)
                options->profile = atoi(optarg);
            if (strcmp(prm.longOpt, "level") == 0)
                options->level = atoi(optarg);
            if (strcmp(prm.longOpt, "frameRateNum") == 0)
                options->frameRateNum = atoi(optarg);
            if (strcmp(prm.longOpt, "frameRateDen") == 0)
                options->frameRateDen = atoi(optarg);
            if (strcmp(prm.longOpt, "bitDepthLuma") == 0)
                options->bitDepthLuma = atoi(optarg);
            if (strcmp(prm.longOpt, "bitDepthChroma") == 0)
                options->bitDepthChroma = atoi(optarg);
            if (strcmp(prm.longOpt, "gopSize") == 0)
                options->gopSize = atoi(optarg);
            if (strcmp(prm.longOpt, "gdrDuration") == 0)
                options->gdrDuration = atoi(optarg);
            if (strcmp(prm.longOpt, "lookaheadDepth") == 0)
                options->lookaheadDepth = atoi(optarg);
            if (strcmp(prm.longOpt, "qualityMode") == 0)
                options->qualityMode = atoi(optarg);
            if (strcmp(prm.longOpt, "tune") == 0)
                options->tune = atoi(optarg);
            if (strcmp(prm.longOpt, "keyInt") == 0)
                options->keyInt = atoi(optarg);
            if (strcmp(prm.longOpt, "crf") == 0)
                options->crf = atoi(optarg);
            if (strcmp(prm.longOpt, "cqp") == 0)
                options->cqp = atoi(optarg);
            if (strcmp(prm.longOpt, "llRc") == 0)
                options->llRc = atoi(optarg);
            if (strcmp(prm.longOpt, "bitRate") == 0)
                options->bitRate = atoi(optarg);
            if (strcmp(prm.longOpt, "initQp") == 0)
                options->initQp = atoi(optarg);
            if (strcmp(prm.longOpt, "vbvBufSize") == 0)
                options->vbvBufSize = atoi(optarg) * 1000;
            if (strcmp(prm.longOpt, "vbvMaxRate") == 0)
                options->vbvMaxRate = atoi(optarg) * 1000;
            if (strcmp(prm.longOpt, "intraQpDelta") == 0)
                options->intraQpDelta = atoi(optarg);
            if (strcmp(prm.longOpt, "qpMinI") == 0)
                options->qpMinI = atoi(optarg);
            if (strcmp(prm.longOpt, "qpMaxI") == 0)
                options->qpMaxI = atoi(optarg);
            if (strcmp(prm.longOpt, "qpMinPB") == 0)
                options->qpMinPB = atoi(optarg);
            if (strcmp(prm.longOpt, "qpMaxPB") == 0)
                options->qpMaxPB = atoi(optarg);
            if (strcmp(prm.longOpt, "aqStrength") == 0)
                options->aqStrength = atof(optarg);
            if (strcmp(prm.longOpt, "P2B") == 0)
                options->P2B = atoi(optarg);
            if (strcmp(prm.longOpt, "bBPyramid") == 0)
                options->bBPyramid = atoi(optarg);
            if (strcmp(prm.longOpt, "maxFrameSizeMultiple") == 0)
                options->maxFrameSizeMultiple = atof(optarg);
            if (strcmp(prm.longOpt, "maxFrameSize") == 0)
                options->maxFrameSize = atoi(optarg);
            if (strcmp(prm.longOpt, "outbufNum") == 0)
                options->outbufNum = atoi(optarg);
            if (strcmp(prm.longOpt, "roiType") == 0)
                options->roiType = atoi(optarg);
            if (strcmp(prm.longOpt, "roiInt") == 0)
                options->roiInt = atoi(optarg);
            if (strcmp(prm.longOpt, "roiParam") == 0)
                options->roiParam = optarg;
            if (strcmp(prm.longOpt, "extSEIInt") == 0)
                options->extSEIInt = atoi(optarg);
            if (strcmp(prm.longOpt, "forceIDRInt") == 0)
                options->forceIDRInt = atoi(optarg);
            if (strcmp(prm.longOpt, "logLevel") == 0)
                options->logLevel = atoi(optarg);
            if (strcmp(prm.longOpt, "roiMapDeltaQpBlockUnit") == 0)
                options->roiMapDeltaQpBlockUnit = atoi(optarg);
            if (strcmp(prm.longOpt, "roiMapQpDeltaVersion") == 0)
                options->roiMapQpDeltaVersion = atoi(optarg);
            if (strcmp(prm.longOpt, "enableDynamicBitrate") == 0)
                options->enableDynamicBitrate = atoi(optarg);
            if (strcmp(prm.longOpt, "enableDynamicFrameRate") == 0)
                options->enableDynamicFrameRate = atoi(optarg);
            if (strcmp(prm.longOpt, "maxBFrames") == 0)
                options->maxBFrames = atoi(optarg);
            if (strcmp(prm.longOpt, "hrd") == 0)
                options->hrd = atoi(optarg);
            if (strcmp(prm.longOpt, "picSkip") == 0)
                options->pictureSkip = atoi(optarg);
            if (strcmp(prm.longOpt, "vfr") == 0)
                options->vfr = atoi(optarg);
            if (strcmp(prm.longOpt, "svcTLayers") == 0)
                options->svcTLayers = atoi(optarg);
            if (strcmp(prm.longOpt, "svcExtractMaxTLayer") == 0)
                options->svcExtractMaxTLayer = atoi(optarg);
            if (strcmp(prm.longOpt, "sliceSize") == 0)
                options->sliceSize = atoi(optarg);
            if (strcmp(prm.longOpt, "enableDynamicCrf") == 0)
                options->enableDynamicCrf = atoi(optarg);
            if (strcmp(prm.longOpt, "psnr") == 0)
                options->enableCalcPSNR = atoi(optarg);
            if (strcmp(prm.longOpt, "ssim") == 0)
                options->enableCalcSSIM = atoi(optarg);
            if (strcmp(prm.longOpt, "ltrInterval") == 0)
                options->ltrInterval = atoi(optarg);
            if (strcmp(prm.longOpt, "ltrQpDelta") == 0)
                options->ltrQpDelta = atoi(optarg);
            if (strcmp(prm.longOpt, "ltrRefGap") == 0)
                options->ltrRefGap = atoi(optarg);
            if (strcmp(prm.longOpt, "ltrInsertTest") == 0)
                options->ltrInsertTest = atoi(optarg);
            if (strcmp(prm.longOpt, "rotation") == 0)
                options->rotation = atoi(optarg);
            if (strcmp(prm.longOpt, "coreID") == 0)
                options->coreID = atoi(optarg);
            if (strcmp(prm.longOpt, "openGop") == 0)
                options->openGop = atoi(optarg);
            if (strcmp(prm.longOpt, "smartEnc") == 0)
                options->smartEnc = atoi(optarg);
            if (strcmp(prm.longOpt, "enableSpsCropInfo") == 0)
                options->enableSpsCropInfo = atoi(optarg);
            if (strcmp(prm.longOpt, "cropRect") == 0)
                options->cropRect = optarg;
            if (strcmp(prm.longOpt, "enableDynamicKeyInt") == 0)
                options->enableDynamicKeyInt = atoi(optarg);
            if (strcmp(prm.longOpt, "disableMMCO") == 0)
                options->disableMMCO = atoi(optarg);
            if (strcmp(prm.longOpt, "inLoopDSRatio") == 0)
                options->inLoopDSRatio = atoi(optarg);
            if (strcmp(prm.longOpt, "aqMode") == 0)
                options->aqMode = atoi(optarg);
            if (strcmp(prm.longOpt, "psyFactor") == 0)
                options->psyFactor = atof(optarg);
            if (strcmp(prm.longOpt, "rdoLevel") == 0)
                options->rdoLevel = atoi(optarg);
            if (strcmp(prm.longOpt, "enableRdoQuant") == 0)
                options->enableRdoQuant = atoi(optarg);
            if (strcmp(prm.longOpt, "qCompress") == 0)
                options->qCompress = atof(optarg);
            if (strcmp(prm.longOpt, "bitRateBalanceLevel") == 0)
                options->bitRateBalanceLevel = atoi(optarg);
            break;
        default:
            usage(argv[0]);
            return -1;
        }
    }
    return 0;
}


void default_params(enc_options *option)
{
    if (!option) {
        fprintf(stderr, "set param error, ch_apr is null.\n");
    }

    // set default values
    option->input_file = "/home/vastai/resource/dataset/yuv/1080P/xg1920x1088_x265_121_num.yuv";
    option->width = 1920;
    option->height = 1088;
    option->stride = 0;
    option->encDevice = "/dev/hantroenc";
    option->memDevice = "/dev/memalloc";
    option->pixel_format = vmpp_PIX_FMT_NV12;
    option->loop = 0;
    option->store = 1;
    option->codec = 0;
    option->vframes = 0;
    option->output_file = NULL;
    /*if (option->codec == 0) {
        option->profile = vmpp_VIDEO_PRFL_H264_HIGH;
        option->level = vmpp_VIDEO_LVL_H264_5_1;
    } else if (option->codec == 1) {
        option->profile = vmpp_VIDEO_PRFL_HEVC_MAIN;
        option->level = vmpp_VIDEO_LVL_HEVC_6;
    }*/
    option->profile = 0;
    option->level = 0;
    option->gopSize = VMPP_ENC_DEFAULT_PAR;
    option->frameRateNum = 30;
    option->frameRateDen = 1;
    option->bitDepthLuma = 8;
    option->bitDepthChroma = 8;
    option->lookaheadDepth = 0;
    option->tune = vmpp_ENC_TUNE_PSNR;
    option->keyInt = VMPP_ENC_DEFAULT_PAR;
    option->crf = VMPP_ENC_DEFAULT_PAR;
    option->cqp = 0;
    option->llRc = 0;
    option->bitRate = 0;
    option->initQp = VMPP_ENC_DEFAULT_PAR;
    option->vbvBufSize = VMPP_ENC_DEFAULT_PAR;
    option->vbvMaxRate = VMPP_ENC_DEFAULT_PAR;
    option->intraQpDelta = VMPP_ENC_DEFAULT_PAR;
    option->qpMinI = VMPP_ENC_DEFAULT_PAR;
    option->qpMaxI = VMPP_ENC_DEFAULT_PAR;
    option->qpMinPB = VMPP_ENC_DEFAULT_PAR;
    option->qpMaxPB = VMPP_ENC_DEFAULT_PAR;
    option->aqStrength = VMPP_ENC_DEFAULT_PAR;
    option->qualityMode = vmpp_BRONZE_QUALITY;
    option->vbr = 0;
    option->gdrDuration = 0;
    option->P2B = VMPP_ENC_DEFAULT_PAR;
    option->bBPyramid = 1;
    option->maxFrameSizeMultiple = VMPP_ENC_DEFAULT_PAR;
    option->maxFrameSize = VMPP_ENC_DEFAULT_PAR;
    option->outbufNum = OUT_BUF_NUM;
    option->roiType = 0;
    option->roiInt = 0;
    option->roiParam = "top=0,left=0,bottom=200,right=200,qpType=0,qpValue=1";
    option->extSEIInt = 0;
    option->forceIDRInt = 0;
    option->logLevel = 2; // INFO
    option->roiMapDeltaQpBlockUnit = 0;
    option->roiMapQpDeltaVersion = 0;
    option->enableDynamicBitrate = 0;
    option->enableDynamicFrameRate = 0;
    option->maxBFrames = VMPP_ENC_DEFAULT_PAR;
    option->hrd = 0;
    option->pictureSkip = 0;
    option->vfr = 0;
    option->svcTLayers = 0;
    option->svcExtractMaxTLayer = VMPP_ENC_DEFAULT_PAR;
    option->sliceSize = 0;
    option->enableDynamicCrf = 0;
    option->enableCalcPSNR = 0;
    option->enableCalcSSIM = 0;
    option->ltrInterval = 0;
    option->ltrQpDelta = 0;
    option->ltrRefGap = 0;
    option->ltrInsertTest = 0;
    option->rotation = 0;
    option->coreID = VMPP_ENC_DEFAULT_PAR;
    option->openGop = 0;
    option->smartEnc = 0;
    option->enableSpsCropInfo = 0;
    option->cropRect = "xOffset=0,cropWidth=0,yOffset=0,cropHeight=0";
    option->enableDynamicKeyInt = 0;
    option->disableMMCO = 0;
    option->inLoopDSRatio = VMPP_ENC_DEFAULT_PAR;
    option->aqMode = VMPP_ENC_DEFAULT_PAR;
    option->psyFactor = VMPP_ENC_DEFAULT_PAR;
    option->rdoLevel = VMPP_ENC_DEFAULT_PAR;
    option->enableRdoQuant = VMPP_ENC_DEFAULT_PAR;
    option->qCompress = VMPP_ENC_DEFAULT_PAR;
    option->bitRateBalanceLevel = 0;
}

void set_params(enc_options *option, vmppEncChannelParameters *ch_apr)
{
    if (!option) {
        fprintf(stderr, "set param error, option is null.\n");
    }
    if (!ch_apr) {
        fprintf(stderr, "set param error, ch_apr is null.\n");
    }

    ch_apr->encDevice           = option->encDevice;
    ch_apr->memDevice           = option->memDevice;
    ch_apr->videoConfig.width = option->width;
    ch_apr->videoConfig.height = option->height;
    ch_apr->codecType = option->codec == 0  ? vmpp_CODEC_ENC_H264 : option->codec == 1 ? vmpp_CODEC_ENC_HEVC : vmpp_CODEC_ENC_AV1;
    ch_apr->videoConfig.profile = (vmppVideoProfile)option->profile;
    ch_apr->videoConfig.level = (vmppVideoLevel)option->level;
    if (ch_apr->codecType == vmpp_CODEC_ENC_H264) {
        if (ch_apr->videoConfig.profile < vmpp_VIDEO_PRFL_H264_BASELINE ||
            ch_apr->videoConfig.profile > vmpp_VIDEO_PRFL_H264_HIGH_10) {
            ch_apr->videoConfig.profile = vmpp_VIDEO_PRFL_H264_HIGH;
            ch_apr->videoConfig.level = vmpp_VIDEO_LVL_H264_5_1;
        }
    } else if (ch_apr->codecType == vmpp_CODEC_ENC_HEVC) {
        if (ch_apr->videoConfig.profile < vmpp_VIDEO_PRFL_HEVC_MAIN ||
            ch_apr->videoConfig.profile > vmpp_VIDEO_PRFL_HEVC_MAIN_REXT) {
            ch_apr->videoConfig.profile = vmpp_VIDEO_PRFL_HEVC_MAIN;
            ch_apr->videoConfig.level = vmpp_VIDEO_LVL_HEVC_6;
        }
    } else if (ch_apr->codecType == vmpp_CODEC_ENC_AV1) {
        if (ch_apr->videoConfig.profile < vmpp_VIDEO_PRFL_AV1_MAIN ||
            ch_apr->videoConfig.profile > vmpp_VIDEO_PRFL_AV1_PROFESSIONAL) {
            ch_apr->videoConfig.profile = vmpp_VIDEO_PRFL_AV1_MAIN;
        }
        ch_apr->videoConfig.level = 0;
    }
    if (ch_apr->videoConfig.level == 0)
    {
        if (ch_apr->codecType == vmpp_CODEC_ENC_H264)
            ch_apr->videoConfig.level = vmpp_VIDEO_LVL_H264_5_1;
        else if (ch_apr->codecType == vmpp_CODEC_ENC_HEVC)
            ch_apr->videoConfig.level = vmpp_VIDEO_LVL_HEVC_6;
    }
    ch_apr->videoConfig.frameRate.numerator = option->frameRateNum;
    ch_apr->videoConfig.frameRate.denominator = option->frameRateDen;
    ch_apr->videoConfig.bitDepthLuma = option->bitDepthLuma;
    if (ch_apr->videoConfig.bitDepthLuma > 8)
    {
        if (ch_apr->codecType == vmpp_CODEC_ENC_H264)
            ch_apr->videoConfig.profile = vmpp_VIDEO_PRFL_H264_HIGH_10;
        else if (ch_apr->codecType == vmpp_CODEC_ENC_HEVC)
            ch_apr->videoConfig.profile = vmpp_VIDEO_PRFL_HEVC_MAIN_10;
    }
    ch_apr->videoConfig.bitDepthChroma = option->bitDepthChroma;
    ch_apr->outbufNum = option->outbufNum;
    ch_apr->videoConfig.lookaheadDepth = option->lookaheadDepth;
    ch_apr->videoConfig.tune = (vmppEncTuneType)option->tune;
    ch_apr->videoConfig.keyInt = option->keyInt;
    ch_apr->videoConfig.crf = option->crf;
    ch_apr->videoConfig.cqp = option->cqp;
    ch_apr->videoConfig.llRc = option->llRc;
    ch_apr->videoConfig.bitRate = option->bitRate;
    ch_apr->videoConfig.initQp = option->initQp;
    ch_apr->videoConfig.vbvBufSize = option->vbvBufSize;
    ch_apr->videoConfig.vbvMaxRate = option->vbvMaxRate;
    ch_apr->videoConfig.intraQpDelta = option->intraQpDelta;
    ch_apr->videoConfig.qpMinI = option->qpMinI;
    ch_apr->videoConfig.qpMaxI = option->qpMaxI;
    ch_apr->videoConfig.qpMinPB = option->qpMinPB;
    ch_apr->videoConfig.qpMaxPB = option->qpMaxPB;
    ch_apr->videoConfig.aqStrength = option->aqStrength;
    ch_apr->videoConfig.qualityMode = (vmppEncQualityMode)option->qualityMode;
    ch_apr->videoConfig.vbr = option->vbr;
    ch_apr->videoConfig.gopSize = option->gopSize;
    ch_apr->videoConfig.gdrDuration = option->gdrDuration;
    ch_apr->videoConfig.P2B = option->P2B;
    ch_apr->videoConfig.bBPyramid = option->bBPyramid;
    ch_apr->videoConfig.maxFrameSizeMultiple = option->maxFrameSizeMultiple;
    ch_apr->videoConfig.maxFrameSize = option->maxFrameSize;
    ch_apr->videoConfig.roiType = option->roiType;
    ch_apr->videoConfig.roiMapDeltaQpBlockUnit = option->roiMapDeltaQpBlockUnit;
    ch_apr->videoConfig.roiMapQpDeltaVersion = option->roiMapQpDeltaVersion;
    ch_apr->videoConfig.maxBFrames = option->maxBFrames;
    ch_apr->videoConfig.hrd = option->hrd;
    ch_apr->videoConfig.pictureSkip = option->pictureSkip;
    ch_apr->videoConfig.vfr = option->vfr;
    ch_apr->videoConfig.svcTLayers = option->svcTLayers;
    ch_apr->videoConfig.alignmentEnable = 0;
    ch_apr->videoConfig.sliceSize = option->sliceSize;
    ch_apr->videoConfig.ltrInterval = option->ltrInterval;
    ch_apr->videoConfig.ltrQpDelta = option->ltrQpDelta;
    ch_apr->videoConfig.ltrRefGap = option->ltrRefGap;
    ch_apr->videoConfig.preProcess.rotation = option->rotation;
    ch_apr->videoConfig.vbr = (option->coreID + (VMPP_SPECIFY_COREID << 16));
    ch_apr->videoConfig.openGop = option->openGop;
    ch_apr->videoConfig.smartEnc = option->smartEnc;
    ch_apr->videoConfig.disableMMCO = option->disableMMCO;
    ch_apr->videoConfig.inLoopDSRatio = option->inLoopDSRatio;
    ch_apr->videoConfig.aqMode = option->aqMode;
    ch_apr->videoConfig.psyFactor = option->psyFactor;
    ch_apr->videoConfig.rdoLevel = option->rdoLevel;
    ch_apr->videoConfig.enableRdoQuant = option->enableRdoQuant;
    ch_apr->videoConfig.qCompress = option->qCompress;
    ch_apr->videoConfig.bitRateBalanceLevel = option->bitRateBalanceLevel;
}

struct thread_opts {
    enc_options *opts;
    vmppChannel ch;
    vmppEncChannelParameters param;
    int thread_id;
};

void update_psnr(vmppStream *pStream, enc_options *options, double psnr_total[], int *psnr_num)
{
    if (pStream->psnrInfo[0] && pStream->psnrInfo[1] && pStream->psnrInfo[2]) {
        int lum_max_value, cbcr_max_value;
        lum_max_value = (1 << options->bitDepthLuma) - 1;
        cbcr_max_value = (1 << options->bitDepthChroma) - 1;

        double y_psnr, cb_psnr, cr_psnr;
        y_psnr  =  10.0 * log10f(lum_max_value * lum_max_value / pStream->psnrInfo[0]);
        cb_psnr =  10.0 * log10f(cbcr_max_value * cbcr_max_value / pStream->psnrInfo[1]);
        cr_psnr =  10.0 * log10f(cbcr_max_value * cbcr_max_value / pStream->psnrInfo[2]);

        psnr_total[0] += y_psnr;
        psnr_total[1] += cb_psnr;
        psnr_total[2] += cr_psnr;

        // LOG_DEBUG("MSE %4.2f %4.2f %4.2f\n", pStream->psnrInfo[0], pStream->psnrInfo[1], pStream->psnrInfo[2]);
        // LOG_DEBUG("psnr_total %4.2f %4.2f %4.2f\n", psnr_total[0], psnr_total[1], psnr_total[2]);

        *psnr_num = *psnr_num + 1;
    }
}

void update_ssim(vmppStream *pStream, double psnr_total[], int *ssim_num)
{
    if (pStream->psnrInfo[3] && pStream->psnrInfo[4] && pStream->psnrInfo[5]) {
        // ssim[3] = (ssim[0] * 4 + ssim[1] + ssim[2]) / 6;

        psnr_total[3] += pStream->psnrInfo[3];
        psnr_total[4] += pStream->psnrInfo[4];
        psnr_total[5] += pStream->psnrInfo[5];

        // LOG_DEBUG("SSIM %4.2f %4.2f %4.2f\n", pStream->psnrInfo[3], pStream->psnrInfo[4], pStream->psnrInfo[5]);
        // LOG_DEBUG("ssim_total %4.2f %4.2f %4.2f\n", psnr_total[3], psnr_total[4], psnr_total[5]);

        *ssim_num = *ssim_num + 1;
    }
}

static void *enc_thread(void *arg)
{
    struct thread_opts *topts = (struct thread_opts *)arg;
    enc_options *options = topts->opts;
    vmppChannel ch = NULL; // topts->ch;
    int thread_id = topts->thread_id;
    vmppResult enc_ret;

    int top = 0, left = 0, right = 0, bottom = 0;
    int qpType = 0, qpValue = 0;

    /* For roi map */
    int roimap_value[3] = {30, 10, -15};
    int roimap_index = 0;
    uint32_t blksize = 0;
    uint32_t roiwidth, roiheight, roimap_size;
    int8_t *roi_map_delta_qp_buffer = NULL;

    double psnr_total[6];
    int psnr_num = 0;
    int ssim_num = 0;

    memset(&psnr_total[0], 0, sizeof(double) * 6);

    /* create cahnnel */
    enc_ret = vmppEncCreateChannel(&ch, &topts->param);
    if (enc_ret != vmpp_RSLT_OK) {
        fprintf(stderr, "send frame error %d.\n", enc_ret);
        return 0;
    }

    struct raw_context raw_ctx;
    struct raw_context *raw_ctx_ptr = NULL;
    int ret = 0;
    int pic_size = 0;
    int comp1_size, comp2_size, comp3_size;
#ifdef ENABLE_DYNAMIC_RES
    struct raw_context raw_ctx_2;
    vmppFrame dummyFrame = {0};
    int comp1_size1, comp2_size1, comp3_size1;
    int comp1_size2, comp2_size2, comp3_size2;
#endif
    vmppFrame frame;
    uint32_t seiCount = 1;
    int loop_count = 0;
    char output_file[MAX_PATH_LEN] = {0};
    vmppStream stream;
    memset(&stream, 0, sizeof(vmppStream));
    vmppEncExtendedParams extParams = {0};
    vmppResult rel_ret;
    FILE *file_output = NULL;
    struct vmpp_queue *frame_counts;
    vmppSEI *sei = NULL;
    if (vmpp_queue_init(&frame_counts) < 0)
        goto thread_end;

    sprintf(output_file, "%s/enc_%d_%d.%s", options->output_file, thread_id, loop_count,
            options->codec == 0 ? "h264" : (options->codec == 1 ? "hevc" : "ivf"));
    sei = (vmppSEI *)malloc(sizeof(vmppSEI));
    if (sei) {
        sei->nalType = vmpp_SEI_PREFIX;
        sei->payloadType = SEI_USER_DATA_UNREGISTERED;
        sei->payloadData = (uint8_t *)"0123456789ABCDEF-01234567890123456789012345678901234567890";
        sei->payloadDataSize = strlen((char *)sei->payloadData);
    }
    ret = raw_open(options->input_file, options->pixel_format, options->width, options->height,
                   options->stride, &raw_ctx);
    if (ret < 0) {
        LOG_ERROR("Failed to open input file %s", options->input_file);
        goto thread_end;
    }
    pic_size = raw_pic_size(&raw_ctx, &comp1_size, &comp2_size, &comp3_size);
#ifdef ENABLE_DYNAMIC_RES
    ret = raw_open("/home/vastai/resource/dataset/yuv/nv12/Tennis_1280x720_24.yuv",
                   vmpp_PIX_FMT_NV12, 1280, 720, 1280, &raw_ctx_2);
#define MAX_FRAME_SIZE 10 * 1024 * 1024
    pic_size = MAX_FRAME_SIZE;
    raw_pic_size(&raw_ctx_2, &comp1_size2, &comp2_size2, &comp3_size2);
    // store size 1
    comp1_size1 = comp1_size;
    comp2_size1 = comp2_size;
    comp3_size1 = comp3_size;
    int64_t last_send = 0;
    int64_t last_receive = 0;
#endif
    memset(&frame, 0, sizeof(frame));
    frame.data[0] = malloc(pic_size);
    if (!frame.data[0]) {
        LOG_ERROR("Failed to malloc buffer for frame, size %d", pic_size);
        goto thread_end;
    }
    frame.data[1] = frame.data[0] + comp1_size;
    if (comp3_size)
        frame.data[2] = frame.data[1] + comp2_size;

    frame.seiData = (vmppSEI **)malloc(sizeof(vmppSEI *) * seiCount);
    frame.seiData[0] = sei;
    raw_ctx_ptr = &raw_ctx;

    uint64_t total_frames = 0;
    uint64_t total_frames_send = 0;
    do {
        memset(&stream, 0, sizeof(vmppStream));
        ret = raw_read_frame(raw_ctx_ptr, &frame);
        if (ret <= 0) {
            if (raw_eof(raw_ctx_ptr)) {
                raw_seek_to_start(raw_ctx_ptr);
#ifdef ENABLE_DYNAMIC_RES
                if (raw_ctx_ptr == &raw_ctx) {
                    raw_ctx_ptr = &raw_ctx_2;
                    comp1_size = comp1_size2;
                    comp2_size = comp2_size2;
                    comp3_size = comp3_size2;

                } else {
                    raw_ctx_ptr = &raw_ctx;
                    comp1_size = comp1_size1;
                    comp2_size = comp2_size1;
                    comp3_size = comp3_size1;
                }
                frame.data[1] = frame.data[0] + comp1_size;
                if (comp3_size)
                    frame.data[2] = frame.data[1] + comp2_size;

                vmpp_queue_push_back(frame_counts, (void *)total_frames_send);
                LOG_WARN("NEW RES, frame count for last RES %lld (send %lld, last send %lld), ",
                                        (u64)(total_frames_send - last_send), (u64)total_frames_send, (u64)last_send);
                last_send = total_frames_send;
#endif
                loop_count++;
            }

            if (loop_count >= options->loop) {
                break;
            } else {
                LOG_INFO("Loop %d over for thread %d", loop_count, thread_id);
                continue;
            }
        }
        frame.pts = total_frames;

        if (options->roiInt) {
            sscanf(options->roiParam, "top=%d,left=%d,bottom=%d,right=%d,qpType=%d,qpValue=%d",
                    &top, &left, &bottom, &right, &qpType, &qpValue);
            // printf("top=%d,left=%d,bottom=%d,right=%d,qpType=%d,qpValue=%d\n", top, left,
            // right, bottom, qpType, qpValue);
        }

        if (options->enableSpsCropInfo) {
            frame.cropInfo.flag = 1;
            sscanf(options->cropRect, "xOffset=%d,cropWidth=%d,yOffset=%d,cropHeight=%d",
                    &frame.cropInfo.xOffset, &frame.cropInfo.width, &frame.cropInfo.yOffset, &frame.cropInfo.height);
        }

        memset(&extParams, 0, sizeof(vmppEncExtendedParams));
        switch (options->roiType) {
        case vmpp_ENC_ROI_RANGE:
            if (total_frames_send && options->roiInt && (total_frames_send % options->roiInt == 0)) {
                /* only effective when videoConfig.enableROI == 1 */
                // extParams.roiType = vmpp_ENC_ROI_RANGE;
                int i = total_frames_send % VMPP_ENC_MAX_ROI_NUM;
                extParams.roi[i].area.top = top;
                extParams.roi[i].area.left = left;
                extParams.roi[i].area.bottom = bottom;
                extParams.roi[i].area.right = right;
                extParams.roi[i].area.enable = 1;
                extParams.roi[i].qpType = qpType;
                extParams.roi[i].qpValue = qpValue;
            }
            break;
        case vmpp_ENC_ROI_MAP:
            blksize = 64 >> (options->roiMapDeltaQpBlockUnit & 3);
            roiwidth = (frame.width + blksize - 1) / blksize;
            roiheight = (frame.height + blksize - 1) / blksize;
            roimap_size = roiwidth * roiheight;

            if (!roi_map_delta_qp_buffer)
                roi_map_delta_qp_buffer = (int8_t *)malloc(roimap_size);
            if (!roi_map_delta_qp_buffer) {
                break;
            }
            memset(roi_map_delta_qp_buffer, 0, roimap_size);
            roimap_index = (total_frames_send / 10) % 3;
            for (uint32_t y = 0; y < roiheight / 3; y++) {
                for (uint32_t x = 0; x < roiwidth / 2; x++) {
                    roi_map_delta_qp_buffer[y * roiwidth + x] = -roimap_value[roimap_index];
                }
            }
            for (uint32_t y = roiheight / 3; y < roiheight; y++) {
                for (uint32_t x = 3 * roiwidth / 4; x < roiwidth; x++) {
                    roi_map_delta_qp_buffer[y * roiwidth + x] = roimap_value[roimap_index];
                }
            }
            extParams.roiMap.roiMapDeltaQp = roi_map_delta_qp_buffer;
            extParams.roiMap.roiMapDeltaQpSize = roimap_size;

            break;
        case vmpp_ENC_ROI_NONE:
        default:
            break;
        }

        if (total_frames_send && options->extSEIInt && total_frames_send % options->extSEIInt == 0) {
            // extParams.extSEICount = 1;
            // extParams.extSEI = &sei;
            frame.seiCount = 1;
        } else {
            frame.seiCount = 0;
        }

        // test insert IDR in interval
        if (total_frames_send && options->forceIDRInt &&
            total_frames_send % options->forceIDRInt == 0) {
            extParams.forceIDR = 1;
        }
        // test insert IDR, choose some frames to insert IDR
        /*if (total_frames == 5 || total_frames == 6 || total_frames ==
        113) { extParams.forceIDR = 1;
        }*/

        if(options->ltrInsertTest != 0) {
            // test insert LTR
            if (total_frames_send && (total_frames_send % 18 == 0)) {
                extParams.forceLTR = 1;
            }
        }

        if (total_frames_send && (options->enableDynamicBitrate == 1) && (total_frames_send % 200 == 0)) {
            extParams.updateBitRate = (total_frames_send % 8000) * 1000;//target bitrate, bps
            extParams.updateVbvBufSize = 0;//vbvBufSize, bits, set to 0 if use default
            extParams.updateVbvMaxRate = 0;//vbvMaxRate, bps, set to 0 if use default
            extParams.updateTypeMask |= VMPP_ENC_UPDATE_CBR;
        }

        if (total_frames_send && (options->enableDynamicFrameRate == 1) && (total_frames_send % 300 == 0)) {
            extParams.updateFrameRate.numerator = 60;
            extParams.updateFrameRate.denominator = 1;
            extParams.updateTypeMask |= VMPP_ENC_UPDATE_FRAMERATE;
        }

        if (total_frames_send && (options->enableDynamicCrf == 1) && (total_frames_send % 100 == 0)) {
            extParams.updateCrf = 10 + total_frames_send / 100;
            extParams.updateVbvBufSize = 0;
            extParams.updateVbvMaxRate = 0;
            extParams.updateTypeMask |= VMPP_ENC_UPDATE_CRF;
        }

        frame.timebase.denominator = 0;
        frame.timebase.numerator = 0;
        if (options->vfr) {
            if (total_frames_send >= 198) {
                frame.timebase.denominator = 1;
                frame.timebase.numerator = 48;
            } else if (total_frames_send >= 98) {
                frame.timebase.denominator = 1;
                frame.timebase.numerator = 24;
            } else {
                frame.timebase.denominator = 1;
                frame.timebase.numerator = 48;
            }
        }

        if (total_frames_send && (options->enableDynamicKeyInt == 1) ) {
            if (total_frames_send == 118) {
                extParams.updateKeyInt = 60;
                extParams.updateTypeMask |= VMPP_ENC_UPDATE_KEYINT;
            }
            if (total_frames_send == 410) {
                extParams.updateKeyInt = 120;
                extParams.updateTypeMask |= VMPP_ENC_UPDATE_KEYINT;
            }
        }

        do {
            LOG_DEBUG("vmppEncEncodeFrame ---------- send %lld, receive %lld", (u64)total_frames_send, (u64)total_frames);
            enc_ret = vmppEncEncodeFrame(ch, &frame, &extParams, &stream, 4000);
            if (enc_ret == vmpp_RSLT_OK) {
                if (options->enableCalcPSNR)
                    update_psnr(&stream, options, psnr_total, &psnr_num);
                if (options->enableCalcSSIM)
                    update_ssim(&stream, psnr_total, &ssim_num);
            }

            if (enc_ret == vmpp_RSLT_OK || enc_ret == vmpp_RSLT_ENC_INPUT_INSERTED) {
                total_frames_send++;
            } else if (enc_ret < 0) {
                LOG_ERROR("Send frame to %dth enc channel: %p, ret %d", thread_id, ch, enc_ret);
                assert(0);
                goto flush;
            }

            if (enc_ret == vmpp_RSLT_ENC_AGAIN || enc_ret == vmpp_RSLT_ENC_AGAIN_WITH_NO_OUTPUT) {
                LOG_DEBUG("vmpp_RSLT_ENC_AGAIN %lld to %lld", (u64)total_frames_send, (u64)(total_frames_send - 1));
            }

            if (enc_ret == vmpp_RSLT_OK || enc_ret == vmpp_RSLT_ENC_AGAIN) {
#ifdef ENABLE_DYNAMIC_RES
                // first frame
                if (!dummyFrame.width && !dummyFrame.height)
                    dummyFrame = frame;
#endif
                if (options->store) {
#ifdef ENABLE_DYNAMIC_RES
                    int res_changed = 0;
                    if (vmpp_queue_size(frame_counts) &&
                        total_frames >= (uint64_t)vmpp_queue_peek(frame_counts, 0)) {
                        res_changed = 1;
                        vmpp_queue_pop_front(frame_counts);
                        LOG_WARN("DEBUG - NEW RES ENCODED, frame count for last RES %lld",
                                 (u64)(total_frames - last_receive));
                        last_receive = total_frames;
                    }
                    if (total_frames > 1 &&
                        (dummyFrame.width != frame.width || dummyFrame.height != frame.height) &&
                        res_changed) {
                        if (file_output) {
                            fflush(file_output);
                            fclose(file_output);
                            file_output = NULL;
                        }
                        memset(output_file, 0, MAX_PATH_LEN);
                        sprintf(output_file, "%s/enc_%d_%d.%s", options->output_file, thread_id, loop_count,
                                options->codec == 0 ? "h264" : (options->codec == 1 ? "hevc" : "ivf"));
                        dummyFrame = frame;
                    }
#endif

                    if (!file_output) {
                        file_output = fopen(output_file, "wb");
                    }

                    if (file_output) {
                        fwrite(stream.stream, 1, stream.len, file_output);
                    }
                }

                total_frames++;
                rel_ret = vmppEncReleaseStream(ch, &stream);
                if (rel_ret < 0) {
                    LOG_ERROR("release stream error %d.", rel_ret);
                    goto thread_end;
                }

                if (total_frames && total_frames % 100 == 0)
                    LOG_INFO("Successfully encode %lld frames with %d th thread.", (u64)total_frames,
                             thread_id);
            }
        } while (enc_ret == vmpp_RSLT_ENC_AGAIN || enc_ret == vmpp_RSLT_ENC_AGAIN_WITH_NO_OUTPUT);

        if (options->vframes > 0 && total_frames_send >= (uint64_t)options->vframes) {
            break;
        }
    } while (1);

flush:
    do {
        memset(&stream, 0, sizeof(vmppStream));
        frame.memoryType = vmpp_MEM_FLUSH;
        enc_ret = vmppEncEncodeFrame(ch, &frame, NULL, &stream, 4000);
        if (enc_ret == vmpp_RSLT_WARN_EOS) {
            LOG_INFO("[APP][%p]vmpp_RSLT_WARN_EOS", ch);
            break;
        }

        if (enc_ret < 0)
            assert(0);

        if (stream.inputBusAddress) {
            LOG_INFO("[APP][%p] Do flush job for remained frame", ch);
            // !!! Flush frame from decoder with memory type == vmpp_MEM_DEVICE
        }

        if (enc_ret == vmpp_RSLT_ENC_FLUSH)
            continue;

        if (enc_ret == vmpp_RSLT_OK || enc_ret == vmpp_RSLT_ENC_AGAIN) {
            total_frames++;
            if (options->store && (options->svcExtractMaxTLayer == VMPP_ENC_DEFAULT_PAR
                || stream.svcTemporalId <= options->svcExtractMaxTLayer)) {
                if (!file_output) {
                    file_output = fopen(output_file, "wb");
                }

                if (file_output) {
                    fwrite(stream.stream, 1, stream.len, file_output);
                }
            }
            vmppEncReleaseStream(ch, &stream);

            LOG_DEBUG("vmppEncEncodeFrame ---------- send %lld, receive %lld", (u64)total_frames_send, (u64)total_frames);
        }
    } while (1);

    LOG_INFO("Successfully encode %lld frames (send %lld) with %d th thread.", (u64)total_frames, (u64)total_frames_send,
        thread_id);

    if (options->enableCalcPSNR && psnr_num) {
        LOG_WARN("Average PSNR[%d]: Y %4.2f, U %4.2f, V %4.2f",
            psnr_num,
            psnr_total[0] / psnr_num,
            psnr_total[1] / psnr_num,
            psnr_total[2] / psnr_num);
    }

    if (options->enableCalcSSIM && ssim_num) {
        LOG_WARN("Average SSIM[%d]: Y %4.2f, U %4.2f, V %4.2f",
            ssim_num,
            psnr_total[3] / ssim_num,
            psnr_total[4] / ssim_num,
            psnr_total[5] / ssim_num);
    }
thread_end:
    vmpp_queue_free(&frame_counts);
    raw_close(&raw_ctx);
#ifdef ENABLE_DYNAMIC_RES
    raw_close(&raw_ctx_2);
#endif

    if (frame.data[0])
        free(frame.data[0]);

    if (sei) {
        free(sei);
        sei = NULL;
    }

    if (frame.seiData) {
        free(frame.seiData);
        frame.seiData = NULL;
    }
    if (file_output)
        fclose(file_output);

    if (ch)
        vmppEncDestroyChannel(&ch);
    return NULL;
};

int MainTask(MainArgs *args)
{
    int32_t ret = -1;
    vmppChannel enc_ch[MAX_ENC_CHANNEL_COUNT];
    struct thread_opts enc_thread_opts[MAX_ENC_CHANNEL_COUNT];
    pthread_t thread_handle[MAX_ENC_CHANNEL_COUNT] = {0};
    int i;

    memset(enc_thread_opts, 0, sizeof(enc_thread_opts));
    memset(enc_ch, 0, sizeof(enc_ch));


    setLogLevel(LOG_LEVEL_INFO);

    /* set default params */
    enc_options *options = malloc(sizeof(enc_options));
    default_params(options);

    ret = parse_options(args->argc, args->argv, options);
    if (ret < 0) {
        goto end;
    }

    if (thread_count < 1)
        thread_count = 1;
    else if (!options->auto_devices && thread_count > 32) {
        LOG_ERROR("Please turn on Auto Devices funciton by -a option to support more than 24 threads!");
        goto end;
    }
    else if (thread_count > MAX_ENC_CHANNEL_COUNT){
        LOG_ERROR("Thread count read max limit: %d!", MAX_ENC_CHANNEL_COUNT);
        goto end;
    }
    if (options->stride == 0) {
        options->stride = options->width;
    }

    if (options->output_file == NULL) {
        if (options->codec == 0) {
            options->output_file = "enc_out.h264";
        } else if (options->codec == 1) {
            options->output_file = "enc_out.h265";
        } else if (options->codec == 2) {
            options->output_file = "enc_out.ivf";
        }
    }

    /* init encoder */
    vmppConfiguration cfg;
    memset(&cfg, 0, sizeof(vmppConfiguration));
    if (options->logLevel) {
        cfg.logCtx.enableCustomLog = 1;
        cfg.logCtx.logLevel = options->logLevel;
    }

    ret = vmppInitEncoder(&cfg);
    if (ret != vmpp_RSLT_OK) {
        LOG_INFO("vmppInitEncoder failed %d", ret);
        goto end;
    }

    for (i = 0; i < thread_count; i++) {
        enc_thread_opts[i].ch = enc_ch[i];
        enc_thread_opts[i].opts = options;
        enc_thread_opts[i].thread_id = i;
/* set input params */
        memset(&enc_thread_opts[i].param, 0, sizeof(vmppEncChannelParameters));
        set_params(options, &enc_thread_opts[i].param);
        enc_thread_opts[i].param.enProfiling = 1;

        ret = pthread_create(&thread_handle[i], NULL, enc_thread, &enc_thread_opts[i]);
        if (ret != 0) {
            LOG_ERROR("fail to start encode thread %d, ret %d", i, ret);
            goto end;
        }
    }

    for (i = 0; i < thread_count; i++) {
        pthread_join(thread_handle[i], NULL);
    }
end:

    if (options) {
        free(options);
        options = NULL;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    MainArgs args = {argc, argv};
    return MainTask(&args);
}
