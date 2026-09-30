#include "ffmpeg-wrapper.h"

#include "log.h"
//#include "vmpp_defs.h"

#ifdef USING_FFMPEG
int ff_open(const char *file_name, struct ff_context *ctx)
{
    if (!file_name || !ctx) {
        LOG_ERROR("Invalid parameters: file %s, ctx %p", file_name, ctx);
        return -1;
    }

    int ret = 0;
    memset(ctx, 0, sizeof(struct ff_context));

    ret = avformat_open_input(&ctx->fmt_ctx, file_name, NULL, NULL);
    if (ret) {
        LOG_ERROR("avformat_open_input failed, err %d, <%s>", ret, file_name);
        goto err;
    }

    ret = avformat_find_stream_info(ctx->fmt_ctx, NULL);
    if (ret) {
        LOG_ERROR("avformat_find_stream_info failed, err %d, <%s>", ret, file_name);
        goto err;
    }

    ctx->video_stream_idx = av_find_best_stream(ctx->fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (ctx->video_stream_idx < 0) {
        ret = -1;
        LOG_ERROR("no video stream in file <%s>", file_name);
        goto err;
    }

    if (!strcmp(ctx->fmt_ctx->iformat->long_name, "raw H.264 video") ||
        !strcmp(ctx->fmt_ctx->iformat->long_name, "raw HEVC video") ||
        !strcmp(ctx->fmt_ctx->iformat->long_name, "raw MJPEG video")) {
        LOG_INFO("Raw data in file %s, %s", file_name, ctx->fmt_ctx->iformat->long_name);
        ctx->raw_data = 1;
    } else {
        ctx->raw_data = 0;
        const AVBitStreamFilter *bsf = NULL;
        const char *name = NULL;
        switch (ctx->fmt_ctx->streams[ctx->video_stream_idx]->codecpar->codec_id) {
        case AV_CODEC_ID_H264:
            name = "h264_mp4toannexb";
            break;
        case AV_CODEC_ID_HEVC:
            name = "hevc_mp4toannexb";
            break;
        case AV_CODEC_ID_MJPEG:
            name = "mjpeg2jpeg";
            break;
        default:
            LOG_ERROR("Unsupported codec %d",
                      ctx->fmt_ctx->streams[ctx->video_stream_idx]->codecpar->codec_id);
        }
        bsf = av_bsf_get_by_name(name);
        if (!bsf) {
            LOG_ERROR("bsf '%s' not fount", name);
            goto err;
        }
        ret = av_bsf_alloc(bsf, &ctx->bsf_ctx);
        if (ret < 0) {
            LOG_ERROR("av_bsf_alloc failed for '%s' , ret %d <%s>", name, ret, av_err2str(ret));
            goto err;
        }
        ret = avcodec_parameters_copy(ctx->bsf_ctx->par_in,
                                      ctx->fmt_ctx->streams[ctx->video_stream_idx]->codecpar);
        if (ret < 0) {
            LOG_ERROR("avcodec_parameters_copy failed for '%s', ret %d <%s>", name, ret,
                      av_err2str(ret));
            goto err;
        }
        ret = av_bsf_init(ctx->bsf_ctx);
        if (ret < 0) {
            LOG_ERROR("Error initializing bitstream filter: %s, ret %d <%s>", name, ret,
                      av_err2str(ret));
            return ret;
        }
    }

    return 0;
err:
    if (ctx->fmt_ctx)
        avformat_close_input(&ctx->fmt_ctx);

    return ret;
}

void ff_close(struct ff_context *ctx)
{
    if (ctx) {
        if (ctx->fmt_ctx)
            avformat_close_input(&ctx->fmt_ctx);
    }
}

unsigned int ff_read_frame(unsigned char *buffer, unsigned int buffer_size, unsigned long *pts,
                           struct ff_context *ctx)
{
    if (!buffer || buffer_size <= 0 || !ctx) {
        LOG_ERROR("Invalid parameters: buffer %p, size %d, ctx %p", buffer, buffer_size, ctx);
        return 0;
    }

    AVPacket pkt = {0};
    pkt.data = NULL;
    pkt.size = 0;
    int ret;
    do {
        av_packet_unref(&pkt);
        ret = av_read_frame(ctx->fmt_ctx, &pkt);
    } while (ret == 0 && pkt.stream_index != ctx->video_stream_idx);

    if (ret != 0) {
        if (ret == AVERROR_EOF) {
            LOG_WARN("av_read_frame failed: %d <%s>", ret, av_err2str(ret));
            ctx->eof = 1;
        } else {
            LOG_ERROR("av_read_frame failed: %d <%s>", ret, av_err2str(ret));
        }
        ret = 0;
        goto end;
    }

    if (!ctx->raw_data) {
        ret = av_bsf_send_packet(ctx->bsf_ctx, &pkt);
        if (ret < 0) {
            LOG_ERROR("av_bsf_send_packet failed, ret %d <%s>", ret, av_err2str(ret));
            goto end;
        }
        ret = av_bsf_receive_packet(ctx->bsf_ctx, &pkt);
        if (ret < 0) {
            LOG_ERROR("av_bsf_receive_packet failed, ret %d <%s>", ret, av_err2str(ret));
            goto end;
        }
    }

    memcpy(buffer, pkt.data, pkt.size);
    ret = pkt.size;
    *pts = pkt.pts;
end:
    av_packet_unref(&pkt);
    return ret;
}

int ff_seek_to_start(struct ff_context *ctx)
{
    if (!ctx) {
        LOG_ERROR("Invalid parameters: ctx %p", ctx);
        return 0;
    }

    int ret = avformat_seek_file(ctx->fmt_ctx, -1, -INT64_MAX, 0, INT64_MAX, AVSEEK_FLAG_BYTE);
    if (ret < 0) {
        LOG_WARN("avformat_seek_file failed, err %d <%s>", ret, av_err2str(ret));
        ret = avformat_seek_file(ctx->fmt_ctx, -1, -INT64_MAX, 0, INT64_MAX,
                                 AVSEEK_FLAG_ANY | AVSEEK_FLAG_FRAME);
        LOG_WARN("re-do avformat_seek_file, ret %d <%s>", ret, av_err2str(ret));
    }
    return ret;
}

static inline int ff_jpeg(const char *file_name)
{
    const char *suffix = strrchr(file_name, '.');
    if (suffix && (!strcmp(suffix, ".jpeg") || !strcmp(suffix, ".jpg") || !strcmp(suffix, ".mjpeg") ||
                      !strcmp(suffix, ".mjpg"))) {
        return 1;
    }
    return 0;
}

static inline int ff_probe_size(const char *file_name)
{
    int size = 0;
    FILE *pf = fopen(file_name, "rb");
    if (pf) {
        fseeko(pf, 0, SEEK_END);
        size = ftello(pf);
        fclose(pf);
    } else {
        LOG_ERROR("Fail to open file '%s' for getting probe size", file_name);
    }
    return size;
}

ff_context_ptr ff_open2(const char *file_name)
{
    if (!file_name) {
        LOG_ERROR("Invalid parameters.");
        return NULL;
    }

    int ret = 0;
    ff_context_ptr ctx = NULL;
    const AVInputFormat *file_iformat = NULL;

    ctx = malloc(sizeof(struct ff_context));
    if (!ctx) {
        LOG_ERROR("Fail to malloc ff_context for '%s'", file_name);
        goto err;
    }
    memset(ctx, 0, sizeof(struct ff_context));

    ctx->fmt_ctx = avformat_alloc_context();
    if (!ctx->fmt_ctx) {
        LOG_ERROR("avformat_alloc_context failed");
        goto err;
    }
    ctx->fmt_ctx->flags |= AVFMT_FLAG_NONBLOCK;
    av_dict_set(&ctx->format_opts, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
    if (ff_jpeg(file_name)) {
        file_iformat = av_find_input_format("mjpeg");
    }

    ret = avformat_open_input(&ctx->fmt_ctx, file_name, file_iformat, &ctx->format_opts);
    if (ret) {
#if 0    // Currently not in use
        if (ret == AVERROR_INVALIDDATA) {
            int probe_size = ff_probe_size(file_name);
            if (probe_size > (1024 * 1024) /* default probe size for FFmpeg */) {
                char probe_size_str[32] = { 0 };
                LOG_WARN("Using new probe size(%d) to try avformat_open_input again.", probe_size);
                sprintf(probe_size_str, "%d", probe_size);
                av_dict_set(&ctx->format_opts, "formatprobesize", probe_size_str, AV_DICT_DONT_OVERWRITE);
                ret = avformat_open_input(&ctx->fmt_ctx, file_name, file_iformat, &ctx->format_opts);
                if (ret) {
                    LOG_ERROR("Retry avformat_open_input failed, err %d (%s), <%s>", ret, av_err2str(ret), file_name);
                    goto err;
                }
            } else {
                LOG_ERROR("avformat_open_input failed, err %d (%s), <%s>", ret, av_err2str(ret), file_name);
                goto err;
            }
        } else
#endif
        {
            LOG_ERROR("avformat_open_input failed, err %d (%s), <%s>", ret, av_err2str(ret), file_name);
            goto err;
        }
    }

    ret = avformat_find_stream_info(ctx->fmt_ctx, NULL);
    if (ret) {
        LOG_ERROR("avformat_find_stream_info failed, err %d (%s), <%s>", ret, av_err2str(ret), file_name);
        goto err;
    }

    ctx->video_stream_idx = av_find_best_stream(ctx->fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (ctx->video_stream_idx < 0) {
        ret = -1;
        LOG_ERROR("no video stream in file <%s>", file_name);
        goto err;
    }

    if (!strcmp(ctx->fmt_ctx->iformat->long_name, "raw H.264 video") ||
        !strcmp(ctx->fmt_ctx->iformat->long_name, "raw HEVC video") ||
        !strcmp(ctx->fmt_ctx->iformat->long_name, "raw MJPEG video") ||
        !strcmp(ctx->fmt_ctx->iformat->long_name, "AV1 low overhead OBU") ||
        !strcmp(ctx->fmt_ctx->iformat->long_name, "raw AVS2-P2/IEEE1857.4") ||
        ctx->fmt_ctx->streams[ctx->video_stream_idx]->codecpar->codec_id == AV_CODEC_ID_AVS2) {
        /* For avs2, it seems that no additional bsf is needed. */
        LOG_INFO("Raw data in file %s, '%s', codec id %d", file_name, ctx->fmt_ctx->iformat->long_name,
            ctx->fmt_ctx->streams[ctx->video_stream_idx]->codecpar->codec_id);
        ctx->raw_data = 1;
    } else {
        ctx->raw_data = 0;
        const AVBitStreamFilter *bsf = NULL;
        const char *name = NULL;
        switch (ctx->fmt_ctx->streams[ctx->video_stream_idx]->codecpar->codec_id) {
        case AV_CODEC_ID_H264:
            name = "h264_mp4toannexb";
            break;
        case AV_CODEC_ID_HEVC:
            name = "hevc_mp4toannexb";
            break;
        case AV_CODEC_ID_MJPEG:
            name = "mjpeg2jpeg";
            break;
        case AV_CODEC_ID_AV1:
            name = "av1_metadata";
            break;
        case AV_CODEC_ID_VP9:
            name = "vp9_superframe";
            break;
        default:
            LOG_ERROR("Unsupported codec %d",
                      ctx->fmt_ctx->streams[ctx->video_stream_idx]->codecpar->codec_id);
            goto err;
        }
        bsf = av_bsf_get_by_name(name);
        if (!bsf) {
            LOG_ERROR("bsf '%s' not fount", name);
            goto err;
        }
        ret = av_bsf_alloc(bsf, &ctx->bsf_ctx);
        if (ret < 0) {
            LOG_ERROR("av_bsf_alloc failed for '%s' , ret %d <%s>", name, ret, av_err2str(ret));
            goto err;
        }
        ret = avcodec_parameters_copy(ctx->bsf_ctx->par_in,
                                      ctx->fmt_ctx->streams[ctx->video_stream_idx]->codecpar);
        if (ret < 0) {
            LOG_ERROR("avcodec_parameters_copy failed for '%s', ret %d <%s>", name, ret,
                      av_err2str(ret));
            goto err;
        }
        ret = av_bsf_init(ctx->bsf_ctx);
        if (ret < 0) {
            LOG_ERROR("Error initializing bitstream filter: %s, ret %d <%s>", name, ret,
                      av_err2str(ret));
            goto err;
        }
    }

    return ctx;
err:
    if (ctx && ctx->bsf_ctx) {
        av_bsf_free(&ctx->bsf_ctx);
    }
    if (ctx && ctx->fmt_ctx) {
        avformat_close_input(&ctx->fmt_ctx);
    }
    if (ctx) {
        av_dict_free(&ctx->format_opts);
        av_packet_unref(&ctx->pkt);
        free(ctx);
    }

    return NULL;
}

void ff_close2(ff_context_ptr *pctx)
{
    if (pctx && *pctx) {
        ff_context_ptr ctx = *pctx;
        if (ctx->bsf_ctx) {
            av_bsf_free(&ctx->bsf_ctx);
        }
        if (ctx->fmt_ctx) {
            avformat_close_input(&ctx->fmt_ctx);
        }
        av_dict_free(&ctx->format_opts);
        av_packet_unref(&ctx->pkt);
        free(ctx);
    }
}

unsigned int ff_read_frame2(ff_context_ptr ctx, unsigned char **pdata, unsigned long *pts)
{
    if (!pdata || !pts || !ctx) {
        LOG_ERROR("Invalid parameters: pdata %p, pts %p, ctx %p", pdata, pts, ctx);
        return 0;
    }

    int ret;
    do {
        av_packet_unref(&ctx->pkt);
        ret = av_read_frame(ctx->fmt_ctx, &ctx->pkt);
    } while (ret == 0 && ctx->pkt.stream_index != ctx->video_stream_idx);

    if (ret != 0) {
        if (ret == AVERROR_EOF) {
            LOG_DEBUG("av_read_frame failed: %d <%s>", ret, av_err2str(ret));
            ctx->eof = 1;
        } else {
            LOG_ERROR("av_read_frame failed: %d <%s>", ret, av_err2str(ret));
        }
        ret = 0;
        goto fail;
    }

    if (!ctx->raw_data) {
        ret = av_bsf_send_packet(ctx->bsf_ctx, &ctx->pkt);
        if (ret < 0) {
            LOG_ERROR("av_bsf_send_packet failed, ret %d <%s>", ret, av_err2str(ret));
            goto fail;
        }
        ret = av_bsf_receive_packet(ctx->bsf_ctx, &ctx->pkt);
        if (ret < 0) {
            LOG_ERROR("av_bsf_receive_packet failed, ret %d <%s>", ret, av_err2str(ret));
            goto fail;
        }
    }

    *pdata = ctx->pkt.data;
    *pts = ctx->pkt.pts;
    return ctx->pkt.size;
fail:
    *pdata = NULL;
    *pts = 0;
    return 0;
}

void ff_set_log_level(int level)
{
    int ffLogLevel = AV_LOG_WARNING, flag;
    switch (level) {
    case LOG_LEVEL_FATAL:
        ffLogLevel = AV_LOG_FATAL;
        break;
    case LOG_LEVEL_ERROR:
        ffLogLevel = AV_LOG_ERROR;
        break;
    case LOG_LEVEL_WARN:
        ffLogLevel = AV_LOG_WARNING;
        break;
    case LOG_LEVEL_INFO:
        ffLogLevel = AV_LOG_INFO;
        break;
    case LOG_LEVEL_DEBUG:
        ffLogLevel = AV_LOG_DEBUG;
        break;
    case LOG_LEVEL_TRACE:
        ffLogLevel = AV_LOG_TRACE;
        break;
    default:
        break;
    }
    av_log_set_level(ffLogLevel);

    flag = av_log_get_flags();
    flag |= AV_LOG_PRINT_LEVEL;
    av_log_set_flags(flag);
}

void ff_log_cb(void *ptr, int level, const char *fmt, va_list vl)
{
    va_list vl2;
    char msg[1024];
    static int print_prefix = 1;

    va_copy(vl2, vl);
    av_log_format_line(ptr, level, fmt, vl2, msg, sizeof(msg), &print_prefix);
    va_end(vl2);
    switch (level) {
    case AV_LOG_PANIC:
    case AV_LOG_FATAL:
        LOGNLB(LOG_LEVEL_FATAL, COLOR_PURPLE, "[FFmpeg]%s", msg);
        break;
    case AV_LOG_ERROR:
        LOGNLB_ERROR("[FFmpeg]%s", msg);
        break;
    case AV_LOG_WARNING:
        LOGNLB_WARN("[FFmpeg]%s", msg);
        break;
    case AV_LOG_INFO:
        LOGNLB_INFO("[FFmpeg]%s", msg);
        break;
    case AV_LOG_VERBOSE:
    case AV_LOG_DEBUG:
        LOGNLB_DEBUG("[FFmpeg]%s", msg);
        break;
    case AV_LOG_TRACE:
        LOGNLB_TRACE("[FFmpeg]%s", msg);
        break;
    default:
        break;
    }
}
#endif
