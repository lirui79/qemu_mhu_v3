#include <assert.h>
#include <ctype.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#include "cJSON.h"
#include "ffmpeg-wrapper.h"
#include "stream.h"
#include "utils.h"

#include "defs.h"
#include "option.h"
#include "queue.h"
#ifdef __cplusplus
}
#endif /* __cplusplus */
#include "vmpp_enc_api.h"
#include "vmpp_enc_defs.h"

#define LIB_VACCRT_PATH "libvaccrt.so"

#define OUT_BUF_NUM 4
//#define RELEASE_THREAD_TEST
#define DYNAMIC_BITRATE_TEST
#define ENABLE_WRITE_CUINFO

// endof  the va_dec.h

//#define DEV_INPUT_TEST
#ifdef DEV_INPUT_TEST
typedef enum memcpy_direction {
    kHost2Ddr = 0,// memcpy from host to ddr
    kDdr2Host = 1 // memcpy from ddr to host
} memcpy_direction_t;
typedef rtError_t (*vaccrt_memcpy_t)(uint32_t dev_id, const void *addr_from, size_t size, void *addr_to,
                        memcpy_direction_t direction);
#endif

double psnr_total[6];
int psnr_num = 0;
int ssim_num = 0;
#define INSERTIDR_TEST
#ifdef  INSERTIDR_TEST
#define MAX_IDRBUF_LEN 500
static uint32_t idr_index_buf[MAX_IDRBUF_LEN];
static uint32_t idrbuf_len = 0;
static uint32_t idr_test_flag = 0;
#endif

static int firstFrame_Time_test = 0;
static int firstFrame_deltaTime = 0;

typedef struct _params_video_enc_t {
    char *input_file;
    char *output_file;
    int out_name_check;
    int width;
    int height;
    int stride;
    char *device;
    enum vmppPixelFormat pixel_format;
    char *pixel_fmt;
    int loop;
    int store;
    int buffer_count;
    int codec;
    int naltype_check_num;
    int core_mode;
    int vframes;
    char *idr_indices;
    int firstFrame_time_test;
    char *json_params;
} params_video_enc_t;

static struct option_t ops[] = {
    {(char*)"help", 'H', 2},
    {(char*)"input", 'i', 1},
    {(char*)"output", 'o', 1},
    {(char*)"width", 'w', 1},
    {(char*)"height", 'h', 1},
    {(char*)"stride", 't', 1}, /* Input image format */
    {(char*)"device", 'd', 1},
    {(char*)"pixelFormat", 'f', 1},
    {(char*)"loop", 'l', 1},
    {(char*)"save", 's', 1},
    {(char*)"codecFormat", 'c', 1},
    {(char*)"bufferCount", 'b', 1},
    {(char*)"vframes", 'n', 1},
    {(char*)"coreMode", 'C', 1},
#ifdef  INSERTIDR_TEST
    {(char*)"idr_index_file", 'I', 1},
#endif
    /* Only long option can be used for all the following parameters because
     * we have no more letters to use. All shortOpt=0 will be identified by
     * long option. */
    {(char*)"profile", '0', 1},
    {(char*)"level", '0', 1},
    {(char*)"frameRateNum", '0', 1},
    {(char*)"frameRateDen", '0', 1},
    {(char*)"bitDepthLuma", '0', 1},
    {(char*)"bitDepthChroma", '0', 1},
    {(char*)"gopSize", '0', 1},
    {(char*)"gdrDuration", '0', 1},
    {(char*)"lookaheadDepth", '0', 1},
    {(char*)"qualityMode", '0', 1},
    {(char*)"keyInt", '0', 1},
    {(char*)"crf", '0', 1},
    {(char*)"cqp", '0', 1},
    {(char*)"llRc", '0', 1},
    {(char*)"bitRate", '0', 1},
    {(char*)"initQp", '0', 1},
    {(char*)"vbvBufSize", '0', 1},
    {(char*)"vbvMaxRate", '0', 1},/* max bitrate for CPB VBR/CBR */
    {(char*)"intraQpDelta", '0', 1},
    {(char*)"qpMinI", '0', 1},
    {(char*)"qpMaxI", '0', 1},
    {(char*)"qpMinPB", '0', 1},
    {(char*)"qpMaxPB", '0', 1},
    {(char*)"vbr", '0', 1},
    {(char*)"tolCtbRcInter", '0', 1},
    {(char*)"aqStrength", '0', 1},
    {(char*)"tune", '0', 1},
    {(char*)"P2B", '0', 1},
    {(char*)"bBPyramid", '0', 1},
    {(char*)"maxFrameSizeMultiple", '0', 1},
    {(char*)"maxFrameSize", '0', 1},
    {(char*)"outbufNum",'0',1},
    {(char*)"roiType", '0', 1},
    {(char*)"roiInt", '0', 1},
    {(char*)"roiParam", '0', 1},
    {(char*)"extSEIInt", '0', 1},
    {(char*)"forceIDRInt", '0', 1},
    {(char*)"logLevel", '0', 1},
    {(char*)"roiMapDeltaQpBlockUnit", '0', 1},
    {(char*)"roiMapQpDeltaVersion", '0', 1},
    {(char*)"enableDynamicBitrate", '0', 1},
    {(char*)"enableDynamicFrameRate", '0', 1},
    {(char*)"maxBFrames", '0', 1},
    {(char*)"hrd", '0', 1},
    {(char*)"picSkip", '0', 1},
    {(char*)"vfr", '0', 1},
    {(char*)"svcTLayers", '0', 1},
    {(char*)"svcExtractMaxTLayer", '0', 1},
    {(char*)"sliceSize", '0', 1},
    {(char*)"enableDynamicCrf", '0', 1},
    {(char*)"psnr", '0', 1},
    {(char*)"ssim", '0', 1},
    {(char*)"ltrInterval", '0', 1},
    {(char*)"ltrQpDelta", '0', 1},    
    {(char*)"ltrRefGap", '0', 1},
    {(char*)"ltrInsertTest", '0', 1},
    {(char*)"rotation", '0', 1},
    {(char*)"coreID", '0', 1},
    {(char*)"openGop", '0', 1},
    {(char*)"smartEnc", '0', 1},
    {(char*)"enableSpsCropInfo", '0', 1},
    {(char*)"cropRect", '0', 1},
    {(char*)"enableDynamicKeyInt", '0', 1},
    {(char*)"disableMMCO", '0', 1},
    {(char*)"inLoopDSRatio", '0', 1},
    {(char*)"aqMode", '0', 1},
    {(char*)"psyFactor", '0', 1},
    {(char*)"rdoLevel", '0', 1},
    {(char*)"enableRdoQuant", '0', 1},
    {(char*)"qCompress", '0', 1},
    {(char*)"bitRateBalanceLevel", '0', 1},
    {(char*)"rcMode", '0', 1},
    {(char*)"enableOutputCuInfo", '0', 1},
    {NULL, 0, 0} /* Format of last line */
};

// static char *default_dev_str = (char *)"/dev/vastai_video0";
typedef int RET_TYPE;
typedef void *task_handle;
typedef void *(*task_func)(void *);
static FILE *output_file_handle = NULL;
static FILE *cuinfo_file_handle = NULL;
#define X_FRAMES 30
static uint64_t time_start = 0;
static uint64_t time_end = 0;
static uint64_t time_tick = 0;
static uint64_t time_total_frames = 0;
static uint64_t total_frames = 0;
uint64_t total_frames_send = 0;
static uint64_t total_bits = 0;
static uint64_t last_total_bits = 0;
uint64_t svcExtractMaxTLayerLast = 0;
uint64_t svcExtractMaxTLayerCur = 0;
static struct timeval fBegin, fEnd;
static int first_begin = 0;

// vmppStream stream[OUT_BUF_NUM];

typedef struct {
    int argc;
    char **argv;
} MainArgs;

// options struct
typedef struct {
    char *input_file;
    char *output_file;
    int width;
    int height;
    int stride;
    char *device;
    enum vmppPixelFormat pixel_format;
    char *pixel_fmt;
    int loop;
    int store;
    int buffer_count;
    int codec; // 0 for h264, 1 for hevc, 2 for av1
    int core_mode;
    int vframes;
    char *json_params;

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

    float crf;
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
    unsigned int rcMode;
    unsigned int enableOutputCuInfo;
} enc_options;

typedef struct {
    void *enc_ch;
    enc_options *options;
} thread_param_t;

static int get_channels_load(vmppEncChannelParameters *ch_apr)
{
    int quality = 100;
    int normalization = 38281846;
    int weight = ch_apr->videoConfig.width * ch_apr->videoConfig.height * (ch_apr->videoConfig.frameRate.numerator
                 / (double)ch_apr->videoConfig.frameRate.denominator) * quality / normalization;
    return weight;
}

int compareFiles(std::string fname1, std::string fname2)
{
    FILE *fp1 = fopen((char *)fname1.c_str(), "r");
    FILE *fp2 = fopen((char *)fname2.c_str(), "r");
  
    if (!fp1 || !fp2) {
        printf("Cannot open %s for reading\n", !fp1 ? fname1.c_str() : fname2.c_str());
        if (fp1) fclose(fp1);
        if (fp2) fclose(fp2);
        return -1;
    }

    int ch1, ch2;
    while ((ch1 = getc(fp1)) == (ch2 = getc(fp2)) && ch1 != EOF && ch2 != EOF);

    int ret = (ch1 == ch2)? 0 : 1;
    printf("Files %s: %s vs %s.\n", ret ? "diff" : "same", fname1.c_str(), fname2.c_str());

    fclose(fp1);
    fclose(fp2);
    return ret;
}

static void parse_params_json(const char *file, enc_options *options, cJSON **proot)
{
    FILE *fp_json_file = NULL;
    char json_buf[102400];
    cJSON *root, *arrayItem, *item, *object;

    fp_json_file = fopen(file, "r");
    if (fp_json_file == NULL) {
        fprintf(stderr, "Open source json file %s error.\n", file);
        return;
    }

    uint32_t ret = fread(json_buf, 1, sizeof(json_buf), fp_json_file);
    (void)ret;
    root = cJSON_Parse(json_buf);
    if (!root) {
        fprintf(stderr, "Error before: [%s]\n", cJSON_GetErrorPtr());
        fclose(fp_json_file);
        return;
    }

    arrayItem = cJSON_GetObjectItem(root, "params");
    if (arrayItem != NULL) {
        int32_t size = cJSON_GetArraySize(arrayItem);
        for (int32_t index = 0; index < size; index++) {
            // source_t *a_src = (source_t *)malloc(sizeof(source_t));
            if (!options) {
                fprintf(stderr, "malloc options failed!\n");
                // free(a_src);
                return;
            }
            object = cJSON_GetArrayItem(arrayItem, index);
            item = cJSON_GetObjectItem(object, "profile");
            if (item != NULL) {
                options->profile = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "level");
            if (item != NULL) {
                options->level = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "frameRateNum");
            if (item != NULL) {
                options->frameRateNum = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "frameRateDen");
            if (item != NULL) {
                options->frameRateDen = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "bitDepthLuma");
            if (item != NULL) {
                options->bitDepthLuma = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "bitDepthChroma");
            if (item != NULL) {
                options->bitDepthChroma = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "gopSize");
            if (item != NULL) {
                options->gopSize = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "gdrDuration");
            if (item != NULL) {
                options->gdrDuration = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "lookaheadDepth");
            if (item != NULL) {
                options->lookaheadDepth = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "qualityMode");
            if (item != NULL) {
                options->qualityMode = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "tune");
            if (item != NULL) {
                options->tune = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "keyInt");
            if (item != NULL) {
                options->keyInt = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "crf");
            if (item != NULL) {
                options->crf = item->valuedouble;
            }
            item = cJSON_GetObjectItem(object, "cqp");
            if (item != NULL) {
                options->cqp = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "llRc");
            if (item != NULL) {
                options->llRc = item->valueint;
            }item = cJSON_GetObjectItem(object, "bitRate");
            if (item != NULL) {
                options->bitRate = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "initQp");
            if (item != NULL) {
                options->initQp = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "vbvBufSize");
            if (item != NULL) {
                options->vbvBufSize = item->valueint * 1000;
            }
            item = cJSON_GetObjectItem(object, "vbvMaxRate");
            if (item != NULL) {
                options->vbvMaxRate = item->valueint * 1000;
            }
            item = cJSON_GetObjectItem(object, "intraQpDelta");
            if (item != NULL) {
                options->intraQpDelta = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "qpMinI");
            if (item != NULL) {
                options->qpMinI = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "qpMaxI");
            if (item != NULL) {
                options->qpMaxI = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "qpMinPB");
            if (item != NULL) {
                options->qpMinPB = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "qpMaxPB");
            if (item != NULL) {
                options->qpMaxPB = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "vbr");
            if (item != NULL) {
                options->vbr = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "aqStrength");
            if (item != NULL) {
                options->aqStrength = item->valuedouble;
            }
            item = cJSON_GetObjectItem(object, "P2B");
            if (item != NULL) {
                options->P2B = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "bBPyramid");
            if (item != NULL) {
                options->bBPyramid = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "maxFrameSizeMultiple");
            if (item != NULL) {
                options->maxFrameSizeMultiple = item->valuedouble;
            }
            item = cJSON_GetObjectItem(object, "maxFrameSize");
            if (item != NULL) {
                options->maxFrameSize = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "outbufNum");
            if (item != NULL) {
                options->outbufNum = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "roiType");
            if (item != NULL) {
                options->roiType = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "roiInt");
            if (item != NULL) {
                options->roiInt = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "roiParam");
            if (item != NULL) {
                options->roiParam = item->valuestring;
            }
            item = cJSON_GetObjectItem(object, "extSEIInt");
            if (item != NULL) {
                options->extSEIInt = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "forceIDRInt");
            if (item != NULL) {
                options->forceIDRInt = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "logLevel");
            if (item != NULL) {
                options->logLevel = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "roiMapDeltaQpBlockUnit");
            if (item != NULL) {
                options->roiMapDeltaQpBlockUnit = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "roiMapQpDeltaVersion");
            if (item != NULL) {
                options->roiMapQpDeltaVersion = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "enableDynamicBitrate");
            if (item != NULL) {
                options->enableDynamicBitrate = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "enableDynamicFrameRate");
            if (item != NULL) {
                options->enableDynamicFrameRate = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "maxBFrames");
            if (item != NULL) {
                options->maxBFrames = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "hrd");
            if (item != NULL) {
                options->hrd = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "picSkip");
            if (item != NULL) {
                options->pictureSkip = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "vfr");
            if (item != NULL) {
                options->vfr = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "svcTLayers");
            if (item != NULL) {
                options->svcTLayers = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "svcExtractMaxTLayer");
            if (item != NULL) {
                options->svcExtractMaxTLayer = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "sliceSize");
            if (item != NULL) {
                options->sliceSize = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "enableDynamicCrf");
            if (item != NULL) {
                options->enableDynamicCrf = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "psnr");
            if (item != NULL) {
                options->enableCalcPSNR = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "ssim");
            if (item != NULL) {
                options->enableCalcSSIM = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "ltrInterval");
            if (item != NULL) {
                options->ltrInterval = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "ltrQpDelta");
            if (item != NULL) {
                options->ltrQpDelta = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "ltrRefGap");
            if (item != NULL) {
                options->ltrRefGap = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "ltrInsertTest");
            if (item != NULL) {
                options->ltrInsertTest = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "rotation");
            if (item != NULL) {
                options->rotation = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "coreID");
            if (item != NULL) {
                options->coreID = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "openGop");
            if (item != NULL) {
                options->openGop = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "smartEnc");
            if (item != NULL) {
                options->smartEnc = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "enableSpsCropInfo");
            if (item != NULL) {
                options->enableSpsCropInfo = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "cropRect");
            if (item != NULL) {
                options->cropRect = item->valuestring;
            }
            item = cJSON_GetObjectItem(object, "enableDynamicKeyInt");
            if (item != NULL) {
                options->enableDynamicKeyInt = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "disableMMCO");
            if (item != NULL) {
                options->disableMMCO = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "inLoopDSRatio");
            if (item != NULL) {
                options->inLoopDSRatio = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "aqMode");
            if (item != NULL) {
                options->aqMode = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "psyFactor");
            if (item != NULL) {
                options->psyFactor = item->valuedouble;
            }
            item = cJSON_GetObjectItem(object, "rdoLevel");
            if (item != NULL) {
                options->rdoLevel = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "enableRdoQuant");
            if (item != NULL) {
                options->enableRdoQuant = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "qCompress");
            if (item != NULL) {
                options->qCompress = item->valuedouble;
            }
            item = cJSON_GetObjectItem(object, "bitRateBalanceLevel");
            if (item != NULL) {
                options->bitRateBalanceLevel = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "rcMode");
            if (item != NULL) {
                options->rcMode = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "enableOutputCuInfo");
            if (item != NULL) {
                options->enableOutputCuInfo = item->valueint;
            }
            // vmpp_queue_push_back(queue, a_src);
        }
    }
    fclose(fp_json_file);
    *proot = root;
}

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
    printf("  -d                 device name\n");
    printf("  -f                 pixel format\n");
    printf("  -l                 loop time, if loop = 0, it will loop forever\n");
    printf("  -b                 yuv frames buffer count, a reading thread will be start if > 0\n");
    printf("  -C                 core mode, default 0, 0-auto, 1-single core, 2-multicore\n");
    printf("  -s                 stored in output file or not, 0/1\n");
    printf("  -c                 codec format: 0 for h264, 1 for hevc, 2 for av1, default is 0\n");
    printf("  -n                 yuv frame numbers to be encoded\n");
#ifdef INSERTIDR_TEST
    printf("  -I                 insert IDR frame index\n");
#endif
    printf("  -j                 json for params of encode\n");
    // printf("  --profile          main profile or main still picture profile\n");
    // printf("  --level            main profile level\n");
    // printf("  --frameRateNum     frame rate numerator\n");
    // printf("  --frameRateDen     frame rate denominator\n");
    // printf("  --bitDepthLuma     luma bit depth\n");
    // printf("  --bitDepthChroma   chroma bit depth\n");
    // printf("  --gopSize          gop size\n");
    // printf("  --gdrDuration      gdr duration\n");
    // printf("  --lookaheadDepth   lookahead depth\n");
    // printf("  --qualityMode      quality mode\n");
    // printf("  --tune             tune\n");
    // printf("  --keyInt           IDR interval\n");
    // printf("  --crf              CRF constant\n");
    // printf("  --cqp              cqp\n");
    // printf("  --llRc             llRc\n");
    // printf("  --bitRate          bps, bit rate,\n");
    // printf("  --cqp              cqp\n");
    // printf("  --initQp           init qp\n");
    // printf("  --vbvBufSize       kb, vbv Buffer size\n");
    // printf("  --vbvMaxRate       kbps, vbv max rate\n");
    // printf("  --intraQpDelta     intra qp delta\n");
    // printf("  --qpMinI           min qp value of I frame\n");
    // printf("  --qpMaxI           max qp value of I frame\n");
    // printf("  --qpMinPB          min qp value of B/P frame\n");
    // printf("  --qpMaxPB          max qp value of B/P frame\n");
    // printf("  --aqStrength       aq strength\n");
    // printf("  --P2B              P2B\n");
    // printf("  --bBPyramid        bBPyramid\n");
    // printf("  --maxFrameSizeMultiple        maxFrameSizeMultiple\n");
    // printf("  --maxFrameSize     max frame size\n");
    // printf("  --outbufNum        max output buf number\n");
    // printf("  --roiType           ROI type, 0 for none, 1 for roi range, 2 for roi map\n");
    // printf("  --roiInt           ROI interval\n");
    // printf("  --roiParam           ROI param\n");
    // printf("  --extSEIInt        extSEI interval\n");
    // printf("  --forceIDRInt      forceIDR interval\n");
    // printf("  --logLevel         log level for SDK, default: 2(INFO),  1-DEBUG 2-INFO 3-WARN 4-ERROR\n");
    // printf("  --roiMapDeltaQpBlockUnit      roi map delta qp block unit\n");
    // printf("  --roiMapQpDeltaVersion        roi map delta qp version\n");
    // printf("  --enableDynamicBitrate        enable dynamic bitrate or not(1/0)\n");
    // printf("  --enableDynamicFrameRate      enable dynamic framerate or not(1/0)\n");
    // printf("  --maxBFrames       max B frames control for adaptive GOP decision\n");
    // printf("  --hrd              Hypothetical Reference Decoder model\n");
    // printf("  --picSkip          Frame All Skip Mode When Overflow\n");
    // printf("  --vfr              variable frame rate\n");
    // printf("  --svcTLayers              Temporal Layers for Scalable Video Coding\n");
    // printf("  --svcExtractMaxTLayer     Max Temporal Layer to Extract for Scalable Video Coding\n");
    // printf("  --sliceSize        Slice size in CTB/MB rows for multislice\n");
    // printf("  --enableDynamicCrf        enable dynamic crf or not(1/0)\n");
    // printf("  --psnr        enable caculation of PSNR or not, default: 0\n");
    // printf("  --ssim        enable caculation of SSIM or not, default: 0\n");
    // printf("  --ltrInterval        enable LTR, default: 0\n");
    // printf("  --ltrQpDelta        set LTR frame QpDelta, default: 0\n");
    // printf("  --ltrRefGap        set frame gap that references the LTR, default: 0\n");
    // printf("  --ltrInsertTest    Enable test insert LTR: 0\n");
    // printf("  --rotation    Rotate input image, 0-Disabled (Default), 1-90 degrees right, 2-90 degrees left, 3-180 degrees right\n");
    // printf("  --coreID           Enable specify core id: 0-3\n");
    // printf("  --openGop           openGop\n");
    // printf("  --smartEnc          smartEnc Mode\n")
    // printf("  --enableSpsCropInfo     enable sps crop rect info\n");
    // printf("  --cropRect           sps crop rect info\n");
    // printf("  --enableDynamicKeyInt        enable dynamic keyInt or not(1/0)\n");
    // printf("  --disableMMCO      disable h264 memory management control operation or not(1/0)\n");
    // printf("  --inLoopDSRatio    in-loop downsample ratio for first pass(1/0)\n");
    // printf("  --aqMode           aq mode:0-3\n");
    // printf("  --psyFactor        weight of psycho-visual encoding:0.0-4.0\n");
    // printf("  --rdoLevel         RDO Level can balance the quality and throughput:1-3\n");
    // printf("  --enableRdoQuant   enable RDO quantization or not(1/0)\n");
    // printf("  --qCompress        qCompress sets the quantizer curve compression factor:0.0-1.0\n");
    // printf("  --bitRateBalanceLevel        set the matching degree between the encoding output bitrate and the target bitrate in simple or static scenarios:0-4\n");
    // printf("  --rcMode           rc mode\n");
    // printf("  --enableOutputCuInfo          enable output cu info.\n");
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
    if (params == NULL) {
        return;
    }

    char params_buffer[128];
    strncpy(params_buffer, params, sizeof(params_buffer) - 1);
    params_buffer[sizeof(params_buffer) - 1] = '\0';
    printf("============params: %s\n", params_buffer);
    char *temp = strtok(params_buffer, ":");
    int i = 0;
    idr_test_flag = 1;
    while (temp && i < MAX_IDRBUF_LEN) {
        idr_index_buf[i] = atoi(temp);
        temp = strtok(NULL,":");
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

static int parse_options(int argc, char **argv, enc_options *options)
{
    struct parameter prm;
    int ret;
    char *optarg;
    prm.cnt = 1;
#ifdef INSERTIDR_TEST
    init_idr_params();
#endif
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
//         case 'i':
//             options->input_file = optarg;
//             break;
//         case 'o':
//             options->output_file = optarg;
//             break;
//         case 'w':
//             options->width = atoi(optarg);
//             break;
//         case 'h':
//             options->height = atoi(optarg);
//             break;
//         case 't':
//             options->stride = atoi(optarg);
//             break;
//         case 'd':
//             options->device = optarg;
//             break;
//         case 'f':
//             options->pixel_fmt = optarg; // options->pixel_format = get_pixel_format(optarg);
//             break;
//         case 'l':
//             options->loop = atoi(optarg);
//             break;
//         case 's':
//             options->store = atoi(optarg);
//             break;
//         case 'b':
//             options->buffer_count = atoi(optarg);
//             break;
//         case 'c':
//             options->codec = atoi(optarg);
//             break;
//         case 'C':
//             options->core_mode = atoi(optarg);
//             break;
        // case 'n':
        //     options->vframes = atoi(optarg);
        //     break;
// #ifdef INSERTIDR_TEST
//         case 'I':
//             split_idr_params(optarg);
//             break;
// #endif
//         case 'j':
//             options->json_params = optarg;
//             break;
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
                options->crf = atof(optarg);
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
            if (strcmp(prm.longOpt, "rcMode") == 0)
                options->rcMode = atoi(optarg);
            break;
        default:
            usage(argv[0]);
            return -1;
        }
    }
    return 0;
}

#if 0
static void timer_trigger(int force_reset)
{
    static int timer_started = 0;
    static struct timeval tBegin, tEnd;
    static long time_used[10];
    static long decode_count = 0;
    long avg_time = 0;
    long deltaTime = 0;

    if (timer_started == 0 || force_reset) {
        gettimeofday(&tBegin, NULL);
        timer_started = 1;
    } else {
        gettimeofday(&tEnd, NULL);
        deltaTime = 1000000L * (tEnd.tv_sec - tBegin.tv_sec) + (tEnd.tv_usec - tBegin.tv_usec);
        time_used[decode_count % 10] = deltaTime;
        decode_count++;

        if (decode_count >= 10) {
            for (int i = 0; i < 10; i++) {
                avg_time += time_used[i];
            }
            avg_time = avg_time / 10;
        }
        LOG_INFO("curr: %ld us, avg: %ld us", deltaTime, avg_time);
        LOG_INFO("curr: %.2f fps, avg: %.2f fps", 1. / deltaTime * 1000000,
                 1. / avg_time * 1000000);
        fflush(stdout);
        // gettimeofday(&tBegin, NULL);
        tBegin = tEnd;
    }
}
#endif

#ifdef RELEASE_THREAD_TEST
static task_handle run_task(task_func func, void *param)
{
    int ret;
    pthread_attr_t attr;
    struct sched_param par;
    pthread_t *thread_handle = (pthread_t *)malloc(sizeof(pthread_t));

#ifndef ARM64
    pthread_attr_init(&attr);
    ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    assert(ret == 0);
    // ret = pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    par.sched_priority = 60;
    ret = pthread_attr_setschedparam(&attr, &par);

    ret = pthread_create(thread_handle, &attr, func, param);
#else
    ret = pthread_create(thread_handle, NULL, func, param);
#endif

    assert(ret == 0);

    if (ret != 0) {
        free(thread_handle);
        thread_handle = NULL;
    }

    return thread_handle;
}
#endif

void writeCuInfo(vmppChannel enc_ch, vmppStream *out_stream)
{

    if (!out_stream)
        return;
    vmppResult ret = vmpp_RSLT_OK;
    vmppEncOutData *encOutData = &out_stream->encOutData;
    uint32_t iCu = 0;
    vmppEncCuInfo* curCuInfo = NULL;
    char *cuType[] = {(char*)"INTER", (char*)"INTRA", (char*)"IPCM"};
    char *interDir[] = {(char*)"PRED_L0",(char*)"PRED_L1",(char*)"PRED_BI"};

    if (cuinfo_file_handle == NULL) {
        cuinfo_file_handle = fopen("cuInfo.txt", "w");
        if (!cuinfo_file_handle) {
            LOG_ERROR("open cuinfo file error.");
            return;
        }
    }

    fprintf(cuinfo_file_handle, "\n#Pic frameNum %ld, frameType %d, frameAvgQP %d.\n", total_frames, encOutData->frameType, encOutData->frameAvgQP);

    vmppEncOutInfo outInfo = {0};
    outInfo.cuInfo = (vmppEncCuInfo *) malloc(encOutData->maxCuNum * sizeof(vmppEncCuInfo));
    ret = vmppEncParseCuInformation(enc_ch, encOutData, &outInfo);
    if (ret) {
        LOG_INFO("vmppEncParseCuInformation failed: err %d", ret);
        goto end;
    }

    for (iCu = 0; iCu < outInfo.totalCuNum; iCu++) {
        curCuInfo = &outInfo.cuInfo[iCu];
        fprintf(cuinfo_file_handle, " iCu %d %2dx%-2d at (%2d,%2d) %s",
                    iCu, curCuInfo->cuSize, curCuInfo->cuSize, curCuInfo->cuLocationX, curCuInfo->cuLocationY, cuType[curCuInfo->cuMode]);

        if (curCuInfo->cuMode == 1) {
            fprintf(cuinfo_file_handle, " costIntraSatd[%d] costInterSatd[%d] \n", curCuInfo->costIntraSatd, curCuInfo->costInterSatd);
        } else if (curCuInfo->cuMode == 0) {
            fprintf(cuinfo_file_handle, " costIntraSatd[%d] costInterSatd[%d] interDir[%-7s] ", curCuInfo->costIntraSatd, curCuInfo->costInterSatd, interDir[curCuInfo->interPredIdc]);
            if (curCuInfo->interPredIdc != 1)
             fprintf(cuinfo_file_handle, " mv0.refIdx[%d] mv0.mvX[%d] mv0.mvY[%d]",curCuInfo->mv[0].refIdx,curCuInfo->mv[0].mvX,curCuInfo->mv[0].mvY);
            if (curCuInfo->interPredIdc != 0)
             fprintf(cuinfo_file_handle, " mv1.refIdx[%d] mv1.mvX[%d] mv1.mvY[%d]",curCuInfo->mv[1].refIdx,curCuInfo->mv[1].mvX,curCuInfo->mv[1].mvY);
            fprintf(cuinfo_file_handle, "\n");
        }
    }
    fprintf(cuinfo_file_handle, " totalCuNum=%d frameSatd=%d intraMBcount=%d interMBCount=%d avgMVX=%d avgMVY=%d\n",
        outInfo.totalCuNum, outInfo.frameSatd, outInfo.intraMBCount, outInfo.interMBCount, outInfo.averageMVX, outInfo.averageMVY);
end:
    if (outInfo.cuInfo)
        free(outInfo.cuInfo);
}

static void handle_output(vmppChannel enc_ch, enc_options *options, vmppStream *out_stream,
                          vmppResult result)
{
    int ret = 0;
    if (output_file_handle == NULL && options->store) {
        output_file_handle = fopen(options->output_file, "wb");
        if (!output_file_handle) {
            LOG_ERROR("open output file %s error.", options->output_file);
            return;
        } else
            LOG_INFO("[APP][%p]open enc file to write: %s", enc_ch, options->output_file);
    }

    do {
        if (result == vmpp_RSLT_OK) {
            if (options->svcExtractMaxTLayer != VMPP_ENC_DEFAULT_PAR &&
                out_stream->svcTemporalId > options->svcExtractMaxTLayer)
                break;
            if (options->svcTLayers && options->svcExtractMaxTLayer != VMPP_ENC_DEFAULT_PAR && options->svcExtractMaxTLayer >= options->svcTLayers)
            {
                if (out_stream->pts == 0)
                {
                    svcExtractMaxTLayerCur = svcExtractMaxTLayerLast = options->svcTLayers - 1;
                }
                svcExtractMaxTLayerCur = (out_stream->pts + 1) % (8 + rand() % 8) == 0 ? rand() % options->svcTLayers : svcExtractMaxTLayerLast;

                if (svcExtractMaxTLayerCur != svcExtractMaxTLayerLast) {
                    if (svcExtractMaxTLayerLast > svcExtractMaxTLayerCur || out_stream->svcTemporalId <= svcExtractMaxTLayerLast)
                        svcExtractMaxTLayerLast = svcExtractMaxTLayerCur;
                    svcExtractMaxTLayerCur = svcExtractMaxTLayerLast;
                }

                if (out_stream->svcTemporalId > svcExtractMaxTLayerCur)
                    break;
            }
            total_bits += out_stream->len;
            if (options->store && output_file_handle) {
                /* Store encode data into output file */
                ret = fwrite(out_stream->stream, 1, out_stream->len, output_file_handle);
                if (ret <= 0) {
                    LOG_ERROR("[enc][%p] write output file %s error.", enc_ch,
                              options->output_file);
                }
            }

#ifdef ENABLE_WRITE_CUINFO
            if (options->enableOutputCuInfo) {
                writeCuInfo(enc_ch, out_stream);
            }
#endif
        } else if (result == vmpp_RSLT_ENC_INPUT_INSERTED) {
            LOG_INFO("frame inserted.");
        } else {
            LOG_ERROR("error: %d.", result);
        }
    } while (0);

    return;
}

typedef rtError_t (*vaccrt_init_t)(uint32_t dev_id);

#ifdef RELEASE_THREAD_TEST
typedef struct {
    void *enc_ch;
    vmppStream *stream;
} late_release_parmam;

static void *late_release_stream_thread(void *arg)
{
    late_release_parmam *param = arg;
    vmppResult enc_ret;

    // wait for one second
    usleep(1000000);

    enc_ret = vmppEncReleaseStream(param->enc_ch, param->stream);
    if (enc_ret < 0) {
        LOG_ERROR("release frame error %d.", enc_ret);
    } else {
        LOG_INFO("release frame done! %p", param->stream);
    }

    memset(param->stream, 0, sizeof(vmppStream));

    free(param->stream);
    free(param);

    return NULL;
}
#endif

void default_params(enc_options *option)
{
    if (!option) {
        fprintf(stderr, "set param error, ch_apr is null.\n");
    }

    // set default values
    option->input_file = (char *)"/home/vastai/resource/dataset/yuv/1080P/xg1920x1088_x265_121_num.yuv";
    option->width = 1920;
    option->height = 1088;
    option->stride = 0;
    option->device = NULL;
    option->pixel_format = vmpp_PIX_FMT_NV12;
    option->loop = 0;
    option->store = 1;
    option->codec = 0;
    option->core_mode = 0;
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
    option->roiParam = (char *)"top=0,left=0,bottom=200,right=200,qpType=0,qpValue=1";
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
    option->cropRect = (char *)"xOffset=0,cropWidth=0,yOffset=0,cropHeight=0";
    option->enableDynamicKeyInt = 0;
    option->disableMMCO = 0;
    option->inLoopDSRatio = VMPP_ENC_DEFAULT_PAR;
    option->aqMode = VMPP_ENC_DEFAULT_PAR;
    option->psyFactor = VMPP_ENC_DEFAULT_PAR;
    option->rdoLevel = VMPP_ENC_DEFAULT_PAR;
    option->enableRdoQuant = VMPP_ENC_DEFAULT_PAR;
    option->qCompress = VMPP_ENC_DEFAULT_PAR;
    option->bitRateBalanceLevel = 0;
    option->rcMode = vmpp_ENC_RC_DEFAULT;
    option->enableOutputCuInfo = 0;
}

void set_params(enc_options *option, vmppEncChannelParameters *ch_apr)
{
    if (!option) {
        fprintf(stderr, "set param error, option is null.\n");
    }
    if (!ch_apr) {
        fprintf(stderr, "set param error, ch_apr is null.\n");
    }

    ch_apr->coreMode = (vmppCoreMode)option->core_mode;
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
        ch_apr->videoConfig.level = (vmppVideoLevel)0;
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
    if (option->crf == VMPP_ENC_DEFAULT_PAR) {
        ch_apr->videoConfig.crf = VMPP_ENC_DEFAULT_PAR;
    } else {
        ch_apr->videoConfig.crf = ((int16_t)floor(option->crf));//The lower 16 bits represent the integer part of crf.
        int crf_frac_int = (((int32_t)(roundf((option->crf - ch_apr->videoConfig.crf) * 10))) & 0xFFFF) << 16;
        ch_apr->videoConfig.crf |= crf_frac_int;//The upper 16 bits represent the fractional part of crf.
    }
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
    ch_apr->videoConfig.roiType = (vmppEncROIType)option->roiType;
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
    ch_apr->videoConfig.preProcess.rotation = (vmppEncPictureRotation)option->rotation;
    ch_apr->videoConfig.vbr = (option->coreID + (VMPP_SPECIFY_COREID << 16));
    ch_apr->videoConfig.openGop = option->openGop;
    ch_apr->videoConfig.smartEnc = option->smartEnc;
    ch_apr->videoConfig.disableMMCO = option->disableMMCO;
    ch_apr->videoConfig.inLoopDSRatio = option->inLoopDSRatio;
    ch_apr->videoConfig.aqMode = (vmppEncAQMode)option->aqMode;
    ch_apr->videoConfig.psyFactor = option->psyFactor;
    ch_apr->videoConfig.rdoLevel = option->rdoLevel;
    ch_apr->videoConfig.enableRdoQuant = option->enableRdoQuant;
    ch_apr->videoConfig.qCompress = option->qCompress;
    ch_apr->videoConfig.bitRateBalanceLevel = option->bitRateBalanceLevel;
    ch_apr->videoConfig.rcMode = (vmppEncRcMode)option->rcMode;
    ch_apr->videoConfig.enableOutputCuInfo = option->enableOutputCuInfo;
}

struct read_params {
    struct raw_context *p_raw_ctx;
    int loop;
    int eof;
    int exit;
    int frame_buffer_count;
    struct vmpp_queue *frame_queue;
    struct vmpp_queue *idle_frame_queue;
    pthread_mutex_t frame_mutex;
};

static void *reading_thread(void *arg)
{
    struct read_params *rparams = (struct read_params *)arg;
    if (!rparams) {
        LOG_ERROR("Invalid parameters");
        return NULL;
    }
    int idle_frame_count = 0;
    int64_t read_frame_count = 0;
    int ret = 0;
    vmppFrame *frame = NULL;
    int loop_count = 0;

    while (!rparams->exit) {
        pthread_mutex_lock(&rparams->frame_mutex);
        idle_frame_count = vmpp_queue_size(rparams->idle_frame_queue);
        pthread_mutex_unlock(&rparams->frame_mutex);
        if (!idle_frame_count) {
            sched_yield();
            continue;
        }

        pthread_mutex_lock(&rparams->frame_mutex);
        frame = (vmppFrame *)vmpp_queue_pop_front(rparams->idle_frame_queue);
        pthread_mutex_unlock(&rparams->frame_mutex);
        if (!frame) {
            LOG_WARN("Incorrect frame");
            assert(0);
            continue;
        }

        ret = raw_read_frame(rparams->p_raw_ctx, frame);
        if (ret <= 0) {
            pthread_mutex_lock(&rparams->frame_mutex);
            vmpp_queue_push_back(rparams->idle_frame_queue, frame);
            pthread_mutex_unlock(&rparams->frame_mutex);
            if (raw_eof(rparams->p_raw_ctx)) {
                raw_seek_to_start(rparams->p_raw_ctx);
                loop_count++;
            }

            if (loop_count >= rparams->loop) {
                rparams->eof = 1;
                break;
            } else {
                LOG_INFO("Loop Over: %d", loop_count);
                continue;
            }
        }
        read_frame_count++;
        frame->pts = read_frame_count;

        pthread_mutex_lock(&rparams->frame_mutex);
        vmpp_queue_push_back(rparams->frame_queue, frame);
        pthread_mutex_unlock(&rparams->frame_mutex);
    }
    return NULL;
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
        // LOG_DEBUG("MSE %4.2f %4.2f %4.2f\n", pStream->psnrInfo[0], pStream->psnrInfo[1], pStream->psnrInfo[2]);
        // LOG_DEBUG("psnr_total %4.2f %4.2f %4.2f\n", psnr_total[0], psnr_total[1], psnr_total[2]);

        psnr_num++;
    }
}

void update_ssim(vmppStream *pStream) {
    if (pStream->psnrInfo[3] && pStream->psnrInfo[4] && pStream->psnrInfo[5]) {
        // ssim[3] = (ssim[0] * 4 + ssim[1] + ssim[2]) / 6;

        psnr_total[3] += pStream->psnrInfo[3];
        psnr_total[4] += pStream->psnrInfo[4];
        psnr_total[5] += pStream->psnrInfo[5];

        // LOG_DEBUG("SSIM %4.2f %4.2f %4.2f\n", pStream->psnrInfo[3], pStream->psnrInfo[4], pStream->psnrInfo[5]);
        // LOG_DEBUG("ssim_total %4.2f %4.2f %4.2f\n", psnr_total[3], psnr_total[4], psnr_total[5]);

        ssim_num++;
    }
}

static void print_enc_options(const params_video_enc_t *opts) {
    if (opts == NULL) {
        printf("Options pointer is NULL.\n");
        return;
    }
    printf("input_file: %s\n", opts->input_file ? opts->input_file : "(null)");
    printf("output_file: %s\n", opts->output_file ? opts->output_file : "(null)");
    printf("out_name_check: %d\n", opts->out_name_check);
    printf("width: %d\n", opts->width);
    printf("height: %d\n", opts->height);
    printf("stride: %d\n", opts->stride);
    printf("device: %s\n", opts->device ? opts->device : "(null)");
    printf("pixel_format: %d\n", opts->pixel_format); // 打印枚举的整数值
    printf("pixel_fmt: %s\n", opts->pixel_fmt ? opts->pixel_fmt : "(null)");
    printf("loop: %d\n", opts->loop);
    printf("store: %d\n", opts->store);
    printf("buffer_count: %d\n", opts->buffer_count);
    printf("codec: %d\n", opts->codec);
    printf("naltype_check_num: %d\n", opts->naltype_check_num);
    printf("core_mode: %d\n", opts->core_mode);
    printf("vframes: %d\n", opts->vframes);
    printf("idr_indices: %s\n", opts->idr_indices ? opts->idr_indices : "(null)");
    printf("firstFrame_time_test: %d\n", opts->firstFrame_time_test);
    printf("json_params: %s\n", opts->json_params ? opts->json_params : "(null)");
}

extern char *params[110];
extern int params_num;

int video_enc(params_video_enc_t *params_ut)
{
    int ret_end = 0;
    int32_t ret = -1;
    int enc_fd = -1, i = 0;
    uint64_t time4Xframes = 0;
    float timePerframe = 0, rate = 0;
    int dieId = 0;
    cJSON *root = NULL;
    vaccrt_init_t vaccrt_init;
    rtError_t vaccRet = 0;
    vmppConfiguration cfg;
    int chns = 0;
    vmppStream *pStream = nullptr;

    vmppChannel enc_ch;
    vmppEncChannelParameters ch_apr;
    vmppRuntimeInstance rt_inst;
    struct raw_context raw_ctx;
    int pic_size = 0, comp1_size = 0, comp2_size = 0, comp3_size = 0;
    vmppResult enc_ret = vmpp_RSLT_OK;
    int loop_count = 0;
    struct read_params rparams = {0};
    pthread_t rthread_handle;
    int top = 0, left = 0, right = 0, bottom = 0;
    int qpType = 0, qpValue = 0;
    first_begin = 0;
    total_frames_send = 0;

    /* For Nal Type */
    int naltype[2][8] = { {0, 1, 7, 9, 9, 9, 9, 9}, {2, 0, 1, 7, 9, 8, 8, 8}}; // gopSize 1, h264:0, 1, 7, 9, 9, 9; hevc:2, 0, 1, 7, 9, 8(P2B enabled by default)
    int naltype_no = 0;

    /* For roi map */
    int roimap_value[3] = {30, 10, -15};
    int roimap_index = 0;
    uint32_t blksize = 0;
    uint32_t roiwidth, roiheight, roimap_size;
    int8_t *roi_map_delta_qp_buffer = NULL;
    char out_file[128];

    setLogLevel(LOG_LEVEL_INFO);

    vmppFrame frame;
    memset(&frame, 0, sizeof(frame));
    vmppFrame *pFrame = NULL, *tmp = NULL;
    char idleDie[64];
    vmppStream stream;
    vmppEncExtendedParams extParams = {0};
    extern int loop_process_enc;
    uint32_t seiCount = 1;
    vmppSEI *sei = (vmppSEI *)malloc(sizeof(vmppSEI));
    if (sei) {
        sei->nalType = vmpp_SEI_PREFIX;
        sei->payloadType = SEI_USER_DATA_UNREGISTERED;
        sei->payloadData = (uint8_t *)"0123456789ABCDEF-01234567890123456789012345678901234567890";
        sei->payloadDataSize = strlen((char *)sei->payloadData);
    }

#ifdef RELEASE_THREAD_TEST
    task_handle release_task = NULL;
#endif

    /* set default params */
    enc_options *options = (enc_options *)malloc(sizeof(enc_options));
    memset(options, 0, sizeof(enc_options));
    default_params(options);

    if (params_ut->input_file != NULL) options->input_file = params_ut->input_file;
    std::string output_name;
    if (params_ut->out_name_check == 0)
        output_name = ((std::string)params_ut->input_file).substr(((std::string)params_ut->input_file).find_last_of("/"));
    else output_name = ((std::string)params_ut->output_file).substr(((std::string)params_ut->output_file).find_last_of("/"));
    std::string tm = ((std::string)params_ut->output_file).substr(0, ((std::string)params_ut->output_file).find_last_of("/")) + output_name.substr(0, output_name.find_last_of("."));
    memcpy(out_file, tm.c_str() , tm.length());
    memcpy(out_file + tm.length(), params_ut->codec == 0 ? ".h264\0" : params_ut->codec == 1 ? ".hevc\0" : ".ivf\0", params_ut->codec == 2 ? 5 : 6);
    options->output_file = out_file;
    options->width = params_ut->width;
    options->height = params_ut->height;
    options->stride = params_ut->stride;
    options->device = params_ut->device;
    options->pixel_format = params_ut->pixel_format = get_pixel_format(params_ut->pixel_fmt);
    options->loop = params_ut->loop;
    options->store = params_ut->store;
    options->buffer_count = params_ut->buffer_count;
    options->codec = params_ut->codec;
    options->core_mode = params_ut->core_mode;
    options->vframes = params_ut->vframes;
    split_idr_params((char *)params_ut->idr_indices);
    firstFrame_Time_test = params_ut->firstFrame_time_test;
    if (params_ut->json_params != NULL) {
        LOG_INFO("json_params string %s.", params_ut->json_params);
        parse_params_json(params_ut->json_params, options, &root);
    } 
    else {
        for (int i=0; i<params_num; i++)
            printf("%s ", params[i]);
        printf("\n");
        ret = parse_options(params_num, params, options);
        if (ret < 0) {
            free(options);
            free(sei);
            return 0;
        }
    }

    if (options->stride == 0) {
        options->stride = options->width;
    }
    print_enc_options(params_ut);
    printf("options->output_file: %s\n", options->output_file);

    if (options->width >= 7680 && options->codec == 2) options->codec = 0; // todo: wait for AV1 8k dec MULTI_CORE_MODE (single core actually) support
    if (options->output_file == NULL) {
        if (options->codec == 0) {
            options->output_file = (char *)"enc_out.h264";
        } else if (options->codec == 1) {
            options->output_file = (char *)"enc_out.h265";
        } else if (options->codec == 2) {
            options->output_file = (char *)"enc_out.ivf";
            
        }
    }
    

    /* set input params */
    memset(&ch_apr, 0, sizeof(ch_apr));
    set_params(options, &ch_apr);

    /* open input yuv */
    ret = raw_open(options->input_file, options->pixel_format, options->width, options->height,
                   options->stride, &raw_ctx);
    if (ret < 0) {
        LOG_ERROR("Failed to open input file %s", options->input_file);
        ret_end = 1;
        goto end;
    }

    pic_size = raw_pic_size(&raw_ctx, &comp1_size, &comp2_size, &comp3_size);

    if (options->buffer_count) {
        if (vmpp_queue_init(&rparams.frame_queue) < 0) {
            LOG_ERROR("[transcode] Failed to init frame_queue!");
            ret_end = 1;
            goto end;
        }
        if (vmpp_queue_init(&rparams.idle_frame_queue) < 0) {
            LOG_ERROR("[transcode] Failed to init idle_frame_queue!");
            ret_end = 1;
            goto end;
        }
        rparams.loop = options->loop;
        rparams.eof = 0;
        rparams.exit = 0;
        rparams.p_raw_ctx = &raw_ctx;
        rparams.frame_buffer_count = options->buffer_count;
        pthread_mutex_init(&rparams.frame_mutex, NULL);

        for (i = 0; i < options->buffer_count; i++) {
            tmp = (vmppFrame *)malloc(sizeof(vmppFrame));
            if (!tmp) {
                LOG_ERROR("[transcode] Failed to alloc frame!");
                ret_end = 1;
                goto end;
            }
            memset(tmp, 0, sizeof(vmppFrame));
            tmp->data[0] = (uint8_t *)malloc(pic_size);
            if (!tmp->data[0]) {
                LOG_ERROR("Failed to malloc buffer for frame, size %d", pic_size);
                ret_end = 1;
                goto end;
            }
            tmp->data[1] = tmp->data[0] + comp1_size;
            if (comp3_size)
                tmp->data[2] = tmp->data[1] + comp2_size;
            vmpp_queue_push_back(rparams.idle_frame_queue, tmp);
        }

        ret = pthread_create(&rthread_handle, NULL, reading_thread, &rparams);
        if (ret != 0) {
            LOG_ERROR("[transcode] fail to start reading thread %d", ret);
            ret_end = 1;
            goto end;
        }
    } else {
        memset(&frame, 0, sizeof(frame));
        frame.data[0] = (uint8_t *)malloc(pic_size);
        if (!frame.data[0]) {
            LOG_ERROR("Failed to malloc buffer for yuv frame, size %d", pic_size);
            ret_end = 1;
            goto end;
        }
        frame.data[1] = frame.data[0] + comp1_size;
        if (comp3_size)
            frame.data[2] = frame.data[1] + comp2_size;

        frame.seiData = (vmppSEI **)malloc(sizeof(vmppSEI *) * seiCount);
        frame.seiData[0] = sei;
    }

    /* init encoder */
    open_runtime(&rt_inst); // run time init

    if(loop_process_enc == 1) {
        if (!options->device) {
            ret = vmppEncQueryIdleDie(VMPP_PRE_OCCUPY_LOAD, idleDie, 0);
            if (ret != vmpp_RSLT_OK) {
                LOG_INFO("vmppEncQueryIdleDie failed %d, using die 0\n", ret);
                options->device = (char *)"/dev/vastai_video0";
            }
            else
                options->device = idleDie;
        } else {
            ret = vmppEncQueryIdleDie(VMPP_PRE_OCCUPY_LOAD, options->device, 1);
            if (ret < 0) {
                LOG_INFO("vmppEncQueryIdleDie failed %d\n", ret);
            }
        }
    }

    enc_fd = open(options->device, O_RDWR);
    if (enc_fd < 0) {
        LOG_INFO("Cannot open DRM render node for device %s", options->device);
        ret_end = 1;
        goto end;
    }
    ch_apr.device = (vmppDevice)enc_fd;

    dieId = 0;
    vaccrt_init = (vaccrt_init_t)(&rt_inst)->init;
    sscanf(options->device, "/dev/vastai_video%d", &dieId);
    vaccRet = vaccrt_init(dieId);
    if (vaccRet) {
        LOG_INFO("vaccrt_init failed: err %d", vaccRet);
        ret_end = 1;
        goto end;
    }

    #ifdef DEV_INPUT_TEST
    vaccrt_malloc_video_t vaccrt_malloc_video = (&rt_inst)->mallocVideo;
    vmppDevAddr input_addr = (vmppDevAddr)NULL;
    vaccRet =  vaccrt_malloc_video(dieId, 4096, 1920*1088*4, &input_addr);
    if (vaccRet) {
        LOG_INFO("vaccrt_malloc_video failed: err %d", vaccRet);
        goto end;
    }
    vaccrt_memcpy_t vaccrt_memcpy = dlsym(rt_inst.runtimeHandle, "vaccrt_memcpy");
	char* error = dlerror();
	if (error != NULL) {
		LOG_ERROR("failed to dlsym vaccrt_memcpy, err: %s", error);
		goto end;
	}
#endif

    
    memset(&cfg, 0, sizeof(vmppConfiguration));
    cfg.runtimeInst = rt_inst;
    if (options->logLevel) {
        cfg.logCtx.enableCustomLog = 1;
        cfg.logCtx.logLevel = (vmppLogLevel)options->logLevel;
    }

    ret = vmppInitEncoder(&cfg);
    if (ret != vmpp_RSLT_OK) {
        LOG_INFO("vmppInitEncoder failed %d", ret);
        ret_end = 1;
        goto end;
    }

    ch_apr.enProfiling = 1;

    vmppEncVideoCapability caps;
    vmppEncGetVideoCaps(ch_apr.codecType, &caps);

    memset(psnr_total, 0, sizeof(psnr_total));
    psnr_num = 0;
    ssim_num = 0;

    /* create channel */
    ret = vmppEncCreateChannel(&enc_ch, &ch_apr);
    if (ret != vmpp_RSLT_OK) {
        fprintf(stderr, "create channel error %d.\n", ret);
        ret_end = 1;
        goto end;
    }

    memset(&stream, 0, sizeof(vmppStream));
    pStream = &stream;
    time_start = time_tick = gettime_ns();
    do {
#ifdef RELEASE_THREAD_TEST
        pStream = (vmppStream *)malloc(sizeof(vmppStream));
#endif
        memset(pStream, 0, sizeof(vmppStream));
        if (options->buffer_count) {
            pFrame = NULL;
            pthread_mutex_lock(&rparams.frame_mutex);
            pFrame = (vmppFrame *)vmpp_queue_pop_front(rparams.frame_queue);
            pthread_mutex_unlock(&rparams.frame_mutex);
            if (!pFrame) {
                if (rparams.eof)
                    break;
                sched_yield();
                continue;
            }
        } else {
            ret = raw_read_frame(&raw_ctx, &frame);
            if (ret <= 0) {
                if (raw_eof(&raw_ctx)) {
                    raw_seek_to_start(&raw_ctx);
                    loop_count++;
                }

                if (loop_count >= options->loop) {
                    break;
                } else {
                    LOG_INFO("[APP][%p]Loop Over: %d", enc_ch, loop_count);
                    continue;
                }
            }
            frame.pts = total_frames;
        #ifdef DEV_INPUT_TEST
            vaccrt_memcpy(dieId, frame.data[0], frame.dataSize, (void*)input_addr, kHost2Ddr);
            frame.busAddress[0] = input_addr;
            frame.memoryType = vmpp_MEM_DEVICE;
        #endif

            if (options->roiInt) {
                sscanf(options->roiParam, "top=%d,left=%d,bottom=%d,right=%d,qpType=%d,qpValue=%d",
                       &top, &left, &bottom, &right, &qpType, &qpValue);
                // printf("top=%d,left=%d,bottom=%d,right=%d,qpType=%d,qpValue=%d\n", top, left,
                // right, bottom, qpType, qpValue);
            }

            if (options->enableSpsCropInfo) {
                frame.cropInfo.flag = vmpp_CROP_SPS_INFO;
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
                    extParams.roi[i].qpType = (vmppEncQPType)qpType;
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
            pFrame = &frame;
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
#ifdef INSERTIDR_TEST
        if (idr_test_flag && compare_idr_index(total_frames_send)) {
            extParams.forceIDR = 1;
        }
#endif

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

        if (total_frames_send && (options->enableDynamicCrf == 1) && total_frames_send % 100 == 0) {
            extParams.updateCrf = 10 + total_frames_send / 100;
            extParams.updateVbvBufSize = 0;
            extParams.updateVbvMaxRate = 0;
            extParams.updateTypeMask |= VMPP_ENC_UPDATE_CRF;
        }

        /*if ((total_frames_send / 10) % 2 == 0) {
            extParams.updateQpSetting.updateInitQp = total_frames_send % 51;
            extParams.updateQpSetting.updateQpMinI = VMPP_ENC_DEFAULT_PAR;
            extParams.updateQpSetting.updateQpMaxI = VMPP_ENC_DEFAULT_PAR;
            extParams.updateQpSetting.updateQpMinPB = VMPP_ENC_DEFAULT_PAR;
            extParams.updateQpSetting.updateQpMaxPB = VMPP_ENC_DEFAULT_PAR;
            extParams.updateTypeMask |= VMPP_ENC_UPDATE_QP;
        } else {
            extParams.updateQpSetting.updateInitQp = VMPP_ENC_DEFAULT_PAR;
            extParams.updateQpSetting.updateQpMinI = VMPP_ENC_DEFAULT_PAR;
            extParams.updateQpSetting.updateQpMaxI = VMPP_ENC_DEFAULT_PAR;
            extParams.updateQpSetting.updateQpMinPB = VMPP_ENC_DEFAULT_PAR;
            extParams.updateQpSetting.updateQpMaxPB = VMPP_ENC_DEFAULT_PAR;
            extParams.updateTypeMask |= VMPP_ENC_UPDATE_QP;
        }*/


        pFrame->timebase.denominator = 0;
        pFrame->timebase.numerator = 0;
        if (options->vfr) {
            if (total_frames_send >= 198) {
                pFrame->timebase.denominator = 1;
                pFrame->timebase.numerator = 48;
            } else if (total_frames_send >= 98) {
                pFrame->timebase.denominator = 1;
                pFrame->timebase.numerator = 24;
            } else {
                pFrame->timebase.denominator = 1;
                pFrame->timebase.numerator = 48;
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

        enc_ret = vmppEncEncodeFrame(enc_ch, pFrame, &extParams, pStream, 4000);
        if (firstFrame_Time_test && first_begin == 0) {
            first_begin = 1;
            gettimeofday(&fBegin, NULL);
        }
        LOG_DEBUG("[APP][%p]vmppEncEncodeFrame %lld#, Done: %d, stream size: %d, stream.nalCnt %d.", enc_ch,
                  (u64)total_frames, enc_ret, pStream->len, pStream->encedNals.cnt);
        for (uint32_t i = 0; i < pStream->encedNals.cnt; i++) {
            LOG_DEBUG("stream.encedNals[%d] %d.", i, pStream->encedNals.nals[i]);
            if (options->codec != 2 && naltype_no < params_ut->naltype_check_num) {
                if (options->codec == 0) {
                    assert(naltype[0][naltype_no++] == pStream->encedNals.nals[i]);
                } else if (options->codec == 1) {
                    assert(naltype[1][naltype_no++] == pStream->encedNals.nals[i]);
                } else {
                    LOG_INFO("codec %d error. naltype[0][%d]: %d, naltype[1][%d]: %d", options->codec, naltype_no, naltype[0][naltype_no], naltype_no, naltype[1][naltype_no]);
                    ret_end = 1;
                    goto end;
                }
            }
        }

        if (options->buffer_count) {
            pthread_mutex_lock(&rparams.frame_mutex);
            vmpp_queue_push_back(rparams.idle_frame_queue, pFrame);
            pthread_mutex_unlock(&rparams.frame_mutex);
        }

        handle_output((void **)enc_ch, options, pStream, enc_ret);
        if (enc_ret == vmpp_RSLT_OK) {
            if (firstFrame_Time_test && first_begin == 1) {
                gettimeofday(&fEnd, NULL);
                first_begin = 2;
                firstFrame_deltaTime = 1000000L * (fEnd.tv_sec - fBegin.tv_sec) + (fEnd.tv_usec - fBegin.tv_usec);
                LOG_WARN("[APP][%p][Performance] loop %d, firstFrame deltaTime %d us", enc_ch, params_ut->loop,
                        firstFrame_deltaTime);
                if( firstFrame_deltaTime >= firstFrame_Time_test * 1000 )
                    exit(-1);
            }
            if (options->enableCalcPSNR)
                update_psnr(pStream, options);
            if (options->enableCalcSSIM)
                update_ssim(pStream);
            total_frames++;

            // if (firstFrame_Time_test)
            //     if (total_frames >= 1) break;
        }
        if (enc_ret == vmpp_RSLT_OK || enc_ret == vmpp_RSLT_ENC_INPUT_INSERTED)
            total_frames_send++;

if (enc_ret == vmpp_RSLT_OK) {
        #ifdef RELEASE_THREAD_TEST
            LOG_INFO("to release stream %p", pStream);
            late_release_parmam *release_param =
                (late_release_parmam *)malloc(sizeof(late_release_parmam));
            release_param->enc_ch = enc_ch;
            release_param->stream = pStream;
            release_task = run_task(late_release_stream_thread, release_param);
        #else
            enc_ret = vmppEncReleaseStream(enc_ch, pStream);
            if (enc_ret < 0) {
                LOG_ERROR("release stream error %d.", enc_ret);
                goto end;
            }
        #endif
        }
        if (total_frames && total_frames % X_FRAMES == 0) {
            time4Xframes = gettime_ns() - time_tick;
            timePerframe = (float)time4Xframes / (float)X_FRAMES;
            rate = 0;
            if (options->frameRateDen) {
                rate = (total_bits - last_total_bits) * 8 / 1024.0 / X_FRAMES *
                       options->frameRateNum / options->frameRateDen;
                last_total_bits = total_bits;
            }
            LOG_INFO("[APP][%p] %d frames: %.1f fps (%.1f us/frame), %-7.1fkbps",
                     enc_ch, X_FRAMES, 1000000000.0f / timePerframe, timePerframe / 1000.0f, rate);
            time_tick = gettime_ns();
        }
        if (options->vframes > 0 && total_frames_send >= (uint64_t)options->vframes) {
            break;
        }
    } while (1);

    // flush
    do {
        memset(pStream, 0, sizeof(vmppStream));
        frame.memoryType = vmpp_MEM_FLUSH;
        enc_ret = vmppEncEncodeFrame(enc_ch, &frame, NULL, pStream, 4000);
        if (enc_ret == vmpp_RSLT_WARN_EOS) {
            LOG_INFO("[APP][%p]vmpp_RSLT_WARN_EOS: total output %lld", enc_ch, (u64)total_frames);
            break;
        }

        if (enc_ret == vmpp_RSLT_ERR_ENC_DRIVER_MISMATCH) {
            LOG_ERROR("[APP][%p] Abort! Hash Len Mismatch!", enc_ch);
            break;
        }

        if (pStream->inputBusAddress) {
            LOG_INFO("[APP][%p] Do flush job for remained frame", enc_ch);
            // !!! Flush frame from decoder with memory type == vmpp_MEM_DEVICE
        }

        if (enc_ret == vmpp_RSLT_ENC_FLUSH)
            continue;

        LOG_DEBUG("[APP][%p]vmppEncodeFrame, Done: %d, stream size: %d", enc_ch, enc_ret,
                  pStream->len);
        handle_output((void **)enc_ch, options, pStream, enc_ret);
        if (enc_ret == vmpp_RSLT_OK) {
            if (options->enableCalcPSNR)
                update_psnr(pStream, options);
            if (options->enableCalcSSIM)
                update_ssim(pStream);
            total_frames++;
            vmppEncReleaseStream(enc_ch, pStream);
        }
    } while (enc_ret == vmpp_RSLT_OK || enc_ret == vmpp_RSLT_ENC_FLUSH || enc_ret == vmpp_RSLT_ENC_INPUT_INSERTED);

#ifdef RELEASE_THREAD_TEST
    if (release_task != NULL) {
        pthread_join(*((pthread_t *)release_task), NULL);
    }
#endif
    time_end = gettime_ns();
    time_total_frames = time_end - time_start;

    if (total_frames) {
        timePerframe = (float)time_total_frames / (float)total_frames;
        rate = 0;
        if (options->frameRateDen) {
            rate = total_bits * 8 / 1024.0 / total_frames * options->frameRateNum /
                   options->frameRateDen;
        }

        if (options->enableCalcPSNR && psnr_num) {
            LOG_WARN("[APP][%p] Average PSNR[%d]: Y %4.2f, U %4.2f, V %4.2f",
                    enc_ch,
                    psnr_num,
                    psnr_total[0] / psnr_num,
                    psnr_total[1] / psnr_num,
                    psnr_total[2] / psnr_num);
        }

        if (options->enableCalcSSIM && ssim_num) {
            LOG_WARN("[APP][%p] Average SSIM[%d]: Y %4.2f, U %4.2f, V %4.2f",
                enc_ch,
                ssim_num,
                psnr_total[3] / ssim_num,
                psnr_total[4] / ssim_num,
                psnr_total[5] / ssim_num);
        }

        LOG_WARN("[APP][%p] Total %lld frames: %.1f fps (%.1f us/frame), %-7.1fkbps",
                 enc_ch, (u64)total_frames, 1000000000.0f / timePerframe, timePerframe / 1000.0f, rate);
        // if(firstFrame_Time_test && firstFrame_deltaTime >= timePerframe / 1000.0f )
        //     exit(-1);
        total_bits = 0;
        total_frames = 0;
        last_total_bits = 0;
    }

    enc_ret = vmppEncDestroyChannel(&enc_ch);
    if (enc_ret < 0) {
        LOG_ERROR("destroy chn error %d.", enc_ret);
        ret_end = 1;
        goto end;
    }

end:
    if (roi_map_delta_qp_buffer)
        free(roi_map_delta_qp_buffer);
    if (options->buffer_count) {
        rparams.exit = 1;
        pthread_join(rthread_handle, NULL);
        for (i = 0; i < vmpp_queue_size(rparams.frame_queue); i++) {
            pFrame = (vmppFrame *)vmpp_queue_pop_front(rparams.frame_queue);
            if (pFrame) {
                if (pFrame->data[0])
                    free(pFrame->data[0]);
                free(pFrame);
            }
        }

        vmpp_queue_free(&rparams.frame_queue);

        for (i = 0; i < vmpp_queue_size(rparams.idle_frame_queue); i++) {
            pFrame = (vmppFrame *)vmpp_queue_pop_front(rparams.idle_frame_queue);
            if (pFrame) {
                if (pFrame->data[0])
                    free(pFrame->data[0]);
                free(pFrame);
            }
        }

        vmpp_queue_free(&rparams.idle_frame_queue);

        pthread_mutex_destroy(&rparams.frame_mutex);
    }

    raw_close(&raw_ctx);

    if (frame.data[0])
        free(frame.data[0]);

    if (options) {
        free(options);
        options = NULL;
    }

    if (output_file_handle) {
        fclose(output_file_handle);
        output_file_handle = NULL;
    }

    if (cuinfo_file_handle) {
        fclose(cuinfo_file_handle);
        cuinfo_file_handle = NULL;
    }

    if (sei) {
        free(sei);
        sei = NULL;
    }

    if (frame.seiData) {
        free(frame.seiData);
        frame.seiData = NULL;
    }

    if (enc_fd != -1)
        close(enc_fd);
    
    close_runtime(&rt_inst);
    if (root) {
        cJSON_Delete(root);
    }
    if (ret_end == 1)
        return -1;
    return 0;
}

// int main(int argc, char *argv[])
// {
//     int32_t ret = -1;
//     /* set default params */
//     enc_options *options_main = malloc(sizeof(enc_options));
//     // default_params(options_main);

//     ret = parse_options(argc, argv, options_main);
//     if (ret < 0) {
//         return 0;
//     }
    
//     return video_enc(options_main->input_file, options_main->output_file, options_main->width, options_main->height, options_main->stride, options_main->device, options_main->pixel_fmt, options_main->loop, options_main->store, options_main->buffer_count, options_main->codec, options_main->json_params);
//     // MainArgs args = {argc, argv};
//     // return MainTask(&args);
// }

