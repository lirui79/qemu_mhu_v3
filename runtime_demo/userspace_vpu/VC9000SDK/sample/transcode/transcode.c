/*
 * Copyright (c) 2022, Vastai Tech. All rights reserved
 *
 * The information contained herein is confidential
 * property of Company. The user, copying, transfer or
 * disclosure of such information is prohibited except
 * by express written agreement with VASTAITECH.
 */

#include <assert.h>
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <math.h>
#include <unistd.h>

#include "cJSON.h"
#include "defs.h"
#include "ffmpeg-wrapper.h"
#include "md5.h"
#include "queue.h"
#include "stream.h"
#include "utils.h"
#include "vmpp_dec_api.h"
#include "vmpp_enc_api.h"
#include "option.h"

#define INSERTIDR_TEST
#define MIN(x, y) ((x) < (y)) ? ((x)) : ((y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define EXT_BUF_NUM (2)

double psnr_total[3];
int psnr_num = 0;

static struct option_t ops[] = {
    //{"help", 'H', 2},
    {"input", 'i', 1},
    {"output", 'o', 1},
    {"save",'s',1},
    {"loopForFile",'l',1},
    {"loopInJson",'L',1},
    {"md5",'m',1},
    {"ffmpeg",'f',1},
    {"codeType",'c',1},
    {"encodeCodeType",'C',1},
    {"debug",'p',1},
    {"performace",'P',1},
    {"frameNumber",'n',1},
    {"help",'h',1},
    {"decBitDepth",'b',1},
    {"userOutBuf",'u',0},
    {"memoryMode",'M',1},
    //{"width", 'w', 1},
    //{"height", 'h', 1},
    //{"stride", 't', 1}, /* Input image format */
    //{"device", 'd', 1},
    //{"pixelFormat", 'f', 1},
    //{"loop", 'l', 1},
    //{"save", 's', 1},
    //{"codecFormat", 'c', 1},
    //{"bufferCount", 'b', 1},
#ifdef  INSERTIDR_TEST
    {"idr_index_file", 'I', 1},
#endif
    /* Only long option can be used for all the following parameters because
     * we have no more letters to use. All shortOpt=0 will be identified by
     * long option. */
    {"encDevice", '0', 1},
    {"decDevice", '0', 1},
    {"memDevice", '0', 1},
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
    {"coreID", '0', 1},
    {"rotation", '0', 1},
    {"openGop", '0', 1},
    {"smartEnc", '0', 1},
    {"enableDynamicKeyInt", '0', 1},
    {"disableMMCO", '0', 1},
    {"inLoopDSRatio", '0', 1},
    {"aqMode", '0', 1},
    {"psyFactor", '0', 1},
    {"rdoLevel", '0', 1},
    {"enableRdoQuant", '0', 1},
    {"qCompress", '0', 1},
    {"bitRateBalanceLevel", '0', 1},
    {"multicore", '0', 1},
    {NULL, 0, 0} /* Format of last line */
};

#define OUT_BUF_NUM 4
#define MAX_VIDEO_DEC_WIDTH 8192
#define MAX_VIDEO_DEC_HEIGHT 8192

typedef struct {
    char*                           encDevice;                    // video device node name
    char*                           memDevice;                    // memory device node name
    char *enc_codec;
    int width;
    int height;
    int profile;
    unsigned int level;
    unsigned int gopSize;
    unsigned int frameRateNum;
    unsigned int frameRateDen;
    unsigned int bitDepthLuma;
    unsigned int bitDepthChroma;
    unsigned int lookaheadDepth;
    unsigned int tune;
    unsigned int keyInt;
    unsigned int gdrDuration;
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
    unsigned int qualityMode;
    unsigned int vbr;
    float aqStrength;
    char *pCom;
    int comLength;
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
    unsigned int enableDynamicKeyInt;
    unsigned int disableMMCO;

    unsigned int inLoopDSRatio;
    unsigned int aqMode;
    float psyFactor;
    unsigned int rdoLevel;
    unsigned int enableRdoQuant;
    double qCompress;
    unsigned int bitRateBalanceLevel;
    unsigned int multicore;
} enc_options;

struct transcode_options {
    struct vmpp_queue *urls;
    char*                           codec;
    char*                           encDevice;                    // video device node name
    char*                           decDevice;                    // video device node name
    char*                           memDevice;                    // memory device node name
    char *output_directory;
    char *output_file;
    char *enc_codec;
    int save;
    int loop;
    int main_loop;
    int check_md5;
    int using_ffmpeg;
    int period;
    int perf_period;
    int vframes;
    int bitDepth;
    int memory_mode;
    enc_options default_enc_opts;
    struct vmpp_queue *enc_options;
};

static struct transcode_options options;
uint64_t transcode_index = 0;
char *current_url = NULL, *current_device = NULL;
int current_main_loop = 0, current_loop = 0;
uint8_t cur_md5sum[MD5_HASH_LEN] = {0};
uint8_t last_md5sum[MD5_HASH_LEN] = {0};
int md5_saved = 0;
uint64_t file_start_time;
uint64_t file_read_time;
uint64_t file_write_time;
uint64_t check_md5_time;
int md5ctx_inited = 0;
struct md5_context md5ctx = {0};
int has_irregular_url = 0;
uint64_t period_file_start_time;
uint64_t enc_start, enc_stop, enc_total_frames = 0, enc_tick;
struct vmpp_queue *frame_queue;
struct vmpp_queue *idle_frame_queue;
struct vmpp_queue *releasing_frame_queue;
pthread_mutex_t frame_mutex;
vmppChannel dec_ch = NULL;
vmppChannel enc_ch = NULL;
int dec_finished = 0;
int enc_error = 0;
pthread_t enc_thread_handle;
static FILE *enc_output_file = NULL;
static int encoder_enable = 0;
static int encoder_inited = 0;
static int jpeg_stream = 0;
enc_options *current_enc_opts = NULL;

#ifdef  INSERTIDR_TEST
#define MAX_IDRBUF_LEN 500
static uint32_t idr_index_buf[MAX_IDRBUF_LEN];
static uint32_t idrbuf_len = 0;
static uint32_t idr_test_flag = 0;
#endif

static void usage(const char *program)
{
    LOG(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "[transcode] Usage: %s [options]", program);
    LOG_INFO("  -i    url for input file/directory or a json file with device/urls list");
    LOG_INFO("  -o    (opt) output directory or file name, default is: NULL, directory must exist");
    LOG_INFO("  -s    (opt) whether to save YUV data from decoder and data from encoder, default: 0");
    LOG_INFO("  -l    (opt) loop count for file, default:0, negative value for infinite loop");
    LOG_INFO(
        "  -L    (opt) loop count for list in json, default: 0, negative value for infinite loop");
    LOG_INFO("  -m    (opt) whether to check md5, default: 0 ");
    LOG_INFO("  -f    (opt) whether using ffmpeg for demuxing, default: 1 ");
    LOG_INFO(
        "  -c    (opt) codec type('h264','hevc','av1','jpeg'), MUST if not using ffmpeg for demuxing");
    LOG_INFO("  -C    (opt) encode codec type('h264','hevc','av1','jpeg'), MUST if encoder is enabled");
    LOG_INFO("  -p    (opt) print debug log every X frames, default:100");
    LOG_INFO("  -P    (opt) print performance every X frames, default:1000");
    LOG_INFO("  -n    (opt) specify the frame number to be decoded, default:0");
    LOG_INFO("  -u    (opt) use user output buffer or not, default:0");
    LOG_INFO("  -b    (opt) specify stream bit depth, default:8");
    LOG_INFO("  -M    (opt) memory mode, default:0");
    LOG_INFO("  -I    (opt) insert IDR frame index\n");
    LOG_INFO("  -h    (opt) help");
    LOG_INFO("  --encDevice        video device node name for encoder");
    LOG_INFO("  --decDevice        video device node name for decoder");
    LOG_INFO("  --memDevice        memory device node name");
    LOG_INFO("  --profile          main profile or main still picture profile");
    LOG_INFO("  --level            main profile level");
    LOG_INFO("  --frameRateNum     frame rate numerator");
    LOG_INFO("  --frameRateDen     frame rate denominator");
    LOG_INFO("  --bitDepthLuma     luma bit depth");
    LOG_INFO("  --bitDepthChroma   chroma bit depth");
    LOG_INFO("  --gopSize          gop size");
    LOG_INFO("  --gdrDuration      gdr duration");
    LOG_INFO("  --lookaheadDepth   lookahead depth");
    LOG_INFO("  --qualityMode      quality mode");
    LOG_INFO("  --tune             tune");
    LOG_INFO("  --keyInt           IDR interval");
    LOG_INFO("  --crf              CRF constant");
    LOG_INFO("  --cqp              cqp");
    LOG_INFO("  --llRc             llRc");
    LOG_INFO("  --bitRate          bps, bit rate");
    LOG_INFO("  --cqp              cqp");
    LOG_INFO("  --initQp           init qp");
    LOG_INFO("  --vbvBufSize       kb, vbv Buffer size");
    LOG_INFO("  --vbvMaxRate       kbps, vbv max rate");
    LOG_INFO("  --intraQpDelta     intra qp delta");
    LOG_INFO("  --qpMinI           min qp value of I frame");
    LOG_INFO("  --qpMaxI           max qp value of I frame");
    LOG_INFO("  --qpMinPB          min qp value of B/P frame");
    LOG_INFO("  --qpMaxPB          max qp value of B/P frame");
    LOG_INFO("  --aqStrength       aq strength");
    LOG_INFO("  --P2B              P2B");
    LOG_INFO("  --bBPyramid        bBPyramid");
    LOG_INFO("  --maxFrameSizeMultiple        maxFrameSizeMultiple");
    LOG_INFO("  --maxFrameSize     max frame size");
    LOG_INFO("  --outbufNum        outbufNum");
    LOG_INFO("  --roiType           ROI type, 0 for none, 1 for roi range, 2 for roi map");
    LOG_INFO("  --roiInt           ROI interval");
    LOG_INFO("  --roiParam           ROI param");
    LOG_INFO("  --extSEIInt        extSEI interval");
    LOG_INFO("  --forceIDRInt      forceIDR interval");
    LOG_INFO("  --logLevel         log level for SDK, default: 3(WARN),  1-DEBUG 2-INFO 3-WARN 4-ERROR");
    LOG_INFO("  --roiMapDeltaQpBlockUnit      roi map delta qp block unit");
    LOG_INFO("  --roiMapQpDeltaVersion        roi map delta qp version");
    LOG_INFO("  --enableDynamicBitrate        enable dynamic bitrate or not(1/0)");
    LOG_INFO("  --enableDynamicFrameRate      enable dynamic framerate or not(1/0)\n");
    LOG_INFO("  --maxBFrames       max B frames control for adaptive GOP decision\n");
    LOG_INFO("  --hrd              Hypothetical Reference Decoder model\n");
    LOG_INFO("  --picSkip          Frame All Skip Mode When Overflow\n");
    LOG_INFO("  --vfr              variable frame rate\n");
    LOG_INFO("  --svcTLayers              Temporal Layers for Scalable Video Coding\n");
    LOG_INFO("  --svcExtractMaxTLayer     Max Temporal Layer to Extract for Scalable Video Coding\n");
    LOG_INFO("  --sliceSize        Slice size in CTB/MB rows for multislice\n");
    LOG_INFO("  --enableDynamicCrf        enable dynamic crf or not(1/0)");
    LOG_INFO("  --psnr        enable caculation of PSNR or not, default: 0\n");
    LOG_INFO("  --ssim        enable caculation of SSIM or not, default: 0\n");
    LOG_INFO("  --ltrInterval        enable LTR, default: 0\n");
    LOG_INFO("  --ltrQpDelta        set LTR frame QpDelta, default: 0\n");
    LOG_INFO("  --ltrRefGap        set frame gap that references the LTR, default: 0\n");
    LOG_INFO("  --ltrInsertTest    Enable test insert LTR: 0\n");
    LOG_INFO("  --rotation    Rotate input image, 0-Disabled (Default), 1-90 degrees right, 2-90 degrees left, 3-180 degrees right\n");
    LOG_INFO("  --coreID           Enable specify core id: 0-3\n");
    LOG_INFO("  --openGop           openGop\n");
    LOG_INFO("  --smartEnc          smartEnc Mode\n");
    LOG_INFO("  --enableDynamicKeyInt        enable dynamic keyInt or not(1/0)\n");
    LOG_INFO("  --disableMMCO      disable h264 memory management control operation or not(1/0)\n");
    LOG_INFO("  --inLoopDSRatio    in-loop downsample ratio for first pass(1/0)\n");
    LOG_INFO("  --aqMode           aq mode:0-3\n");
    LOG_INFO("  --psyFactor        weight of psycho-visual encoding:0.0-4.0\n");
    LOG_INFO("  --rdoLevel         RDO Level can balance the quality and throughput:1-3\n");
    LOG_INFO("  --enableRdoQuant   enable RDO quantization or not(1/0)\n");
    LOG_INFO("  --qCompress        qCompress sets the quantizer curve compression factor:0.0-1.0\n");
    LOG_INFO("  --bitRateBalanceLevel        set the matching degree between the encoding output bitrate and the target bitrate in simple or static scenarios:0-4\n");
    LOG_INFO("  --multicore        enable multi-core encoding or not(1/0)\n");
    LOG_INFO("Example:");
    LOG(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN,
        "  transcode -i input.json -o outputdir --encDevice /dev/hantroenc --decDevice /dev/hantrodec --memDevice /dev/memalloc -s 0 -l 0 -L 0 -m 1");
    LOG_INFO(" ./transcode -i /home/stone/workspace/akiyo_352x288_300_IBBBP.h264 -o output_352x288_300.hevc --encDevice /dev/hantroenc --decDevice /dev/hantrodec --memDevice /dev/memalloc -s 1 -l 1 -m 0 -C hevc -f 0 -c h264\n");
    LOG_INFO(" ./transcode -i /home/stone/workspace/sample_1920x1080.hevc  -o output_1920x1080.hevc   --encDevice /dev/hantroenc --decDevice /dev/hantrodec --memDevice /dev/memalloc   -s 1 -l 1 -m 0 -C h264 -f 0 -c hevc\n");
    LOG_INFO(" ./transcode -i /home/stone/workspace/vpu_sdk/test/jsons/transcode.json -o /home/stone/workspace/OUTPUT/ -l 1 -s 1\n");
}

#ifdef INSERTIDR_TEST
void init_idr_params()
{
    memset(idr_index_buf, 0, MAX_IDRBUF_LEN * sizeof(uint32_t));
    idr_test_flag = 0;
    idrbuf_len = 0;
}
void split_idr_params(char *params)
{
    char *temp = strtok(params, ":");
    uint32_t i = 0;
    idr_test_flag = 1;
    while (temp && i < MAX_IDRBUF_LEN) {
        idr_index_buf[i] = atoi(temp);
        temp = strtok(NULL, ":");
        i++;
    }
    idrbuf_len = i;
}
// compare the frame count with the set idr buffer.
int compare_idr_index(uint32_t frame_count)
{
    uint32_t i = 0;
    for (i = 0; i < idrbuf_len; i++) {
        if (idr_index_buf[i] == frame_count ) {
            printf("frame count %d need to insert IDR!\n", frame_count);
            return 1;
        }
    }
    return 0;
}
#endif

void set_video_params(enc_options *option, vmppEncChannelParameters *ch_apr)
{
    if (!option) {
        fprintf(stderr, "set param error, option is null.\n");
    }
    if (!ch_apr) {
        fprintf(stderr, "set param error, ch_apr is null.\n");
    }

    /* Only reset the video configuration: codecType and the device names have
       already been filled in by the caller (encoder_task_init) and must be
       preserved, otherwise the encoder is created with codec type 0. */
    memset(&ch_apr->videoConfig, 0, sizeof(ch_apr->videoConfig));

    ch_apr->encDevice = option->encDevice;
    ch_apr->memDevice = option->memDevice;

    ch_apr->videoConfig.width = option->width;
    ch_apr->videoConfig.height = option->height;
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
        if (ch_apr->codecType == vmpp_CODEC_ENC_H264) {
            ch_apr->videoConfig.profile = vmpp_VIDEO_PRFL_H264_HIGH_10;
            ch_apr->videoConfig.level = vmpp_VIDEO_LVL_H264_5_1;
        }
        else if (ch_apr->codecType == vmpp_CODEC_ENC_HEVC) {
            ch_apr->videoConfig.profile = vmpp_VIDEO_PRFL_HEVC_MAIN_10;
            ch_apr->videoConfig.level = vmpp_VIDEO_LVL_HEVC_6;
        }
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

void set_extparams(vmppEncExtendedParams *extParams,uint64_t total_frames,enc_options *options,vmppFrame *frame) {

    int roimap_value[3] = {30, 10, -15};
    int roimap_index = 0;
    uint32_t blksize = 0;
    uint32_t roiwidth, roiheight, roimap_size;
    int8_t *roi_map_delta_qp_buffer = NULL;

    int top = 0, left = 0, right = 0, bottom = 0;
    int qpType = 0, qpValue = 0;
    if (options->roiInt) {
                    sscanf(options->roiParam, "top=%d,left=%d,bottom=%d,right=%d,qpType=%d,qpValue=%d",
                        &top, &left, &right, &bottom, &qpType, &qpValue);
                    // printf("top=%d,left=%d,bottom=%d,right=%d,qpType=%d,qpValue=%d\n", top, left,
                    // right, bottom, qpType, qpValue);
                }
    switch (options->roiType) {
            case vmpp_ENC_ROI_RANGE:
                if (total_frames && options->roiInt && (total_frames % options->roiInt == 0)) {
                    /* only effective when videoConfig.enableROI == 1 */
                    // extParams.roiType = vmpp_ENC_ROI_RANGE;
                    int i = total_frames % VMPP_ENC_MAX_ROI_NUM;
                    extParams->roi[i].area.top = top;
                    extParams->roi[i].area.left = left;
                    extParams->roi[i].area.bottom = bottom;
                    extParams->roi[i].area.right = right;
                    extParams->roi[i].area.enable = 1;
                    extParams->roi[i].qpType = qpType;
                    extParams->roi[i].qpValue = qpValue;
                }
                break;
            case vmpp_ENC_ROI_MAP:
                blksize = 64 >> (options->roiMapDeltaQpBlockUnit & 3);
                roiwidth = (frame->width + blksize - 1) / blksize;
                roiheight = (frame->height + blksize - 1) / blksize;
                roimap_size = roiwidth * roiheight;

                if (!roi_map_delta_qp_buffer)
                    roi_map_delta_qp_buffer = (int8_t *)malloc(roimap_size);
                if (!roi_map_delta_qp_buffer) {
                    break;
                }
                memset(roi_map_delta_qp_buffer, 0, roimap_size);
                roimap_index = (total_frames / 10) % 3;
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
                extParams->roiMap.roiMapDeltaQp = roi_map_delta_qp_buffer;
                extParams->roiMap.roiMapDeltaQpSize = roimap_size;

                break;
            case vmpp_ENC_ROI_NONE:
            default:
                break;
            }

    if (total_frames && options->extSEIInt && total_frames % options->extSEIInt == 0) {
                // extParams.extSEICount = 1;
                // extParams.extSEI = &sei;
                frame->seiCount = 1;
            } else {
                frame->seiCount = 0;
            }   
}

static void set_default_enc_params(enc_options *enc_opts)
{
    enc_opts->encDevice = "/dev/hantroenc";
    enc_opts->memDevice = "/dev/memalloc";
    enc_opts->enc_codec = NULL; // Disable encoder
    enc_opts->width = 1920;
    enc_opts->height = 1080;
    enc_opts->profile = vmpp_VIDEO_PRFL_HEVC_MAIN;
    enc_opts->level = vmpp_VIDEO_LVL_HEVC_6;
    enc_opts->gopSize = VMPP_ENC_DEFAULT_PAR;
    enc_opts->frameRateNum = 30;
    enc_opts->frameRateDen = 1;
    enc_opts->bitDepthLuma = 8;
    enc_opts->bitDepthChroma = 8;
    enc_opts->outbufNum = OUT_BUF_NUM;
    enc_opts->lookaheadDepth = 0;
    enc_opts->tune = vmpp_ENC_TUNE_PSNR;
    enc_opts->keyInt = VMPP_ENC_DEFAULT_PAR;
    enc_opts->crf = VMPP_ENC_DEFAULT_PAR;
    enc_opts->cqp = 0;
    enc_opts->llRc = 0;
    enc_opts->bitRate = 0;
    enc_opts->initQp = VMPP_ENC_DEFAULT_PAR;
    enc_opts->vbvBufSize = VMPP_ENC_DEFAULT_PAR;
    enc_opts->vbvMaxRate = VMPP_ENC_DEFAULT_PAR;
    enc_opts->intraQpDelta = VMPP_ENC_DEFAULT_PAR;
    enc_opts->qpMinI = VMPP_ENC_DEFAULT_PAR;
    enc_opts->qpMaxI = VMPP_ENC_DEFAULT_PAR;
    enc_opts->qpMinPB = VMPP_ENC_DEFAULT_PAR;
    enc_opts->qpMaxPB = VMPP_ENC_DEFAULT_PAR;
    enc_opts->aqStrength = VMPP_ENC_DEFAULT_PAR;
    enc_opts->qualityMode = vmpp_BRONZE_QUALITY;
    enc_opts->vbr = 0;
    enc_opts->pCom = NULL;
    enc_opts->comLength = 0;
    enc_opts->gdrDuration = 0;
    enc_opts->P2B = VMPP_ENC_DEFAULT_PAR;
    enc_opts->bBPyramid = 1;
    enc_opts->maxFrameSizeMultiple = VMPP_ENC_DEFAULT_PAR;
    enc_opts->maxFrameSize = VMPP_ENC_DEFAULT_PAR;

    enc_opts->roiType = 0;
    enc_opts->roiInt = 0;
    enc_opts->roiParam = "top=0,left=0,bottom=200,right=200,qpType=0,qpValue=1";
    enc_opts->extSEIInt = 0;
    enc_opts->forceIDRInt = 0;
    enc_opts->logLevel = 3;
    enc_opts->roiMapDeltaQpBlockUnit = 0;
    enc_opts->roiMapQpDeltaVersion = 0;
    enc_opts->enableDynamicBitrate = 0;
    enc_opts->enableDynamicFrameRate = 0;
    enc_opts->maxBFrames = VMPP_ENC_DEFAULT_PAR;
    enc_opts->hrd = 0;
    enc_opts->pictureSkip = 0;
    enc_opts->vfr = 0;
    enc_opts->svcTLayers = 0;
    enc_opts->svcExtractMaxTLayer = VMPP_ENC_DEFAULT_PAR;
    enc_opts->sliceSize = 0;
    enc_opts->enableCalcPSNR = 0;
    enc_opts->enableCalcSSIM = 0;
    enc_opts->ltrInterval = 0;
    enc_opts->ltrQpDelta = 0;
    enc_opts->ltrRefGap = 0;
    enc_opts->ltrInsertTest = 0;
    enc_opts->rotation = 0;
    enc_opts->coreID = VMPP_ENC_DEFAULT_PAR;
    enc_opts->openGop = 0;
    enc_opts->smartEnc = 0;
    enc_opts->enableDynamicKeyInt = 0;
    enc_opts->disableMMCO = 0;
    enc_opts->inLoopDSRatio = VMPP_ENC_DEFAULT_PAR;
    enc_opts->aqMode = VMPP_ENC_DEFAULT_PAR;
    enc_opts->psyFactor = VMPP_ENC_DEFAULT_PAR;
    enc_opts->rdoLevel = VMPP_ENC_DEFAULT_PAR;
    enc_opts->enableRdoQuant = VMPP_ENC_DEFAULT_PAR;
    enc_opts->qCompress = VMPP_ENC_DEFAULT_PAR;
    enc_opts->bitRateBalanceLevel = 0;
    enc_opts->multicore = 0;
}

static void get_output_file(char *output, const char *suffix)
{
    char *tmp = NULL;
    char *input_suffix = NULL;
    char *suffix_device = NULL;
    char file_name[MAX_PATH_LEN] = {0};
    char output_url[MAX_PATH_LEN*2] = {0};
    char device_name[64] = {0};
    if (options.output_directory) {
        tmp = strrchr(current_url, '/');
        input_suffix = strrchr(current_url, '.');
        strncpy(file_name, tmp + 1, strlen(tmp) - strlen(input_suffix) - 1);
        strcpy(output_url, options.output_directory);
        suffix_device = strrchr(current_device, '/');
        strncpy(device_name, suffix_device + 1, strlen(suffix_device) - 1);
        if ('/' == options.output_directory[strlen(options.output_directory) - 1])
            sprintf(&output_url[strlen(options.output_directory)], "%lld_%s_%s_%d_%d.%s",
                    (u64)transcode_index, file_name, device_name, current_main_loop, current_loop,
                    suffix);
        else
            sprintf(&output_url[strlen(options.output_directory)], "/%lld_%s_%s_%d_%d.%s",
                    (u64)transcode_index, file_name, device_name, current_main_loop, current_loop,
                    suffix);
        memcpy(output, output_url, MAX_PATH_LEN);
    } else if (options.output_file) {
        memcpy(output, options.output_file, strlen(options.output_file));
    }
}

static int encoder_task_init(enc_options *enc_opt)
{
    char *suffix = NULL;
    vmppEncChannelParameters ch_apr;
    char output_file_str[MAX_PATH_LEN] = {0};
    vmppCodecType codecType = vmpp_CODEC_ENC_HEVC;
    vmppResult ret = vmpp_RSLT_OK;

    if (strcmp(enc_opt->enc_codec, "h264") == 0) {
        codecType = vmpp_CODEC_ENC_H264;
        suffix = "h264";
    } else if (strcmp(enc_opt->enc_codec, "hevc") == 0) {
        codecType = vmpp_CODEC_ENC_HEVC;
        suffix = "hevc";
    } else if (strcmp(enc_opt->enc_codec, "av1") == 0) {
        codecType = vmpp_CODEC_ENC_AV1;
        suffix = "ivf";
    } else if (strcmp(enc_opt->enc_codec, "jpeg") == 0) {
        codecType = vmpp_CODEC_ENC_JPEG;
        suffix = "jpg";
    }

    if (options.save && suffix) {
        get_output_file(output_file_str, suffix);
        enc_output_file = fopen(output_file_str, "wb");
        if (!enc_output_file)
            LOG_WARN("[transcode][enc] Fail to open output file <%s>", output_file_str);
    }

    memset(&ch_apr, 0, sizeof(ch_apr));
    ch_apr.codecType = codecType;
    ch_apr.encDevice   = enc_opt->encDevice;
    ch_apr.memDevice   = enc_opt->memDevice;

    vmppConfiguration cfg;
    memset(&cfg, 0, sizeof(vmppConfiguration));
    cfg.logCtx.enableCustomLog = 1;
    cfg.logCtx.logLevel = options.default_enc_opts.logLevel;

    ret = vmppInitEncoder(&cfg);
    if (ret != vmpp_RSLT_OK) {
        LOG_ERROR("[transcode][enc] vmppInitEncoder failed %d", ret);
        return -1;
    }

    if (ch_apr.codecType == vmpp_CODEC_ENC_JPEG) {
        ch_apr.jpegConfig.frameType = vmpp_PIX_FMT_NV12;
        ch_apr.jpegConfig.comLength = enc_opt->comLength;
        ch_apr.jpegConfig.pCom = (uint8_t *)enc_opt->pCom;
        ch_apr.jpegConfig.codingWidth = enc_opt->width;
        ch_apr.jpegConfig.codingHeight = enc_opt->height;
    } else {
        set_video_params(enc_opt, &ch_apr);
        
#ifdef ENABLE_DYNAMIC_RES
        // Do not support these features when running dynamic resolution transcoding case
        ch_apr.videoConfig.lookaheadDepth = 0;
        ch_apr.videoConfig.gopSize = 1;
#endif
    }

    ch_apr.enProfiling = 1;

    psnr_total[0] = psnr_total[1] = psnr_total[2] = 0;
    psnr_num = 0;

    ret = vmppEncCreateChannel(&enc_ch, &ch_apr);
    if (ret != vmpp_RSLT_OK) {
        fprintf(stderr, "create channel error %d.\n", ret);
        return -1;
    }

    LOG_INFO("[transcode][enc] channel: %p for %s created.", enc_ch, suffix);
    return 0;
}

void update_psnr (vmppStream* pStream, enc_options *options) {
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
        //printf("MSE %4.2f %4.2f %4.2f\n", pStream->psnrInfo[0], pStream->psnrInfo[1], pStream->psnrInfo[2]);
        //printf("SSIM %4.2f %4.2f %4.2f\n", pStream->psnrInfo[3], pStream->psnrInfo[4], pStream->psnrInfo[5]);
        //printf("psnr_total %4.2f %4.2f %4.2f\n", psnr_total[0], psnr_total[1], psnr_total[2]);

        psnr_num++;
    }
}

static void *encode_thread(void *arg)
{
    vmppChannel enc_ch;
    enc_ch = arg;
    vmppFrame *frame = NULL;
    vmppFrame flushFrame;
    vmppStream stream;
    vmppEncExtendedParams extParams = {0};
    int i = 0;
    vmppFrame *tmp = NULL;
    vmppResult enc_ret = vmpp_RSLT_OK;
    memset(&flushFrame, 0, sizeof(vmppFrame));
    flushFrame.memoryType = vmpp_MEM_FLUSH;
    enc_tick = enc_start = gettime_ns(); 
    enc_total_frames = 0;
    uint64_t total_frames_send = 0; // for test insertIDR

    uint32_t sei_count = 2;
    vmppSEI **sei_array = (vmppSEI **)malloc(sizeof(vmppSEI *) * sei_count);
    if (sei_array) {
        sei_array[0] = (vmppSEI *)malloc(sizeof(vmppSEI));
        sei_array[0]->nalType = vmpp_SEI_PREFIX;
        sei_array[0]->payloadType = SEI_USER_DATA_UNREGISTERED;
        sei_array[0]->payloadData =
            (uint8_t *)"0123456789ABCDEF-01234567890123456789012345678901234567890-index0";
        sei_array[0]->payloadDataSize = strlen((char *)sei_array[0]->payloadData);

        sei_array[1] = (vmppSEI *)malloc(sizeof(vmppSEI));
        sei_array[1]->nalType = vmpp_SEI_PREFIX;
        sei_array[1]->payloadType = SEI_USER_DATA_UNREGISTERED;
        sei_array[1]->payloadData =
            (uint8_t *)"0123456789ABCDEF-01234567890123456789012345678901234567890-ABCindex1";
        sei_array[1]->payloadDataSize = strlen((char *)sei_array[1]->payloadData);
    }

    do {
        pthread_mutex_lock(&frame_mutex);
        frame = vmpp_queue_pop_front(frame_queue);
        pthread_mutex_unlock(&frame_mutex);
        if (!frame) {
            if (!dec_finished) {
                usleep(1000);
                continue;
            } else {
                frame = &flushFrame;
            }
        }
        memset(&stream, 0, sizeof(vmppStream));
        memset(&extParams, 0, sizeof(vmppEncExtendedParams));
        set_extparams(&extParams,enc_total_frames,current_enc_opts,frame);

        // test insert IDR in interval
        if (total_frames_send && current_enc_opts->forceIDRInt &&
            total_frames_send % current_enc_opts->forceIDRInt == 0) {
            extParams.forceIDR = 1;
        }
#ifdef INSERTIDR_TEST
        if (idr_test_flag && compare_idr_index(total_frames_send)) {
            extParams.forceIDR = 1;
        }
#endif

        if (current_enc_opts->ltrInsertTest != 0) {
            // test insert LTR
            if (total_frames_send && (total_frames_send % 18 == 0)) {
                extParams.forceLTR = 1;
            }
        }

        if (total_frames_send && (current_enc_opts->enableDynamicBitrate == 1) && (total_frames_send % 200 == 0)) {
            extParams.updateBitRate = (total_frames_send % 8000) * 1000;//target bitrate, bps
            extParams.updateVbvBufSize = 0;//vbvBufSize, bits, set to 0 if use default
            extParams.updateVbvMaxRate = 0;//vbvMaxRate, bps, set to 0 if use default
            extParams.updateTypeMask |= VMPP_ENC_UPDATE_CBR;
        }

        if (total_frames_send && (current_enc_opts->enableDynamicFrameRate == 1) && (total_frames_send % 300 == 0)) {
            extParams.updateFrameRate.numerator = 60;
            extParams.updateFrameRate.denominator = 1;
            extParams.updateTypeMask |= VMPP_ENC_UPDATE_FRAMERATE;
        }

        if (total_frames_send && (current_enc_opts->enableDynamicCrf == 1) && (total_frames_send % 100 == 0)) {
            extParams.updateCrf = 10 + total_frames_send / 100;
            extParams.updateVbvBufSize = 0;
            extParams.updateVbvMaxRate = 0;
            extParams.updateTypeMask |= VMPP_ENC_UPDATE_CRF;
        }

        frame->timebase.denominator = 0;
        frame->timebase.numerator = 0;
        if (current_enc_opts->vfr) {
            if (total_frames_send >= 198) {
                frame->timebase.denominator = 1;
                frame->timebase.numerator = 48;
            } else if (total_frames_send >= 98) {
                frame->timebase.denominator = 1;
                frame->timebase.numerator = 24;
            } else {
                frame->timebase.denominator = 1;
                frame->timebase.numerator = 48;
            }
        }

        if (total_frames_send && (current_enc_opts->enableDynamicKeyInt == 1) ) {
            if (total_frames_send == 118) {
                extParams.updateKeyInt = 60;
                extParams.updateTypeMask |= VMPP_ENC_UPDATE_KEYINT;
            }
            if (total_frames_send == 410) {
                extParams.updateKeyInt = 120;
                extParams.updateTypeMask |= VMPP_ENC_UPDATE_KEYINT;
            }
        }

        /*** just for test
        if (enc_total_frames && enc_total_frames % 5 == 0) {
            // only effective when videoConfig.enableROI == 1 
            int i = enc_total_frames % VMPP_ENC_MAX_ROI_NUM;
            extParams.roi[i].area.top = 0;
            extParams.roi[i].area.left = 0;
            extParams.roi[i].area.bottom = 200;
            extParams.roi[i].area.right = 200;
            extParams.roi[i].area.enable = 1;
            extParams.roi[i].qpType = vmpp_ENC_QP;
            extParams.roi[i].qpValue = 1;
        }

        if (!frame->seiCount) {
            if (enc_total_frames && enc_total_frames % 6 == 0) {
                frame->seiCount = sei_count;
                frame->seiData = sei_array;
            } else {
                frame->seiCount = 0;
            }
        }

        if (enc_total_frames && enc_total_frames % 7 == 0) {
            extParams.forceIDR = 1;
        }
        ***/

#ifdef ENABLE_DYNAMIC_RES // ENABLE_DYNAMIC_RES
        do {
            enc_ret = vmppEncEncodeFrame(enc_ch, frame, &extParams, &stream, 4000);
            if (enc_ret == vmpp_RSLT_OK) {
                enc_total_frames++;
                if (stream.inputBusAddress == frame->busAddress[0]) {
                    if (stream.inputBusAddress != 0) {
                        vmppDecReleaseFrame(dec_ch, frame, 500);
                        if (frame->memoryType != vmpp_MEM_FLUSH) {
                            pthread_mutex_lock(&frame_mutex);
                            frame->busAddress[0] = (vmppDevAddr)NULL;
                            vmpp_queue_push_back(idle_frame_queue, frame);
                            pthread_mutex_unlock(&frame_mutex);
                        }
                    }
                } else {
                    tmp = NULL;
                    pthread_mutex_lock(&frame_mutex);
                    for (i = 0; i < vmpp_queue_size(releasing_frame_queue); i++) {
                        tmp = vmpp_queue_peek(releasing_frame_queue, i);
                        if (tmp->busAddress[0] == stream.inputBusAddress) {
                            tmp = vmpp_queue_get(releasing_frame_queue, i);
                            break;
                        } else {
                            tmp = NULL;
                        }
                    }
                    pthread_mutex_unlock(&frame_mutex);

                    if (tmp) {
                        vmppDecReleaseFrame(dec_ch, tmp, 500);
                        pthread_mutex_lock(&frame_mutex);
                        tmp->busAddress[0] = (vmppDevAddr)NULL;
                        vmpp_queue_push_back(idle_frame_queue, tmp);
                        pthread_mutex_unlock(&frame_mutex);
                    }

                    if (frame->memoryType != vmpp_MEM_FLUSH) {
                        pthread_mutex_lock(&frame_mutex);
                        vmpp_queue_push_back(releasing_frame_queue, frame);
                        pthread_mutex_unlock(&frame_mutex);
                    }
                }
                if (enc_total_frames % options.period == 0) {
                    uint64_t tmp = gettime_ns() - enc_tick;
                    LOG(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN,
                        "[transcode] Enc Performance: %.3f fps for recent %d frames",
                        ((float)options.period / ((float)tmp / 1000000000.0)), options.period);
                    enc_tick = gettime_ns();
                }

                if (enc_output_file && options.save && (current_enc_opts->svcExtractMaxTLayer == VMPP_ENC_DEFAULT_PAR
                    || stream.svcTemporalId <= current_enc_opts->svcExtractMaxTLayer))
                    fwrite(stream.stream, 1, stream.len, enc_output_file);

                vmppEncReleaseStream(enc_ch, &stream);
                break;
            } else if (enc_ret == vmpp_RSLT_ENC_AGAIN) {
                enc_total_frames++;
                tmp = NULL;
                pthread_mutex_lock(&frame_mutex);
                for (i = 0; i < vmpp_queue_size(releasing_frame_queue); i++) {
                    tmp = vmpp_queue_peek(releasing_frame_queue, i);
                    if (tmp->busAddress[0] == stream.inputBusAddress) {
                        tmp = vmpp_queue_get(releasing_frame_queue, i);
                        break;
                    } else {
                        tmp = NULL;
                    }
                }
                pthread_mutex_unlock(&frame_mutex);

                if (tmp) {
                    vmppDecReleaseFrame(dec_ch, tmp, 500);
                    pthread_mutex_lock(&frame_mutex);
                    tmp->busAddress[0] = (vmppDevAddr)NULL;
                    vmpp_queue_push_back(idle_frame_queue, tmp);
                    pthread_mutex_unlock(&frame_mutex);
                }

                if (enc_total_frames % options.period == 0) {
                    uint64_t tmp = gettime_ns() - enc_tick;
                    LOG(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN,
                        "[transcode] Enc Performance: %.3f fps for recent %d frames",
                        ((float)options.period / ((float)tmp / 1000000000.0)), options.period);
                    enc_tick = gettime_ns();
                }

                if (enc_output_file && options.save)
                    fwrite(stream.stream, 1, stream.len, enc_output_file);
                continue;
            } else if (enc_ret == vmpp_RSLT_ENC_AGAIN_WITH_NO_OUTPUT) {
                continue;
            } else if (enc_ret == vmpp_RSLT_ENC_INPUT_INSERTED) {
                if (frame->memoryType != vmpp_MEM_FLUSH) {
                    pthread_mutex_lock(&frame_mutex);
                    vmpp_queue_push_back(releasing_frame_queue, frame);
                    pthread_mutex_unlock(&frame_mutex);
                }
                break;
            } else if (enc_ret == vmpp_RSLT_WARN_EOS) {
                LOG_WARN("[transcode] Enc Finished!!");
                /* Ignore the END_SEQUENCE */
                if (enc_total_frames > 1)
                    --enc_total_frames;
                goto EOS;
            } else if (enc_ret == vmpp_RSLT_ENC_FLUSH) {
                tmp = NULL;
                pthread_mutex_lock(&frame_mutex);
                for (i = 0; i < vmpp_queue_size(releasing_frame_queue); i++) {
                    tmp = vmpp_queue_peek(releasing_frame_queue, i);
                    if (tmp->busAddress[0] == stream.inputBusAddress) {
                        tmp = vmpp_queue_get(releasing_frame_queue, i);
                        break;
                    } else {
                        tmp = NULL;
                    }
                }
                pthread_mutex_unlock(&frame_mutex);
                if (tmp) {
                    LOG_INFO("[transcode] Enc flush frame: busAddr 0x%llx, private %p",
                             (u64)tmp->busAddress[0], tmp->privateData);
                    vmppDecReleaseFrame(dec_ch, tmp, 500);

                    free(tmp);
                }
                break;
            }
        } while (1);
#else  // ENABLE_DYNAMIC_RES
        enc_ret = vmppEncEncodeFrame(enc_ch, frame, &extParams, &stream, 4000);
        if (enc_ret == vmpp_RSLT_OK || enc_ret == vmpp_RSLT_ENC_INPUT_INSERTED)
            total_frames_send++;
        if (enc_ret == vmpp_RSLT_OK) {
            if (options.default_enc_opts.enableCalcPSNR)
                update_psnr (&stream, &options.default_enc_opts);

            enc_total_frames++;
            if (stream.inputBusAddress == frame->busAddress[0]) {
                if (stream.inputBusAddress != 0) {
                    vmppDecReleaseFrame(dec_ch, frame, 500);
                    if (frame->memoryType != vmpp_MEM_FLUSH) {
                        pthread_mutex_lock(&frame_mutex);
                        frame->busAddress[0] = (vmppDevAddr)NULL;
                        vmpp_queue_push_back(idle_frame_queue, frame);
                        pthread_mutex_unlock(&frame_mutex);
                    }
                }
            } else {
                tmp = NULL;
                pthread_mutex_lock(&frame_mutex);
                for (i = 0; i < vmpp_queue_size(releasing_frame_queue); i++) {
                    tmp = vmpp_queue_peek(releasing_frame_queue, i);
                    if (tmp->busAddress[0] == stream.inputBusAddress) {
                        tmp = vmpp_queue_get(releasing_frame_queue, i);
                        break;
                    } else {
                        tmp = NULL;
                    }
                }
                pthread_mutex_unlock(&frame_mutex);

                if (tmp) {
                    vmppDecReleaseFrame(dec_ch, tmp, 500);
                    pthread_mutex_lock(&frame_mutex);
                    tmp->busAddress[0] = (vmppDevAddr)NULL;
                    vmpp_queue_push_back(idle_frame_queue, tmp);
                    pthread_mutex_unlock(&frame_mutex);
                }

                if (frame->memoryType != vmpp_MEM_FLUSH) {
                    pthread_mutex_lock(&frame_mutex);
                    vmpp_queue_push_back(releasing_frame_queue, frame);
                    pthread_mutex_unlock(&frame_mutex);
                }
            }
            if (enc_total_frames % options.period == 0) {
                uint64_t tmp = gettime_ns() - enc_tick;
                LOG(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN,
                    "[transcode] Enc Performance: %.3f fps for recent %d frames",
                    ((float)options.period / ((float)tmp / 1000000000.0)), options.period);
                enc_tick = gettime_ns();
            }

            if (enc_output_file && options.save && (current_enc_opts->svcExtractMaxTLayer == VMPP_ENC_DEFAULT_PAR
                || stream.svcTemporalId <= current_enc_opts->svcExtractMaxTLayer))
                fwrite(stream.stream, 1, stream.len, enc_output_file);

            vmppEncReleaseStream(enc_ch, &stream);
            continue;
        } else if (enc_ret == vmpp_RSLT_ENC_INPUT_INSERTED) {
            if (frame->memoryType != vmpp_MEM_FLUSH) {
                pthread_mutex_lock(&frame_mutex);
                vmpp_queue_push_back(releasing_frame_queue, frame);
                pthread_mutex_unlock(&frame_mutex);
            }
        } else if (enc_ret == vmpp_RSLT_WARN_EOS) {
            LOG_WARN("[transcode] Enc Finished!!");
            /* Ignore the END_SEQUENCE */
            if (enc_total_frames > 1)
                --enc_total_frames;
            break;
        } else if (enc_ret == vmpp_RSLT_ENC_FLUSH) {
            tmp = NULL;
            pthread_mutex_lock(&frame_mutex);
            for (i = 0; i < vmpp_queue_size(releasing_frame_queue); i++) {
                tmp = vmpp_queue_peek(releasing_frame_queue, i);
                if (tmp->busAddress[0] == stream.inputBusAddress) {
                    tmp = vmpp_queue_get(releasing_frame_queue, i);
                    break;
                } else {
                    tmp = NULL;
                }
            }
            pthread_mutex_unlock(&frame_mutex);
            if (tmp) {
                LOG_INFO("[transcode] Enc flush frame: busAddr 0x%llx, private %p",
                        (u64)tmp->busAddress[0], tmp->privateData);
                vmppDecReleaseFrame(dec_ch, tmp, 500);

                free(tmp);
            }
        } else {
            if (enc_ret == vmpp_RSLT_ERR_ENC_INIT) {
                LOG_ERROR("[transcode] vmppEncEncodeFrame ret = %d", enc_ret);
                enc_error = 1;
                break;
            }
        }
#endif // ENABLE_DYNAMIC_RES
    } while (1);

#ifdef ENABLE_DYNAMIC_RES
EOS:
#endif
    pthread_mutex_lock(&frame_mutex);
    /* These releasing steps are additional protection usd for releasing decode frames,
     * should not be triggered at all */
    for (i = 0; i < vmpp_queue_size(releasing_frame_queue); i++) {
        tmp = vmpp_queue_peek(releasing_frame_queue, i);
        if (tmp->busAddress[0]) {
            LOG_WARN("[transcode] Enc flush remained frames in releasing_frame_queue: busAddr "
                     "0x%llx, private %p",
                     (u64)tmp->busAddress[0], tmp->privateData);
            vmppDecReleaseFrame(dec_ch, tmp, 500);
        }
    }

    for (i = 0; i < vmpp_queue_size(frame_queue); i++) {
        tmp = vmpp_queue_peek(frame_queue, i);
        if (tmp->busAddress[0]) {
            LOG_WARN(
                "[transcode] Enc flush remained frames in frame_queue: busAddr 0x%llx, private %p",
                (u64)tmp->busAddress[0], tmp->privateData);
            vmppDecReleaseFrame(dec_ch, tmp, 500);
        }
    }

    for (i = 0; i < vmpp_queue_size(idle_frame_queue); i++) {
        tmp = vmpp_queue_peek(idle_frame_queue, i);
        if (tmp->busAddress[0]) {
            LOG_WARN("[transcode] Enc flush remained frames in idle_frame_queue: busAddr 0x%llx, "
                     "private %p",
                     (u64)tmp->busAddress[0], tmp->privateData);
            vmppDecReleaseFrame(dec_ch, tmp, 500);
        }
    }
    pthread_mutex_unlock(&frame_mutex);

    enc_stop = gettime_ns();
    if (enc_total_frames) {
        float timePerframe = (float)(enc_stop - enc_start) / (float)enc_total_frames;
        if (options.default_enc_opts.enableCalcPSNR && psnr_num) {
            LOG(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN,
                "[transcode] Average PSNR[%d]: Y %4.2f, U %4.2f, V %4.2f",
                psnr_num,
                psnr_total[0] / psnr_num,
                psnr_total[1] / psnr_num,
                psnr_total[2] / psnr_num);
        }
        LOG(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN,
            "[transcode] Enc Performance: %.3f fps for total %lld frames (%.2f us per frame)",
            1000000000.0f / timePerframe, (u64)enc_total_frames, timePerframe / 1000.0f);
    }

    if (sei_array) {
        uint32_t i;
        for (i = 0; i < sei_count; i++) {
            if (sei_array[i]) {
                free(sei_array[i]);
                sei_array[i] = NULL;
            }
        }
        free(sei_array);
        sei_array = NULL;
    }

    return NULL;
}

static int parse_json(const char *json)
{
    int ret = 0;
    FILE *json_file = NULL;
    char *json_buffer = NULL;
    int json_size = 0;
    cJSON *root;
    cJSON *arrayItem, *item, *subItem;
    int item_count = 0;
    int i = 0;
    char *tmp = NULL;
    struct stat st;
    int len = 0;

    json_file = fopen(json, "rb");
    if (!json_file) {
        LOG_ERROR("[transcode] Fail to open json file %s", json);
        ret = -1;
        goto fail;
    }
    fseek(json_file, 0, SEEK_END);
    json_size = (int)ftell(json_file);
    fseek(json_file, 0, SEEK_SET);
    json_buffer = (char *)malloc(json_size + 1);
    if (!json_buffer) {
        LOG_ERROR("[transcode] Fail to malloc json buffer. size: %d", json_size);
        ret = -1;
        goto fail;
    }
    //memset(json_buffer, 0, json_size + 1);
    fread(json_buffer, 1, json_size, json_file);
    json_buffer[json_size] = '\0';

    root = cJSON_Parse(json_buffer);
    if (!root) {
        LOG_ERROR("[transcode] Fail to parse json file, err: %s", cJSON_GetErrorPtr());
        ret = -1;
        goto fail;
    }

    item = cJSON_GetObjectItem(root, "encDevice");
    if (item) {
        tmp = (char *)malloc(32);
        if (!tmp) {
            LOG_ERROR("[transcode] fail to malloc buffer for file url in json(%s): %s", json, item->valuestring);
            ret = -1;
            goto fail;
        }
        strcpy(tmp, item->valuestring);
        options.encDevice = tmp;
    }

    item = cJSON_GetObjectItem(root, "decDevice");
    if (item) {
        tmp = (char *)malloc(32);
        if (!tmp) {
            LOG_ERROR("[transcode] fail to malloc buffer for file url in json(%s): %s", json, item->valuestring);
            ret = -1;
            goto fail;
        }
        strcpy(tmp, item->valuestring);
        options.decDevice = tmp;
    }

    item = cJSON_GetObjectItem(root, "memDevice");
    if (item) {
        tmp = (char *)malloc(32);
        if (!tmp) {
            LOG_ERROR("[transcode] fail to malloc buffer for file url in json(%s): %s", json, item->valuestring);
            ret = -1;
            goto fail;
        }
        strcpy(tmp, item->valuestring);
        options.memDevice = tmp;
    }

    arrayItem = cJSON_GetObjectItem(root, "url");
    if (arrayItem) {
        item_count = cJSON_GetArraySize(arrayItem);
        for (i = 0; i < item_count; i++) {
            item = cJSON_GetArrayItem(arrayItem, i);
            len = strlen(item->valuestring) + 1;
            tmp = (char *)malloc(len);
            if (!tmp) {
                LOG_ERROR("[transcode] fail to malloc buffer for file url in json(%s): %s", json,
                          item->valuestring);
                ret = -1;
                goto fail;
            }
            //memset(tmp, 0, len);
            strcpy(tmp, item->valuestring);
            vmpp_queue_push_back(options.urls, tmp);
        }
    }

    arrayItem = cJSON_GetObjectItem(root, "folder");
    if (arrayItem) {
        item_count = cJSON_GetArraySize(arrayItem);
        for (i = 0; i < item_count; i++) {
            item = cJSON_GetArrayItem(arrayItem, i);

            // check dir validation
            memset(&st, 0, sizeof(struct stat));
            lstat(item->valuestring, &st);
            if (!S_ISDIR(st.st_mode))
                continue;

            read_files_from_dir(options.urls, item->valuestring);
        }
    }

    arrayItem = cJSON_GetObjectItem(root, "encoder_params");
    if (arrayItem) {
        item_count = cJSON_GetArraySize(arrayItem);
        for (i = 0; i < item_count; i++) {
            enc_options *enc_opt = (enc_options *)malloc(sizeof(enc_options));
            if (!enc_opt) {
                LOG_ERROR("[transcode] fail to malloc buffer for encoder options in json(%s): %s",
                          json, item->valuestring);
                ret = -1;
                goto fail;
            }
            memset(enc_opt, 0, sizeof(enc_options));
            set_default_enc_params(enc_opt);
            item = cJSON_GetArrayItem(arrayItem, i);
            enc_opt->encDevice = options.encDevice;
            enc_opt->memDevice = options.memDevice;
            if (item) {
                subItem = cJSON_GetObjectItem(item, "codec");
                if (subItem) {
                    if (strcmp(subItem->valuestring, "hevc") == 0)
                        enc_opt->enc_codec = "hevc";
                    else if (strcmp(subItem->valuestring, "h264") == 0)
                        enc_opt->enc_codec = "h264";
                    else if (strcmp(subItem->valuestring, "av1") == 0)
                        enc_opt->enc_codec = "av1";
                    else if (strcmp(subItem->valuestring, "jpeg") == 0)
                        enc_opt->enc_codec = "jpeg";
                }

                subItem = cJSON_GetObjectItem(item, "profile");
                if (subItem)
                    enc_opt->profile = subItem->valueint;

                subItem = cJSON_GetObjectItem(item, "level");
                if (subItem)
                    enc_opt->level = subItem->valueint;

                subItem = cJSON_GetObjectItem(item, "gopSize");
                if (subItem)
                    enc_opt->gopSize = subItem->valueint;

                subItem = cJSON_GetObjectItem(item, "frameRateNum");
                if (subItem)
                    enc_opt->frameRateNum = subItem->valueint;

                subItem = cJSON_GetObjectItem(item, "frameRateDen");
                if (subItem)
                    enc_opt->frameRateDen = subItem->valueint;

                subItem = cJSON_GetObjectItem(item, "bitDepthLuma");
                if (subItem)
                    enc_opt->bitDepthLuma = subItem->valueint;

                subItem = cJSON_GetObjectItem(item, "bitDepthChroma");
                if (subItem)
                    enc_opt->bitDepthChroma = subItem->valueint;

                subItem = cJSON_GetObjectItem(item, "lookaheadDepth");
                if (subItem)
                    enc_opt->lookaheadDepth = subItem->valueint;

                subItem = cJSON_GetObjectItem(item, "tune");
                if (subItem)
                    enc_opt->tune = subItem->valueint;

                subItem = cJSON_GetObjectItem(item, "keyInt");
                if (subItem)
                    enc_opt->keyInt = subItem->valueint;

                subItem = cJSON_GetObjectItem(item, "gdrDuration");
                if (subItem)
                    enc_opt->gdrDuration = subItem->valueint;

                subItem = cJSON_GetObjectItem(item, "crf");
                if (subItem)
                    enc_opt->crf = subItem->valueint;

                subItem = cJSON_GetObjectItem(item, "cqp");
                if (subItem)
                    enc_opt->cqp = subItem->valueint;

                subItem = cJSON_GetObjectItem(item, "llRc");
                if (subItem)
                    enc_opt->llRc = subItem->valueint;

                subItem = cJSON_GetObjectItem(item, "bitRate");
                if (subItem)
                    enc_opt->bitRate = subItem->valueint;

                subItem = cJSON_GetObjectItem(item, "initQp");
                if (subItem)
                    enc_opt->initQp = subItem->valueint;

                subItem = cJSON_GetObjectItem(item, "vbvBufSize");
                if (subItem)
                    enc_opt->vbvBufSize = subItem->valueint;

                subItem = cJSON_GetObjectItem(item, "vbvMaxRate");
                if (subItem)
                    enc_opt->vbvMaxRate = subItem->valueint;

                subItem = cJSON_GetObjectItem(item, "intraQpDelta");
                if (subItem)
                    enc_opt->intraQpDelta = subItem->valueint;

                subItem = cJSON_GetObjectItem(item, "qpMinI");
                if (subItem)
                    enc_opt->qpMinI = subItem->valueint;

                subItem = cJSON_GetObjectItem(item, "qpMaxI");
                if (subItem)
                    enc_opt->qpMaxI = subItem->valueint;

                subItem = cJSON_GetObjectItem(item, "qpMinPB");
                if (subItem)
                    enc_opt->qpMinPB = subItem->valueint;

                subItem = cJSON_GetObjectItem(item, "qpMaxPB");
                if (subItem)
                    enc_opt->qpMaxPB = subItem->valueint;

                subItem = cJSON_GetObjectItem(item, "aqStrength");
                if (subItem)
                    enc_opt->aqStrength = subItem->valuedouble;

                subItem = cJSON_GetObjectItem(item, "qualityMode");
                if (subItem)
                    enc_opt->qualityMode = subItem->valueint;

                subItem = cJSON_GetObjectItem(item, "vbr");
                if (subItem)
                    enc_opt->vbr = subItem->valueint;

                subItem = cJSON_GetObjectItem(item, "pCom");
                if (subItem) {
                    len = strlen(subItem->valuestring) + 1;
                    tmp = (char *)malloc(len);
                    if (!tmp) {
                        LOG_ERROR("[transcode] fail to malloc buffer for pCom in json(%s): %s",
                                  json, subItem->valuestring);
                        ret = -1;
                        goto fail;
                    }
                    //memset(tmp, 0, len);
                    strcpy(tmp, subItem->valuestring);
                    enc_opt->pCom = tmp;
                    enc_opt->comLength = strlen(subItem->valuestring);
                }

                subItem = cJSON_GetObjectItem(item, "P2B");
                if (subItem)
                    enc_opt->P2B = subItem->valueint;

                subItem = cJSON_GetObjectItem(item, "bBPyramid");
                if (subItem)
                    enc_opt->bBPyramid = subItem->valueint;
            }

            vmpp_queue_push_back(options.enc_options, enc_opt);
        }
    }
fail:
    if (json_buffer)
        free(json_buffer);
    if (json_file)
        fclose(json_file);
    return ret;
}

static int parse_options(int argc, char **argv)
{
    //static const char optstr[] = "i:o:r:l:L:s:m:c:f:p:P:n:b:h:?:C:";
    char *suffix = NULL;
    char *url = NULL;
    //int c;
    struct stat st;

    struct parameter prm;
    int ret;
    char *optarg;
    prm.cnt = 1;

    if (argc == 2 && !strcmp(argv[1], "-h")) {
        usage(argv[0]);
        return -1;
    }

    while ((ret = get_option(argc, argv, ops, &prm)) != -1) {
        if (ret == -2 && prm.enable == 1) {
            LOG_ERROR("Unassigned value,please check the parameters");
            return -1;
        }
        optarg = prm.argument;
        switch (prm.short_opt) {
        case 'i':
            memset(&st, 0, sizeof(struct stat));
            lstat(optarg, &st);
            if (S_ISDIR(st.st_mode))
                read_files_from_dir(options.urls, optarg);
            else if (S_ISREG(st.st_mode)) {
                suffix = strrchr(optarg, '.');
                if (suffix && 0 == strcmp(suffix, ".json")) {
                    if (parse_json(optarg) < 0) {
                        LOG_ERROR("[transcode] Invalid json file: %s", optarg);
                        usage(argv[0]);
                        return -1;
                    }
                } else {
                    url = (char *)malloc((strlen(optarg) + 1) * sizeof(char));
                    if (!url) {
                        LOG_ERROR("[transcode] fail to malloc buffer for file url: %s", optarg);
                        return -1;
                    }
                    memset(url, 0, (strlen(optarg) + 1) * sizeof(char));
                    strcpy(url, optarg);
                    vmpp_queue_push_back(options.urls, url);
                }
            } else {
                has_irregular_url = 1;
                LOG_WARN("[transcode] url: %s is not a regular file or directory", optarg);
                url = (char *)malloc((strlen(optarg) + 1) * sizeof(char));
                if (!url) {
                    LOG_ERROR("[transcode] fail to malloc buffer for file url: %s", optarg);
                    return -1;
                }
                memset(url, 0, (strlen(optarg) + 1) * sizeof(char));
                strcpy(url, optarg);
                vmpp_queue_push_back(options.urls, url);
            }

            break;
        case 'o':
            memset(&st, 0, sizeof(struct stat));
            lstat(optarg, &st);
            if (S_ISDIR(st.st_mode))
                options.output_directory = optarg;
            else
                options.output_file = optarg;
            break;
        case 'l':
            options.loop = atoi(optarg);
            if (options.loop < 0)
                options.loop = INT32_MAX - 1;
            break;
        case 'L':
            options.main_loop = atoi(optarg);
            if (options.main_loop < 0)
                options.main_loop = INT32_MAX - 1;
            break;
        case 's':
            options.save = atoi(optarg);
            break;
        case 'm':
            options.check_md5 = atoi(optarg);
            break;
        case 'c':
            options.codec = optarg;
            break;
        case 'C':
            options.enc_codec = optarg;
            break;
        case 'f':
        #ifdef USING_FFMPEG
            options.using_ffmpeg = atoi(optarg);
        #else
            if (atoi(optarg) == 1) {
                LOG_ERROR("[transcode] using_ffmpeg can not be set!");
                usage(argv[0]);
                return -1;
            }
        #endif
            break;
        case 'p':
            options.period = atoi(optarg);
            if (options.period <= 0) {
                LOG_WARN("[transcode] Invalid period frame count <%d>, using default value 100",
                         options.period);
                options.period = DEFAULT_PERIOD_FRAMES;
            }
            break;
        case 'P':
            options.perf_period = atoi(optarg);
            if (options.perf_period <= 0) {
                LOG_WARN("[transcode] Invalid period frame count <%d> for performance, using "
                         "default value 1000",
                         options.perf_period);
                options.perf_period = PERF_PERIOD_FRAMES;
            }
            break;
        case 'n':
            options.vframes = atoi(optarg);
            break;
        case 'b':
            options.bitDepth = atoi(optarg);
            break;
        case 'M':
            options.memory_mode = atoi(optarg);
            assert(options.memory_mode != vmpp_DEC_MEM_USER_OUT_BUF_DEV &&
                   "vmpp_DEC_MEM_USER_OUT_BUF_DEV is not supported by this transcoding application "
                   "yet!\n");
            break;
#ifdef INSERTIDR_TEST
            case 'I':
            split_idr_params(optarg);
            break;
#endif
        case '?':
        case 'h':
            usage(argv[0]);
            return -1;
        case '0':
            if (strcmp(prm.longOpt, "encDevice") == 0)
                options.encDevice = options.default_enc_opts.encDevice = optarg;
            if (strcmp(prm.longOpt, "decDevice") == 0)
                options.decDevice = optarg;
            if (strcmp(prm.longOpt, "memDevice") == 0)
                options.memDevice = options.default_enc_opts.memDevice = optarg;

            if (strcmp(prm.longOpt, "profile") == 0)
                options.default_enc_opts.profile = atoi(optarg);
            if (strcmp(prm.longOpt, "level") == 0)
                options.default_enc_opts.level = atoi(optarg);
            if (strcmp(prm.longOpt, "frameRateNum") == 0)
                options.default_enc_opts.frameRateNum = atoi(optarg);
            if (strcmp(prm.longOpt, "frameRateDen") == 0)
                options.default_enc_opts.frameRateDen = atoi(optarg);
            if (strcmp(prm.longOpt, "bitDepthLuma") == 0)
                options.default_enc_opts.bitDepthLuma = atoi(optarg);
            if (strcmp(prm.longOpt, "bitDepthChroma") == 0)
                options.default_enc_opts.bitDepthChroma = atoi(optarg);
            if (strcmp(prm.longOpt, "gopSize") == 0)
                options.default_enc_opts.gopSize = atoi(optarg);
            if (strcmp(prm.longOpt, "gdrDuration") == 0)
                options.default_enc_opts.gdrDuration = atoi(optarg);
            if (strcmp(prm.longOpt, "lookaheadDepth") == 0)
                options.default_enc_opts.lookaheadDepth = atoi(optarg);
            if (strcmp(prm.longOpt, "qualityMode") == 0)
                options.default_enc_opts.qualityMode = atoi(optarg);
            if (strcmp(prm.longOpt, "tune") == 0)
                options.default_enc_opts.tune = atoi(optarg);
            if (strcmp(prm.longOpt, "keyInt") == 0)
                options.default_enc_opts.keyInt = atoi(optarg);
            if (strcmp(prm.longOpt, "crf") == 0)
                options.default_enc_opts.crf = atoi(optarg);
            if (strcmp(prm.longOpt, "cqp") == 0)
                options.default_enc_opts.cqp = atoi(optarg);
            if (strcmp(prm.longOpt, "llRc") == 0)
                options.default_enc_opts.llRc = atoi(optarg);
            if (strcmp(prm.longOpt, "bitRate") == 0)
                options.default_enc_opts.bitRate = atoi(optarg);
            if (strcmp(prm.longOpt, "initQp") == 0)
                options.default_enc_opts.initQp = atoi(optarg);
            if (strcmp(prm.longOpt, "vbvBufSize") == 0)
                options.default_enc_opts.vbvBufSize = atoi(optarg) * 1000;
            if (strcmp(prm.longOpt, "vbvMaxRate") == 0)
                options.default_enc_opts.vbvMaxRate = atoi(optarg) * 1000;
            if (strcmp(prm.longOpt, "intraQpDelta") == 0)
                options.default_enc_opts.intraQpDelta = atoi(optarg);
            if (strcmp(prm.longOpt, "qpMinI") == 0)
                options.default_enc_opts.qpMinI = atoi(optarg);
            if (strcmp(prm.longOpt, "qpMaxI") == 0)
                options.default_enc_opts.qpMaxI = atoi(optarg);
            if (strcmp(prm.longOpt, "qpMinPB") == 0)
                options.default_enc_opts.qpMinPB = atoi(optarg);
            if (strcmp(prm.longOpt, "qpMaxPB") == 0)
                options.default_enc_opts.qpMaxPB = atoi(optarg);
            if (strcmp(prm.longOpt, "aqStrength") == 0)
                options.default_enc_opts.aqStrength = atof(optarg);
            if (strcmp(prm.longOpt, "P2B") == 0)
                options.default_enc_opts.P2B = atoi(optarg);
            if (strcmp(prm.longOpt, "bBPyramid") == 0)
                options.default_enc_opts.bBPyramid = atof(optarg);
            if (strcmp(prm.longOpt, "maxFrameSizeMultiple") == 0)
                options.default_enc_opts.maxFrameSizeMultiple = atof(optarg);
            if (strcmp(prm.longOpt, "maxFrameSize") == 0)
                options.default_enc_opts.maxFrameSize = atoi(optarg);
            if (strcmp(prm.longOpt, "roiType") == 0)
                options.default_enc_opts.roiType = atoi(optarg);
            if (strcmp(prm.longOpt, "roiInt") == 0)
                options.default_enc_opts.roiInt = atoi(optarg);
            if (strcmp(prm.longOpt, "roiParam") == 0)
                options.default_enc_opts.roiParam = optarg;
            if (strcmp(prm.longOpt, "extSEIInt") == 0)
                options.default_enc_opts.extSEIInt = atoi(optarg);
            if (strcmp(prm.longOpt, "forceIDRInt") == 0)
                options.default_enc_opts.forceIDRInt = atoi(optarg);
            if (strcmp(prm.longOpt, "logLevel") == 0)
                options.default_enc_opts.logLevel = atoi(optarg);
            if (strcmp(prm.longOpt, "roiMapDeltaQpBlockUnit") == 0)
                options.default_enc_opts.roiMapDeltaQpBlockUnit = atoi(optarg);
            if (strcmp(prm.longOpt, "roiMapQpDeltaVersion") == 0)
                options.default_enc_opts.roiMapQpDeltaVersion = atoi(optarg);
            if (strcmp(prm.longOpt, "enableDynamicBitrate") == 0)
                options.default_enc_opts.enableDynamicBitrate = atoi(optarg);
            if (strcmp(prm.longOpt, "enableDynamicFrameRate") == 0)
                options.default_enc_opts.enableDynamicFrameRate = atoi(optarg);
            if (strcmp(prm.longOpt, "maxBFrames") == 0)
                options.default_enc_opts.maxBFrames = atoi(optarg);
            if (strcmp(prm.longOpt, "hrd") == 0)
                options.default_enc_opts.hrd = atoi(optarg);
            if (strcmp(prm.longOpt, "picSkip") == 0)
                options.default_enc_opts.pictureSkip = atoi(optarg);
            if (strcmp(prm.longOpt, "vfr") == 0)
                options.default_enc_opts.vfr = atoi(optarg);
            if (strcmp(prm.longOpt, "svcTLayers") == 0)
                options.default_enc_opts.svcTLayers = atoi(optarg);
            if (strcmp(prm.longOpt, "svcExtractMaxTLayer") == 0)
                options.default_enc_opts.svcExtractMaxTLayer = atoi(optarg);
            if (strcmp(prm.longOpt, "sliceSize") == 0)
                options.default_enc_opts.sliceSize = atoi(optarg);
            if (strcmp(prm.longOpt, "enableDynamicCrf") == 0)
                options.default_enc_opts.enableDynamicCrf = atoi(optarg);
            if (strcmp(prm.longOpt, "psnr") == 0)
                options.default_enc_opts.enableCalcPSNR = atoi(optarg);
            if (strcmp(prm.longOpt, "ssim") == 0)
                options.default_enc_opts.enableCalcSSIM = atoi(optarg);
            if (strcmp(prm.longOpt, "ltrInterval") == 0)
                options.default_enc_opts.ltrInterval = atoi(optarg);
            if (strcmp(prm.longOpt, "ltrQpDelta") == 0)
                options.default_enc_opts.ltrQpDelta = atoi(optarg);
            if (strcmp(prm.longOpt, "ltrRefGap") == 0)
                options.default_enc_opts.ltrRefGap = atoi(optarg);
            if (strcmp(prm.longOpt, "ltrInsertTest") == 0)
                options.default_enc_opts.ltrInsertTest = atoi(optarg);
            if (strcmp(prm.longOpt, "rotation") == 0)
                options.default_enc_opts.rotation = atoi(optarg);
            if (strcmp(prm.longOpt, "coreID") == 0)
                options.default_enc_opts.coreID = atoi(optarg);
            if (strcmp(prm.longOpt, "openGop") == 0)
                options.default_enc_opts.openGop = atof(optarg);
            if (strcmp(prm.longOpt, "smartEnc") == 0)
                options.default_enc_opts.smartEnc = atof(optarg);
            if (strcmp(prm.longOpt, "enableDynamicKeyInt") == 0)
                options.default_enc_opts.enableDynamicKeyInt = atoi(optarg);
            if (strcmp(prm.longOpt, "disableMMCO") == 0)
                options.default_enc_opts.disableMMCO = atoi(optarg);
            if (strcmp(prm.longOpt, "inLoopDSRatio") == 0)
                options.default_enc_opts.inLoopDSRatio = atoi(optarg);
            if (strcmp(prm.longOpt, "aqMode") == 0)
                options.default_enc_opts.aqMode = atoi(optarg);
            if (strcmp(prm.longOpt, "psyFactor") == 0)
                options.default_enc_opts.psyFactor = atof(optarg);
            if (strcmp(prm.longOpt, "rdoLevel") == 0)
                options.default_enc_opts.rdoLevel = atoi(optarg);
            if (strcmp(prm.longOpt, "enableRdoQuant") == 0)
                options.default_enc_opts.enableRdoQuant = atoi(optarg);
            if (strcmp(prm.longOpt, "qCompress") == 0)
                options.default_enc_opts.qCompress = atof(optarg);
            if (strcmp(prm.longOpt, "bitRateBalanceLevel") == 0)
                options.default_enc_opts.bitRateBalanceLevel = atoi(optarg);
            if (strcmp(prm.longOpt, "multicore") == 0)
                options.default_enc_opts.multicore = atoi(optarg);
            break;
        default:
            LOG_ERROR("[transcode] Unsupported option: %c", ret);
            usage(argv[0]);
            return -1;
        }
    }

    if (options.loop > 1) {
        /* No mean for md5-checking when it's running file-loop mode */
        LOG_WARN("[transcode] You are checking MD5 in 'file-loop' mode.");
        // options.check_md5 = 0;
    }

    if (!options.using_ffmpeg && !options.codec) {
        LOG_ERROR("[transcode] codec must be set when not using ffmpeg!");
        usage(argv[0]);
        return -1;
    }

    if (!options.using_ffmpeg && has_irregular_url)
        LOG_WARN(
            "[transcode] irregular url(s) may be invalid, you may try to open it with FFmpeg!");

    if (options.save && (!options.output_directory && !options.output_file)) {
        LOG_ERROR(
            "[transcode] output directory/filename must be specified if you want to save yuv!");
        usage(argv[0]);
        return -1;
    }

    if (options.period <= 0)
        options.period = DEFAULT_PERIOD_FRAMES;
    if (options.perf_period <= 0)
        options.perf_period = PERF_PERIOD_FRAMES;
/*
    if (optind < argc) {
        LOG_ERROR("[transcode] Invalid parameters!");
        usage(argv[0]);
        return -1;
    }
*/
    return 0;
}

unsigned char *compute_md5sum(const unsigned char *buf_in, const int buf_len,
                              unsigned char *str_md5sum)
{
    int to_final = 0;
    if (buf_in == NULL || buf_len == 0) {
        to_final = 1;
    }

    /* Initialization of MD5 context. */
    if (md5ctx_inited == 0) {
        md5_init(&md5ctx);
        md5ctx_inited = 1;
    }

    if (to_final == 0) {
        md5_update(&md5ctx, buf_in, (unsigned long)buf_len);
    } else {
        md5_final(str_md5sum, &md5ctx);
        md5ctx_inited = 0;
    }
    return str_md5sum;
}

static void check_enc_params(enc_options *opts, vmppFrame *frame)
{
    /*
    if (strcmp(opts->enc_codec, "h264") == 0) {
        if (opts->profile >= vmpp_VIDEO_PRFL_HEVC_MAIN &&
            opts->profile <= vmpp_VIDEO_PRFL_HEVC_MAIN_REXT) {
            opts->profile = vmpp_VIDEO_PRFL_H264_HIGH;
            opts->level = vmpp_VIDEO_LVL_H264_5_1;
        }
    } else if (strcmp(opts->enc_codec, "hevc") == 0) {
        if (opts->profile >= vmpp_VIDEO_PRFL_H264_BASELINE &&
            opts->profile <= vmpp_VIDEO_PRFL_H264_HIGH_10) {
            opts->profile = vmpp_VIDEO_PRFL_HEVC_MAIN;
            opts->level = vmpp_VIDEO_LVL_HEVC_6;
        }
    }
    */
    opts->width = frame->cropInfo.flag ? frame->cropInfo.width : frame->width;
    opts->height = frame->cropInfo.flag ? frame->cropInfo.height : frame->height;
}

static void *output_thread(void *arg)
{
    vmppFrame out_frame;
    vmppDecOutputOptions out_opt;
    void *dec_ch;
    vmppResult ret;
    char output_file_str[MAX_PATH_LEN] = {0};
    FILE *output_file = NULL;
    uint64_t file_decode_time = 0;
    uint64_t write_start;
    uint64_t md5_start;
    dec_ch = arg;
    uint8_t *frame_buffer = NULL;

    if (options.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_HOST) {
        frame_buffer = malloc(MAX_VIDEO_DEC_WIDTH * MAX_VIDEO_DEC_HEIGHT * 3 / 2);
    }

    if (encoder_enable == 1)
        out_opt.memoryType = vmpp_MEM_DEVICE;
    else
        out_opt.memoryType = vmpp_MEM_HOST;

    out_opt.enableCrop = 1;
    if (options.save && out_opt.memoryType == vmpp_MEM_HOST) {
        get_output_file(output_file_str, "yuv");
        output_file = fopen(output_file_str, "wb");
        if (!output_file)
            LOG_WARN("[transcode] Fail to open output file <%s>", output_file_str);
    }

    uint32_t out_count = 0;
    uint32_t period_out_count = 0;

    while (1) {
        memset(&out_frame, 0, sizeof(out_frame));
        if (options.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_HOST) {
            if (out_opt.memoryType == vmpp_MEM_HOST)
                out_frame.data[0] = frame_buffer;
        }

        ret = vmppDecReceiveFrame(dec_ch, &out_frame, &out_opt, 500);
        if (ret == vmpp_RSLT_OK) {
            out_count++;
            period_out_count++;
            if (out_count % options.period == 0)
                LOG_DEBUG("[transcode] vmppDecReceiveFrame (%d) succeed. pts %lld, "
                          "frame_type %d, format %d, "
                          "ori_wh: %dx%d, crop_wh: %dx%d",
                          out_count, (u64)out_frame.pts, out_frame.frameType, out_frame.pixelFormat,
                          out_frame.width, out_frame.height, out_frame.cropInfo.width,
                          out_frame.cropInfo.height);
            if (period_out_count % options.period == 0) {
                file_decode_time = gettime_ns() - period_file_start_time;
                LOG(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN,
                    "[transcode] Dec Performance: %.3f fps for recent %d frames; Detail: "
                    "decode %d us (%d us/f), out_frame %dx%d, out_frame.seiCount %d",
                    ((float)period_out_count / ((float)file_decode_time / 1000000000.0)),
                    period_out_count, (int)(file_decode_time / 1000.0),
                    (int)(file_decode_time / period_out_count / 1000.0), out_frame.width,
                    out_frame.height, out_frame.seiCount);
                period_out_count = 0;
                period_file_start_time = gettime_ns();
            }
            if (encoder_enable == 0) {
                /* check md5. */
                if (options.check_md5 && out_frame.memoryType == vmpp_MEM_HOST) {
                    md5_start = gettime_ns();
                    compute_md5sum((const unsigned char *)out_frame.data[0], out_frame.dataSize,
                                   cur_md5sum);
                    check_md5_time += (gettime_ns() - md5_start);
                }
                /* save output file. */
                if (/*out_count % options.period == 0 && */ options.save && output_file &&
                    out_frame.memoryType == vmpp_MEM_HOST) {
                    write_start = gettime_ns();
                    if (out_opt.enableCrop == 1) {
                        // write Y
                        fwrite(out_frame.data[0], 1,
                               out_frame.cropInfo.width * out_frame.cropInfo.height, output_file);

                        // write UV
                        if (out_frame.data[1]) {
                            uint32_t uv_crop_width = (out_frame.cropInfo.width % 2 == 1)
                                                         ? (out_frame.cropInfo.width + 1)
                                                         : (out_frame.cropInfo.width);
                            uint32_t uv_crop_height = (out_frame.cropInfo.height % 2 == 1)
                                                          ? (out_frame.cropInfo.height + 1)
                                                          : (out_frame.cropInfo.height);

                            fwrite(out_frame.data[1], 1, uv_crop_width * uv_crop_height / 2,
                                   output_file);
                        }

                    } else {
                        // write Y
                        for (uint32_t j = 0; j < out_frame.cropInfo.height; j++) {
                            fwrite(out_frame.data[0] + out_frame.width * j, 1,
                                   out_frame.cropInfo.width, output_file);
                        }

                        // write UV
                        if (out_frame.data[1]) {
                            uint32_t uv_crop_width = (out_frame.cropInfo.width % 2 == 1)
                                                         ? (out_frame.cropInfo.width + 1)
                                                         : (out_frame.cropInfo.width);
                            uint32_t uv_crop_height = (out_frame.cropInfo.height % 2 == 1)
                                                          ? (out_frame.cropInfo.height + 1)
                                                          : (out_frame.cropInfo.height);
                            for (uint32_t j = 0; j < uv_crop_height / 2; j++) {
                                fwrite(out_frame.data[1] + out_frame.width * j, 1, uv_crop_width,
                                       output_file);
                            }
                        }
                    }
                    file_write_time += (gettime_ns() - write_start);
                }
                ret = vmppDecReleaseFrame(dec_ch, &out_frame, 500);
                if (ret < 0)
                    LOG_WARN("[transcode] release frame error %d", ret);
            } else {
                vmppFrame *tmp = NULL;
                pthread_mutex_lock(&frame_mutex);
                if (vmpp_queue_size(idle_frame_queue)) {
                    tmp = vmpp_queue_pop_front(idle_frame_queue);
                } else {
                    tmp = malloc(sizeof(vmppFrame));
                }
                if (tmp) {
                    memcpy(tmp, &out_frame, sizeof(vmppFrame));
                    vmpp_queue_push_back(frame_queue, tmp);
                } else {
                    LOG_ERROR("[transcode] Fail to malloc frame.");
                    assert(0);
                }
                pthread_mutex_unlock(&frame_mutex);
                if (encoder_inited == 0 && current_enc_opts) {
                    check_enc_params(current_enc_opts, &out_frame);
                    ret = encoder_task_init(current_enc_opts);
                    if (ret != 0) {
                        LOG_ERROR("[transcode] fail to init encode thread %d", ret);
                        enc_error = 1;
                        //assert(0);
                    } else {
                        encoder_inited = 1;
                        ret = pthread_create(&enc_thread_handle, NULL, encode_thread, enc_ch);
                        if (ret != 0) {
                            LOG_ERROR("[transcode] fail to start encode thread %d", ret);
                            encoder_inited = 0;
                            //assert(0);
                            enc_error = 1;
                        }
                    }
                }
                if (enc_error)
                    break;
            }
            continue;
        } else if (ret == vmpp_RSLT_WARN_EOS) {
            if (out_count) {
                file_decode_time = gettime_ns() - file_start_time - file_read_time -
                                   file_write_time - check_md5_time;
                LOG(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN,
                    "[transcode] Dec Performance: %.3f fps for total %d frames; Detail: decode "
                    "%dus"
                    "(%dus/f), read %dus(%dus/f), write %dus(%dus/f), md5 %dus(%dus/f)",
                    ((float)out_count / ((float)file_decode_time / 1000000000.0)), out_count,
                    (int)(file_decode_time / 1000.0), (int)(file_decode_time / out_count / 1000.0),
                    (int)(file_read_time / 1000.0), (int)(file_read_time / out_count / 1000.0),
                    (int)(file_write_time / 1000.0), (int)(file_write_time / out_count / 1000.0),
                    (int)(check_md5_time / 1000.0), (int)(check_md5_time / out_count / 1000.0));
            }

            if (encoder_enable == 0) {
                if (options.check_md5) {
                    compute_md5sum(NULL, 0, cur_md5sum);
                    if (md5_saved && (memcmp(last_md5sum, cur_md5sum, sizeof(cur_md5sum)) != 0)) {
                        LOG_ERROR("[transcode] loop %d, md5 check <<<<FAILED>>>> :", current_loop);
                        print_md5(cur_md5sum, MD5_HASH_LEN);
                    } else {
                        LOG_INFO("[transcode] loop %d, md5 check <<<<PASS>>>> :", current_loop);
                        print_md5(cur_md5sum, MD5_HASH_LEN);
                    }
                    memcpy(last_md5sum, cur_md5sum, sizeof(cur_md5sum));
                    md5_saved = 1;
                }
            }
            break;
        } else if (ret == vmpp_RSLT_WARN_MORE_DATA) {
            // LOG_INFO("[transcode] receive more data");
            usleep(1000);
            //  break;
        } else {
            LOG_ERROR("[transcode] receive frame error %d", ret);
            break;
        }
    }
    if (options.save && output_file)
        fclose(output_file);

    if (frame_buffer)
        free(frame_buffer);

    return NULL;
}


#ifdef ENABLE_DYNAMIC_RES
#define DYNAMIC_RES_BS_COUNT 2
static const char *dynamic_res_bs_h264[DYNAMIC_RES_BS_COUNT] = {
		"/home/stone/workspace/akiyo_352x288_300_IBBBP.h264",
		"/home/stone/workspace/sintel_trailer_1920x1080p_1253_IBBBP.h264"};

static const char *dynamic_res_bs_hevc[DYNAMIC_RES_BS_COUNT] = {
		"/home/stone/workspace/sample_1920x1080.hevc",
		"/home/stone/workspace/sample_640x360.hevc"};
#endif

static int run_transcode(void)
{
    uint32_t stream_len = 0;
    uint8_t *stream_p = NULL;
    int loop;
    vmppDecChannelParameters ch_apr = {0};
    vmppStream input_stream;
    pthread_t thread_handle;
    vmppResult ret = vmpp_RSLT_OK;
    uint32_t in_count = 0;
    uint64_t pts_cnt = 0;
    md5_saved = 0;
#ifdef USING_FFMPEG
    ff_context_ptr ffctx = NULL;
#endif
    stream_context_ptr strmctx = NULL;
    int strmtype = BIT_STREAM_JPEG;
    file_start_time = 0;
    file_read_time = 0;
    file_write_time = 0;
    check_md5_time = 0;
    uint64_t read_start;
    int using_ffmpeg = 1;
    char *suffix = NULL;
    transcode_index++;
    current_loop = 1;
    int32_t tmp_ret = 0;
    int j = 0;
    void *tmp = NULL;

    if (encoder_enable) {
        if (vmpp_queue_init(&frame_queue) < 0) {
            LOG_ERROR("[transcode] Failed to init frame queue!");
            return -1;
        }
        if (vmpp_queue_init(&idle_frame_queue) < 0) {
            LOG_ERROR("[transcode] Failed to init idle frame queue!");
            return -1;
        }
        if (vmpp_queue_init(&releasing_frame_queue) < 0) {
            LOG_ERROR("[transcode] Failed to init releasing frame queue!");
            return -1;
        }
        current_enc_opts = &options.default_enc_opts;
        if (vmpp_queue_size(options.enc_options)) {
            int index = rand() % vmpp_queue_size(options.enc_options);
            current_enc_opts = vmpp_queue_peek(options.enc_options, index);
            LOG_INFO("[transcode] Random encode options index %d, codec %s", index,
                     current_enc_opts->enc_codec);
        }
    }

    suffix = strrchr(current_url, '.');

    // !!!FIXME, decoding bug exist reading jpeg using ffmpeg
    if (!suffix || 0 == strcmp(suffix, ".mjpeg")) {
        LOG_ERROR("[transcode] Currently not support decoding mjpeg");
        goto handle_error;
    }
    jpeg_stream = (0 == strcmp(suffix, ".jpeg") || /*0 == strcmp(suffix, ".mjpeg") ||*/
                   0 == strcmp(suffix, ".jpg"));
#ifdef USING_FFMPEG
    using_ffmpeg = jpeg_stream ? 0 : options.using_ffmpeg;
#else
    using_ffmpeg = 0;
#endif

    if (using_ffmpeg) {
#ifdef USING_FFMPEG
        ffctx = ff_open2(current_url);
        if (!ffctx) {
            LOG_ERROR("[transcode] Unable to open input file through ffmpeg <%s>", current_url);
            goto handle_error;
        }

        if (ff_video_codec(ffctx) == AV_CODEC_ID_H264)
            ch_apr.codecType = vmpp_CODEC_DEC_H264;
        else if (ff_video_codec(ffctx) == AV_CODEC_ID_HEVC)
            ch_apr.codecType = vmpp_CODEC_DEC_HEVC;
        else if (ff_video_codec(ffctx) == AV_CODEC_ID_AV1)
            ch_apr.codecType = vmpp_CODEC_DEC_AV1;
        else if (ff_video_codec(ffctx) == AV_CODEC_ID_MJPEG) {
            jpeg_stream = 1;
            ch_apr.codecType = vmpp_CODEC_DEC_JPEG;
        } else {
            LOG_ERROR("[transcode] Unsupported codec :%d", ff_video_codec(ffctx));
            goto handle_error;
        }
#endif
    } else {
        if (jpeg_stream || !strcmp(options.codec, "jpeg")) {
            ch_apr.codecType = vmpp_CODEC_DEC_JPEG;
            strmtype = BIT_STREAM_JPEG;
        } else if (!strcmp(options.codec, "hevc")) {
            ch_apr.codecType = vmpp_CODEC_DEC_HEVC;
            strmtype = BIT_STREAM_HEVC;
        } else if (!strcmp(options.codec, "h264")) {
            ch_apr.codecType = vmpp_CODEC_DEC_H264;
            strmtype = BIT_STREAM_H264;
        } else if (!strcmp(options.codec, "av1")) {
            ch_apr.codecType = vmpp_CODEC_DEC_AV1;
            strmtype = BIT_STREAM_AV1;
        } else if (!strcmp(options.codec, "vp9")) {
            ch_apr.codecType = vmpp_CODEC_DEC_VP9;
            strmtype = BIT_STREAM_VP9;
        } else if (!strcmp(options.codec, "avs2")) {
            ch_apr.codecType = vmpp_CODEC_DEC_AVS2;
            strmtype = BIT_STREAM_AVS2;
        } else {
            LOG_ERROR("[transcode] Unsupported codec :%s", options.codec);
            goto handle_error;
        }
        strmctx = stream_open(current_url, strmtype);
        if (!strmctx) {
            LOG_ERROR("[transcode] Unable to open input file <%s>", current_url);
            goto handle_error;
        }
        if (strmctx->type == BIT_STREAM_VP9) {
            ch_apr.codecType = vmpp_CODEC_DEC_VP9;
        }
    }

    vmppConfiguration cfg;
    memset(&cfg, 0, sizeof(vmppConfiguration));
    cfg.logCtx.enableCustomLog = 1;
    cfg.logCtx.logLevel = options.default_enc_opts.logLevel;

    ret = vmppInitDecoder(&cfg);
    if (ret != vmpp_RSLT_OK) {
        LOG_ERROR("[transcode] vmppInitDecoder failed %d", ret);
        goto handle_error;
    }

    ch_apr.decDevice = options.decDevice;
    ch_apr.memDevice = options.memDevice;

    uint32_t encoderDelayNum = 0;
    if (encoder_enable && !jpeg_stream) {
        uint32_t maxGopSize = current_enc_opts->gopSize;
        if (maxGopSize == VMPP_ENC_DEFAULT_PAR) {
            if (current_enc_opts->lookaheadDepth > 0) {
                maxGopSize = 8;
            } else {
                maxGopSize = 1;
            }
        } else if ((maxGopSize > 8 && maxGopSize != 16) || maxGopSize == 0) {
            maxGopSize = 8;
        }
        encoderDelayNum += maxGopSize; // for input reorder
        if (current_enc_opts->lookaheadDepth) {
            if (current_enc_opts->multicore) {
                encoderDelayNum += (current_enc_opts->lookaheadDepth + maxGopSize + 3); // 1pass delay, add 3 for 1pass multicore.
            } else {
                encoderDelayNum += MAX(maxGopSize, current_enc_opts->lookaheadDepth); // 1pass delay.
            }
        }
        encoderDelayNum += current_enc_opts->multicore ? 3 : 0; // add 3 for multicore
    }
    ch_apr.extraBufferNumber = encoderDelayNum + EXT_BUF_NUM;

#if 1 // these parameter are reserved.
    ch_apr.sourceMode = vmpp_SRC_FRAME;
    ch_apr.decodeMode = vmpp_DEC_NORMAL;
    ch_apr.maxWidth = 32768;
    ch_apr.maxHeight = 32768;
    ch_apr.streamBufferSize = MAX_STREAM_SIZE;
    ch_apr.pixelFormat = vmpp_PIX_FMT_NV12;
#endif

    if (options.bitDepth > 8) {
        ch_apr.pixelFormat = vmpp_PIX_FMT_YUV420_PLANAR_10BIT_P010;
    }

    ch_apr.enProfiling = 1;
    ch_apr.memoryMode = options.memory_mode;
    if (encoder_enable && ch_apr.memoryMode != vmpp_DEC_MEM_NORMAL) {
        LOG_WARN("memory mode [%d] is not recommended for transcoding, force to vmpp_DEC_MEM_NORMAL.", ch_apr.memoryMode);
        ch_apr.memoryMode = vmpp_DEC_MEM_NORMAL;
    }

    ret = vmppDecCreateChannel(&dec_ch, &ch_apr);
    if (ret != vmpp_RSLT_OK || !dec_ch) {
        fprintf(stderr, "create channel error %d or chn is null.\n", ret);
        goto handle_error;
    }

    ret = vmppDecStart(dec_ch); // set start status.
    if (ret < 0) {
        LOG_ERROR("[transcode] start recv stream error %d", ret);
        goto handle_error;
    }

    ret = pthread_create(&thread_handle, NULL, output_thread, dec_ch);
    if (ret != 0) {
        LOG_ERROR("[transcode] fail to start output thread %d", ret);
        vmppDecStop(dec_ch);
        vmppDecDestroyChannel(&dec_ch);
        goto handle_error;
    }

    file_start_time = gettime_ns();
    period_file_start_time = gettime_ns();
    loop = options.loop == 0 ? options.loop + 1 : options.loop;
#ifdef ENABLE_DYNAMIC_RES
    int tmp_idx = 0;
#endif
    do {
        if (current_loop > 1) {
            LOG_DEBUG("[transcode] seek to start.");
            if (using_ffmpeg) {
#ifdef USING_FFMPEG
                ff_seek_to_start(ffctx);
#endif
            } else {
                stream_seek_to_start(strmctx);
            } 
        }
        LOG_DEBUG("[transcode] >>>>> List Loop %d, File Loop %d, file <%s> on device <%s>!",
                  current_main_loop, current_loop, current_url, current_device);
        do {
            read_start = gettime_ns();
            if (using_ffmpeg) {
#ifdef USING_FFMPEG
                stream_len = ff_read_frame2(ffctx, &stream_p, &pts_cnt);
                if (!stream_len || !stream_p) {
#ifdef ENABLE_DYNAMIC_RES
                    ff_close2(&ffctx);
                    if (tmp_idx < DYNAMIC_RES_BS_COUNT) {
                        const char *path = ch_apr.codecType == vmpp_CODEC_DEC_HEVC
                                               ? dynamic_res_bs_hevc[tmp_idx]
                                               : dynamic_res_bs_h264[tmp_idx];
                        ffctx = ff_open2(path);
                        if (!ffctx) {
                            LOG_ERROR("[transcode] Unable to open input file through ffmpeg <%s>",
                                      path);
                            goto handle_error;
                        }
                    } else {
                        if (loop > 0) {
                            ffctx = ff_open2(current_url);
                            if (!ffctx) {
                                LOG_ERROR(
                                    "[transcode] Unable to open input file through ffmpeg <%s>",
                                    current_url);
                                goto handle_error;
                            }
                        }

                        break; // goto next loop
                    }

                    tmp_idx++;
                    stream_len = ff_read_frame2(ffctx, &stream_p, &pts_cnt);
#else
                    if (ff_eof(ffctx))
                        LOG_DEBUG("[transcode] stream end");
                    else
                        LOG_ERROR("[transcode] read frame error.");
                    break;
#endif  // ENABLE_DYNAMIC_RES
                }
#endif  // USING_FFMPEG
            } else {
                tmp_ret = stream_read_frame(strmctx, &stream_p);
                if (tmp_ret <= 0) {
                    if (tmp_ret == 0 && stream_eof(strmctx)) {
                        LOG_DEBUG("[transcode] stream end");
                        break;
                    }
                    LOG_ERROR("stream_read_frame failed, tmp_ret:%d",tmp_ret);
                    break;
                }
                stream_len = (uint32_t)tmp_ret;
                pts_cnt++;
            }

            file_read_time += (gettime_ns() - read_start);

            input_stream.stream = stream_p;
            input_stream.len = stream_len;
            input_stream.pts = pts_cnt;
            ret = vmppDecSendStream(dec_ch, &input_stream, 15000);
            if (ret < 0) {
                LOG_ERROR("[transcode] vmppDecSendStream (%d), error: %d", in_count, ret);
                if (vmpp_RSLT_ERR_INVALID_DATA == ret)
                    break;
            } else {
                in_count++;
                if (in_count % options.period == 0)
                    LOG_DEBUG("[transcode] vmppDecSendStream (%d) succeed", in_count);

                if (options.vframes > 0) {
                    if (in_count >= (uint32_t)options.vframes)
                        break;
                }
            }
            if (enc_error)
            {
                goto transcode_stop;
            }
        } while (1);

        current_loop++;
    } while (--loop);

transcode_stop:
    LOG_DEBUG("[transcode] ready to stop decoder");

    ret = vmppDecStop(dec_ch);
    if (ret < 0) {
        LOG_ERROR("[transcode] stop recv stream error %d", ret);
        goto handle_error;
    }

    LOG_DEBUG("[transcode] decoder stopped");

    pthread_join(thread_handle, NULL);
    if (encoder_enable && encoder_inited) {
        dec_finished = 1;
        pthread_join(enc_thread_handle, NULL);
    }
handle_error:
    encoder_inited = 0;
    dec_finished = 0;
    if (enc_ch) {
        ret = vmppEncDestroyChannel(&enc_ch);
        if (ret < 0)
            LOG_ERROR("[transcode] destroy enc chn error %d", ret);
    }

    if (dec_ch) {
        ret = vmppDecDestroyChannel(&dec_ch);
        if (ret < 0)
            LOG_ERROR("[transcode] destroy dec chn error %d", ret);
    }

    if (encoder_enable) {
        for (j = 0; j < vmpp_queue_size(idle_frame_queue); j++) {
            tmp = vmpp_queue_peek(idle_frame_queue, j);
            free(tmp);
        }
        vmpp_queue_free(&idle_frame_queue);

        for (j = 0; j < vmpp_queue_size(frame_queue); j++) {
            tmp = vmpp_queue_peek(frame_queue, j);
            free(tmp);
        }
        vmpp_queue_free(&frame_queue);

        for (j = 0; j < vmpp_queue_size(releasing_frame_queue); j++) {
            tmp = vmpp_queue_peek(releasing_frame_queue, j);
            free(tmp);
        }
        vmpp_queue_free(&releasing_frame_queue);
    }

    if (using_ffmpeg) {
#ifdef USING_FFMPEG
        ff_close2(&ffctx);
#endif
    }
    else
        stream_close(&strmctx);

    if (enc_output_file) {
        fclose(enc_output_file);
        enc_output_file = NULL;
    }

    return ret;
}

int main(int argc, char *argv[])
{
    int loop = 1;
    int ret = 0;
    int i, j;
    void *tmp;
#if 0
    char log_path[MAX_PATH_LEN] = {0};
    sprintf(log_path, "transcode-%d.txt", getpid());
    setLogFile(log_path);
#endif

    setLogLevel(LOG_LEVEL_INFO);

    transcode_index = 0;
    memset(&options, 0, sizeof(struct transcode_options));
#ifdef USING_FFMPEG
    options.using_ffmpeg = 1;
#else
    options.using_ffmpeg = 0;
#endif
    options.bitDepth = 8;

#ifdef INSERTIDR_TEST
    init_idr_params();
#endif

    options.encDevice = "/dev/hantroenc";
    options.decDevice = "/dev/hantrodec";
    options.memDevice = "/dev/memalloc";

    set_default_enc_params(&options.default_enc_opts);

    if (vmpp_queue_init(&options.urls) < 0) {
        LOG_ERROR("[transcode] Failed to init url queue!");
        return -1;
    }

    if (vmpp_queue_init(&options.enc_options) < 0) {
        LOG_ERROR("[transcode] Failed to init encoder options queue!");
        return -1;
    }

    if (parse_options(argc, argv) < 0)
        return -1;

    if (vmpp_queue_size(options.urls) == 0) {
        LOG_ERROR("[transcode] You have to provide input file url (or in a json file)!");
        usage(argv[0]);
        return -1;
    }

    LOG_INFO("[transcode] Render device for transcoding: %s %s %s", options.encDevice, options.decDevice, options.memDevice);

    LOG_INFO("[transcode] Total files %d to be decoded!", vmpp_queue_size(options.urls));

    md5ctx_inited = 0;
    memset(&md5ctx, 0, sizeof(struct md5_context));

    if ((options.enc_codec &&
         (strcmp(options.enc_codec, "h264") == 0 || strcmp(options.enc_codec, "hevc") == 0 ||
          strcmp(options.enc_codec, "av1") == 0 || strcmp(options.enc_codec, "jpeg") == 0)) ||
        vmpp_queue_size(options.enc_options)) {
        options.default_enc_opts.enc_codec = options.enc_codec;
        encoder_enable = 1;
    }

    if (encoder_enable)
        pthread_mutex_init(&frame_mutex, NULL);

    loop = options.main_loop == 0 ? options.main_loop + 1 : options.main_loop;
    current_main_loop = 0;
    do {
        current_main_loop++;
        LOG_INFO("[transcode] >>>>> List Loop %d starting!", current_main_loop);
        for (i = 0; i < vmpp_queue_size(options.urls); i++) {
            current_url = vmpp_queue_peek(options.urls, i);
            current_device = options.decDevice;
            LOG_INFO("[transcode] >>>>> List Loop %d, file %d <%s> on device <%s> starting!",
                     current_main_loop, i, current_url, current_device);
            ret = run_transcode();
            if (ret < 0)
                goto finish;
            LOG_INFO("[transcode] <<<<< List Loop %d, file %d <%s> on device <%s> finished!",
                     current_main_loop, i, current_url, current_device);
        }
        LOG_INFO("[transcode] <<<<< List Loop %d finished!", current_main_loop);
    } while (--loop);

finish:
    for (j = 0; j < vmpp_queue_size(options.urls); j++) {
        tmp = vmpp_queue_peek(options.urls, j);
        free(tmp);
    }
    vmpp_queue_free(&options.urls);

    for (j = 0; j < vmpp_queue_size(options.enc_options); j++) {
        tmp = vmpp_queue_peek(options.enc_options, j);
        if (((enc_options *)tmp)->pCom) {
            free(((enc_options *)tmp)->pCom);
        }
        free(tmp);
    }
    vmpp_queue_free(&options.enc_options);

    if (encoder_enable)
        pthread_mutex_destroy(&frame_mutex);

    if (!ret)
        LOG(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "[transcode] Goodbye!");
    else
        LOG(LOG_LEVEL_INFO, COLOR_LIGHT_RED, "[transcode] Oops!! We got an error:%d!", ret);

    closeLogFile();
    return ret;
}