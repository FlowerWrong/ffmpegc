/*
 * main_demux_android.c
 *
 *  Created on: 2016年3月16日
 *      Author: yy
 */


#include "demux_android.h"

int main(int argc, char **argv) {
	if (argc != 4) {
		av_log(NULL, AV_LOG_ERROR, "Usage: %s <input file> <output video file> <output audio file>\n",
				argv[0]);
		return 1;
	}
	const char *src_file = argv[1];
	const char *out_video_file = argv[2];
	const char *out_audio_file = argv[3];

	int ret = demux(src_file, out_video_file, out_audio_file);
	return ret;
}
