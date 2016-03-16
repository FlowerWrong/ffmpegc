/*
 * demux_android.c
 *
 *  Created on: 2016年3月16日
 *      Author: yy
 */

#include "demux_android.h"

int demux(const char *src_file, const char *video_file, const char *audio_file) {
	AVFormatContext *ifmt_ctx = NULL;
	AVPacket pkt;
	int ret, i;
	int videoindex = -1, audioindex = -1;
	const char *in_filename = src_file;
	const char *out_filename_v = video_file;
	const char *out_filename_a = audio_file;

	av_register_all();
	//Input
	if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
		return -1;
	}
	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		return -1;
	}

	videoindex = -1;
	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoindex = i;
		} else if (ifmt_ctx->streams[i]->codec->codec_type
				== AVMEDIA_TYPE_AUDIO) {
			audioindex = i;
		}
	}

	FILE *fp_audio = fopen(out_filename_a, "wb+");
	FILE *fp_video = fopen(out_filename_v, "wb+");

	/*
	 FIX: H.264 in some container format (FLV, MP4, MKV etc.) need
	 "h264_mp4toannexb" bitstream filter (BSF)
	 *Add SPS,PPS in front of IDR frame
	 *Add start code ("0,0,0,1") in front of NALU
	 H.264 in some container (MPEG2TS) don't need this BSF.
	 */
#if USE_H264BSF
	AVBitStreamFilterContext* h264bsfc = av_bitstream_filter_init("h264_mp4toannexb");
#endif

	while (av_read_frame(ifmt_ctx, &pkt) >= 0) {
		if (pkt.stream_index == videoindex) {
#if USE_H264BSF
			av_bitstream_filter_filter(h264bsfc, ifmt_ctx->streams[videoindex]->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif
			fwrite(pkt.data, 1, pkt.size, fp_video);
		} else if (pkt.stream_index == audioindex) {
			fwrite(pkt.data, 1, pkt.size, fp_audio);
		}
		av_free_packet(&pkt);
	}

#if USE_H264BSF
	av_bitstream_filter_close(h264bsfc);
#endif

	fclose(fp_video);
	fclose(fp_audio);

	avformat_close_input(&ifmt_ctx);

	if (ret < 0 && ret != AVERROR_EOF) {
		printf("Error occurred.\n");
		return -1;
	}
	return 0;
}
