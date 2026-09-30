#include <assert.h>
#include <ctype.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <libgen.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
// #define ENABLE_LATENCY_TEST
#ifdef ENABLE_LATENCY_TEST
#include <semaphore.h>
#endif
#include <unistd.h>

#include "defs.h"
#include "md5.h"
#include "utils.h"
#include "stream.h"
#include "vmpp_dec_api.h"
#include "vmpp_dec_defs.h"

#define EXT_OUTPUT_BUF_NUM (20)

#define MAX_STREAM_SIZE (16 * 1024 * 1024)
#define MAX_OUT_SIZE (24 * 1024 * 1024)
#define MD5_BUFSIZE (16 * 1024)
#define SEND_STREAM_TIMEOUT_VALUE (0XFFFFFF)
#define MAX_VIDEO_DEC_WIDTH 8192
#define MAX_VIDEO_DEC_HEIGHT 8192

#define MIN(x, y) ((x) < (y)) ? ((x)) : ((y))

//#define PRINT_SINGLE_MD5

//#define ENABLE_INPUT_THREAD
#ifdef ENABLE_INPUT_THREAD
#include "buf_queue.h"
#endif

//#define RETURN_ON_STREAM_ERROR
//#define DROP_TEST
//#define STREAM_ERROR_TEST
//#define PERFORMANCE_PRINT_ON_EACH_FRAME
//#define DEBUG_SINGLE_FRAME_SAVE
//#define   DPB_IDLE_COUNT_OUTPUT

typedef int RET_TYPE;
typedef void *task_handle;
typedef void *(*task_func)(void *);

#ifdef ENABLE_LATENCY_TEST
// for latency test
FILE *latency_log;
static struct timeval st;
static struct timeval ed;
uint32_t more_num=0, count = 0;

#define SIZE 3
int buff[SIZE] = {0};
sem_t sem_r;//保存可读信号量的变量，代表buf可读资源数
sem_t sem_w;//保存可写信号量的变量，代表buf可写资源数
#endif

typedef struct {
    char*               decDevice;          // video decoder device node name
    char*               memDevice;          // memory device node name
    char *input;
    char *output;
    char *codec;
    char *pix_fmt;
    uint32_t vframes;
    int save;
    int md5;
    int loop;
    int core_mode;
    int memory_mode;
    int output_align;
    int decode_mode;
    int log_level;
    int crop;
    char *crop_detail;
    int no_output_reordering;
    int buf_slim_mode;
    int stream_repeat_times;
    int apiMode;
} option_t;

typedef struct {
    stream_context_ptr strmctx;
#ifdef ENABLE_INPUT_THREAD
    queue_t *queue;
#endif
} input_t;



static option_t *option = NULL;
static struct timeval tBegin, tEnd;
static struct timeval tGlobalBegin;
int has_begin = 0;
int loop = 0;
int md5ctx_inited = 0;
struct md5_context md5ctx = {0};
uint8_t cur_md5sum[MD5_HASH_LEN] = {0};
uint8_t last_md5sum[MD5_HASH_LEN] = {0};
int md5_saved = 0;
int end_decoding = 0;
int repeat_time = 0;

#ifdef ENABLE_INPUT_THREAD
int has_read_file = 0;
#endif

static void usage(const char *program)
{
    LOG(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN,
        "Usage: %s -i [input] -o [output] -d [video decoder device] -m [memory device] -c [codec] -s [save] -k [check md5] -l [loop] -n [vframe] "
        "-C [coreMode] -a [outputAlignment] -M [memoryMode] -D [decodeMode] -L [logLevel] -P [crop] -p [cropDetail] "
        "-N [noOutputReordering] -S [bufSlimMode] -r [streamRepeat]  -A [apiMode]",
        program);
    LOG_INFO("  input[M]: the input file");
    LOG_INFO("  output[O]: the output file, default: output.yuv");
    LOG_INFO("  video device[O]: render video device name, default: /dev/hantrodec");
    LOG_INFO("  memory device[O]: render memory device name, default: /dev/memalloc");
    LOG_INFO("  codec[O]: codec type string, default: h264");
    LOG_INFO("  pix_fmt[O]: pixel format, default: NV12, options: NV12 or P010");
    LOG_INFO("  vframes[O]: frame number to be decoded");
    LOG_INFO("  save[O]: need save yuv file, default: 1");
    LOG_INFO("  md5[O]: need calculate md5, default: 1");
    LOG_INFO("  loop[O]: loop times, default: 1");
    LOG_INFO("  coreMode[O]: core mode, default 0, 0-auto, 1-single core, 2-multicore");
    LOG_INFO("  outputAlignment[O]: output alignment, default 0");
    LOG_INFO("  memoryMode[O]: memory mode, default: 0");
    LOG_INFO("  decodeMode[O]: decode mode, default: 0, 0-NORMAL, 1-INTRA-ONLY, 2-SKIP-NONREF, 3-LOW-DELAY, 4-NO-BFRAME");
    LOG_INFO("  logLevel[O]: SDK log level, default: 2(INFO),  1-DEBUG 2-INFO 3-WARN 4-ERROR");
    LOG_INFO("  crop[O]: crop flag, disable:0, default crop:1, customized crop:2");
    LOG_INFO("  crop_detail[O]: string format: 'x=?,y=?,w=?,h=?'");
    LOG_INFO("  noOutputReordering[O]: turn off output reordering");
    LOG_INFO("  bufSlimMode[O]: to reduce DDR buffer usage");
    LOG_INFO("  streamRepeat[O]: decode stream repeat times in one channel, default: 0");
    LOG_INFO("  apiMode[O]: decode api mode, default: 0, 0-parallel mode, 1-serial mode");
    LOG_INFO(" ./video_dec -i /home/stone/workspace/sample_640x360.hevc -o output.yuv -d /dev/hantrodec  -m /dev/memalloc -c hevc -l 1 -k 0");
    LOG_INFO(" ./video_dec -i /home/stone/workspace/sample_1920x1080.hevc -o output.yuv -d /dev/hantrodec  -m /dev/memalloc -c hevc -l 1 -k 0");
    LOG_INFO(" ./video_dec -i /home/stone/workspace/sintel_trailer_1920x1080p_1253_IBBBP.h264 -o output.yuv -d /dev/hantrodec  -m /dev/memalloc -c h264 -l 1 -k 0");
}

static int parse_options(int argc, char **argv, option_t *opt)
{
    // h264_dec_mt -i input.json -s 0
#ifdef ENABLE_LATENCY_TEST
    static const char optstr[] = "i:o:d:m:c:f:n:s:k:l:h:C:M:a:D:L:P:p:F:";
    char logname[100];
#else
    static const char optstr[] = "i:o:d:m:c:f:n:s:k:l:h:C:M:a:D:L:P:p:N:S:r:A:";
#endif
    int c;
    while ((c = getopt(argc, argv, optstr)) != -1) {
        switch (c) {
        case 'i':
            opt->input = optarg;
            break;
        case 'o':
            opt->output = optarg;
            break;
        case 'd':
            opt->decDevice = optarg;
            break;
        case 'm':
            opt->memDevice = optarg;
            break;
        case 'c':
            opt->codec = optarg;
            break;
        case 'f':
            opt->pix_fmt = optarg;
            break;
        case 'n':
            opt->vframes = atoi(optarg);
            break;
        case 's':
            opt->save = atoi(optarg);
            break;
        case 'k':
            opt->md5 = atoi(optarg);
            break;
        case 'l':
            opt->loop = atoi(optarg);
            break;
        case 'C':
            opt->core_mode = atoi(optarg);
            break;
        case 'M':
            opt->memory_mode = atoi(optarg);
            break;
        case 'a':
            opt->output_align = atoi(optarg);
            break;
        case 'D':
            opt->decode_mode = atoi(optarg);
            break;  
        case 'L':
            opt->log_level = atoi(optarg);
            break;
        case 'P':
            opt->crop = atoi(optarg);
            break;
        case 'p':
            opt->crop_detail = optarg;
            break;
        case 'N':
            opt->no_output_reordering = atoi(optarg);
            break;
        case 'S':
            opt->buf_slim_mode = atoi(optarg);
            break;
#ifdef ENABLE_LATENCY_TEST
        case 'F':
            strcpy(logname, optarg);
            printf("%c with args %s\n", c, logname);

            latency_log = fopen(logname,"w+");
            if(latency_log == NULL)
                printf("error:open file %s failed !\n",logname);
            else
                printf("open file %s success!\n",logname);
            break;
#endif
        case 'r':
            opt->stream_repeat_times = atoi(optarg);
            break;
        case 'A':
            opt->apiMode = atoi(optarg);
            break;
        case 'h':
            return -2;
        case '?':
            return -1;
        default:
            LOG_ERROR(" Unsupported option: %c ", c);
            return -1;
        }
    }

    if (optind < argc) {
        return -1;
    }

    return 0;
}


static task_handle run_task(task_func func, void *param)
{
    int ret;
    pthread_t *thread_handle = malloc(sizeof(pthread_t));

#ifndef ARM64
    pthread_attr_t attr;
    struct sched_param par;
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

static void get_output_file(char *output)
{
    if (option->loop == 1)
        sprintf(output, "%s", option->output);
    else {
        const char *suffix = ".yuv";
        char name[1001] = {0};
        strncpy(name, option->output, MIN(strlen(option->output) - strlen(suffix), 1000));
        sprintf(output, "%s_%d.yuv", name, loop % 3);
    }
}

#ifdef PRINT_SINGLE_MD5
unsigned char *compute_md5sum_single(const unsigned char *buf_in, const int buf_len,
                                     unsigned char *str_md5sum)
{
    struct md5_context md5ctx_s;

    if (buf_in == NULL || buf_len == 0) {
        sprintf((char *)str_md5sum, "INVALID BUFFER!");
        return str_md5sum;
    }

    memset(&md5ctx_s, 0, sizeof(struct md5_context));
    md5_init(&md5ctx_s);
    md5_update(&md5ctx_s, buf_in, (unsigned long)buf_len);
    md5_final(str_md5sum, &md5ctx_s);

    return str_md5sum;
}
#endif

unsigned char *compute_md5sum(const unsigned char *buf_in, const int buf_len,
                              unsigned char *str_md5sum)
{
    if (md5ctx_inited == 0) {
        md5_init(&md5ctx);
        md5ctx_inited = 1;
    }

    if (buf_in == NULL || buf_len == 0) {
        md5_final(str_md5sum, &md5ctx);
        md5ctx_inited = 0;
    } else {
#ifdef PRINT_SINGLE_MD5
        compute_md5sum_single(buf_in, buf_len, str_md5sum);
#endif
        md5_update(&md5ctx, buf_in, (unsigned long)buf_len);
    }

    return str_md5sum;
}

#ifdef ENABLE_INPUT_THREAD


static void *read_file_thread(void *arg)
{
    if (!arg) {
        LOG_ERROR("read file error, arg is null.");
        return NULL;
    }
    input_t *input = (input_t *)arg;
    queue_t *queue = (queue_t *)input->queue;
    stream_context_ptr strmctx = input->strmctx;
    uint32_t stream_len = 0;
#ifdef READ_FRAME_OPTIMIZE
    uint8_t *stream_p;
#else
    free(strmctx->buffer);
#endif

    while (1) {
        buf_t *buf = write_buffer(queue);
        if (!buf) {
            // sched_yield();
            continue;
        }
#ifdef READ_FRAME_OPTIMIZE
        stream_len = stream_read_frame(strmctx, &stream_p);
        memmove(buf->buffer,stream_p, stream_len);
#else
        strmctx->buffer = buf->buffer;
        stream_len = stream_read_frame(strmctx, &buf->buffer);
        strmctx->buffer = NULL;
#endif
        LOG_DEBUG("got a nal. write buffer %p, index %d, len %d.", buf, buf->index, stream_len);
        if (stream_len == 0) {
            has_read_file = 1;
            buf->length = 0;
            break;
        } else {
            buf->length = stream_len;
            move_to_next_write_buffer(queue);
        }
        // sched_yield();
    }
    return NULL;
}
#endif

static char *formatDuring(long uss, char *buf)
{
    long mss = uss / 1000;
    long days = mss / (1000 * 60 * 60 * 24);
    long hours = (mss % (1000 * 60 * 60 * 24)) / (1000 * 60 * 60);
    long minutes = (mss % (1000 * 60 * 60)) / (1000 * 60);
    long seconds = (mss % (1000 * 60)) / 1000;
    long mini_sec = mss % 1000;
    long micro_sec = uss % 1000;

    if (days > 0)
        sprintf(buf, "%ld days, %ld hours, %ld minutes, %ld sec, %ld.%ld ms", days, hours, minutes,
                seconds, mini_sec, micro_sec);
    else if (hours > 0)
        sprintf(buf, "%ld hours, %ld minutes, %ld sec, %ld.%ld ms", hours, minutes, seconds,
                mini_sec, micro_sec);
    else if (minutes > 0)
        sprintf(buf, "%ld minutes, %ld sec, %ld.%ld ms", minutes, seconds, mini_sec, micro_sec);
    else if (seconds > 0)
        sprintf(buf, "%ld sec, %ld.%ld ms", seconds, mini_sec, micro_sec);
    else
        sprintf(buf, "%ld.%ld ms", mini_sec, micro_sec);
    return buf;
}

static void *output_thread(void *arg)
{
    vmppFrame out_frame;
    vmppDecOutputOptions out_opt;
    void *dec_ch;
    vmppResult ret;
    char output_file_str[MAX_PATH_LEN] = {0};
    FILE *output_file = NULL;
    uint8_t *frame_buffer = NULL;
#ifdef ENABLE_LATENCY_TEST
    int time_latency = 0;
#endif

    out_opt.memoryType = vmpp_MEM_HOST;
    if (option->memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_HOST) {
        frame_buffer = malloc(MAX_VIDEO_DEC_WIDTH * MAX_VIDEO_DEC_HEIGHT * 3 / 2);
    } else if (option->memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_DEV) {
        out_opt.memoryType = vmpp_MEM_DEVICE;
        option->save = 0; // force to not save to avoid crash issue
    } else if (option->memory_mode == vmpp_DEC_MEM_LESS_DEV_MEM) {
        out_opt.memoryType = vmpp_MEM_HOST; // only support host memory
    } else {
        out_opt.memoryType = option->save ? vmpp_MEM_HOST : vmpp_MEM_DEVICE;
    }

    if (out_opt.memoryType != vmpp_MEM_HOST)
        option->md5 = 0;

    dec_ch = arg;
    get_output_file(output_file_str);

    out_opt.enableCrop = 0;

    if (option->save) {
        output_file = fopen(output_file_str, "wb");
    }

    uint32_t out_count = 0;

    while (1) {
        if (option->memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_HOST)
            out_frame.data[0] = frame_buffer;

        // fake device address, only for function instructions
        if (option->memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_DEV)
            out_frame.busAddress[0] = 0x10ad9c0000; //0x900000000;

#ifdef ENABLE_LATENCY_TEST
        sem_wait(&sem_r);
#endif
        ret = vmppDecReceiveFrame(dec_ch, &out_frame, &out_opt, 500);
#ifdef ENABLE_LATENCY_TEST
        gettimeofday(&ed,NULL);
        time_latency = (ed.tv_sec-st.tv_sec)*1000000+ed.tv_usec-st.tv_usec;
        if (!(count > 10 && count-out_count <= more_num))
            fprintf(latency_log,"%ld\t %ld\t %ld \n",st.tv_usec,ed.tv_usec,ed.tv_sec-st.tv_sec);
        if (time_latency <= 0) {
            printf("---------------------------------err: negative value\n");
            fprintf(latency_log,"< <0 >\n");
            exit(-1);
        }
        sem_post(&sem_w);
#endif
        if (ret == vmpp_RSLT_OK) {
            out_count++;
            LOG_INFO("vmppDecReceiveFrame(%d) %s Frame, pts %d, [%dx%d, %dx%d, %d %d] %s",
                         out_count, (out_frame.frameType == 0)?("I"): ((out_frame.frameType == 1)?("P"): ("B")),
                         (uint32_t)out_frame.pts, out_frame.width, out_frame.height,
                         out_frame.cropInfo.width, out_frame.cropInfo.height, out_frame.stride[0],
                         out_frame.stride[1], (option->save) ? ("saved!"): (""));
            if (out_frame.seiCount > 0) {
                uint32_t sei_index;
                for (sei_index = 0; sei_index < out_frame.seiCount; sei_index++) {
                    vmppSEI *sei = out_frame.seiData[sei_index];
                    if (sei) {
                        switch (sei->payloadType) {
                        case SEI_USER_DATA_UNREGISTERED:
                            LOG_INFO("size: %d, user data:%s", sei->payloadDataSize, sei->payloadData);
                            break;

                        default:
                            break;
                        }
                    }
                }
            }

            if (out_frame.pixelFormat == vmpp_PIX_FMT_YUV420_PLANAR_10BIT_P010
                || out_frame.pixelFormat == vmpp_PIX_FMT_YUV420_PLANAR_10BIT_I010) {
                out_frame.width *= 2;
                out_frame.cropInfo.width *= 2;
            }

            /* check md5. */
            if (option->md5 && out_frame.memoryType == vmpp_MEM_HOST) {
                if (!end_decoding)
                    compute_md5sum((const unsigned char *)out_frame.data[0], out_frame.dataSize,
                               cur_md5sum);
#ifdef PRINT_SINGLE_MD5
                printf("frame %2d: ", out_count);
                print_md5(cur_md5sum, MD5_HASH_LEN);
#endif
            }
            /* save output file. */
            if (option->save && end_decoding == 0 && out_frame.memoryType == vmpp_MEM_HOST &&
                output_file) {
                if (out_opt.enableCrop == 1) {
                    //  write Y
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
                        fwrite(out_frame.data[0] + out_frame.stride[0] * (j + out_frame.cropInfo.yOffset) +
                               out_frame.cropInfo.xOffset, 1, out_frame.cropInfo.width, output_file);
                    }

                    uint32_t uv_crop_width = (out_frame.cropInfo.width % 2 == 1)
                                                 ? (out_frame.cropInfo.width + 1)
                                                 : (out_frame.cropInfo.width);
                    uint32_t uv_crop_height = (out_frame.cropInfo.height % 2 == 1)
                                                  ? (out_frame.cropInfo.height + 1)
                                                  : (out_frame.cropInfo.height);

                    // write UV
                    if (out_frame.data[1]) {
                        for (uint32_t j = 0; j < uv_crop_height / 2; j++) {
                            fwrite(out_frame.data[1] + out_frame.stride[1] * (j + out_frame.cropInfo.yOffset / 2) +
                                   out_frame.cropInfo.xOffset / 2, 1, uv_crop_width, output_file);
                        }
                    }

                    /* not to crop */
                    // fwrite(out_frame.data[0], 1, out_frame.dataSize, output_file);
                    // LOG_INFO("[APP][%p][en %d]write YUV done! width: %d, height: %d", dec_ch,
                    // out_opt.crop_en, out_frame.width,
                    //    out_frame.height);
                }
                // LOG_INFO("[APP][%p][en %d] write and crop YUV done! orig: %dx%d, crop: %dx%d, "
                //          "stride: %d %d",
                //          dec_ch, out_opt.enableCrop, out_frame.width, out_frame.height,
                //          out_frame.cropInfo.width, out_frame.cropInfo.height, out_frame.stride[0],
                //          out_frame.stride[1]);
            }

            if (option->vframes > 0 && out_count >= option->vframes) {
                end_decoding = 1;
            }

#ifdef DEBUG_SINGLE_FRAME_SAVE
            FILE *fd_single_frame = fopen("debug_single_frame.yuv", "wb");
            fwrite(out_frame.data[0], 1, out_frame.dataSize /*output_len*/, fd_single_frame);
            fclose(fd_single_frame);
#endif
#ifdef PERFORMANCE_PRINT_ON_EACH_FRAME
            /* print performance. */
            gettimeofday(&tEnd, NULL);
            long deltaTime =
                1000000L * (tEnd.tv_sec - tBegin.tv_sec) + (tEnd.tv_usec - tBegin.tv_usec);
            float frame_rate = (float)(out_count) / (float)(deltaTime)*1000000L;
            LOG_INFO("[APP][%p][Performance] loop %d, total %d frames, spend %ld us, frame rate "
                     "%.2ffps",
                     dec_ch, loop, out_count, deltaTime, frame_rate);
#endif
            ret = vmppDecReleaseFrame(dec_ch, &out_frame, 500);
            if (ret < 0) {
                LOG_ERROR("release frame error %d.", ret);
            }
            continue;
        } else if (ret == vmpp_RSLT_WARN_EOS) {
            /* print performance. */
            gettimeofday(&tEnd, NULL);
            long deltaTime =
                1000000L * (tEnd.tv_sec - tBegin.tv_sec) + (tEnd.tv_usec - tBegin.tv_usec);
            long deltaTimeGlobal = 1000000L * (tEnd.tv_sec - tGlobalBegin.tv_sec) +
                                   (tEnd.tv_usec - tGlobalBegin.tv_usec);
            float frame_rate = (float)(out_count) / (float)(deltaTime)*1000000L;
            char buf1[100], buf2[100];
            LOG_INFO("[APP][%p][Performance] loop %d, spend %ld us, during %ld us", dec_ch, loop,
                     deltaTime, deltaTimeGlobal);
            LOG_INFO("[APP][%p][Performance] Decoded %s, loop %d, total %d frames, spend (%s), "
                     "frame rate %.2ffps, during (%s)",
                     dec_ch, option->input, loop, out_count, formatDuring(deltaTime, buf1),
                     frame_rate, formatDuring(deltaTimeGlobal, buf2));
            /* compute md5. */
            if (option->md5) {
                compute_md5sum(NULL, 0, cur_md5sum);
                if (md5_saved && (memcmp(last_md5sum, cur_md5sum, sizeof(cur_md5sum)) != 0)) {
                    LOG_ERROR("*********[APP] loop %d, md5 check <<<<failed>>>>, ", loop);
                    print_md5(cur_md5sum, MD5_HASH_LEN);
#if 0
                    if (option->save) {
                        fclose(output_file);
                    }
                    exit(-1);
#endif
                } else {
                    LOG_INFO("*********[APP] loop %d, md5 check PASS!, ", loop);
                    print_md5(cur_md5sum, MD5_HASH_LEN);
                }
                memcpy(last_md5sum, cur_md5sum, sizeof(cur_md5sum));
                md5_saved = 1;
            }
            break;
        } else if (ret == vmpp_RSLT_WARN_MORE_DATA) {
#ifdef ENABLE_LATENCY_TEST
            ++more_num;
#endif
            // LOG_INFO("receive more data.");
            usleep(1000);
            //  break;
        } else {
            LOG_ERROR("receive frame error %d.", ret);
            break;
        }
    }
    if (option->save) {
        fclose(output_file);
    }

    if (frame_buffer)
        free(frame_buffer);

    return NULL;
}

static int get_channels_load(int width, int height, double fps)
{
    int quality = 100;
    int normalization = 38281846;
    int weight = width * height * fps * quality / normalization;
    return weight;
}

int get_stream_info(stream_context_ptr ctx, vmppStream *stream, vmppCodecType codecType, uint32_t *width, uint32_t *height)
{
    vmppDecVideoInfo videoInfo = {0};
    vmppResult ret = vmppDecGetVideoInfo(stream, codecType, &videoInfo);
    if (ret == vmpp_RSLT_OK) {
        if (ctx->ivf_headers_read) {
            videoInfo.fps.numerator = ctx->ivf_header.rate;
            videoInfo.fps.denominator = ctx->ivf_header.scale;
            videoInfo.width = ctx->ivf_header.width;
            videoInfo.height = ctx->ivf_header.height;
        }
        *width = videoInfo.width;
        *height = videoInfo.height;
        LOG_INFO(
            "video info(size %d x %d, cropFlag %d, cw %d, ch %d, xoffset %d, yoffset %d, fps_n "
            "%d, fps_d %d, pf %d, buf_num %d, reoder_num %d).\n",
            videoInfo.width, videoInfo.height, videoInfo.cropFlag, videoInfo.cropWidth,
            videoInfo.cropHeight, videoInfo.xOffset, videoInfo.yOffset, videoInfo.fps.numerator,
            videoInfo.fps.denominator, videoInfo.pixelFormat, videoInfo.requiredBufNum, videoInfo.reorderNum);
        return 0;
    }
    return -1;
}

/** return  0 codec is not consistent with input file or unknown type
           -1 input file is null 
            1 input file is support format and codec is valid **/
int check_input_type(char* input) {
    if (input == NULL) return -1;
    char* suffix = NULL;
    suffix = strrchr(option->input,'.');
    if (suffix != NULL && (!strcmp(suffix, ".h264") || !strcmp(suffix, ".264"))) {
       if (option->codec == NULL) {
            option->codec = "h264";
        } else {
            if (!strcmp(option->codec,"h264")) {
                return 1;    
            } else {
                LOG_WARN("codec is not consistent with input file");
                return 0;
            }
       }
    } else if (suffix != NULL && (!strcmp(suffix, ".hevc") || !strcmp(suffix, ".h265") || !strcmp(suffix, ".265"))) {
        if (option->codec == NULL) {
            option->codec = "hevc";
        } else {
            if (!strcmp(option->codec,"hevc")) {
                return 1;    
            } else {
                LOG_WARN("codec is not consistent with input file");
                return 0;
            }
       }
    } else if (suffix != NULL && (!strcmp(suffix, ".av1") || !strcmp(suffix, ".obu") || !strcmp(suffix, ".ivf")  || !strcmp(suffix, ".avif"))) {
        if (option->codec == NULL) {
            option->codec = "av1";
        } else {
            if (!strcmp(option->codec,"av1")) {
                return 1;    
            } else {
                if (!strcmp(option->codec,"vp9") && !strcmp(suffix, ".ivf"))
                    return 1;
                LOG_WARN("codec is not consistent with input file");
                return 0;
            }
       }
    } else if (suffix != NULL && !strcmp(suffix, ".avs")) {
        if (option->codec == NULL) {
            option->codec = "avs2";
        } else {
            if (!strcmp(option->codec,"avs2")) {
                return 1;
            } else {
                LOG_WARN("codec is not consistent with input file");
                return 0;
            }
       }
    } else {
        if (option->codec != NULL && (!strcmp(option->codec, "h264") || !strcmp(option->codec, "hevc")
           || !strcmp(option->codec, "av1") || !strcmp(option->codec, "vp9") || !strcmp(option->codec, "avs2"))) {
            return 1;
        } else {
            option->codec = "h264";
            LOG_WARN("No support type is specified,codec default is h264");
            return 0;
        }
    }
    return 1;
}

#if 0
void  logCallback(const void *pUser, int level, const char *module, const char *file, const char *func, int line, const char *msg)
{
    printf("LOG[%s] pUser:%p, level:%d, file:%s, func:%s, line:%d, msg:%s\n", module, pUser, level, file, func, line, msg);
}
#endif

#if 0
int vbuf[1024 * 1024 * 1];
#endif

vmppResult handle_output_serial(vmppChannel dec_ch, vmppFrame *out_frame, vmppDecOutputOptions *out_opt,
    FILE **output_file, char *output_file_str, uint32_t *out_count)
{
    vmppResult ret = 0;

    *out_count = *out_count + 1;

    if (option->save && !end_decoding) {
        if (*output_file == NULL) {
            get_output_file(output_file_str);
            *output_file = fopen(output_file_str, "wb");
        }
        fwrite(out_frame->data[0], 1, out_frame->dataSize, *output_file);
        LOG_INFO("[APP][%p][en %d]write YUV done! width: %d, height: %d", dec_ch,
                    out_opt->enableCrop, out_frame->width, out_frame->height);
    }

    /* check md5. */
    if (option->md5 && out_frame->memoryType == vmpp_MEM_HOST) {
        if (!end_decoding)
            compute_md5sum((const unsigned char *)out_frame->data[0], out_frame->dataSize,
                        cur_md5sum);
        #ifdef PRINT_SINGLE_MD5
            printf("frame %2d: ", *out_count);
            print_md5(cur_md5sum, MD5_HASH_LEN);
        #endif
    }

    if (option->vframes > 0 && *out_count >= option->vframes) {
        end_decoding = 1;
    }

    ret = vmppDecReleaseFrame(dec_ch, out_frame, 500);

    return ret;
}

void MainTask(option_t *option) {
    uint32_t stream_len;
    uint8_t *stream_p;
    stream_context_ptr strmctx = NULL;
#ifdef ENABLE_INPUT_THREAD
    input_t *input = NULL;
    buf_t *buf = NULL;
    task_handle input_task = NULL;
#else
    int32_t offset = 0;
#endif
    vmppChannel dec_ch;
    vmppDecChannelParameters ch_apr = {0};
    vmppStream input_stream;
    task_handle task = NULL;
    vmppResult ret;
    uint32_t in_count = 0;

    // defines for serial mode
    uint32_t out_count = 0;
    vmppFrame out_frame;
    vmppDecOutputOptions out_opt;
    FILE *output_file = NULL;
    char output_file_str[MAX_PATH_LEN] = { 0 };

    uint64_t pts_cnt = 0;  
    int strmtype = BIT_STREAM_JPEG;
    int32_t tmp_ret = 0;

    end_decoding = 0;
    repeat_time = 0;

    ch_apr.decDevice = option->decDevice;
    ch_apr.memDevice = option->memDevice;

    if (!strcmp(option->codec, "hevc")) {
        ch_apr.codecType = vmpp_CODEC_DEC_HEVC;
        strmtype = BIT_STREAM_HEVC;
    } else if (!strcmp(option->codec, "h264")) {
        ch_apr.codecType = vmpp_CODEC_DEC_H264;
        strmtype = BIT_STREAM_H264;
    } else if (!strcmp(option->codec, "av1")) {
        ch_apr.codecType = vmpp_CODEC_DEC_AV1;
        strmtype = BIT_STREAM_AV1;
    } else if (!strcmp(option->codec, "vp9")) {
        ch_apr.codecType = vmpp_CODEC_DEC_VP9;
        strmtype = BIT_STREAM_VP9;
    } else if (!strcmp(option->codec, "avs2")) {
        ch_apr.codecType = vmpp_CODEC_DEC_AVS2;
        strmtype = BIT_STREAM_AVS2;
    } else {
        LOG_ERROR("codec is an invalid value");
        goto handle_error;
    }
    strmctx = stream_open(option->input, strmtype);
    if (!strmctx) {
        LOG_ERROR("[transcode] Unable to open input file <%s>", option->input);
        goto handle_error;
    }
    if (strmctx->type == BIT_STREAM_VP9) {
        ch_apr.codecType = vmpp_CODEC_DEC_VP9;
    }

    vmppConfiguration cfg;
    memset(&cfg, 0, sizeof(vmppConfiguration));

    cfg.logCtx.enableCustomLog = 1;
    cfg.logCtx.logCallback = NULL;
    cfg.logCtx.logLevel = option->log_level;
    cfg.logCtx.usrParameters = NULL;

    ret = vmppInitDecoder(&cfg);
    if (ret != vmpp_RSLT_OK) {
        LOG_ERROR("vmppInitDecoder failed %d", ret);
        goto handle_error;
    }
    ch_apr.extraBufferNumber = EXT_OUTPUT_BUF_NUM;

#if 1 // these parameter are reserved.
    ch_apr.sourceMode = vmpp_SRC_FRAME;
    ch_apr.decodeMode = option->decode_mode;
    ch_apr.maxWidth = MAX_VIDEO_DEC_WIDTH;
    ch_apr.maxHeight = MAX_VIDEO_DEC_HEIGHT;
    ch_apr.streamBufferSize = MAX_STREAM_SIZE;
    ch_apr.pixelFormat = vmpp_PIX_FMT_NV12;

    // 10-bit support
    if (option->pix_fmt != NULL && strcmp(option->pix_fmt, "P010") == 0) {
        ch_apr.pixelFormat = vmpp_PIX_FMT_YUV420_PLANAR_10BIT_P010;
    }
    if (option->pix_fmt != NULL && strcmp(option->pix_fmt, "I010") == 0) {
        ch_apr.pixelFormat = vmpp_PIX_FMT_YUV420_PLANAR_10BIT_I010;
    }
#endif

    ch_apr.enProfiling = 1;
    ch_apr.coreMode = option->core_mode;
    ch_apr.memoryMode = option->memory_mode;
    ch_apr.outputAlign = option->output_align;
    ch_apr.apiMode = option->apiMode;

    ch_apr.enSEIParser = 1;
    ch_apr.noOutputReordering = option->no_output_reordering;
    ch_apr.bufSlimMode = option->buf_slim_mode;

    uint32_t w, h;
    while (1) {
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

        input_stream.stream = stream_p;
        input_stream.len = stream_len;
        input_stream.pts = 0;
        if (get_stream_info(strmctx, &input_stream, ch_apr.codecType, &w, &h) == 0)
            break;
    }

    if (option->crop) {
        ch_apr.cropInfo.flag = vmpp_CROP_ENABLE;
        if (option->crop == 2) {
            /*customized crop*/
            ch_apr.cropInfo.flag = vmpp_CROP_CUSTOMIZED;
            if (option->crop_detail) {
                sscanf(option->crop_detail, "x=%d,y=%d,w=%d,h=%d", &ch_apr.cropInfo.xOffset, &ch_apr.cropInfo.yOffset,
                       &ch_apr.cropInfo.width, &ch_apr.cropInfo.height);
            } else {
                ch_apr.cropInfo.xOffset = (w / 4 + 1) & (~1);
                ch_apr.cropInfo.yOffset = (h / 4 + 1) & (~1);
                ch_apr.cropInfo.width = (w / 2 + 1) & (~1);
                ch_apr.cropInfo.height = (h / 2 + 1) & (~1);
            }
        }
    }

    stream_seek_to_start(strmctx);

    ret = vmppDecCreateChannel(&dec_ch, &ch_apr);
    if (ret != vmpp_RSLT_OK || !dec_ch) {
        LOG_ERROR("create channel error %d or chn is null.", ret);
        goto handle_error;
    }

#ifdef ENABLE_INPUT_THREAD
    input = malloc(sizeof(input_t));

    queue_t *queue = init_buffer_queue(MAX_STREAM_SIZE);
    input->queue = queue;
    input->strmctx = strmctx;
    input_task = run_task(read_file_thread, input);
#endif

    ret = vmppDecStart(dec_ch); // set start status.
    if (ret < 0) {
        LOG_ERROR("start recv stream error %d.", ret);
        goto handle_error;
    }

    if (!option->apiMode) {
        task = run_task(output_thread, dec_ch);
    }

#ifdef ENABLE_INPUT_THREAD
    int count = 0;
#ifdef DROP_TEST
    FILE *fd_drop = fopen("test_drop.h264", "wr");
#endif
    while (1) {
        buf = read_buffer(queue);
        if (buf) {
            LOG_DEBUG("read buffer %p, index %d, length %d.", buf, buf->index, buf->length);
        }
        if (!buf || !buf->length) {
            if (has_read_file) {
                break;
            }
            sched_yield();
            continue;
        }
        count++;

        if (!has_begin) {
            gettimeofday(&tBegin, NULL);
            has_begin = 1;
        }

#ifdef DROP_TEST
        if (count % 100 == 99 && count < 300) {
            LOG_ERROR("drop(%d)!!!!", count);
        } else
#endif
        {
#ifdef STREAM_ERROR_TEST
            if (count % 100 == 10) {
                LOG_ERROR("make stream ERROR(%d)!!!!", count);
                stream_len = 10;
            }
#endif
            input_stream.stream = buf->buffer;
            input_stream.len = buf->length;
            input_stream.pts = pts_cnt++;
            ret = vmppDecSendStream(dec_ch, &input_stream, SEND_STREAM_TIMEOUT_VALUE);
            move_to_next_read_buffer(queue);
#ifdef DROP_TEST
            fwrite(stream_p, 1, stream_len, fd_drop);
#endif
            if (ret < 0) {
                LOG_INFO("va_vdec_send_stream(%d), error: %d", in_count, ret);
#ifdef RETURN_ON_STREAM_ERROR
                break;
#endif
            } else {
                LOG_INFO("va_vdec_send_stream(%d) succeed.", in_count);
            }

            in_count++;
        }
    }
    free(input);
    free_buffer_queue(queue);
#ifdef DROP_TEST
    fclose(fd_drop);
#endif

#else
    int count = 0;
#ifdef DROP_TEST
    FILE *fd_drop = fopen("test_drop.h264", "wr");
#endif
    while (!end_decoding) {
        tmp_ret = stream_read_frame(strmctx, &stream_p);
        if (tmp_ret <= 0) {
            if (tmp_ret == 0 && stream_eof(strmctx)) {
                LOG_INFO("\n>>>>>>>stream end the %dth times<<<<<<<<<<<<<<<<<", repeat_time);
                if (repeat_time >= option->stream_repeat_times) {
                    break;
                } else {
                    stream_seek_to_start(strmctx);
                    repeat_time++;
                    continue;
                }
            }
            LOG_ERROR("stream_read_frame failed, tmp_ret:%d",tmp_ret);
            break;
        }
         
        stream_len = (uint32_t)tmp_ret;

        offset += stream_len;
        count++;

        if (!has_begin) {
            gettimeofday(&tBegin, NULL);
            has_begin = 1;
        }

#ifdef DROP_TEST
        if (count % 100 == 99 && count < 300) {
            LOG_ERROR("drop(%d)!!!!", count);
        } else
#endif
        {
#ifdef STREAM_ERROR_TEST
            if (count % 100 == 10) {
                LOG_ERROR("make stream ERROR(%d)!!!!", count);
                stream_len = 10;
            }
#endif
            input_stream.stream = stream_p;
            input_stream.len = stream_len;
            input_stream.pts = pts_cnt++;
            // fake device address for vmpp_DEC_MEM_USER_AS_HWOUT test
            if (option->memory_mode == vmpp_DEC_MEM_USER_AS_HWOUT) {
                input_stream.outputBusAddress[0] = 0x900000000;
                input_stream.outputBusAddress[1] = 0x900300000;
                input_stream.outputBusAddress[2] = 0;
            }
#ifdef ENABLE_LATENCY_TEST
            sem_wait(&sem_w);
            gettimeofday(&st, NULL);
#endif
            ret = vmppDecSendStream(dec_ch, &input_stream, SEND_STREAM_TIMEOUT_VALUE);
#ifdef ENABLE_LATENCY_TEST
            sem_post(&sem_r);
#endif

#ifdef DPB_IDLE_COUNT_OUTPUT
            int32_t idle_count = 0;
            idle_count = vmppDecGetIdleDpbBufferCount(dec_ch);
            LOG_INFO("idle dpb buffer count :%d", idle_count);
#endif

#ifdef DROP_TEST
            fwrite(stream_p, 1, stream_len, fd_drop);
#endif
            if (ret < 0) {
                LOG_ERROR("vmppDecSendStream(%d), len: %d, error: %d", in_count, input_stream.len, ret);
#ifdef RETURN_ON_STREAM_ERROR
                break;
#endif
            } else {
                LOG_INFO("vmppDecSendStream(%d), len: %d, ret: %d", in_count, input_stream.len, ret);

                if (ret == vmpp_RSLT_DEC_INPUT_AGAIN) {
                    do {
                        if (option->apiMode == 1) {
                            // flush all old buffers
                            while(1) {
                                ret = vmppDecReceiveFrame(dec_ch, &out_frame, &out_opt, 500);
                                LOG_INFO("vmppDecReceiveFrame(%d), ret: %d", out_count, ret);
                                if (ret == vmpp_RSLT_OK) {
                                    handle_output_serial(dec_ch, &out_frame, &out_opt, &output_file, output_file_str, &out_count);
                                } else {
                                    break;
                                }
                            }
                        } else {
                            usleep(5000);
                        }
                        // send again
                        ret = vmppDecSendStream(dec_ch, &input_stream, SEND_STREAM_TIMEOUT_VALUE);
                        if (ret < 0) {
                            LOG_ERROR("vmppDecSendStream(%d), len: %d, error: %d", in_count, input_stream.len, ret);
                            break;
                        } else {
                            LOG_INFO("vmppDecSendStream(%d), len: %d, ret: %d", in_count, input_stream.len, ret);
                        }
                    }while(ret == vmpp_RSLT_DEC_INPUT_AGAIN);
                }
            }

            in_count++;

            if (option->apiMode == 1) {
                out_opt.enableCrop = 0;
                out_opt.memoryType = vmpp_MEM_HOST;

                while (1) {
                    ret = vmppDecReceiveFrame(dec_ch, &out_frame, &out_opt, 500);
                    LOG_INFO("vmppDecReceiveFrame(%d), ret: %d", out_count, ret);
                    if (ret == vmpp_RSLT_OK) {
                        handle_output_serial(dec_ch, &out_frame, &out_opt, &output_file, output_file_str, &out_count);
                    } else if (ret == vmpp_RSLT_WARN_MORE_DATA) {
                        break;
                    } else {
                        // TODO: error handling?
                    }
                }
            }
        }
    }
#ifdef ENABLE_LATENCY_TEST
    for(uint32_t i=0; i<=more_num+1; i++) {
        // sem_wait(&sem_w);
        // ++s;
        sem_post(&sem_r);
        // printf("==================send %d\n", s);
    }
#endif
#ifdef DROP_TEST
    fclose(fd_drop);
#endif
#endif // ENABLE_INPUT_THREAD

    ret = vmppDecStop(dec_ch); // disable start status.
    if (ret < 0) {
        LOG_ERROR("stop recv stream error %d.", ret);
        goto handle_error;
    }


    if (option->apiMode == 1) {
        while(1) {
            ret = vmppDecReceiveFrame(dec_ch, &out_frame, &out_opt, 500);
            LOG_INFO("vmppDecReceiveFrame(%d), ret: %d", out_count, ret);
            if (ret == vmpp_RSLT_OK) {
                handle_output_serial(dec_ch, &out_frame, &out_opt, &output_file, output_file_str, &out_count);
            }
            else if (ret == vmpp_RSLT_WARN_EOS) {
                /* compute md5. */
                if (option->md5) {
                    compute_md5sum(NULL, 0, cur_md5sum);
                    if (md5_saved && (memcmp(last_md5sum, cur_md5sum, sizeof(cur_md5sum)) != 0)) {
                        LOG_ERROR("*********[APP] loop %d, md5 check <<<<failed>>>>, ", loop);
                        print_md5(cur_md5sum, MD5_HASH_LEN);
                    #if 0
                        if (option->save) {
                            fclose(output_file);
                        }
                        exit(-1);
                    #endif
                    } else {
                        LOG_INFO("*********[APP] loop %d, md5 check PASS!, ", loop);
                        print_md5(cur_md5sum, MD5_HASH_LEN);
                    }
                    memcpy(last_md5sum, cur_md5sum, sizeof(cur_md5sum));
                    md5_saved = 1;
                }
                break;
            }
            else {
                // TODO: error handling?
            }
        }
    } else {
        pthread_join(*((pthread_t *)task), NULL);
    }

    ret = vmppDecDestroyChannel(&dec_ch);
    if (ret < 0) {
        LOG_ERROR("destroy chn error %d.", ret);
        goto handle_error;
    }

handle_error:
    if (task != NULL)
        free(task);

#ifdef ENABLE_INPUT_THREAD
    if (input_task != NULL) {
        free(input_task);
    }
#endif

    stream_close(&strmctx);
}

RET_TYPE main(int argc, char *argv[])
{
    option_t *g_option = NULL;
    int32_t ret = 0;

    setLogLevel(LOG_LEVEL_INFO);

    g_option = malloc(sizeof(option_t));
    option = g_option;
    memset(option, 0, sizeof(*option));
    option->save = 1;
    option->loop = 1;
    option->md5 = 1;
    option->decDevice = "/dev/hantrodec";
    option->memDevice = "/dev/memalloc";
    //option->codec = "h264";
    option->output = "output.yuv";
    option->core_mode = 0;
    option->memory_mode = 0;
    option->output_align = 0;
    option->log_level = 2; //INFO
    option->buf_slim_mode = 0;
    option->stream_repeat_times = 0;

#ifdef ENABLE_LATENCY_TEST
    if(sem_init(&sem_r,0,0) == -1) {//初始化信号量
        perror("semr_init");
        return -1;
    }
    if(sem_init(&sem_w,0,1) == -1) {//初始化信号量
        perror("semw_init");
        return -1;
    }
#endif
    ret = parse_options(argc, argv, option);
    if (ret < 0) {
        usage(argv[0]);
        if (ret != -2)
            LOG_ERROR("Failed to parse the input arguments!");
        return -1;
    }
    ret = check_input_type(option->input);
    if (ret < 0) {
        LOG_ERROR("Please specify the input H264 or Hevc file!");
        usage(argv[0]);
        return -1;
    }

#ifdef ENABLE_LATENCY_TEST
    if(latency_log == NULL){
        printf("open latency file error or need to input the file with -F !");
        return -1;
    }
#endif
    LOG_INFO("render video device name: %s", option->decDevice);
    LOG_INFO("render memory device name: %s", option->memDevice);
    LOG_INFO("perf_count: %d", option->loop);
    LOG_INFO("md5_check: %d", option->md5);
    LOG_INFO("save_yuv: %d", option->save);
    LOG_INFO("output file: %s", option->output);
    LOG_INFO("output align: %d", option->output_align);
    LOG_INFO("core_mode: %d", option->core_mode);
    LOG_INFO("memory_mode: %d", option->memory_mode);

    md5ctx_inited = 0;
    memset(&md5ctx, 0, sizeof(struct md5_context));

    gettimeofday(&tGlobalBegin, NULL);

    while ((loop++ < option->loop) || (option->loop == 0)) {
        LOG_INFO("\n>>>>>>>To decode the %dth times<<<<<<<<<<<<<<<<<", loop);
        MainTask(option);
        has_begin = 0;
    }
#ifdef ENABLE_LATENCY_TEST
    if(latency_log) {
        fclose(latency_log);
        latency_log = NULL;
    }
#endif
    free(option);
    return 0;
}
