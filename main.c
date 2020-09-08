/*
 * 实现水印的叠加效果
*/
#include <stdio.h>
#include "util.h"
#include "libavformat/avformat.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
#include "libavcodec/avcodec.h"

AVFilterContext *mainsrc_ctx = NULL;
AVFilterContext *logosrc_ctx = NULL;
AVFilterContext *resultsink_ctx = NULL;
AVFilterGraph *filter_graph = NULL;


int init_filters(const AVFrame* main_frame, const AVFrame* logo_frame, int x, int y)
{
    int ret = 0;
    AVFilterInOut *inputs = NULL;
    AVFilterInOut *outputs = NULL;
    char filter_args[1024] = { 0 };

    filter_graph = avfilter_graph_alloc();
    if (!filter_graph) {
        printf("Error: allocate filter graph failed\n");
        return -1;
    }

    snprintf(filter_args, sizeof(filter_args),
             "buffer=video_size=%dx%d:pix_fmt=%d:time_base=1/25:pixel_aspect=%d/%d[main];" // Parsed_buffer_0
             "buffer=video_size=%dx%d:pix_fmt=%d:time_base=1/25:pixel_aspect=%d/%d[logo];" // Parsed_bufer_1
             "[main][logo]overlay=%d:%d[result];" // Parsed_overlay_2
             "[result]buffersink", // Parsed_buffer_sink_3
             main_frame->width, main_frame->height, main_frame->format, main_frame->sample_aspect_ratio.num, main_frame->sample_aspect_ratio.den,
             logo_frame->width, logo_frame->height, logo_frame->format, logo_frame->sample_aspect_ratio.num, logo_frame->sample_aspect_ratio.den,
             x, y);

    ret = avfilter_graph_parse2(filter_graph, filter_args, &inputs, &outputs);
    if (ret < 0) {
        printf("Cannot parse graph\n");
        return ret;
    }

    ret = avfilter_graph_config(filter_graph, NULL);
    if (ret < 0) {
        printf("Cannot configure graph\n");
        return ret;
    }

    // Get AVFilterContext from AVFilterGraph parsing from string
    mainsrc_ctx = avfilter_graph_get_filter(filter_graph, "Parsed_buffer_0");
    logosrc_ctx = avfilter_graph_get_filter(filter_graph, "Parsed_buffer_1");
    resultsink_ctx = avfilter_graph_get_filter(filter_graph, "Parsed_buffersink_3");

    return 0;
}

int main_picture_mix_logo(AVFrame *main_frame, AVFrame *logo_frame, AVFrame *result_frame)
{
    int ret = 0;
    // Fill data to buffer filter context(main, logo)
    ret = av_buffersrc_add_frame(mainsrc_ctx, main_frame);      // 输入0
    if(ret < 0)
        return ret;
    ret = av_buffersrc_add_frame(logosrc_ctx, logo_frame);      // 输入1
    if(ret < 0)
        return ret;
    ret = av_buffersink_get_frame(resultsink_ctx, result_frame); //输出
    return ret;
}

int main(int argc, char* argv[])
{
    AVFrame *main_frame = get_frame_from_jpegfile("main.jpg");
    AVFrame *logo_frame = get_frame_from_jpegfile("logo3.jpg");
    AVFrame* result_frame = av_frame_alloc();
    int ret = 0;

    if(ret = init_filters(main_frame, logo_frame, 100, 200) < 0) {
        printf("init_filters failed\n");
        goto FAILED;
    }

    // Get AVFrame from sink buffer filter(result)
    if(main_picture_mix_logo(main_frame, logo_frame, result_frame) < 0) {
        printf("main_picture_mix_logo failed\n");
        goto FAILED;
    }

    // Save AVFrame to picture and check
    save_frame_to_jpeg("test-output3.jpg", result_frame);
FAILED:
    if(main_frame)
        av_frame_free(&main_frame);
    if(logo_frame)
        av_frame_free(&logo_frame);
    if(result_frame)
        av_frame_free(&result_frame);
    if(filter_graph)
        avfilter_graph_free(&filter_graph);
    printf("finish\n");
    return 0;
}
