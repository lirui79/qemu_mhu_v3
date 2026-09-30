/*
 * Copyright (c) 2022, Vastai Tech. All rights reserved
 *
 * The information contained herein is confidential
 * property of Company. The user, copying, transfer or
 * disclosure of such information is prohibited except
 * by express written agreement with VASTAITECH.
 */

#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#ifdef __linux__
#include <dirent.h>
#include <dlfcn.h>
#include <sys/ioctl.h>
#include <unistd.h>
#else
#define S_ISDIR(m) (((m) & 0170000) == (0040000))
#define S_ISREG(m) ((m & _S_IFMT) == _S_IFREG)
#define lstat      _stat
#endif    // __linux__
#include "cJSON.h"
#include "defs.h"
#ifdef USING_FFMPEG
#include "ffmpeg-wrapper.h"
#endif
#include "md5.h"
#include "option.h"
#include "queue.h"
#include "stream.h"
#include "utils.h"
#include "vmpp_dec_api.h"
#include "vmpp_enc_api.h"

#define DEFAULT_OUT_BUF_NUM     (4)
#define MIN(x, y)               ((x) < (y)) ? ((x)) : ((y))
#define MAX(x, y)               ((x) < (y)) ? ((y)) : ((x))
#define EXT_BUF_NUM             (2)
#define MAX_QUEUED_FRAME        (3)
#define MAX_QUEUED_STREAM       (2)
#define DMABUF_IOCTL_ALLOC      (0)
#define MAX_USRBUF_CTX          (3)
#define NEXT_MULTIPLE(value, n) (((value) + (n) - 1) & ~((n) - 1))
#define NANOSEC_PER_SEC         (1000000000ULL)
#define NANOSEC_PER_MICROSEC    (1000)
#define NANOSEC_PER_MILLISEC    (1000000)
#define DEFAULT_TIMEOUT_MS      (4000)
#define USER_DEFINED_QTABLE     (101)


static const char *cuType[] = { "INTER", "INTRA", "IPCM" };
static const char *interDir[] = { "PRED_L0", "PRED_L1", "PRED_BI" };
/* clang-format off */
static const uint8_t qTable[64] = { 2, 2, 2, 1, 1, 1, 1, 1,
                                    1, 1, 1, 1, 1, 1, 1, 1,
                                    1, 1, 1, 1, 1, 1, 1, 1,
                                    1, 1, 1, 1, 1, 1, 1, 1,
                                    1, 1, 1, 1, 1, 1, 1, 1,
                                    1, 1, 1, 1, 1, 1, 1, 1,
                                    1, 1, 1, 1, 1, 1, 1, 1,
                                    1, 1, 1, 1, 1, 1, 1, 1 };
/* clang-format on */

typedef struct _params_log {
    int log_into_file;
    int log_level;
    int log_level_sdk;
    int log_level_ffmpeg;
    int log_callback;
    int enable_error_assert;
} params_log_t;

typedef struct _params_transcode_mt {
    char *input;
    char *device_name;
    char *enc_codec;
    char *output_directory;
    char *output_file;
    int loop;
    int main_loop;
    int save;
    int check_md5;
    char *codec;
    int using_ffmpeg;
    int log_period;
    int perf_period;
    int vframes;
    int bitDepth;
    int memory_mode;
    int thread_count;
    int dec_output_align;
    int width;
    int height;
    params_log_t params_log;
    int argc;
    char **argv;
} params_transcode_mt_t;

static struct option_t ops[] = {
    {                  (char*)"input", 'i', 1 },
    {                 (char*)"output", 'o', 1 },
    {           (char*)"renderDevice", 'd', 1 },
    {           (char*)"renderDevice", 'r', 1 },
    {                   (char*)"save", 's', 1 },
    {            (char*)"loopForFile", 'l', 1 },
    {             (char*)"loopInJson", 'L', 1 },
    {                    (char*)"md5", 'm', 1 },
    {                 (char*)"ffmpeg", 'f', 1 },
    {               (char*)"codeType", 'c', 1 },
    {         (char*)"encodeCodeType", 'C', 1 },
    {                  (char*)"debug", 'p', 1 },
    {             (char*)"performace", 'P', 1 },
    {            (char*)"frameNumber", 'n', 1 },
    {                   (char*)"help", 'h', 1 },
    {            (char*)"decBitDepth", 'b', 1 },
    {             (char*)"userOutBuf", 'u', 0 },
    {             (char*)"memoryMode", 'M', 1 },
    {            (char*)"threadCount", 't', 1 },
    {         (char*)"decOutputAlign", 'a', 1 },
    // {                  (char*)"width", 'W', 1 },
    // {                 (char*)"height", 'H', 1 },

    /* Only long option can be used for all the following parameters because
     * we have no more letters to use. All shortOpt=0 will be identified by
     * long option. */
    /* Options for decoder */
    {                (char*)"decMode", '0', 1 },
    {             (char*)"decApiMode", '0', 1 },
    {         (char*)"decOutputAlign", '0', 1 },
    {          (char*)"decMemoryMode", '0', 1 },
    {                (char*)"decCrop", '0', 1 },
    {            (char*)"decCropInfo", '0', 1 },
    {  (char*)"decNoOutputReordering", '0', 1 },
    {      (char*)"decRecvMemoryType", '0', 1 },
    {            (char*)"decCoreMode", '0', 1 },

    /* Options for encoder */
    {                  (char*)"width", '0', 1 },
    {                 (char*)"height", '0', 1 },
    {            (char*)"pixelFormat", '0', 1 },
    {                (char*)"profile", '0', 1 },
    {                  (char*)"level", '0', 1 },
    {           (char*)"frameRateNum", '0', 1 },
    {           (char*)"frameRateDen", '0', 1 },
    {           (char*)"bitDepthLuma", '0', 1 },
    {         (char*)"bitDepthChroma", '0', 1 },
    {                (char*)"gopSize", '0', 1 },
    {            (char*)"gdrDuration", '0', 1 },
    {         (char*)"lookaheadDepth", '0', 1 },
    {            (char*)"qualityMode", '0', 1 },
    {                 (char*)"keyInt", '0', 1 }, // max bitrate for CPB VBR/CBR
    {                    (char*)"crf", '0', 1 },
    {                    (char*)"cqp", '0', 1 },
    {                   (char*)"llRc", '0', 1 },
    {                (char*)"bitRate", '0', 1 },
    {                 (char*)"initQp", '0', 1 },
    {             (char*)"vbvBufSize", '0', 1 },
    {             (char*)"vbvMaxRate", '0', 1 },
    {           (char*)"intraQpDelta", '0', 1 },
    {                 (char*)"qpMinI", '0', 1 },
    {                 (char*)"qpMaxI", '0', 1 },
    {                (char*)"qpMinPB", '0', 1 },
    {                (char*)"qpMaxPB", '0', 1 },
    {          (char*)"tolCtbRcInter", '0', 1 },
    {             (char*)"aqStrength", '0', 1 },
    {                   (char*)"tune", '0', 1 },
    {                    (char*)"P2B", '0', 1 },
    {              (char*)"bBPyramid", '0', 1 },
    {   (char*)"maxFrameSizeMultiple", '0', 1 },
    {           (char*)"maxFrameSize", '0', 1 },
    {              (char*)"outbufNum", '0', 1 },
    {                (char*)"roiType", '0', 1 },
    {                 (char*)"roiInt", '0', 1 },
    {               (char*)"roiParam", '0', 1 },
    {              (char*)"extSEIInt", '0', 1 },
    {            (char*)"forceIDRInt", '0', 1 },
    { (char*)"roiMapDeltaQpBlockUnit", '0', 1 },
    {   (char*)"roiMapQpDeltaVersion", '0', 1 },
    {   (char*)"enableDynamicBitrate", '0', 1 },
    { (char*)"enableDynamicFrameRate", '0', 1 },
    {             (char*)"maxBFrames", '0', 1 },
    {                    (char*)"hrd", '0', 1 },
    {                (char*)"picSkip", '0', 1 },
    {                    (char*)"vfr", '0', 1 },
    {             (char*)"svcTLayers", '0', 1 },
    {    (char*)"svcExtractMaxTLayer", '0', 1 },
    {              (char*)"sliceSize", '0', 1 },
    {       (char*)"enableDynamicCrf", '0', 1 },
    {                   (char*)"psnr", '0', 1 },
    {                   (char*)"ssim", '0', 1 },
    {            (char*)"ltrInterval", '0', 1 },
    {             (char*)"ltrQpDelta", '0', 1 },
    {              (char*)"ltrRefGap", '0', 1 },
    {          (char*)"ltrInsertTest", '0', 1 },
    {                 (char*)"coreID", '0', 1 },
    {               (char*)"rotation", '0', 1 },
    {                (char*)"openGop", '0', 1 },
    {               (char*)"smartEnc", '0', 1 },
    {    (char*)"enableDynamicKeyInt", '0', 1 },
    {            (char*)"disableMMCO", '0', 1 },
    {          (char*)"inLoopDSRatio", '0', 1 },
    {                 (char*)"aqMode", '0', 1 },
    {              (char*)"psyFactor", '0', 1 },
    {               (char*)"rdoLevel", '0', 1 },
    {         (char*)"enableRdoQuant", '0', 1 },
    {              (char*)"qCompress", '0', 1 },
    {                 (char*)"rcMode", '0', 1 },
    {              (char*)"multicore", '0', 1 },
    {           (char*)"outputCuInfo", '0', 1 }, // enable output cuinfo
    {            (char*)"parseCuInfo", '0', 1 }, // enable parse cuinfo
    {             (char*)"saveCuInfo", '0', 1 }, // save cu info into file
    {                 (char*)"qLevel", '0', 1 },
    {                (char*)"comment", '0', 1 },

    /* Other options */
    {            (char*)"multiDevice", '0', 1 }, // whether enable transcoding on multi-device at the same time
    {           (char*)"randomDevice", '0', 1 }, // whether enable transcoding using random-device
    {               (char*)"log2File", '0', 1 }, // whether enable saving logs to file
    {            (char*)"logCallback", '0', 1 }, // whether register log callback into SDK
    {               (char*)"logLevel", '0', 1 },
    {            (char*)"logLevelSDK", '0', 1 },
    {         (char*)"logLevelFFmpeg", '0', 1 },
    {      (char*)"enableErrorAssert", '0', 1 },
    {           (char*)"multiRuntime", '0', 1 }, // whether enable multiple runtime test
    {       (char*)"uniqueOutputFile", '0', 1 }, // whether save different resolution data into one single file
    {         (char*)"maxQueuedFrame", '0', 1 }, // max queued frame for encoder
    {        (char*)"maxQueuedStream", '0', 1 }, // max queued stream for encoder output thread
    {   (char*)"anotherThread4EncOut", '0', 1 }, // whether start a seperate thread for saving encoded data/cuinfo
    {              (char*)"targetMD5", '0', 1 }, // target md5
    {    (char*)"disableDecProfiling", '0', 1 },
    {    (char*)"disableEncProfiling", '0', 1 },
    {        (char*)"forceHostBuffer", '0', 0 },
    {              (char*)"targetFPS", '0', 1 },
    {      (char*)"targetFrameNumber", '0', 1 },
    {    (char*)"reCountFrame4Target", '0', 1 }, // whether re-count frame for targetFrameNumber in multi-loop cases
    {         (char*)"collectLatency", '0', 1 },
    {              (char*)"inputMode", '0', 1 },
    {     (char*)"separateLumaChroma", '0', 1 },

    {                     NULL,   0, 0 }  // Format of last line
};

#pragma pack(push)
#pragma pack(1)
struct dmabuf_cmd {
    union {
        uint8_t padding[256];
        struct {
            uint32_t size;
            int dma_buf_fd;
            uint64_t dma_addr_t;
        } alloc_cmd;
    };
};

typedef struct _usrbuf {
    union {
        int fd;
        void *virt_addr;
        uint64_t bus_addr;
    };
    int size;
    int type;
    int cid;    // context id
} usrbuf;
#pragma pack(pop)

typedef enum _USR_BUF_TYPE {
    UBT_NONE,
    UBT_HOST,
    UBT_FD,
    UBT_BUS_ADDR
} USR_BUF_TYPE;

typedef struct _usrbuf_context {
    USR_BUF_TYPE type;
    int ctx_id;    // context id
    int buf_size;
    int dmabuf_fd;
    int dev_id;
    vmppRuntimeInstance *runtime_inst;
    struct vmpp_queue *buf_queue;
    struct vmpp_queue *idle_buf_queue;
} usrbuf_context;

typedef struct _usrbuf_factory {
    int active_id;
    int dev_id;
    USR_BUF_TYPE type;
    vmppRuntimeInstance *runtime_inst;
    usrbuf_context ctxs[MAX_USRBUF_CTX];
    pthread_mutex_t ctx_mutex;
    uint64_t request_number;
    uint64_t return_number;
} usrbuf_factory;

/* clang-format off */
typedef enum _INPUT_MODE {
    IM_DEFAULT          = 0,
    IM_SELECT_RANDOMLY  = 1,
    IM_SELECT_ORDERLY   = 2,
    IM_ALL_ORDERLY      = 3
} INPUT_MODE;

typedef enum _DEC_RECV_MEM_TYPE {
    /* To compatible with vmppMemoryType */
    RECV_MT_DEVICE  = 0,
    RECV_MT_HOST    = 1,
    RECV_MT_SHARED  = 3,
    RECV_MT_AUTO    = 0xF
} DEC_RECV_MEM_TYPE;
/* clang-format on */

typedef struct _encode_options {
    char *encCodec;
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
    float maxFrameSizeMultiple;    // deprecated! maximum multiple to average target frame size
    signed int maxFrameSize;       // max frame size, only valid in llrc mode
    unsigned int outbufNum;
    /* For roi, 0 for none, 1 for roi range, 2 for roi map.*/
    unsigned int roiType;
    /* For roi range*/
    unsigned int roiInt;         // ROI interval
    char *roiParam;              // ROI param
    unsigned int extSEIInt;      // extSEI interval
    unsigned int forceIDRInt;    // forceIDR interval

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
    unsigned int ltrInterval;    // 0: disable ltr, 1~max frame num
    int ltrQpDelta;              // default 0
    unsigned int ltrRefGap;      // the frame gap that references the ltr, 1~ltrInterval,
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
    unsigned int multicore;
    unsigned int rcMode;
    unsigned int qLevel;

    vmppPixelFormat pixelFormat;
} enc_options;

typedef struct _transcode_context {
    int save;
    int loop;
    int main_loop;
    int check_md5;
    int using_ffmpeg;
    int period;
    int perf_period;
    int vframes;
    int re_count_vframes;
    int bitDepth;
    int memory_mode;
    int thread_count;
    int multi_device;
    int random_device;
    int encoder_enable;
    int log_into_file;
    int log_callback;
    int log_level;
    int log_level_sdk;
    int log_level_ffmpeg;
    int dec_api_mode;
    int multi_runtime;
    int unique_output_file;
    int max_queued_frame;
    int max_queued_stream;
    int output_cu_info;
    int parse_cu_info;
    int save_cu_info;
    int another_thread_for_enc_out;
    int dec_output_align;
    int dec_crop;
    int dec_mode;
    int dec_no_output_reordering;
    int encode_yuv;
    int disable_dec_profiling;
    int disable_enc_profiling;
    int target_fps;
    int collect_latency;
    int input_mode;
    int enable_error_assert;
    int dec_recv_mem_type;
    int real_recv_mem_type;
    int separate_luma_chroma;
    int dec_core_mode;
    char *codec;
    char *device_name;
    char *output_directory;
    char *output_file;
    char *enc_codec;
    char *dec_crop_info;
    char *target_md5;
    struct vmpp_queue *urls;
    struct vmpp_queue *devices;
    struct vmpp_queue *customized_enc_opts;
    enc_options default_enc_opts;
    volatile uint32_t dec_error_cnt;
    volatile uint32_t enc_error_cnt;
    volatile uint32_t input_error_cnt;
    volatile uint32_t output_error_cnt;
    volatile uint32_t stats_error_cnt;
} transcode_context;

typedef struct _transcode_thread_parameters {
    pthread_t thread_handle;
    int thread_index;
    int job_index;
    int dec_out_index;
    int enc_out_index;
    char *current_url;
    char *current_device;
    int current_main_loop;
    int current_loop;
    uint8_t cur_md5sum[MD5_HASH_LEN];
    uint8_t last_md5sum[MD5_HASH_LEN];
    char last_md5_string[MD5_HASH_LEN * 2 + 1];
    int md5_saved;
    uint64_t file_start_time;
    uint64_t file_read_time;
    uint64_t file_write_time;
    uint64_t file_stop_time;
    uint64_t check_md5_time;
    int md5ctx_inited;
    struct md5_context md5ctx;
    uint64_t period_file_start_time;
    uint64_t enc_start;
    uint64_t enc_stop;
    uint64_t enc_total_frames;
    uint64_t enc_tick;
    uint64_t enc_save_time;
    struct vmpp_queue *frame_queue;
    struct vmpp_queue *idle_frame_queue;
    struct vmpp_queue *releasing_frame_queue;
    struct vmpp_queue *stream_queue;
    struct vmpp_queue *idle_stream_queue;
    pthread_mutex_t frame_mutex;
    pthread_mutex_t stream_mutex;
    vmppChannel dec_ch;
    vmppChannel enc_ch;
    int dec_finished;
    int input_error;
    int output_error;
    int dec_error;
    int enc_error;
    pthread_t dec_output_thread_handle;
    pthread_t enc_thread_handle;
    pthread_t enc_out_thread_handle;
    FILE *enc_output_file;
    int encoder_inited;
    vmppDevice dec_device_handle;
    vmppDevice enc_device_handle;
    enc_options current_enc_opts;
    double psnr_total[3];
    double ssim_total[3];
    int psnr_num;
    int ssim_num;
    uint32_t dec_width;
    uint32_t dec_height;
    int force_flush_encoder;
    int wait_flush_decoder;
    int wait_process_stream;
    uint64_t dec_in_count;
    uint64_t dec_out_count;
    uint64_t period_out_count;
    usrbuf_factory ubf;
    int device_id;
    uint32_t last_width;
    uint32_t last_height;
    uint32_t pic_stride;
    FILE *enc_cu_info_file;
    void *enc_cu_info_buffer;
    uint32_t enc_cu_info_buffer_size;
    uint64_t enc_cu_info_number;
    uint64_t cu_info_parse_time;
    uint64_t cu_info_save_time;

    int using_ffmpeg;
    int jpeg_stream;
    union {
#ifdef USING_FFMPEG
        ff_context_ptr ffctx;
#endif
        stream_context_ptr strmctx;
        struct raw_context *rawctx;
    };
    uint64_t raw_frame_count;
    FILE *dec_output_file;
    vmppCodecType src_codec;
    vmppCodecType dst_codec;
    uint64_t total_latency_us;
    uint64_t max_latency_us;
    uint64_t min_latency_us;
    uint64_t latency_count;
    uint64_t dec_first_send_timestamp;
    uint64_t dec_first_latency_us;
    uint64_t dec_discarded_frame_count;
    uint64_t dec_notshow_frame_count;
    uint64_t dec_skipped_frame_count;
    char *codec;
    int dec_first_latency_frames;
} transcode_thread_params;

static transcode_context g_transcode_context = { 0 };
static vmppRuntimeInstance g_runtime_instance = { 0 };
static int g_has_irregular_url = 0;
#ifdef _WIN32
static bool have_clockfreq = false;
static LARGE_INTEGER clock_freq = { 0 };
#endif
extern char* codec[10];

enum _OPT_ {
    OPT_ALL,
    OPT_SHORT,
    OPT_DEC,
    OPT_ENC,
    OPT_EXTRA,
    OPT_DEPRECATED,
    OPT_MAX
};

static int usage_for_option(const char *opt)
{
    /* only for options with multiple values */
    if (!opt) {
        return -1;
    }
    /* clang-format off */
    if (strcmp(opt, "c") == 0) {
        LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "  'c': source codec type('h264'/'avc','hevc'/'h265','av1','vp9','avs2','jpeg'). And specially, please use 'yuv' for encoder input.");
    }  else if (strcmp(opt, "C") == 0) {
        LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "  'C': target codec type for encoder('h264'/'avc','hevc'/'h265','av1','jpeg').");
    }  else if (strcmp(opt, "M") == 0 || strcmp(opt, "decMemoryMode") == 0) {
        LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "  '%s': memory mode for decoder, default:0, pls check 'vmppDecMemoryMode' for detail", opt);
        LOGIL_INFO("    - 0: vmpp_DEC_MEM_NORMAL");
        LOGIL_INFO("    - 1: vmpp_DEC_MEM_USER_OUT_BUF_HOST");
        LOGIL_INFO("    - 2: vmpp_DEC_MEM_USER_OUT_BUF_DEV");
        LOGIL_INFO("    - 3: vmpp_DEC_MEM_LESS_DEV_MEM");
        LOGIL_INFO("    - 4: vmpp_DEC_MEM_USER_AS_HWOUT");
    } else if (strcmp(opt, "decCrop") == 0) {
        LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "  'decCrop': crop flag for decoder, default:0(disable).");
        LOGIL_INFO("    - 1: enable cropping through post proc module based on information from parameters set");
        LOGIL_INFO("    - 2: enable cropping through post proc module based on information provided by user, pls see '--decCropInfo'");
        LOGIL_INFO("    - 3: enable cropping through function inside SDK (only effective when output to host buffer)");
    } else if (strcmp(opt, "decCropInfo") == 0) {
        LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "  'decCropInfo': crop info for decoder, USED when decCrop==2, string format: 'x=?,y=?,w=?,h=?'.");
    } else if (strcmp(opt, "decRecvMemoryType") == 0) {
        LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "  'decRecvMemoryType': memory type for vmppDecReceiveFrame. To replace 'forceHostBuffer'.");
        LOGIL_INFO("    - 0: device.    (should not be used when decMemoryMode is 1 / 3)");
        LOGIL_INFO("    - 1: host.      (should not be used when decMemoryMode is 4 / 5)");
        LOGIL_INFO("    - 3: shared fd. (only for sg100, and is not supported yet!)");
        LOGIL_INFO("    - *: auto.      (default, for all other values)");
    } else if (strcmp(opt, "decCoreMode") == 0) {
        LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "  'decCoreMode': core mode for decoder, default:0(auto)");
        LOGIL_INFO("    - 0: auto select core mode");
        LOGIL_INFO("    - 1: single core mode");
        LOGIL_INFO("    - 2: multi core mode");
    } else if (strcmp(opt, "decMode") == 0) {
        LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "  'decMode': decode mode, default:0(normal), 1-intra only, 2-skip nonref, 3-low delay.");
    } else if (strcmp(opt, "decApiMode") == 0) {
        LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "  'decApiMode': decoder api mode, default:0(parallel), 1-serial.");
    } else if (strcmp(opt, "inputMode") == 0) {
        LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "  'inputMode': input mode: 0(default, as usual).");
        LOGIL_INFO("    - 0: ~");
        LOGIL_INFO("    - 1: Randomly select one from input list");
        LOGIL_INFO("    - 2: Orderly select one from input list");
        LOGIL_INFO("    - 3: Orderly starting from different file for each thread");
    } else if (strcmp(opt, "pixelFormat") == 0) {
        LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "  'pixelFormat': pixel format for encoder input, such as 'nv12','yuv420p','p010le','rgba', default:nv12.");
    }  else if (strncmp(opt, "logLevel", strlen("logLevel")) == 0) {
        LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "  '%s': 0-TRACE 1-DEBUG 2-INFO 3-WARN 4-ERROR.", opt);
    }  else if (strcmp(opt, "help") == 0 || strcmp(opt, "h") == 0) {
        LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "  '%s': help for options.", opt);
        LOGIL_INFO("    - %d: all (except deprecated, default)", OPT_ALL);
        LOGIL_INFO("    - %d: only short options", OPT_SHORT);
        LOGIL_INFO("    - %d: only options for dec", OPT_DEC);
        LOGIL_INFO("    - %d: only options for enc", OPT_ENC);
        LOGIL_INFO("    - %d: only extra options", OPT_EXTRA);
        LOGIL_INFO("    - %d: only deprecated", OPT_DEPRECATED);
        LOGIL_INFO("    - One of the options with multiple values");
    }  else {
        return 1;
    }
    /* clang-format on */
    return 0;
}
// static void usage(const char *program)
// {
//     /* clang-format off */
//     LOGIL_INFO("");
//     LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "Multi-Thread Transcoding App powered by VMPP");
//     LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, " - Decoder Version: %s", vmppDecGetVersion()->versionString);
//     LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, " - Encoder Version: %s", vmppEncGetVersion()->versionString);
//     LOGIL_INFO("");

//     LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN,
//         "Example: %s -i $in_file [ -o $out_dir -d /dev/va_video0 -s 0 -l 1 -L 1 -m 0 --logLevel 2 ... ]", program);
//     LOGIL_INFO("");
    // LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "  Try using '-h'/'--help' with:");
    // LOGIL(LOG_LEVEL_INFO, COLOR_CYAN, "        - %d: all (except deprecated, default)", OPT_ALL);
    // LOGIL(LOG_LEVEL_INFO, COLOR_CYAN, "        - %d: only short options", OPT_SHORT);
    // LOGIL(LOG_LEVEL_INFO, COLOR_CYAN, "        - %d: only options for dec", OPT_DEC);
    // LOGIL(LOG_LEVEL_INFO, COLOR_CYAN, "        - %d: only options for enc", OPT_ENC);
    // LOGIL(LOG_LEVEL_INFO, COLOR_CYAN, "        - %d: only extra options", OPT_EXTRA);
    // LOGIL(LOG_LEVEL_INFO, COLOR_CYAN, "        - %d: only deprecated", OPT_DEPRECATED);
    // LOGIL(LOG_LEVEL_INFO, COLOR_CYAN, "        - One of the following options with multiple values");
    // LOGIL_INFO("");

    // if (opt == OPT_ALL || opt == OPT_SHORT) {
//     LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "Short Options:");
//     LOGIL_INFO("  -i    url for input file/directory or a json file with device/urls list");
//     LOGIL_INFO("  -o    (opt) output directory or file name, default is:NULL, directory must exist");
//     LOGIL_INFO("  -d    (opt) video device name, we will enum all available devices and choose a random one if you have not spicified that");
//     LOGIL_INFO("  -r    (opt) same as '-d', in order to be compatible with old 'transcode' sample");
//     LOGIL_INFO("  -s    (opt) whether to save YUV data from decoder or data from encoder, default:0");
//     LOGIL_INFO("  -l    (opt) loop count for file, default:1, infinite loop if <= 0");
//     LOGIL_INFO("  -L    (opt) loop count for file list, default:1, infinite loop if <= 0");
//     LOGIL_INFO("  -m    (opt) whether enable MD5 checking, default:0");
//     LOGIL_INFO("  -f    (opt) whether using FFmpeg for demuxing, default:1 if FFmpeg is enabled during compiling, otherwise 0.");
    // LOGIL_INFO("              - Exceptional: Disabled on ARM64 by default(May be enabled when FFmpeg is recompiled)");
//     LOGIL_INFO("  -c    (opt) codec type('h264'/'avc','hevc'/'h265','av1','vp9','avs2','jpeg'). And specially, the 'yuv' for encoder input is MUST.");
//     LOGIL_INFO("  -C    (opt) encode codec type('h264'/'avc','hevc'/'h265','av1','jpeg'), MUST if encoder is enabled");
//     LOGIL_INFO("  -p    (opt) print debug log every X frames, default:100");
//     LOGIL_INFO("  -P    (opt) print performance every X frames, default:1000");
//     LOGIL_INFO("  -n    (opt) same as '--targetFrameNumber', specify the frame number to be decoded/encoded, default:0, to decode/encode all frames");
//     LOGIL_INFO("  -b    (opt) specify stream bit depth, default:8");
//     LOGIL_INFO("  -M    (opt) same as '--decMemoryMode', memory mode for decoder, default:0, pls check 'vmppDecMemoryMode' for detail");
    // LOGIL_INFO("                - 0: vmpp_DEC_MEM_NORMAL");
    // LOGIL_INFO("                - 1: vmpp_DEC_MEM_USER_OUT_BUF_HOST");
    // LOGIL_INFO("                - 2: vmpp_DEC_MEM_USER_OUT_BUF_DEV");
    // LOGIL_INFO("                - 3: vmpp_DEC_MEM_LESS_DEV_MEM");
    // LOGIL_INFO("                - 4: vmpp_DEC_MEM_USER_AS_HWOUT");
    //     LOGIL_INFO("  -a    (opt) same as '--decOutputAlign', output alignment for decoder, default:0");
    //     LOGIL_INFO("  -t    (opt) thread count, default:1");
    //     LOGIL_INFO("  -W    (opt) same as '--width', must when encoding yuv data");
    //     LOGIL_INFO("  -H    (opt) same as '--height', must when encoding yuv data");

    //     LOGIL_INFO("");
    // }

    // if (opt != OPT_SHORT) {
    //     LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "Long Options (opt):");
    // }

    // if (opt == OPT_ALL || opt == OPT_DEC) {
    //     LOGIL_INFO(" ----- Options for Decoder ----- ");
//     LOGIL_INFO(" ----- Options for Decoder ----- ");
//     LOGIL_INFO("  --decMode                   decode mode, default:0(normal), 1-intra only, 2-skip nonref, 3-low delay");
//     LOGIL_INFO("  --decApiMode                decoder api mode, default:0(parallel), 1-serial");
//     LOGIL_INFO("  --decOutputAlign            same as '-a', output alignment for decoder, default:0");
//     LOGIL_INFO("  --decMemoryMode             same as '-M', memory mode for decoder, default:0.");
    // LOGIL_INFO("  --decCrop                   crop flag for decoder, default:0(disable).");
    // LOGIL_INFO("                                - 1: enable cropping through post proc module based on information from parameters set");
    // LOGIL_INFO("                                - 2: enable cropping through post proc module based on information provided by user, pls see '--decCropInfo'");
    // LOGIL_INFO("                                - 3: enable cropping through function inside SDK (only effective when output to host buffer)");
//     LOGIL_INFO("  --decCropInfo               crop info for decoder, USED when decCrop==2, string format: 'x=?,y=?,w=?,h=?'");
//     LOGIL_INFO("  --decNoOutputReordering     output reordering for decoder, default:0");
    // LOGIL_INFO("  --decRecvMemoryType         memory type for vmppDecReceiveFrame. To replace 'forceHostBuffer'.");
    // LOGIL_INFO("                                - 0: device.    (should not be used when decMemoryMode is 1 / 3)");
    // LOGIL_INFO("                                - 1: host.      (should not be used when decMemoryMode is 4 / 5)");
    // LOGIL_INFO("                                - 3: shared fd. (only for sg100, and is not supported yet!)");
    // LOGIL_INFO("                                - *: auto.      (default, for all other values)");
    //     LOGIL_INFO("  --decCoreMode               core mode for decoder, default:0(auto)");
    //     LOGIL_INFO("                                - 0: auto select core mode");
    //     LOGIL_INFO("                                - 1: single core mode");
    //     LOGIL_INFO("                                - 2: multi core mode");
    //     LOGIL_INFO("");
    // }

    // if (opt == OPT_ALL || opt == OPT_ENC) {
//     LOGIL_INFO(" ----- Options for Encoder ----- ");
//     LOGIL_INFO("  --width                     same as '-W', width, must when encoding yuv data");
        // LOGIL_INFO("  --height                    same as '-H', height, must when encoding yuv data");
//     LOGIL_INFO("  --pixelFormat               pixel format for encoder input, such as 'nv12','yuv420p','p010le','rgba', default:nv12");
//     LOGIL_INFO("  --profile                   main profile or main still picture profile");
//     LOGIL_INFO("  --level                     main profile level");
//     LOGIL_INFO("  --frameRateNum              frame rate numerator");
//     LOGIL_INFO("  --frameRateDen              frame rate denominator");
//     LOGIL_INFO("  --bitDepthLuma              luma bit depth");
//     LOGIL_INFO("  --bitDepthChroma            chroma bit depth");
//     LOGIL_INFO("  --gopSize                   gop size");
//     LOGIL_INFO("  --gdrDuration               gdr duration");
//     LOGIL_INFO("  --lookaheadDepth            lookahead depth");
//     LOGIL_INFO("  --qualityMode               quality mode");
//     LOGIL_INFO("  --tune                      tune");
//     LOGIL_INFO("  --keyInt                    IDR interval");
//     LOGIL_INFO("  --crf                       CRF constant");
//     LOGIL_INFO("  --cqp                       cqp");
//     LOGIL_INFO("  --llRc                      llRc");
//     LOGIL_INFO("  --bitRate                   bps, bit rate");
//     LOGIL_INFO("  --cqp                       cqp");
//     LOGIL_INFO("  --initQp                    init qp");
//     LOGIL_INFO("  --vbvBufSize                kb, vbv Buffer size");
//     LOGIL_INFO("  --vbvMaxRate                kbps, vbv max rate");
//     LOGIL_INFO("  --intraQpDelta              intra qp delta");
//     LOGIL_INFO("  --qpMinI                    min qp value of I frame");
//     LOGIL_INFO("  --qpMaxI                    max qp value of I frame");
//     LOGIL_INFO("  --qpMinPB                   min qp value of B/P frame");
//     LOGIL_INFO("  --qpMaxPB                   max qp value of B/P frame");
//     LOGIL_INFO("  --vbr                       vbr mode");
//     LOGIL_INFO("  --aqStrength                aq strength");
//     LOGIL_INFO("  --P2B                       P2B");
//     LOGIL_INFO("  --bBPyramid                 bBPyramid");
//     LOGIL_INFO("  --maxFrameSizeMultiple      maxFrameSizeMultiple");
//     LOGIL_INFO("  --maxFrameSize              max frame size");
//     LOGIL_INFO("  --outbufNum                 outbufNum, default:4");
//     LOGIL_INFO("  --roiType                   ROI type, 0 for none, 1 for roi range, 2 for roi map");
//     LOGIL_INFO("  --roiInt                    ROI interval");
//     LOGIL_INFO("  --roiParam                  ROI param");
//     LOGIL_INFO("  --extSEIInt                 extSEI interval");
//     LOGIL_INFO("  --forceIDRInt               forceIDR interval");
//     LOGIL_INFO("  --roiMapDeltaQpBlockUnit    roi map delta qp block unit");
//     LOGIL_INFO("  --roiMapQpDeltaVersion      roi map delta qp version");
//     LOGIL_INFO("  --enableDynamicBitrate      enable dynamic bitrate or not(1/0)");
//     LOGIL_INFO("  --enableDynamicFrameRate    enable dynamic framerate or not(1/0)");
//     LOGIL_INFO("  --maxBFrames                max B frames control for adaptive GOP decision");
//     LOGIL_INFO("  --hrd                       Hypothetical Reference Decoder model");
//     LOGIL_INFO("  --picSkip                   Frame All Skip Mode When Overflow");
//     LOGIL_INFO("  --vfr                       variable frame rate");
//     LOGIL_INFO("  --svcTLayers                Temporal Layers for Scalable Video Coding");
//     LOGIL_INFO("  --svcExtractMaxTLayer       Max Temporal Layer to Extract for Scalable Video Coding");
//     LOGIL_INFO("  --sliceSize                 Slice size in CTB/MB rows for multislice");
//     LOGIL_INFO("  --enableDynamicCrf          enable dynamic crf or not(1/0)");
//     LOGIL_INFO("  --psnr                      enable caculation of PSNR or not, default: 0");
//     LOGIL_INFO("  --ssim                      enable caculation of SSIM or not, default: 0");
//     LOGIL_INFO("  --ltrInterval               enable LTR, default: 0");
//     LOGIL_INFO("  --ltrQpDelta                set LTR frame QpDelta, default: 0");
//     LOGIL_INFO("  --ltrRefGap                 set frame gap that references the LTR, default: 0");
//     LOGIL_INFO("  --ltrInsertTest             Enable test insert LTR: 0");
//     LOGIL_INFO("  --rotation                  Rotate input image, 0-Disabled (Default), 1-90 degrees right, 2-90 degrees left, 3-180 degrees right");
//     LOGIL_INFO("  --coreID                    Enable specify core id: 0-3");
//     LOGIL_INFO("  --openGop                   openGop");
//     LOGIL_INFO("  --smartEnc                  smartEnc Mode");
//     LOGIL_INFO("  --enableDynamicKeyInt       enable dynamic keyInt or not(1/0)");
//     LOGIL_INFO("  --disableMMCO               disable h264 memory management control operation or not(1/0)");
//     LOGIL_INFO("  --inLoopDSRatio             in-loop downsample ratio for first pass(1/0)");
//     LOGIL_INFO("  --aqMode                    aq mode:0-3");
//     LOGIL_INFO("  --psyFactor                 weight of psycho-visual encoding:0.0-4.0");
//     LOGIL_INFO("  --rdoLevel                  RDO Level can balance the quality and throughput:1-3");
//     LOGIL_INFO("  --enableRdoQuant            enable RDO quantization or not(1/0)");
//     LOGIL_INFO("  --qCompress                 qCompress sets the quantizer curve compression factor:0.0-1.0");
//     LOGIL_INFO("  --rcMode                    rate control mode, default:0, pls check 'vmppEncRcMode' for detail");
//     LOGIL_INFO("  --multicore                 enable multi-core encoding or not(1/0)");
//     LOGIL_INFO("  --outputCuInfo              whether enable output Cu Info, default:0, will be force enabled when 'parseCuInfo' or 'saveCuInfo' is enabled");
    // LOGIL_INFO("  --parseCuInfo               whether enable parse Cu Info, default:0, will be force enabled when 'saveCuInfo' is enabled");
//     LOGIL_INFO("  --saveCuInfo                whether parse and save Cu Info into local file, default:0");
    //     LOGIL_INFO("  --qLevel                    quantization level for JPEG encoder [1 ~ %d], %d for user defined qTable", USER_DEFINED_QTABLE, USER_DEFINED_QTABLE);
    //     LOGIL_INFO("  --comment                   comment for JPEG encoder");
    //     LOGIL_INFO("");
    // }

    // if (opt == OPT_ALL || opt == OPT_EXTRA) {
    //     LOGIL_INFO(" ----- Extra Options ----- ");
//     LOGIL_INFO("  --multiDevice               assign device in order if necessary: default:0, should not used on sg100 for multi-thread encoding cases");
//     LOGIL_INFO("  --randomDevice              assign device randomly if necessary: default:0, should not used on sg100 for multi-thread encoding cases");
//     LOGIL_INFO("  --log2File                  whether enable saving logs to file, default:0");
//     LOGIL_INFO("  --logCallback               whether register log callback into SDK (and FFmpeg), default:0");
    // LOGIL_INFO("  --logLevel                  log level,            default:2(INFO), 1-DEBUG 2-INFO 3-WARN 4-ERROR");
    // LOGIL_INFO("  --logLevelSDK               log level for SDK,    default:3(WARN), 1-DEBUG 2-INFO 3-WARN 4-ERROR");
    // LOGIL_INFO("  --logLevelFFmpeg            log level for FFmpeg, default:3(WARN), 1-DEBUG 2-INFO 3-WARN 4-ERROR. Only effective when FFmpeg is enabled.");
    // LOGIL_INFO("  --enableErrorAssert         whether enable assert when error happens, default:0");
    // LOGIL_INFO("  --multiRuntime              whether enable multiple runtime test, aka: every channel with a runtime instance (NO global init & denit for runtime & vmpp), default:0");
//     LOGIL_INFO("  --uniqueOutputFile          whether save different resolution data into one single file, default:0");
//     LOGIL_INFO("  --maxQueuedFrame            max frame number in queue for encoder, default:3, only effective when decApiMode==serial");
//     LOGIL_INFO("  --maxQueuedStream           max stream number in queue for encoder output thread, default:2, only effective when anotherThread4EncOut is enabled");
//     LOGIL_INFO("  --anotherThread4EncOut      using a separate thread to save encoded data/cuinfo, default:0");
//     LOGIL_INFO("  --targetMD5                 the target MD5, default:null, if not set, the MD5 checking will based on MD5 of last main loop");
//     LOGIL_INFO("  --disableDecProfiling       whether disable profiling for decoder, default:0");
//     LOGIL_INFO("  --disableEncProfiling       whether disable profiling for encoder, default:0");
    // LOGIL_INFO("                                - Exceptional: Can't be used when decMemoryMode is 2 / 4 / 5");
//     LOGIL_INFO("  --targetFPS                 target frame rate, default:0(No FPS Control)");
//     LOGIL_INFO("  --targetFrameNumber         target frame number, same as '-n', default:0");
//     LOGIL_INFO("  --reCountFrame4Target       whether re-count frame for 'targetFrameNumber' in multi-loop cases, default:0");
//     LOGIL_INFO("  --collectLatency            whether enable the statistics of latency, default:0, meaningless when 'gopSize != 1' for encoder");
//     LOGIL_INFO("  --inputMode                 input mode: 0(default, as usual).");
    // LOGIL_INFO("                                - 0: ~");
    // LOGIL_INFO("                                - 1: Randomly select one from input list");
    // LOGIL_INFO("                                - 2: Orderly select one from input list");
    // LOGIL_INFO("                                - 3: Orderly starting from different file for each thread");
    //     LOGIL_INFO("  --separateLumaChroma        whether memory for luma and chroma are separated, default: 0");
    //     LOGIL_INFO("                                - Exceptional: Only effective when 'decMemoryMode' is 4('vmpp_DEC_MEM_USER_AS_HWOUT') and encoder is not enabled.");
    //     LOGIL_INFO("");
    // }

    // if (opt == OPT_DEPRECATED) {
    //     LOGIL_TRACE(" ----- Deprecated Options ----- ");
    // LOGIL_TRACE("  --forceHostBuffer           force decoder output to host buffer, default:0. Deprecated, pls use 'decRecvMemoryType' instead.");
    // LOGIL_TRACE("                                - Exceptional: Can't be used when decMemoryMode is 2 / 4 / 5");
    // LOGIL_TRACE("");
//}

static inline int is_thread_active(pthread_t *thread_id)
{
#ifdef __linux__
    if (*thread_id) {
#else
    if (thread_id->p) {
#endif
        return 1;
    }
    return 0;
}

static inline uint32_t atomic_inc(volatile uint32_t *ptr)
{
#ifdef _WIN32
    return InterlockedExchangeAdd((long *)ptr, (long)1);
#else
    return __atomic_add_fetch(ptr, 1, __ATOMIC_SEQ_CST);
#endif
}

#ifdef _WIN32
static inline uint64_t clockfreq(void)
{
    if (!have_clockfreq) {
        QueryPerformanceFrequency(&clock_freq);
        have_clockfreq = true;
    }
    return clock_freq.QuadPart;
}

static inline uint64_t mul_div64(uint64_t num, uint64_t mul, uint64_t div)
{
    const uint64_t rem = num % div;
    return (num / div) * mul + (rem * mul) / div;
}
#endif

static inline void sleep_ns(uint64_t target)
{
    struct timespec req, remain;
    memset(&req, 0, sizeof(req));
    memset(&remain, 0, sizeof(remain));
    req.tv_sec = target / NANOSEC_PER_SEC;
    req.tv_nsec = target % NANOSEC_PER_SEC;

    while (nanosleep(&req, &remain)) {
        req = remain;
        memset(&remain, 0, sizeof(remain));
    }
}

static inline void md5_hexstring(uint8_t md5[], int size, char *out_string)
{
    int i;
    for (i = 0; i < size; i++) {
        sprintf(out_string + 2 * i, "%02X", md5[i]);
    }
}

static inline const char *codec_string(vmppCodecType type)
{
    switch (type) {
    case vmpp_CODEC_DEC_JPEG:
    case vmpp_CODEC_ENC_JPEG:
        return "JPEG";
    case vmpp_CODEC_DEC_H264:
    case vmpp_CODEC_ENC_H264:
        return "H.264";
    case vmpp_CODEC_DEC_HEVC:
    case vmpp_CODEC_ENC_HEVC:
        return "HEVC";
    case vmpp_CODEC_DEC_AV1:
    case vmpp_CODEC_ENC_AV1:
        return "AV1";
    case vmpp_CODEC_DEC_VP9:
        return "VP9";
    case vmpp_CODEC_DEC_AVS2:
        return "AVS2";
    default:
        break;
    }
    return "NA";
}

#define CODEC(s) codec_string(s)

static inline const char *frame_type_string(vmppFrameType type)
{
    switch (type) {
    case vmpp_FRM_I:
        return "I";
    case vmpp_FRM_P:
        return "P";
    case vmpp_FRM_B:
        return "B";
    default:
        break;
    }
    return "NA";
}

#define FRAME(s) frame_type_string(s)

static inline const char *nal_type_string(vmppNalType type)
{
    switch (type) {
    case vmpp_NAL_SPS:
        return "SPS";
    case vmpp_NAL_PPS:
        return "PPS";
    case vmpp_NAL_VPS:
        return "VPS";
    case vmpp_NAL_IVF_HEADER:
        return "IVF-HDR";
    case vmpp_NAL_PREFIX_SEI:
        return "P-SEI";
    case vmpp_NAL_SUFFIX_SEI:
        return "S-SEI";
    case vmpp_NAL_FILLER_DATA:
        return "FIL-DATA";
    case vmpp_NAL_I:
        return "I";
    case vmpp_NAL_B:
        return "B";
    case vmpp_NAL_P:
        return "P";
    case vmpp_NAL_NONE:
    default:
        break;
    }
    return "NA";
}

#define NAL(s) nal_type_string(s)

static inline void usrbuf_ctx_free_buf(usrbuf_context *ctx, usrbuf *buf)
{
    vaccrt_free_video_t vaccrt_free_video;
    if (!ctx || !buf) {
        return;
    }
    switch (ctx->type) {
    case UBT_HOST:
        if (buf->virt_addr) {
            free(buf->virt_addr);
        }
        break;
    case UBT_FD:
        if (buf->fd < 0) {
            close(buf->fd);
        }
        break;
    case UBT_BUS_ADDR:
        if (buf->bus_addr && ctx->runtime_inst && ctx->runtime_inst->freeVideo) {
            vaccrt_free_video = (vaccrt_free_video_t)ctx->runtime_inst->freeVideo;
            vaccrt_free_video(ctx->dev_id, buf->bus_addr);
        }
        break;
    default:
        break;
    }
    free(buf);
}

static void usrbuf_ctx_reset(usrbuf_context *ctx)
{
    int i;

    if (ctx->buf_queue) {
        for (i = 0; i < vmpp_queue_size(ctx->buf_queue); i++) {
            usrbuf_ctx_free_buf(ctx, (usrbuf *)vmpp_queue_peek(ctx->buf_queue, i));
        }
        vmpp_queue_free(&ctx->buf_queue);
    }
    if (ctx->idle_buf_queue) {
        for (i = 0; i < vmpp_queue_size(ctx->idle_buf_queue); i++) {
            usrbuf_ctx_free_buf(ctx, (usrbuf *)vmpp_queue_peek(ctx->idle_buf_queue, i));
        }
        vmpp_queue_free(&ctx->idle_buf_queue);
    }

    if (ctx->type == UBT_FD && ctx->dmabuf_fd < 0) {
        close(ctx->dmabuf_fd);
    }

    ctx->buf_size = 0;
    ctx->dmabuf_fd = -1;
    ctx->dev_id = 0;
    ctx->runtime_inst = NULL;
    ctx->buf_queue = NULL;
    ctx->idle_buf_queue = NULL;
    ctx->type = UBT_NONE;
}

static int usrbuf_ctx_init(
    usrbuf_context *ctx, USR_BUF_TYPE type, int ctx_id, int buf_size, int dev_id, vmppRuntimeInstance *rt)
{
    char kchar_device[1024] = { 0 };
    ctx->type = UBT_NONE;

    if (type <= 0 || type > UBT_BUS_ADDR || buf_size <= 0) {
        LOG_ERROR("[transcode_mt] Invalid buffer type(%d) or size(%d) for usr buffer!", type, buf_size);
        return -1;
     }

#ifdef _WIN32
    if (type == UBT_FD) {
        LOG_ERROR("[transcode_mt] FD mode is not supported on Windows!");
        return -1;
    }
#endif

    switch (type) {
    case UBT_FD:
        if (dev_id < 0) {
            LOG_ERROR("[transcode_mt] Invalid device id(%d) for usr buffer!", dev_id);
            return -1;
        }
        sprintf(kchar_device, "/dev/va%d_ctl", dev_id);
#ifdef __linux__
        if (access(kchar_device, F_OK) == 0) {
        } else {
            snprintf(kchar_device, sizeof(kchar_device), "/dev/vastai%d_ctl", dev_id);
        }
#endif
        ctx->dmabuf_fd = open(kchar_device, O_RDWR);
        if (ctx->dmabuf_fd < 0) {
            LOG_ERROR("[transcode_mt] open dma device (%d) failed for usr buffer!", dev_id);
            return -1;
        }
        break;
    case UBT_BUS_ADDR:
        if (!rt || dev_id < 0 || !rt->mallocVideo || !rt->freeVideo) {
            LOG_ERROR("[transcode_mt] Invalid runtime instance context for usr buffer!");
            return -1;
        }

        // we assume runtime initialization is already done outside
        break;
    case UBT_HOST:
    default:
        break;
    }

    if (vmpp_queue_init(&ctx->buf_queue) < 0) {
        LOG_ERROR("[transcode_mt] Failed to init buf queue for user buffer!");
        goto init_fail;
    }

    if (vmpp_queue_init(&ctx->idle_buf_queue) < 0) {
        LOG_ERROR("[transcode_mt] Failed to init idle buf queue for user buffer!");
        goto init_fail;
    }

    ctx->type = type;
    ctx->buf_size = buf_size;
    ctx->dev_id = dev_id;
    ctx->runtime_inst = rt;
    ctx->ctx_id = ctx_id;

    return 0;
init_fail:
    if (ctx->buf_queue) {
        vmpp_queue_free(&ctx->buf_queue);
    }
    if (ctx->idle_buf_queue) {
        vmpp_queue_free(&ctx->idle_buf_queue);
    }
    if (type == UBT_FD && ctx->dmabuf_fd < 0) {
        close(ctx->dmabuf_fd);
    }
    return -1;
}

static usrbuf *usrbuf_ctx_request_buf(usrbuf_context *ctx)
{
    int ret;
    void *tmp_host = NULL;
    uint64_t tmp_busaddr = 0;
    usrbuf *buf = NULL;
    struct dmabuf_cmd cmd;
    rtError_t vaccRet;
    vaccrt_malloc_video_t vaccrt_malloc_video;

    if (!ctx || ctx->type == UBT_NONE || ctx->type > UBT_BUS_ADDR) {
        LOG_ERROR("[transcode_mt] invalid parameters to get usr buffer!");
        return NULL;
    }

    if (vmpp_queue_size(ctx->idle_buf_queue) > 0) {
        buf = (usrbuf *)vmpp_queue_pop_front(ctx->idle_buf_queue);
        vmpp_queue_push_back(ctx->buf_queue, buf);
        return buf;
    }

    buf = (usrbuf *)malloc(sizeof(usrbuf));
    if (!buf) {
        LOG_ERROR("[transcode_mt] fail to malloc buf for user buffer!");
        return NULL;
    }

    switch (ctx->type) {
    case UBT_HOST:
        tmp_host = malloc(ctx->buf_size);
        if (!tmp_host) {
            LOG_ERROR("[transcode_mt] fail to malloc user buffer, size %d!", ctx->buf_size);
            goto usrbuf_get_error;
        }
        buf->virt_addr = tmp_host;
        break;
    case UBT_FD:
        // !!!FIXME
        /* FD mode seems not used like this, this dma fd isn't same thing with fd from GPU??? */
        cmd.alloc_cmd.size = ctx->buf_size;
        cmd.alloc_cmd.dma_buf_fd = -1;
        if (ctx->dmabuf_fd < 0) {
            LOG_ERROR("dma fd is invalid: %d\n", ctx->dmabuf_fd);
            goto usrbuf_get_error;
        }

        ret = ioctl(ctx->dmabuf_fd, DMABUF_IOCTL_ALLOC, &cmd, sizeof(cmd));
        if (ret < 0 || cmd.alloc_cmd.dma_buf_fd < 0) {
            LOG_ERROR("ioctl for alloc dma buf is failed %d\n", ret);
            goto usrbuf_get_error;
        }
        buf->fd = cmd.alloc_cmd.dma_buf_fd;
        break;
    case UBT_BUS_ADDR:
        vaccrt_malloc_video = (vaccrt_malloc_video_t)ctx->runtime_inst->mallocVideo;
        vaccRet = vaccrt_malloc_video(ctx->dev_id, 4096, ctx->buf_size, &tmp_busaddr);
        if (vaccRet) {
            LOG_ERROR("vaccrt malloc failed: err %d, size %d", vaccRet, ctx->buf_size);
            goto usrbuf_get_error;
        }
        buf->bus_addr = tmp_busaddr;
        break;
    default:
        break;
    }

    buf->size = ctx->buf_size;
    buf->type = ctx->type;
    buf->cid = ctx->ctx_id;
    vmpp_queue_push_back(ctx->buf_queue, buf);

    return buf;

usrbuf_get_error:
    if (buf) {
        free(buf);
    }
    return NULL;
}

static int usrbuf_ctx_return_buf(usrbuf_context *ctx, usrbuf *buf)
{
    int i;
    usrbuf *tmp = 0;
    if (!ctx || !buf) {
        LOG_ERROR("invalid parameters: ctx %p, buf %p\n", ctx, buf);
        return -1;
    }
    if (ctx->ctx_id == buf->cid) {
        for (i = 0; i < vmpp_queue_size(ctx->buf_queue); i++) {
            tmp = (usrbuf *)vmpp_queue_peek(ctx->buf_queue, i);
            if (buf == tmp) {
                break;
            }
        }
        if (i == vmpp_queue_size(ctx->buf_queue)) {
            LOG_ERROR("abnormal buf: ctx id %d, buf %p\n", buf->cid, buf);
            return -1;
        }
        vmpp_queue_get(ctx->buf_queue, i);
        vmpp_queue_push_back(ctx->idle_buf_queue, buf);
    } else {
        LOG_ERROR("mismatch context id: %d VS %d\n", ctx->ctx_id, buf->cid);
        return -1;
    }

    return 0;
}

static usrbuf *usrbuf_ctx_find_buf(usrbuf_context *ctx, uint64_t buf)
{
    int i;
    usrbuf *target = NULL, *tmp = NULL;
    USR_BUF_TYPE type = ctx->type;
    for (i = 0; i < vmpp_queue_size(ctx->buf_queue); i++) {
        tmp = (usrbuf *)vmpp_queue_peek(ctx->buf_queue, i);
        if (type == UBT_HOST) {
            if (tmp->virt_addr == (void *)buf) {
                break;
            }
        } else if (type == UBT_BUS_ADDR) {
            if (tmp->bus_addr == buf) {
                break;
            }
        } else if (type == UBT_FD) {
            if (tmp->fd == (int)buf) {
                break;
            }
        }
    }
    if (i < vmpp_queue_size(ctx->buf_queue)) {
        target = tmp;
    }

    return target;
}

static inline int usrbuf_ctx_idle(usrbuf_context *ctx)
{
    return (vmpp_queue_size(ctx->buf_queue) <= 0);
}

static int usrbuf_factory_setup(usrbuf_factory *factory, USR_BUF_TYPE type, int dev_id, vmppRuntimeInstance *rt)
{
    if (!factory) {
        LOG_ERROR("[transcode_mt] Invalid parameters for usrbuf factory, factory(%p)", factory);
        return -1;
    }

    if (type == UBT_FD) {
        if (dev_id < 0) {
            LOG_ERROR("[transcode_mt] Invalid parameters for usrbuf factory(FD), dev_id(%d)!", dev_id);
            return -1;
        }
    } else if (type == UBT_BUS_ADDR) {
        if (dev_id < 0 || !rt) {
            LOG_ERROR("[transcode_mt] Invalid parameters for usrbuf factory(DEVMEM), dev_id(%d), rt(%p)!", dev_id, rt);
            return -1;
        } else if (!rt->mallocVideo || !rt->freeVideo) {
            LOG_ERROR(
                "[transcode_mt] Invalid parameters for usrbuf factory(DEVMEM), dev_id(%d), rt(%p), mallocVideo(%p), freeVideo(%p)!",
                dev_id, rt, rt->mallocVideo, rt->freeVideo);
            return -1;
        }
    }

    factory->dev_id = dev_id;
    factory->runtime_inst = rt;
    factory->type = type;
    pthread_mutex_init(&factory->ctx_mutex, NULL);
    return 0;
}

static void usrbuf_factory_reset(usrbuf_factory *factory)
{
    int i;

    for (i = 0; i < MAX_USRBUF_CTX; i++) {
        usrbuf_ctx_reset(&factory->ctxs[i]);
    }
    factory->active_id = 0;
    factory->type = UBT_NONE;
    if (factory->request_number != factory->return_number) {
        LOG_ERROR("[transcode_mt] %s: request_number %lu return_number %lu!", __func__, factory->request_number,
            factory->return_number);
    } else {
        LOG_DEBUG("[transcode_mt] %s: request_number %lu return_number %lu!", __func__, factory->request_number,
            factory->return_number);
    }
    factory->return_number = 0;
    factory->request_number = 0;
    pthread_mutex_destroy(&factory->ctx_mutex);
}

static int usrbuf_factory_new_size_internal(usrbuf_factory *factory, int new_size)
{
    int ret, new_id, size;
    usrbuf_context *ctx;
    size = NEXT_MULTIPLE(new_size, 4096);
    new_id = factory->active_id;
    ctx = &factory->ctxs[factory->active_id];

    while (!usrbuf_ctx_idle(ctx)) {
        new_id = (new_id + 1) % MAX_USRBUF_CTX;
        if (new_id == factory->active_id) {
            LOG_ERROR(
                "[transcode_mt] %s: all %d usrbuf context are busy, new size %d!", __func__, MAX_USRBUF_CTX, new_size);
            return -1;
        }
        ctx = &factory->ctxs[new_id];
    }

    LOG_TRACE("[transcode_mt] %s: usrbuf context shift (ID %d -> %d, size %d -> %d)!", __func__, factory->active_id,
        new_id, ctx->buf_size, new_size);

    usrbuf_ctx_reset(ctx);
    ret = usrbuf_ctx_init(ctx, factory->type, new_id, size, factory->dev_id, factory->runtime_inst);
    if (ret < 0) {
        LOG_ERROR("[transcode_mt] %s: init usrbuf context failed, new size %d!", __func__, new_size);
        return ret;
    }

    factory->active_id = new_id;
    return 0;
}

static int usrbuf_factory_new_size(usrbuf_factory *factory, int new_size)
{
    int ret;
    pthread_mutex_lock(&factory->ctx_mutex);
    ret = usrbuf_factory_new_size_internal(factory, new_size);
    pthread_mutex_unlock(&factory->ctx_mutex);
    return ret;
}

static inline usrbuf *usrbuf_factory_request_buf_internal(usrbuf_factory *factory)
{
    usrbuf *buf = NULL;
    usrbuf_context *ctx = &factory->ctxs[factory->active_id];
    buf = usrbuf_ctx_request_buf(ctx);
    if (buf) {
        factory->request_number++;
    } else {
        LOG_ERROR("[transcode_mt] ERR: request %llu, return %llu!", (u64)factory->request_number,
            (u64)factory->return_number);
    }
    return buf;
}

static usrbuf *usrbuf_factory_request_buf(usrbuf_factory *factory)
{
    usrbuf *buf = NULL;
    pthread_mutex_lock(&factory->ctx_mutex);
    buf = usrbuf_factory_request_buf_internal(factory);
    pthread_mutex_unlock(&factory->ctx_mutex);
    return buf;
}

static usrbuf *usrbuf_factory_find_buf(usrbuf_factory *factory, uint64_t buf)
{
    int i;
    usrbuf *target = NULL;
    usrbuf_context *ctx;
    pthread_mutex_lock(&factory->ctx_mutex);
    for (i = 0; i < MAX_USRBUF_CTX; i++) {
        ctx = &factory->ctxs[i];
        target = usrbuf_ctx_find_buf(ctx, buf);
        if (target) {
            break;
        }
    }
    pthread_mutex_unlock(&factory->ctx_mutex);
    return target;
}

static int usrbuf_factory_return_buf(usrbuf_factory *factory, usrbuf *buf)
{
    int ret;
    usrbuf_context *ctx;
    pthread_mutex_lock(&factory->ctx_mutex);
    ctx = &factory->ctxs[buf->cid];
    ret = usrbuf_ctx_return_buf(ctx, buf);
    if (buf->cid != factory->active_id && usrbuf_ctx_idle(ctx)) {
        usrbuf_ctx_reset(ctx);
    }
    factory->return_number++;
    pthread_mutex_unlock(&factory->ctx_mutex);
    return ret;
}

static inline int usrbuf_factory_active_size(usrbuf_factory *factory)
{
    int size;
    pthread_mutex_lock(&factory->ctx_mutex);
    size = factory->ctxs[factory->active_id].buf_size;
    pthread_mutex_unlock(&factory->ctx_mutex);
    return size;
}

static uint64_t usrbuf_factory_request_buf_callback(void *opaque, int size)
{
    int new_size, i, ret;
    usrbuf *buf = NULL;
    usrbuf_context *ctx;
    usrbuf_factory *factory;
    factory = (usrbuf_factory *)opaque;
    new_size = NEXT_MULTIPLE(size, 4096);

    pthread_mutex_lock(&factory->ctx_mutex);
    for (i = 0; i < MAX_USRBUF_CTX; i++) {
        ctx = &factory->ctxs[i];
        if (new_size <= ctx->buf_size) {
            break;
        }
    }

    if (i < MAX_USRBUF_CTX) {
        buf = usrbuf_ctx_request_buf(ctx);
        if (!buf) {
            LOG_ERROR("[transcode_mt] %s:%d request buffer failed, size %d!", __func__, __LINE__, size);
        } else {
            factory->request_number++;
        }
    } else {
        ret = usrbuf_factory_new_size_internal(factory, size);
        if (!ret) {
            buf = usrbuf_factory_request_buf_internal(factory);
            if (!buf) {
                LOG_ERROR("[transcode_mt] %s:%d request buffer failed, size %d!", __func__, __LINE__, size);
            }
        } else {
            LOG_ERROR("[transcode_mt] %s: add new size failed, size %d!", __func__, size);
        }
    }
    pthread_mutex_unlock(&factory->ctx_mutex);
    if (buf) {
        return buf->bus_addr;
    }
    return 0;
}

static inline USR_BUF_TYPE usrbuf_factory_type(usrbuf_factory *factory)
{
    return factory->type;
}

static const char *UBTSTR[] = { "NA", "HOST", "FD", "DEV" };
#define UBT(f) UBTSTR[usrbuf_factory_type((usrbuf_factory *)f)]

static void set_video_params(enc_options *option, vmppEncChannelParameters *p_enc_params)
{
    p_enc_params->videoConfig.width = option->width;
    p_enc_params->videoConfig.height = option->height;
    p_enc_params->videoConfig.profile = (vmppVideoProfile)option->profile;
    p_enc_params->videoConfig.level = (vmppVideoLevel)option->level;
    if (p_enc_params->codecType == vmpp_CODEC_ENC_H264) {
        if (p_enc_params->videoConfig.profile < vmpp_VIDEO_PRFL_H264_BASELINE ||
            p_enc_params->videoConfig.profile > vmpp_VIDEO_PRFL_H264_HIGH_10) {
            p_enc_params->videoConfig.profile = vmpp_VIDEO_PRFL_H264_HIGH;
            p_enc_params->videoConfig.level = vmpp_VIDEO_LVL_H264_5_1;
        }
    } else if (p_enc_params->codecType == vmpp_CODEC_ENC_HEVC) {
        if (p_enc_params->videoConfig.profile < vmpp_VIDEO_PRFL_HEVC_MAIN ||
            p_enc_params->videoConfig.profile > vmpp_VIDEO_PRFL_HEVC_MAIN_REXT) {
            p_enc_params->videoConfig.profile = vmpp_VIDEO_PRFL_HEVC_MAIN;
            p_enc_params->videoConfig.level = vmpp_VIDEO_LVL_HEVC_6;
        }
    }
    if (p_enc_params->videoConfig.level == 0) {
        if (p_enc_params->codecType == vmpp_CODEC_ENC_H264) {
            p_enc_params->videoConfig.level = vmpp_VIDEO_LVL_H264_5_1;
        } else if (p_enc_params->codecType == vmpp_CODEC_ENC_HEVC) {
            p_enc_params->videoConfig.level = vmpp_VIDEO_LVL_HEVC_6;
        }
    }
    p_enc_params->videoConfig.frameRate.numerator = option->frameRateNum;
    p_enc_params->videoConfig.frameRate.denominator = option->frameRateDen;
    p_enc_params->videoConfig.bitDepthLuma = option->bitDepthLuma;
    if (p_enc_params->videoConfig.bitDepthLuma > 8) {
        if (p_enc_params->codecType == vmpp_CODEC_ENC_H264) {
            p_enc_params->videoConfig.profile = vmpp_VIDEO_PRFL_H264_HIGH_10;
            p_enc_params->videoConfig.level = vmpp_VIDEO_LVL_H264_5_1;
        } else if (p_enc_params->codecType == vmpp_CODEC_ENC_HEVC) {
            p_enc_params->videoConfig.profile = vmpp_VIDEO_PRFL_HEVC_MAIN_10;
            p_enc_params->videoConfig.level = vmpp_VIDEO_LVL_HEVC_6;
        }
    }
    p_enc_params->videoConfig.bitDepthChroma = option->bitDepthChroma;
    p_enc_params->outbufNum = option->outbufNum;
    p_enc_params->videoConfig.lookaheadDepth = option->lookaheadDepth;
    p_enc_params->videoConfig.tune = (vmppEncTuneType)option->tune;
    p_enc_params->videoConfig.keyInt = option->keyInt;
    p_enc_params->videoConfig.crf = option->crf;
    p_enc_params->videoConfig.cqp = option->cqp;
    p_enc_params->videoConfig.llRc = option->llRc;
    p_enc_params->videoConfig.bitRate = option->bitRate;
    p_enc_params->videoConfig.initQp = option->initQp;
    p_enc_params->videoConfig.vbvBufSize = option->vbvBufSize;
    p_enc_params->videoConfig.vbvMaxRate = option->vbvMaxRate;
    p_enc_params->videoConfig.intraQpDelta = option->intraQpDelta;
    p_enc_params->videoConfig.qpMinI = option->qpMinI;
    p_enc_params->videoConfig.qpMaxI = option->qpMaxI;
    p_enc_params->videoConfig.qpMinPB = option->qpMinPB;
    p_enc_params->videoConfig.qpMaxPB = option->qpMaxPB;
    p_enc_params->videoConfig.aqStrength = option->aqStrength;
    p_enc_params->videoConfig.qualityMode = (vmppEncQualityMode)option->qualityMode;
    p_enc_params->videoConfig.vbr = option->vbr;
    p_enc_params->videoConfig.gopSize = option->gopSize;
    p_enc_params->videoConfig.gdrDuration = option->gdrDuration;
    p_enc_params->videoConfig.P2B = option->P2B;
    p_enc_params->videoConfig.bBPyramid = option->bBPyramid;
    p_enc_params->videoConfig.maxFrameSizeMultiple = option->maxFrameSizeMultiple;
    p_enc_params->videoConfig.maxFrameSize = option->maxFrameSize;
    p_enc_params->videoConfig.roiType = (vmppEncROIType)option->roiType;
    p_enc_params->videoConfig.roiMapDeltaQpBlockUnit = option->roiMapDeltaQpBlockUnit;
    p_enc_params->videoConfig.roiMapQpDeltaVersion = option->roiMapQpDeltaVersion;
    p_enc_params->videoConfig.maxBFrames = option->maxBFrames;
    p_enc_params->videoConfig.hrd = option->hrd;
    p_enc_params->videoConfig.pictureSkip = option->pictureSkip;
    p_enc_params->videoConfig.vfr = option->vfr;
    p_enc_params->videoConfig.svcTLayers = option->svcTLayers;
    p_enc_params->videoConfig.sliceSize = option->sliceSize;
    p_enc_params->videoConfig.ltrInterval = option->ltrInterval;
    p_enc_params->videoConfig.ltrQpDelta = option->ltrQpDelta;
    p_enc_params->videoConfig.ltrRefGap = option->ltrRefGap;
    p_enc_params->videoConfig.preProcess.rotation = (vmppEncPictureRotation)option->rotation;
    p_enc_params->videoConfig.vbr = (option->coreID + (VMPP_SPECIFY_COREID << 16));
    p_enc_params->videoConfig.openGop = option->openGop;
    p_enc_params->videoConfig.smartEnc = option->smartEnc;
    p_enc_params->videoConfig.disableMMCO = option->disableMMCO;
    p_enc_params->videoConfig.preProcess.rotation = (vmppEncPictureRotation)option->rotation;
    p_enc_params->videoConfig.inLoopDSRatio = option->inLoopDSRatio;
    p_enc_params->videoConfig.aqMode = (vmppEncAQMode)option->aqMode;
    p_enc_params->videoConfig.psyFactor = option->psyFactor;
    p_enc_params->videoConfig.rdoLevel = option->rdoLevel;
    p_enc_params->videoConfig.enableRdoQuant = option->enableRdoQuant;
    p_enc_params->videoConfig.qCompress = option->qCompress;
    p_enc_params->videoConfig.rcMode = (vmppEncRcMode)option->rcMode;
    p_enc_params->videoConfig.enableOutputCuInfo = g_transcode_context.output_cu_info;
}

static void set_extparams(transcode_thread_params *args, vmppEncExtendedParams *extParams, uint64_t total_frames,
    enc_options *options, vmppFrame *frame, uint64_t total_frames_send)
{
    int roimap_value[3] = { 30, 10, -15 };
    int roimap_index = 0;
    uint32_t blksize = 0;
    uint32_t roiwidth, roiheight, roimap_size;
    int8_t *roi_map_delta_qp_buffer = NULL;

    int top = 0, left = 0, right = 0, bottom = 0;
    int qpType = 0, qpValue = 0;
    if (options->roiInt && options->roiParam) {
        sscanf(options->roiParam, "top=%d,left=%d,bottom=%d,right=%d,qpType=%d,qpValue=%d", &top, &left, &right,
            &bottom, &qpType, &qpValue);
        LOG_DEBUG("[transcode_mt %3d] top=%d,left=%d,bottom=%d,right=%d,qpType=%d,qpValue=%d", args->thread_index, top,
            left, right, bottom, qpType, qpValue);
    }
    switch (options->roiType) {
    case vmpp_ENC_ROI_RANGE:
        if (total_frames && options->roiInt && (total_frames % options->roiInt == 0)) {
            LOG_DEBUG("[transcode_mt %3d] (FNB %llu) update roi range, roiInt %u", args->thread_index,
                (u64)total_frames, options->roiInt);
            /* only effective when videoConfig.enableROI == 1 */
            // extParams.roiType = vmpp_ENC_ROI_RANGE;
            int i = total_frames % VMPP_ENC_MAX_ROI_NUM;
            extParams->roi[i].area.top = top;
            extParams->roi[i].area.left = left;
            extParams->roi[i].area.bottom = bottom;
            extParams->roi[i].area.right = right;
            extParams->roi[i].area.enable = 1;
            extParams->roi[i].qpType = (vmppEncQPType)qpType;
            extParams->roi[i].qpValue = qpValue;
        }
        break;
    case vmpp_ENC_ROI_MAP:
        LOG_TRACE("[transcode_mt %3d] (FNB %llu) update roi map", args->thread_index, (u64)total_frames);
        blksize = 64 >> (options->roiMapDeltaQpBlockUnit & 3);
        roiwidth = (frame->width + blksize - 1) / blksize;
        roiheight = (frame->height + blksize - 1) / blksize;
        roimap_size = roiwidth * roiheight;

        if (!roi_map_delta_qp_buffer) {
            roi_map_delta_qp_buffer = (int8_t *)malloc(roimap_size);
        }
        if (!roi_map_delta_qp_buffer) {
            LOG_ERROR("[transcode_mt %3d] roi_map_delta_qp_buffer malloc failed", args->thread_index);
            break;
        }
        // memset(roi_map_delta_qp_buffer, 0, roimap_size);
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

    /* test insert IDR in interval */
    if (total_frames_send && options->forceIDRInt && total_frames_send % options->forceIDRInt == 0) {
        LOG_DEBUG("[transcode_mt %3d] (FNB %llu) force IDR, forceIDRInt %u", args->thread_index, (u64)total_frames_send,
            options->forceIDRInt);
        extParams->forceIDR = 1;
    }

    if (options->ltrInsertTest != 0) {
        // test insert LTR
        if (total_frames_send && (total_frames_send % 18 == 0)) {
            LOG_DEBUG("[transcode_mt %3d] (FNB %llu) force LTR", args->thread_index, (u64)total_frames_send);
            extParams->forceLTR = 1;
        }
    }

    if (total_frames_send && (options->enableDynamicKeyInt == 1)) {
        if (total_frames_send == 118) {
            extParams->updateKeyInt = 60;
            extParams->updateTypeMask |= VMPP_ENC_UPDATE_KEYINT;
            LOG_DEBUG("[transcode_mt %3d] (FNB %llu) dynamic KeyInt enabled, set to %d", args->thread_index,
                (u64)total_frames_send, extParams->updateKeyInt);
        }
        if (total_frames_send == 410) {
            extParams->updateKeyInt = 120;
            extParams->updateTypeMask |= VMPP_ENC_UPDATE_KEYINT;
            LOG_DEBUG("[transcode_mt %3d] (FNB %llu) dynamic KeyInt enabled, set to %d", args->thread_index,
                (u64)total_frames_send, extParams->updateKeyInt);
        }
    }

    if (total_frames_send && (options->enableDynamicBitrate == 1) && (total_frames_send % 200 == 0)) {
        extParams->updateBitRate = (total_frames_send % 8000) * 1000;    // target bitrate, bps
        extParams->updateVbvBufSize = 0;                                 // vbvBufSize, bits, set to 0 if use default
        extParams->updateVbvMaxRate = 0;                                 // vbvMaxRate, bps, set to 0 if use default
        extParams->updateTypeMask |= VMPP_ENC_UPDATE_CBR;
        LOG_DEBUG("[transcode_mt %3d] (FNB %llu) update bitrate: %d bps, vbvBufSize %d, vbvMaxRate %d, mask 0x%x",
            args->thread_index, (u64)total_frames_send, extParams->updateBitRate, extParams->updateVbvBufSize,
            extParams->updateVbvMaxRate, extParams->updateTypeMask);
    }

    if (total_frames_send && (options->enableDynamicFrameRate == 1) && (total_frames_send % 300 == 0)) {
        extParams->updateFrameRate.numerator = 60;
        extParams->updateFrameRate.denominator = 1;
        extParams->updateTypeMask |= VMPP_ENC_UPDATE_FRAMERATE;
        LOG_DEBUG("[transcode_mt %3d] (FNB %llu) update frame rate: %d/%d fps, mask 0x%x", args->thread_index,
            (u64)total_frames_send, extParams->updateFrameRate.numerator, extParams->updateFrameRate.denominator,
            extParams->updateTypeMask);
    }

    if (total_frames_send && (options->enableDynamicCrf == 1) && (total_frames_send % 100 == 0)) {
        extParams->updateCrf = 10 + total_frames_send / 100;
        extParams->updateVbvBufSize = 0;
        extParams->updateVbvMaxRate = 0;
        extParams->updateTypeMask |= VMPP_ENC_UPDATE_CRF;
        LOG_DEBUG("[transcode_mt %3d] (FNB %llu) update crf: %d, vbvBufSize %d, vbvMaxRate %d, mask 0x%x",
            args->thread_index, (u64)total_frames_send, extParams->updateCrf, extParams->updateVbvBufSize,
            extParams->updateVbvMaxRate, extParams->updateTypeMask);
    }
}

static void set_default_enc_params(enc_options *enc_opts)
{
    enc_opts->encCodec = NULL;    // Disable encoder
    enc_opts->pixelFormat = vmpp_PIX_FMT_NONE;
    enc_opts->profile = vmpp_VIDEO_PRFL_HEVC_MAIN;
    enc_opts->level = vmpp_VIDEO_LVL_HEVC_6;
    enc_opts->gopSize = VMPP_ENC_DEFAULT_PAR;
    enc_opts->frameRateNum = 30;
    enc_opts->frameRateDen = 1;
    enc_opts->bitDepthLuma = 8;
    enc_opts->bitDepthChroma = 8;
    enc_opts->outbufNum = DEFAULT_OUT_BUF_NUM;
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
    enc_opts->roiParam = (char*)"top=0,left=0,bottom=200,right=200,qpType=0,qpValue=1";
    enc_opts->extSEIInt = 0;
    enc_opts->forceIDRInt = 0;
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
    enc_opts->rcMode = vmpp_ENC_RC_DEFAULT;
    enc_opts->multicore = 0;
}

static inline void do_fps_control(transcode_thread_params *args, uint64_t *timestamp, const uint64_t interval_ns)
{
    uint64_t temp = *timestamp;
    uint64_t target = temp + interval_ns;
    uint64_t current = gettime_ns();
    if (target > current) {
        const uint64_t udiff = target - current;
#ifdef _WIN32
        const uint64_t freq = clockfreq();
        const LONGLONG count_target = mul_div64(target, freq, NANOSEC_PER_SEC);

        LARGE_INTEGER count;
        QueryPerformanceCounter(&count);

        const bool stall = count.QuadPart < count_target;
        if (stall) {
            const DWORD milliseconds = (DWORD)(((count_target - count.QuadPart) * 1000.0) / freq);
            if (milliseconds > 1) {
                Sleep(milliseconds - 1);
            }

            for (;;) {
                QueryPerformanceCounter(&count);
                if (count.QuadPart >= count_target) {
                    break;
                }

                YieldProcessor();
            }
        }
#else
        sleep_ns(udiff);
#endif
        *timestamp = target;
        LOG_TRACE("[transcode_mt %3d] Sleep %lu(ns) to achieve the target FPS(%d)!", args->thread_index, udiff,
            g_transcode_context.target_fps);
    } else if (target < current) {
        const uint64_t udiff = current - temp;
        const uint64_t clamped_diff = (udiff > interval_ns) ? (uint64_t)udiff : interval_ns;
        int count = (int)(clamped_diff / interval_ns);
        *timestamp = temp + (interval_ns * count);
        LOG_DEBUG(
            "[transcode_mt %3d] Current FPS has fallen behind the target(%d), adjust control timestamp(+%d * interval_ns %lu)",
            args->thread_index, g_transcode_context.target_fps, count, interval_ns);
    }
}

static void generate_output_url(transcode_thread_params *args, char *url, const char *suffix)
{
    int url_offset = 0, out_index = 0;
    char *tmp = NULL;
    char *input_suffix = NULL;
    char *output_suffix = NULL;
    char *suffix_device = NULL;
    char file_name[MAX_PATH_LEN] = { 0 };
    char output_url[MAX_PATH_LEN * 2] = { 0 };
    char device_name[64] = { 0 };
    int width = 0, height = 0;
    if (strcmp(suffix, "yuv") == 0) {
        out_index = args->dec_out_index;
        width = args->dec_width;
        height = args->dec_height;
    } else {
        out_index = args->enc_out_index;
        width = args->current_enc_opts.width;
        height = args->current_enc_opts.height;
    }
    if (g_transcode_context.output_directory) {
        tmp = strrchr(args->current_url, '/');
        if (!tmp) {
            /* No '/' in current_url */
            tmp = args->current_url;
        } else {
            url_offset = 1;
        }

        input_suffix = strrchr(args->current_url, '.');
        if (input_suffix) {
            strncpy(file_name, tmp + url_offset, strlen(tmp) - strlen(input_suffix) - 1);
        } else {
            strncpy(file_name, tmp + url_offset, strlen(tmp));
        }

        suffix_device = strrchr(args->current_device, '/');
        strncpy(device_name, suffix_device + 1, strlen(suffix_device) - 1);

        strcpy(output_url, g_transcode_context.output_directory);
        if ('/' == g_transcode_context.output_directory[strlen(g_transcode_context.output_directory) - 1]) {
            sprintf(&output_url[strlen(g_transcode_context.output_directory)],
                "trans_mt_%03d_L%d_j%lld_l%d_%s_%s_%d_%dx%d.%s", args->thread_index, args->current_main_loop,
                (u64)args->job_index, args->current_loop, device_name, file_name, out_index, width, height, suffix);
        } else {
            sprintf(&output_url[strlen(g_transcode_context.output_directory)],
                "/trans_mt_%03d_L%d_j%lld_l%d_%s_%s_%d_%dx%d.%s", args->thread_index, args->current_main_loop,
                (u64)args->job_index, args->current_loop, device_name, file_name, out_index, width, height, suffix);
        }
        memcpy(url, output_url, MAX_PATH_LEN * 2);
    } else if (g_transcode_context.output_file) {
        if (out_index || args->job_index > 1 || strcmp(suffix, "txt") == 0) {
            tmp = strrchr(args->current_url, '/');
            if (!tmp) {
                /* No '/' in current_url */
                tmp = args->current_url;
            } else {
                url_offset = 1;
            }

            input_suffix = strrchr(args->current_url, '.');
            if (input_suffix) {
                strncpy(file_name, tmp + url_offset, strlen(tmp) - strlen(input_suffix) - 1);
            } else {
                strncpy(file_name, tmp + url_offset, strlen(tmp));
            }

            suffix_device = strrchr(args->current_device, '/');
            strncpy(device_name, suffix_device + 1, strlen(suffix_device) - 1);

            /* not the first file or the corresponding txt with output file */
            tmp = g_transcode_context.output_file;
            output_suffix = strrchr(tmp, '.');
            if (output_suffix) {
                // strncpy(output_url, tmp, strlen(tmp) - strlen(output_suffix));
                safe_strncpy(output_url, tmp, MAX_PATH_LEN * 2, strlen(tmp) - strlen(output_suffix));
            } else {
                // strncpy(output_url, tmp, strlen(tmp));
                safe_strncpy(output_url, tmp, MAX_PATH_LEN * 2, strlen(tmp));
            }

            if (strcmp(suffix, "txt") == 0) {
                if (!out_index && args->job_index == 1) {
                    /* first file */
                    sprintf(&output_url[strlen(output_url)], ".%s", suffix);
                } else {
                    sprintf(&output_url[strlen(output_url)], "_L%d_j%lld_l%d_%s_%s_%d_%dx%d.%s",
                        args->current_main_loop, (u64)args->job_index, args->current_loop, device_name, file_name,
                        out_index, width, height, suffix);
                }
            } else {
                if (output_suffix) {
                    output_suffix = &output_suffix[1];    // remove '.'
                    sprintf(&output_url[strlen(output_url)], "_L%d_j%lld_l%d_%s_%s_%d_%dx%d.%s",
                        args->current_main_loop, (u64)args->job_index, args->current_loop, device_name, file_name,
                        out_index, width, height, strcmp(suffix, "txt") == 0 ? suffix : output_suffix);
                } else {
                    /* No suffix */
                    sprintf(&output_url[strlen(output_url)], "_L%d_j%lld_l%d_%s_%s_%d_%dx%d", args->current_main_loop,
                        (u64)args->job_index, args->current_loop, device_name, file_name, out_index, width, height);
                }
            }

            memcpy(url, output_url, MAX_PATH_LEN * 2);
        } else {
            memcpy(url, g_transcode_context.output_file, strlen(g_transcode_context.output_file));
        }
    }

    LOG_DEBUG("[transcode_mt %3d] the generated output url: '%s'", args->thread_index, url);
}

static inline void return_usrbuf(transcode_thread_params *args, uint64_t usrbuf_value)
{
    usrbuf *ub = NULL;
    ub = usrbuf_factory_find_buf(&args->ubf, usrbuf_value);
    if (ub) {
        usrbuf_factory_return_buf(&args->ubf, ub);
    } else {
        LOG_ERROR("[transcode_mt %3d] can not find usr buffer: 0x%lx", args->thread_index, usrbuf_value);
    }
}

static int encoder_task_init(transcode_thread_params *args)
{
    int chns, i;
    char *suffix = NULL;
    vmppEncChannelParameters enc_params = { 0 };
    char output_file_str[MAX_PATH_LEN * 2] = { 0 };
    vmppCodecType codecType = vmpp_CODEC_ENC_HEVC;
    vmppResult ret = vmpp_RSLT_OK;
    enc_options *enc_opts = &args->current_enc_opts;

    if (strcmp(enc_opts->encCodec, "h264") == 0 || strcmp(enc_opts->encCodec, "avc") == 0) {
        codecType = vmpp_CODEC_ENC_H264;
        suffix = (char*)"h264";
    } else if (strcmp(enc_opts->encCodec, "hevc") == 0 || strcmp(enc_opts->encCodec, "h265") == 0) {
        codecType = vmpp_CODEC_ENC_HEVC;
        suffix = (char*)"hevc";
    } else if (strcmp(enc_opts->encCodec, "av1") == 0) {
        codecType = vmpp_CODEC_ENC_AV1;
        suffix = (char*)"ivf";
    } else if (strcmp(enc_opts->encCodec, "jpeg") == 0) {
        codecType = vmpp_CODEC_ENC_JPEG;
        suffix = (char*)"jpg";
    }

    args->dst_codec = codecType;

    if (g_transcode_context.save && suffix) {
        if (!args->enc_output_file) {
            generate_output_url(args, output_file_str, suffix);
            args->enc_output_file = fopen(output_file_str, "wb");
            if (!args->enc_output_file) {
                LOG_WARN("[transcode_mt %3d] Fail to open output file '%s' for encoder", args->thread_index,
                    output_file_str);
                args->output_error = 1;
            }
        } else if (!g_transcode_context.unique_output_file) {
            fflush(args->enc_output_file);
            fclose(args->enc_output_file);
            LOG_INFO("[transcode_mt %3d] Encode resolution changed to %dx%d, create a new output file.",
                args->thread_index, enc_opts->width, enc_opts->height);

            generate_output_url(args, output_file_str, suffix);
            args->enc_output_file = fopen(output_file_str, "wb");
            if (!args->enc_output_file) {
                LOG_WARN("[transcode_mt %3d] Fail to open output file '%s' for encoder", args->thread_index,
                    output_file_str);
                args->output_error = 1;
            }
        }
    }

    if (g_transcode_context.save_cu_info) {
        memset(output_file_str, 0, MAX_PATH_LEN * 2);
        if (!args->enc_cu_info_file) {
            generate_output_url(args, output_file_str, "txt");
            args->enc_cu_info_file = fopen(output_file_str, "wb");
            if (!args->enc_cu_info_file) {
                LOG_WARN("[transcode_mt %3d] Fail to open cuinfo output file '%s' for encoder", args->thread_index,
                    output_file_str);
                args->output_error = 1;
            }
        } else if (!g_transcode_context.unique_output_file) {
            fflush(args->enc_cu_info_file);
            fclose(args->enc_cu_info_file);
            LOG_INFO("[transcode_mt %3d] Encode resolution changed to %dx%d, create a new cuinfo output file.",
                args->thread_index, enc_opts->width, enc_opts->height);

            generate_output_url(args, output_file_str, "txt");
            args->enc_cu_info_file = fopen(output_file_str, "wb");
            if (!args->enc_cu_info_file) {
                LOG_WARN("[transcode_mt %3d] Fail to open output file '%s' for encoder", args->thread_index,
                    output_file_str);
                args->output_error = 1;
            }
        }
    }

    enc_params.device = args->enc_device_handle;
    enc_params.codecType = codecType;

    if (enc_params.codecType == vmpp_CODEC_ENC_JPEG) {
        enc_params.jpegConfig.frameType = enc_opts->pixelFormat;
        enc_params.jpegConfig.comLength = enc_opts->comLength;
        enc_params.jpegConfig.pCom = (uint8_t *)enc_opts->pCom;
        enc_params.jpegConfig.codingWidth = enc_opts->width;
        enc_params.jpegConfig.codingHeight = enc_opts->height;
        enc_params.jpegConfig.qLevel = enc_opts->qLevel;
        if (enc_params.jpegConfig.qLevel == USER_DEFINED_QTABLE) {
            for (i = 0; i < 64; i++) {
                enc_params.jpegConfig.qTableLuma[i] = qTable[i];
                enc_params.jpegConfig.qTableChroma[i] = qTable[i];
            }
        }
    } else {
        set_video_params(enc_opts, &enc_params);
    }

    enc_params.enProfiling = !g_transcode_context.disable_enc_profiling;

    LOG_INFO("The thread_index %d encoder is %s on device(%s <%d>) ", args->thread_index, codec[enc_params.codecType - 94], args->current_device, args->enc_device_handle);
    ret = vmppEncCreateChannel(&args->enc_ch, &enc_params);
    if (ret != vmpp_RSLT_OK) {
        LOG_ERROR("[transcode_mt %3d] create channel error %d.", args->thread_index, ret);
        return -1;
    }

    LOG_DEBUG("[transcode_mt %3d] enc channel: %p for '%s' created.", args->thread_index, args->enc_ch,
        CODEC(enc_params.codecType));
    return 0;
}

static void update_psnr(transcode_thread_params *args, vmppStream *pStream, enc_options *options)
{
    int lum_max_value, cbcr_max_value;
    double y_psnr, cb_psnr, cr_psnr;
    if (pStream->psnrInfo[0] && pStream->psnrInfo[1] && pStream->psnrInfo[2]) {
        lum_max_value = (1 << options->bitDepthLuma) - 1;
        cbcr_max_value = (1 << options->bitDepthChroma) - 1;

        y_psnr = 10.0 * log10f(lum_max_value * lum_max_value / pStream->psnrInfo[0]);
        cb_psnr = 10.0 * log10f(cbcr_max_value * cbcr_max_value / pStream->psnrInfo[1]);
        cr_psnr = 10.0 * log10f(cbcr_max_value * cbcr_max_value / pStream->psnrInfo[2]);

        args->psnr_total[0] += y_psnr;
        args->psnr_total[1] += cb_psnr;
        args->psnr_total[2] += cr_psnr;
        LOG_TRACE("[transcode_mt %3d] MSE %4.2f %4.2f %4.2f", args->thread_index, pStream->psnrInfo[0],
            pStream->psnrInfo[1], pStream->psnrInfo[2]);
        LOG_TRACE("[transcode_mt %3d] psnr_total %4.2f %4.2f %4.2f", args->thread_index, args->psnr_total[0],
            args->psnr_total[1], args->psnr_total[2]);

        args->psnr_num++;
    }
}

static void update_ssim(transcode_thread_params *args, vmppStream *pStream)
{
    if (pStream->psnrInfo[3] && pStream->psnrInfo[4] && pStream->psnrInfo[5]) {
        args->ssim_total[0] += pStream->psnrInfo[3];
        args->ssim_total[1] += pStream->psnrInfo[4];
        args->ssim_total[2] += pStream->psnrInfo[5];
        LOG_TRACE("[transcode_mt %3d] SSIM %4.2f %4.2f %4.2f", args->thread_index, pStream->psnrInfo[3],
            pStream->psnrInfo[4], pStream->psnrInfo[5]);
        LOG_TRACE("[transcode_mt %3d] ssim_total %4.2f %4.2f %4.2f", args->thread_index, args->ssim_total[0],
            args->ssim_total[1], args->ssim_total[2]);

        args->ssim_num++;
    }
}

static void parse_and_save_cuinfo(transcode_thread_params *args, vmppStream *out_stream, uint64_t stream_number)
{
    if (!out_stream) {
        return;
    }
    vmppResult ret = vmpp_RSLT_OK;
    vmppEncOutData *encOutData = &out_stream->encOutData;
    uint32_t iCu = 0, cuinfo_buf_size;
    vmppEncCuInfo *curCuInfo = NULL;
    vmppEncOutInfo outInfo = { 0 };
    uint64_t timer_start, timer_tick;

    if (!encOutData->cuData || !encOutData->cuDataTotalSize) {
        LOG_TRACE("[transcode_mt %3d] No CuInfo in current stream(END_SEQUENCE or JPEG stream)", args->thread_index);
        return;
    }

    cuinfo_buf_size = encOutData->maxCuNum * sizeof(vmppEncCuInfo);
    if (!args->enc_cu_info_buffer || args->enc_cu_info_buffer_size < cuinfo_buf_size) {
        if (args->enc_cu_info_buffer) {
            LOG_DEBUG("[transcode_mt %3d] realloc memory for cu info:%p, %d->%d", args->thread_index,
                args->enc_cu_info_buffer, args->enc_cu_info_buffer_size, cuinfo_buf_size);
            free(args->enc_cu_info_buffer);
            args->enc_cu_info_buffer = NULL;
            args->enc_cu_info_buffer_size = 0;
        }

        args->enc_cu_info_buffer = malloc(cuinfo_buf_size);
        if (!args->enc_cu_info_buffer) {
            LOG_ERROR("[transcode_mt %3d] malloc memory for cuInfo failed: maxCuNum %d", args->thread_index,
                encOutData->maxCuNum);
            args->output_error = 1;
            return;
        }
        args->enc_cu_info_buffer_size = cuinfo_buf_size;
    }

    outInfo.cuInfo = (vmppEncCuInfo *)args->enc_cu_info_buffer;

    timer_start = gettime_ns();

    ret = vmppEncParseCuInformation(args->enc_ch, encOutData, &outInfo);
    if (ret) {
        LOG_ERROR("[transcode_mt %3d] vmppEncParseCuInformation failed: err %d", args->thread_index, ret);
        args->enc_error = 1;
        return;
    }

    timer_tick = gettime_ns();
    args->cu_info_parse_time += (timer_tick - timer_start);

    LOG_TRACE("[transcode_mt %3d] vmppEncParseCuInformation(%llu): cu[v%d, %p, %d] -> "
              "totalCuNum %u, frameSatd %u intraMBCount %u, interMBCount %u, averageMVX %d, averageMVY %d",
        args->thread_index, (u64)stream_number, encOutData->cuInfoVersion, encOutData->cuData,
        encOutData->cuDataTotalSize, outInfo.totalCuNum, outInfo.frameSatd, outInfo.interMBCount, outInfo.intraMBCount,
        outInfo.averageMVX, outInfo.averageMVY);

    if (args->enc_cu_info_file && g_transcode_context.save_cu_info) {
        fprintf(args->enc_cu_info_file, "\n#Pic frameNum %ld, frameType %s, frameAvgQP %d.\n", stream_number,
            FRAME(encOutData->frameType), encOutData->frameAvgQP);

        for (iCu = 0; iCu < outInfo.totalCuNum; iCu++) {
            curCuInfo = &outInfo.cuInfo[iCu];
            fprintf(args->enc_cu_info_file, " iCu %-4d %-2dx%-2d at (%2d,%2d) %-5s", iCu, curCuInfo->cuSize,
                curCuInfo->cuSize, curCuInfo->cuLocationX, curCuInfo->cuLocationY, cuType[curCuInfo->cuMode]);

            if (curCuInfo->cuMode == 1) {
                fprintf(args->enc_cu_info_file, " costIntraSatd[%8d] costInterSatd[%8d] \n", curCuInfo->costIntraSatd,
                    curCuInfo->costInterSatd);
            } else if (curCuInfo->cuMode == 0) {
                fprintf(args->enc_cu_info_file, " costIntraSatd[%8d] costInterSatd[%8d] interDir[%7s] ",
                    curCuInfo->costIntraSatd, curCuInfo->costInterSatd, interDir[curCuInfo->interPredIdc]);
                if (curCuInfo->interPredIdc != 1) {
                    fprintf(args->enc_cu_info_file, " mv0.refIdx[%2d] mv0.mvX[%5d] mv0.mvY[%5d]",
                        curCuInfo->mv[0].refIdx, curCuInfo->mv[0].mvX, curCuInfo->mv[0].mvY);
                }
                if (curCuInfo->interPredIdc != 0) {
                    fprintf(args->enc_cu_info_file, " mv1.refIdx[%2d] mv1.mvX[%5d] mv1.mvY[%5d]",
                        curCuInfo->mv[1].refIdx, curCuInfo->mv[1].mvX, curCuInfo->mv[1].mvY);
                }
                fprintf(args->enc_cu_info_file, "\n");
            }
        }
        fprintf(args->enc_cu_info_file,
            " totalCuNum=%d frameSatd=%d intraMBcount=%d interMBCount=%d avgMVX=%d avgMVY=%d\n", outInfo.totalCuNum,
            outInfo.frameSatd, outInfo.intraMBCount, outInfo.interMBCount, outInfo.averageMVX, outInfo.averageMVY);
        fflush(args->enc_cu_info_file);

        args->cu_info_save_time += (gettime_ns() - timer_tick);
    }


    if (stream_number && stream_number % g_transcode_context.period == 0) {
        LOG_INFO("[transcode_mt %3d] CuInfo Performance: parse[%lu us]/save[%lu us] per frame for total %lu frames",
            args->thread_index, args->cu_info_parse_time / stream_number / NANOSEC_PER_MICROSEC,
            args->cu_info_save_time / stream_number / NANOSEC_PER_MICROSEC, stream_number);
    }
}

static unsigned char *compute_md5sum(
    transcode_thread_params *args, const unsigned char *buf_in, const int buf_len, unsigned char *str_md5sum);
static void *enc_output_thread(void *params)
{
    uint64_t timer_tick;
    vmppStream *strm;
    transcode_thread_params *args = (transcode_thread_params *)params;
    enc_options *enc_opts = &args->current_enc_opts;
    do {
        pthread_mutex_lock(&args->stream_mutex);
        strm = (vmppStream *)vmpp_queue_pop_front(args->stream_queue);
        pthread_mutex_unlock(&args->stream_mutex);
        if (strm) {
            if (args->dst_codec == vmpp_CODEC_ENC_HEVC || args->dst_codec == vmpp_CODEC_ENC_H264) {
                /* ignore the END_SEQUENCE of HEVC/H264 */
                args->enc_cu_info_number += (strm->encedNals.cnt ? 1 : 0);
            } else {
                args->enc_cu_info_number++;
            }

            /* check md5. */
            if (g_transcode_context.check_md5) {
                timer_tick = gettime_ns();
                compute_md5sum(args, (const unsigned char *)strm->stream, strm->len, args->cur_md5sum);
                args->check_md5_time += (gettime_ns() - timer_tick);
            }
            if (args->enc_output_file && g_transcode_context.save &&
                (enc_opts->svcExtractMaxTLayer == VMPP_ENC_DEFAULT_PAR ||
                    strm->svcTemporalId <= enc_opts->svcExtractMaxTLayer)) {
                timer_tick = gettime_ns();
                fwrite(strm->stream, 1, strm->len, args->enc_output_file);
                args->enc_save_time += (gettime_ns() - timer_tick);
            }

            if (g_transcode_context.parse_cu_info) {
                parse_and_save_cuinfo(args, strm, args->enc_cu_info_number);
            }

            LOG_TRACE("[transcode_mt %3d] vmppEncReleaseStream: stream %p", args->thread_index, strm->stream);
            vmppEncReleaseStream(args->enc_ch, strm);

            pthread_mutex_lock(&args->stream_mutex);
            vmpp_queue_push_back(args->idle_stream_queue, strm);
            pthread_mutex_unlock(&args->stream_mutex);
        } else if (args->enc_error || args->enc_stop) {
            LOG_DEBUG("[transcode_mt %3d] exit enc output thread, args->enc_error(%d), args->enc_stop(%lld)",
                args->thread_index, args->enc_error, (u64)args->enc_stop);
            break;
        } else {
            if (args->wait_process_stream) {
                args->wait_process_stream = 0;
            }
            usleep(10);
        }
    } while (1);
    args->wait_process_stream = 0;
    return NULL;
}


static inline bool force_flush_encoder_finished(transcode_thread_params *args)
{
    return (args->force_flush_encoder == 0);
}

static inline bool wait_flush_decoder_finished(transcode_thread_params *args)
{
    return (args->wait_flush_decoder == 0);
}

static inline bool wait_process_stream_finished(transcode_thread_params *args)
{
    return (args->wait_process_stream == 0);
}

static inline void sync_threads(
    transcode_thread_params *args, int sync_stream, bool (*break_condition_cb)(transcode_thread_params *))
{
    int queued_numer = 0, max_number = 0;
    uint64_t time_gap, timer_tick = gettime_ns();
    do {
        if (args->encoder_inited) {
            if (sync_stream) {
                max_number = g_transcode_context.max_queued_stream;
                pthread_mutex_lock(&args->stream_mutex);
                queued_numer = vmpp_queue_size(args->stream_queue);
                pthread_mutex_unlock(&args->stream_mutex);
            } else {
                max_number = g_transcode_context.max_queued_frame;
                pthread_mutex_lock(&args->frame_mutex);
                queued_numer = vmpp_queue_size(args->frame_queue);
                pthread_mutex_unlock(&args->frame_mutex);
            }
        }

        if (break_condition_cb) {
            if (!queued_numer && break_condition_cb(args)) {
                break;
            }
        } else if (queued_numer <= max_number) {
            break;
        }
        usleep(1);
        time_gap = (gettime_ns() - timer_tick);
        if (!break_condition_cb && time_gap > 30 * NANOSEC_PER_SEC) {
            LOG_WARN("[transcode_mt %3d] sync thread timeout, sync for stream? %d, queued_numer %d!",
                args->thread_index, sync_stream, queued_numer);
            break;
        }

        if (args->enc_error || args->dec_error) {
            LOG_WARN("[transcode_mt %3d] exit thread sync due to dec(%d)/enc(%d) error!", args->thread_index,
                args->dec_error, args->enc_error);
            break;
        }
    } while (1);
}

static inline bool frames_on_device(void)
{
    return ((g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_DEV ||
                g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_AS_HWOUT ||
                (g_transcode_context.memory_mode == vmpp_DEC_MEM_NORMAL &&
                    g_transcode_context.real_recv_mem_type == vmpp_MEM_DEVICE)) &&
            !g_transcode_context.encode_yuv);
}

static inline void release_host_frames(transcode_thread_params *args, vmppFrame *frame)
{
    if (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_HOST) {
        LOG_TRACE("[transcode_mt %3d] return usrbuf(HOST): %p.", args->thread_index, frame->data[0]);
        return_usrbuf(args, (uint64_t)frame->data[0]);
        frame->data[0] = NULL;
    } else if (!g_transcode_context.encode_yuv) {
        LOG_TRACE("[transcode_mt %3d] vmppDecReleaseFrame DIRECTLY HOST, privateData %p.", args->thread_index,
            frame->privateData);
        vmppDecReleaseFrame(args->dec_ch, frame, 500);
        frame->data[0] = NULL;
    }
    pthread_mutex_lock(&args->frame_mutex);
    vmpp_queue_push_back(args->idle_frame_queue, frame);
    pthread_mutex_unlock(&args->frame_mutex);
}

static inline void release_device_frame(transcode_thread_params *args, vmppFrame *frame, const char *description)
{
    if (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_DEV ||
        g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_AS_HWOUT) {
        LOG_TRACE("[transcode_mt %3d] return usrbuf(DEVMEM, 0x%llx) - desc: %s.", args->thread_index,
            (u64)frame->busAddress[0], description);
        return_usrbuf(args, (uint64_t)frame->busAddress[0]);
    } else {
        LOG_TRACE("[transcode_mt %3d] vmppDecReleaseFrame, privateData %p - desc: %s.", args->thread_index,
            frame->privateData, description);
        vmppDecReleaseFrame(args->dec_ch, frame, 500);
    }
    frame->busAddress[0] = (vmppDevAddr)NULL;
    pthread_mutex_lock(&args->frame_mutex);
    vmpp_queue_push_back(args->idle_frame_queue, frame);
    pthread_mutex_unlock(&args->frame_mutex);
}

static void handle_enc_recovery(transcode_thread_params *args, vmppFrame *frame)
{
    vmppFrame *tmp = NULL;
    if (frames_on_device()) {
        LOG_INFO("[transcode_mt %3d] try to re-encode current frame & %d frames in releasing queue.",
            args->thread_index, vmpp_queue_size(args->releasing_frame_queue));
        pthread_mutex_lock(&args->frame_mutex);
        if (frame->memoryType != vmpp_MEM_FLUSH) {
            vmpp_queue_insert_front(args->frame_queue, frame);
            LOG_DEBUG("[transcode_mt %3d] insert current frame(0x%llx, busAddr 0x%llx) into frame queue(%d).",
                args->thread_index, (u64)frame, (u64)frame->busAddress[0], vmpp_queue_size(args->frame_queue));
        }
        do {
            tmp = (vmppFrame *)vmpp_queue_pop_tail(args->releasing_frame_queue);
            if (!tmp) {
                break;
            }
            vmpp_queue_insert_front(args->frame_queue, tmp);
            LOG_DEBUG("[transcode_mt %3d] pop the tail frame(0x%llx, busAddr 0x%llx) from releaseing queue(%d)"
                      " and insert it into frame queue(%d).",
                args->thread_index, (u64)tmp, (u64)tmp->busAddress[0], vmpp_queue_size(args->releasing_frame_queue),
                vmpp_queue_size(args->frame_queue));
        } while (1);
        pthread_mutex_unlock(&args->frame_mutex);
    } else {
        if (frame->memoryType != vmpp_MEM_FLUSH) {
            pthread_mutex_lock(&args->frame_mutex);
            vmpp_queue_insert_front(args->frame_queue, frame);
            LOG_DEBUG("[transcode_mt %3d] insert current frame(0x%llx, busAddr 0x%llx) into frame queue(%d).",
                args->thread_index, (u64)frame, (u64)frame->busAddress[0], vmpp_queue_size(args->frame_queue));
            pthread_mutex_unlock(&args->frame_mutex);
        }
        LOG_WARN("[transcode_mt %3d] we may lost the frames that have been sent to encoder but not outputed yet!",
            args->thread_index);
    }
}

static inline void get_nal_types(vmppStream *stream, char nal_types[])
{
    int i, offset = 0;
    char *p;
    if (stream->encedNals.cnt) {
        sprintf(nal_types, "%s", NAL(stream->encedNals.nals[0]));
        offset += strlen(NAL(stream->encedNals.nals[0]));
        for (i = 1; i < (int)stream->encedNals.cnt; i++) {
            p = &nal_types[offset];
            sprintf(p, ";%s", NAL(stream->encedNals.nals[i]));
            offset += (1 + strlen(NAL(stream->encedNals.nals[i])));
            p = &nal_types[offset];
        }
    } else {
        sprintf(nal_types, "%s", NAL(vmpp_NAL_NONE));
    }
}

static inline void collect_enc_latency(transcode_thread_params *args, uint64_t timer_tick, int *p_first_frame)
{
    uint64_t time_gap = (gettime_ns() - timer_tick) / NANOSEC_PER_MICROSEC;
    args->total_latency_us += time_gap;
    if (time_gap > args->max_latency_us) {
        args->max_latency_us = time_gap;
    }
    if (*p_first_frame) {
        args->min_latency_us = time_gap;
        *p_first_frame = 0;
    }
    if (time_gap && time_gap < args->min_latency_us) {
        args->min_latency_us = time_gap;
    }
    args->latency_count++;

    LOG_TRACE(
        "[transcode_mt %3d] Enc Latency Info(%llu): current %llu us, average %llu us/f, max %llu us, min %llu us.",
        args->thread_index, (u64)args->latency_count, (u64)time_gap, (u64)args->total_latency_us / args->latency_count,
        (u64)args->max_latency_us, (u64)args->min_latency_us);
}

static void additional_frame_releasing(transcode_thread_params *args)
{
    vmppFrame *tmp;
    int i;
    pthread_mutex_lock(&args->frame_mutex);
    /* These releasing steps are additional protection used for releasing decode frames,
     * should NEVER be triggered at all(EXCEPT idle frame queue for yuv encoder) */
    for (i = 0; i < vmpp_queue_size(args->releasing_frame_queue); i++) {
        tmp = (vmppFrame *)vmpp_queue_peek(args->releasing_frame_queue, i);
        if (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_DEV ||
            g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_AS_HWOUT) {
            if (tmp->busAddress[0]) {
                LOG_WARN("[transcode_mt %3d] return usrbuf(DEVMEM) E REL: 0x%llx.", args->thread_index,
                    (u64)tmp->busAddress[0]);
                return_usrbuf(args, (uint64_t)tmp->busAddress[0]);
                tmp->busAddress[0] = (vmppDevAddr)NULL;
            }
        } else {
            LOG_WARN("[transcode_mt %3d] vmppDecReleaseFrame REL, busAddr 0x%llx, privateData %p.", args->thread_index,
                (u64)tmp->busAddress[0], tmp->privateData);
            vmppDecReleaseFrame(args->dec_ch, tmp, 500);
        }
    }

    for (i = 0; i < vmpp_queue_size(args->frame_queue); i++) {
        tmp = (vmppFrame *)vmpp_queue_peek(args->frame_queue, i);
        if (g_transcode_context.encode_yuv || g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_HOST) {
            LOG_WARN("[transcode_mt %3d] return usrbuf(HOST) left behind in frame queue: %p", args->thread_index,
                tmp->data[0]);
            return_usrbuf(args, (uint64_t)tmp->data[0]);
            tmp->data[0] = NULL;
            /* no decoder active or dec frame is already released */
        } else if (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_DEV ||
                   g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_AS_HWOUT) {
            LOG_WARN(
                "[transcode_mt %3d] return usrbuf(DEVMEM) E FRM: 0x%llx.", args->thread_index, (u64)tmp->busAddress[0]);
            return_usrbuf(args, (uint64_t)tmp->busAddress[0]);
            tmp->busAddress[0] = (vmppDevAddr)NULL;
            /* dec frame is already released */
        } else {
            LOG_WARN("[transcode_mt %3d] vmppDecReleaseFrame FRM, busAddr 0x%llx, privateData %p.", args->thread_index,
                (u64)tmp->busAddress[0], tmp->privateData);
            vmppDecReleaseFrame(args->dec_ch, tmp, 500);
        }
    }

    for (i = 0; i < vmpp_queue_size(args->idle_frame_queue); i++) {
        tmp = (vmppFrame *)vmpp_queue_peek(args->idle_frame_queue, i);
        if (g_transcode_context.encode_yuv || g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_HOST) {
            if (tmp->data[0]) {
                if (g_transcode_context.encode_yuv) {
                    /* Expected Results */
                    LOG_TRACE("[transcode_mt %3d] return usrbuf(HOST) for YUV in idle frame queue: %p",
                        args->thread_index, tmp->data[0]);
                } else {
                    LOG_WARN("[transcode_mt %3d] return usrbuf(HOST) left behind in idle frame queue: %p",
                        args->thread_index, tmp->data[0]);
                }
                return_usrbuf(args, (uint64_t)tmp->data[0]);
                tmp->data[0] = NULL;
            }
        } else if (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_DEV ||
                   g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_AS_HWOUT) {
            if (tmp->busAddress[0]) {
                LOG_WARN("[transcode_mt %3d] return usrbuf(DEVMEM) E IDLE: 0x%llx.", args->thread_index,
                    (u64)tmp->busAddress[0]);
                return_usrbuf(args, (uint64_t)tmp->busAddress[0]);
                tmp->busAddress[0] = (vmppDevAddr)NULL;
            }
        }
    }
    pthread_mutex_unlock(&args->frame_mutex);
}

static vmppSEI **prepare_sei_data(transcode_thread_params *args, uint32_t *p_sei_count)
{
    uint32_t sei_count = *p_sei_count;
    vmppSEI **sei_array;
    sei_array = (vmppSEI **)malloc(sizeof(vmppSEI *) * sei_count);
    if (sei_array) {
        do {
            sei_array[0] = (vmppSEI *)malloc(sizeof(vmppSEI));
            if (sei_array[0]) {
                sei_array[0]->nalType = vmpp_SEI_PREFIX;
                sei_array[0]->payloadType = SEI_USER_DATA_UNREGISTERED;
                sei_array[0]->payloadData =
                    (uint8_t *)"0123456789ABCDEF-01234567890123456789012345678901234567890-index0";
                sei_array[0]->payloadDataSize = strlen((char *)sei_array[0]->payloadData);
            } else {
                LOG_WARN("[transcode_mt %3d] malloc buffer for SEI[0/%d] failed!", args->thread_index, sei_count);
                free(sei_array);
                sei_array = NULL;
                sei_count = 0;
                break;
            }
            sei_array[1] = (vmppSEI *)malloc(sizeof(vmppSEI));
            if (sei_array[1]) {
                sei_array[1]->nalType = vmpp_SEI_PREFIX;
                sei_array[1]->payloadType = SEI_USER_DATA_UNREGISTERED;
                sei_array[1]->payloadData =
                    (uint8_t *)"0123456789ABCDEF-01234567890123456789012345678901234567890-ABCindex1";
                sei_array[1]->payloadDataSize = strlen((char *)sei_array[1]->payloadData);
            } else {
                LOG_WARN("[transcode_mt %3d] malloc buffer for SEI[1/%d] failed!", args->thread_index, sei_count);
                free(sei_array[0]);
                free(sei_array);
                sei_array = NULL;
                sei_count = 0;
            }
        } while (0);
    } else {
        LOG_WARN("[transcode_mt %3d] malloc buffer for SEI array failed, count %d!", args->thread_index, sei_count);
        sei_count = 0;
    }
    *p_sei_count = sei_count;
    return sei_array;
}

static inline void clear_sei_data(vmppSEI **sei_array, uint32_t sei_count)
{
    uint32_t i;
    for (i = 0; i < sei_count; i++) {
        if (sei_array[i]) {
            free(sei_array[i]);
            sei_array[i] = NULL;
        }
    }
    free(sei_array);
}

static void check_enc_params(transcode_thread_params *args, enc_options *opts, vmppFrame *frame);
static void *enc_thread(void *params)
{
    transcode_thread_params *args = (transcode_thread_params *)params;
    vmppFrame *frame = NULL, *standbyFrame = NULL, *tmp = NULL, flushFrame;
    vmppStream stream, *tmp_stream = NULL;
    vmppEncExtendedParams extParams = { 0 };
    int i = 0, res_changing = 0, res_changed = 0, tmp_ret = 0, error_cnt = 0, first_frame = 1, actual_frame = 0;
    vmppResult enc_ret = vmpp_RSLT_OK;
    uint64_t total_frames_send = 0, timer_tick, time_gap, fps_control_timestamp, interval_ns = 0;
    uint32_t sei_count = 2;
    vmppSEI **sei_array = NULL;
    enc_options *enc_opts = &args->current_enc_opts;

    if (g_transcode_context.encode_yuv && g_transcode_context.target_fps) {
        /* For other cases, the FPS control will be executed in decoding thread */
        interval_ns = NANOSEC_PER_SEC / g_transcode_context.target_fps;
    }
    memset(&flushFrame, 0, sizeof(vmppFrame));
    flushFrame.memoryType = vmpp_MEM_FLUSH;
    args->enc_total_frames = 0;

    if (enc_opts->extSEIInt) {
        LOG_DEBUG("[transcode_mt %3d] prepare data for SEI: extSEIInt %d, count %d!", args->thread_index,
            enc_opts->extSEIInt, sei_count);
        sei_array = prepare_sei_data(args, &sei_count);
    }

    fps_control_timestamp = args->enc_tick = args->enc_start = gettime_ns();
    do {
        /* handle dynamic resolution */
        if (!res_changing) {
            pthread_mutex_lock(&args->frame_mutex);
            frame = (vmppFrame *)vmpp_queue_pop_front(args->frame_queue);
            if (frame && !g_transcode_context.encode_yuv &&
                (frame->cropInfo.width != (uint32_t)enc_opts->width ||
                    frame->cropInfo.height != (uint32_t)enc_opts->height)) {
                LOG_INFO("[transcode_mt %3d] resolution changed from %dx%d to %dx%d, do flushing", args->thread_index,
                    enc_opts->width, enc_opts->height, frame->cropInfo.width, frame->cropInfo.height);
                res_changing = 1;
                res_changed = 0;
                standbyFrame = frame;
                frame = NULL;
            }
            pthread_mutex_unlock(&args->frame_mutex);
        } else {
            if (res_changed) {
                LOG_INFO("[transcode_mt %3d] flushing for resolution change finished, re-create enc channel",
                    args->thread_index);
                if (g_transcode_context.another_thread_for_enc_out) {
                    args->wait_process_stream = 1;
                    sync_threads(args, 1, wait_process_stream_finished);
                }
                if (args->enc_ch) {
                    LOG_DEBUG("[transcode_mt %3d] destroy enc channel %p", args->thread_index, args->enc_ch);
                    enc_ret = vmppEncDestroyChannel(&args->enc_ch);
                    if (enc_ret < 0) {
                        LOG_ERROR("[transcode_mt %3d] destroy enc chn error %d", args->thread_index, enc_ret);
                        args->enc_error = 1;
                        break;
                    }
                }

                args->enc_out_index++;
                check_enc_params(args, enc_opts, standbyFrame);
                tmp_ret = encoder_task_init(args);
                if (tmp_ret != 0) {
                    LOG_ERROR("[transcode_mt %3d] fail to re-init encoder after resolution changed %d",
                        args->thread_index, tmp_ret);
                    args->enc_error = 1;
                    break;
                }
                frame = standbyFrame;
                res_changing = 0;
            } else {
                frame = NULL;
            }
        }

        if (!frame) {
            if (args->force_flush_encoder) {
                frame = &flushFrame;
            } else if (!args->dec_finished && !res_changing) {
                usleep(1000);
                continue;
            } else {
                frame = &flushFrame;
            }
        }

        /* setup frame */
        memset(&stream, 0, sizeof(vmppStream));
        memset(&extParams, 0, sizeof(vmppEncExtendedParams));
        if (frame->memoryType != vmpp_MEM_FLUSH) {
            set_extparams(args, &extParams, args->enc_total_frames, enc_opts, frame, total_frames_send);
            frame->timebase.denominator = 0;
            frame->timebase.numerator = 0;
            if (enc_opts->vfr) {
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
                LOG_TRACE("[transcode_mt %3d] (FNB %llu) fps %d", args->thread_index, (u64)total_frames_send,
                    frame->timebase.numerator);
            }
            if (enc_opts->extSEIInt && args->enc_total_frames && args->enc_total_frames % enc_opts->extSEIInt == 0) {
                frame->seiCount = sei_count;
                frame->seiData = sei_array;
                LOG_DEBUG("[transcode_mt %3d] (FNB %llu) set sei, extSEIInt %d", args->thread_index,
                    (u64)args->enc_total_frames, enc_opts->extSEIInt);
            } else {
                frame->seiCount = 0;
            }
        } else {
            LOG_TRACE("[transcode_mt %3d] (FNB %llu) using flush frame.", args->thread_index, (u64)total_frames_send);
        }

        /* encode */
        timer_tick = gettime_ns();
        enc_ret = vmppEncEncodeFrame(args->enc_ch, frame, &extParams, &stream, DEFAULT_TIMEOUT_MS);
        if (enc_ret == vmpp_RSLT_ERR_ENC_RECOVERY) {
            time_gap = (gettime_ns() - timer_tick) / NANOSEC_PER_MILLISEC;
            LOG_WARN("[transcode_mt %3d] enc channel has already recoveried from a serious error, cost %lu ms.",
                args->thread_index, time_gap);
            handle_enc_recovery(args, frame);
            continue;
        }

        if (frame->memoryType == vmpp_MEM_HOST) {
            /* release host frames anyway */
            release_host_frames(args, frame);
        }

        if (enc_ret == vmpp_RSLT_OK) {
            char nal_types[256] = { 0 };
            total_frames_send++;
            if (args->dst_codec == vmpp_CODEC_ENC_HEVC || args->dst_codec == vmpp_CODEC_ENC_H264) {
                /* ignore the END_SEQUENCE of HEVC/H264 */
                actual_frame = (stream.encedNals.cnt ? 1 : 0);
            } else {
                actual_frame = 1;
            }

            args->enc_total_frames += actual_frame;

            if (actual_frame && g_transcode_context.collect_latency) {
                collect_enc_latency(args, timer_tick, &first_frame);
            }

            if (g_transcode_context.default_enc_opts.enableCalcPSNR) {
                update_psnr(args, &stream, &g_transcode_context.default_enc_opts);
            }
            if (g_transcode_context.default_enc_opts.enableCalcSSIM) {
                update_ssim(args, &stream);
            }

            get_nal_types(&stream, nal_types);
            LOG_TRACE("[transcode_mt %3d] vmppEncEncodeFrame OK (%llu), stream: %p, len %u, pts %ld, "
                      "%s frame, %d NALs[%s], avgQP %d, inBusAddr 0x%llx",
                args->thread_index, (u64)args->enc_total_frames, stream.stream, stream.len, stream.pts,
                FRAME(stream.encOutData.frameType), stream.encedNals.cnt, nal_types, stream.encOutData.frameAvgQP,
                (u64)stream.inputBusAddress);

            if (frames_on_device()) {
                if (stream.inputBusAddress == frame->busAddress[0]) {
                    if (stream.inputBusAddress != 0) {
                        release_device_frame(args, frame, "Directly");
                    }
                } else {
                    tmp = NULL;
                    pthread_mutex_lock(&args->frame_mutex);
                    for (i = 0; i < vmpp_queue_size(args->releasing_frame_queue); i++) {
                        tmp = (vmppFrame *)vmpp_queue_peek(args->releasing_frame_queue, i);
                        if (tmp->busAddress[0] == stream.inputBusAddress) {
                            tmp = (vmppFrame *)vmpp_queue_get(args->releasing_frame_queue, i);
                            break;
                        } else {
                            tmp = NULL;
                        }
                    }
                    pthread_mutex_unlock(&args->frame_mutex);

                    if (tmp) {
                        release_device_frame(args, tmp, "From Releasing Queue");
                    }

                    if (frame->memoryType != vmpp_MEM_FLUSH) {
                        pthread_mutex_lock(&args->frame_mutex);
                        vmpp_queue_push_back(args->releasing_frame_queue, frame);
                        pthread_mutex_unlock(&args->frame_mutex);
                    }
                }
            }

            if (args->enc_total_frames % g_transcode_context.period == 0 && actual_frame) {
                time_gap = gettime_ns() - args->enc_tick;
                LOG_INFO("[transcode_mt %3d] Enc Performance: %.3f fps for recent %d frames", args->thread_index,
                    ((float)g_transcode_context.period / ((float)time_gap / (float)NANOSEC_PER_SEC)),
                    g_transcode_context.period);
                args->enc_tick = gettime_ns();
            }

            if (g_transcode_context.another_thread_for_enc_out) {
                sync_threads(args, 1, NULL);
                pthread_mutex_lock(&args->stream_mutex);
                tmp_stream = (vmppStream *)vmpp_queue_pop_front(args->idle_stream_queue);
                if (!tmp_stream) {
                    tmp_stream = (vmppStream *)malloc(sizeof(vmppStream));
                    if (!tmp_stream) {
                        LOG_ERROR("[transcode_mt %3d] malloc buffer for stream struct failed.", args->thread_index);
                    }
                }
                if (tmp_stream) {
                    memcpy(tmp_stream, &stream, sizeof(vmppStream));
                    vmpp_queue_push_back(args->stream_queue, tmp_stream);
                } else {
                    assert(0);
                }
                pthread_mutex_unlock(&args->stream_mutex);
            } else {
                /* check md5. */
                if (g_transcode_context.check_md5) {
                    timer_tick = gettime_ns();
                    compute_md5sum(args, (const unsigned char *)stream.stream, stream.len, args->cur_md5sum);
                    args->check_md5_time += (gettime_ns() - timer_tick);
                }
                if (args->enc_output_file && g_transcode_context.save &&
                    (enc_opts->svcExtractMaxTLayer == VMPP_ENC_DEFAULT_PAR ||
                        stream.svcTemporalId <= enc_opts->svcExtractMaxTLayer)) {
                    timer_tick = gettime_ns();
                    fwrite(stream.stream, 1, stream.len, args->enc_output_file);
                    args->enc_save_time += (gettime_ns() - timer_tick);
                }

                if (g_transcode_context.parse_cu_info) {
                    parse_and_save_cuinfo(args, &stream, args->enc_total_frames);
                }

                LOG_TRACE("[transcode_mt %3d] vmppEncReleaseStream: stream %p", args->thread_index, stream.stream);
                vmppEncReleaseStream(args->enc_ch, &stream);
            }

            if (g_transcode_context.encode_yuv && g_transcode_context.target_fps) {
                do_fps_control(args, &fps_control_timestamp, interval_ns);
            }

            continue;
        } else if (enc_ret == vmpp_RSLT_ENC_INPUT_INSERTED) {
            total_frames_send++;
            LOG_TRACE("[transcode_mt %3d] vmppEncEncodeFrame INSERTED (%llu)", args->thread_index,
                (u64)args->enc_total_frames);
            if (frames_on_device() && frame->memoryType != vmpp_MEM_FLUSH &&
                g_transcode_context.memory_mode != vmpp_DEC_MEM_USER_OUT_BUF_HOST) {
                pthread_mutex_lock(&args->frame_mutex);
                vmpp_queue_push_back(args->releasing_frame_queue, frame);
                pthread_mutex_unlock(&args->frame_mutex);
            }
        } else if (enc_ret == vmpp_RSLT_WARN_EOS) {
            LOG_TRACE(
                "[transcode_mt %3d] vmppEncEncodeFrame EOS (%llu)!!!", args->thread_index, (u64)args->enc_total_frames);
            if (res_changing) {
                res_changed = 1;
                continue;
            } else if (args->force_flush_encoder) {
                args->force_flush_encoder = 0;
                continue;
            } else {
                LOG_DEBUG("[transcode_mt %3d] exit enc thread, args->enc_error(%d), args->enc_stop(%lld)",
                    args->thread_index, args->enc_error, (u64)args->enc_stop);
                break;
            }
        } else if (enc_ret == vmpp_RSLT_ENC_FLUSH) {
            LOG_TRACE(
                "[transcode_mt %3d] vmppEncEncodeFrame FLUSH (%llu)", args->thread_index, (u64)args->enc_total_frames);
            if (frames_on_device()) {
                tmp = NULL;
                pthread_mutex_lock(&args->frame_mutex);
                for (i = 0; i < vmpp_queue_size(args->releasing_frame_queue); i++) {
                    tmp = (vmppFrame *)vmpp_queue_peek(args->releasing_frame_queue, i);
                    if (tmp->busAddress[0] == stream.inputBusAddress) {
                        tmp = (vmppFrame *)vmpp_queue_get(args->releasing_frame_queue, i);
                        break;
                    } else {
                        tmp = NULL;
                    }
                }
                pthread_mutex_unlock(&args->frame_mutex);
                if (tmp) {
                    release_device_frame(args, tmp, "Flush");
                }
            }
        } else {
            LOG_INFO("[transcode_mt %3d] vmppEncEncodeFrame ret = %d", args->thread_index, enc_ret);
            if (enc_ret == vmpp_RSLT_ERR_ENC_INIT) {
                LOG_ERROR("[transcode_mt %3d] exit enc thread because error(%d)", args->thread_index, enc_ret);
                args->enc_error = 1;
                break;
            }
            if (enc_ret < 0) {
                error_cnt++;
                if (error_cnt >= 3) {
                    LOG_ERROR(
                        "[transcode_mt %3d] exit enc thread because error_cnt(%d) >= 3", args->thread_index, error_cnt);
                    args->enc_error = 1;
                    break;
                }
            }
        }
    } while (1);

    args->enc_stop = gettime_ns();

    additional_frame_releasing(args);

    if (args->force_flush_encoder) {
        args->force_flush_encoder = 0;
    }

    if (sei_array) {
        clear_sei_data(sei_array, sei_count);
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
    cJSON *root = NULL;
    cJSON *arrayItem = NULL, *item = NULL, *subItem = NULL;
    int item_count = 0;
    int i = 0;
    char *tmp = NULL;
    struct stat st;
    int len = 0;

    json_file = fopen(json, "rb");
    if (!json_file) {
        LOG_ERROR("[transcode_mt] Fail to open json file '%s'", json);
        ret = -1;
        goto parse_json_fail;
    }
    fseek(json_file, 0, SEEK_END);
    json_size = (int)ftell(json_file);
    fseek(json_file, 0, SEEK_SET);
    json_buffer = (char *)malloc(json_size + 1);
    if (!json_buffer) {
        LOG_ERROR("[transcode_mt] Fail to malloc json buffer. size: %d", json_size);
        ret = -1;
        goto parse_json_fail;
    }

    ret = fread(json_buffer, 1, json_size, json_file);
    if (ret <= 0) {
        LOG_ERROR("[transcode_mt] Fail to read data from json file. size: %d", json_size);
        ret = -1;
        goto parse_json_fail;
    }
    json_buffer[json_size] = '\0';

    root = cJSON_Parse(json_buffer);
    if (!root) {
        LOG_ERROR("[transcode_mt] Fail to parse json file, err: %s", cJSON_GetErrorPtr());
        ret = -1;
        goto parse_json_fail;
    }

    arrayItem = cJSON_GetObjectItem(root, "device");
    if (arrayItem) {
        item_count = cJSON_GetArraySize(arrayItem);
        for (i = 0; i < item_count; i++) {
            item = cJSON_GetArrayItem(arrayItem, i);
            len = strlen(item->valuestring) + 1;
            tmp = (char *)malloc(len);
            if (!tmp) {
                LOG_ERROR("[transcode_mt] Fail to malloc buffer for device url: %s", item->valuestring);
                ret = -1;
                goto parse_json_fail;
            }
            memset(tmp, 0, len);
            strcpy(tmp, item->valuestring);
            vmpp_queue_push_back(g_transcode_context.devices, tmp);
        }
    }

    arrayItem = cJSON_GetObjectItem(root, "url");
    if (arrayItem) {
        item_count = cJSON_GetArraySize(arrayItem);
        for (i = 0; i < item_count; i++) {
            item = cJSON_GetArrayItem(arrayItem, i);
            len = strlen(item->valuestring) + 1;
            tmp = (char *)malloc(len+1024);
            if (!tmp) {
                LOG_ERROR("[transcode_mt] Fail to malloc buffer for file url in json(%s): %s", json, item->valuestring);
                ret = -1;
                goto parse_json_fail;
            }
            memset(tmp, 0, len+1024);
            sprintf(tmp, "%s%s", UT_RES_PATH, item->valuestring);
            // strcpy(tmp, item->valuestring);
            vmpp_queue_push_back(g_transcode_context.urls, tmp);
        }
    }

    arrayItem = cJSON_GetObjectItem(root, "folder");
    if (arrayItem) {
        item_count = cJSON_GetArraySize(arrayItem);
        for (i = 0; i < item_count; i++) {
            item = cJSON_GetArrayItem(arrayItem, i);

            /* check dir validation */
            memset(&st, 0, sizeof(struct stat));
            lstat(item->valuestring, &st);
            if (!S_ISDIR(st.st_mode)) {
                continue;
            }

            read_files_from_dir(g_transcode_context.urls, item->valuestring);
        }
    }

    arrayItem = cJSON_GetObjectItem(root, "encoder_params");
    if (arrayItem) {
        item_count = cJSON_GetArraySize(arrayItem);
        for (i = 0; i < item_count; i++) {
            enc_options *enc_opt = (enc_options *)malloc(sizeof(enc_options));
            if (!enc_opt) {
                LOG_ERROR("[transcode_mt] Fail to malloc buffer for encoder options in json(%s): %s", json,
                    item->valuestring);
                ret = -1;
                goto parse_json_fail;
            }
            memset(enc_opt, 0, sizeof(enc_options));
            set_default_enc_params(enc_opt);
            item = cJSON_GetArrayItem(arrayItem, i);
            if (item) {
                subItem = cJSON_GetObjectItem(item, "codec");
                if (subItem) {
                    if (strcmp(subItem->valuestring, "hevc") == 0 || strcmp(subItem->valuestring, "h265") == 0) {
                        enc_opt->encCodec = (char*)"hevc";
                    } else if (strcmp(subItem->valuestring, "h264") == 0 || strcmp(subItem->valuestring, "avc") == 0) {
                        enc_opt->encCodec = (char*)"h264";
                    } else if (strcmp(subItem->valuestring, "av1") == 0) {
                        enc_opt->encCodec = (char*)"av1";
                    } else if (strcmp(subItem->valuestring, "jpeg") == 0) {
                        enc_opt->encCodec = (char*)"jpeg";
                    }
                }

                subItem = cJSON_GetObjectItem(item, "profile");
                if (subItem) {
                    enc_opt->profile = subItem->valueint;
                }

                subItem = cJSON_GetObjectItem(item, "level");
                if (subItem) {
                    enc_opt->level = subItem->valueint;
                }

                subItem = cJSON_GetObjectItem(item, "gopSize");
                if (subItem) {
                    enc_opt->gopSize = subItem->valueint;
                }

                subItem = cJSON_GetObjectItem(item, "frameRateNum");
                if (subItem) {
                    enc_opt->frameRateNum = subItem->valueint;
                }

                subItem = cJSON_GetObjectItem(item, "frameRateDen");
                if (subItem) {
                    enc_opt->frameRateDen = subItem->valueint;
                }

                subItem = cJSON_GetObjectItem(item, "bitDepthLuma");
                if (subItem) {
                    enc_opt->bitDepthLuma = subItem->valueint;
                }

                subItem = cJSON_GetObjectItem(item, "bitDepthChroma");
                if (subItem) {
                    enc_opt->bitDepthChroma = subItem->valueint;
                }

                subItem = cJSON_GetObjectItem(item, "lookaheadDepth");
                if (subItem) {
                    enc_opt->lookaheadDepth = subItem->valueint;
                }

                subItem = cJSON_GetObjectItem(item, "tune");
                if (subItem) {
                    enc_opt->tune = subItem->valueint;
                }

                subItem = cJSON_GetObjectItem(item, "keyInt");
                if (subItem) {
                    enc_opt->keyInt = subItem->valueint;
                }

                subItem = cJSON_GetObjectItem(item, "gdrDuration");
                if (subItem) {
                    enc_opt->gdrDuration = subItem->valueint;
                }

                subItem = cJSON_GetObjectItem(item, "crf");
                if (subItem) {
                    enc_opt->crf = subItem->valueint;
                }

                subItem = cJSON_GetObjectItem(item, "cqp");
                if (subItem) {
                    enc_opt->cqp = subItem->valueint;
                }

                subItem = cJSON_GetObjectItem(item, "llRc");
                if (subItem) {
                    enc_opt->llRc = subItem->valueint;
                }

                subItem = cJSON_GetObjectItem(item, "bitRate");
                if (subItem) {
                    enc_opt->bitRate = subItem->valueint;
                }

                subItem = cJSON_GetObjectItem(item, "initQp");
                if (subItem) {
                    enc_opt->initQp = subItem->valueint;
                }

                subItem = cJSON_GetObjectItem(item, "vbvBufSize");
                if (subItem) {
                    enc_opt->vbvBufSize = subItem->valueint;
                }

                subItem = cJSON_GetObjectItem(item, "vbvMaxRate");
                if (subItem) {
                    enc_opt->vbvMaxRate = subItem->valueint;
                }

                subItem = cJSON_GetObjectItem(item, "intraQpDelta");
                if (subItem) {
                    enc_opt->intraQpDelta = subItem->valueint;
                }

                subItem = cJSON_GetObjectItem(item, "qpMinI");
                if (subItem) {
                    enc_opt->qpMinI = subItem->valueint;
                }

                subItem = cJSON_GetObjectItem(item, "qpMaxI");
                if (subItem) {
                    enc_opt->qpMaxI = subItem->valueint;
                }

                subItem = cJSON_GetObjectItem(item, "qpMinPB");
                if (subItem) {
                    enc_opt->qpMinPB = subItem->valueint;
                }

                subItem = cJSON_GetObjectItem(item, "qpMaxPB");
                if (subItem) {
                    enc_opt->qpMaxPB = subItem->valueint;
                }

                subItem = cJSON_GetObjectItem(item, "aqStrength");
                if (subItem) {
                    enc_opt->aqStrength = subItem->valuedouble;
                }

                subItem = cJSON_GetObjectItem(item, "qualityMode");
                if (subItem) {
                    enc_opt->qualityMode = subItem->valueint;
                }

                subItem = cJSON_GetObjectItem(item, "vbr");
                if (subItem) {
                    enc_opt->vbr = subItem->valueint;
                }

                subItem = cJSON_GetObjectItem(item, "pCom");
                if (subItem) {
                    len = strlen(subItem->valuestring) + 1;
                    tmp = (char *)malloc(len);
                    if (!tmp) {
                        LOG_ERROR("[transcode_mt] Fail to malloc buffer for pCom in json(%s): %s", json,
                            subItem->valuestring);
                        ret = -1;
                        goto parse_json_fail;
                    }
                    memset(tmp, 0, len);
                    strcpy(tmp, subItem->valuestring);
                    enc_opt->pCom = tmp;
                    enc_opt->comLength = strlen(subItem->valuestring);
                }

                subItem = cJSON_GetObjectItem(item, "P2B");
                if (subItem) {
                    enc_opt->P2B = subItem->valueint;
                }

                subItem = cJSON_GetObjectItem(item, "bBPyramid");
                if (subItem) {
                    enc_opt->bBPyramid = subItem->valueint;
                }
            }

            vmpp_queue_push_back(g_transcode_context.customized_enc_opts, enc_opt);
        }
    }
parse_json_fail:
    if (root) {
        cJSON_Delete(root);
        root = NULL;
    }
    if (json_buffer) {
        free(json_buffer);
        json_buffer = NULL;
    }
    if (json_file) {
        fclose(json_file);
        json_file = NULL;
    }
    return ret;
}

static inline vmppPixelFormat get_pixel_format(const char *pf)
{
    vmppPixelFormat pixel_format = vmpp_PIX_FMT_NONE;
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
    } else if (!strcmp(pf, "bgra")) {
        pixel_format = vmpp_PIX_FMT_BGRA;
    } else if (!strcmp(pf, "rgba10")) {
        pixel_format = vmpp_PIX_FMT_RGBA10;
    } else {
        pixel_format = vmpp_PIX_FMT_NONE;
    }
    return pixel_format;
}
#define PIXFMT(f) get_pixel_format(f)

static int parse_resolution_from_url(const char *url, int *p_witdh, int *p_height)
{
    char ch;
    size_t i;
    int url_offset = 0, temp_w = 0, temp_h = 0, valid = 0;
    const char *tmp = NULL;
    char *input_suffix = NULL, *end;
    char file_name[MAX_PATH_LEN] = { 0 };
    tmp = strrchr(url, '/');
    if (!tmp) {
        /* No '/' in current_url */
        tmp = url;
    } else {
        url_offset = 1;
    }

    input_suffix = (char *)strrchr(url, '.');
    if (input_suffix) {
        strncpy(file_name, tmp + url_offset, strlen(tmp) - strlen(input_suffix) - 1);
    } else {
        strncpy(file_name, tmp + url_offset, strlen(tmp));
    }

    for (i = 0; i < strlen(file_name); i++) {
        tmp = &file_name[i];
        ch = file_name[i];
        if (ch <= '9' && ch >= '0') {
            temp_w = strtol(tmp, &end, 10);
            tmp = end;
            if (tmp && (*tmp == 'x' || *tmp == 'X')) {
                valid = 1;
                break;
            }
        }
    }
    tmp++;
    temp_h = strtol(tmp, &end, 10);
    valid &= (temp_w && temp_h);
    if (valid) {
        *p_witdh = temp_w;
        *p_height = temp_h;
        return 0;
    }

    return -1;
}

static int parse_options(params_transcode_mt_t *params_ut)
{
    char *suffix = NULL;
    char *url = NULL;
    struct stat st;

    // struct parameter prm;
    // int ret, val;
    // char *optarg;
    // prm.cnt = 1;

    // if (argc == 1) {
    //     LOGIL_ERROR("[transcode_mt] Parameters Not Enough! You should provide one input file at least.");
    //     usage(argv[0], OPT_ALL);
    //     return -1;
    // }

    // if (argc == 2 && !strcmp(argv[1], "-h")) {
    //     usage(argv[0], OPT_ALL);
    //     return 1;
    // }

    // while ((ret = get_option(argc, argv, ops, &prm)) != -1) {
    //     if (ret == -2 && prm.enable == 1) {
    //     if (prm.short_opt == 'h' || (prm.longOpt && strcmp(prm.longOpt, "help") == 0)) {
    //         usage(argv[0], OPT_ALL);
    //         return 1;
    //     }

    //     LOGIL_ERROR("[transcode_mt] Unassigned value,please check the parameters");
    //     usage(argv[0], OPT_ALL);
    //     return -1;
    // }
    //     optarg = prm.argument;
    //     switch (prm.short_opt) {
    //     case 'i':
            memset(&st, 0, sizeof(struct stat));
            lstat(params_ut->input, &st);
            if (S_ISDIR(st.st_mode)) {
                LOG_DEBUG("[transcode_mt] parameter:i '%s' is a directory, read files from it.", params_ut->input);
                read_files_from_dir(g_transcode_context.urls, params_ut->input);
            } else if (S_ISREG(st.st_mode)) {
                suffix = strrchr(params_ut->input, '.');
                if (suffix && 0 == strcmp(suffix, ".json")) {
                    LOGIL_WARN("[transcode_mt] The code for parsing parameters from JSON is a bit outdated, "
                               "so you may encounter some kind of error, Be Careful!");
                    if (parse_json(params_ut->input) < 0) {
                        LOGIL_ERROR("[transcode_mt] Invalid json file: %s", params_ut->input);
                        //  usage(argv[0], OPT_SHORT);
                        return -1;
                    }
                } else {
                    url = (char *)malloc((strlen(params_ut->input) + 1) * sizeof(char));
                    if (!url) {
                        LOGIL_ERROR("[transcode_mt] fail to malloc buffer for file url: %s", params_ut->input);
                        return -1;
                    }
                    memset(url, 0, (strlen(params_ut->input) + 1) * sizeof(char));
                    strcpy(url, params_ut->input);
                    vmpp_queue_push_back(g_transcode_context.urls, url);
                }
            } else {
                g_has_irregular_url = 1;
                LOGIL_WARN("[transcode_mt] url '%s' is not a regular file or directory", optarg);
                url = (char *)malloc((strlen(params_ut->input) + 1) * sizeof(char));
                if (!url) {
                    LOGIL_ERROR("[transcode_mt] fail to malloc buffer for file url: %s", params_ut->input);
                    return -1;
                }
                memset(url, 0, (strlen(params_ut->input) + 1) * sizeof(char));
                strcpy(url, params_ut->input);
                vmpp_queue_push_back(g_transcode_context.urls, url);
            }

        //     break;
        // case 'o':
            memset(&st, 0, sizeof(struct stat));
            lstat(params_ut->output_directory, &st);
            if (S_ISDIR(st.st_mode)) {
                LOG_DEBUG("[transcode_mt] parameter:o '%s' is a directory.", params_ut->output_directory);
                g_transcode_context.output_directory = params_ut->output_directory;
            } else {
                LOG_DEBUG("[transcode_mt] parameter:o '%s'.", params_ut->output_file);
                g_transcode_context.output_file = params_ut->output_file;
            }
        //     break;
        // case 'd':
        // case 'r':
            g_transcode_context.device_name = params_ut->device_name;
        //     break;
        // case 'l':
            g_transcode_context.loop = params_ut->loop;
            if (g_transcode_context.loop <= 0) {
                LOGIL_WARN("[transcode_mt] parameter:l negative, infinite loop.");
                g_transcode_context.loop = INT32_MAX - 1;
            }
        //     break;
        // case 'L':
            g_transcode_context.main_loop = params_ut->main_loop;
            if (g_transcode_context.main_loop <= 0) {
                LOGIL_WARN("[transcode_mt] parameter:L negative, infinite loop.");
                g_transcode_context.main_loop = INT32_MAX - 1;
            }
        //     break;
        // case 's':
            g_transcode_context.save = params_ut->save;
        //     break;
        // case 'm':
            g_transcode_context.check_md5 = params_ut->check_md5;
        //     break;
        // case 'c':
            g_transcode_context.codec = params_ut->codec;
        //     break;
        // case 'C':
            g_transcode_context.enc_codec = params_ut->enc_codec;
        //     break;
        // case 'f':
#ifdef USING_FFMPEG
            g_transcode_context.using_ffmpeg = params_ut->using_ffmpeg;
#else
            if (params_ut->using_ffmpeg == 1) {
                LOGIL_ERROR("[transcode_mt] using_ffmpeg can not be set!");
                // usage(argv[0], OPT_SHORT);
                return -1;
            }
#endif
        //     break;
        // case 'p':
            g_transcode_context.period = params_ut->log_period;
            if (g_transcode_context.period <= 0) {
                LOGIL_WARN("[transcode_mt] Invalid period frame count <%d>, using default value 100",
                    g_transcode_context.period);
                g_transcode_context.period = DEFAULT_PERIOD_FRAMES;
            }
        //     break;
        // case 'P':
            g_transcode_context.perf_period = params_ut->perf_period;
            if (g_transcode_context.perf_period <= 0) {
                LOGIL_WARN("[transcode_mt] Invalid period frame count <%d> for performance, using "
                           "default value 1000",
                    g_transcode_context.perf_period);
                g_transcode_context.perf_period = PERF_PERIOD_FRAMES;
            }
        //     break;
        // case 'n':
            g_transcode_context.vframes = params_ut->vframes;
        //     break;
        // case 'b':
            g_transcode_context.bitDepth = params_ut->bitDepth;
        //     break;
        // case 'M':
            g_transcode_context.memory_mode = params_ut->memory_mode;
        //     break;
        // case 't':
            g_transcode_context.thread_count = params_ut->thread_count;
            if (g_transcode_context.thread_count < 1) {
                LOGIL_WARN("[transcode_mt] Incorrect number of threads [%d], use '1' instead!",
                    g_transcode_context.thread_count);
                g_transcode_context.thread_count = 1;
            }
        //     break;
        // case 'a':
            g_transcode_context.dec_output_align = params_ut->dec_output_align;
            // break;
        // case 'W':
            g_transcode_context.default_enc_opts.width = params_ut->width;
        //     break;
        // case 'H':
            g_transcode_context.default_enc_opts.height = params_ut->height;
            // break;
        // case '?':
        //     LOGIL_ERROR("[transcode_mt] Unknown option(s) : '%s'!!!", optarg);
        //     usage(argv[0], OPT_ALL);
        //     return -1;
        // case 'h':
        //     val = atoi(optarg);
            // val = (val >= OPT_ALL && val < OPT_MAX) ? val : OPT_ALL;
            // if (usage_for_option(optarg)) {
            //     usage(argv[0], val);
            // }

            // return 1;

        struct parameter prm;
        int ret;
        char *optarg;
        prm.cnt = 1;

        while ((ret = get_option(params_ut->argc, params_ut->argv, ops, &prm)) != -1) {
            if (ret == -2 && prm.enable == 1) {
                LOG_ERROR("[transcode_mt] Unassigned value,please check the parameters");
                // usage(argv[0]);
                return -1;
            }
            optarg = prm.argument;
            switch (prm.short_opt) {
            case '0':
                if (strcmp(prm.longOpt, "profile") == 0) {
                    g_transcode_context.default_enc_opts.profile = atoi(optarg);
                } else if (strcmp(prm.longOpt, "level") == 0) {
                    g_transcode_context.default_enc_opts.level = atoi(optarg);
                } else if (strcmp(prm.longOpt, "frameRateNum") == 0) {
                    g_transcode_context.default_enc_opts.frameRateNum = atoi(optarg);
                } else if (strcmp(prm.longOpt, "frameRateDen") == 0) {
                    g_transcode_context.default_enc_opts.frameRateDen = atoi(optarg);
                } else if (strcmp(prm.longOpt, "bitDepthLuma") == 0) {
                    g_transcode_context.default_enc_opts.bitDepthLuma = atoi(optarg);
                } else if (strcmp(prm.longOpt, "bitDepthChroma") == 0) {
                    g_transcode_context.default_enc_opts.bitDepthChroma = atoi(optarg);
                } else if (strcmp(prm.longOpt, "gopSize") == 0) {
                    g_transcode_context.default_enc_opts.gopSize = atoi(optarg);
                } else if (strcmp(prm.longOpt, "gdrDuration") == 0) {
                    g_transcode_context.default_enc_opts.gdrDuration = atoi(optarg);
                } else if (strcmp(prm.longOpt, "lookaheadDepth") == 0) {
                    g_transcode_context.default_enc_opts.lookaheadDepth = atoi(optarg);
                } else if (strcmp(prm.longOpt, "outbufNum") == 0) {
                    g_transcode_context.default_enc_opts.outbufNum = atoi(optarg);
                } else if (strcmp(prm.longOpt, "qualityMode") == 0) {
                    g_transcode_context.default_enc_opts.qualityMode = atoi(optarg);
                } else if (strcmp(prm.longOpt, "tune") == 0) {
                    g_transcode_context.default_enc_opts.tune = atoi(optarg);
                } else if (strcmp(prm.longOpt, "keyInt") == 0) {
                    g_transcode_context.default_enc_opts.keyInt = atoi(optarg);
                } else if (strcmp(prm.longOpt, "crf") == 0) {
                    g_transcode_context.default_enc_opts.crf = atoi(optarg);
                } else if (strcmp(prm.longOpt, "cqp") == 0) {
                    g_transcode_context.default_enc_opts.cqp = atoi(optarg);
                } else if (strcmp(prm.longOpt, "llRc") == 0) {
                    g_transcode_context.default_enc_opts.llRc = atoi(optarg);
                } else if (strcmp(prm.longOpt, "bitRate") == 0) {
                    g_transcode_context.default_enc_opts.bitRate = atoi(optarg);
                } else if (strcmp(prm.longOpt, "initQp") == 0) {
                    g_transcode_context.default_enc_opts.initQp = atoi(optarg);
                } else if (strcmp(prm.longOpt, "vbvBufSize") == 0) {
                    g_transcode_context.default_enc_opts.vbvBufSize = atoi(optarg) * 1000;
                } else if (strcmp(prm.longOpt, "vbvMaxRate") == 0) {
                    g_transcode_context.default_enc_opts.vbvMaxRate = atoi(optarg) * 1000;
                } else if (strcmp(prm.longOpt, "intraQpDelta") == 0) {
                    g_transcode_context.default_enc_opts.intraQpDelta = atoi(optarg);
                } else if (strcmp(prm.longOpt, "qpMinI") == 0) {
                    g_transcode_context.default_enc_opts.qpMinI = atoi(optarg);
                } else if (strcmp(prm.longOpt, "qpMaxI") == 0) {
                    g_transcode_context.default_enc_opts.qpMaxI = atoi(optarg);
                } else if (strcmp(prm.longOpt, "qpMinPB") == 0) {
                    g_transcode_context.default_enc_opts.qpMinPB = atoi(optarg);
                } else if (strcmp(prm.longOpt, "qpMaxPB") == 0) {
                    g_transcode_context.default_enc_opts.qpMaxPB = atoi(optarg);
                } else if (strcmp(prm.longOpt, "aqStrength") == 0) {
                    g_transcode_context.default_enc_opts.aqStrength = atof(optarg);
                } else if (strcmp(prm.longOpt, "P2B") == 0) {
                    g_transcode_context.default_enc_opts.P2B = atoi(optarg);
                } else if (strcmp(prm.longOpt, "bBPyramid") == 0) {
                    g_transcode_context.default_enc_opts.bBPyramid = atof(optarg);
                } else if (strcmp(prm.longOpt, "maxFrameSizeMultiple") == 0) {
                    g_transcode_context.default_enc_opts.maxFrameSizeMultiple = atof(optarg);
                } else if (strcmp(prm.longOpt, "maxFrameSize") == 0) {
                    g_transcode_context.default_enc_opts.maxFrameSize = atoi(optarg);
                } else if (strcmp(prm.longOpt, "roiType") == 0) {
                    g_transcode_context.default_enc_opts.roiType = atoi(optarg);
                } else if (strcmp(prm.longOpt, "roiInt") == 0) {
                    g_transcode_context.default_enc_opts.roiInt = atoi(optarg);
                } else if (strcmp(prm.longOpt, "roiParam") == 0) {
                    g_transcode_context.default_enc_opts.roiParam = optarg;
                } else if (strcmp(prm.longOpt, "extSEIInt") == 0) {
                    g_transcode_context.default_enc_opts.extSEIInt = atoi(optarg);
                } else if (strcmp(prm.longOpt, "forceIDRInt") == 0) {
                    g_transcode_context.default_enc_opts.forceIDRInt = atoi(optarg);
                } else if (strcmp(prm.longOpt, "roiMapDeltaQpBlockUnit") == 0) {
                    g_transcode_context.default_enc_opts.roiMapDeltaQpBlockUnit = atoi(optarg);
                } else if (strcmp(prm.longOpt, "roiMapQpDeltaVersion") == 0) {
                    g_transcode_context.default_enc_opts.roiMapQpDeltaVersion = atoi(optarg);
                } else if (strcmp(prm.longOpt, "enableDynamicBitrate") == 0) {
                    g_transcode_context.default_enc_opts.enableDynamicBitrate = atoi(optarg);
                } else if (strcmp(prm.longOpt, "enableDynamicFrameRate") == 0) {
                    g_transcode_context.default_enc_opts.enableDynamicFrameRate = atoi(optarg);
                } else if (strcmp(prm.longOpt, "maxBFrames") == 0) {
                    g_transcode_context.default_enc_opts.maxBFrames = atoi(optarg);
                } else if (strcmp(prm.longOpt, "hrd") == 0) {
                    g_transcode_context.default_enc_opts.hrd = atoi(optarg);
                } else if (strcmp(prm.longOpt, "picSkip") == 0) {
                    g_transcode_context.default_enc_opts.pictureSkip = atoi(optarg);
                } else if (strcmp(prm.longOpt, "vfr") == 0) {
                    g_transcode_context.default_enc_opts.vfr = atoi(optarg);
                } else if (strcmp(prm.longOpt, "svcTLayers") == 0) {
                    g_transcode_context.default_enc_opts.svcTLayers = atoi(optarg);
                } else if (strcmp(prm.longOpt, "svcExtractMaxTLayer") == 0) {
                    g_transcode_context.default_enc_opts.svcExtractMaxTLayer = atoi(optarg);
                } else if (strcmp(prm.longOpt, "sliceSize") == 0) {
                    g_transcode_context.default_enc_opts.sliceSize = atoi(optarg);
                } else if (strcmp(prm.longOpt, "enableDynamicCrf") == 0) {
                    g_transcode_context.default_enc_opts.enableDynamicCrf = atoi(optarg);
                } else if (strcmp(prm.longOpt, "psnr") == 0) {
                    g_transcode_context.default_enc_opts.enableCalcPSNR = atoi(optarg);
                } else if (strcmp(prm.longOpt, "ssim") == 0) {
                    g_transcode_context.default_enc_opts.enableCalcSSIM = atoi(optarg);
                } else if (strcmp(prm.longOpt, "ltrInterval") == 0) {
                    g_transcode_context.default_enc_opts.ltrInterval = atoi(optarg);
                } else if (strcmp(prm.longOpt, "ltrQpDelta") == 0) {
                    g_transcode_context.default_enc_opts.ltrQpDelta = atoi(optarg);
                } else if (strcmp(prm.longOpt, "ltrRefGap") == 0) {
                    g_transcode_context.default_enc_opts.ltrRefGap = atoi(optarg);
                } else if (strcmp(prm.longOpt, "ltrInsertTest") == 0) {
                    g_transcode_context.default_enc_opts.ltrInsertTest = atoi(optarg);
                } else if (strcmp(prm.longOpt, "rotation") == 0) {
                    g_transcode_context.default_enc_opts.rotation = atoi(optarg);
                } else if (strcmp(prm.longOpt, "coreID") == 0) {
                    g_transcode_context.default_enc_opts.coreID = atoi(optarg);
                } else if (strcmp(prm.longOpt, "openGop") == 0) {
                    g_transcode_context.default_enc_opts.openGop = atof(optarg);
                } else if (strcmp(prm.longOpt, "smartEnc") == 0) {
                    g_transcode_context.default_enc_opts.smartEnc = atoi(optarg);
                } else if (strcmp(prm.longOpt, "enableDynamicKeyInt") == 0) {
                    g_transcode_context.default_enc_opts.enableDynamicKeyInt = atoi(optarg);
                } else if (strcmp(prm.longOpt, "disableMMCO") == 0) {
                    g_transcode_context.default_enc_opts.disableMMCO = atoi(optarg);
                } else if (strcmp(prm.longOpt, "inLoopDSRatio") == 0) {
                    g_transcode_context.default_enc_opts.inLoopDSRatio = atoi(optarg);
                } else if (strcmp(prm.longOpt, "aqMode") == 0) {
                    g_transcode_context.default_enc_opts.aqMode = atoi(optarg);
                } else if (strcmp(prm.longOpt, "psyFactor") == 0) {
                    g_transcode_context.default_enc_opts.psyFactor = atof(optarg);
                } else if (strcmp(prm.longOpt, "rdoLevel") == 0) {
                    g_transcode_context.default_enc_opts.rdoLevel = atoi(optarg);
                } else if (strcmp(prm.longOpt, "enableRdoQuant") == 0) {
                    g_transcode_context.default_enc_opts.enableRdoQuant = atoi(optarg);
                } else if (strcmp(prm.longOpt, "qCompress") == 0) {
                    g_transcode_context.default_enc_opts.qCompress = atof(optarg);
                } else if (strcmp(prm.longOpt, "rcMode") == 0) {
                    g_transcode_context.default_enc_opts.rcMode = atof(optarg);
                } else if (strcmp(prm.longOpt, "multiDevice") == 0) {
                    g_transcode_context.multi_device = atof(optarg);
                } else if (strcmp(prm.longOpt, "randomDevice") == 0) {
                    g_transcode_context.random_device = atof(optarg);
                } else if (strcmp(prm.longOpt, "multicore") == 0) {
                    g_transcode_context.default_enc_opts.multicore = atoi(optarg);
                } else if (strcmp(prm.longOpt, "decApiMode") == 0) {
                    g_transcode_context.dec_api_mode = (vmppDecApiMode)atoi(optarg);
                        if (g_transcode_context.dec_api_mode != vmpp_DEC_API_MODE_PARALLEL &&
                        g_transcode_context.dec_api_mode != vmpp_DEC_API_MODE_SERIAL) {
                        LOGIL_ERROR("[transcode_mt] Unsupported decApiMode=%d!", g_transcode_context.dec_api_mode);
                        // usage(argv[0], OPT_DEC);
                        return -1;
                    }
                } else if (strcmp(prm.longOpt, "multiRuntime") == 0) {
                    g_transcode_context.multi_runtime = atoi(optarg);
                } else if (strcmp(prm.longOpt, "uniqueOutputFile") == 0) {
                    g_transcode_context.unique_output_file = atoi(optarg);
                } else if (strcmp(prm.longOpt, "maxQueuedFrame") == 0) {
                    g_transcode_context.max_queued_frame = atoi(optarg);
                } else if (strcmp(prm.longOpt, "maxQueuedStream") == 0) {
                    g_transcode_context.max_queued_stream = atoi(optarg);
                } else if (strcmp(prm.longOpt, "outputCuInfo") == 0) {
                    g_transcode_context.output_cu_info = atoi(optarg);
                } else if (strcmp(prm.longOpt, "parseCuInfo") == 0) {
                    g_transcode_context.parse_cu_info = atoi(optarg);
                } else if (strcmp(prm.longOpt, "saveCuInfo") == 0) {
                    g_transcode_context.save_cu_info = atoi(optarg);
                } else if (strcmp(prm.longOpt, "anotherThread4EncOut") == 0) {
                    g_transcode_context.another_thread_for_enc_out = atoi(optarg);
                } else if (strcmp(prm.longOpt, "decCrop") == 0) {
                    g_transcode_context.dec_crop = atoi(optarg);
                } else if (strcmp(prm.longOpt, "decCropInfo") == 0) {
                    g_transcode_context.dec_crop_info = optarg;
                } else if (strcmp(prm.longOpt, "decMemoryMode") == 0) {
                    g_transcode_context.memory_mode = atoi(optarg);
                } else if (strcmp(prm.longOpt, "decOutputAlign") == 0) {
                    g_transcode_context.dec_output_align = atoi(optarg);
                } else if (strcmp(prm.longOpt, "decMode") == 0) {
                    g_transcode_context.dec_mode = atoi(optarg);
                    if (g_transcode_context.dec_mode < 0 || g_transcode_context.dec_mode > vmpp_DEC_LOW_DELAY) {
                        LOGIL_ERROR("[transcode_mt] Unsupported decMode=%d!", g_transcode_context.dec_mode);
                        // usage(argv[0], OPT_DEC);
                        return -1;
                    }
                } else if (strcmp(prm.longOpt, "decNoOutputReordering") == 0) {
                    g_transcode_context.dec_no_output_reordering = atoi(optarg);
                } else if (strcmp(prm.longOpt, "width") == 0) {
                    g_transcode_context.default_enc_opts.width = atoi(optarg);
                } else if (strcmp(prm.longOpt, "height") == 0) {
                    g_transcode_context.default_enc_opts.height = atoi(optarg);
                } else if (strcmp(prm.longOpt, "pixelFormat") == 0) {
                    g_transcode_context.default_enc_opts.pixelFormat = PIXFMT(optarg);
                    if (g_transcode_context.default_enc_opts.pixelFormat == vmpp_PIX_FMT_NONE) {
                        LOGIL_ERROR("[transcode_mt] Unsupported pixelFormat='%s'!", optarg);
                        // usage(argv[0], OPT_ENC);
                        return -1;
                    }
                } else if (strcmp(prm.longOpt, "targetMD5") == 0) {
                    g_transcode_context.target_md5 = optarg;
                    if (g_transcode_context.target_md5 && strlen(g_transcode_context.target_md5) != MD5_HASH_LEN * 2) {
                        LOGIL_ERROR("[transcode_mt] Invalid targetMD5='%s'!", optarg);
                        // usage(argv[0], OPT_EXTRA);
                        return -1;
                    }
                } else if (strcmp(prm.longOpt, "disableDecProfiling") == 0) {
                    g_transcode_context.disable_dec_profiling = atoi(optarg);
                } else if (strcmp(prm.longOpt, "disableEncProfiling") == 0) {
                    g_transcode_context.disable_enc_profiling = atoi(optarg);
                } else if (strcmp(prm.longOpt, "forceHostBuffer") == 0) {
                    LOGIL_WARN("[transcode_mt] Deprecated Option: 'forceHostBuffer' will be ignored! "
                           "Please use 'decRecvMemoryType' for instead.");
                } else if (strcmp(prm.longOpt, "targetFPS") == 0) {
                    g_transcode_context.target_fps = atoi(optarg);
                    if (g_transcode_context.target_fps < 0) {
                        LOGIL_ERROR("[transcode_mt] Invalid targetFPS=%d !", g_transcode_context.target_fps);
                        // usage(argv[0], OPT_EXTRA);
                        return -1;
                    }
                } else if (strcmp(prm.longOpt, "collectLatency") == 0) {
                    g_transcode_context.collect_latency = atoi(optarg);
                } else if (strcmp(prm.longOpt, "targetFrameNumber") == 0) {
                    g_transcode_context.vframes = atoi(optarg);
                } else if (strcmp(prm.longOpt, "reCountFrame4Target") == 0) {
                    g_transcode_context.re_count_vframes = atoi(optarg);
                } else if (strcmp(prm.longOpt, "inputMode") == 0) {
                    g_transcode_context.input_mode = atoi(optarg);
                } else if (strcmp(prm.longOpt, "decRecvMemoryType") == 0) {
                    g_transcode_context.dec_recv_mem_type = atoi(optarg);
                    if (g_transcode_context.dec_recv_mem_type == RECV_MT_SHARED) {
                        LOGIL_ERROR(
                            "[transcode_mt] Unsupported decRecvMemoryType=%d !", g_transcode_context.dec_recv_mem_type);
                        // usage(argv[0]);
                        return -1;
                    } else if (g_transcode_context.dec_recv_mem_type != RECV_MT_DEVICE &&
                            g_transcode_context.dec_recv_mem_type != RECV_MT_HOST) {
                        LOGIL_WARN("[transcode_mt] Invalid decRecvMemoryType=%d, will be ignored and auto decided!",
                            g_transcode_context.dec_recv_mem_type);
                    }
                } else if (strcmp(prm.longOpt, "separateLumaChroma") == 0) {
                    g_transcode_context.separate_luma_chroma = atoi(optarg);
                } else if (strcmp(prm.longOpt, "qLevel") == 0) {
                    g_transcode_context.default_enc_opts.qLevel = atoi(optarg);
                } else if (strcmp(prm.longOpt, "comment") == 0) {
                    g_transcode_context.default_enc_opts.pCom = optarg;
                    g_transcode_context.default_enc_opts.comLength = strlen(optarg);
                } else if (strcmp(prm.longOpt, "decCoreMode") == 0) {
                    g_transcode_context.dec_core_mode = atoi(optarg);
                    if (g_transcode_context.dec_core_mode < 0 || g_transcode_context.dec_core_mode > 2) {
                        LOGIL_ERROR("[transcode_mt] Invalid coreMode=%d! Valid values: 0(auto), 1(single), 2(multi)",
                            g_transcode_context.dec_core_mode);
                        // usage(argv[0], OPT_DEC);
                        return -1;
                    }
                }

                break;
            default:
                LOGIL_ERROR("[transcode_mt] Unsupported Option: %c", ret);
                // usage(argv[0], OPT_ALL);
                return -1;
            }
        }

    if (vmpp_queue_size(g_transcode_context.urls) == 0) {
        LOGIL_ERROR("[transcode_mt] Missing Option: No Input! You must provide at least one input file!");
        // usage(argv[0], OPT_SHORT);
        return -1;
    } else if (vmpp_queue_size(g_transcode_context.urls) == 1 && !g_transcode_context.codec) {
        const char *tmp = (const char *)vmpp_queue_peek(g_transcode_context.urls, 0);
        const char *suffix = strrchr(tmp, '.');
        if (suffix && !strcmp(suffix, ".yuv")) {
            g_transcode_context.codec = (char *)"yuv";
        }
    }

    if (g_transcode_context.multi_device && g_transcode_context.random_device) {
        LOGIL_ERROR(
            "[transcode_mt] Incompatible Options: 'multiDevice' and 'randomDevice' can not be activated simultaneously!");
        // usage(argv[0], OPT_EXTRA);
        return -1;
    } else if ((g_transcode_context.multi_device || g_transcode_context.random_device) &&
               g_transcode_context.device_name) {
        LOGIL_ERROR(
            "[transcode_mt] Incompatible Option: 'multiDevice' (%d) or 'randomDevice' (%d) can not be activated when device (%s) is specified!",
            g_transcode_context.multi_device, g_transcode_context.random_device, g_transcode_context.device_name);
        // usage(argv[0], OPT_EXTRA);
        return -1;
    }

    if ((!g_transcode_context.device_name || g_transcode_context.multi_device || g_transcode_context.random_device) &&
        !vmpp_queue_size(g_transcode_context.devices)) {
        LOGIL_WARN("[transcode_mt] Missing Option: No device specified or multi(%d)/random(%d) device enabled! "
                   "We will try to enumerate all available video devices...",
            g_transcode_context.multi_device, g_transcode_context.random_device);
        if (get_available_devices(g_transcode_context.devices) <= 0) {
            LOGIL_ERROR("[transcode_mt] No available device");
            return -1;
        }
    }

    if ((g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_DEV ||
            g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_AS_HWOUT) &&
        g_transcode_context.dec_recv_mem_type == RECV_MT_HOST) {
        LOGIL_WARN(
            "[transcode_mt] Incompatible Options: decMemoryMode=%d .vs decRecvMemoryType=%d (HOST)! The decRecvMemoryType may be ignored by VMPP!!!",
            g_transcode_context.memory_mode, g_transcode_context.dec_recv_mem_type);
    } else if ((g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_HOST ||
                   g_transcode_context.memory_mode == vmpp_DEC_MEM_LESS_DEV_MEM) &&
               g_transcode_context.dec_recv_mem_type == RECV_MT_DEVICE) {
        LOGIL_WARN(
            "[transcode_mt] Incompatible Options: decMemoryMode=%d .vs decRecvMemoryType=%d (DEVICE)! The decRecvMemoryType may be ignored by VMPP!!!",
            g_transcode_context.memory_mode, g_transcode_context.dec_recv_mem_type);
    }

    if ((g_transcode_context.enc_codec &&
            (strcmp(g_transcode_context.enc_codec, "h264") == 0 || strcmp(g_transcode_context.enc_codec, "avc") == 0 ||
                strcmp(g_transcode_context.enc_codec, "hevc") == 0 ||
                strcmp(g_transcode_context.enc_codec, "h265") == 0 ||
                strcmp(g_transcode_context.enc_codec, "av1") == 0 ||
                strcmp(g_transcode_context.enc_codec, "jpeg") == 0)) ||
        vmpp_queue_size(g_transcode_context.customized_enc_opts)) {
        g_transcode_context.encoder_enable = 1;
    } else if (g_transcode_context.enc_codec) {
        LOGIL_ERROR("[transcode_mt] Unknown Option: Codec type '%s' Will be ignored!!!", g_transcode_context.enc_codec);
    }

    if (g_transcode_context.save && !g_transcode_context.encoder_enable &&
        g_transcode_context.memory_mode != vmpp_DEC_MEM_NORMAL &&
        g_transcode_context.memory_mode != vmpp_DEC_MEM_USER_OUT_BUF_HOST) {
        LOGIL_ERROR(
            "[transcode_mt] Incompatible Option: Saving YUV data when decoding in memory mode(%d) is unsupported!",
            g_transcode_context.memory_mode);
        // usage(argv[0], OPT_DEC);
        return -1;
    }

    if (g_transcode_context.codec && strcmp(g_transcode_context.codec, "yuv") == 0) {
        if (!g_transcode_context.encoder_enable) {
            LOGIL_ERROR("[transcode_mt] Missing Option: enc codec must be set (by '-C') when input file is YUV!");
            // usage(argv[0], OPT_ALL);
            return -1;
        }

        if (!g_transcode_context.default_enc_opts.width || !g_transcode_context.default_enc_opts.height ||
            g_transcode_context.default_enc_opts.pixelFormat == vmpp_PIX_FMT_NONE) {
            if (vmpp_queue_size(g_transcode_context.urls) > 1 &&
                vmpp_queue_size(g_transcode_context.urls) != vmpp_queue_size(g_transcode_context.customized_enc_opts)) {
                LOGIL_ERROR("[transcode_mt] Missing Options: width(%d) / height(%d) / pixelFormat(%d) is not provided, "
                          "encoding multiple(%d) YUV files (maybe with different resolution/format) "
                          "in one single process is not supported yet.",
                    g_transcode_context.default_enc_opts.width, g_transcode_context.default_enc_opts.height,
                    g_transcode_context.default_enc_opts.pixelFormat, vmpp_queue_size(g_transcode_context.urls));
                // usage(argv[0], OPT_ENC);
                return -1;
            }
        }

        if ((!g_transcode_context.default_enc_opts.width || !g_transcode_context.default_enc_opts.height) &&
            !vmpp_queue_size(g_transcode_context.customized_enc_opts)) {
            LOGIL_WARN("[transcode_mt] Missing Options: width(%d) / height(%d) is not provided, "
                       "try to parse them from input url...",
                g_transcode_context.default_enc_opts.width, g_transcode_context.default_enc_opts.height);
            if (parse_resolution_from_url((const char *)vmpp_queue_peek(g_transcode_context.urls, 0),
                    &g_transcode_context.default_enc_opts.width, &g_transcode_context.default_enc_opts.height) < 0) {
                LOGIL_ERROR("[transcode_mt] Parsing width / height from url failed, "
                          "you must provide them with '--width' & '--height'!");
                // usage(argv[0], OPT_ALL);
                return -1;
            } else {
                LOGIL_WARN("[transcode_mt] width (%d) & height (%d) parsed from url (%s) will be used!",
                    g_transcode_context.default_enc_opts.width, g_transcode_context.default_enc_opts.height,
                    (const char *)vmpp_queue_peek(g_transcode_context.urls, 0));
            }
        }
        g_transcode_context.encode_yuv = 1;
    } else if (!g_transcode_context.using_ffmpeg && !g_transcode_context.codec) {
        LOGIL_WARN("[transcode_mt] Missing Option: The input codec is not specified (by '-c')! "
                   "We will try to parse it from the url...");
    } else if (g_transcode_context.using_ffmpeg) {
        LOGIL_WARN("[transcode_mt] The precompiled FFmpeg may not support all formats you need.");
    }

    if ((g_transcode_context.save || g_transcode_context.save_cu_info) &&
        (!g_transcode_context.output_directory && !g_transcode_context.output_file)) {
        LOGIL_ERROR(
            "[transcode_mt] Missing Option: output directory/filename must be specified if you want to save output data / Cu Info!");
        // usage(argv[0], OPT_ALL);
        return -1;
    }

    if ((g_transcode_context.save || g_transcode_context.save_cu_info) && g_transcode_context.thread_count > 1 &&
        !g_transcode_context.output_directory) {
        LOGIL_ERROR(
            "[transcode_mt] Incompatible Option: Save stream/yuv(%d) or Cu Info(%d) is enabled for multi-thread(%d). "
            "What is needed for '-o' is a folder name instead of a file name.",
            g_transcode_context.save, g_transcode_context.save_cu_info, g_transcode_context.thread_count);
        // usage(argv[0], OPT_ALL);
        return -1;
    }

    if (g_transcode_context.dec_crop == 2 && !g_transcode_context.dec_crop_info) {
        LOGIL_ERROR("[transcode_mt] Missing Option: Crop info is needed for customized crop (by 'decCropInfo').");
        // usage(argv[0], OPT_DEC);
        return -1;
    }

    return 0;
}

static unsigned char *compute_md5sum(
    transcode_thread_params *args, const unsigned char *buf_in, const int buf_len, unsigned char *str_md5sum)
{
    int to_final = 0;
    if (buf_in == NULL || buf_len == 0) {
        to_final = 1;
    }

    /* Initialization of MD5 context. */
    if (args->md5ctx_inited == 0) {
        md5_init(&args->md5ctx);
        args->md5ctx_inited = 1;
    }

    if (to_final == 0) {
        md5_update(&args->md5ctx, buf_in, (unsigned long)buf_len);
    } else {
        md5_final(str_md5sum, &args->md5ctx);
        args->md5ctx_inited = 0;
    }
    return str_md5sum;
}

static void check_enc_params(transcode_thread_params *args, enc_options *opts, vmppFrame *frame)
{
    opts->width = frame->cropInfo.width;
    opts->height = frame->cropInfo.height;
    if (g_transcode_context.another_thread_for_enc_out && !args->enc_out_index &&
        g_transcode_context.max_queued_stream) {
        LOG_DEBUG("[transcode_mt %3d] add extra number(maxQueuedStream %d) to outbufNum(%d)", args->thread_index,
            g_transcode_context.max_queued_stream, opts->outbufNum);
        opts->outbufNum += g_transcode_context.max_queued_stream;
    }
}

static void save_yuv(vmppFrame *frame, int cropped, FILE *yuv_file)
{
    if (cropped) {
        // write Y
        fwrite(frame->data[0], 1, frame->cropInfo.width * frame->cropInfo.height, yuv_file);

        // write UV
        if (frame->data[1]) {
            uint32_t uv_crop_width =
                (frame->cropInfo.width % 2 == 1) ? (frame->cropInfo.width + 1) : (frame->cropInfo.width);
            uint32_t uv_crop_height =
                (frame->cropInfo.height % 2 == 1) ? (frame->cropInfo.height + 1) : (frame->cropInfo.height);

            fwrite(frame->data[1], 1, uv_crop_width * uv_crop_height / 2, yuv_file);
        }
    } else {
        // write Y
        for (uint32_t j = 0; j < frame->cropInfo.height; j++) {
            fwrite(frame->data[0] + frame->width * j, 1, frame->cropInfo.width, yuv_file);
        }

        // write UV
        if (frame->data[1]) {
            uint32_t uv_crop_width =
                (frame->cropInfo.width % 2 == 1) ? (frame->cropInfo.width + 1) : (frame->cropInfo.width);
            uint32_t uv_crop_height =
                (frame->cropInfo.height % 2 == 1) ? (frame->cropInfo.height + 1) : (frame->cropInfo.height);
            for (uint32_t j = 0; j < uv_crop_height / 2; j++) {
                fwrite(frame->data[1] + frame->width * j, 1, uv_crop_width, yuv_file);
            }
        }
    }
}

static int create_and_start_encoder(transcode_thread_params *args)
{
    int ret;
    ret = encoder_task_init(args);
    if (ret != 0) {
        LOG_ERROR("[transcode_mt %3d] fail to init encode thread %d", args->thread_index, ret);
        goto fail2start_encoder;
    } else {
        args->encoder_inited = 1;
        if (g_transcode_context.another_thread_for_enc_out) {
            ret = pthread_create(&args->enc_out_thread_handle, NULL, enc_output_thread, args);
            if (ret != 0) {
                LOG_ERROR("[transcode_mt %3d] fail to start encode output thread %d", args->thread_index, ret);
                args->encoder_inited = 0;
                goto fail2start_encoder;
            } else {
                LOG_DEBUG("[transcode_mt %3d] enc output thread started.", args->thread_index);
            }
        }
        ret = pthread_create(&args->enc_thread_handle, NULL, enc_thread, args);
        if (ret != 0) {
            LOG_ERROR("[transcode_mt %3d] fail to start encode thread %d", args->thread_index, ret);
            args->encoder_inited = 0;
            goto fail2start_encoder;
        } else {
            LOG_DEBUG("[transcode_mt %3d] enc thread started.", args->thread_index);
        }
    }
    return 0;
fail2start_encoder:
    args->enc_error = 1;
    return -1;
}

static int do_dec_output(void *params, int parallel)
{
    vmppFrame out_frame;
    vmppDecOutputOptions out_opt;
    void *dec_ch;
    vmppResult ret;
    char output_file_str[MAX_PATH_LEN * 2] = { 0 };
    uint64_t file_decode_time = 0;
    uint64_t write_start;
    uint64_t md5_start;
    int out_ret = 0;
    uint32_t last_dec_width = 0;
    uint32_t last_dec_height = 0;
    usrbuf *ub = NULL;
    transcode_thread_params *args = (transcode_thread_params *)params;
    dec_ch = args->dec_ch;

    out_opt.memoryType = (vmppMemoryType)g_transcode_context.real_recv_mem_type;
    out_opt.enableCrop = g_transcode_context.dec_crop == 3;
    LOG_TRACE("[transcode_mt %3d] Output Options: memoryType %d, enableCrop %d", args->thread_index, out_opt.memoryType,
        out_opt.enableCrop);

    do {
        memset(&out_frame, 0, sizeof(out_frame));
        if (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_DEV ||
            g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_HOST) {
            int buf_size = 0;
            vmppDecStreamInfo strm_info = { 0 };
            ret = vmppDecGetStreamInfo(dec_ch, &strm_info);
            if (ret == vmpp_RSLT_OK) {
                buf_size = strm_info.width * strm_info.height * 3;    // reserved more for 10 bit stream
                buf_size = NEXT_MULTIPLE(buf_size, 4096);
            } else {
                LOG_WARN("[transcode_mt %3d] get stream info FAILED: %d", args->thread_index, ret);
            }

            if (!buf_size) {
                /* stream info is not ready! */
                LOG_TRACE("[transcode_mt %3d] invalid buf size(%d) width=%d, height=%d.", args->thread_index, buf_size,
                    strm_info.width, strm_info.height);
                usleep(1);
                continue;
            }

            if (buf_size > usrbuf_factory_active_size(&args->ubf)) {
                LOG_DEBUG("[transcode_mt %3d] the required size(%d) is bigger than the active size(%d), do update.",
                    args->thread_index, buf_size, usrbuf_factory_active_size(&args->ubf));
                if (usrbuf_factory_new_size(&args->ubf, buf_size) < 0) {
                    LOG_ERROR("[transcode_mt %3d] set new (%s)buffer size(%d) output buffer failed.",
                        args->thread_index, UBT(&args->ubf), buf_size);
                    break;
                }
            }

            ub = usrbuf_factory_request_buf(&args->ubf);
            if (!ub) {
                LOG_WARN(
                    "[transcode_mt %3d] Fail to request (%s)buffer for decoder", args->thread_index, UBT(&args->ubf));
                return -1;
            }
            LOG_TRACE("[transcode_mt %3d] request usr (%s)buffer: 0x%llx", args->thread_index, UBT(&args->ubf),
                (u64)ub->bus_addr);
            if (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_DEV) {
                out_frame.busAddress[0] = ub->bus_addr;
            } else {
                out_frame.data[0] = (uint8_t *)ub->virt_addr;
            }
        }

        out_ret = 1;

        LOG_TRACE("[transcode_mt %3d] call vmppDecReceiveFrame: opt.memoryType %d, opt.enableCrop %d",
            args->thread_index, out_opt.memoryType, out_opt.enableCrop);
        ret = vmppDecReceiveFrame(dec_ch, &out_frame, &out_opt, 500);
        if (ret == vmpp_RSLT_OK) {
            out_ret = 0;
            args->dec_out_count++;
            args->period_out_count++;
            if (g_transcode_context.collect_latency && !args->dec_first_latency_us) {
                args->dec_first_latency_us = (gettime_ns() - args->dec_first_send_timestamp) / NANOSEC_PER_MICROSEC;
                LOG_DEBUG("[transcode_mt %3d] dec latency for the first frame: %llu us", args->thread_index,
                    (u64)args->dec_first_latency_us);
            }

            if (g_transcode_context.dec_api_mode == vmpp_DEC_API_MODE_SERIAL && g_transcode_context.collect_latency &&
                args->dec_first_latency_frames < 0) {
                /* It's not reliable to collect frame latency when 'send' and 'receive' running in different threads. */
                args->dec_first_latency_frames = args->dec_in_count;
                LOG_DEBUG(
                    "[transcode_mt %3d] dec latency in frames: %d", args->thread_index, args->dec_first_latency_frames);
            }

            if (!args->dec_width || !args->dec_height) {
                args->dec_width = out_frame.cropInfo.width;
                args->dec_height = out_frame.cropInfo.height;
                if (g_transcode_context.save && out_frame.memoryType == vmpp_MEM_HOST && !args->dec_output_file) {
                    generate_output_url(args, output_file_str, "yuv");
                    args->dec_output_file = fopen(output_file_str, "wb");
                    if (!args->dec_output_file) {
                        LOG_WARN(
                            "[transcode_mt %3d] Fail to open output file '%s'", args->thread_index, output_file_str);
                        args->output_error = 1;
                    }
                }
            } else {
                if (args->dec_output_file && !g_transcode_context.unique_output_file &&
                    (args->dec_width != out_frame.cropInfo.width || args->dec_height != out_frame.cropInfo.height)) {
                    fflush(args->dec_output_file);
                    fclose(args->dec_output_file);
                    memset(output_file_str, 0, MAX_PATH_LEN * 2);
                    args->dec_out_index++;
                    last_dec_width = args->dec_width;
                    last_dec_height = args->dec_height;
                    args->dec_width = out_frame.cropInfo.width;
                    args->dec_height = out_frame.cropInfo.height;
                    generate_output_url(args, output_file_str, "yuv");
                    LOG_INFO("[transcode_mt %3d] resolution changed (%dx%d -> %dx%d), open another output file '%s'",
                        args->thread_index, last_dec_width, last_dec_height, out_frame.cropInfo.width,
                        out_frame.cropInfo.height, output_file_str);
                    args->dec_output_file = fopen(output_file_str, "wb");
                    if (!args->dec_output_file) {
                        LOG_WARN("[transcode_mt %3d] Fail to open output file[%d] '%s'", args->thread_index,
                            args->dec_out_index, output_file_str);
                        args->output_error = 1;
                    }
                } else {
                    args->dec_width = out_frame.cropInfo.width;
                    args->dec_height = out_frame.cropInfo.height;
                }
            }

            LOG_TRACE("[transcode_mt %3d] vmppDecReceiveFrame (%lu) succeed. pts %lld, "
                      "frame_type %s, format %d, ori_wh: %dx%d, crop_wh: %dx%d",
                args->thread_index, args->dec_out_count, (u64)out_frame.pts, FRAME(out_frame.frameType),
                out_frame.pixelFormat, out_frame.width, out_frame.height, out_frame.cropInfo.width,
                out_frame.cropInfo.height);

            if (args->period_out_count % g_transcode_context.period == 0) {
                file_decode_time = gettime_ns() - args->period_file_start_time;
                LOG_INFO("[transcode_mt %3d] Dec Performance: %.3f fps for recent %lu frames, "
                         "decode %d us (%d us/f), output %dx%d, seiCount %d",
                    args->thread_index,
                    ((float)args->period_out_count / ((float)file_decode_time / (float)NANOSEC_PER_SEC)),
                    args->period_out_count, (int)(file_decode_time / NANOSEC_PER_MICROSEC),
                    (int)(file_decode_time / args->period_out_count / NANOSEC_PER_MICROSEC), out_frame.width,
                    out_frame.height, out_frame.seiCount);
                args->period_out_count = 0;
                args->period_file_start_time = gettime_ns();
            }
            if (g_transcode_context.encoder_enable == 0) {
                /* check md5. */
                if (g_transcode_context.check_md5 && out_frame.memoryType == vmpp_MEM_HOST) {
                    md5_start = gettime_ns();
                    compute_md5sum(
                        args, (const unsigned char *)out_frame.data[0], out_frame.dataSize, args->cur_md5sum);
                    args->check_md5_time += (gettime_ns() - md5_start);
                }
                /* save output file. */
                if (/*out_count % g_transcode_context.period == 0 && */ g_transcode_context.save &&
                    args->dec_output_file && out_frame.memoryType == vmpp_MEM_HOST) {
                    write_start = gettime_ns();
                    save_yuv(&out_frame, out_opt.enableCrop, args->dec_output_file);
                    args->file_write_time += (gettime_ns() - write_start);
                }

                LOG_TRACE("[transcode_mt %3d] vmppDecReleaseFrame NE, privateData %p.", args->thread_index,
                    out_frame.privateData);
                ret = vmppDecReleaseFrame(dec_ch, &out_frame, 500);
                if (ret < 0) {
                    LOG_WARN("[transcode_mt %3d] release frame error %d", args->thread_index, ret);
                }
                if (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_DEV ||
                    g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_AS_HWOUT) {
                    LOG_TRACE("[transcode_mt %3d] return usrbuf(DEV) NE: 0x%llx.", args->thread_index,
                        (u64)out_frame.busAddress[0]);
                    return_usrbuf(args, (uint64_t)out_frame.busAddress[0]);
                    out_frame.busAddress[0] = (vmppDevAddr)NULL;
                } else if (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_HOST) {
                    LOG_TRACE("[transcode_mt %3d] return usrbuf(HOST) NE: 0x%llx.", args->thread_index,
                        (u64)out_frame.data[0]);
                    return_usrbuf(args, (uint64_t)out_frame.data[0]);
                    out_frame.data[0] = NULL;
                }
            } else {
                vmppFrame *tmp = NULL;
                if (args->encoder_inited == 0) {
                    check_enc_params(args, &args->current_enc_opts, &out_frame);
                    LOG_TRACE("[transcode_mt %3d] create and start encoder!", args->thread_index);
                    ret = (vmppResult)create_and_start_encoder(args);
                    if (ret < 0) {
                        LOG_ERROR("[transcode_mt %3d] Failed to create and start encoder!", args->thread_index);
                    }
                }

                if (args->enc_error) {
                    LOG_DEBUG("[transcode_mt %3d] vmppDecReleaseFrame DIRECTLY due to enc error, privateData %p.",
                        args->thread_index, out_frame.privateData);
                    ret = vmppDecReleaseFrame(dec_ch, &out_frame, 500);
                    if (ret < 0) {
                        LOG_WARN("[transcode_mt %3d] release frame error %d", args->thread_index, ret);
                    }
                    if (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_DEV ||
                        g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_AS_HWOUT) {
                        LOG_TRACE("[transcode_mt %3d] return usrbuf(DEV): 0x%llx.", args->thread_index,
                            (u64)out_frame.busAddress[0]);
                        return_usrbuf(args, (uint64_t)out_frame.busAddress[0]);
                        out_frame.busAddress[0] = (vmppDevAddr)NULL;
                    } else if (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_HOST) {
                        LOG_TRACE("[transcode_mt %3d] return usrbuf(HOST): 0x%llx.", args->thread_index,
                            (u64)out_frame.data[0]);
                        return_usrbuf(args, (uint64_t)out_frame.data[0]);
                        out_frame.data[0] = NULL;
                    }
                } else {
                    pthread_mutex_lock(&args->frame_mutex);
                    if (vmpp_queue_size(args->idle_frame_queue)) {
                        tmp = (vmppFrame *)vmpp_queue_pop_front(args->idle_frame_queue);
                    } else {
                        tmp = (vmppFrame *)malloc(sizeof(vmppFrame));
                    }
                    if (tmp) {
                        memcpy(tmp, &out_frame, sizeof(vmppFrame));
                        vmpp_queue_push_back(args->frame_queue, tmp);
                    } else {
                        LOG_ERROR("[transcode_mt %3d] Fail to malloc frame.", args->thread_index);
                        assert(0);
                    }

                    if (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_AS_HWOUT ||
                        g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_HOST ||
                        g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_DEV) {
                        /* We could return out_frame here in these memory mode */
                        LOG_TRACE("[transcode_mt %3d] vmppDecReleaseFrame, privateData %p.", args->thread_index,
                            out_frame.privateData);
                        ret = vmppDecReleaseFrame(dec_ch, &out_frame, 500);
                        if (ret < 0) {
                            LOG_WARN("[transcode_mt %3d] release frame error %d", args->thread_index, ret);
                        }
                    }
                    pthread_mutex_unlock(&args->frame_mutex);
                }
            }
            continue;
        } else if (ret == vmpp_RSLT_WARN_EOS) {
            LOG_TRACE(
                "[transcode_mt %3d] vmppDecReceiveFrame EOS (%llu)!!!", args->thread_index, (u64)args->dec_out_count);
            args->file_stop_time = gettime_ns();
            if (ub && (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_HOST ||
                          g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_DEV)) {
                LOG_TRACE("[transcode_mt %3d] return usr buffer(%s) EOS: 0x%lx", args->thread_index, UBT(&args->ubf),
                    ub->bus_addr);
                usrbuf_factory_return_buf(&args->ubf, ub);
                ub = NULL;
            }
            LOG_DEBUG("[transcode_mt %3d] exit dec output thread", args->thread_index);
            break;
        } else if (ret == vmpp_RSLT_WARN_MORE_DATA) {
            if (args->wait_flush_decoder) {
                args->wait_flush_decoder = 0;
            }
            if (ub && (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_HOST ||
                          g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_DEV)) {
                LOG_TRACE("[transcode_mt %3d] return usr buffer(%s) MD: 0x%lx", args->thread_index, UBT(&args->ubf),
                    ub->bus_addr);
                usrbuf_factory_return_buf(&args->ubf, ub);
                ub = NULL;
            }
            usleep(1000);
        } else {
            LOG_ERROR("[transcode_mt %3d] receive frame error %d", args->thread_index, ret);
            args->dec_error = 1;
            if (ub && (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_HOST ||
                          g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_DEV)) {
                LOG_TRACE("[transcode_mt %3d] return usr buffer(%s) ERR: 0x%lx", args->thread_index, UBT(&args->ubf),
                    ub->bus_addr);
                usrbuf_factory_return_buf(&args->ubf, ub);
                ub = NULL;
            }
            break;
        }
    } while (parallel);

    if (args->wait_flush_decoder) {
        args->wait_flush_decoder = 0;
    }

    return out_ret;
}

static void *dec_output_thread(void *params)
{
    do_dec_output(params, 1);
    return NULL;
}

static void reset_args(transcode_thread_params *args)
{
    LOG_TRACE("[transcode_mt %3d] reset some arguments!", args->thread_index);
    args->file_start_time = 0;
    args->file_read_time = 0;
    args->file_write_time = 0;
    args->file_stop_time = 0;
    args->check_md5_time = 0;
    args->md5ctx_inited = 0;
    args->input_error = 0;
    args->output_error = 0;
    args->dec_error = 0;
    args->enc_error = 0;
    args->enc_total_frames = 0;
    args->enc_save_time = 0;
    args->dec_finished = 0;
    args->encoder_inited = 0;
    args->psnr_num = 0;
    args->ssim_num = 0;
    args->dec_width = 0;
    args->dec_height = 0;
    args->force_flush_encoder = 0;
    args->wait_flush_decoder = 0;
    args->dec_in_count = 0;
    args->dec_out_count = 0;
    args->period_out_count = 0;
    args->dec_out_index = 0;
    args->enc_out_index = 0;
    args->dec_device_handle = -1;
    args->enc_device_handle = -1;
    args->enc_output_file = NULL;
    args->enc_cu_info_file = NULL;
    args->dec_ch = NULL;
    args->enc_ch = NULL;
    args->frame_queue = NULL;
    args->idle_frame_queue = NULL;
    args->releasing_frame_queue = NULL;
    args->stream_queue = NULL;
    args->idle_stream_queue = NULL;
    memset(args->cur_md5sum, 0, sizeof(uint8_t) * MD5_HASH_LEN);
    memset(&args->md5ctx, 0, sizeof(struct md5_context));
    memset(args->psnr_total, 0, sizeof(double) * 3);
    usrbuf_factory_reset(&args->ubf);
    args->last_width = 0;
    args->last_height = 0;
    args->enc_stop = 0;
    if (args->enc_cu_info_buffer) {
        free(args->enc_cu_info_buffer);
        args->enc_cu_info_buffer = NULL;
    }
    args->enc_cu_info_buffer_size = 0;
    args->enc_cu_info_number = 0;
    args->cu_info_parse_time = 0;
    args->cu_info_save_time = 0;
    args->raw_frame_count = 0;
    args->dec_output_file = NULL;
    args->total_latency_us = 0;
    args->max_latency_us = 0;
    args->min_latency_us = 0;
    args->latency_count = 0;
    args->dec_first_latency_us = 0;
    args->dec_first_latency_frames = -1;
    args->dec_first_send_timestamp = 0;
    args->dec_discarded_frame_count = 0;
    args->dec_notshow_frame_count = 0;
    args->dec_skipped_frame_count = 0;
}

static int get_device_id(const char *video_device);
static vmppRuntimeInstance *runtime_setup(const char *video_device);
static void runtime_cleanup(vmppRuntimeInstance *runtime_inst);
void log_cb(
    const void *pUser, int level, const char *module, const char *file, const char *func, int line, const char *msg);

static int prepare_for_encoder(transcode_thread_params *args, vmppRuntimeInstance **p_runtime_inst_enc)
{
    int pic_size = 0, comp1_size = 0, comp2_size = 0, comp3_size = 0, i;
    vmppFrame *tmp;
    usrbuf *ub = NULL;
    vmppResult ret;
    vmppConfiguration cfg = { 0 };
    vmppRuntimeInstance *runtime_inst_enc = NULL;
    if (vmpp_queue_init(&args->frame_queue) < 0) {
        LOG_ERROR("[transcode_mt %3d] Failed to init frame queue!", args->thread_index);
        goto fail2prepare_encoder;
    }
    if (vmpp_queue_init(&args->idle_frame_queue) < 0) {
        LOG_ERROR("[transcode_mt %3d] Failed to init idle frame queue!", args->thread_index);
        goto fail2prepare_encoder;
    }
    if (vmpp_queue_init(&args->releasing_frame_queue) < 0) {
        LOG_ERROR("[transcode_mt %3d] Failed to init releasing frame queue!", args->thread_index);
        goto fail2prepare_encoder;
    }

    if (g_transcode_context.another_thread_for_enc_out) {
        if (vmpp_queue_init(&args->stream_queue) < 0) {
            LOG_ERROR("[transcode_mt %3d] Failed to init stream queue!", args->thread_index);
            goto fail2prepare_encoder;
        }

        if (vmpp_queue_init(&args->idle_stream_queue) < 0) {
            LOG_ERROR("[transcode_mt %3d] Failed to init idle stream queue!", args->thread_index);
            goto fail2prepare_encoder;
        }
    }

    if (g_transcode_context.encode_yuv) {
        pic_size = raw_pic_size(args->rawctx, &comp1_size, &comp2_size, &comp3_size);
        LOG_DEBUG("[transcode_mt %3d] setup usrbuf factory(HOST): rt(nullptr), dev_id(0)!", args->thread_index);
        if (usrbuf_factory_setup(&args->ubf, UBT_HOST, 0, NULL) < 0) {
            LOG_ERROR("[transcode_mt %3d] setup user buffer factory(HOST) for YUV input failed.", args->thread_index);
            goto fail2prepare_encoder;
        }

        LOG_TRACE("[transcode_mt %3d] update size(%d) for user buf factory(HOST)!", args->thread_index, pic_size);
        if (usrbuf_factory_new_size(&args->ubf, pic_size) < 0) {
            LOG_ERROR(
                "[transcode_mt %3d] set new buffer(HOST) size(%d) for YUV input failed.", args->thread_index, pic_size);
            goto fail2prepare_encoder;
        }

        for (i = 0; i < g_transcode_context.max_queued_frame; i++) {
            tmp = (vmppFrame *)malloc(sizeof(vmppFrame));
            if (!tmp) {
                LOG_ERROR("[transcode_mt %3d] Failed to alloc frame[%d] %s", args->thread_index, i, args->current_url);
                goto fail2prepare_encoder;
            }
            memset(tmp, 0, sizeof(vmppFrame));
            ub = usrbuf_factory_request_buf(&args->ubf);
            if (!ub) {
                LOG_ERROR("[transcode_mt %3d] request new buffer(HOST) for frame[%d] for %s failed", args->thread_index,
                    i, args->current_url);
                free(tmp);
                goto fail2prepare_encoder;
            }
            LOG_TRACE(
                "[transcode_mt %3d] request usr buffer(HOST) for YUV: %d - %p", args->thread_index, i, ub->virt_addr);
            tmp->data[0] = (uint8_t*)ub->virt_addr;
            tmp->data[1] = tmp->data[0] + comp1_size;
            if (comp3_size) {
                tmp->data[2] = tmp->data[1] + comp2_size;
            }
            vmpp_queue_push_back(args->idle_frame_queue, tmp);
        }
    }

    if (g_transcode_context.multi_runtime) {
        LOG_TRACE("[transcode_mt %3d] setup runtime for encoder!", args->thread_index);
        runtime_inst_enc = runtime_setup(args->current_device);
        if (!runtime_inst_enc) {
            LOG_ERROR("[transcode_mt %3d] Failed to setup runtime for encoder!", args->thread_index);
            goto fail2prepare_encoder;
        }
        *p_runtime_inst_enc = runtime_inst_enc;
        memset(&cfg, 0, sizeof(vmppConfiguration));
        cfg.runtimeInst = *runtime_inst_enc;
        cfg.logCtx.enableCustomLog = 1;
        cfg.logCtx.logLevel = (vmppLogLevel)g_transcode_context.log_level_sdk;
        if (g_transcode_context.log_callback) {
            cfg.logCtx.logCallback = log_cb;
        }

        ret = vmppInitEncoder(&cfg);
        if (ret != vmpp_RSLT_OK) {
            LOG_ERROR("[transcode_mt %3d] vmppInitEncoder failed %d", args->thread_index, ret);
            goto fail2prepare_encoder;
        }
    }

    args->enc_device_handle = open(args->current_device, O_RDWR);
    if (args->enc_device_handle < 0) {
        LOG_ERROR(
            "[transcode_mt %3d] Cannot open video device { %s } for Encoder", args->thread_index, args->current_device);
        goto fail2prepare_encoder;
    }

    if (args->current_enc_opts.lookaheadDepth &&
        0 == strncmp(args->current_device, "/dev/va_video", strlen("/dev/va_video"))) {
        // !!!NOTE
        /* using device name to check chip type is not always reliable. */
        LOG_WARN("[transcode_mt %3d] Two pass is not supported by SG100, disable it.", args->thread_index);
        args->current_enc_opts.lookaheadDepth = 0;
    }

    if (g_transcode_context.encode_yuv && args->encoder_inited == 0) {
        if (g_transcode_context.another_thread_for_enc_out && g_transcode_context.max_queued_stream) {
            args->current_enc_opts.outbufNum += g_transcode_context.max_queued_stream;
        }
        LOG_TRACE("[transcode_mt %3d] create and start encoder(YUV)!", args->thread_index);
        ret = (vmppResult)create_and_start_encoder(args);
        if (ret < 0) {
            LOG_ERROR("[transcode_mt %3d] Failed to create and start encoder!", args->thread_index);
            goto fail2prepare_encoder;
        }
    }
    return 0;
fail2prepare_encoder:
    args->enc_error = 1;
    return -1;
}

static int prepare_for_decoder(transcode_thread_params *args, vmppRuntimeInstance **p_runtime_inst_dec)
{
    vmppResult ret;
    vmppRuntimeInstance *runtime_inst_dec = NULL, *rt_temp = NULL;
    vmppConfiguration cfg = { 0 };
    USR_BUF_TYPE ubt = UBT_NONE;
    int device_id;
    LOG_DEBUG("[transcode_mt %3d] open device: { %s }", args->thread_index, args->current_device);
    args->dec_device_handle = open(args->current_device, O_RDWR);
    if (args->dec_device_handle < 0) {
        LOG_ERROR(
            "[transcode_mt %3d] Cannot open video device { %s } for Decoder", args->thread_index, args->current_device);
        goto fail2prepare_decoder;
    }
    if (g_transcode_context.multi_runtime) {
        LOG_DEBUG("[transcode_mt %3d] setup runtime for decoder", args->thread_index);
        runtime_inst_dec = runtime_setup(args->current_device);
        if (!runtime_inst_dec) {
            LOG_ERROR("[transcode_mt %3d] Failed to setup runtime for decoder!", args->thread_index);
            goto fail2prepare_decoder;
        }

        *p_runtime_inst_dec = runtime_inst_dec;

        memset(&cfg, 0, sizeof(vmppConfiguration));
        cfg.runtimeInst = *runtime_inst_dec;
        cfg.logCtx.enableCustomLog = 1;
        cfg.logCtx.logLevel = (vmppLogLevel)g_transcode_context.log_level_sdk;
        if (g_transcode_context.log_callback) {
            cfg.logCtx.logCallback = log_cb;
        }

        ret = vmppInitDecoder(&cfg);
        if (ret != vmpp_RSLT_OK) {
            LOG_ERROR("[transcode_mt %3d] vmppInitDecoder failed %d", args->thread_index, ret);
            goto fail2prepare_decoder;
        }

        rt_temp = runtime_inst_dec;
    } else {
        rt_temp = &g_runtime_instance;
    }

    if (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_DEV ||
        g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_HOST ||
        g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_AS_HWOUT) {
        switch (g_transcode_context.memory_mode) {
        case vmpp_DEC_MEM_USER_OUT_BUF_HOST:
            ubt = UBT_HOST;
            break;
        case vmpp_DEC_MEM_USER_AS_HWOUT:
        case vmpp_DEC_MEM_USER_OUT_BUF_DEV:
            ubt = UBT_BUS_ADDR;
            break;
        default:
            ubt = UBT_NONE;
            break;
        }

        device_id = get_device_id(args->current_device);
        LOG_DEBUG("[transcode_mt %3d] setup usrbuf factory(%s): rt(%p), dev_id(%d)!", args->thread_index, UBTSTR[ubt],
            rt_temp, device_id);
        if (usrbuf_factory_setup(&args->ubf, ubt, device_id, rt_temp) < 0) {
            LOG_ERROR("[transcode_mt %3d] setup user buffer factory failed.", args->thread_index);
            goto fail2prepare_decoder;
        }
    }

    return 0;
fail2prepare_decoder:
    args->dec_error = 1;
    return -1;
}

int prepare_for_input(transcode_thread_params *args, vmppDecChannelParameters *p_dec_params)
{
    LOG_TRACE("[transcode_mt %3d] do preparation for input!", args->thread_index);
    int strmtype = BIT_STREAM_JPEG, ret;
    struct raw_context *rawctx_temp = NULL;

    if (g_transcode_context.encode_yuv) {
        LOG_TRACE("[transcode_mt %3d] raw yuv encoder is enabled, create raw context!", args->thread_index);
        rawctx_temp = (struct raw_context *)calloc(1, sizeof(struct raw_context));
        if (!rawctx_temp) {
            LOG_ERROR("[transcode_mt %3d] Failed to malloc raw context for %s", args->thread_index, args->current_url);
            args->input_error = 1;
            goto fail2prepare_input;
        }
        args->rawctx = rawctx_temp;
        /* open input yuv */
        ret = raw_open(args->current_url, args->current_enc_opts.pixelFormat, args->current_enc_opts.width,
            args->current_enc_opts.height, args->current_enc_opts.width, rawctx_temp);
        if (ret < 0) {
            LOG_ERROR("[transcode_mt %3d] Failed to open input file %s", args->thread_index, args->current_url);
            args->input_error = 1;
            goto fail2prepare_input;
        }
    } else {
        if (args->using_ffmpeg) {
#ifdef USING_FFMPEG
            LOG_TRACE("[transcode_mt %3d] open input file with FFmpeg!", args->thread_index);
            args->ffctx = ff_open2(args->current_url);
            if (!args->ffctx) {
                LOG_ERROR("[transcode_mt %3d] Unable to open input file '%s' through FFmpeg", args->thread_index,
                    args->current_url);
                args->input_error = 1;
                goto fail2prepare_input;
            }

            if (ff_video_codec(args->ffctx) == AV_CODEC_ID_H264) {
                p_dec_params->codecType = vmpp_CODEC_DEC_H264;
            } else if (ff_video_codec(args->ffctx) == AV_CODEC_ID_HEVC) {
                p_dec_params->codecType = vmpp_CODEC_DEC_HEVC;
            } else if (ff_video_codec(args->ffctx) == AV_CODEC_ID_AV1) {
                p_dec_params->codecType = vmpp_CODEC_DEC_AV1;
            } else if (ff_video_codec(args->ffctx) == AV_CODEC_ID_AVS2) {
                p_dec_params->codecType = vmpp_CODEC_DEC_AVS2;
            } else if (ff_video_codec(args->ffctx) == AV_CODEC_ID_VP9) {
                p_dec_params->codecType = vmpp_CODEC_DEC_VP9;
            } else if (ff_video_codec(args->ffctx) == AV_CODEC_ID_MJPEG) {
                args->jpeg_stream = 1;
                p_dec_params->codecType = vmpp_CODEC_DEC_JPEG;
            } else {
                LOG_ERROR("[transcode_mt %3d] Unsupported codec :%d", args->thread_index, ff_video_codec(args->ffctx));
                args->input_error = 1;
                goto fail2prepare_input;
            }
#endif
        } else {
            if (args->jpeg_stream || !strcmp(args->codec, "jpeg")) {
                p_dec_params->codecType = vmpp_CODEC_DEC_JPEG;
                strmtype = BIT_STREAM_JPEG;
            } else if (!strcmp(args->codec, "hevc") || !strcmp(args->codec, "h265")) {
                p_dec_params->codecType = vmpp_CODEC_DEC_HEVC;
                strmtype = BIT_STREAM_HEVC;
            } else if (!strcmp(args->codec, "h264") || !strcmp(args->codec, "avc")) {
                p_dec_params->codecType = vmpp_CODEC_DEC_H264;
                strmtype = BIT_STREAM_H264;
            } else if (!strcmp(args->codec, "av1")) {
                p_dec_params->codecType = vmpp_CODEC_DEC_AV1;
                strmtype = BIT_STREAM_AV1;
            } else if (!strcmp(args->codec, "vp9")) {
                p_dec_params->codecType = vmpp_CODEC_DEC_VP9;
                strmtype = BIT_STREAM_VP9;
            } else if (!strcmp(args->codec, "avs2")) {
                p_dec_params->codecType = vmpp_CODEC_DEC_AVS2;
                strmtype = BIT_STREAM_AVS2;
            } else {
                LOG_ERROR("[transcode_mt %3d] Unsupported codec :%s", args->thread_index, args->codec);
                args->input_error = 1;
                goto fail2prepare_input;
            }
            args->strmctx = stream_open(args->current_url, strmtype);
            if (!args->strmctx) {
                LOG_ERROR("[transcode_mt %3d] Unable to open input file '%s'", args->thread_index, args->current_url);
                args->input_error = 1;
                goto fail2prepare_input;
            }
            if (args->strmctx->type == BIT_STREAM_VP9) {
                p_dec_params->codecType = vmpp_CODEC_DEC_VP9;
            }
        }
        args->src_codec = p_dec_params->codecType;
    }

    return 0;
fail2prepare_input:
    return -1;
}

int create_and_start_decoder(transcode_thread_params *args, vmppDecChannelParameters *p_dec_params)
{
    vmppResult ret = vmpp_RSLT_OK;
    uint32_t encoderDelayNum = 0;
    int chns;
    chns = vmppDecGetAvailableChannels(args->dec_device_handle, p_dec_params->codecType);
    if (chns <= 0) {
        LOG_WARN(
            "[transcode_mt %3d] Fail to get available channel count(%d) for '%s' decoder on device { %s }, handle %d.",
            args->thread_index, chns, CODEC(p_dec_params->codecType), args->current_device, args->dec_device_handle);
        goto fail2start_decoder;
    } else {
        LOG_INFO("[transcode_mt %3d] Available channel count for decoder is %d on device { %s }, handle %d",
            args->thread_index, chns, args->current_device, args->dec_device_handle);
    }

    p_dec_params->device = args->dec_device_handle;

    if (g_transcode_context.encoder_enable && !args->jpeg_stream) {
        uint32_t maxGopSize = args->current_enc_opts.gopSize;
        if (maxGopSize == VMPP_ENC_DEFAULT_PAR) {
            if (args->current_enc_opts.lookaheadDepth > 0) {
                maxGopSize = 8;
            } else {
                maxGopSize = 1;
            }
        } else if ((maxGopSize > 8 && maxGopSize != 16) || maxGopSize == 0) {
            maxGopSize = 8;
        }
        encoderDelayNum += maxGopSize;    // for input reorder
        if (args->current_enc_opts.lookaheadDepth) {
            if (args->current_enc_opts.multicore) {
                encoderDelayNum +=
                    (args->current_enc_opts.lookaheadDepth + maxGopSize + 3);    // add 3 for 1pass multicore.
            } else {
                encoderDelayNum += MAX(maxGopSize, args->current_enc_opts.lookaheadDepth);
            }
        }
        encoderDelayNum += args->current_enc_opts.multicore ? 3 : 0;    // add 3 for multicore
        if (g_transcode_context.memory_mode != vmpp_DEC_MEM_USER_AS_HWOUT &&
            g_transcode_context.memory_mode != vmpp_DEC_MEM_USER_OUT_BUF_HOST &&
            g_transcode_context.memory_mode != vmpp_DEC_MEM_USER_OUT_BUF_DEV) {
            encoderDelayNum += g_transcode_context.max_queued_frame;
        }
    }
    p_dec_params->extraBufferNumber = encoderDelayNum + EXT_BUF_NUM;
    p_dec_params->sourceMode = vmpp_SRC_FRAME;
    p_dec_params->decodeMode = (vmppDecMode)g_transcode_context.dec_mode;
    p_dec_params->maxWidth = 32768;
    p_dec_params->maxHeight = 32768;
    p_dec_params->streamBufferSize = MAX_STREAM_SIZE;
    p_dec_params->pixelFormat = vmpp_PIX_FMT_NV12;
    p_dec_params->apiMode = (vmppDecApiMode)g_transcode_context.dec_api_mode;
    p_dec_params->noOutputReordering = g_transcode_context.dec_no_output_reordering;
    p_dec_params->outputAlign = g_transcode_context.dec_output_align;
    p_dec_params->coreMode = (vmppCoreMode)g_transcode_context.dec_core_mode;

    if (g_transcode_context.bitDepth > 8) {
        p_dec_params->pixelFormat = vmpp_PIX_FMT_YUV420_PLANAR_10BIT_P010;
    }

    p_dec_params->enProfiling = !g_transcode_context.disable_dec_profiling;
    p_dec_params->memoryMode = (vmppDecMemoryMode)g_transcode_context.memory_mode;

    if (g_transcode_context.dec_crop == 1 || g_transcode_context.dec_crop == 2) {
        p_dec_params->cropInfo.flag = vmpp_CROP_ENABLE;
        if (g_transcode_context.dec_crop == 2) {
            /*customized crop*/
            p_dec_params->cropInfo.flag = vmpp_CROP_CUSTOMIZED;
            sscanf(g_transcode_context.dec_crop_info, "x=%d,y=%d,w=%d,h=%d", &p_dec_params->cropInfo.xOffset,
                &p_dec_params->cropInfo.yOffset, &p_dec_params->cropInfo.width, &p_dec_params->cropInfo.height);
            LOG_INFO("[transcode_mt %3d] customized crop: x=%d,y=%d,w=%d,h=%d.", args->thread_index,
                p_dec_params->cropInfo.xOffset, p_dec_params->cropInfo.yOffset, p_dec_params->cropInfo.width,
                p_dec_params->cropInfo.height);
        }
    }

    ret = vmppDecCreateChannel(&args->dec_ch, p_dec_params);
    if (ret != vmpp_RSLT_OK || !args->dec_ch) {
        LOG_ERROR("[transcode_mt %3d] create channel error %d or chn is null.", args->thread_index, ret);
        goto fail2start_decoder;
    }

    LOG_DEBUG("[transcode_mt %3d] dec channel: %p for '%s' created.", args->thread_index, args->dec_ch,
        CODEC(p_dec_params->codecType));

    ret = vmppDecStart(args->dec_ch);    // set start status.
    if (ret < 0) {
        LOG_ERROR("[transcode_mt %3d] start decoder failed, error %d", args->thread_index, ret);
        goto fail2start_decoder;
    }
    if (g_transcode_context.dec_api_mode == vmpp_DEC_API_MODE_PARALLEL) {
        ret = (vmppResult)pthread_create(&args->dec_output_thread_handle, NULL, dec_output_thread, args);
        if (ret != 0) {
            LOG_ERROR("[transcode_mt %3d] fail to start output thread %d", args->thread_index, ret);
            vmppDecStop(args->dec_ch);
            vmppDecDestroyChannel(&args->dec_ch);
            goto fail2start_decoder;
        }
        LOG_DEBUG("[transcode_mt %3d] dec output thread started.", args->thread_index);
    }

    return 0;
fail2start_decoder:
    args->dec_error = 1;
    return -1;
}

static void cleanup_for_encoder(transcode_thread_params *args)
{
    int j;
    void *tmp;
    for (j = 0; j < vmpp_queue_size(args->idle_frame_queue); j++) {
        tmp = vmpp_queue_peek(args->idle_frame_queue, j);
        if (g_transcode_context.encode_yuv || g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_HOST) {
            if (((vmppFrame *)tmp)->data[0]) {
                LOG_TRACE("[transcode_mt %3d] return usrbuf(HOST) CLEAN E IDLE: 0x%llx.", args->thread_index,
                    (u64)((vmppFrame *)tmp)->data[0]);
                return_usrbuf(args, (uint64_t)((vmppFrame *)tmp)->data[0]);
                ((vmppFrame *)tmp)->data[0] = NULL;
            }
        } else if (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_DEV) {
            if (((vmppFrame *)tmp)->busAddress[0]) {
                LOG_TRACE("[transcode_mt %3d] return usrbuf(DEV) CLEAN E IDLE: 0x%llx.", args->thread_index,
                    (u64)((vmppFrame *)tmp)->busAddress[0]);
                return_usrbuf(args, (uint64_t)((vmppFrame *)tmp)->busAddress[0]);
                ((vmppFrame *)tmp)->busAddress[0] = 0;
            }
        }
        free(tmp);
    }
    vmpp_queue_free(&args->idle_frame_queue);

    for (j = 0; j < vmpp_queue_size(args->frame_queue); j++) {
        tmp = vmpp_queue_peek(args->frame_queue, j);
        if (g_transcode_context.encode_yuv || g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_HOST) {
            if (((vmppFrame *)tmp)->data[0]) {
                LOG_TRACE("[transcode_mt %3d] return usrbuf(HOST) CLEAN E FRM: 0x%llx.", args->thread_index,
                    (u64)((vmppFrame *)tmp)->data[0]);
                return_usrbuf(args, (uint64_t)((vmppFrame *)tmp)->data[0]);
                ((vmppFrame *)tmp)->data[0] = NULL;
            }
        } else if (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_DEV) {
            if (((vmppFrame *)tmp)->busAddress[0]) {
                LOG_TRACE("[transcode_mt %3d] return usrbuf(DEV) CLEAN E FRM: 0x%llx.", args->thread_index,
                    (u64)((vmppFrame *)tmp)->busAddress[0]);
                return_usrbuf(args, (uint64_t)((vmppFrame *)tmp)->busAddress[0]);
                ((vmppFrame *)tmp)->busAddress[0] = 0;
            }
        }
        free(tmp);
    }
    vmpp_queue_free(&args->frame_queue);

    for (j = 0; j < vmpp_queue_size(args->releasing_frame_queue); j++) {
        tmp = vmpp_queue_peek(args->releasing_frame_queue, j);
        if (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_DEV) {
            if (((vmppFrame *)tmp)->busAddress[0]) {
                LOG_TRACE("[transcode_mt %3d] return usrbuf(DEV) CLEAN E FRM: 0x%llx.", args->thread_index,
                    (u64)((vmppFrame *)tmp)->busAddress[0]);
                return_usrbuf(args, (uint64_t)((vmppFrame *)tmp)->busAddress[0]);
                ((vmppFrame *)tmp)->busAddress[0] = 0;
            }
        }
        free(tmp);
    }
    vmpp_queue_free(&args->releasing_frame_queue);

    if (g_transcode_context.another_thread_for_enc_out) {
        for (j = 0; j < vmpp_queue_size(args->stream_queue); j++) {
            tmp = vmpp_queue_peek(args->stream_queue, j);
            free(tmp);
        }
        vmpp_queue_free(&args->stream_queue);

        for (j = 0; j < vmpp_queue_size(args->idle_stream_queue); j++) {
            tmp = vmpp_queue_peek(args->idle_stream_queue, j);
            free(tmp);
        }
        vmpp_queue_free(&args->idle_stream_queue);
    }
}

static int set_usrbuf(transcode_thread_params *args, vmppStream *input_stream, vmppCodecType codec_type)
{
    vmppDecVideoInfo video_info;
    vmppDecJpegInfo jpeg_info;
    usrbuf *ub = NULL;
    vmppResult ret = vmpp_RSLT_OK;
    int new_size, pic_stride, bit_depth = 8, margin = 0;

    if (g_transcode_context.bitDepth > 8) {
        bit_depth = 10;
    }

    if (g_transcode_context.separate_luma_chroma) {
        /* Just to make it looks as if we have separate luma and chroma */
        margin = 128;
    }
    if (codec_type == vmpp_CODEC_DEC_JPEG) {
        memset(&jpeg_info, 0, sizeof(jpeg_info));
        ret = vmppDecGetJpegInfo(input_stream, &jpeg_info);
        if (ret == vmpp_RSLT_OK) {
            /* parameters set exist */
            if (!args->last_width || !args->last_height || args->last_width != jpeg_info.width ||
                args->last_height != jpeg_info.height) {
                pic_stride = jpeg_info.width * bit_depth * 4 / 8;
                LOG_INFO("[transcode_mt %3d] new parameters set, and resolution will change from %dx%d to %dx%d",
                    args->thread_index, args->last_width, args->last_height, jpeg_info.width, jpeg_info.height);

                new_size = pic_stride * jpeg_info.height * 3 / 2 + margin;
                if (new_size > usrbuf_factory_active_size(&args->ubf)) {
                    if (usrbuf_factory_new_size(&args->ubf, new_size) < 0) {
                        LOG_ERROR("[transcode_mt %3d] set new buffer size failed", args->thread_index);
                        goto set_usrbuf_failed;
                    }
                }

                args->last_width = jpeg_info.width;
                args->last_height = jpeg_info.height;
                args->pic_stride = pic_stride;
            }
        } else {
            LOG_ERROR("[transcode_mt %3d] stream ERR! vmppDecGetJpegInfo failed(%d)", args->thread_index, ret);
            goto set_usrbuf_failed;
        }
    } else {
        memset(&video_info, 0, sizeof(video_info));
        ret = vmppDecGetVideoInfo(input_stream, codec_type, &video_info);
        if (ret == vmpp_RSLT_OK) {
            /* parameters set exist */
            if (!args->last_width || !args->last_height || args->last_width != video_info.width ||
                args->last_height != video_info.height) {
                pic_stride = video_info.width * bit_depth * 4 / 8;
                LOG_INFO(
                    "[transcode_mt %3d] new parameters set, and resolution will change from %dx%d to %dx%d, crop %d:%dx%d",
                    args->thread_index, args->last_width, args->last_height, video_info.width, video_info.height,
                    video_info.cropFlag, video_info.cropWidth, video_info.cropHeight);

                new_size = pic_stride * video_info.height * 3 / 2 + margin;
                if (new_size > usrbuf_factory_active_size(&args->ubf)) {
                    if (usrbuf_factory_new_size(&args->ubf, new_size) < 0) {
                        LOG_ERROR("[transcode_mt %3d] set new buffer size failed", args->thread_index);
                        goto set_usrbuf_failed;
                    }
                }

                args->last_width = video_info.width;
                args->last_height = video_info.height;
                args->pic_stride = pic_stride;
            }
        } else if (usrbuf_factory_active_size(&args->ubf) == 0) {
            LOG_ERROR("[transcode_mt %3d] Stream ERR! vmppDecGetVideoInfo failed(%d)", args->thread_index, ret);
            goto set_usrbuf_failed;
        }
    }

    if (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_AS_HWOUT) {
        ub = usrbuf_factory_request_buf(&args->ubf);
        if (!ub) {
            LOG_ERROR("[transcode_mt %3d] request new buffer failed", args->thread_index);
            goto set_usrbuf_failed;
        }
    }

    if (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_AS_HWOUT) {
        input_stream->outputBusAddress[0] = ub->bus_addr;
        /* If the address for UV is in different memory block with Y, then you must set it separately. */
        if (g_transcode_context.separate_luma_chroma) {
            input_stream->outputBusAddress[1] =
                input_stream->outputBusAddress[0] + args->pic_stride / 4 * args->last_height + margin;
        } else {
            input_stream->outputBusAddress[1] = 0;
        }

        LOG_TRACE("[transcode_mt %3d] request usr buffer(DEVMEM): 0x%lx", args->thread_index,
            (uint64_t)input_stream->outputBusAddress[0]);
    }

    return 0;
set_usrbuf_failed:
    return -1;
}

static void dump_statics_info(transcode_thread_params *args)
{
    int stats_count = 0, perf_count = 0, stats_error = 0;
    uint64_t file_decode_time, file_encode_time, count_base, md5_check_time;
    float time_per_frame;
    char md5_string[MD5_HASH_LEN * 2 + 1] = { 0 };
    char msg[1024] = { 0 }, *p_msg;
    const char *temp =
        g_transcode_context.encoder_enable ? (g_transcode_context.encode_yuv ? "Encoding" : "Transcoding") : "Decoding";
    const char *src_codec = g_transcode_context.encode_yuv ? "YUV" : CODEC(args->src_codec);
    const char *dst_codec = g_transcode_context.encoder_enable ? CODEC(args->dst_codec) : "YUV";
    LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN,
        "[transcode_mt %3d] -------------------- %s Statistic [L%d/J%d] ---------------------", args->thread_index,
        temp, args->current_main_loop, args->job_index);
    LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "[transcode_mt %3d] Basic Information:", args->thread_index);
    LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "[transcode_mt %3d]  - %s -> %s", args->thread_index, src_codec, dst_codec);
    LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "[transcode_mt %3d]  - Source: %s", args->thread_index, args->current_url);
    LOGIL(
        LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "[transcode_mt %3d]  - Device: %s", args->thread_index, args->current_device);
    LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "[transcode_mt %3d] STATS:", args->thread_index);
    if (args->dec_error || args->enc_error || args->input_error || args->output_error) {
        LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_RED, "[transcode_mt %3d]  # Error Happens: Dec %d, Enc %d, In %d, Out %d",
            args->thread_index, args->dec_error, args->enc_error, args->input_error, args->output_error);
        stats_count++;
    }
    if (args->dec_out_count && args->enc_total_frames && (args->dec_out_count != args->enc_total_frames)) {
        if (args->dst_codec == vmpp_CODEC_ENC_AV1) {
            LOGIL(LOG_LEVEL_INFO, COLOR_YELLOW,
                "[transcode_mt %3d]  - Frame Count: Dec %ld NEQ vs. Enc %ld (ShowExistingFrame may exist for AV1)",
                args->thread_index, args->dec_out_count, args->enc_total_frames);
        } else {
            LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_RED, "[transcode_mt %3d]  # Frame Count: Dec %ld NEQ vs. Enc %ld",
                args->thread_index, args->dec_out_count, args->enc_total_frames);
            stats_error = 1;
        }
        stats_count++;
    }
    if (args->raw_frame_count && args->enc_total_frames && (args->raw_frame_count != args->enc_total_frames)) {
        if (args->dst_codec == vmpp_CODEC_ENC_AV1) {
            LOGIL(LOG_LEVEL_INFO, COLOR_YELLOW,
                "[transcode_mt %3d]  - Frame Count: Raw %ld NEQ vs. Enc %ld (ShowExistingFrame may exist for AV1)",
                args->thread_index, args->raw_frame_count, args->enc_total_frames);
        } else {
            LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_RED, "[transcode_mt %3d]  # Frame Count: Raw %ld NEQ vs. Enc %ld",
                args->thread_index, args->raw_frame_count, args->enc_total_frames);
            stats_error = 1;
        }
        stats_count++;
    }
    if (args->enc_cu_info_number && args->enc_total_frames && (args->enc_cu_info_number != args->enc_total_frames)) {
        LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_RED, "[transcode_mt %3d]  # Frame Count: Enc %ld NEQ vs. CuInf %ld",
            args->thread_index, args->enc_total_frames, args->enc_cu_info_number);
        stats_count++;
        stats_error = 1;
    }
    if (args->dec_discarded_frame_count || args->dec_skipped_frame_count || args->dec_notshow_frame_count) {
        memset(msg, 0, 1024);
        if (args->dec_discarded_frame_count) {
            p_msg = &msg[strlen(msg)];
            sprintf(p_msg, "%s Discarded Err %ld", strlen(msg) ? "," : "", args->dec_discarded_frame_count);
        }
        if (args->dec_skipped_frame_count) {
            p_msg = &msg[strlen(msg)];
            sprintf(p_msg, "%s Skipped NonRef %ld", strlen(msg) ? "," : "", args->dec_skipped_frame_count);
        }
        if (args->dec_notshow_frame_count) {
            p_msg = &msg[strlen(msg)];
            sprintf(p_msg, "%s AV1 NotShow %ld", strlen(msg) ? "," : "", args->dec_notshow_frame_count);
        }

        LOGIL(LOG_LEVEL_INFO, COLOR_YELLOW, "[transcode_mt %3d]  - Dec Skipped Frames:%s", args->thread_index, msg);
        stats_count++;
    }

    if (g_transcode_context.collect_latency) {
        if (!g_transcode_context.encode_yuv) {
            memset(msg, 0, 1024);
            if (args->dec_first_latency_frames > 0) {
                p_msg = &msg[strlen(msg)];
                sprintf(p_msg, ", %d in frames", (args->dec_first_latency_frames - 1));
            }

            LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "[transcode_mt %3d]  - Dec Latency: %llu us in time%s",
                args->thread_index, (u64)args->dec_first_latency_us, msg);
            stats_count++;
        }
        if (args->latency_count) {
            LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN,
                "[transcode_mt %3d]  - Enc Latency(%llu): average %llu us/f, max %llu us, min %llu us",
                args->thread_index, (u64)args->latency_count, (u64)args->total_latency_us / args->latency_count,
                (u64)args->max_latency_us, (u64)args->min_latency_us);
            stats_count++;
        }
    }

    if (g_transcode_context.default_enc_opts.enableCalcPSNR && args->psnr_num) {
        LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "[transcode_mt %3d]  - Average PSNR(%d): Y %4.2f, U %4.2f, V %4.2f",
            args->thread_index, args->psnr_num, args->psnr_total[0] / args->psnr_num,
            args->psnr_total[1] / args->psnr_num, args->psnr_total[2] / args->psnr_num);
        stats_count++;
    }
    if (g_transcode_context.default_enc_opts.enableCalcSSIM && args->ssim_num) {
        LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "[transcode_mt %3d]  - Average SSIM(%d): Y %4.2f, U %4.2f, V %4.2f",
            args->thread_index, args->ssim_num, args->ssim_total[0] / args->ssim_num,
            args->ssim_total[1] / args->ssim_num, args->ssim_total[2] / args->ssim_num);
        stats_count++;
    }

    if (g_transcode_context.check_md5) {
        if (args->check_md5_time) {
            compute_md5sum(args, NULL, 0, args->cur_md5sum);
            md5_hexstring(args->cur_md5sum, MD5_HASH_LEN, md5_string);
            if ((args->md5_saved || g_transcode_context.target_md5) && vmpp_queue_size(g_transcode_context.urls) == 1 &&
                vmpp_queue_size(g_transcode_context.customized_enc_opts) <= 1) {
                /* Only do MD5 comparison for single input and unique enc options,
                 * because there's no meaning to compare MD5 for different files (or with different enc options) */
                memset(msg, 0, 1024);
                p_msg = &msg[strlen(msg)];
                sprintf(p_msg, "[transcode_mt %3d]  - MD5: %s", args->thread_index, md5_string);
                if (g_transcode_context.target_md5) {
                    if (memcmp(g_transcode_context.target_md5, md5_string, sizeof(md5_string)) != 0) {
                        LOGIL(LOG_LEVEL_INFO, COLOR_YELLOW, "%s", msg);
                        LOGIL(LOG_LEVEL_ERROR, COLOR_LIGHT_RED, "[transcode_mt %3d]   ## != %s (Target)",
                            args->thread_index, g_transcode_context.target_md5);
                        stats_error = 1;
                    } else {
                        p_msg = &msg[strlen(msg)];
                        sprintf(p_msg, " (== Target)");
                        LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_GREEN, "%s", msg);
                    }
                } else if (memcmp(args->last_md5_string, md5_string, sizeof(md5_string)) != 0) {
                    LOGIL(LOG_LEVEL_INFO, COLOR_YELLOW, "%s", msg);
                    LOGIL(LOG_LEVEL_ERROR, COLOR_LIGHT_RED, "[transcode_mt %3d]   ## != %s (Last)", args->thread_index,
                        args->last_md5_string);
                    stats_error = 1;
                } else {
                    p_msg = &msg[strlen(msg)];
                    sprintf(p_msg, " (== Last)");
                    LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_GREEN, "%s", msg);
                }
            } else {
                LOGIL(
                    LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "[transcode_mt %3d]  - MD5: %s", args->thread_index, md5_string);
            }
            memcpy(args->last_md5_string, md5_string, sizeof(md5_string));
            args->md5_saved = 1;
        } else {
            LOGIL(LOG_LEVEL_INFO, COLOR_DARK_GRAY, "[transcode_mt %3d]  - MD5: NA", args->thread_index);
        }
        stats_count++;
    }

    if (!stats_count) {
        LOGIL(LOG_LEVEL_INFO, COLOR_DARK_GRAY, "[transcode_mt %3d]  - NA", args->thread_index);
    }

    if (stats_error) {
        atomic_inc(&g_transcode_context.stats_error_cnt);
    }

    LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "[transcode_mt %3d] Performance:", args->thread_index);

    if (args->dec_out_count) {
        md5_check_time = g_transcode_context.encoder_enable ? 0 : args->check_md5_time;
        file_decode_time = args->file_stop_time -
                           args->file_start_time;    // - args->file_read_time - args->file_write_time - md5_check_time;
        memset(msg, 0, 1024);
        if (args->file_read_time) {
            p_msg = &msg[strlen(msg)];
            sprintf(p_msg, " | read %d us/f (total %llu us)",
                (int)(args->file_read_time / args->dec_out_count / NANOSEC_PER_MICROSEC),
                (u64)(args->file_read_time / NANOSEC_PER_MICROSEC));
        }
        if (args->file_write_time) {
            p_msg = &msg[strlen(msg)];
            sprintf(p_msg, " | save %d us/f (total %llu us)",
                (int)(args->file_write_time / args->dec_out_count / NANOSEC_PER_MICROSEC),
                (u64)(args->file_write_time / NANOSEC_PER_MICROSEC));
        }
        if (md5_check_time) {
            p_msg = &msg[strlen(msg)];
            sprintf(p_msg, " | MD5 %d us/f (total %llu us)",
                (int)(md5_check_time / args->dec_out_count / NANOSEC_PER_MICROSEC),
                (u64)(md5_check_time / NANOSEC_PER_MICROSEC));
        }
        LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN,
            "[transcode_mt %3d]  - Decode(%llu): FPS %.3f (%d us/f, total %llu us)%s", args->thread_index,
            (u64)args->dec_out_count, ((float)args->dec_out_count / ((float)file_decode_time / (float)NANOSEC_PER_SEC)),
            (int)(file_decode_time / args->dec_out_count / NANOSEC_PER_MICROSEC),
            (u64)(file_decode_time / NANOSEC_PER_MICROSEC), msg);
        perf_count++;
    }

    if (args->enc_total_frames) {
        md5_check_time = g_transcode_context.encoder_enable ? args->check_md5_time : 0;
        file_encode_time = args->enc_stop - args->enc_start;
        time_per_frame = (float)file_encode_time / (float)args->enc_total_frames;
        count_base = g_transcode_context.another_thread_for_enc_out ? args->enc_cu_info_number : args->enc_total_frames;
        memset(msg, 0, 1024);
        if (args->enc_save_time) {
            p_msg = &msg[strlen(msg)];
            sprintf(p_msg, " | save %d us/f (total %llu us)",
                (int)(count_base ? (args->enc_save_time / count_base) / NANOSEC_PER_MICROSEC : 0),
                (u64)args->enc_save_time / NANOSEC_PER_MICROSEC);
        }

        if (md5_check_time) {
            p_msg = &msg[strlen(msg)];
            sprintf(p_msg, " | MD5 %d us/f (total %llu us)",
                (int)(md5_check_time / args->enc_total_frames / NANOSEC_PER_MICROSEC),
                (u64)(md5_check_time / NANOSEC_PER_MICROSEC));
        }

        LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN,
            "[transcode_mt %3d]  - Encode(%llu): FPS %.3f (%d us/f, total %llu us)%s", args->thread_index,
            (u64)args->enc_total_frames, (float)NANOSEC_PER_SEC / time_per_frame,
            (int)(time_per_frame / NANOSEC_PER_MICROSEC), (u64)(file_encode_time / NANOSEC_PER_MICROSEC), msg);
        perf_count++;
    }

    if (g_transcode_context.encode_yuv && args->raw_frame_count) {
        LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN, "[transcode_mt %3d]   -- RD RawYUV(%llu): %d us/f (total %llu us)",
            args->thread_index, (u64)args->raw_frame_count,
            (int)(args->file_read_time / args->raw_frame_count / NANOSEC_PER_MICROSEC),
            (u64)(args->file_read_time / NANOSEC_PER_MICROSEC));
        perf_count++;
    }

    if (args->cu_info_parse_time) {
        if (!args->enc_cu_info_number || !g_transcode_context.another_thread_for_enc_out) {
            args->enc_cu_info_number = args->enc_total_frames;
        }
        if (args->enc_cu_info_number) {
            memset(msg, 0, 1024);
            if (args->cu_info_save_time) {
                p_msg = &msg[strlen(msg)];
                sprintf(p_msg, " | save %d us/f (total %llu us)",
                    (int)(args->cu_info_save_time / args->enc_cu_info_number / NANOSEC_PER_MICROSEC),
                    (u64)args->cu_info_save_time / NANOSEC_PER_MICROSEC);
            }
            LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN,
                "[transcode_mt %3d]   -- P&S CuInf(%llu): parse %d us/f (total %llu us)%s", args->thread_index,
                (u64)args->enc_cu_info_number,
                (int)(args->cu_info_parse_time / args->enc_cu_info_number / NANOSEC_PER_MICROSEC),
                (u64)args->cu_info_parse_time / NANOSEC_PER_MICROSEC, msg);
            perf_count++;
        }
    }

    if (!perf_count) {
        LOGIL(LOG_LEVEL_INFO, COLOR_DARK_GRAY, "[transcode_mt %3d]  - NA", args->thread_index);
    }

    LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN,
        "[transcode_mt %3d] -------------------- %s Statistic [L%d/J%d] END -----------------", args->thread_index,
        temp, args->current_main_loop, args->job_index);
}

static inline vmppResult sync_and_send_again(transcode_thread_params *args, vmppStream *strm, int *err_cnt, int *abort)
{
    int error_cnt = *err_cnt;
    vmppResult ret;
    if (args->encoder_inited) {
        sync_threads(args, 0, NULL);
        if (args->current_enc_opts.gopSize != 1 || args->current_enc_opts.lookaheadDepth) {
            LOG_INFO("[transcode_mt %3d] got 'DEC_INPUT_AGAIN', resolution will change, "
                     "and the gopSize(%d) is not 1 or lookahead enabled(%d), force flush encoder",
                args->thread_index, args->current_enc_opts.gopSize, args->current_enc_opts.lookaheadDepth);
            args->force_flush_encoder = 1;
            sync_threads(args, 0, force_flush_encoder_finished);
        }
    }
    ret = vmppDecSendStream(args->dec_ch, strm, DEFAULT_TIMEOUT_MS);
    if (ret < 0) {
        LOG_ERROR("[transcode_mt %3d] vmppDecSendStream again failed, error: %d", args->thread_index, ret);
        if (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_AS_HWOUT) {
            LOG_TRACE("[transcode_mt %3d] return usrbuf(DEVMEM) AGAIN ERR: 0x%llx.", args->thread_index,
                (u64)strm->outputBusAddress[0]);
            return_usrbuf(args, strm->outputBusAddress[0]);
        }

        if (vmpp_RSLT_ERR_INVALID_DATA == ret || vmpp_RSLT_ERR_NO_MEMORY == ret) {
            LOG_ERROR("[transcode_mt %3d] invalid data or no memory, abort", args->thread_index);
            *abort = 1;
            args->dec_error = 1;
        }
        error_cnt++;
        if (error_cnt > 3) {
            LOG_ERROR("[transcode_mt %3d] error count(%d) >= 3, abort", args->thread_index, error_cnt);
            *abort = 1;
            args->dec_error = 1;
        }
        *err_cnt = error_cnt;
    }
    return ret;
}

static inline int read_frame(transcode_thread_params *args)
{
    int ret;
    vmppFrame *frame;
    uint64_t read_start;
    pthread_mutex_lock(&args->frame_mutex);
    frame = (vmppFrame *)vmpp_queue_pop_front(args->idle_frame_queue);
    pthread_mutex_unlock(&args->frame_mutex);
    if (!frame) {
        return 0;
    }

    read_start = gettime_ns();
    ret = raw_read_frame(args->rawctx, frame);
    if (ret <= 0) {
        pthread_mutex_lock(&args->frame_mutex);
        vmpp_queue_push_back(args->idle_frame_queue, frame);
        pthread_mutex_unlock(&args->frame_mutex);
        if (raw_eof(args->rawctx)) {
            LOG_DEBUG("[transcode_mt %3d] raw yuv end", args->thread_index);
            return 1;
        } else {
            LOG_ERROR("[transcode_mt %3d] raw yuv read error", args->thread_index);
            args->input_error = 1;
            return -1;
        }
    } else {
        args->raw_frame_count++;
        args->file_read_time += (gettime_ns() - read_start);
    }

    frame->pts = args->raw_frame_count;
    pthread_mutex_lock(&args->frame_mutex);
    vmpp_queue_push_back(args->frame_queue, frame);
    pthread_mutex_unlock(&args->frame_mutex);
    return 0;
}

static inline int read_stream(transcode_thread_params *args, vmppStream *strm, uint64_t *ppts)
{
    int ret;
    uint32_t stream_len = 0;
    uint8_t *stream_p = NULL;
    uint64_t pts = 0;
    uint64_t read_start = gettime_ns();
    if (args->using_ffmpeg) {
#ifdef USING_FFMPEG
        stream_len = ff_read_frame2(args->ffctx, &stream_p, &pts);
        if (!stream_len || !stream_p) {

            if (ff_eof(args->ffctx)) {
                LOG_DEBUG("[transcode] stream end");
            } else {
                LOG_ERROR("[transcode] read frame error.");
                args->input_error = 1;
            }
            return -1;
        }
#endif    // USING_FFMPEG
    } else {
        pts = *ppts;
        ret = stream_read_frame(args->strmctx, &stream_p);
        if (ret <= 0) {
            if (ret == 0 && stream_eof(args->strmctx)) {
                LOG_DEBUG("[transcode_mt %3d] stream end", args->thread_index);
                return -1;
            }
            LOG_ERROR("[transcode_mt %3d] stream_read_frame failed, tmp_ret:%d", args->thread_index, ret);
            args->input_error = 1;
            return -1;
        }
        stream_len = (uint32_t)ret;
        pts++;
        *ppts = pts;
    }

    args->file_read_time += (gettime_ns() - read_start);

    strm->stream = stream_p;
    strm->len = stream_len;
    strm->pts = pts;

    return 0;
}

static inline void back_to_start(transcode_thread_params *args)
{
    if (g_transcode_context.encode_yuv) {
        raw_seek_to_start(args->rawctx);
    } else if (args->using_ffmpeg) {
#ifdef USING_FFMPEG
        ff_seek_to_start(args->ffctx);
#endif
    } else {
        stream_seek_to_start(args->strmctx);
    }
}

static inline void cleanup_input(transcode_thread_params *args)
{
    if (g_transcode_context.encode_yuv) {
        if (args->rawctx) {
            LOG_TRACE("[transcode_mt %3d] close raw yuv context (%p)", args->thread_index, args->rawctx);
            raw_close(args->rawctx);
            free(args->rawctx);
            args->rawctx = NULL;
        }
    } else if (args->using_ffmpeg) {
#ifdef USING_FFMPEG
        LOG_TRACE("[transcode_mt %3d] close FFmpeg context", args->thread_index);
        ff_close2(&args->ffctx);
#endif
    } else {
        LOG_TRACE("[transcode_mt %3d] close stream context", args->thread_index);
        stream_close(&args->strmctx);
    }
}

int parse_codec_from_url(transcode_thread_params *args)
{
    char *suffix = NULL;
    suffix = strrchr(args->current_url, '.');
    if (suffix) {
        if (!strcmp(suffix, ".h264") || !strcmp(suffix, ".avc") || !strcmp(suffix, ".264")) {
            args->codec = (char*)"h264";
        } else if (!strcmp(suffix, ".hevc") || !strcmp(suffix, ".h265") || !strcmp(suffix, ".265")) {
            args->codec = (char*)"hevc";
        } else if (!strcmp(suffix, ".av1") || !strcmp(suffix, ".obu") /* || !strcmp(suffix, ".ivf")*/ ||
                   !strcmp(suffix, ".avif")) {
            args->codec = (char*)"av1";
        } else if (!strcmp(suffix, ".avs")) {
            args->codec = (char*)"avs2";
        } else if (!strcmp(suffix, ".vp9")) {
            args->codec = (char*)"vp9";
        } else if (!strcmp(suffix, ".jpeg") || !strcmp(suffix, ".jpg")) {
            args->codec = (char*)"jpeg";
            args->jpeg_stream = 1;
        } else if (!strcmp(suffix, ".mjpeg") || !strcmp(suffix, ".mjpg")) {
#ifdef USING_FFMPEG
            LOG_WARN(
                "[transcode_mt %3d] Demuxing mjpeg is not fully supported, you may try using '-f 1' to parsing mjpeg through FFmpeg.",
                args->thread_index);
#else
            LOG_WARN("[transcode_mt %3d] Demuxing mjpeg is not fully supported, bugs may exist.", args->thread_index);
#endif
            args->codec = (char*)"jpeg";
            args->jpeg_stream = 1;
        } else {
            args->codec = NULL;
            LOG_ERROR("[transcode_mt %3d] Can not parse codec from url :%s", args->thread_index, args->current_url);
            return -1;
        }
    } else {
        args->codec = NULL;
        LOG_ERROR("[transcode_mt %3d] Can not parse codec from url :%s", args->thread_index, args->current_url);
        return -1;
    }

    LOG_INFO("[transcode_mt %3d] Codec '%s' parsed from url :%s", args->thread_index, args->codec, args->current_url);

    return 0;
}

static int run_transcode_mt(transcode_thread_params *args, const char *job_string)
{
    int loop, error_cnt = 0, abort = 0;
    uint32_t tmp_ret = 0, frame_number = 0, vframes;
    uint64_t fps_control_timestamp, interval_ns = 0, pts_cnt = 0;
    vmppDecChannelParameters dec_params = { 0 };
    vmppStream input_stream;
    vmppResult ret = vmpp_RSLT_OK;
    vmppRuntimeInstance *runtime_inst_dec = NULL, *runtime_inst_enc = NULL;

    if (g_transcode_context.target_fps) {
        interval_ns = NANOSEC_PER_SEC / g_transcode_context.target_fps;
    }

    /* reset arguments */
    reset_args(args);

    args->job_index++;
    args->current_loop = 0;
    args->using_ffmpeg = g_transcode_context.using_ffmpeg;

    if (g_transcode_context.codec && strcmp(g_transcode_context.codec, (char *)"h264")) {
        args->codec = g_transcode_context.codec;
    } else if (!g_transcode_context.using_ffmpeg && parse_codec_from_url(args) < 0) {
        LOG_ERROR("[transcode_mt %3d] Parse codec from input url failed, exit current case!", args->thread_index);
        goto transcode_stop;
    }

    if (prepare_for_input(args, &dec_params)) {
        LOG_ERROR("[transcode_mt %3d] Failed to do preparation for input!", args->thread_index);
        goto transcode_stop;
    }

    if (g_transcode_context.encoder_enable) {
        LOG_TRACE("[transcode_mt %3d] encoder is enabled, prepare for it!", args->thread_index);
        if (prepare_for_encoder(args, &runtime_inst_enc)) {
            LOG_ERROR("[transcode_mt %3d] Failed to do preparation for encoder!", args->thread_index);
            goto transcode_stop;
        }
    }

    if (!g_transcode_context.encode_yuv) {
        LOG_TRACE("[transcode_mt %3d] prepare for decoder!", args->thread_index);
        if (prepare_for_decoder(args, &runtime_inst_dec)) {
            LOG_ERROR("[transcode_mt %3d] Failed to do preparation for decoder!", args->thread_index);
            goto transcode_stop;
        }
        LOG_TRACE("[transcode_mt %3d] create and start decoder!", args->thread_index);
        if (create_and_start_decoder(args, &dec_params)) {
            LOG_ERROR("[transcode_mt %3d] Failed to create and start decoder!", args->thread_index);
            goto transcode_stop;
        }
    }

    fps_control_timestamp = args->file_start_time = args->period_file_start_time = gettime_ns();
    loop = g_transcode_context.loop;
    do {
        args->current_loop++;
        if (args->current_loop > 1) {
            LOG_DEBUG("[transcode_mt %3d] seek to start.", args->thread_index);
            back_to_start(args);
        }
        LOG_DEBUG("[transcode_mt %3d] ->>> Run '%s' Job [L%d/J%d/l%d] ...", args->thread_index, job_string,
            args->current_main_loop, args->job_index, args->current_loop);
        do {
            if (args->enc_error) {
                LOG_WARN("[transcode_mt %3d] enc error happens, exit transcode main thread.", args->thread_index);
                goto transcode_stop;
            }

            if (args->dec_error) {
                LOG_WARN("[transcode_mt %3d] dec error happens, exit transcode main thread.", args->thread_index);
                goto transcode_stop;
            }

            if (g_transcode_context.encode_yuv) {
                vframes = g_transcode_context.re_count_vframes ? frame_number : args->raw_frame_count;
                if (g_transcode_context.vframes > 0 && vframes >= (uint32_t)g_transcode_context.vframes) {
                    LOG_INFO("[transcode_mt %3d] reading of the target %d YUV frame(s) finished%s", args->thread_index,
                        g_transcode_context.vframes,
                        g_transcode_context.re_count_vframes ? " for current loop, re-count frame number for target" :
                                                               "!");
                    frame_number = 0;
                    break;
                }
            } else {
                vframes = g_transcode_context.re_count_vframes ? frame_number : args->dec_in_count;
                if (g_transcode_context.vframes > 0 && vframes >= (uint32_t)g_transcode_context.vframes) {
                    LOG_INFO("[transcode_mt %3d] decoding of the target %d frame(s) finished%s", args->thread_index,
                        g_transcode_context.vframes,
                        g_transcode_context.re_count_vframes ? " for current loop, re-count frame number for target" :
                                                               "!");
                    frame_number = 0;
                    break;
                }
            }

            if (args->encoder_inited) {
                // !!!NOTE
                /* Synchronize with the encoding thread.
                 * When we run transcoding, if 'Send' is much faster than 'Buffer Release', we may
                 * encounter with the 'vmpp_RSLT_ERR_NO_BUFFER' error which can not recover itself.
                 * Or, the app may occupying too many usr buffer which may lead to request failure for new usr buf */
                sync_threads(args, 0, NULL);
            }

            /* read input data */
            if (g_transcode_context.encode_yuv) {
                if (read_frame(args)) {
                    break;    // eof or error
                } else {
                    frame_number++;
                    /* skip the decoding logic */
                    usleep(1);
                    continue;
                }
            } else {
                if (read_stream(args, &input_stream, &pts_cnt)) {
                    break;    // eof or error
                }
                frame_number++;
            }

            if (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_AS_HWOUT) {
                if (set_usrbuf(args, &input_stream, dec_params.codecType)) {
                    LOG_ERROR("[transcode_mt %3d] set usrbuf(%s) failed, mode(%d), abort current case.",
                        args->thread_index, UBT(&args->ubf), g_transcode_context.memory_mode);
                    break;
                }
            }

            if (g_transcode_context.collect_latency && !args->dec_first_send_timestamp) {
                args->dec_first_send_timestamp = gettime_ns();
            }

            ret = vmppDecSendStream(args->dec_ch, &input_stream, DEFAULT_TIMEOUT_MS);
            if (ret < 0) {
                LOG_ERROR("[transcode_mt %3d] vmppDecSendStream (%llu), error: %d", args->thread_index,
                    (u64)args->dec_in_count, ret);
                if (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_AS_HWOUT) {
                    LOG_TRACE("[transcode_mt %3d] return usrbuf(DEVMEM) ERR: 0x%llx.", args->thread_index,
                        (u64)input_stream.outputBusAddress[0]);
                    return_usrbuf(args, input_stream.outputBusAddress[0]);
                }
                if (vmpp_RSLT_ERR_INVALID_DATA == ret || vmpp_RSLT_ERR_NO_MEMORY == ret) {
                    LOG_ERROR("[transcode_mt %3d] exit transcode main thread, send error: %d", args->thread_index, ret);
                    abort = 1;
                    args->dec_error = 1;
                    break;
                }
                error_cnt++;
                if (error_cnt > 3) {
                    LOG_ERROR("[transcode_mt %3d] exit transcode main thread, send error count(%d) >= 3",
                        args->thread_index, error_cnt);
                    abort = 1;
                    args->dec_error = 1;
                    break;
                }
            } else {
                if (ret == vmpp_RSLT_DEC_INPUT_AGAIN && g_transcode_context.dec_api_mode == vmpp_DEC_API_MODE_SERIAL) {
                    LOG_TRACE("[transcode_mt %3d] vmppDecSendStream return AGAIN in SERIAL mode!", args->thread_index);
                    do {
                        do {
                            tmp_ret = do_dec_output(args, 0);
                            if (tmp_ret || args->jpeg_stream) {
                                break;
                            }
                        } while (1);

                        ret = sync_and_send_again(args, &input_stream, &error_cnt, &abort);
                        if (abort) {
                            LOG_ERROR("[transcode_mt %3d] re-send fail in serial mode, exit transcode main thread",
                                args->thread_index);
                            goto transcode_stop;
                        }
                    } while (ret == vmpp_RSLT_DEC_INPUT_AGAIN);
                } else if (ret == vmpp_RSLT_DEC_INPUT_AGAIN) {
                    LOG_TRACE("[transcode_mt %3d] vmppDecSendStream return AGAIN!", args->thread_index);
                    do {
                        /* parallel mode, wait output thread flushing */
                        args->wait_flush_decoder = 1;
                        sync_threads(args, 0, wait_flush_decoder_finished);

                        ret = sync_and_send_again(args, &input_stream, &error_cnt, &abort);
                        if (abort) {
                            LOG_ERROR("[transcode_mt %3d] re-send fail in parallel mode, exit transcode main thread",
                                args->thread_index);
                            goto transcode_stop;
                        }
                    } while (ret == vmpp_RSLT_DEC_INPUT_AGAIN);
                }

                if (ret != vmpp_RSLT_OK) {
                    if (ret == vmpp_RSLT_DEC_AV1_NOTSHOW) {
                        LOG_DEBUG("[transcode_mt %3d] vmppDecSendStream (%llu): ret %d - AV1_NOTSHOW frame.",
                            args->thread_index, (u64)args->dec_in_count, ret);
                        args->dec_notshow_frame_count++;
                    } else if (ret == vmpp_RSLT_DEC_DISCARD_FRAME) {
                        LOG_DEBUG("[transcode_mt %3d] vmppDecSendStream (%llu): ret %d - DISCARD_FRAME.",
                            args->thread_index, (u64)args->dec_in_count, ret);
                        args->dec_discarded_frame_count++;
                    } else if (ret == vmpp_RSLT_WARN_FRAME_SKIPPED) {
                        LOG_DEBUG("[transcode_mt %3d] vmppDecSendStream (%llu): ret %d - SKIPPED_FRAME.",
                            args->thread_index, (u64)args->dec_in_count, ret);
                        args->dec_skipped_frame_count++;
                    } else {
                        LOG_WARN("[transcode_mt %3d] vmppDecSendStream (%llu) STRM ERR: ret %d", args->thread_index,
                            (u64)args->dec_in_count, ret);
                    }

                    if (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_AS_HWOUT) {
                        LOG_TRACE("[transcode_mt %3d] return usrbuf(DEVMEM) STRM ERR: 0x%llx.", args->thread_index,
                            (u64)input_stream.outputBusAddress[0]);
                        return_usrbuf(args, input_stream.outputBusAddress[0]);
                    }
                } else {
                    ++args->dec_in_count;
                    LOG_TRACE("[transcode_mt %3d] vmppDecSendStream (%llu) succeed", args->thread_index,
                        (u64)args->dec_in_count);
                }

                if (g_transcode_context.dec_api_mode == vmpp_DEC_API_MODE_SERIAL) {
                    do {
                        tmp_ret = do_dec_output(args, 0);
                        if (tmp_ret || args->jpeg_stream) {
                            break;
                        }
                    } while (1);
                }

                if (g_transcode_context.target_fps) {
                    do_fps_control(args, &fps_control_timestamp, interval_ns);
                }
            }
        } while (!abort);
        LOG_DEBUG("[transcode_mt %3d] <<<- Run '%s' Job [L%d/J%d/l%d] Done!", args->thread_index, job_string,
            args->current_main_loop, args->job_index, args->current_loop);

        printf("================ --loop: %d\n", loop);
    } while (--loop && !abort);

transcode_stop:
    if (!g_transcode_context.encode_yuv && args->dec_ch) {
        LOG_DEBUG("[transcode_mt %3d] ready to stop decoder", args->thread_index);

        ret = vmppDecStop(args->dec_ch);
        if (ret < 0) {
            LOG_ERROR("[transcode_mt %3d] stop decoder failed, error %d", args->thread_index, ret);
            args->dec_error = 1;
        }

        if (g_transcode_context.dec_api_mode == vmpp_DEC_API_MODE_SERIAL) {
            /* FLUSH */
            do {
                tmp_ret = do_dec_output(args, 0);
                if (tmp_ret || args->jpeg_stream) {
                    break;
                }
            } while (1);
        }

        if (g_transcode_context.dec_api_mode == vmpp_DEC_API_MODE_PARALLEL) {
            if (is_thread_active(&args->dec_output_thread_handle)) {
                LOG_DEBUG("[transcode_mt %3d] waiting for dec output thread", args->thread_index);
                pthread_join(args->dec_output_thread_handle, NULL);
            }
        }
    }

    if (g_transcode_context.encoder_enable) {
        args->dec_finished = 1;
        if (is_thread_active(&args->enc_thread_handle)) {
            LOG_DEBUG("[transcode_mt %3d] waiting for enc thread", args->thread_index);
            pthread_join(args->enc_thread_handle, NULL);
        }

        if (is_thread_active(&args->enc_out_thread_handle)) {
            LOG_DEBUG("[transcode_mt %3d] waiting for enc output thread", args->thread_index);
            pthread_join(args->enc_out_thread_handle, NULL);
        }
    }

    if (args->enc_ch) {
        LOG_DEBUG("[transcode_mt %3d] destroy encode channel (%p)", args->thread_index, args->enc_ch);
        ret = vmppEncDestroyChannel(&args->enc_ch);
        if (ret < 0) {
            LOG_ERROR("[transcode_mt %3d] destroy enc chn error %d", args->thread_index, ret);
            args->enc_error = 1;
        }
    }

    if (args->dec_ch) {
        LOG_DEBUG("[transcode_mt %3d] destroy decode channel (%p)", args->thread_index, args->dec_ch);
        ret = vmppDecDestroyChannel(&args->dec_ch);
        if (ret < 0) {
            LOG_ERROR("[transcode_mt %3d] destroy dec chn error %d", args->thread_index, ret);
            args->dec_error = 1;
            LOG_WARN("[transcode_mt %3d] force destroy decode channel (%p)", args->thread_index, args->dec_ch);
            ret = vmppDecDestroyChannelForced(&args->dec_ch);
            if (ret < 0) {
                LOG_ERROR("[transcode_mt %3d] force destroy dec chn error %d", args->thread_index, ret);
            }
        }
    }

    if (g_transcode_context.encoder_enable) {
        LOG_DEBUG("[transcode_mt %3d] clean up resource for encoder", args->thread_index);
        cleanup_for_encoder(args);
    }

    cleanup_input(args);

    if (args->dec_device_handle != -1) {
        LOG_TRACE("[transcode_mt %3d] close dec device handle: %d", args->thread_index, args->dec_device_handle);
        close(args->dec_device_handle);
    }
    if (g_transcode_context.encoder_enable) {
        if (args->enc_device_handle != -1) {
            LOG_TRACE("[transcode_mt %3d] close enc device handle: %d", args->thread_index, args->enc_device_handle);
            close(args->enc_device_handle);
        }
    }
    if (args->enc_output_file) {
        LOG_TRACE("[transcode_mt %3d] close enc output file: 0x%llx", args->thread_index, (u64)args->enc_output_file);
        fclose(args->enc_output_file);
        args->enc_output_file = NULL;
    }

    if (args->enc_cu_info_file) {
        LOG_TRACE(
            "[transcode_mt %3d] close cuinfo output file: 0x%llx", args->thread_index, (u64)args->enc_cu_info_file);
        fclose(args->enc_cu_info_file);
        args->enc_cu_info_file = NULL;
    }

    if (args->dec_output_file) {
        LOG_TRACE("[transcode_mt %3d] close dec output file: 0x%llx", args->thread_index, (u64)args->dec_output_file);
        fclose(args->dec_output_file);
        args->dec_output_file = NULL;
    }

    usrbuf_factory_reset(&args->ubf);
    if (args->enc_cu_info_buffer) {
        LOG_TRACE(
            "[transcode_mt %3d] free buffer for cu info: 0x%llx", args->thread_index, (u64)args->enc_cu_info_buffer);
        free(args->enc_cu_info_buffer);
        args->enc_cu_info_buffer = NULL;
    }

    if (g_transcode_context.multi_runtime) {
        LOG_TRACE("[transcode_mt %3d] global deinit for runtime & vmpp", args->thread_index);
        vmppDeInitDecoder();
        if (g_transcode_context.encoder_enable) {
            vmppDeInitEncoder();
        }
        runtime_cleanup(runtime_inst_enc);
        runtime_cleanup(runtime_inst_dec);
    }

    if (args->dec_error) {
        atomic_inc(&g_transcode_context.dec_error_cnt);
    }

    if (args->enc_error) {
        atomic_inc(&g_transcode_context.enc_error_cnt);
    }

    if (args->input_error) {
        atomic_inc(&g_transcode_context.input_error_cnt);
    }

    if (args->output_error) {
        atomic_inc(&g_transcode_context.output_error_cnt);
    }

    dump_statics_info(args);

    LOG_DEBUG("[transcode_mt %3d] exit transcoding thread", args->thread_index);

    return 0;
}

static void *transcode_thread(void *args)
{
    int loop, i, ret, index, input_index;
    const char *job_string =
        g_transcode_context.encoder_enable ? (g_transcode_context.encode_yuv ? "Encoding" : "Transcoding") : "Decoding";
    transcode_thread_params *params = (transcode_thread_params *)args;
    loop = g_transcode_context.main_loop;
    params->current_main_loop = 0;
    do {
        params->current_main_loop++;
        input_index = params->thread_index;
        LOG_INFO("[transcode_mt %3d] -> List Loop %d starting!", params->thread_index, params->current_main_loop);
        for (i = 0; i < vmpp_queue_size(g_transcode_context.urls); i++) {
            if (g_transcode_context.input_mode == IM_SELECT_RANDOMLY) {
                srand((unsigned int)time(NULL));
                srand(params->thread_index + rand());
                input_index = rand() % vmpp_queue_size(g_transcode_context.urls);
                LOG_INFO(
                    "[transcode_mt %3d] The random input index is %d, loop %d!", params->thread_index, input_index, i);
            } else if (g_transcode_context.input_mode == IM_SELECT_ORDERLY ||
                       g_transcode_context.input_mode == IM_ALL_ORDERLY) {
                input_index = input_index % vmpp_queue_size(g_transcode_context.urls);
                LOG_INFO("[transcode_mt %3d] The input index is %d, loop %d", params->thread_index, input_index, i);
            } else {
                input_index = i;
            }

            params->current_url = (char *)vmpp_queue_peek(g_transcode_context.urls, input_index);

            /* set device */
            params->current_device = g_transcode_context.device_name;
            if (g_transcode_context.multi_device && vmpp_queue_size(g_transcode_context.devices)) {
                index = params->thread_index % vmpp_queue_size(g_transcode_context.devices);
                params->current_device = (char*)vmpp_queue_peek(g_transcode_context.devices, index);
                LOG_INFO("[transcode_mt %3d] Assign device in order { %s }, index %d", params->thread_index,
                    params->current_device, index);
            } else if (g_transcode_context.random_device && vmpp_queue_size(g_transcode_context.devices)) {
                srand((unsigned int)time(NULL));
                index = rand() % vmpp_queue_size(g_transcode_context.devices);
                params->current_device = (char*)vmpp_queue_peek(g_transcode_context.devices, index);
                LOG_INFO("[transcode_mt %3d] Assign device randomly { %s }, index %d", params->thread_index,
                    params->current_device, index);
            }

            /* set options for encoder */
            if (g_transcode_context.encoder_enable) {
                memcpy(&params->current_enc_opts, &g_transcode_context.default_enc_opts,
                    sizeof(g_transcode_context.default_enc_opts));
                if (vmpp_queue_size(g_transcode_context.customized_enc_opts)) {
                    index = params->thread_index % vmpp_queue_size(g_transcode_context.customized_enc_opts);
                    memcpy(&params->current_enc_opts, vmpp_queue_peek(g_transcode_context.customized_enc_opts, index),
                        sizeof(g_transcode_context.default_enc_opts));
                    LOG_INFO("[transcode_mt %3d] Random encode options index %d, codec %s", params->thread_index, index,
                        params->current_enc_opts.encCodec);
                }
            }

            LOG_INFO("[transcode_mt %3d] ->> '%s' on file[%d] '%s' on device { %s } starting!", params->thread_index,
                job_string, i, params->current_url, params->current_device);
            ret = run_transcode_mt(params, job_string);
            if (ret < 0) {
                /* Should never happens currently! */
                LOG_WARN("[transcode_mt %3d] Exit current '%s' thread due to error!", params->thread_index, job_string);
                return NULL;
            }
            LOG_INFO("[transcode_mt %3d] <<- '%s' on file[%d] '%s' on device { %s } finished!", params->thread_index,
                job_string, i, params->current_url, params->current_device);
            if (g_transcode_context.input_mode == IM_SELECT_RANDOMLY ||
                g_transcode_context.input_mode == IM_SELECT_ORDERLY) {
                break;
            } else if (g_transcode_context.input_mode == IM_ALL_ORDERLY) {
                input_index++;
            }
        }
        LOG_INFO("[transcode_mt %3d] <- List Loop %d finished!", params->thread_index, params->current_main_loop);
    } while (--loop);
    return NULL;
}

static char *device_x(struct vmpp_queue *device_queue, int id)
{
    int device_id = 0, i;
    char *device = NULL;
    for (i = 0; i < vmpp_queue_size(device_queue); i++) {
        device = (char *)vmpp_queue_peek(device_queue, i);
        if (strncmp(device, "/dev/vastai_video", strlen("/dev/vastai_video")) == 0) {
            sscanf(device, "/dev/vastai_video%d", &device_id);
        } else {
            sscanf(device, "/dev/va_video%d", &device_id);
        }
        if (device_id == id) {
            return device;
        }
    }

    return NULL;
}

static int options_setup(params_transcode_mt_t *params_ut)
{
    int ret;
    char *job_string;
    set_default_enc_params(&g_transcode_context.default_enc_opts);
    g_transcode_context.main_loop = 1;
    g_transcode_context.loop = 1;
    g_transcode_context.bitDepth = 8;
    g_transcode_context.thread_count = 1;
    g_transcode_context.max_queued_frame = MAX_QUEUED_FRAME;
    g_transcode_context.max_queued_stream = MAX_QUEUED_STREAM;
    g_transcode_context.period = DEFAULT_PERIOD_FRAMES;
    g_transcode_context.perf_period = PERF_PERIOD_FRAMES;
    g_transcode_context.dec_recv_mem_type = RECV_MT_AUTO;
#ifdef USING_FFMPEG
    g_transcode_context.using_ffmpeg = 1;
#else
    g_transcode_context.using_ffmpeg = 0;
#endif

    if (vmpp_queue_init(&g_transcode_context.urls) < 0) {
        LOGIL_ERROR("[transcode_mt] Failed to init url queue!");
        return -1;
    }

    if (vmpp_queue_init(&g_transcode_context.devices) < 0) {
        LOGIL_ERROR("[transcode_mt] Failed to init device queue!");
        return -1;
    }

    if (vmpp_queue_init(&g_transcode_context.customized_enc_opts) < 0) {
        LOGIL_ERROR("[transcode_mt] Failed to init encoder options queue!");
        return -1;
    }

    ret = parse_options(params_ut);
    if (ret) {
        return ret;
    }

    /* Re-checking some parameters, and Do some warning if possible */
    if (!g_transcode_context.using_ffmpeg && g_has_irregular_url && !g_transcode_context.encode_yuv) {
        LOGIL_WARN("[transcode_mt] irregular url(s) may be invalid, you may try to open it with FFmpeg!");
    }

    if (g_transcode_context.encoder_enable) {
        g_transcode_context.default_enc_opts.encCodec = g_transcode_context.enc_codec;
        if (g_transcode_context.encode_yuv) {
            job_string = (char*)"Encoding";
            if (g_transcode_context.max_queued_frame <= 0) {
                LOGIL_WARN("[transcode_mt] You have set an invalid value(%d) for maxQueuedFrame to run Encoding(YUV)! "
                           "We will fall back to default value(%d)!",
                    g_transcode_context.max_queued_frame, MAX_QUEUED_FRAME);
                g_transcode_context.max_queued_frame = MAX_QUEUED_FRAME;
            }

            if (g_transcode_context.memory_mode) {
                /* reset unused memory mode to avoid unexpected impacts */
                g_transcode_context.memory_mode = vmpp_DEC_MEM_NORMAL;
            }
        } else {
            job_string = (char*)"Transcoding";
            if (g_transcode_context.dec_api_mode == vmpp_DEC_API_MODE_SERIAL) {
                LOGIL_WARN("[transcode_mt] You are tring to run transcoding in serial mode for decoder!");
                if (g_transcode_context.max_queued_frame <= 0) {
                    LOGIL_WARN(
                        "[transcode_mt] You have set an invalid value(%d) for maxQueuedFrame to run transcoding(with serial mode decoding)! "
                        "We will fall back to default value(%d)!",
                        g_transcode_context.max_queued_frame, MAX_QUEUED_FRAME);
                    g_transcode_context.max_queued_frame = MAX_QUEUED_FRAME;
                }
            }
        }
    } else {
        job_string = (char*)"Decoding";
    }

    if (g_transcode_context.encoder_enable && (strcmp(g_transcode_context.enc_codec, "jpeg") == 0) &&
        (g_transcode_context.output_cu_info || g_transcode_context.parse_cu_info || g_transcode_context.save_cu_info)) {
        LOGIL_WARN("[transcode_mt] Cu Info is not supported by '%s' encoder.", g_transcode_context.enc_codec);
        g_transcode_context.output_cu_info = 0;
        g_transcode_context.parse_cu_info = 0;
        g_transcode_context.save_cu_info = 0;
    }

    if (g_transcode_context.encoder_enable && g_transcode_context.parse_cu_info &&
        !g_transcode_context.output_cu_info) {
        LOGIL_WARN("[transcode_mt] Can not parse Cu Info when Cu Info output is disabled! Force enable it.");
        g_transcode_context.output_cu_info = 1;
    }

    if (g_transcode_context.encoder_enable && g_transcode_context.save_cu_info &&
        (!g_transcode_context.output_cu_info || !g_transcode_context.parse_cu_info)) {
        LOGIL_WARN(
            "[transcode_mt] Can not save Cu Info when output(%d)/parse(%d) Cu Info is diabled! Force enable them.",
            g_transcode_context.output_cu_info, g_transcode_context.parse_cu_info);
        g_transcode_context.parse_cu_info = 1;
        g_transcode_context.output_cu_info = 1;
    }

    if (g_transcode_context.output_file && !g_transcode_context.output_directory &&
        (g_transcode_context.main_loop > 1 || vmpp_queue_size(g_transcode_context.urls) > 1)) {
        LOGIL_WARN(
            "[transcode_mt] You only set one output file name for multi-list-loop(%d)/multi-input-file(%d) case! "
            "We will use it to name the first output file, but will automatically name subsequent output files..",
            g_transcode_context.main_loop, vmpp_queue_size(g_transcode_context.urls));
    }

    if (g_transcode_context.dec_crop == 2 && vmpp_queue_size(g_transcode_context.urls) > 1) {
        LOGIL_WARN("[transcode_mt] You are doing same customized crop for multiple files.");
    }

    if (g_transcode_context.check_md5 && !g_transcode_context.encoder_enable) {
        LOGIL_WARN("[transcode_mt] The actual memory type of decoded frame may not support MD5 checking!");
    }

    /* Set some default value if it's not specified */
    if (g_transcode_context.default_enc_opts.pixelFormat == vmpp_PIX_FMT_NONE) {
        if (g_transcode_context.encode_yuv) {
            LOGIL_WARN("[transcode_mt] pixel format is not provided, use NV12 as default value.");
        }
        g_transcode_context.default_enc_opts.pixelFormat = vmpp_PIX_FMT_NV12;
    }

    if (!g_transcode_context.device_name) {
        if (!g_transcode_context.multi_device && !g_transcode_context.random_device) {
            g_transcode_context.device_name = device_x(g_transcode_context.devices, 0);
            LOG_INFO("[transcode_mt] Multi/Random device is not enabled, use the first device as default: { %s }",
                g_transcode_context.device_name);
        }
    }

    /* print some basic information */
    if (g_transcode_context.dec_api_mode == vmpp_DEC_API_MODE_SERIAL && !g_transcode_context.encode_yuv) {
        LOGIL_INFO("[transcode_mt] Serail mode for decoder is enabled!");
    }

    if (g_transcode_context.multi_runtime) {
        LOGIL_INFO("[transcode_mt] Multiple runtime mode enabled!");
    }

    if (g_transcode_context.unique_output_file) {
        LOGIL_INFO("[transcode_mt] Unique output file is enabled!");
    }

    if (g_transcode_context.disable_dec_profiling || g_transcode_context.disable_enc_profiling) {
        LOGIL_INFO("[transcode_mt] Profiling for Dec(%d) / Enc(%d) is disabled.",
            g_transcode_context.disable_dec_profiling, g_transcode_context.disable_enc_profiling);
    }

    if (g_transcode_context.target_fps) {
        LOGIL_INFO("[transcode_mt] FPS control is enabled, target FPS %d !", g_transcode_context.target_fps);
    }

    if (g_transcode_context.check_md5 && g_transcode_context.target_md5 &&
        vmpp_queue_size(g_transcode_context.urls) > 1) {
        LOGIL_WARN("[transcode_mt] One single MD5(%s) provided for multiple(%d) input, MD5 checking will be ignored "
                   "(While the calculation of MD5 will be executed still).",
            g_transcode_context.target_md5, vmpp_queue_size(g_transcode_context.urls));
    }

    if (g_transcode_context.input_mode == IM_SELECT_RANDOMLY || g_transcode_context.input_mode == IM_SELECT_ORDERLY) {
        LOGIL_WARN(
            "[transcode_mt] Input Mode '%d' is enabled, we will only chose one file from list in every list loop!",
            g_transcode_context.input_mode);
    }

    if (g_transcode_context.separate_luma_chroma &&
        (g_transcode_context.encoder_enable || g_transcode_context.memory_mode != vmpp_DEC_MEM_USER_AS_HWOUT)) {
        /* Remove or modify this restriction if we support it */
        LOGIL_WARN(
            "[transcode_mt] Separate Luma & Chroma is not supported when encoder is enabled(%d) or memory mode is %d!",
            g_transcode_context.encoder_enable, g_transcode_context.memory_mode);
        g_transcode_context.separate_luma_chroma = 0;
    }

    if (!g_transcode_context.encode_yuv) {
        if (g_transcode_context.dec_recv_mem_type == RECV_MT_HOST) {
            g_transcode_context.real_recv_mem_type = vmpp_MEM_HOST;
        } else if (g_transcode_context.dec_recv_mem_type == RECV_MT_DEVICE) {
            g_transcode_context.real_recv_mem_type = vmpp_MEM_DEVICE;
        } else {
            /* auto */
            if (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_DEV ||
                g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_AS_HWOUT) {
                g_transcode_context.real_recv_mem_type = vmpp_MEM_DEVICE;
            } else if (g_transcode_context.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_HOST ||
                       g_transcode_context.memory_mode == vmpp_DEC_MEM_LESS_DEV_MEM) {
                g_transcode_context.real_recv_mem_type = vmpp_MEM_HOST;
            } else if (g_transcode_context.encoder_enable == 1 ||
                       (!g_transcode_context.save && !g_transcode_context.check_md5)) {
                g_transcode_context.real_recv_mem_type = vmpp_MEM_DEVICE;
            } else {
                g_transcode_context.real_recv_mem_type = vmpp_MEM_HOST;
            }
            LOGIL_INFO("[transcode_mt] The auto dec recv memory type %d (%s)!", g_transcode_context.real_recv_mem_type,
                g_transcode_context.real_recv_mem_type == vmpp_MEM_DEVICE ? "DEVICE" : "HOST");
        }
    }

    if (g_transcode_context.multi_device) {
        LOGIL_INFO("[transcode_mt] Run '%s' parallel in %d thread(s) with %d file(s) on %d multi-device(s)!",
            job_string, g_transcode_context.thread_count, vmpp_queue_size(g_transcode_context.urls),
            vmpp_queue_size(g_transcode_context.devices));
    } else if (g_transcode_context.random_device) {
        LOGIL_INFO("[transcode_mt] Run '%s' parallel in %d thread(s) with %d file(s) on %d random-device(s)!",
            job_string, g_transcode_context.thread_count, vmpp_queue_size(g_transcode_context.urls),
            vmpp_queue_size(g_transcode_context.devices));
    } else {
        LOGIL_INFO("[transcode_mt] Run '%s' parallel in %d thread(s) with %d file(s) on { %s }!", job_string,
            g_transcode_context.thread_count, vmpp_queue_size(g_transcode_context.urls),
            g_transcode_context.device_name);
    }

    return 0;
}

static void options_cleanup()
{
    void *tmp;
    int j;
    for (j = 0; j < vmpp_queue_size(g_transcode_context.urls); j++) {
        tmp = vmpp_queue_peek(g_transcode_context.urls, j);
        free(tmp);
    }
    vmpp_queue_free(&g_transcode_context.urls);

    for (j = 0; j < vmpp_queue_size(g_transcode_context.devices); j++) {
        tmp = vmpp_queue_peek(g_transcode_context.devices, j);
        free(tmp);
    }
    vmpp_queue_free(&g_transcode_context.devices);

    for (j = 0; j < vmpp_queue_size(g_transcode_context.customized_enc_opts); j++) {
        tmp = vmpp_queue_peek(g_transcode_context.customized_enc_opts, j);
        if (((enc_options *)tmp)->pCom) {
            free(((enc_options *)tmp)->pCom);
        }
        free(tmp);
    }
    vmpp_queue_free(&g_transcode_context.customized_enc_opts);
}

static int get_device_id(const char *video_device)
{
    int device_id = 0;
    if (video_device) {
        if (strncmp(video_device, "/dev/vastai_video", strlen("/dev/vastai_video")) == 0) {
            sscanf(video_device, "/dev/vastai_video%d", &device_id);
        } else {
            sscanf(video_device, "/dev/va_video%d", &device_id);
        }
    }
    return device_id;
}

static int runtime_setup_global()
{
    int ret;
    int dieId = 0;
    int i;
    char *device = NULL;
    vaccrt_init_t vaccrt_init;
    ret = open_runtime(&g_runtime_instance);
    if (ret < 0) {
        LOG_ERROR("[transcode_mt] open runtime failed!");
        return -1;
    }
    vaccrt_init = (vaccrt_init_t)(&g_runtime_instance)->init;
    if (g_transcode_context.multi_device || g_transcode_context.random_device) {
        for (i = 0; i < vmpp_queue_size(g_transcode_context.devices); i++) {
            device = (char *)vmpp_queue_peek(g_transcode_context.devices, i);
            dieId = get_device_id(device);
            LOG_INFO("[transcode_mt] vaccrt_init: dieId = %d", dieId);
            rtError_t vaccRet = vaccrt_init(dieId);
            if (vaccRet) {
                LOG_ERROR("[transcode_mt] vaccrt_init failed: err %d", vaccRet);
                return -1;
            }
        }
    } else if (g_transcode_context.device_name) {
        dieId = get_device_id(g_transcode_context.device_name);
        LOG_INFO("[transcode_mt] vaccrt_init: dieId = %d", dieId);
        rtError_t vaccRet = vaccrt_init(dieId);
        if (vaccRet) {
            LOG_ERROR("[transcode_mt] vaccrt_init failed: err %d", vaccRet);
            return -1;
        }
    } else {
        LOG_ERROR("[transcode_mt] Invalid Status: should never reach here.");
        return -1;
    }
    return 0;
}

static void runtime_cleanup_global()
{
    close_runtime(&g_runtime_instance);
}

static vmppRuntimeInstance *runtime_setup(const char *video_device)
{
    int ret;
    int dieId = 0;
    vaccrt_init_t vaccrt_init;
    vmppRuntimeInstance *runtime_inst = NULL;

    if (!video_device) {
        LOG_ERROR("[transcode_mt] video device not provided!");
        return NULL;
    }

    runtime_inst = (vmppRuntimeInstance *)malloc(sizeof(vmppRuntimeInstance));
    if (!runtime_inst) {
        LOG_ERROR("[transcode_mt] malloc memory for runtime instance failed!");
        return NULL;
    }

    ret = open_runtime(runtime_inst);
    if (ret < 0) {
        LOG_ERROR("[transcode_mt] open runtime failed!");
        close_runtime(runtime_inst);
        free(runtime_inst);
        return NULL;
    }
    vaccrt_init = (vaccrt_init_t)runtime_inst->init;
    dieId = get_device_id(video_device);
    LOG_DEBUG("[transcode_mt] vaccrt_init: dieId = %d", dieId);
    rtError_t vaccRet = vaccrt_init(dieId);
    if (vaccRet) {
        LOG_ERROR("[transcode_mt] vaccrt_init failed: err %d", vaccRet);
        close_runtime(runtime_inst);
        free(runtime_inst);
        return NULL;
    }

    return runtime_inst;
}

static void runtime_cleanup(vmppRuntimeInstance *runtime_inst)
{
    if (runtime_inst) {
        close_runtime(runtime_inst);
        free(runtime_inst);
        runtime_inst = NULL;
    }
}

static int prefecth_log_options(params_log_t *params_log)
{
    // struct parameter prm;
    // int ret;
    // char *optarg;
    char log_path[MAX_PATH_LEN] = { 0 };
    memset(&g_transcode_context, 0, sizeof(transcode_context));
    g_transcode_context.log_level = LOG_LEVEL_INFO;
    g_transcode_context.log_level_sdk = vmpp_LOG_WARN;
    g_transcode_context.log_level_ffmpeg = vmpp_LOG_WARN;

    // prm.cnt = 1;
    g_transcode_context.log_into_file = params_log->log_into_file;
    g_transcode_context.log_level = params_log->log_level;
    g_transcode_context.log_level_sdk = params_log->log_level_sdk;
    g_transcode_context.log_level_ffmpeg = params_log->log_level_ffmpeg;
    g_transcode_context.log_callback = params_log->log_callback;
    g_transcode_context.enable_error_assert = params_log->enable_error_assert;
    // while ((ret = get_option(argc, argv, ops, &prm)) != -1) {
    //     optarg = prm.argument;
    //     switch (prm.short_opt) {
    //     case '0':
        //     if (strcmp(prm.longOpt, "log2File") == 0) {
        //         g_transcode_context.log_into_file = atof(optarg);
        //     } else if (strcmp(prm.longOpt, "logLevel") == 0) {
        //         g_transcode_context.log_level = atoi(optarg);
        //     } else if (strcmp(prm.longOpt, "logLevelSDK") == 0) {
        //         g_transcode_context.log_level_sdk = atoi(optarg);
            // } else if (strcmp(prm.longOpt, "logLevelFFmpeg") == 0) {
            //     g_transcode_context.log_level_ffmpeg = atoi(optarg);
        //     } else if (strcmp(prm.longOpt, "logCallback") == 0) {
        //         g_transcode_context.log_callback = atoi(optarg);
        //     } else if (strcmp(prm.longOpt, "enableErrorAssert") == 0) {
        //         g_transcode_context.enable_error_assert = atoi(optarg);
        //     }
        //     break;
        // default:
        //     break;
    //     }
    // }

    if (g_transcode_context.log_level_sdk < vmpp_LOG_TRACE || g_transcode_context.log_level_sdk > vmpp_LOG_FATAL) {
        g_transcode_context.log_level_sdk = vmpp_LOG_WARN;
    }

    if (g_transcode_context.log_level < LOG_LEVEL_TRACE || g_transcode_context.log_level > LOG_LEVEL_FATAL) {
        g_transcode_context.log_level = LOG_LEVEL_INFO;
    }

    if (g_transcode_context.log_into_file) {
        sprintf(log_path, "transcode_mt_%s_pid%d.txt", timenow(VA_TIMESTAMP2), getpid());
        setLogFile(log_path);
    }

    setLogLevel((enum logLevel)g_transcode_context.log_level);
    setLogErrorAssert(g_transcode_context.enable_error_assert);

    return 0;
}

void log_cb(
    const void *pUser, int level, const char *module, const char *file, const char *func, int line, const char *msg)
{
    UNUSED(pUser);
    switch (level) {
    case vmpp_LOG_FATAL:
        LOGIL(LOG_LEVEL_FATAL, COLOR_PURPLE, "[SDK][%s %s:%d %s] %s", module, file, line, func, msg);
        break;
    case vmpp_LOG_ERROR:
        LOGIL(LOG_LEVEL_ERROR, COLOR_LIGHT_RED, "[SDK][%s %s:%d %s] %s", module, file, line, func, msg);
        break;
    case vmpp_LOG_WARN:
        LOGIL(LOG_LEVEL_WARN, COLOR_YELLOW, "[SDK][%s %s:%d %s] %s", module, file, line, func, msg);
        break;
    case vmpp_LOG_INFO:
        LOGIL(LOG_LEVEL_INFO, COLOR_CYAN, "[SDK][%s %s:%d %s] %s", module, file, line, func, msg);
        break;
    case vmpp_LOG_DEBUG:
        LOGIL(LOG_LEVEL_DEBUG, COLOR_WHITE, "[SDK][%s %s:%d %s] %s", module, file, line, func, msg);
        break;
    case vmpp_LOG_TRACE:
    default:
        LOGIL(LOG_LEVEL_TRACE, COLOR_DARK_GRAY, "[SDK][%s %s:%d %s] %s", module, file, line, func, msg);
        break;
    }
}

static int vmpp_global_init()
{
    int ret = 0;
    vmppConfiguration cfg = { 0 };
    cfg.runtimeInst = g_runtime_instance;
    cfg.logCtx.enableCustomLog = 1;
    cfg.logCtx.logLevel = (vmppLogLevel)g_transcode_context.log_level_sdk;
    if (g_transcode_context.log_callback) {
        cfg.logCtx.logCallback = log_cb;
    }

    ret = vmppInitDecoder(&cfg);
    if (ret != vmpp_RSLT_OK) {
        LOG_ERROR("[transcode_mt] vmppInitDecoder failed %d", ret);
        return -1;
    }
    if (g_transcode_context.encoder_enable) {
        ret = vmppInitEncoder(&cfg);
        if (ret != vmpp_RSLT_OK) {
            LOG_ERROR("[transcode_mt] vmppInitEncoder failed %d", ret);
            return -1;
        }
    }
    return 0;
}

static int vmpp_global_deinit()
{
    vmppDeInitDecoder();
    if (g_transcode_context.encoder_enable) {
        vmppDeInitEncoder();
    }
    return 0;
}

#ifdef USING_FFMPEG
extern void ff_log_cb(void *ptr, int level, const char *fmt, va_list vl);
#endif

extern char *params_enc[36];
extern int params_enc_num;

#define PRINT_STR(field) printf("  %s: %s\n", #field, params->field ? params->field : "(null)")
#define PRINT_INT(field) printf("  %s: %d\n", #field, params->field)

void print_params_transcode_mt(const params_transcode_mt_t *params) {
    printf("params_transcode_mt_t:\n");
    PRINT_STR(input);
    PRINT_STR(device_name);
    PRINT_STR(enc_codec);
    PRINT_STR(output_directory);
    PRINT_STR(output_file);
    PRINT_INT(loop);
    PRINT_INT(main_loop);
    PRINT_INT(save);
    PRINT_INT(check_md5);
    PRINT_STR(codec);
    PRINT_INT(using_ffmpeg);
    PRINT_INT(log_period);
    PRINT_INT(perf_period);
    PRINT_INT(vframes);
    PRINT_INT(bitDepth);
    PRINT_INT(memory_mode);
    PRINT_INT(thread_count);
    PRINT_INT(dec_output_align);
    PRINT_INT(argc);

    printf("  argv: ");
    if (params->argv) {
        for (int i = 0; i < params->argc; i++) {
            printf("%s ", params->argv[i] ? params->argv[i] : "(null)");
        }
    }
    printf("\n");

    printf("  params_log:\n");
    PRINT_INT(params_log.log_into_file);
    PRINT_INT(params_log.log_level);
    PRINT_INT(params_log.log_level_sdk);
    PRINT_INT(params_log.log_level_ffmpeg);
    PRINT_INT(params_log.log_callback);
    PRINT_INT(params_log.enable_error_assert);
}


int transcode_mt(params_transcode_mt_t *params_ut)
{
    print_params_transcode_mt(params_ut);

    #ifdef USING_FFMPEG
    printf("==============================there is defined USING_FFMPEG\n");
    #endif
    int ret = 0;
    int i, tasks = 0;
    transcode_thread_params *params = NULL;

    /* prefetch and setup options related to logs */
    prefecth_log_options(&params_ut->params_log);

    LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_GREEN, "[transcode_mt] Let's Go!");

    for (int i=0; i<params_enc_num; i++)
        printf("%s ", params_enc[i]);
    printf("\nHere are argv[]\n");

    ret = options_setup(params_ut);
    if (ret) {
        goto main_finish;
    }

#ifdef USING_FFMPEG
    if (g_transcode_context.using_ffmpeg) {
        ff_set_log_level(g_transcode_context.log_level_ffmpeg);
        if (g_transcode_context.log_callback) {
            ff_set_log_callback(ff_log_cb);
        }
    }
#endif

    if (!g_transcode_context.multi_runtime) {
        LOG_TRACE("[transcode_mt] setup global runtime!");
        ret = runtime_setup_global();
        if (ret) {
            LOG_ERROR("[transcode_mt] global setup runtime failed!");
            goto main_finish;
        }

        LOG_TRACE("[transcode_mt] do global initialization for vmpp!");
        ret = vmpp_global_init();
        if (ret < 0) {
            LOG_ERROR("[transcode_mt] global init vmpp failed!");
            goto main_finish;
        }
    }

    params = (transcode_thread_params*)calloc(g_transcode_context.thread_count, sizeof(transcode_thread_params));
    if (!params) {
        LOG_ERROR("[transcode_mt] malloc parameters failed!");
        goto main_finish;
    }

    for (i = 0; i < g_transcode_context.thread_count; i++) {
        params[i].thread_index = i;
        if (g_transcode_context.encoder_enable) {
            pthread_mutex_init(&params[i].frame_mutex, NULL);
            if (g_transcode_context.another_thread_for_enc_out) {
                pthread_mutex_init(&params[i].stream_mutex, NULL);
            }
        }

        ret = pthread_create(&params[i].thread_handle, NULL, transcode_thread, &params[i]);
        if (ret != 0) {
            LOG_ERROR("[transcode_mt] Fail to start transcoding thread[%d] ret %d, info: url '%s', device { %s }", i,
                ret, params[i].current_url, params[i].current_device);
            break;
        }
        LOG_TRACE("[transcode_mt %3d] transcode thread started.", i);
    }

    for (i = 0; i < g_transcode_context.thread_count; i++) {
        if (is_thread_active(&params[i].thread_handle)) {
            int ret = pthread_join(params[i].thread_handle, NULL);
            if (ret != 0) {
                fprintf(stderr, "pthread_join 错误 [%d]: %s\n", i, strerror(ret));
                // ESRCH: 线程无效；EINVAL: 线程为分离状态；EDEADLK: 死锁
            }
        }
    }

    LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_GREEN, "[transcode_mt] All %d '%s' thread(s) exited.",
        g_transcode_context.thread_count,
        g_transcode_context.encoder_enable ? (g_transcode_context.encode_yuv ? "Encoding" : "Transcoding") :
                                             "Decoding");

main_finish:
    if (params) {
        for (i = 0; i < g_transcode_context.thread_count; i++) {
            if (g_transcode_context.encoder_enable) {
                pthread_mutex_destroy(&params[i].frame_mutex);
                if (g_transcode_context.another_thread_for_enc_out) {
                    pthread_mutex_destroy(&params[i].stream_mutex);
                }
            }
            tasks += params[i].job_index;
        }
        free(params);
    }

    if (!g_transcode_context.multi_runtime) {
        LOG_TRACE("[transcode_mt] do global deinitialization for vmpp!");
        vmpp_global_deinit();
        LOG_TRACE("[transcode_mt] cleanup global runtime!");
        runtime_cleanup_global();
    }
    options_cleanup();

    if (ret < 0 || g_transcode_context.dec_error_cnt || g_transcode_context.enc_error_cnt ||
        g_transcode_context.input_error_cnt || g_transcode_context.output_error_cnt ||
        g_transcode_context.stats_error_cnt) {
        LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_RED,
            "[transcode_mt] Oops!! We got (a) problem(s) during the execution of total %d task(s): Dec %d, Enc %d, In %d, Out %d, Stats %d!",
            tasks, g_transcode_context.dec_error_cnt, g_transcode_context.enc_error_cnt,
            g_transcode_context.input_error_cnt, g_transcode_context.output_error_cnt,
            g_transcode_context.stats_error_cnt);
    } else if (tasks) {
        LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_GREEN, "[transcode_mt] Total %d task(s) have been successfully completed!",
            tasks);
    }

    LOGIL(LOG_LEVEL_INFO, COLOR_LIGHT_GREEN, "[transcode_mt] Goodbye!");
    if (g_transcode_context.log_into_file) {
        closeLogFile();
    }

    return ret;
}
