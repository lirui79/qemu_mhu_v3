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

#include "utils.h"

#include "vmpp_dec_api.h"
#include "vmpp_dec_defs.h"

#define LIB_VACCRT_PATH "libvaccrt.so"

// endof  the va_dec.h

static char dev_str_default[100] = "/dev/vastai_video0";
typedef int RET_TYPE;
typedef void *task_handle;
typedef void *(*task_func)(void *);
static int32_t loop = 0;
// static int need_save_file = 0;

typedef struct {
    char *input_file_str;
    char *output_file_str;
    char *dev_str;
    int save_yuv;
} MainArgs;

typedef struct {
    char* input_file;
    char* output_file;
    char* device;
    int save_yuv;
    int loop;
} source_t;

typedef struct {
    char* json;
    int save;
    int loop;
    int drop;
    int distort;
} option_t;

static void parse_source_json(const char* file, struct vmpp_queue* queue, cJSON** proot) {
    FILE* fp_json_file = NULL;
    char json_buf[102400];
    cJSON* root, * arrayItem, * item, * object;

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

    arrayItem = cJSON_GetObjectItem(root, "sources");
    if (arrayItem != NULL) {
        int32_t size = cJSON_GetArraySize(arrayItem);
        for (int32_t index = 0; index < size; index++) {
            char *input_buf = (char*)malloc(1024*sizeof(char));
            char *output_buf = (char*)malloc(1024*sizeof(char));
            source_t* a_src = (source_t*)malloc(sizeof(source_t));
            if (!a_src) {
                fprintf(stderr, "malloc a_source failed!\n");
                free(a_src);
                return;
            }
            object = cJSON_GetArrayItem(arrayItem, index);
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
            vmpp_queue_push_back(queue, a_src);
        }
    }
    fclose(fp_json_file);
    *proot = root;
}

static void print_source_list(struct vmpp_queue* queue) {
    for (int32_t i = 0; i < vmpp_queue_size(queue); i++) {
        source_t* source = (source_t*)vmpp_queue_peek(queue, i);
        if (source) {
            fprintf(stdout, "source_list[%d]: input %s, output %s, device %s.\n", i,
                source->input_file, source->output_file, source->device);
        }
    }
}

static uint32_t GetBytes(uint8_t *stream, uint32_t idx, uint8_t *buffer, uint32_t buffer_length)
{
    uint32_t offset = (vmppAddr)stream - (vmppAddr)buffer;
    if (offset + idx < buffer_length)
        return buffer[offset + idx];
    else
        return buffer[offset + idx - buffer_length];
}

static uint32_t FindImageEOI(uint8_t *stream, uint32_t stream_length, uint32_t *p_offset, uint8_t *buffer,
                      uint32_t buf_len)
{
    uint32_t i, j;
    uint32_t jpeg_thumb_in_stream = 0;
    uint32_t tmp, tmp1, tmp_total = 0;

    *p_offset = 0;
    for (i = 0; i < stream_length; ++i) {
        if (0xFF == GetBytes(stream, i, buffer, buf_len)) {
            /* if 0xFFE1 to 0xFFFD ==> skip  */

            if( (((i + 1) < stream_length) &&
          0xE1 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE2 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE3 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE4 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE5 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE6 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE7 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE8 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE9 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xEA == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xEB == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xEC == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xED == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xEE == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xEF == GetBytes(stream, i + 1, buffer, buf_len)) /*||
          (((i + 1) < stream_length) && 0xF0 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF1 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF2 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF3 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF4 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF5 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF6 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF7 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF8 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF9 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xFA == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xFB == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xFC == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xFD == stream[i + 1])*/ ) {
                /* increase counter */
                i += 2;

                /* check length vs. data */
                if ((i + 1) > (stream_length))
                    return (-1);

                /* get length */
                tmp = GetBytes(stream, i, buffer, buf_len);
                tmp1 = GetBytes(stream, i + 1, buffer, buf_len);
                tmp_total = (tmp << 8) | tmp1;

                /* check length vs. data */
                if ((tmp_total + i) > (stream_length))
                    return (-1);
                /* update */
                i += tmp_total - 1;
                continue;
            }

            /* if 0xFFC2 to 0xFFCB ==> skip  */
            if ((((i + 1) < stream_length) && 0xC1 == GetBytes(stream, i + 1, buffer, buf_len)) ||
                (((i + 1) < stream_length) && 0xC2 == GetBytes(stream, i + 1, buffer, buf_len)) ||
                (((i + 1) < stream_length) && 0xC3 == GetBytes(stream, i + 1, buffer, buf_len)) ||
                (((i + 1) < stream_length) && 0xC5 == GetBytes(stream, i + 1, buffer, buf_len)) ||
                (((i + 1) < stream_length) && 0xC6 == GetBytes(stream, i + 1, buffer, buf_len)) ||
                (((i + 1) < stream_length) && 0xC7 == GetBytes(stream, i + 1, buffer, buf_len)) ||
                (((i + 1) < stream_length) && 0xC8 == GetBytes(stream, i + 1, buffer, buf_len)) ||
                (((i + 1) < stream_length) && 0xC9 == GetBytes(stream, i + 1, buffer, buf_len)) ||
                (((i + 1) < stream_length) && 0xCA == GetBytes(stream, i + 1, buffer, buf_len)) ||
                (((i + 1) < stream_length) && 0xCB == GetBytes(stream, i + 1, buffer, buf_len))) {
                /* increase counter */
                i += 2;

                /* check length vs. data */
                if ((i + 1) > (stream_length))
                    return (-1);

                /* get length */
                tmp = GetBytes(stream, i, buffer, buf_len);
                tmp1 = GetBytes(stream, i + 1, buffer, buf_len);
                tmp_total = (tmp << 8) | tmp1;

                /* check length vs. data */
                if ((tmp_total + i) > (stream_length))
                    return (-1);
                /* update */
                i += tmp_total - 1;

                /* look for EOI */
                for (j = i; j < stream_length; ++j) {
                    if (0xFF == GetBytes(stream, j, buffer, buf_len)) {
                        /* EOI */
                        if (((j + 1) < stream_length) &&
                            0xD9 == GetBytes(stream, j + 1, buffer, buf_len)) {
                            /* check length vs. data */
                            if ((j + 2) >= (stream_length)) {
                                *p_offset = j + 2;
                                return (0);
                            }
                            /* update */
                            i = j;
                            /* stil data left ==> continue */
                            continue;
                        }
                    }
                }
            }

            /* check if thumbnails in stream */
            if (((i + 1) < stream_length) && 0xE0 == GetBytes(stream, i + 1, buffer, buf_len)) {
                if (((i + 9) < stream_length) && 0x4A == GetBytes(stream, i + 4, buffer, buf_len) &&
                    0x46 == GetBytes(stream, i + 5, buffer, buf_len) &&
                    0x58 == GetBytes(stream, i + 6, buffer, buf_len) &&
                    0x58 == GetBytes(stream, i + 7, buffer, buf_len) &&
                    0x00 == GetBytes(stream, i + 8, buffer, buf_len) &&
                    0x10 == GetBytes(stream, i + 9, buffer, buf_len)) {
                    jpeg_thumb_in_stream = 1;
                }
            }

            /* EOI */
            if (((i + 1) < stream_length) && 0xD9 == GetBytes(stream, i + 1, buffer, buf_len)) {
                *p_offset = i + 2;
                /* update amount of thumbnail or full resolution image */
                if (jpeg_thumb_in_stream) {
                    jpeg_thumb_in_stream = 0;
                } else
                    return 0;
            }
        }
    }

    return -1;
}

static task_handle run_task(task_func func, void *param)
{
    int ret;
    pthread_t *thread_handle = (pthread_t *)malloc(sizeof(pthread_t));

#ifndef ARM64
    pthread_attr_t attr;
    struct sched_param par;
    pthread_attr_init(&attr);
    // ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    // assert(ret == 0);
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

struct thread_param_t {
    void *dec_ch;
    char *output_file_str;
    int save_yuv;
};

static void *output_thread(void *arg)
{
    vmppFrame out_frame;
    vmppDecOutputOptions out_opt;
    void *dec_ch;
    vmppResult ret;
    FILE *output_file = NULL;
    struct thread_param_t *thread_param = (struct thread_param_t *)arg;
    int recv_num = 0;

    dec_ch = thread_param->dec_ch;

    if (thread_param->save_yuv) {
        output_file = fopen(thread_param->output_file_str, "wb");
        if (!output_file) {
            fprintf(stderr, "open output file %s error.\n", thread_param->output_file_str);
            exit(-1);
        }
        printf("[APP][%p]open YUV file to write: %s\n", dec_ch, thread_param->output_file_str);
    }

    out_opt.memoryType = vmpp_MEM_HOST; // vmpp_MEM_DEVICE
    out_opt.enableCrop = 0;

    if (!thread_param->save_yuv)
        out_opt.memoryType = vmpp_MEM_DEVICE;

    while (1) {
        ret = vmppDecReceiveFrame(dec_ch, &out_frame, &out_opt, 500);

        printf("[APP][%p]vmppDecReceiveFrame (%d), len: %d, ret: %d, display: %dx%d, real: %dx%d\n", dec_ch, recv_num, 
            out_frame.dataSize, ret, out_frame.cropInfo.width, out_frame.cropInfo.height, out_frame.width, out_frame.height);

        if (ret == vmpp_RSLT_OK) {
            recv_num ++;
            if (thread_param->save_yuv) {
                if (out_opt.enableCrop == 1) {
                    //  write Y
                    fwrite(out_frame.data[0], 1, out_frame.cropInfo.width * out_frame.cropInfo.height,
                        output_file);

                    // write UV
                    if (out_frame.data[1]) {
                        uint32_t uv_crop_width = (out_frame.cropInfo.width % 2 == 1)
                                                    ? (out_frame.cropInfo.width + 1)
                                                    : (out_frame.cropInfo.width);
                        uint32_t uv_crop_height = (out_frame.cropInfo.height % 2 == 1)
                                                    ? (out_frame.cropInfo.height + 1)
                                                    : (out_frame.cropInfo.height);
                        fwrite(out_frame.data[1], 1, uv_crop_width * uv_crop_height / 2, output_file);
                    }
                } else {
                    // write Y
                    for (uint32_t j = 0; j < out_frame.cropInfo.height; j++) {
                        fwrite(out_frame.data[0] + out_frame.width * j, 1, out_frame.cropInfo.width,
                            output_file);
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

                    /* not to crop */
                    // fwrite(out_frame.data, 1, out_frame.dataSize, output_file);
                    // printf("[APP][%p][en %d]write YUV done! width: %d, height: %d\n", dec_ch,
                    // out_opt.enableCrop, out_frame.width,
                    //    out_frame.height);
                }
            }

#if 0
            while(1) {
                vmppStatus status;
                vmppResult ret = va_vdec_get_status(dec_ch, &status);
                printf("[APP][%p]va_vdec_get_status %d, %d\n", dec_ch, ret, status.state);
                if (status.state == vmpp_ST_STOPPED)
                    break;
            }
#endif
            ret = vmppDecReleaseFrame(dec_ch, &out_frame, 500);
            if (ret < 0) {
                fprintf(stderr, "release frame error %d.\n", ret);
            }

            // break;

        } else if (ret == vmpp_RSLT_WARN_EOS) {
            break;
        } else if (ret == vmpp_RSLT_WARN_MORE_DATA) {
            fprintf(stdout, "receive more data.\n");
            break;
        } else {
            fprintf(stderr, "receive frame error %d.\n", ret);
            break;
        }
    }

    if (thread_param->save_yuv && output_file) {
        fclose(output_file);
        output_file = NULL;
    }

    return NULL;
}

static uint8_t *get_stream(const char *input_file_str, uint8_t *input_buf, uint32_t buf_len,
                    uint32_t *in_stream_len)
{
    FILE *f_in;
    uint32_t ret;
    uint32_t len;
    uint32_t stream_len;
    uint8_t *stream_p;

    f_in = fopen(input_file_str, "rb");
    if (f_in == NULL) {
        fprintf(stderr, "Unable to open input file\n");
        exit(-1);
    }

    /* file i/o pointer to full */
    fseek(f_in, 0L, SEEK_END);
    len = ftell(f_in);
    rewind(f_in);

    if (len > buf_len)
        len = buf_len;

    /* read input stream from file to buffer and close input file */
    ret = fread(input_buf, sizeof(uint8_t), len, f_in);
    (void)ret;

    fclose(f_in);

    stream_p = input_buf;
    ret = FindImageEOI(stream_p, len, &stream_len, input_buf, buf_len);
    if (ret != 0) {
        printf("EOI missing from end of file!\n");
    }

    *in_stream_len = stream_len;
    return stream_p;
}

static void print_usage(char** argv)
{
    printf("Usage: %s <input_file_path> <output_file_folder> [render_node_path] [thread_num] [save_yuv]\n"
           "  input_file_path [M]:   the input jpeg file path\n"
           "  output_file_folder[M]: the output yuv folder path\n"
           "  render_node_path[O]:   default value: %s\n"
           "  thread_num[O]:         default: 100\n"
           "  save_yuv[O]:           default: 0\n",
           argv[0], dev_str_default);
}

typedef rtError_t (*vaccrt_init_t)(uint32_t dev_id);

static void *MainTask(void *args)
{
    uint8_t *input_buf;
    uint32_t input_len = MAX_STREAM_SIZE_4_JPEG;
    uint32_t stream_len;
    uint8_t *stream_p;
    vmppChannel dec_ch;
    int dec_fd;
    vmppDecChannelParameters ch_apr = {0};
    vmppStream input_stream;
    task_handle task = NULL;
    vmppRuntimeInstance rt_inst;
    vmppResult ret;
    // vmppDecStreamInfo info;
    // MainArgs *args = args_void;
    char *input_file_str = NULL;
    char *output_file_folder = NULL;
    char *dev_str = dev_str_default;
    int save_yuv = 0;
    struct thread_param_t thread_param;
    struct timeval tFirstBegin, tBegin, tEnd;
    int frame_num = 0;

    source_t* source = (source_t*)args;

    if (source->input_file && source->output_file) {
        input_file_str = source->input_file;
        output_file_folder = source->output_file;
        if (source->device) {
            dev_str = source->device;
        }
        if (source->save_yuv) {
            save_yuv = source->save_yuv;
        }
    }
    else {
        print_usage((char **)"ut_multi_thread_jpeg_dec");
        exit(-1);
    }

    open_runtime(&rt_inst); // run time init

    dec_fd = open(dev_str, O_RDWR);
    if (dec_fd < 0) {
        printf("Cannot open DRM render node for device %s\n", dev_str);
        exit(-1);
    }

    input_buf = (uint8_t *)malloc(input_len);
    stream_p = get_stream(input_file_str, input_buf, input_len, &stream_len);

    int dieId = 0;
    vaccrt_init_t vaccrt_init = (vaccrt_init_t)(&rt_inst)->init;
    sscanf(dev_str, "/dev/vastai_video%d", &dieId);
    rtError_t vaccRet = vaccrt_init(dieId);
    if (vaccRet) {
        printf("vaccrt_init failed: err %d\n", vaccRet);
        exit(-1);
    }
    vmppConfiguration cfg;
    memset(&cfg, 0, sizeof(vmppConfiguration));
    cfg.runtimeInst = rt_inst;

    ret = vmppInitDecoder(&cfg);
    if (ret != vmpp_RSLT_OK) {
        printf("vmppInitDecoder failed %d\n", ret);
        exit(-1);
    }

    ch_apr.device = dec_fd;
    ch_apr.codecType = vmpp_CODEC_DEC_JPEG;
    ch_apr.sourceMode = vmpp_SRC_FRAME;
    ch_apr.decodeMode = vmpp_DEC_NORMAL;
    ch_apr.maxWidth = 32768;
    ch_apr.maxHeight = 32768;
    ch_apr.streamBufferSize = MAX_STREAM_SIZE_4_JPEG;
    ch_apr.pixelFormat = vmpp_PIX_FMT_NV12;

    ch_apr.enProfiling = 1;

    ret = vmppDecCreateChannel(&dec_ch, &ch_apr);
    if (ret != vmpp_RSLT_OK || !dec_ch) {
        fprintf(stderr, "create channel error %d or chn is null.\n", ret);
        exit(-1);
    }

    ret = vmppDecStart(dec_ch); // set start status.
    // printf("[APP][%p]vmppDecStart, ret: %d\n", dec_ch, ret);
    if (ret < 0) {
        fprintf(stderr, "start recv stream error %d.\n", ret);
        exit(-1);
    }

    thread_param.dec_ch = dec_ch;
    thread_param.output_file_str = output_file_folder;
    thread_param.save_yuv = save_yuv;
    task = run_task(output_thread, &thread_param);

    gettimeofday(&tFirstBegin, NULL);

    for (frame_num = 0; frame_num< source->loop; frame_num++)
    {
        gettimeofday(&tBegin, NULL);

        input_stream.stream = stream_p;
        input_stream.len = stream_len;
        ret = vmppDecSendStream(dec_ch, &input_stream, 4000);
        if (ret < 0) {
            fprintf(stderr, "[APP][%p]vmppDecSendStream, ret: %d\n", dec_ch, ret);
            exit(-1);
        }

        gettimeofday(&tEnd, NULL);
        long deltaTime = 1000000L * (tEnd.tv_sec - tBegin.tv_sec) + (tEnd.tv_usec - tBegin.tv_usec);
        printf("[APP][%p]vmppDecSendStream(%d) len=%d, ret=%d, cost %ldus, %.2ffps\n", dec_ch, frame_num, 
            stream_len, ret, deltaTime, 1. / deltaTime * 1000000);
#if 0
        ret = vmppDecGetStreamInfo(dec_ch, &info);
        printf("[APP][%p]vmppDecGetStreamInfo, width: %d, height: %d, ret: %d\n", dec_ch,
               info.width, info.height, ret);
#endif

#if 0
        stream_p = get_stream(input_buf, input_len, &stream_len);
        if(stream_p == NULL)
            break;
#endif
    }

    gettimeofday(&tEnd, NULL);
    long deltaTime = 1000000L * (tEnd.tv_sec - tFirstBegin.tv_sec) + (tEnd.tv_usec - tFirstBegin.tv_usec);
    deltaTime /= frame_num;
    printf("[APP][%p]Total Decode %d frames, avg cost %ldus, avg %.2ffps\n", dec_ch, frame_num, deltaTime, 1. / deltaTime * 1000000);

    ret = vmppDecStop(dec_ch); // disable start status.
    //printf("[APP][%p]vmppDecStop, ret: %d\n", dec_ch, ret);
    if (ret < 0) {
        fprintf(stderr, "stop recv stream error %d.\n", ret);
        exit(-1);
    }

    //printf("[APP][%p]waiting output thread  ...\n", dec_ch);
    pthread_join(*((pthread_t *)task), NULL);

    free(task);
    free(input_buf);

    printf("[APP][%p]channel destroyed!\n", dec_ch);
    ret = vmppDecDestroyChannel(&dec_ch);
    if (ret < 0) {
        fprintf(stderr, "destroy chn error %d.\n", ret);
        exit(-1);
    }

    if (rt_inst.runtimeHandle) {
        close_runtime(&rt_inst);
    }
    close(dec_fd);
    return 0;
}

static void usage(const char* program) {
    printf("Usage: %s -i <input_json> -s <save_output> -l <loop>\n"
        "  input_json [M]: input json file.\n"
        "  save_output[M]: need save out put file.\n"
        "  loop[M]: loop times.\n",
        program);
}

// static int parse_options(int argc, char** argv, option_t* opt) {
//     // h264_dec_mt -i input.json -s 0
//     static const char optstr[] = "i:s:l:h";
//     int c;
//     while ((c = getopt(argc, argv, optstr)) != -1) {
//         switch (c) {
//         case 'i':
//             opt->json = optarg;
//             break;
//         case 's':
//             opt->save = atoi(optarg);
//             break;
//         case 'l':
//             opt->loop = atoi(optarg);
//             break;
//         case 'h':
//             return -2;
//         case '?':
//             usage(argv[0]);
//             return -1;
//         default:
//             fprintf(stderr, "\n Unsupported option: %c \n", c);
//             usage(argv[0]);
//             return -1;
//         }
//     }

//     if (optind < argc || argc < 7) {
//         usage(argv[0]);
//         return -1;
//     }

//     return 0;
// }

static RET_TYPE decode(char* json, int save, int loopp, int loop_in_loopp)
{
    struct vmpp_queue* task_queue = NULL;
    task_handle tasks[100] = { 0 };
    cJSON *root = NULL;

    option_t* option = (option_t*)malloc(sizeof(option_t));
    option->json = json;
    option->save = save;
    option->loop = loopp;
    // need_save_file = option->save;
    // switch_drop = switch_drop_main;
    // switch_distort = switch_distort_main;

    vmpp_queue_init(&task_queue);

    /* parse json to get source list. */
    parse_source_json(option->json, task_queue, &root);
    print_source_list(task_queue);

    source_t* source = NULL;
    while ((loop++ < option->loop) || (option->loop == 0)) {
        printf("================the %dth loop================\n", loop);
        for (int32_t i = 0; i < vmpp_queue_size(task_queue); i++) {
            source = (source_t*)vmpp_queue_peek(task_queue, i);
            source->save_yuv = option->save;
            source->loop = loop_in_loopp;
            if (source) {
                // random seed
                srand(time(NULL));
                tasks[i] = run_task(MainTask, source);
            }
        }

        for (int32_t i = 0; i < vmpp_queue_size(task_queue); i++) {
            pthread_join(*((pthread_t*)tasks[i]), NULL);
            free(tasks[i]);
        }
        // has_begin = 0;
    }
    loop = 0;

    for (int32_t i = 0; i < vmpp_queue_size(task_queue); i++) {
        source = (source_t*)vmpp_queue_peek(task_queue, i);
        if(source->input_file)
            free(source->input_file);
        if(source->output_file)
            free(source->output_file);
        free(source);
    }

    vmpp_queue_free(&task_queue);
    if (root) {
        cJSON_Delete(root);
    }
    free(option);

    return 0;
}
