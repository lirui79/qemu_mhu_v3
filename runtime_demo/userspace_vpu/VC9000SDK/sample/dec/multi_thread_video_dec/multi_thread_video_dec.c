#include <assert.h>
#include <ctype.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "cJSON.h"

#include "queue.h"
#include "utils.h"
#include "stream.h"
#include "vmpp_dec_api.h"
#include "vmpp_dec_defs.h"

#define MAX_VIDEO_DEC_WIDTH 4096
#define MAX_VIDEO_DEC_HEIGHT 4096
#define SEND_STREAM_TIMEOUT_VALUE (0XFFFFFF)
#define MAX_THREAD_NUM (24)

#define MAX_STREAM_SIZE (16 * 1024 * 1024)
#define MAX_OUT_SIZE (24 * 1024 * 1024)
typedef int32_t RET_TYPE;
typedef void *task_handle;
typedef void *(*task_func)(void *);
int32_t need_save_file = 0;
int32_t loop = 0;

typedef struct {
    int argc;
    char **argv;
} MainArgs;

typedef struct {
    char*               decDevice;          // video decoder device node name
    char*               memDevice;          // memory device node name
    int index;
    char *input_file;
    char *output_file;
    char *codec;
    int memory_mode;
    int output_align;
} source_t;

typedef struct {
    char*               decDevice;          // video decoder device node name
    char*               memDevice;          // memory device node name
    char *input;
    char *output;    //ouput folder
    char *codec;
    int save;
    int loop;
    int thread_count;
    int memory_mode;
    int output_align;
    int stream_repeat_times;
} option_t;

struct thread_param_t {
    void *dec_ch;
    source_t *source;
};

option_t option = {0};

static int get_channels_load(int width, int height, double fps)
{
    int quality = 100;
    int normalization = 38281846;
    int weight = width * height * fps * quality / normalization;
    return weight;
}

/* The strings returned by cJSON are owned by the cJSON tree and are released by
   cJSON_Delete(). The sample keeps its own copy of them, otherwise releasing a
   source_t would free cJSON memory and cJSON_Delete() would free it again. */
static char *dup_json_string(cJSON *object, const char *key)
{
    cJSON *item = cJSON_GetObjectItem(object, key);

    if (item == NULL || !cJSON_IsString(item) || item->valuestring == NULL)
        return NULL;

    return strdup(item->valuestring);
}

static void parse_source_json(const char *file, struct vmpp_queue *queue, cJSON **proot)
{
    FILE *fp_json_file = NULL;
    char json_buf[102400];
    char *dec_device = NULL;
    char *mem_device = NULL;
    cJSON *root, *arrayItem, *item, *object;
    int ret;

    fp_json_file = fopen(file, "r");
    if (fp_json_file == NULL) {
        fprintf(stderr, "Open source json file %s error.\n", file);
        return;
    }

    ret = fread(json_buf, 1, sizeof(json_buf), fp_json_file);
    if (ret <= 0) {
        LOG_ERROR("Read source json file %s error.", file);
        fclose(fp_json_file);
        return;
    }
    root = cJSON_Parse(json_buf);
    if (!root) {
        fprintf(stderr, "Error before: [%s]\n", cJSON_GetErrorPtr());
        fclose(fp_json_file);
        return;
    }

    item = cJSON_GetObjectItem(root, "decDevice");
    if (item != NULL && cJSON_IsString(item)) {
        dec_device = strdup(item->valuestring);
    }
    item = cJSON_GetObjectItem(root, "memDevice");
    if (item != NULL && cJSON_IsString(item)) {
        mem_device = strdup(item->valuestring);
    }

    arrayItem = cJSON_GetObjectItem(root, "sources");
    if (arrayItem != NULL) {
        int32_t size = cJSON_GetArraySize(arrayItem);
        for (int32_t index = 0; index < size; index++) {
            source_t *a_src = (source_t *)malloc(sizeof(source_t));
            if (!a_src) {
                fprintf(stderr, "malloc a_source failed!\n");
                break;
            }
            memset(a_src, 0, sizeof(*a_src));
            a_src->decDevice = dec_device ? strdup(dec_device) : NULL;
            a_src->memDevice = mem_device ? strdup(mem_device) : NULL;
            object = cJSON_GetArrayItem(arrayItem, index);
            item = cJSON_GetObjectItem(object, "index");
            if (item != NULL) {
                a_src->index = item->valueint;
            }
            a_src->input_file = dup_json_string(object, "input");
            a_src->output_file = dup_json_string(object, "output");
            a_src->codec = dup_json_string(object, "codec");
            item = cJSON_GetObjectItem(object, "output_align");
            if (item != NULL)  {
                a_src->output_align = item->valueint;
            }
            vmpp_queue_push_back(queue, a_src);
        }
    }

    free(dec_device);
    free(mem_device);
    fclose(fp_json_file);
    *proot = root;
}

static void parse_source_option(option_t* option, struct vmpp_queue *queue)
{
    int32_t size = option->thread_count;
    for (int32_t index = 0; index < size; index++) {
        source_t *a_src = (source_t *)malloc(sizeof(source_t));
        char *out_file = (char*) malloc(1024);
        if (!a_src || !out_file) {
            fprintf(stderr, "malloc source failed!\n");
            free(a_src);
            free(out_file);
            return;
        }
        memset(a_src, 0, sizeof(*a_src));
        snprintf(out_file, 1024, "%s/output_%d.yuv", option->output, index);
        a_src->index = index;
        a_src->input_file = option->input;
        a_src->output_file = out_file;
        a_src->decDevice = option->decDevice;
        a_src->memDevice = option->memDevice;
        a_src->codec = option->codec;
        a_src->memory_mode = option->memory_mode;
        a_src->output_align = option->output_align;

        vmpp_queue_push_back(queue, a_src);
    }
}

static void print_source_list(struct vmpp_queue *queue)
{
    for (int32_t i = 0; i < vmpp_queue_size(queue); i++) {
        source_t *source = (source_t *)vmpp_queue_peek(queue, i);
        if (source) {
            fprintf(stdout, "source_list[%d]: input %s, output %s, video device %s, memory device %s, codec %s.\n", i,
                    source->input_file, source->output_file, source->decDevice, source->memDevice, source->codec);
        }
    }
}


static task_handle run_task(task_func func, void *param)
{
    int32_t ret;
    pthread_t *thread_handle = malloc(sizeof(pthread_t));

#ifndef ARM64
    pthread_attr_t attr;
    struct sched_param par;
    pthread_attr_init(&attr);
    ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    assert(ret == 0);
    //ret = pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
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

static void *output_thread(void *arg)
{
    vmppFrame out_frame;
    vmppDecOutputOptions out_opt;
    void *dec_ch;
    vmppResult ret;
    FILE *output_file = NULL;
    struct thread_param_t *thread_param = (struct thread_param_t *)arg;
    uint8_t *frame_buffer = NULL;
    struct timeval tBegin, tEnd;

    out_opt.memoryType = vmpp_MEM_HOST;
    if (option.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_HOST) {
        frame_buffer = malloc(MAX_VIDEO_DEC_WIDTH * MAX_VIDEO_DEC_HEIGHT * 3 / 2);
    } else if (option.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_DEV) {
        out_opt.memoryType = vmpp_MEM_DEVICE;
        option.save = 0; // force to not save to avoid crash issue
        need_save_file = 0;
    } else if (option.memory_mode == vmpp_DEC_MEM_LESS_DEV_MEM) {
        out_opt.memoryType = vmpp_MEM_HOST;
    } else {
        out_opt.memoryType = need_save_file ? vmpp_MEM_HOST : vmpp_MEM_DEVICE;
    }

    LOG_INFO("memoryType : %d", out_opt.memoryType);

    dec_ch = thread_param->dec_ch;

    if (need_save_file) {
        output_file = fopen(thread_param->source->output_file, "wb");
        if (!output_file)
            LOG_ERROR("Unable to open output file %s", thread_param->source->output_file);
    }

    out_opt.enableCrop = 0;

    uint32_t out_count = 0;

    gettimeofday(&tBegin, NULL);

    while (1) {
        if (option.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_HOST)
            out_frame.data[0] = frame_buffer;

        // fake device address, only for function instructions
        if (option.memory_mode == vmpp_DEC_MEM_USER_OUT_BUF_DEV)
            out_frame.busAddress[0] = 0x900000000;

        ret = vmppDecReceiveFrame(dec_ch, &out_frame, &out_opt, 500);
        if (ret == vmpp_RSLT_OK) {
            out_count++;
            if (need_save_file && out_frame.memoryType == vmpp_MEM_HOST && output_file) {
                if (out_opt.enableCrop == 1) {
                    printf("[APP][%p][en %d]write %d YUV to %s done! width: %d, height: %d\n",
                           dec_ch, out_opt.enableCrop, out_count, thread_param->source->output_file,
                           out_frame.width, out_frame.height);

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
                    printf("[APP][%p][en %d]write %d YUV to %s done! width: %d, height: %d\n",
                           dec_ch, out_opt.enableCrop, out_count, thread_param->source->output_file,
                           out_frame.width, out_frame.height);
                    // write Y
                    for (uint32_t j = 0; j < out_frame.cropInfo.height; j++) {
                        fwrite(out_frame.data[0] + out_frame.stride[0] * (j + out_frame.cropInfo.yOffset) +
                               out_frame.cropInfo.xOffset, 1, out_frame.cropInfo.width, output_file);
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
                            fwrite(out_frame.data[1] + out_frame.stride[1] * (j + out_frame.cropInfo.yOffset / 2) +
                                   out_frame.cropInfo.xOffset / 2, 1, uv_crop_width, output_file);
                        }
                    }
                }
            }

            ret = vmppDecReleaseFrame(dec_ch, &out_frame, 500);
            if (ret < 0) {
                fprintf(stderr, "release frame error %d.\n", ret);
            }

            if (out_count % 100 == 0) {
                gettimeofday(&tEnd, NULL);
                long deltaTime =
                    1000000L * (tEnd.tv_sec - tBegin.tv_sec) + (tEnd.tv_usec - tBegin.tv_usec);
                float frame_rate = (float)(out_count) / deltaTime * 1000000;
                LOG_INFO("[Perf] Decoded %d th file %s for the %dth time, total %d frames, spend "
                   "%ld us, frame rate %.2ffps.",
                   thread_param->source->index, thread_param->source->input_file, loop, out_count,
                   deltaTime, frame_rate);
            }
        } else if (ret == vmpp_RSLT_WARN_EOS) {
            gettimeofday(&tEnd, NULL);
            long deltaTime =
                1000000L * (tEnd.tv_sec - tBegin.tv_sec) + (tEnd.tv_usec - tBegin.tv_usec);
            float frame_rate = (float)(out_count) / deltaTime * 1000000;
            LOG_WARN("[Perf-FINAL] Decoded %dth file %s for the %dth time, total %d frames, spend "
                   "%ld us, frame rate %.2ffps.",
                   thread_param->source->index, thread_param->source->input_file, loop, out_count,
                   deltaTime, frame_rate);
            break;
        } else if (ret == vmpp_RSLT_WARN_MORE_DATA) {
            // fprintf(stdout, "receive more data.\n");
            usleep(1000);
            //  break;
        } else {
            fprintf(stderr, "receive frame error %d.\n", ret);
            break;
        }
    }
    if (need_save_file) {
        if (output_file)
            fclose(output_file);
        else
            LOG_ERROR("No frame was written: unable to create %s",
                      thread_param->source->output_file);
    }

    if (frame_buffer)
        free(frame_buffer);

    return NULL;
}


static void *MainTask(void *arg)
{
    uint32_t stream_len;
    uint8_t *stream_p;
    stream_context_ptr strmctx = NULL;
    int32_t offset = 0;
    vmppChannel dec_ch;
    vmppDecChannelParameters ch_apr = {0};
    vmppStream input_stream;
    task_handle task = NULL;
    vmppResult ret;
    struct thread_param_t thread_param;
    int repeat_time = 0;

    int strmtype = BIT_STREAM_JPEG;
    int32_t tmp_ret = 0;
    uint64_t pts_cnt = 0; 

    source_t *source = arg;

    fprintf(stdout, ">>>>>>>>To decode %s.\n", source->input_file);

    ch_apr.decDevice = source->decDevice;
    ch_apr.memDevice = source->memDevice;

    if (source->codec != NULL) {
        if (!strcmp(source->codec, "hevc")) {
            ch_apr.codecType = vmpp_CODEC_DEC_HEVC;
            strmtype = BIT_STREAM_HEVC;
        } else if (!strcmp(source->codec, "h264")) {
            ch_apr.codecType = vmpp_CODEC_DEC_H264;
            strmtype = BIT_STREAM_H264;
        } else if (!strcmp(source->codec, "av1")) {
            ch_apr.codecType = vmpp_CODEC_DEC_AV1;
            strmtype = BIT_STREAM_AV1;
        } else if (!strcmp(source->codec, "vp9")) {
            ch_apr.codecType = vmpp_CODEC_DEC_VP9;
            strmtype = BIT_STREAM_VP9;
        } else if (!strcmp(source->codec, "avs2")) {
            ch_apr.codecType = vmpp_CODEC_DEC_AVS2;
            strmtype = BIT_STREAM_AVS2;
        } else {
            LOG_ERROR("codec is an invalid value");
            return 0;
        }
    } else {
        LOG_ERROR("Please use -c option to specify a valid codec value!");
        return 0;
    }

    strmctx = stream_open(source->input_file, strmtype);
    if (!strmctx) {
        LOG_ERROR("[transcode] Unable to open input file <%s>", source->input_file);
        return 0;
    }
    if (strmctx->type == BIT_STREAM_VP9) {
        ch_apr.codecType = vmpp_CODEC_DEC_VP9;
    }

    vmppConfiguration cfg;
    memset(&cfg, 0, sizeof(vmppConfiguration));

    cfg.logCtx.enableCustomLog = 1;
    cfg.logCtx.logCallback = NULL;
    cfg.logCtx.logLevel = vmpp_LOG_WARN;
    cfg.logCtx.usrParameters = NULL;

    ret = vmppInitDecoder(&cfg);
    if (ret != vmpp_RSLT_OK) {
        printf("vmppInitDecoder failed %d\n", ret);
        return 0;
    }

    ch_apr.decDevice = source->decDevice;
    ch_apr.memDevice = source->memDevice;

    ch_apr.sourceMode = vmpp_SRC_FRAME;
    ch_apr.decodeMode = vmpp_DEC_NORMAL;
    ch_apr.maxWidth = 3840;
    ch_apr.maxHeight = 2160;
    ch_apr.extraBufferNumber = 4;
    ch_apr.streamBufferSize = MAX_STREAM_SIZE;
    ch_apr.pixelFormat = vmpp_PIX_FMT_NV12;

    ch_apr.enProfiling = 1;
    ch_apr.coreMode = vmpp_CORE_AUTO;
    ch_apr.memoryMode = source->memory_mode;
    ch_apr.outputAlign = source->output_align;

    ret = vmppDecCreateChannel(&dec_ch, &ch_apr);
    if (ret != vmpp_RSLT_OK || !dec_ch) {
        fprintf(stderr, "create channel error %d or chn is null.\n", ret);
        return 0;
    }

    ret = vmppDecStart(dec_ch); // set start status.
    if (ret < 0) {
        fprintf(stderr, "start recv stream error %d.\n", ret);
        return 0;
    }

    thread_param.dec_ch = dec_ch;
    thread_param.source = source;
    task = run_task(output_thread, &thread_param);

    int count = 0;
    while (1) {
        
       tmp_ret = stream_read_frame(strmctx,&stream_p);
        if (tmp_ret <= 0) {
            if (tmp_ret == 0 && stream_eof(strmctx)) {
                printf("\n>>>>>>>stream end the %dth times<<<<<<<<<<<<<<<<<\n", repeat_time);
                if (repeat_time >= option.stream_repeat_times) {
                    break;
                } else {
                    stream_seek_to_start(strmctx);
                    repeat_time++;
                    continue;
                }
                break;
            }
            LOG_ERROR("stream_read_frame failed, tmp_ret:%d",tmp_ret);
            break;
        } 
        stream_len = (uint32_t)tmp_ret;

        offset += stream_len;
        count++;
        if (stream_len == 0) {
            fprintf(stderr, "stream end.\n");
            break;
        }
        input_stream.stream = stream_p;
        input_stream.len = stream_len;
        input_stream.pts = pts_cnt++;

        ret = vmppDecSendStream(dec_ch, &input_stream, SEND_STREAM_TIMEOUT_VALUE);
        if (ret < 0) {
            fprintf(stderr, "send stream error %d.\n", ret);
            break;
        }
    }

    ret = vmppDecStop(dec_ch); // disable start status.
    if (ret < 0) {
        fprintf(stderr, "stop recv stream error %d.\n", ret);
        return 0;
    }

    pthread_join(*((pthread_t *)task), NULL);

    free(task);

    ret = vmppDecDestroyChannel(&dec_ch);
    if (ret < 0) {
        fprintf(stderr, "destroy chn error %d.\n", ret);
        return 0;
    }
    stream_close(&strmctx);

    return 0;
}

static void usage(const char *program)
{
    printf("Usage: %s -i <input_json> -o < output_file> -s <save_output> -d [video decoder device] -m [memory device] -l <loop> -T <thread_count> -u <userOutBuf> -a <outputAlignment> -r <streamRepeat>\n"
           "  input_json [M]: input json file.\n"
           "  output_file [M]: output yuv file.\n"
           "  save_output[M]: need save out put file.\n"
           "  video device[O]: render video decoder device name, default: /dev/hantrodec\n"
           "  memory device[O]: render memory device name, default: /dev/memalloc\n"
           "  loop[M]: loop times.\n"
           "  thread_count[M]: how many threads to be created [1, 24].\n"
           "  userOutBuf[M]: use user output buffer or not, default: 0.\n"
           "  outputAlignment[O]: output alignment, default 0.\n"
           "  streamRepeat[O]: decode stream repeat times in one channel, default: 0\n",
           program);
    printf("example: ./video_dec_mt -i /home/stone/workspace/akiyo_352x288_300_IBBBP.h264 -s 1 -o output -d /dev/hantrodec -m /dev/memalloc -l 1 -T 4 -c h264 \n");
    printf("  ./video_dec_mt -i /home/stone/workspace/sample_640x360.hevc -s 1 -o  output -d /dev/hantrodec -m /dev/memalloc -l 1 -T 4 -c hevc \n");
    printf("  ./video_dec_mt -i /home/stone/workspace/vpu_sdk/test/jsons/sources.json  -s 1 -o  output -d /dev/hantrodec -m /dev/memalloc -l 1 \n");
}

static int parse_options(int argc, char **argv, option_t *opt)
{
    // h264_dec_mt -i input.json -s 0
    static const char optstr[] = "i:s:o:d:m:l:T:c:M:a:r:";
    int c;
    while ((c = getopt(argc, argv, optstr)) != -1) {
        switch (c) {
        case 'i':
            opt->input = optarg;
            break;
        case 's':
            opt->save = atoi(optarg);
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
        case 'l':
            opt->loop = atoi(optarg);
            break;
        case 'T':
            opt->thread_count = atoi(optarg);
            if (opt->thread_count > MAX_THREAD_NUM || opt->thread_count < 1) {
                fprintf(stderr, "thread_count must be in [1, %d].\n", MAX_THREAD_NUM);
                return -1;
            }
            break;
        case 'c':
            opt->codec = optarg;
            break;
        case 'M':
            opt->memory_mode = atoi(optarg);
            break;
        case 'a':
            opt->output_align = atoi(optarg);
            break;
        case 'r':
            opt->stream_repeat_times = atoi(optarg);
            break;
        case '?':
            usage(argv[0]);
            return -1;
        default:
            fprintf(stderr, "\n Unsupported option: %c \n", c);
            usage(argv[0]);
            return -1;
        }
    }

    if (optind < argc) {
        usage(argv[0]);
        return -1;
    }

    return 0;
}


RET_TYPE main(int argc, char *argv[])
{
    struct vmpp_queue *task_queue = NULL;
    task_handle tasks[MAX_THREAD_NUM] = {0};
    int ret = 0;
    cJSON *root = NULL;

    memset(&option, 0, sizeof(option));
    option.loop = 1;
    option.thread_count = 1;
    option.decDevice = "/dev/hantrodec";
    option.memDevice = "/dev/memalloc";
    option.stream_repeat_times = 0;

    ret = parse_options(argc, argv, &option);
    if (ret < 0) {
        fprintf(stderr, "Failed to parse args.\n");
        usage(argv[0]);
        return -1;
    }

    if (option.input == NULL) {
        LOG_ERROR("Please specify the input file!");
        usage(argv[0]);
        return -1;
    }

    need_save_file = option.save;

    /* The output thread fopen()s "<dir>/output_N.yuv" without checking the
     * result, so make sure the directory exists before starting the threads.
     * Otherwise -s 1 silently decodes without writing a single YUV. */
    if (need_save_file && option.output) {
        struct stat st;
        if (stat(option.output, &st) != 0 && mkdir(option.output, 0777) != 0)
            LOG_ERROR("Unable to create output directory: %s", option.output);
    }

    vmpp_queue_init(&task_queue);

    size_t input_len = strlen(option.input);
    int json_source = (input_len >= 5) && (strcmp(option.input + input_len - 5, ".json") == 0);

    if (json_source) {
        /* parse json to get source list. */
        parse_source_json(option.input, task_queue, &root);
    } else {
        /* read option to get source list. */
        parse_source_option(&option, task_queue);
    }

    print_source_list(task_queue);

    source_t *source;
    while ((loop++ < option.loop) || (option.loop == 0)) {
        printf("================the %dth loop================\n", loop);
        for (int32_t i = 0; i < vmpp_queue_size(task_queue); i++) {
            source = (source_t *)vmpp_queue_peek(task_queue, i);
            if (source) {
                tasks[i] = run_task(MainTask, source);
            }
        }

        for (int32_t i = 0; i < vmpp_queue_size(task_queue); i++) {
            pthread_join(*((pthread_t *)tasks[i]), NULL);
            free(tasks[i]);
        }
    }

    for (int32_t i = 0; i < vmpp_queue_size(task_queue); i++) {
        source = (source_t *)vmpp_queue_peek(task_queue, i);
        if (!source)
            continue;
        if (source->output_file)
            free(source->output_file);
        /* Sources built from json own every string; sources built from the
           command line only own the generated output file name. */
        if (json_source) {
            free(source->input_file);
            free(source->codec);
            free(source->decDevice);
            free(source->memDevice);
        }
        free(source);
    }

    vmpp_queue_free(&task_queue);
    if (root) {
        cJSON_Delete(root);
    }
    return 0;
}
