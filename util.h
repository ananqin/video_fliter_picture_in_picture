#ifndef UTIL_H
#define UTIL_H

#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libavcodec/avcodec.h"
#include "libavutil/pixfmt.h"

AVFrame* av_frame_malloc(int width, int height, int format);
int save_frame_to_i420p(const char *filename, const AVFrame* frame);
int save_frame_to_jpeg(const char *filename, const AVFrame* frame);
AVFrame *get_frame_from_yuvile(const char * filename, int width, int height, int format);
AVFrame* get_frame_from_jpegfile(const char *filename);

#endif // UTIL_H
