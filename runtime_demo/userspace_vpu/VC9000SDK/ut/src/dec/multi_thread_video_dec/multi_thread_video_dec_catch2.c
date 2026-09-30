#include <assert.h>
#include <ctype.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#define LIB_VACCRT_PATH "libvaccrt.so"

#define MAX_STREAM_SIZE (16 * 1024 * 1024)
#define MAX_OUT_SIZE (24 * 1024 * 1024)
typedef int32_t RET_TYPE;
typedef void *task_handle;
typedef void *(*task_func)(void *);
int32_t need_save_file = 0;
static int32_t loop = 0, ret_end = 0;
int switch_drop = 0;
int switch_distort = 0;

typedef struct {
    int argc;
    char **argv;
} MainArgs;

typedef struct {
    int index;
    char *input_file;
    char *output_file;
    char *device;
    char *codec;
    int memory_mode;
    int output_align;
} source_t;

typedef struct {
    char *input;
    char *output;    //ouput folder
    char *device;
    char *codec;
    int save;
    int loop;
    int thread_count;
    int drop;
    int distort;
    int memory_mode;
    int output_align;
    int stream_repeat_times;
} option_t;

struct thread_param_t {
    void *dec_ch;
    source_t *source;
};

struct LoadBalanceBuffer {
    int channelNum;
    int loads;
    int channelLoad[200];
};

static struct LoadBalanceBuffer load_balance_buffer;
option_t option = {0};


static int get_channels_load(int width, int height, double fps)
{
    int quality = 100;
    int normalization = 38281846;
    int weight = width * height * fps * quality / normalization;
    return weight;
}

static int parse_source_json(const char *file, struct vmpp_queue *queue, cJSON **proot)
{
    FILE *fp_json_file = NULL;
    char json_buf[102400];
    cJSON *root, *arrayItem, *item, *object;
    int ret;

    fp_json_file = fopen(file, "r");
    if (fp_json_file == NULL) {
        fprintf(stderr, "Open source json file %s error.\n", file);
        return -1;
    }

    ret = fread(json_buf, 1, sizeof(json_buf), fp_json_file);
    if (ret <= 0) {
        LOG_ERROR("Read source json file %s error.", file);
        fclose(fp_json_file);
        return -1;
    }
    root = cJSON_Parse(json_buf);
    if (!root) {
        fprintf(stderr, "Error before: [%s]\n", cJSON_GetErrorPtr());
        fclose(fp_json_file);
        return -1;
    }

    arrayItem = cJSON_GetObjectItem(root, "sources");
    if (arrayItem != NULL) {
        int32_t size = cJSON_GetArraySize(arrayItem);
        for (int32_t index = 0; index < size; index++) {
            char *input_buf = (char*)malloc(1024*sizeof(char));
            char *output_buf = (char*)malloc(1024*sizeof(char));
            source_t *a_src = (source_t *)malloc(sizeof(source_t));
            if (!a_src) {
                fprintf(stderr, "malloc a_source failed!\n");
                free(a_src);
                return -1;
            }
	        memset(a_src, 0, sizeof(source_t));
            object = cJSON_GetArrayItem(arrayItem, index);
            item = cJSON_GetObjectItem(object, "index");
            {
                a_src->index = item->valueint;
            }
            item = cJSON_GetObjectItem(object, "input");
            if (item != NULL) {
                sprintf(input_buf, "%s%s", UT_RES_PATH, item->valuestring);
                a_src->input_file = input_buf;
            }
            item = cJSON_GetObjectItem(object, "output");
            if (item != NULL) {
                sprintf(output_buf, "%s%s", UT_RES_OUT, item->valuestring);
                a_src->output_file = output_buf;
            }
            item = cJSON_GetObjectItem(object, "device");
            if (item != NULL) {
                a_src->device = item->valuestring;
            }
            item = cJSON_GetObjectItem(object, "codec");
            if (item != NULL) {
                a_src->codec = item->valuestring;
            }
            item = cJSON_GetObjectItem(object, "output_align");
            {
                a_src->output_align = item->valueint;
            }
            vmpp_queue_push_back(queue, a_src);
        }
    }
    fclose(fp_json_file);
    *proot = root;
    return 0;
}

static void parse_source_option(option_t* option, struct vmpp_queue *queue)
{
    int32_t size = option->thread_count;
    for (int32_t index = 0; index < size; index++) {
        source_t *a_src = (source_t *)malloc(sizeof(source_t));
        char *out_file = (char*) malloc(1024);
        sprintf(out_file, "%s/output_%d.yuv", option->output, index);
        a_src->index = index;
        a_src->input_file = option->input;
        a_src->output_file = out_file;
        a_src->device = option->device;
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
            fprintf(stdout, "source_list[%d]: input %s, output %s, device %s, codec %s.\n", i,
                    source->input_file, source->output_file, source->device, source->codec);
        }
    }
}


static task_handle run_task(task_func func, void *param)
{
    int32_t ret;
    pthread_t *thread_handle = (pthread_t *)malloc(sizeof(pthread_t));

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
        frame_buffer = (uint8_t *)malloc(MAX_VIDEO_DEC_WIDTH * MAX_VIDEO_DEC_HEIGHT * 3 / 2);
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
    if (need_save_file && output_file) {
        fclose(output_file);
        output_file = NULL;
    }

    if (frame_buffer)
        free(frame_buffer);

    return NULL;
}


typedef rtError_t (*vaccrt_init_t)(uint32_t dev_id);

static int parse_infilename(const char* infile_name, char infile_arr[16][200]) {
    char* cp = NULL;
    char* p = (char*)infile_name;
    int infile_num = 0;
    do {
        cp = strstr(p, ",");
        if (NULL == cp) {
            strcpy(infile_arr[infile_num], p);
            infile_num++;
            break;
        }
        strncpy(infile_arr[infile_num], p, cp - p);
        infile_arr[infile_num][cp - p] = '\0';
        // strncpy(infile_array[infile_num] + (cp - infile_path), "\0", 1);

        p = cp + 1;
        while (*p == ' ')
            p++;
        infile_num++;
    } while (cp != NULL);
    return infile_num;
}

static void *MainTask(void *arg)
{
    uint32_t stream_len;
    uint8_t *stream_p;
    stream_context_ptr strmctx = NULL;
    int32_t offset = 0;
    vmppChannel dec_ch;
    int32_t dec_fd, infile_num, file_index = 0;
    vmppDecChannelParameters ch_apr = {0};
    vmppStream input_stream;
    task_handle task = NULL;
    vmppRuntimeInstance rt_inst;
    vmppResult ret;
    uint32_t pos;
    uint32_t random;
    struct thread_param_t thread_param;
    int repeat_time = 0;

    int strmtype = BIT_STREAM_JPEG;
    int32_t tmp_ret = 0;
    uint64_t pts_cnt = 0; 
    char infile_array[16][200];

    source_t *source = (source_t *)arg;

    fprintf(stdout, ">>>>>>>>To decode %s.\n", source->input_file);

    open_runtime(&rt_inst); // run time init

    dec_fd = open(source->device, O_RDWR);
    if (dec_fd < 0) {
        printf("Cannot open DRM render node for device %s\n", source->device);
        ret_end = -1;
        return 0;
    }

    FILE *f_in = NULL;
    infile_num = parse_infilename(source->input_file, infile_array);
    for (file_index = 0; file_index < infile_num; file_index++) {

        ch_apr.device = dec_fd;
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
                ret_end = -1;
                return 0;
            }
        } else {
            LOG_ERROR("Please use -c option to specify a valid codec value!");
            ret_end = -1;
            return 0;
        }

        strmctx = stream_open(infile_array[file_index], strmtype);
        if (!strmctx) {
            LOG_ERROR("[transcode] Unable to open input file <%s>", source->input_file);
            ret_end = -1;
            return 0;
        }
        if (strmctx->type == BIT_STREAM_VP9) {
            ch_apr.codecType = vmpp_CODEC_DEC_VP9;
        }

        int dieId = 0;
        vaccrt_init_t vaccrt_init = (vaccrt_init_t)(&rt_inst)->init;

        sscanf(source->device, "/dev/vastai_video%d", &dieId);
        rtError_t vaccRet = vaccrt_init(dieId);
        if (vaccRet) {
            fprintf(stderr, "vaccrt_init failed: err %d\n", vaccRet);
            ret_end = -1;
            return 0;
        }

        vmppConfiguration cfg;
        memset(&cfg, 0, sizeof(vmppConfiguration));
        cfg.runtimeInst = rt_inst;

    cfg.logCtx.enableCustomLog = 1;
    cfg.logCtx.logCallback = NULL;
    cfg.logCtx.logLevel = vmpp_LOG_WARN;
    cfg.logCtx.usrParameters = NULL;

        ret = vmppInitDecoder(&cfg);
        if (ret != vmpp_RSLT_OK) {
            printf("vmppInitDecoder failed %d\n", ret);
            ret_end = -1;
            return 0;
        }

        ch_apr.device = dec_fd;
        ch_apr.sourceMode = vmpp_SRC_FRAME;
        ch_apr.decodeMode = vmpp_DEC_NORMAL;
        ch_apr.maxWidth = 3840;
        ch_apr.maxHeight = 2160;
        ch_apr.extraBufferNumber = 4;
        ch_apr.streamBufferSize = MAX_STREAM_SIZE;
        ch_apr.pixelFormat = vmpp_PIX_FMT_NV12;

        ch_apr.enProfiling = 1;
        ch_apr.coreMode = vmpp_CORE_AUTO;
        ch_apr.memoryMode = (vmppDecMemoryMode)source->memory_mode;
        ch_apr.outputAlign = source->output_align;

        ret = vmppDecCreateChannel(&dec_ch, &ch_apr);
        if (ret != vmpp_RSLT_OK || !dec_ch) {
            fprintf(stderr, "create channel error %d or chn is null.\n", ret);
            ret_end = -1;
            return 0;
        }

        ret = vmppDecStart(dec_ch); // set start status.
        if (ret < 0) {
            fprintf(stderr, "start recv stream error %d.\n", ret);
            ret_end = -1;
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
            ret_end = -1;
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

            if (count > 1 && switch_drop && rand() % switch_drop == 0) {
                LOG_WARN("drop!!!!");
                continue;
            }

            if (switch_distort && rand() % switch_distort == 0) {
                LOG_WARN("make stream ERROR!!!!");
                pos = rand() % stream_len;
                random = rand() % stream_len;
                if (random + pos >= stream_len) {
                    random = stream_len - pos;
                }
                memset(stream_p + pos, 0, random);
                // printf("packet.size%d, distort.position%d, distort.num%d\n",
                // packet.size, pos, ret);
            }

            ret = vmppDecSendStream(dec_ch, &input_stream, SEND_STREAM_TIMEOUT_VALUE);
            if (ret < 0) {
                fprintf(stderr, "send stream error %d.\n", ret);
                if (ret != vmpp_RSLT_ERR_SYS_ERROR) {
                    ret_end = -1;
                    break;
                }
            }
        }

        ret = vmppDecStop(dec_ch); // disable start status.
        if (ret < 0) {
            fprintf(stderr, "stop recv stream error %d.\n", ret);
            ret_end = -1;
            return 0;
        }

        pthread_join(*((pthread_t *)task), NULL);

        free(task);

        ret = vmppDecDestroyChannel(&dec_ch);
        if (ret < 0) {
            fprintf(stderr, "destroy chn error %d.\n", ret);
            ret_end = -1;
            return 0;
        }

        if (f_in != NULL)
            fclose(f_in);
        stream_close(&strmctx);
    }// file array

    if (rt_inst.runtimeHandle) {
        close_runtime(&rt_inst);
    }
    close(dec_fd);

    close_runtime(&rt_inst);
    return 0;
}

static void usage(const char *program)
{
    printf("Usage: %s -i <input_json> -s <save_output> -l <loop> -T <thread_count> -u <userOutBuf> -a <outputAlignment> -r <streamRepeat>\n"
           "  input_json [M]: input json file.\n"
           "  save_output[M]: need save out put file.\n"
           "  loop[M]: loop times.\n"
           "  thread_count[M]: how many threads to be created.\n"
           "  userOutBuf[M]: use user output buffer or not, default: 0.\n"
           "  outputAlignment[O]: output alignment, default 0.\n"
           "  streamRepeat[O]: decode stream repeat times in one channel, default: 0\n",
           program);
}

static int parse_options(int argc, char **argv, option_t *opt)
{
    // h264_dec_mt -i input.json -s 0
    static const char optstr[] = "i:s:o:d:l:T:c:p:t:M:a:r:";
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
            opt->device = optarg;
            break;
        case 'l':
            opt->loop = atoi(optarg);
            break;
        case 'T':
            opt->thread_count = atoi(optarg);
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
        case 'p': //drop test
            opt->drop = atoi(optarg);
            break;
        case 't': //distort test
            opt->distort = atoi(optarg);
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
        exit(-1);
    }

    return 0;
}


// RET_TYPE main(int argc, char *argv[])
static RET_TYPE decode(char* json, int save, int loopp, int thread_count, int memory_mode, int output_align, int stream_repeat_times, int switch_drop_main, int switch_distort_main)
{
    struct vmpp_queue *task_queue = NULL;
    task_handle tasks[500] = {0};
    int ret = 0;
    cJSON *root = NULL;
    memset(&load_balance_buffer, 0, sizeof(struct LoadBalanceBuffer));

    memset(&option, 0, sizeof(option));
    option.loop = 1;
    option.thread_count = 1;
    option.device = NULL;//"/dev/vastai_video0";
    option.stream_repeat_times = stream_repeat_times;
    
    option.input = json;
    option.save = save;
    option.loop = loopp;
    option.thread_count = thread_count;
    option.memory_mode = memory_mode;
    option.output_align = output_align;
    need_save_file = option.save;
    switch_drop = switch_drop_main;
    switch_distort = switch_distort_main;

    if (option.input == NULL) {
        LOG_ERROR("Please specify the input file!");
        return -1;
    }

    char idleDie[64];
    for (int i = 0; i < option.thread_count; i++) {
        load_balance_buffer.channelLoad[i] = VMPP_PRE_OCCUPY_LOAD;
        load_balance_buffer.loads += load_balance_buffer.channelLoad[i];
        load_balance_buffer.channelNum++;
    }
    if (!option.device) {
        ret = vmppDecQueryIdleDie(load_balance_buffer.loads, idleDie, 0);
        if (ret < 0) {
            printf("vmppDecQueryIdleDie failed %d, using die 0\n", ret);
            option.device = (char *)"/dev/vastai_video0";
        } else {
            option.device = idleDie;
        }
    } else {
        ret = vmppDecQueryIdleDie(load_balance_buffer.loads, option.device, 1);
        if (ret < 0) {
            printf("vmppDecQueryIdleDie failed %d\n", ret);
        }
    }

    vmpp_queue_init(&task_queue);

    // random seed
    srand(time(NULL));

    char* suffix = (char*) &option.input[strlen(option.input) - 5];

    if (strcmp(suffix, ".json") == 0) {
        /* parse json to get source list. */
        if (parse_source_json(option.input, task_queue, &root) == -1) {
            ret_end = -1;
            goto handle_error;
        }
    } else {
        /* read option to get source list. */
        parse_source_option(&option, task_queue);
    }

    print_source_list(task_queue);

    source_t *source;
    while ((loop++ < option.loop) || (option.loop == 0)) {
        printf("================the %dth loop================\n", loop);
        for (int32_t i = 0; i < vmpp_queue_size(task_queue); i++) {
            printf("vmpp_queue_size(task_queue) %d, i %d\n", vmpp_queue_size(task_queue), i);
            source = (source_t *)vmpp_queue_peek(task_queue, i);
            if (source) {
		        source->memory_mode = option.memory_mode;
                tasks[i] = run_task(MainTask, source);
            }
        }

        for (int32_t i = 0; i < vmpp_queue_size(task_queue); i++) {
            pthread_join(*((pthread_t *)tasks[i]), NULL);
            free(tasks[i]);
        }
    }
handle_error:
    for (int32_t i = 0; i < vmpp_queue_size(task_queue); i++) {
        source = (source_t *)vmpp_queue_peek(task_queue, i);
        if(source->input_file) {
            free(source->input_file);
            source->input_file = NULL;
        }
        if(source->output_file) {
            free(source->output_file);
            source->output_file = NULL;
        }
        if(source) {
            free(source);
            source = NULL;
        }
            
    }

    vmpp_queue_free(&task_queue);
    if (root) {
        cJSON_Delete(root);
    }
    loop = 0;
    if (ret_end == -1) return ret_end;
    return 0;
}

