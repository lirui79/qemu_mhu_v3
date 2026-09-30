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

#define MD5_BUFSIZE 1024 * 16


typedef int RET_TYPE;
typedef void *task_handle;
typedef void *(*task_func)(void *);
static struct timeval tBegin, tEnd;

int md5ctx_inited = 0;
struct md5_context md5ctx = {0};

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
    int rotation;
    int qLevel;
    int lossless;
    int predictMode;
    int ptransValue;
} enc_options;

typedef struct {
    void *enc_ch;
    enc_options *options;
} thread_param_t;

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
    fprintf(stderr, "  -f    pixel format: nv12/nv21/yuv420p\n");
    fprintf(stderr, "  -l    loop time, if loop = 0, it will loop forever.\n");
    fprintf(stderr, "  -d    check md5 for decoder & encode. \n");
    fprintf(stderr, "  -s    stored in output file or not, 0/1 \n");
    fprintf(stderr, "  -r    rotate input image, 0:not rotate/1:right 90 degrees/2:left 90 degrees/3:180 degrees. \n");
    fprintf(stderr, "  -q    quantization level (1 - 101).\n");
    fprintf(stderr, "  -L    enable lossless coding, 0/1, default 0.\n");
    fprintf(stderr, "  -p    prediction mode of lossless coding (1 - 7), only used when -L 1.\n");
    fprintf(stderr, "  -P    point transform of lossless coding (0 - 15), only used when -L 1.\n");
    fprintf(stderr, "Example:\n");
    fprintf(stderr, "./jpeg_enc -i /home/stone/workspace/YUV/akiyo_352x288_300.yuv -o output.jpeg -w 352 -h 288 -t 352 -e /dev/hantroenc -m /dev/memalloc -f yuv420p -l 1 -s 0 -d 0"
                    "\n");
    fprintf(stderr, "\n");
}

static int parse_options(int argc, char **argv, enc_options *options)
{
    // jpeg_enc -i input.file -d /dev/dri/renderD128 -w 1920 -h 1080 -f nv12 -o output -l 100
    static const char optstr[] = "i:o:w:h:t:e:m:f:l:s:d:r:q:L:p:P:";
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
        case 'r':
            options->rotation = atoi(optarg);
            break;
        case 'q':
            options->qLevel = atoi(optarg);
            break;
        case 'L':
            options->lossless = atoi(optarg);
            break;
        case 'p':
            options->predictMode = atoi(optarg);
            break;
        case 'P':
            options->ptransValue = atoi(optarg);
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
        decode_count = 0;
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
        printf("frame: %ld, curr: %ld us, avg: %ld us\n", decode_count, deltaTime, avg_time);
        printf("frame: %ld, curr: %.2f fps, avg: %.2f fps\n", decode_count,
               1. / deltaTime * 1000000, 1. / avg_time * 1000000);
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

static int read_input_data(enc_options *options, vmppChannel chn, vmppFrame *frame)
{
    int ret = 0;
    vmppResult enc_ret = vmpp_RSLT_OK;
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

    if (luma_size == 0 || chroma_size_cb == 0) {
        fprintf(stderr,
                "Invalid input geometry: pixel format %d, width %u, height %u, stride %u, "
                "luma size %u, chroma size %u.\n",
                frame->pixelFormat, frame->width, frame->height, frame->stride[0], luma_size,
                chroma_size_cb);
        return -1;
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
            fprintf(stderr,
                    "Failed to read %u bytes of Y into frame->data[0] (%p): ret %d, ferror %d, "
                    "pixel format %d.\n",
                    luma_size, frame->data[0], ret, ferror(fin), frame->pixelFormat);
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

#define SAVE_MULTI_FILE_TEST

static void handle_output(vmppChannel *enc_ch, enc_options *options, vmppStream *out_stream,
                          vmppResult result)
{
    static FILE *output_file;
    unsigned char str_md5sum[MD5_HASH_LEN];
    int ret = 0;

#ifdef SAVE_MULTI_FILE_TEST
    static int file_idx = 0;
    char file_name[MAX_PATH_LEN];

    if (options->loop == 1)
        sprintf(file_name, "%s", options->output_file);
    else
        sprintf(file_name, "%s_%d.jpg", options->output_file, file_idx % 3);
    file_idx++;

    if (result == vmpp_RSLT_OK) {
        if (output_file)
            fclose(output_file);
        output_file = fopen(file_name, "wb");
        if (!output_file) {
            fprintf(stderr, "open output file %s error.\n", options->output_file);
            return;
        } else
            printf("[APP][%p]open output file to write: %s\n", enc_ch, options->output_file);
    }
#else
    if (output_file == NULL) {
        output_file = fopen(options->output_file, "wb");
        if (!output_file) {
            fprintf(stderr, "open output file %s error.\n", options->output_file);
            return;
        } else
            printf("[APP][%p]open YUV file to write: %s\n", enc_ch, options->output_file);
    }
#endif

    do {
        if (result == vmpp_RSLT_OK) {
            if (options->md5) {
                /* Compute md5 */
                compute_md5sum((const unsigned char *)out_stream->stream, out_stream->len,
                               str_md5sum);
                compute_md5sum(NULL, 0, str_md5sum);
                print_md5((uint8_t *)str_md5sum, MD5_HASH_LEN);
            }

            if (options->store) {
                /* Store encode data into output file */
                ret = fwrite(out_stream->stream, 1, out_stream->len, output_file);
            }
            /* Cal encode performance */
            gettimeofday(&tEnd, NULL);
            long deltaTime =
                1000000L * (tEnd.tv_sec - tBegin.tv_sec) + (tEnd.tv_usec - tBegin.tv_usec);
            float frame_rate = (float)(1.0 / deltaTime * 1000000);
            fprintf(stdout, "[Performance] %s --> %s, size: %d, spend %ld us, %.2ffps.\n",
                    options->input_file, options->output_file, out_stream->len, deltaTime, frame_rate);
        } else if (result == vmpp_RSLT_WARN_EOS) {
            if (output_file) {
                fclose(output_file);
                output_file = NULL;
                // printf("file closed!\n");
            }
        } else {
            fprintf(stderr, "receive stream error %d.\n", ret);
        }
    } while (0);

    return;
}

int MainTask(MainArgs *args)
{
    int32_t ret = -1;

    vmppChannel ech;
    vmppEncChannelParameters eparams;
    vmppResult eret = vmpp_RSLT_OK;

    memset(&eparams, 0, sizeof(eparams));

    vmppFrame frame;
    memset(&frame, 0, sizeof(frame));

    int frame_count = 0;

    md5ctx_inited = 0;
    memset(&md5ctx, 0, sizeof(struct md5_context));

    enc_options *options = malloc(sizeof(enc_options));
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
    options->rotation = 0;
    options->qLevel = 50;
    options->lossless = 0;
    options->predictMode = 0;
    options->ptransValue = 0;

    ret = parse_options(args->argc, args->argv, options);
    if (ret < 0) {
        fprintf(stderr, "Failed to parse oargs.\n");
        goto end;
    }

    if (options->stride == 0) {
        options->stride = options->width;
    }

    vmppConfiguration cfg;
    memset(&cfg, 0, sizeof(vmppConfiguration));

    ret = vmppInitEncoder(&cfg);
    if (ret != vmpp_RSLT_OK) {
        printf("vmppInitEncoder failed %d\n", ret);
        goto end;
    }

    eparams.encDevice = options->encDevice;
    eparams.memDevice = options->memDevice;
    eparams.codecType = vmpp_CODEC_ENC_JPEG;
    eparams.jpegConfig.frameType = options->pixel_format;
    eparams.jpegConfig.codingWidth = options->width;
    eparams.jpegConfig.codingHeight = options->height;
//    eparams.jpegConfig.comLength = 7;
//    eparams.jpegConfig.pCom = (uint8_t *)"vastai";

    eparams.jpegConfig.losslessEn = options->lossless;
    eparams.jpegConfig.predictMode = options->predictMode;
    eparams.jpegConfig.ptransValue = options->ptransValue;

    eparams.jpegConfig.qLevel = options->qLevel;
    if (options->qLevel == 101) {
        const uint8_t qTable[64] = {2, 2, 2, 1, 1, 1, 1, 1,
                                    1, 1, 1, 1, 1, 1, 1, 1,
                                    1, 1, 1, 1, 1, 1, 1, 1,
                                    1, 1, 1, 1, 1, 1, 1, 1,
                                    1, 1, 1, 1, 1, 1, 1, 1,
                                    1, 1, 1, 1, 1, 1, 1, 1,
                                    1, 1, 1, 1, 1, 1, 1, 1,
                                    1, 1, 1, 1, 1, 1, 1, 1};

        for (int i = 0; i < 64; i++) {
            eparams.jpegConfig.qTableLuma[i] = qTable[i];
            eparams.jpegConfig.qTableChroma[i] = qTable[i];
        }
    }

    eparams.jpegConfig.preProcess.rotation = options->rotation;
    eparams.enProfiling = 1;

    eret = vmppEncCreateChannel(&ech, &eparams);
    if (eret != vmpp_RSLT_OK) {
        fprintf(stderr, "send frame error %d.\n", ret);
        goto end;
    }

    ret = read_input_data(options, ech, &frame);

    if (ret < 0) {
        goto end;
    }

    vmppStream stream;
    memset(&stream, 0, sizeof(stream));


    timer_trigger(1);
    do {
        gettimeofday(&tBegin, NULL);

        eret = vmppEncEncodeFrame(ech, &frame, NULL, &stream, 4000);
        printf("[APP][%p]vmppEncEncodeFrame %d#, Done: %d\n", ech, frame_count, eret);

        handle_output(ech, options, &stream, eret);

        eret = vmppEncReleaseStream(ech, &stream);

        if (eret < 0) {
            fprintf(stderr, "send frame error %d.\n", ret);
            goto end;
        }
        timer_trigger(0);
        frame_count++;
    } while (options->loop == 0 || frame_count < options->loop);

    // flush
    frame.memoryType = vmpp_MEM_FLUSH;
    eret = vmppEncEncodeFrame(ech, &frame, NULL, &stream, 4000);
    printf("[APP][%p]vmppEncEncodeFrame, Done: %d\n", ech, eret);

    handle_output(ech, options, &stream, eret);

    eret = vmppEncFreeFrame(ech, &frame);

    eret = vmppEncDestroyChannel(&ech);
    if (eret < 0) {
        fprintf(stderr, "destroy chn error %d.\n", ret);
        goto end;
    }

end:
//    if (frame.data[0])
//        free(frame.data[0]);

    if (options) {
        free(options);
        options = NULL;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    MainArgs args = {argc, argv};
    return MainTask(&args);
}
