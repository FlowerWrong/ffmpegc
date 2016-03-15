#include <stdio.h>

#define __STDC_CONSTANT_MACROS

// Linux...
#ifdef __cplusplus
extern "C" {
#endif
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavutil/opt.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#ifdef __cplusplus
}
;
#endif

/*
 FIX: H.264 in some container format (FLV, MP4, MKV etc.) need
 "h264_mp4toannexb" bitstream filter (BSF)
 *Add SPS,PPS in front of IDR frame
 *Add start code ("0,0,0,1") in front of NALU
 H.264 in some container (MPEG2TS) don't need this BSF.
 */

// 将一个视频先demux，然后解码再编码为 output.h264 and output.aac
int demux(const char *in_filename, const char *out_filename_v,
		const char *out_filename_a);

//'1': Use H.264 Bitstream Filter
#define USE_H264BSF 0

int main(int argc, char* argv[]) {
	if (argc < 4) {
		printf("missing some argv\n");
		return -1;
	}
	const char *in_filename = argv[1];
	const char *out_filename_v = argv[2];
	const char *out_filename_a = argv[3];
	int ret = demux(in_filename, out_filename_v, out_filename_a);
	return 0;
}

int demux(const char *in_filename, const char *out_filename_v,
		const char *out_filename_a) {
	AVOutputFormat *ofmt_a = NULL, *ofmt_v = NULL;
	// Input AVFormatContext and Output AVFormatContext
	AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx_a = NULL, *ofmt_ctx_v = NULL;
	AVPacket pkt, enc_pkt;
	int ret, i;
	int video_index = -1, audio_index = -1;
	int frame_index = 0;

	av_register_all();
	// Input
	if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
		printf("Could not open input file.");
		goto end;
	}
	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		printf("Failed to retrieve input stream information");
		goto end;
	}

	// Output
	avformat_alloc_output_context2(&ofmt_ctx_v, NULL, NULL, out_filename_v);
	if (!ofmt_ctx_v) {
		printf("Could not create output context.\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}
	ofmt_v = ofmt_ctx_v->oformat;

	avformat_alloc_output_context2(&ofmt_ctx_a, NULL, NULL, out_filename_a);
	if (!ofmt_ctx_a) {
		printf("Could not create output context\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}
	ofmt_a = ofmt_ctx_a->oformat;

	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		// Create output AVStream according to input AVStream
		AVFormatContext *ofmt_ctx;
		AVStream *in_stream = ifmt_ctx->streams[i];
		AVStream *out_stream = NULL;

		if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			video_index = i;
			out_stream = avformat_new_stream(ofmt_ctx_v,
					in_stream->codec->codec);
			ofmt_ctx = ofmt_ctx_v;
		} else if (ifmt_ctx->streams[i]->codec->codec_type
				== AVMEDIA_TYPE_AUDIO) {
			audio_index = i;
			out_stream = avformat_new_stream(ofmt_ctx_a,
					in_stream->codec->codec);
			ofmt_ctx = ofmt_ctx_a;
		} else {
			break;
		}

		if (!out_stream) {
			printf("Failed allocating output stream\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}
		// Copy the settings of AVCodecContext
		if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
			printf(
					"Failed to copy context from input to output stream codec context\n");
			goto end;
		}
		out_stream->codec->codec_tag = 0;

		if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
			out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}

	// Open output file
	if (!(ofmt_v->flags & AVFMT_NOFILE)) {
		if (avio_open(&ofmt_ctx_v->pb, out_filename_v, AVIO_FLAG_WRITE) < 0) {
			printf("Could not open output file '%s'", out_filename_v);
			goto end;
		}
	}

	if (!(ofmt_a->flags & AVFMT_NOFILE)) {
		if (avio_open(&ofmt_ctx_a->pb, out_filename_a, AVIO_FLAG_WRITE) < 0) {
			printf("Could not open output file '%s'", out_filename_a);
			goto end;
		}
	}

	// Write file header
	if (avformat_write_header(ofmt_ctx_v, NULL) < 0) {
		printf("Error occurred when opening video output file\n");
		goto end;
	}
//	if (avformat_write_header(ofmt_ctx_a, NULL) < 0) {
//		printf("Error occurred when opening audio output file\n");
//		goto end;
//	}

#if USE_H264BSF
	AVBitStreamFilterContext* h264bsfc = av_bitstream_filter_init("h264_mp4toannexb");
#endif

	while (1) {
		AVFormatContext *ofmt_ctx;
		AVStream *in_stream, *out_stream;

		AVCodecContext *dec_ctx = NULL, *enc_ctx = NULL;
		AVCodec *dec = NULL, *encoder = NULL;

		AVFrame *frame = NULL;

		int got_frame;

		// Get an AVPacket
		if (av_read_frame(ifmt_ctx, &pkt) < 0)
			break;
		in_stream = ifmt_ctx->streams[pkt.stream_index];

		if (pkt.stream_index == video_index) {
			ofmt_ctx = ofmt_ctx_v;
			out_stream = avformat_new_stream(ofmt_ctx, NULL);

			/* find decoder for the stream */
			dec_ctx = in_stream->codec;
			dec = avcodec_find_decoder(dec_ctx->codec_id);
			if (!dec) {
				fprintf(stderr, "Failed to find %s codec\n",
						av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
				return AVERROR(EINVAL);
			}

			/* Open decoder */
			int ret = avcodec_open2(dec_ctx, dec, NULL);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR,
						"Failed to open decoder for stream #%u\n", i);
				return ret;
			}

			// decoder is MPEG-4 part 2
			printf("decoder is %s\n", dec->long_name);

			// NOTE
			frame = av_frame_alloc();

			ret = avcodec_decode_video2(dec_ctx, frame, &got_frame, &pkt);
			if (ret < 0) {
				av_frame_free(&frame);
				av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
				break;
			}

			// printf("frame duration is %d\n", frame->pkt_duration);

			// encode
			encoder = avcodec_find_encoder(AV_CODEC_ID_H264);

			// avcodec_copy_context(enc_ctx, dec_ctx);
			enc_ctx = avcodec_alloc_context3(encoder);
			if (!encoder) {
				av_log(NULL, AV_LOG_FATAL, "Necessary encoder not found\n");
				return AVERROR_INVALIDDATA;
			}

			enc_ctx->height = dec_ctx->height;
			enc_ctx->width = dec_ctx->width;
			enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
			enc_ctx->pix_fmt = encoder->pix_fmts[0];
			enc_ctx->time_base = dec_ctx->time_base;
			//enc_ctx->time_base.num = 1;
			//enc_ctx->time_base.den = 25;
			//H264的必备选项，没有就会错
			enc_ctx->me_range = 16;
			enc_ctx->max_qdiff = 4;
			enc_ctx->qmin = 10;
			enc_ctx->qmax = 51;
			enc_ctx->qcompress = 0.6;
			enc_ctx->refs = 3;
			enc_ctx->bit_rate = 1500;

			ret = avcodec_open2(enc_ctx, encoder, NULL);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR,
						"Cannot open video encoder for stream #%u\n", i);
				return ret;
			}

			av_opt_set(enc_ctx->priv_data, "preset", "slow", 0);

			// AVOutputFormat *formatOut = av_guess_format(NULL, out_filename_v, NULL);

			enc_pkt.data = NULL;
			enc_pkt.size = 0;
			av_init_packet(&enc_pkt);
			ret = avcodec_open2(enc_ctx, encoder, NULL);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR,
						"Failed to open encoder for stream #%u\n", i);
				return ret;
			}
			ret = avcodec_encode_video2(enc_ctx, &enc_pkt, frame, &got_frame);

			printf("demo is %s\n", "hello");

			av_frame_free(&frame);
			avcodec_close(enc_ctx);
			avcodec_close(dec_ctx);

			// printf("Write Video Packet. size:%d\tpts:%lld\n", pkt.size, pkt.pts);
#if USE_H264BSF
			av_bitstream_filter_filter(h264bsfc, in_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif
		} else {
			continue;
		}

		// Convert PTS/DTS
		enc_pkt.pts = av_rescale_q_rnd(enc_pkt.pts, in_stream->time_base,
				out_stream->time_base,
				(AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		enc_pkt.dts = av_rescale_q_rnd(enc_pkt.dts, in_stream->time_base,
				out_stream->time_base,
				(AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		enc_pkt.duration = av_rescale_q(enc_pkt.duration, in_stream->time_base,
				out_stream->time_base);
		// enc_pkt.pos = -1;
		enc_pkt.stream_index = video_index;

		if (av_interleaved_write_frame(ofmt_ctx, &enc_pkt) < 0) {
			printf("Error muxing packet\n");
			break;
		}
		av_free_packet(&enc_pkt);
		av_free_packet(&pkt);
		frame_index++;
	}

#if USE_H264BSF
	av_bitstream_filter_close(h264bsfc);
#endif

	// Write file trailer
	av_write_trailer(ofmt_ctx_a);
	av_write_trailer(ofmt_ctx_v);

	end: avformat_close_input(&ifmt_ctx);
	/* close output */
	if (ofmt_ctx_a && !(ofmt_a->flags & AVFMT_NOFILE))
		avio_close(ofmt_ctx_a->pb);

	if (ofmt_ctx_v && !(ofmt_v->flags & AVFMT_NOFILE))
		avio_close(ofmt_ctx_v->pb);

	avformat_free_context(ofmt_ctx_a);
	avformat_free_context(ofmt_ctx_v);

	if (ret < 0 && ret != AVERROR_EOF) {
		printf("Error occurred.\n");
		return -1;
	}
	return 0;
}
