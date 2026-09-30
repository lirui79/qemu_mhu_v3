#include <assert.h>
#include <ctype.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
// #include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#include "defs.h"
#include "ffmpeg-wrapper.h"
#include "md5.h"
#include "utils.h"
#ifdef __cplusplus
}
#endif /* __cplusplus */

#include "vmpp_enc_api.h"
#include "vmpp_enc_defs.h"

#define LIB_VACCRT_PATH "libvaccrt.so"
#define MD5_BUFSIZE 1024 * 16
#define THREAD_SIZE 100

// endof  the va_dec.h

// static char *default_dev_str = (char *)"/dev/vastai_video0";
typedef int RET_TYPE;
typedef void *task_handle;
typedef void *(*task_func)(void *);

struct thread_ctx_t {
    uint32_t thread_id;
    FILE *output_file;
    int recv_num;
    int md5ctx_inited;
    struct md5_context md5ctx;
    struct timeval tVeryBegin;
    struct timeval tBegin;
    struct timeval tEnd;
}g_thread_ctx[THREAD_SIZE];

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
    int md5;
    int store;
    int thread_num;
    int rotation;
} enc_options;

enc_options g_options;

static enum vmppPixelFormat get_pixel_format(const char *pf)
{
    enum vmppPixelFormat pixel_format = vmpp_PIX_FMT_NONE;
    if (!strcmp(pf, "nv12")) {
        pixel_format = vmpp_PIX_FMT_NV12;
    } else if (!strcmp(pf, "nv21")) {
        pixel_format = vmpp_PIX_FMT_NV21;
    } else if (!strcmp(pf, "yuv420p")) {
        pixel_format = vmpp_PIX_FMT_YUV420P;
    } else {
        pixel_format = vmpp_PIX_FMT_NONE;
    }
    return pixel_format;
}

static void usage(const char *program)
{
    fprintf(stderr, "\nUsage: %s [options]\n", program);
    fprintf(stderr, "  -i    url for input file\n");
    fprintf(stderr, "  -o    url for output folder\n");
    fprintf(stderr, "  -w    width\n");
    fprintf(stderr, "  -h    height\n");
    fprintf(stderr, "  -t    stride\n");
    fprintf(stderr, "  -d    device name\n");
    fprintf(stderr, "  -f    pixel format\n");
    fprintf(stderr, "  -l    loop time, if loop = 0, it will loop forever.\n");
    fprintf(stderr, "  -m    check md5 for decoder & encode. \n");
    fprintf(stderr, "  -s    stored in output file or not, 0/1.\n");
    fprintf(stderr, "  -r    rotate input image, 0:3.\n");
    fprintf(stderr, "  -T    thread number, max is 100.\n");
    fprintf(stderr, "Example:\n");
    fprintf(stderr,
        "jpeg_enc_mt -i input.file -d /dev/vastai_video0 -w 1920 -h 1080 -f nv12 -o output -l 10 -s 0 -m 1 -T 100\n");
    fprintf(stderr, "\n");
}

static int parse_options(int argc, char **argv, enc_options *options)
{
    // jpeg_enc -i input.file -d /dev/dri/renderD128 -w 1920 -h 1080 -f nv12 -o output -l 100
    static const char optstr[] = "i:o:w:h:t:d:f:l:s:m:T:";
    int c;
    while ((c = getopt(argc, argv, optstr)) != -1) {
        switch (c) {
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
        case 'd':
            options->device = optarg;
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
        case 'm':
            options->md5 = atoi(optarg);
            break;
        case 'T':
            options->thread_num = atoi(optarg);
            break;
        case 'r':
            options->rotation = atoi(optarg);
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

static unsigned char *compute_md5sum(const unsigned char *buf_in, const int buf_len,
                              unsigned char *str_md5sum, uint32_t thread_id)
{
    int to_final = 0;
    struct thread_ctx_t *ctx = &g_thread_ctx[thread_id];

    if (buf_in == NULL || buf_len == 0) {
        to_final = 1;
    }

    /* Initialization of MD5 context. */
    if (ctx->md5ctx_inited == 0) {
        md5_init(&ctx->md5ctx);
        ctx->md5ctx_inited = 1;
    }

    if (to_final == 0) {
        md5_update(&ctx->md5ctx, buf_in, (unsigned long)buf_len);
    } else {
        md5_final(str_md5sum, &ctx->md5ctx);
        ctx->md5ctx_inited = 0;
    }
    return str_md5sum;
}

static int read_input_data(enc_options *options, vmppFrame *frame)
{
    int ret = 0;
    FILE *fin = NULL;
    uint32_t luma_size, chroma_size_cb, chroma_size_cr, pic_size;

    frame->width = options->width;
    frame->height = options->height;
    frame->stride[0] = options->stride;
    frame->memoryType = vmpp_MEM_HOST;
    frame->pixelFormat = options->pixel_format;

    switch (frame->pixelFormat) {
    case vmpp_PIX_FMT_YUV420P:
        luma_size = frame->stride[0] * frame->height;
        if (frame->stride[0] % 2 == 1) {
            frame->stride[1] = (frame->stride[0] + 1) / 2;
            frame->stride[2] = (frame->stride[0] + 1) / 2;
        } else {
            frame->stride[1] = frame->stride[0] / 2;
            frame->stride[2] = frame->stride[0] / 2;
        }
        if ( frame->height % 2 == 1) {
            chroma_size_cb = frame->stride[1] * (frame->height + 1) / 2;
            chroma_size_cr = frame->stride[2] * (frame->height + 1) / 2;
        } else {
            chroma_size_cb = frame->stride[1] * frame->height / 2;
            chroma_size_cr = frame->stride[2] * frame->height / 2;
        }
        break;
    case vmpp_PIX_FMT_NV12:
    case vmpp_PIX_FMT_NV21:
        luma_size = frame->stride[0] * frame->height;
        frame->stride[1] = frame->stride[0];
        if (frame->stride[1] % 2 == 1){
            frame->stride[1] ++;
        }
        if ( frame->height % 2 == 1)
            chroma_size_cb = frame->stride[1] * (frame->height + 1) / 2;
        else
            chroma_size_cb = frame->stride[1] * frame->height / 2;
        chroma_size_cr = 0;
        break;
    default:
        luma_size = 0;
        chroma_size_cb = chroma_size_cr = 0;
        break;
    }

    pic_size = luma_size + chroma_size_cb + chroma_size_cr;

    frame->data[0] = (uint8_t *)malloc(pic_size);
    if (frame->data[0] == NULL) {
        fprintf(stderr, "Failed to malloc frame memory!\n");
        return -1;
    }
    frame->data[1] = frame->data[0] + luma_size;

    if (chroma_size_cr == 0) {
        frame->data[2] = NULL;
    } else {
        frame->data[2] = frame->data[1] + chroma_size_cb;
    }

    frame->dataSize = pic_size;

    if (!(fin = fopen(options->input_file, "r"))) {
        fprintf(stderr, "Failed to open input file : %s.\n", options->input_file);
        return -2;
    }

    ret = fread(frame->data[0], 1, luma_size, fin);
    if (ret <= 0) {
        if (!feof(fin)) {
            fprintf(stderr, "Failed to read Y to the frame data[0]. pixel format %d.\n",
                    options->pixel_format);
        }
        return -2;
    }

    ret = fread(frame->data[1], 1, chroma_size_cb, fin);
    if (ret <= 0) {
        if (!feof(fin)) {
            fprintf(stderr, "Failed to read UV to the frame data[1]. pixel format %d.\n",
                    options->pixel_format);
        }
        return -2;
    }

    if (chroma_size_cr != 0) {
        ret = fread(frame->data[2], 1, chroma_size_cr, fin);
        if (ret <= 0) {
            if (!feof(fin)) {
                fprintf(stderr, "Failed to read UV to the frame data[2]. pixel format %d.\n",
                        options->pixel_format);
            }
            return -2;
        }
    }

    if (fin)
        fclose(fin);

    return 0;
}

static task_handle run_task(task_func func, void *param)
{
    int ret;
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

#define SAVE_MULTI_FILE_TEST

static void handle_output(vmppChannel *enc_ch, enc_options *options, vmppStream *out_stream,
                          vmppResult result, uint32_t thread_id)
{
    unsigned char str_md5sum[MD5_HASH_LEN];
    int ret = 0;
    struct thread_ctx_t *ctx = &g_thread_ctx[thread_id];
    char file_name[MAX_PATH_LEN*2] = {0};

    if (options->store) {

    #ifdef SAVE_MULTI_FILE_TEST
        
        if (options->loop == 1){
            std::string output_name= ((std::string)options->output_file).substr(0, ((std::string)options->output_file).find_last_of("."));
            sprintf(file_name, "%s_%d.jpg", output_name.c_str(), thread_id);
        } else{
            std::string output_name= ((std::string)options->output_file).substr(0, ((std::string)options->output_file).find_last_of("."));
            sprintf(file_name, "%s_th%d_%d.jpg", output_name.c_str(), thread_id, ctx->recv_num % options->loop);
        }

        if (result == vmpp_RSLT_OK) {
            if (ctx->output_file)
            fclose(ctx->output_file);
        ctx->output_file = fopen(file_name, "wb");
        if (!ctx->output_file) {
            fprintf(stderr, "open output file %s error.\n", file_name);
                return;
            }
        }
    #else
        if (ctx->output_file == NULL) {
            ctx->output_file = fopen(options->output_file, "wb");
            if (!ctx->output_file) {
                fprintf(stderr, "open output file %s error.\n", options->output_file);
                return;
            } else
                printf("[APP][%p]open YUV file to write: %s\n", enc_ch, options->output_file);
        }
    #endif
    }

    do {
        if (result == vmpp_RSLT_OK) {
            if (options->md5) {
                /* Compute md5 */
                compute_md5sum((const unsigned char *)out_stream->stream, out_stream->len,
                               str_md5sum, thread_id);
                compute_md5sum(NULL, 0, str_md5sum, thread_id);
                print_md5((uint8_t *)str_md5sum, MD5_HASH_LEN);
            }

            if (options->store) {
                /* Store encode data into output file */
                ret = fwrite(out_stream->stream, 1, out_stream->len, ctx->output_file);
                printf("[APP][%p][%d]write %d#, file: %s\n", enc_ch, thread_id, ctx->recv_num, file_name);
            }

            /* Cal encode performance */
            gettimeofday(&ctx->tEnd, NULL);
            long deltaTime =
                1000000L * (ctx->tEnd.tv_sec - ctx->tBegin.tv_sec) + (ctx->tEnd.tv_usec - ctx->tBegin.tv_usec);
            float frame_rate = 1.0 / deltaTime * 1000000;
            long deltaTimeOverall = 1000000L * (ctx->tEnd.tv_sec - ctx->tVeryBegin.tv_sec) + (ctx->tEnd.tv_usec - ctx->tVeryBegin.tv_usec);
            float frame_rate_avg = (float)(ctx->recv_num + 1) / deltaTimeOverall * 1000000;
            fprintf(stdout, "[APP][%p][%d]recv %d#, len %d, spend %ld us, curr %.2ffps, avg %.2ffps.\n",
                    enc_ch, ctx->thread_id, ctx->recv_num, out_stream->len, deltaTime, frame_rate, frame_rate_avg);

            ctx->recv_num++;
        } else if (result == vmpp_RSLT_WARN_EOS) {
            if (options->store) {
                if (ctx->output_file) {
                    fclose(ctx->output_file);
                    ctx->output_file = NULL;
                    // printf("file closed!\n");
                }
            }
        } else {
            fprintf(stderr, "receive stream error %d.\n", ret);
        }
    } while (0);

    return;
}

static void *MainTask(void *thread_args)
{
    int32_t ret = -1;
    struct thread_ctx_t *ctx  = (struct thread_ctx_t *) thread_args;
    enc_options *options = &g_options;

    vmppChannel enc_ch;
    vmppEncChannelParameters ch_apr;
    vmppResult enc_ret = vmpp_RSLT_OK;

    int frame_count = 0;

    memset(&ch_apr, 0, sizeof(ch_apr));

    int enc_fd = open(options->device, O_RDWR);
    if (enc_fd < 0) {
        printf("Cannot open DRM render node for device %s\n", options->device);
       goto end;
    }

    ch_apr.device = (vmppDevice)enc_fd;
    ch_apr.codecType = vmpp_CODEC_ENC_JPEG;
    ch_apr.jpegConfig.frameType = options->pixel_format;
    ch_apr.jpegConfig.codingWidth = options->width;
    ch_apr.jpegConfig.codingHeight = options->height;
    ch_apr.jpegConfig.comLength = 7;
    ch_apr.jpegConfig.pCom = (uint8_t *)"vastai";
    ch_apr.jpegConfig.preProcess.rotation = (vmppEncPictureRotation)options->rotation;

    ch_apr.enProfiling = 1;

    vmppFrame frame;
    memset(&frame, 0, sizeof(frame));
    ret = read_input_data(options, &frame);

    if (ret < 0) {
        goto end;
    }

    vmppStream stream;
    memset(&stream, 0, sizeof(stream));

    enc_ret = vmppEncCreateChannel(&enc_ch, &ch_apr);
    if (enc_ret != vmpp_RSLT_OK) {
        printf("%s:%d send frame error %d.\n", __func__, __LINE__, enc_ret);
        goto end;
    }

    gettimeofday(&ctx->tVeryBegin, NULL);

    do {
        gettimeofday(&ctx->tBegin, NULL);

        enc_ret = vmppEncEncodeFrame(enc_ch, &frame, NULL, &stream, 4000);
        printf("[APP][%p][%d]send %d#, size %dx%d, ret %d\n", enc_ch, ctx->thread_id, frame_count, options->width, options->height, enc_ret);

        handle_output(&enc_ch, options, &stream, enc_ret, ctx->thread_id);

        enc_ret = vmppEncReleaseStream(enc_ch, &stream);

        if (enc_ret < 0) {
            printf("%s:%d send frame error %d.\n", __func__, __LINE__, enc_ret);
            goto end;
        }
        frame_count++;
    } while (options->loop == 0 || frame_count < options->loop);

    // flush
    frame.memoryType = vmpp_MEM_FLUSH;
    enc_ret = vmppEncEncodeFrame(enc_ch, &frame, NULL, &stream, 4000);
    //printf("[APP][%p]vmppEncEncodeFrame, Done: %d\n", enc_ch, enc_ret);

    handle_output(&enc_ch, options, &stream, enc_ret, ctx->thread_id);

    enc_ret = vmppEncDestroyChannel(&enc_ch);
    if (enc_ret < 0) {
        fprintf(stderr, "destroy chn error %d.\n", ret);
        goto end;
    }

end:
    if (frame.data[0])
        free(frame.data[0]);

    if (enc_fd != -1) {
        close(enc_fd);
    }

    return 0;
}

static void print_enc_options(const enc_options *opts) {
    if (opts == NULL) {
        printf("Options pointer is NULL.\n");
        return;
    }
    printf("input_file: %s\n", opts->input_file ? opts->input_file : "(null)");
    printf("output_file: %s\n", opts->output_file ? opts->output_file : "(null)");
    printf("width: %d\n", opts->width);
    printf("height: %d\n", opts->height);
    printf("stride: %d\n", opts->stride);
    printf("device: %s\n", opts->device ? opts->device : "(null)");
    printf("pixel_format: %d\n", opts->pixel_format); // 打印枚举的整数值
    printf("pixel_fmt: %s\n", opts->pixel_fmt ? opts->pixel_fmt : "(null)");
    printf("loop: %d\n", opts->loop);
    printf("md5: %d\n", opts->md5);
    printf("store: %d\n", opts->store);
    printf("thread_num: %d\n", opts->thread_num);
    printf("rotation: %d\n", opts->rotation);
}

int jpeg_enc_multi_thread_catch2(enc_options *params_ut)
{
    bool ret_end = 0;
    int32_t ret = -1;
    char out_file[100];
    vmppRuntimeInstance rt_inst;
    memset(&rt_inst, 0, sizeof(rt_inst));

    memset(&g_thread_ctx, 0, sizeof(g_thread_ctx));

    enc_options *options = &g_options;
    memset(options, 0, sizeof(enc_options));

    memcpy(options, params_ut, sizeof(enc_options));

    if (options->output_file != NULL) {
        std::string input_name = ((std::string)params_ut->input_file).substr(((std::string)params_ut->input_file).find_last_of("/"));
        std::string tm = (std::string)params_ut->output_file + input_name.substr(0, input_name.find_last_of("."));
        snprintf(out_file, 100, "%s_rotate%d.jpg", tm.c_str(), options->rotation);
        out_file[100 - 1] = '\0'; 
        options->output_file = out_file;
    }

    int dieId = 0;
    vmppConfiguration cfg;
    task_handle task[THREAD_SIZE];
    rtError_t vaccRet;
    vaccrt_init_t vaccrt_init;

    if (options->stride == 0) {
        options->stride = options->width;
    }
    print_enc_options(options);

    open_runtime(&rt_inst); // run time init

    vaccrt_init = (vaccrt_init_t)(&rt_inst)->init;
    sscanf(options->device, "/dev/vastai_video%d", &dieId);
    vaccRet = vaccrt_init(dieId);
    if (vaccRet) {
        printf("vaccrt_init failed: err %d\n", vaccRet);
        ret_end = 1;
        goto end;
    }

    memset(&cfg, 0, sizeof(vmppConfiguration));
    cfg.runtimeInst = rt_inst;
    cfg.logCtx.enableCustomLog = 1;
    cfg.logCtx.logLevel = vmpp_LOG_INFO;

    ret = vmppInitEncoder(&cfg);
    if (ret != vmpp_RSLT_OK) {
        printf("vmppInitEncoder failed %d\n", ret);
        ret_end = 1;
        goto end;
    }

    for (int i = 0; i < options->thread_num; i++) {
        struct thread_ctx_t* ctx = &g_thread_ctx[i];
        ctx->thread_id = i;
        task[i] = run_task(MainTask, ctx);
        printf("------ run_task %d#\n", i);
    }

    for (int i = 0; i < options->thread_num; i++) {
        pthread_join(*((pthread_t *)task[i]), NULL);
        printf("------ pthread_join %d#\n", i);
    }

    for (int i = 0; i < options->thread_num; i++) {
        free(task[i]);
    }

end:
    if (rt_inst.runtimeHandle) {
        close_runtime(&rt_inst);
    }

    if (ret_end == 1)
        return -1;
    return 0;
}

// int main(int argc, char *argv[]) {
//     int32_t ret = -1;
//     enc_options *options = (enc_options *)malloc(sizeof(enc_options));

//     ret = parse_options(argc, argv, options);
//     if (ret < 0) {
//         fprintf(stderr, "Failed to parse oargs.\n");
//         return -1;
//     }
// }
