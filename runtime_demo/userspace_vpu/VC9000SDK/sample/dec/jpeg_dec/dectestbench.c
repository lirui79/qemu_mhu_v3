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

#include "defs.h"
#include "md5.h"
#include "utils.h"
#include "vmpp_dec_api.h"
#include "vmpp_dec_defs.h"

#define MD5_BUFSIZE 1024 * 16

//#define PRINT_JPEG_INFO

typedef int RET_TYPE;
typedef void *task_handle;
typedef void *(*task_func)(void *);

typedef struct {
    int                 argc;
    char              **argv;
} MainArgs;

typedef struct {
    char*               decDevice;          // video decoder device node name
    char*               memDevice;          // memory device node name
    char*               input;
    char*               output;
    int                 loop;
    int                 md5;
    int                 save;
} option_t;

int md5ctx_inited = 0;
struct md5_context md5ctx = {0};
uint8_t cur_md5sum[MD5_HASH_LEN] = {0};
uint8_t last_md5sum[MD5_HASH_LEN] = {0};
int md5_saved = 0;

static option_t *option = NULL;

static void usage(const char *program)
{// ./jpeg_dec -i ../../resource/stream1.jpg -o output.yuv -d /dev/hantrodec -m /dev/memalloc -l 0 -c 0
    LOG(LOG_LEVEL_INFO, COLOR_LIGHT_CYAN,
        "Usage: %s -i [input] -o [output] -d [video decoder device] -m [memory device]-l [loop] -c [check md5] -s [save]", program);
    LOG_INFO("  input[M]: the input file");
    LOG_INFO("  output[O]: the output file, default: output.yuv");
    LOG_INFO("  video device[O]: render video decoder device name, default: /dev/hantrodec");
    LOG_INFO("  memory device[O]: render memory device name, default: /dev/memalloc");
    LOG_INFO("  loop[O]: loop times, default: 1");
    LOG_INFO("  check md5[O]: need calculate md5, default: 1");
    LOG_INFO("  save[O]: need save yuv file, default:1");
    LOG_INFO(" ./jpeg_dec -i ../../resource/stream1.jpg -o output.yuv -d /dev/hantrodec -m /dev/memalloc -l 0 -c 0");
    LOG_INFO(" ./jpeg_dec -i /home/stone/workspace/stream1.jpg -o /tmp/out.yuv -d /dev/hantrodec -m /dev/memalloc -l 1 -c 0");
    LOG_INFO(" ./jpeg_dec -i /home/stone/workspace/VC9000D.jpg -o /tmp/out2.yuv -d /dev/hantrodec -m /dev/memalloc -l 1 -c 0");
}

static int parse_options(int argc, char **argv, option_t *opt)
{
    static const char optstr[] = "i:o:d:m:s:c:l:h";
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
        LOG_INFO("curr: %.2f fps, avg: %.2f fps\n", 1. / deltaTime * 1000000,
                 1. / avg_time * 1000000);
        fflush(stdout);
        // gettimeofday(&tBegin, NULL);
        tBegin = tEnd;
    }
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

struct thread_param_t {
    void *dec_ch;
    char *output_file_str;
    uint32_t md5_check;
    uint32_t save_yuv;
};

/* Set once the main task has no more stream to send. The JPEG channel never
 * reports EOS (VCDecEndOfStream is not used by the JPEG codec), so this flag is
 * what lets the output thread leave its wait loop instead of hanging in
 * pthread_join(). */
static volatile int g_output_stop = 0;

static void *output_thread(void *arg)
{
    vmppFrame out_frame;
    vmppDecOutputOptions out_opt;
    void *dec_ch;
    vmppResult ret;
    FILE *output_file;
    struct thread_param_t *thread_param = (struct thread_param_t *)arg;
    int recv_num = 0;

    dec_ch = thread_param->dec_ch;
    output_file = fopen(thread_param->output_file_str, "wb");
    LOG_INFO("[APP][%p]open YUV file to write: %s", dec_ch, thread_param->output_file_str);

    out_opt.memoryType = vmpp_MEM_HOST; // vmpp_MEM_DEVICE
    out_opt.enableCrop = 0;

    if (!thread_param->md5_check && !thread_param->save_yuv)
        out_opt.memoryType = vmpp_MEM_DEVICE;

    while (1) {
        ret = vmppDecReceiveFrame(dec_ch, &out_frame, &out_opt, 500);
        LOG_INFO("[APP][%p]vmppDecReceiveFrame (%d), len: %d, ret: %d", dec_ch, recv_num,
                 out_frame.dataSize, ret);

        if (ret == vmpp_RSLT_OK) {
            if (thread_param->md5_check && out_frame.memoryType == vmpp_MEM_HOST) {
                compute_md5sum((const unsigned char *)out_frame.data[0], out_frame.dataSize,
                               cur_md5sum);
                compute_md5sum(NULL, out_frame.dataSize, cur_md5sum);
                print_md5(cur_md5sum, MD5_HASH_LEN);
#if 0
                FILE* fp = fopen("test.bin", "wb");
                fwrite(out_frame.data[0], 1, out_frame.dataSize, fp);
                fclose(fp);
                system("md5sum test.bin");
#endif
            }

            if (thread_param->save_yuv && out_frame.memoryType == vmpp_MEM_HOST && output_file) {
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
                    // fwrite(out_frame.data[0], 1, out_frame.dataSize, output_file);
                    // LOG_INFO("[APP][%p][en %d]write YUV done! width: %d, height: %d", dec_ch,
                    // out_opt.enableCrop, out_frame.width,
                    //    out_frame.height);
                }
            }

            LOG_INFO("[APP][%p][en %d] write and crop YUV done! orig: %dx%d, crop: %dx%d, "
                     "stride: %d %d",
                     dec_ch, out_opt.enableCrop, out_frame.width, out_frame.height,
                     out_frame.cropInfo.width, out_frame.cropInfo.height, out_frame.stride[0],
                     out_frame.stride[1]);

            ret = vmppDecReleaseFrame(dec_ch, &out_frame, 500);
            LOG_INFO("[APP][%p]vmppDecReleaseFrame (%d), ret: %d", dec_ch, recv_num, ret);
            if (ret < 0) {
                LOG_ERROR("release frame error %d.", ret);
            }
            recv_num++;

            // break;
        } else if (ret == vmpp_RSLT_WARN_EOS) {
            break;
        } else if (ret == vmpp_RSLT_WARN_MORE_DATA) {
            /* No frame available yet, keep waiting unless the sender is done. */
            if (g_output_stop)
                break;
            usleep(1000);
        } else {
            LOG_ERROR("receive frame error %d.", ret);
            break;
        }
    }

    fclose(output_file);
    return NULL;
}

uint8_t *get_stream(char *input_file_str, uint8_t *input_buf, uint32_t buf_len,
                    uint32_t *in_stream_len)
{
    FILE *f_in;
    uint32_t ret;
    uint32_t len;
    uint32_t stream_len;
    uint8_t *stream_p;

    f_in = fopen(input_file_str, "rb");
    if (f_in == NULL) {
        LOG_ERROR("Unable to open input file");
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
        LOG_ERROR("EOI missing from end of file!");
    }

    *in_stream_len = stream_len;
    return stream_p;
}

RET_TYPE MainTask(MainArgs *args)
{
    uint8_t *input_buf;
    uint32_t input_len = MAX_STREAM_SIZE_4_JPEG;
    uint32_t stream_len;
    uint8_t *stream_p;
    vmppChannel dec_ch, dec_ch1;
    vmppDecChannelParameters ch_apr = {0};
    vmppStream input_stream;
    task_handle task = NULL;
    vmppResult ret;
#ifdef PRINT_JPEG_INFO
    vmppDecJpegInfo info;
#endif

    // char *input_file_str = NULL;
    // char *output_file_str = NULL;
    // char *dev_str = dev_str_defualt;
    struct thread_param_t thread_param;
    // int32_t perf_count = 1;
    // int32_t md5_check = 0;
    // int32_t save_yuv = 1;
    int frame_count = 0;
    struct timeval tBegin, tEnd;

    option = malloc(sizeof(option_t));
    option->loop = 1;
    option->md5 = 1;
    option->save = 1;
    option->decDevice = "/dev/hantrodec";
    option->memDevice = "/dev/memalloc";
    option->output = "output.yuv";

    ret = parse_options(args->argc, args->argv, option);
    if (ret < 0) {
        usage(args->argv[0]);
        if (ret != -2)
            LOG_ERROR("Failed to parse the input arguments!");
        return -1;
    }
    if (option->input == NULL) {
        LOG_ERROR("Please specify the input JPEG file!");
        usage(args->argv[0]);
        return -1;
    }

    LOG_INFO("render video decoder device name: %s", option->decDevice);
    LOG_INFO("render memory device name: %s", option->memDevice);
    LOG_INFO("perf_count: %d", option->loop);
    LOG_INFO("md5_check: %d", option->md5);
    LOG_INFO("save_yuv: %d", option->save);
    LOG_INFO("output file: %s\n", option->output);

    md5ctx_inited = 0;
    memset(&md5ctx, 0, sizeof(struct md5_context));

    vmppConfiguration cfg;
    memset(&cfg, 0, sizeof(vmppConfiguration));

    ret = vmppInitDecoder(&cfg);
    if (ret != vmpp_RSLT_OK) {
        LOG_ERROR("vmppInitDecoder failed %d", ret);
        return -1;
    }

    ch_apr.decDevice = option->decDevice;
    ch_apr.memDevice = option->memDevice;
    ch_apr.codecType = vmpp_CODEC_DEC_JPEG;
    ch_apr.sourceMode = vmpp_SRC_FRAME;
    ch_apr.decodeMode = vmpp_DEC_NORMAL;
    ch_apr.maxWidth = 32768;
    ch_apr.maxHeight = 32768;
    ch_apr.streamBufferSize = MAX_STREAM_SIZE_4_JPEG;
    ch_apr.extraBufferNumber = 2; // 0, 1, 2
    ch_apr.pixelFormat = vmpp_PIX_FMT_NV12;
    //ch_apr.memoryMode = vmpp_DEC_MEM_USER_AS_HWOUT;
    //ch_apr.cropInfo.flag = vmpp_CROP_ENABLE;

    ch_apr.enProfiling = 1;

    ret = vmppDecCreateChannel(&dec_ch, &ch_apr);
    if (ret != vmpp_RSLT_OK || !dec_ch) {
        LOG_ERROR("create channel error %d or chn is null.", ret);
        return -1;
    }

    ret = vmppDecStart(dec_ch); // set start status.
    LOG_INFO("[APP][%p]vmppDecStart, ret: %d", dec_ch, ret);
    if (ret < 0) {
        LOG_ERROR("start recv stream error %d.", ret);
        return -1;
    }

    thread_param.dec_ch = dec_ch;
    thread_param.output_file_str = option->output;
    thread_param.md5_check = option->md5;
    thread_param.save_yuv = option->save;
    task = run_task(output_thread, &thread_param);

    input_buf = malloc(input_len);

#if 1
    stream_p = get_stream(option->input, input_buf, input_len, &stream_len);
#else
    FILE* fp = fopen(option->input, "rb");
    stream_len = fread(input_buf, sizeof(uint8_t), input_len, fp);
    fclose(fp);
    stream_p = input_buf;
#endif

    gettimeofday(&tBegin, NULL);
    do {
        timer_trigger(1);
#if 0
        if (frame_count % 2 == 0)
            //stream_p = get_stream("/home/vastai/resource/dataset/jpg/16K/126M.jpg", input_buf, input_len, &stream_len);
            stream_p = get_stream("/home/vastai/resource/dataset/jpg/1080P/smile_1920x1080.jpg", input_buf, input_len, &stream_len);
        else
            //stream_p = get_stream("/home/vastai/resource/dataset/jpg/16K/16384_part1_16382x16382.jpg", input_buf, input_len, &stream_len);
            stream_p = get_stream("/home/vastai/resource/dataset/jpg/1080P/duck_1920x1080.jpg", input_buf, input_len, &stream_len);
#endif

        input_stream.stream = stream_p;
        input_stream.len = stream_len;
        // fake device address, test for vmpp_DEC_MEM_USER_AS_HWOUT
        if (ch_apr.memoryMode == vmpp_DEC_MEM_USER_AS_HWOUT) {
            input_stream.outputBusAddress[0] = 0x900000000;// only support set luma addr
            input_stream.outputBusAddress[1] = 0x900300000;
            input_stream.outputBusAddress[2] = 0;
        }
        LOG_INFO("[APP][%p]vmppDecSendStream (%d)", dec_ch, frame_count);
        ret = vmppDecSendStream(dec_ch, &input_stream, 4000);
        gettimeofday(&tEnd, NULL);
        LOG_INFO("[APP][%p]vmppDecSendStream (%d), duration: %ld us, ret: %d", dec_ch, frame_count,
                 1000000L * (tEnd.tv_sec - tBegin.tv_sec) + (tEnd.tv_usec - tBegin.tv_usec), ret);

        if (ret < 0) {
            LOG_ERROR("send stream error %d.", ret);
            if ( ret != vmpp_RSLT_ERR_NO_PTSBUF) {
                g_output_stop = 1;
                return -1;
            }
        }
        timer_trigger(0);
#ifdef PRINT_JPEG_INFO
        ret = vmppDecGetJpegInfo(&input_stream, &info);
        if (ret == vmpp_RSLT_OK) {
            LOG_INFO("[APP]vmppDecGetJpegInfo, size %d x %d, format %d, coding mode %d, x density "
                     "%d, y density %d.",
                     info.width, info.height, info.outputFormat, info.codingMode, info.xDensity,
                     info.yDensity);
        }
#endif
        frame_count++;
        // sleep(10);
    } while (option->loop == 0 || frame_count < option->loop);

    ret = vmppDecStop(dec_ch); // disable start status.
    LOG_INFO("[APP][%p]vmppDecStop, ret: %d", dec_ch, ret);
    if (ret < 0) {
        LOG_ERROR("stop recv stream error %d.", ret);
        return -1;
    }

    /* Give the output thread a chance to drain the already decoded pictures
     * before telling it that no more frames will come. */
    usleep(200 * 1000);
    g_output_stop = 1;

    LOG_INFO("[APP][%p]waiting output thread  ...", dec_ch);
    pthread_join(*((pthread_t *)task), NULL);

    free(task);
    free(input_buf);

    dec_ch1 = dec_ch;

    ret = vmppDecDestroyChannel(&dec_ch);
    LOG_INFO("[APP][%p]vmppDecDestroyChannel, ret: %d", dec_ch1, ret);
    if (ret < 0) {
        LOG_ERROR("destroy chn error %d.", ret);
        return -1;
    }

    return 0;
}

RET_TYPE main(int argc, char *argv[])
{
    MainArgs args = {argc, argv};
    return MainTask(&args);
}
