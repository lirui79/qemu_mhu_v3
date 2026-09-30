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
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "utils.h"
#include "vmpp_dec_api.h"
#include "vmpp_dec_defs.h"

#define THREAD_MAX (24)
// endof  the va_dec.h

typedef int RET_TYPE;
typedef void *task_handle;
typedef void *(*task_func)(void *);

typedef struct {
    char*                           decDevice;          // video decoder device node name
    char*                           memDevice;          // memory device node name
    char*                           input;
    char*                           output;
    int                             loop;
    int                             md5;
    int                             save;
    int                             thread_num;
} option_t;



static option_t g_option;

static void usage(const char *program)
{
    LOG(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN,
        "Usage: %s -i [input] -o [output] -d [video decoder device] -m [memory device] -l [loop] -c [check md5] -s [save] -T [thread num]", program);
    LOG_INFO("  input[M]: the input file");
    LOG_INFO("  output[O]: the output dir, default: output");
    LOG_INFO("  video device[O]: render video decoder device name, default: /dev/hantrodec");
    LOG_INFO("  memory device[O]: render memory device name, default: /dev/memalloc");
    LOG_INFO("  loop[O]: loop times, default: 1");
    LOG_INFO("  check md5[O]: need calculate md5, default: 1");
    LOG_INFO("  save[O]: need save yuv file, default:1");
    LOG_INFO("  thread num[O]: thread num [1, 24], default:1");
    LOG_INFO(" ./jpeg_dec_mt -i /home/stone/workspace/stream1.jpg -o output -d /dev/hantrodec -m /dev/memalloc -l 1 -c 0 -T 4");
    LOG_INFO(" ./jpeg_dec_mt -i /home/stone/workspace/VC9000D.jpg -o output -d /dev/hantrodec -m /dev/memalloc -l 1 -c 0 -T 4");
}

static int parse_options(int argc, char **argv, option_t *opt)
{
    static const char optstr[] = "i:o:d:m:s:c:T:l:h";
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
        case 's':
            opt->save = atoi(optarg);
            break;
        case 'T':
            opt->thread_num = atoi(optarg);
            if (opt->thread_num < 1 || opt->thread_num > THREAD_MAX) {
                LOG_ERROR("thread num must be in range [1, %d]", THREAD_MAX);
                return -1;
            }
            break;
        case 'c':
            opt->md5 = atoi(optarg);
            break;
        case 'l':
            opt->loop = atoi(optarg);
            break;
        case 'h':
            return -2;
        case '?':
            return -1;
        default:
            LOG_ERROR("\n Unsupported option: %c", c);
            return -1;
        }
    }

    if (optind < argc) {
        return -1;
    }

    return 0;
}

static uint32_t GetBytes(uint8_t *stream, uint32_t idx, uint8_t *buffer, uint32_t buffer_length)
{
    uint32_t offset = (vmppAddr)stream - (vmppAddr)buffer;
    if (offset + idx < buffer_length)
        return buffer[offset + idx];
    else
        return buffer[offset + idx - buffer_length];
}

uint32_t FindImageEOI(uint8_t *stream, uint32_t stream_length, uint32_t *p_offset, uint8_t *buffer,
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

struct thread_param_t {
    void  *dec_ch;
    char  *output;
    int    save;
    /* Set by the sender once it is done, so the output thread can leave its
     * wait loop (one flag per channel, the sample runs several channels). */
    volatile int stop;
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

    if (thread_param->save) {
        output_file = fopen(thread_param->output, "wb");
        printf("[APP][%p]open YUV file to write: %s\n", dec_ch, thread_param->output);
        if (output_file == NULL) {
            fprintf(stderr, "[APP][%p]Unable to open output file: %s\n", dec_ch,
                    thread_param->output);
            return NULL;
        }
    }

    out_opt.memoryType = vmpp_MEM_HOST; // vmpp_MEM_DEVICE
    out_opt.enableCrop = 0;

    if (!thread_param->save)
        out_opt.memoryType = vmpp_MEM_DEVICE;

    while (1) {
        ret = vmppDecReceiveFrame(dec_ch, &out_frame, &out_opt, 500);

        printf("[APP][%p]vmppDecReceiveFrame (%d), len: %d, ret: %d, display: %dx%d, real: %dx%d\n", dec_ch, recv_num, 
            out_frame.dataSize, ret, out_frame.cropInfo.width, out_frame.cropInfo.height, out_frame.width, out_frame.height);

        if (ret == vmpp_RSLT_OK) {
            recv_num ++;
            if (thread_param->save) {
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
            /* No frame available yet, keep waiting unless the sender is done.
             * vmppDecStop() queues an end-of-stream picture, so the thread is
             * released through DEC_END_OF_STREAM once the pending frames are
             * drained. */
            if (thread_param->stop)
                break;
            usleep(1000);
        } else {
            fprintf(stderr, "receive frame error %d.\n", ret);
            break;
        }
    }

    if (thread_param->save)
        fclose(output_file);

    return NULL;
}

uint8_t *get_stream(const char *input_file_str, uint8_t *input_buf, uint32_t buf_len,
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

static void *MainTask(void *thread_args) {
    uint8_t *input_buf;
    uint32_t input_len = MAX_STREAM_SIZE_4_JPEG;
    uint32_t stream_len;
    uint8_t *stream_p;
    vmppChannel dec_ch;
    vmppDecChannelParameters ch_apr = {0};
    vmppStream input_stream;
    task_handle task = NULL;
    vmppResult ret;
    //vmppDecStreamInfo info;
    option_t *args = (option_t *) thread_args;
    struct thread_param_t thread_param;
    struct timeval tFirstBegin, tBegin, tEnd;
    int frame_num = 0;

    input_buf = malloc(input_len);
    stream_p = get_stream(args->input, input_buf, input_len, &stream_len);

    ch_apr.decDevice = args->decDevice;
    ch_apr.memDevice = args->memDevice;
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
        return 0;
    }

    ret = vmppDecStart(dec_ch); // set start status.
    //printf("[APP][%p]vmppDecStart, ret: %d\n", dec_ch, ret);
    if (ret < 0) {
        fprintf(stderr, "start recv stream error %d.\n", ret);
        return 0;
    }

    thread_param.dec_ch = dec_ch;
    thread_param.output = args->output;
    thread_param.save   = args->save;
    thread_param.stop   = 0;
    task = run_task(output_thread, &thread_param);

    gettimeofday(&tFirstBegin, NULL);

    for (frame_num = 0; frame_num< args->loop; frame_num++)
    {
        gettimeofday(&tBegin, NULL);

        input_stream.stream = stream_p;
        input_stream.len = stream_len;
        ret = vmppDecSendStream(dec_ch, &input_stream, 4000);
        if (ret < 0) {
            fprintf(stderr, "[APP][%p]vmppDecSendStream, ret: %d\n", dec_ch, ret);
            thread_param.stop = 1;
            return 0;
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
        return 0;
    }

    /* Give the output thread a chance to drain the already decoded pictures
     * before telling it that no more frames will come. */
    usleep(200 * 1000);
    thread_param.stop = 1;

    //printf("[APP][%p]waiting output thread  ...\n", dec_ch);
    pthread_join(*((pthread_t *)task), NULL);

    free(task);
    free(input_buf);

    printf("[APP][%p]channel destroyed!\n", dec_ch);
    ret = vmppDecDestroyChannel(&dec_ch);
    if (ret < 0) {
        fprintf(stderr, "destroy chn error %d.\n", ret);
        return 0;
    }

    return 0;
}

RET_TYPE main(int argc, char** argv) {
    int ret = 0;
    option_t args[THREAD_MAX];
    task_handle task[THREAD_MAX];

    option_t *option = &g_option;
    option = malloc(sizeof(option_t));
    option->loop = 1;
    option->md5 = 1;
    option->save = 1;
    option->decDevice = "/dev/hantrodec";
    option->memDevice = "/dev/memalloc";
    option->output = "output";
    option->thread_num = 1;

    ret = parse_options(argc, argv, option);
    if (ret < 0) {
        usage(argv[0]);
        if (ret != -2)
            LOG_ERROR("Failed to parse the input arguments!");
        return -1;
    }
    if (option->input == NULL) {
        LOG_ERROR("Please specify the input JPEG file!");
        usage(argv[0]);
        return -1;
    }

    LOG_INFO("render video decoder device name: %s", option->decDevice);
    LOG_INFO("render memory device name: %s", option->memDevice);
    LOG_INFO("perf_count: %d", option->loop);
    LOG_INFO("md5_check: %d", option->md5);
    LOG_INFO("save_yuv: %d", option->save);
    LOG_INFO("output file: %s\n", option->output);

    /* The output thread fopen()s "<dir>/output_N.yuv" without checking the
     * result, so make sure the directory exists before starting the threads. */
    if (option->save) {
        struct stat st;
        if (stat(option->output, &st) != 0 && mkdir(option->output, 0777) != 0)
            LOG_ERROR("Unable to create output directory: %s", option->output);
    }

    vmppConfiguration cfg;
    memset(&cfg, 0, sizeof(vmppConfiguration));

    ret = vmppInitDecoder(&cfg);
    if (ret != vmpp_RSLT_OK) {
        printf("vmppInitDecoder failed %d\n", ret);
        return 0;
    }

    for (int i = 0; i < option->thread_num; i++) {
        args[i].input = option->input;
        char *output = (char*) malloc(1024);
        sprintf(output, "%s/output_%d.yuv", option->output, i);
        args[i].output = output;
        args[i].decDevice = option->decDevice;
        args[i].memDevice = option->memDevice;
        args[i].save    = option->save;
        args[i].loop    = option->loop;
    }

    for (int i = 0; i < option->thread_num; i++) {
        task[i] = run_task(MainTask, &args[i]);
    }

    for (int i = 0; i < option->thread_num; i++) {
        pthread_join(*((pthread_t *)task[i]), NULL);
    }

    for (int i = 0; i < option->thread_num; i++) {
        free(task[i]);
    }

    for (int i = 0; i < option->thread_num; i++) {
        if(args[i].output)
            free(args[i].output);
    }

    return 0;
}
