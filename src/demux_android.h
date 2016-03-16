/*
 * demux_android.h
 *
 *  Created on: 2016年3月16日
 *      Author: yy
 */

#ifndef DEMUX_ANDROID_H_
#define DEMUX_ANDROID_H_

#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>

int demux(const char *src_file, const char *video_file, const char *audio_file);

#endif /* DEMUX_ANDROID_H_ */
