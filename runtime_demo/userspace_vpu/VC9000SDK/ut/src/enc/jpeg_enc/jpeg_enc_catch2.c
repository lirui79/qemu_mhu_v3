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
#include <string>
#include <unistd.h>

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

// static char *default_dev_str = (char *)"/dev/vastai_video0";
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
    int rotation;
    int qLevel;
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

// static void usage(const char *program)
// {
//     fprintf(stderr, "\nUsage: %s [options]\n", program);
//     fprintf(stderr, "  -i    url for input file\n");
//     fprintf(stderr, "  -o    url for output folder\n");
//     fprintf(stderr, "  -w    width\n");
//     fprintf(stderr, "  -h    height\n");
//     fprintf(stderr, "  -t    stride\n");
//     fprintf(stderr, "  -d    device name\n");
//     fprintf(stderr, "  -f    pixel format\n");
//     fprintf(stderr, "  -l    loop time, if loop = 0, it will loop forever.\n");
    // fprintf(stderr, "  -m    check md5 for decoder & encode. \n");
    // fprintf(stderr, "  -s    stored in output file or not, 0/1 \n");
    // fprintf(stderr, "  -r    rotate input image, 0:not rotate/1:right 90 degrees/2:left 90 degrees/3:180 degrees. \n");
    // fprintf(stderr, "  -q    quantization level (1 - 101).\n");
    // fprintf(stderr, "Example:\n");
    // fprintf(stderr, "jpeg_enc -i input.file -d /dev/dri/renderD128 -w 1920 -h 1080 -f nv12 -o "
    //                 "output -l 10 -s 0 -m 1"
    //                 "\n");
    // fprintf(stderr, "\n");
// }

// static int parse_options(int argc, char **argv, enc_options *options)
// {
//     // jpeg_enc -i input.file -d /dev/dri/renderD128 -w 1920 -h 1080 -f nv12 -o output -l 100
//     static const char optstr[] = "i:o:w:h:t:d:f:l:s:m:r:q:";
//     int c;
//     while ((c = getopt(argc, argv, optstr)) != -1) {
//         switch (c) {
//         case 'i':
//             options->input_file = optarg;
//             break;
//         case 'o':
//             options->output_file = optarg;
//             break;
//         case 'w':
//             options->width = atoi(optarg);
//             break;
//         case 'h':
//             options->height = atoi(optarg);
//             break;
//         case 't':
//             options->stride = atoi(optarg);
//             break;
//         case 'd':
//             options->device = optarg;
//             break;
//         case 'f':
//             // options->pixel_format = get_pixel_format(optarg);
//             options->pixel_fmt = optarg;
//             break;
//         case 'l':
//             options->loop = atoi(optarg);
//             break;
//         case 's':
//             options->store = atoi(optarg);
//             break;
//         case 'm':
//             options->md5 = atoi(optarg);
//             break;
        // case 'r':
        //     options->rotation = atoi(optarg);
        //     break;
        // case 'q':
        //     options->qLevel = atoi(optarg);
        //     break;
//         case '?':
//             usage(argv[0]);
//             return -1;
//         default:
//             fprintf(stderr, "\n Unsupported option: %c \n", c);
//             usage(argv[0]);
//             return -1;
//         }
//     }

//     if (optind < argc) {
//         usage(argv[0]);
//         return -1;
//     }
//     return 0;
// }

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
    printf("rotation: %d\n", opts->rotation);
    printf("qLevel: %d\n", opts->qLevel);
}

int jpeg_enc(enc_options *params_ut)
{
    uint32_t ret_end = 0;
    int32_t ret = -1;
    int enc_fd = 0;
    char out_file[100];

    vmppChannel enc_ch;
    vmppEncChannelParameters ch_apr;
    vmppRuntimeInstance rt_inst;
    vmppResult enc_ret = vmpp_RSLT_OK;

    vmppFrame frame;
    memset(&frame, 0, sizeof(frame));
    
    int dieId = 0;
    vmppConfiguration cfg;
    vmppStream stream;
    vaccrt_init_t vaccrt_init;
    rtError_t vaccRet;

    int frame_count = 0;

    memset(&rt_inst, 0, sizeof(rt_inst));

    md5ctx_inited = 0;
    memset(&md5ctx, 0, sizeof(struct md5_context));
    enc_options *options = params_ut;// static_cast<enc_options *>(malloc(sizeof(enc_options)));

    if (options->output_file != NULL) {
        std::string input_name = ((std::string)params_ut->input_file).substr(((std::string)params_ut->input_file).find_last_of("/"));
        std::string tm = (std::string)params_ut->output_file + input_name.substr(0, input_name.find_last_of("."));
        snprintf(out_file, 100, "%s_rotate%d_qLevel%d.jpg", tm.c_str(), options->rotation, options->qLevel);
        out_file[100 - 1] = '\0'; 
        options->output_file = out_file;
    }
    if (options->pixel_fmt != NULL) options->pixel_format = get_pixel_format(options->pixel_fmt);

    if (options->stride == 0) {
        options->stride = options->width;
    }
    print_enc_options(options);

    open_runtime(&rt_inst); // run time init

    enc_fd = open(options->device, O_RDWR);
    if (enc_fd < 0) {
        printf("Cannot open DRM render node for device %s\n", options->device);
        ret_end = 1;
        goto end;
    }

    memset(&ch_apr, 0, sizeof(ch_apr));

    
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

    ret = vmppInitEncoder(&cfg);
    if (ret != vmpp_RSLT_OK) {
        printf("vmppInitEncoder failed %d\n", ret);
        ret_end = 1;
        goto end;
    }

    ch_apr.device = (vmppDevice)enc_fd;
    ch_apr.codecType = vmpp_CODEC_ENC_JPEG;
    ch_apr.jpegConfig.frameType = options->pixel_format;
    ch_apr.jpegConfig.codingWidth = options->width;
    ch_apr.jpegConfig.codingHeight = options->height;
    ch_apr.jpegConfig.comLength = 7;
    ch_apr.jpegConfig.pCom = (uint8_t *)"vastai";

    ch_apr.jpegConfig.qLevel = options->qLevel;
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
            ch_apr.jpegConfig.qTableLuma[i] = qTable[i];
            ch_apr.jpegConfig.qTableChroma[i] = qTable[i];
        }
    }

    ch_apr.jpegConfig.preProcess.rotation = (vmppEncPictureRotation)options->rotation;
    ch_apr.enProfiling = 1;

    ret = read_input_data(options, &frame);
    if (ret < 0) {
        ret_end = 1;
        goto end;
    }
    
    memset(&stream, 0, sizeof(stream));

    enc_ret = vmppEncCreateChannel(&enc_ch, &ch_apr);
    if (enc_ret != vmpp_RSLT_OK) {
        fprintf(stderr, "send frame error %d.\n", ret);
        ret_end = 1;
        goto end;
    }

    timer_trigger(1);
    do {
        gettimeofday(&tBegin, NULL);

        enc_ret = vmppEncEncodeFrame(enc_ch, &frame, NULL, &stream, 4000);
        if (enc_ret < 0) {
            fprintf(stderr, "send frame error %d.\n", ret);
            ret_end = 2;
            goto end;
        }
        printf("[APP][%p]vmppEncEncodeFrame %d#, Done: %d\n", enc_ch, frame_count, enc_ret);

        handle_output(&enc_ch, options, &stream, enc_ret);

        enc_ret = vmppEncReleaseStream(enc_ch, &stream);
        if (enc_ret < 0) {
            fprintf(stderr, "return stream buffer error %d.\n", ret);
            ret_end = 1;
            goto end;
        }
        timer_trigger(0);
        frame_count++;
    } while (options->loop == 0 || frame_count < options->loop);

    // flush
    frame.memoryType = vmpp_MEM_FLUSH;
    enc_ret = vmppEncEncodeFrame(enc_ch, &frame, NULL, &stream, 4000);
    printf("[APP][%p]vmppEncEncodeFrame, Done: %d\n", enc_ch, enc_ret);

    handle_output(&enc_ch, options, &stream, enc_ret);

    enc_ret = vmppEncDestroyChannel(&enc_ch);
    if (enc_ret < 0) {
        fprintf(stderr, "destroy chn error %d.\n", ret);
        ret_end = 1;
        goto end;
    }
end:
    if (ret_end == 2){
        frame.memoryType = vmpp_MEM_FLUSH;
        enc_ret = vmppEncEncodeFrame(enc_ch, &frame, NULL, &stream, 4000);
        printf("[APP][%p]vmppEncEncodeFrame, Done: %d\n", enc_ch, enc_ret);
        enc_ret = vmppEncDestroyChannel(&enc_ch);
        if (enc_ret < 0) {
            fprintf(stderr, "destroy chn error %d.\n", ret);
        }
    }

    if (frame.data[0])
        free(frame.data[0]);

    // if (options) {
    //     free(options);
    //     options = NULL;
    // }

    if (rt_inst.runtimeHandle) {
        close_runtime(&rt_inst);
    }

    if (enc_fd) {
        close(enc_fd);
        enc_fd = 0;
    }

    if (ret_end > 0)
        return -1;
    return 0;
}

// int main(int argc, char *argv[])
// {
//     int32_t ret = -1;
//     enc_options *options_main = malloc(sizeof(enc_options));
//     // set default values
//     options_main->input_file = "/root/bozhang/video/yuv/stream1.yuv";
//     options_main->output_file = "/root/bozhang/video/yuv/stream1.jpg";
//     options_main->width = 640;
//     options_main->height = 480;
//     options_main->stride = 0;
//     options_main->device = default_dev_str;
//     // options_main->pixel_format = vmpp_PIX_FMT_NV12;
//     options_main->pixel_fmt = "yuv420p";
//     options_main->loop = 1;
//     options_main->md5 = 1;
//     options_main->store = 1;

//     ret = parse_options(argc, argv, options_main);
//     if (ret < 0) {
//         fprintf(stderr, "Failed to parse oargs.\n");
//         return -1;
//     }

//     jpeg_enc(options_main->input_file, options_main->output_file, options_main->width, options_main->height, options_main->stride, options_main->device, options_main->pixel_fmt, options_main->loop, options_main->md5, options_main->store);
//     // MainArgs args = {argc, argv};
//     // return MainTask(&args);
// }
