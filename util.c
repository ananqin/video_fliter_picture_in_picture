#include "util.h"
#include <stdio.h>

// 只是读取一帧
AVFrame *get_frame_from_yuvile(const char *filename, int width, int height, int format)
{
    AVFrame *frame = av_frame_malloc(width, height, format);

    FILE *infile = NULL;
    infile = fopen(filename, "rb");
    if (!infile) {
        av_frame_free(&frame);
        return NULL;
    }

    int yuv_size = width * height * 1.5;
    uint8_t* data = (uint8_t*)av_malloc(yuv_size);
    if (!data) {
        av_frame_free(&frame);
        return NULL;
    }
    if(fread(data, 1, yuv_size, infile) != yuv_size) {
        printf("warn fread yuv data failed\n");
    }

    fclose(infile);
    av_image_fill_arrays(frame->data, frame->linesize, data, (enum AVPixelFormat)format, width, height, 1);

    return frame;
}

int save_frame_to_i420p(const char *filename, const AVFrame* frame)
{
    if (frame->format != AV_PIX_FMT_YUV420P) {
        printf("the format is't AV_PIX_FMT_YUV420P\n");
        return -1;
    }

    FILE *outfile = fopen(filename, "wb");
    if (!outfile) {
        printf("fopen %s failed\n", filename);
        return -1;
    }


    size_t y_size = frame->width * frame->height;
    size_t u_size = y_size / 4;
    fwrite((const char*)frame->data[0], 1, y_size, outfile);
    fwrite((const char*)frame->data[1], 1, u_size, outfile);
    fwrite((const char*)frame->data[2], 1, u_size, outfile); // u size is equal to v
    fclose(outfile);
    return 0;
}

int save_frame_to_jpeg(const char *filename, const AVFrame* frame)
{
    char error_msg_buf[256] = { 0 };

    AVCodec *jpeg_codec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    if (!jpeg_codec)
        return -1;
    AVCodecContext *jpeg_codec_ctx = avcodec_alloc_context3(jpeg_codec);
    if (!jpeg_codec_ctx)
        return -2;

    jpeg_codec_ctx->pix_fmt = AV_PIX_FMT_YUVJ420P; // NOTE: Don't use AV_PIX_FMT_YUV420P
    jpeg_codec_ctx->width = frame->width;
    jpeg_codec_ctx->height = frame->height;
    jpeg_codec_ctx->time_base =  (AVRational){ 1, 25 };
    jpeg_codec_ctx->framerate =  (AVRational){ 25, 1 };
    AVDictionary *encoder_opts = NULL;
    //    av_dict_set(&encoder_opts, "qscale:v", "2", 0);
    av_dict_set(&encoder_opts, "flags", "+qscale", 0);
    av_dict_set(&encoder_opts, "qmax", "2", 0);
    av_dict_set(&encoder_opts, "qmin", "2", 0);
    int ret = avcodec_open2(jpeg_codec_ctx, jpeg_codec, &encoder_opts);
    if (ret < 0) {
        printf("avcodec open failed:%s\n", av_make_error_string(error_msg_buf, sizeof(error_msg_buf), ret));
        avcodec_free_context(&jpeg_codec_ctx);
        return -3;
    }
    av_dict_free(&encoder_opts);
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    ret = avcodec_send_frame(jpeg_codec_ctx, frame);
    if (ret < 0) {
        printf("Error: %s\n", av_make_error_string(error_msg_buf, sizeof(error_msg_buf), ret));
        avcodec_free_context(&jpeg_codec_ctx);
        return -4;
    }
    ret = 0;
    while (ret >= 0) {
        ret = avcodec_receive_packet(jpeg_codec_ctx, &pkt);
        if (ret == AVERROR(EAGAIN))
            continue;
        if (ret == AVERROR_EOF) {
            ret = 0;
            break;
        }
        FILE *outfile = fopen(filename, "wb");
        if (!outfile) {
            printf("fopen %s failed\n", filename);
            ret = -1;
            break;
        }
        if(fwrite((char*) pkt.data, 1, pkt.size, outfile) == pkt.size) {
            ret = 0;
        } else {
            printf("fwrite %s failed\n", filename);
            ret = -1;
        }
        fclose(outfile);

        ret = 0;
        break;
    }

    avcodec_free_context(&jpeg_codec_ctx);
    return ret;
}

AVFrame* get_frame_from_jpegfile(const char *filename)
{
    int ret = 0;
    AVFormatContext *format_ctx = NULL;
    if ((ret = avformat_open_input(&format_ctx, filename, NULL, NULL)) != 0) {
        char error_msg_buf[256] = {0};
        av_strerror(ret, error_msg_buf, sizeof(error_msg_buf));
        printf("Error: avformat_open_input failed: %s\n", error_msg_buf);
        return NULL;
    }

    avformat_find_stream_info(format_ctx, NULL);

    AVCodec *codec = NULL;
    int video_stream_idx = -1;
    video_stream_idx = av_find_best_stream(format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if (video_stream_idx < 0)
        goto cleanup_and_return;

    AVCodecContext *av_codec_ctx = avcodec_alloc_context3(codec);

    ret = avcodec_open2(av_codec_ctx, codec, NULL);
    if (ret < 0)
        goto cleanup_and_return;

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    ret = av_read_frame(format_ctx, &pkt);
    if (ret < 0)
        goto cleanup_and_return;
    ret = avcodec_send_packet(av_codec_ctx, &pkt);
    if (ret < 0)
        goto cleanup_and_return;

    AVFrame* frame = av_frame_alloc();
    ret = avcodec_receive_frame(av_codec_ctx, frame);
    if (ret < 0)
        av_frame_free(&frame);

cleanup_and_return:
    if (format_ctx)
        avformat_close_input(&format_ctx);
    if (av_codec_ctx)
        avcodec_free_context(&av_codec_ctx);

    return frame;
}

AVFrame* av_frame_malloc(int width, int height, int format)
{
    AVFrame *frame = av_frame_alloc();
    frame->width = width;
    frame->height = height;
    frame->format = format;
    av_frame_get_buffer(frame, 1);
    return frame;
}
