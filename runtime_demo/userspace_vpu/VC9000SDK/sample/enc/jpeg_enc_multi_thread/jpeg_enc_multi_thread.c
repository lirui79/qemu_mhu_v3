#include <assert.h>
#include <ctype.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "defs.h"
#include "ffmpeg-wrapper.h"
#include "md5.h"
#include "utils.h"
#include "vmpp_enc_api.h"
#include "vmpp_enc_defs.h"

#define MD5_BUFSIZE (1024 * 16)
#define THREAD_SIZE (24)

// endof  the va_dec.h

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
    char*                           encDevice;                  // video encoder device node name
    char*                           memDevice;                  // memory device node name
    char *input_file;
    char *output_file;
    int width;
    int height;
    int stride;
    enum vmppPixelFormat pixel_format;
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
    fprintf(stderr, "  -e    video encoder device name\n");
    fprintf(stderr, "  -m    memory device  name\n");
    fprintf(stderr, "  -f    pixel format\n");
    fprintf(stderr, "  -l    loop time, if loop = 0, it will loop forever.\n");
    fprintf(stderr, "  -d    check md5 for decoder & encode. \n");
    fprintf(stderr, "  -s    stored in output file or not, 0/1.\n");
    fprintf(stderr, "  -r    rotate input image, 0:3.\n");
    fprintf(stderr, "  -T    thread number, max is 24.\n");
    fprintf(stderr, "Example:\n");
    fprintf(stderr,
        "./jpeg_enc_mt -i /home/stone/workspace/YUV/sintel_trailer_1920x1080p_1253.yuv -e /dev/hantroenc -m /dev/memalloc -w 1920 -h 1080 -f yuv420p -o output -l 1 -s 0 -d 1 -T 24\n");
    fprintf(stderr, "\n");
}

static int parse_options(int argc, char **argv, enc_options *options)
{
    // jpeg_enc -i input.file -d /dev/dri/renderD128 -w 1920 -h 1080 -f nv12 -o output -l 32
    static const char optstr[] = "i:o:w:h:t:e:m:f:l:s:d:T:r:";
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
        case 'e':
            options->encDevice = optarg;
            break;
        case 'm':
            options->memDevice = optarg;
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
        case 'd':
            options->md5 = atoi(optarg);
            break;
        case 'T':
            options->thread_num = atoi(optarg);
            if (options->thread_num > THREAD_SIZE || options->thread_num <= 0) {
                fprintf(stderr, "thread_num must be in [1, %d]\n", THREAD_SIZE);
                return -1;
            }
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

unsigned char *compute_md5sum(const unsigned char *buf_in, const int buf_len,
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

static int read_input_data(enc_options *options, vmppChannel chn, vmppFrame *frame)
{
    vmppResult enc_ret = vmpp_RSLT_OK;
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
/*
    frame->data[0] = malloc(pic_size);
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
*/
    enc_ret = vmppEncAllocFrame(chn, frame);
    if (enc_ret != vmpp_RSLT_OK) {
        fprintf(stderr, "Failed to allocate frame memory!\n");
        return -1;
    }

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
    char output_name[MAX_PATH_LEN] = {0};
    // strncpy(output_name, options->output_file, strlen(options->output_file)-4);
    safe_strncpy(output_name, options->output_file, MAX_PATH_LEN, strlen(options->output_file) - 4);
    //printf("output_name[MAX_PATH_LEN]:================== %s, len %d\n", output_name, (int)strlen(output_name));
    if (options->loop == 1){
        sprintf(file_name, "%s_%d.jpg", output_name, thread_id);
    } else{
        sprintf(file_name, "%s_th%d_%d.jpg", output_name, thread_id, ctx->recv_num % options->loop);
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
                compute_md5sum((const unsigned char *)out_stream->stream, out_stream->len, str_md5sum, thread_id);
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
    vmppEncChannelParameters enc_ch_params;
    vmppResult enc_ret = vmpp_RSLT_OK;

    int frame_count = 0;

    memset(&enc_ch_params, 0, sizeof(enc_ch_params));
    
    enc_ch_params.encDevice   = options->encDevice;
    enc_ch_params.memDevice   = options->memDevice;
    enc_ch_params.codecType = vmpp_CODEC_ENC_JPEG;
    enc_ch_params.jpegConfig.frameType = options->pixel_format;
    enc_ch_params.jpegConfig.codingWidth = options->width;
    enc_ch_params.jpegConfig.codingHeight = options->height;
//    enc_ch_params.jpegConfig.comLength = 7;
//    enc_ch_params.jpegConfig.pCom = (uint8_t *)"vastai";
    enc_ch_params.jpegConfig.preProcess.rotation = options->rotation;

    enc_ch_params.enProfiling = 1;

    /* The channel has to exist before the input frame is allocated:
     * read_input_data() allocates the frame through vmppEncAllocFrame(chn). */
    enc_ret = vmppEncCreateChannel(&enc_ch, &enc_ch_params);
    if (enc_ret != vmpp_RSLT_OK) {
        printf("%s:%d create channel error %d.\n", __func__, __LINE__, enc_ret);
        goto end;
    }

    vmppFrame frame;
    memset(&frame, 0, sizeof(frame));

    ret = read_input_data(options, enc_ch, &frame);

    if (ret < 0) {
        vmppEncDestroyChannel(&enc_ch);
        goto end;
    }

    vmppStream stream;
    memset(&stream, 0, sizeof(stream));

    gettimeofday(&ctx->tVeryBegin, NULL);

    do {
        gettimeofday(&ctx->tBegin, NULL);

        enc_ret = vmppEncEncodeFrame(enc_ch, &frame, NULL, &stream, 4000);
        printf("[APP][%p][%d]send %d#, size %dx%d, ret %d\n", enc_ch, ctx->thread_id, frame_count, options->width, options->height, enc_ret);

        handle_output(enc_ch, options, &stream, enc_ret, ctx->thread_id);

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

    handle_output(enc_ch, options, &stream, enc_ret, ctx->thread_id);

    enc_ret = vmppEncDestroyChannel(&enc_ch);
    if (enc_ret < 0) {
        fprintf(stderr, "destroy chn error %d.\n", ret);
        goto end;
    }

end:
 //   if (frame.data[0])
 //       free(frame.data[0]);

    return 0;
}

int main(int argc, char *argv[])
{
    int32_t ret = -1;

    memset(&g_thread_ctx, 0, sizeof(g_thread_ctx));

    enc_options *options = &g_options;
    memset(options, 0, sizeof(enc_options));

    // set default values
    options->input_file = "/home/vastai/resource/dataset/yuv/480P/smile_640x480_0.yuv";
    options->output_file = "test.jpg";
    options->width = 640;
    options->height = 480;
    options->stride = 0;
    options->encDevice = "/dev/hantroenc";
    options->memDevice = "/dev/memalloc";
    options->pixel_format = vmpp_PIX_FMT_NV12;
    options->loop = 1;
    options->md5 = 1;
    options->store = 1;
    options->thread_num = 2;
    options->rotation = 0;

    ret = parse_options(argc, argv, options);
    if (ret < 0) {
        fprintf(stderr, "Failed to parse oargs.\n");
        goto end;
    }

    if (options->stride == 0) {
        options->stride = options->width;
    }

    vmppConfiguration cfg;
    memset(&cfg, 0, sizeof(vmppConfiguration));
    cfg.logCtx.enableCustomLog = 1;
    cfg.logCtx.logLevel = vmpp_LOG_INFO;

    ret = vmppInitEncoder(&cfg);
    if (ret != vmpp_RSLT_OK) {
        printf("vmppInitEncoder failed %d\n", ret);
        goto end;
    }

    task_handle task[THREAD_SIZE];

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
    return 0;
}
