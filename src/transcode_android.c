#include "transcode.h"

int transcode(const char *infile, const char *outfile) {
	int ret;
	AVFormatContext *ifmt_ctx = NULL;
	AVFormatContext *ofmt_ctx = NULL;
	AVPacket packet = { .data = NULL, .size = 0 };
	AVFrame *frame = NULL;
	enum AVMediaType type;
	unsigned int stream_index;
	unsigned int i;
	int got_frame;
	int (*dec_func)(AVCodecContext *, AVFrame *, int *, const AVPacket *);

	av_register_all();

	if ((ret = avformat_open_input(&ifmt_ctx, infile, NULL, NULL)) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
		return ret;
	}

	if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
		return ret;
	}

	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		AVStream *stream;
		AVCodecContext *codec_ctx;
		stream = ifmt_ctx->streams[i];
		codec_ctx = stream->codec;
		/* Reencode video & audio and remux subtitles etc. */
		if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
				|| codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
			/* Open decoder */
			ret = avcodec_open2(codec_ctx,
					avcodec_find_decoder(codec_ctx->codec_id), NULL);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR,
						"Failed to open decoder for stream #%u\n", i);
				return ret;
			}
		}
	}

	av_dump_format(ifmt_ctx, 0, infile, 0);

	if (1) {
		AVStream *out_stream;
		AVStream *in_stream;
		AVCodecContext *dec_ctx, *enc_ctx;
		AVCodec *encoder;
		int ret;
		unsigned int i;

		avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, outfile);
		if (!ofmt_ctx) {
			av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
			return AVERROR_UNKNOWN;
		}

		for (i = 0; i < ifmt_ctx->nb_streams; i++) {
			out_stream = avformat_new_stream(ofmt_ctx, NULL);
			if (!out_stream) {
				av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
				return AVERROR_UNKNOWN;
			}

			in_stream = ifmt_ctx->streams[i];
			dec_ctx = in_stream->codec;
			enc_ctx = out_stream->codec;

			if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
					|| dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
				/* In this example, we transcode to same properties (picture size,
				 * sample rate etc.). These properties can be changed for output
				 * streams easily using filters */
				if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
					/* in this example, we choose transcoding to same codec */
					//encoder = avcodec_find_encoder(dec_ctx->codec_id);
					encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
					if (!encoder) {
						av_log(NULL, AV_LOG_FATAL,
								"Necessary encoder not found\n");
						return AVERROR_INVALIDDATA;
					}

					out_stream->codec = avcodec_alloc_context3(encoder);
					enc_ctx = out_stream->codec;
					int tResult = -1;
					if ((tResult = av_opt_set(enc_ctx->priv_data, "preset",
							"slow", 0)) < 0)
						av_log(NULL, AV_LOG_FATAL,
								"Failed to set A/V option \"preset\" because %s(0x%x)\n",
								strerror(AVERROR(tResult)), tResult);

					enc_ctx->height = dec_ctx->height;
					enc_ctx->width = dec_ctx->width;
					enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
					/* take first format from list of supported formats */
					enc_ctx->pix_fmt = encoder->pix_fmts[0];
					/* video time_base can be set to whatever is handy and supported by encoder */
					enc_ctx->time_base = dec_ctx->time_base;
				} else {
					/* in this example, we choose transcoding to same codec */
					// encoder = avcodec_find_encoder(dec_ctx->codec_id);
					encoder = avcodec_find_encoder(AV_CODEC_ID_AAC);
					if (!encoder) {
						av_log(NULL, AV_LOG_FATAL,
								"Necessary encoder not found\n");
						return AVERROR_INVALIDDATA;
					}

					// NOTE
					// FIXME The encoder 'aac' is experimental but experimental codecs are not enabled
					enc_ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

					enc_ctx->sample_rate = dec_ctx->sample_rate;
					enc_ctx->channel_layout = dec_ctx->channel_layout;
					enc_ctx->channels = av_get_channel_layout_nb_channels(
							enc_ctx->channel_layout);
					/* take first format from list of supported formats */
					enc_ctx->sample_fmt = encoder->sample_fmts[0];
					enc_ctx->time_base =
							(AVRational ) { 1, enc_ctx->sample_rate };
				}

				/* Third parameter can be used to pass settings to encoder */
				ret = avcodec_open2(enc_ctx, encoder, NULL);
				if (ret < 0) {
					av_log(NULL, AV_LOG_ERROR,
							"Cannot open video encoder for stream #%u\n", i);
					return ret;
				}
			} else if (dec_ctx->codec_type == AVMEDIA_TYPE_UNKNOWN) {
				av_log(NULL, AV_LOG_FATAL,
						"Elementary stream #%d is of unknown type, cannot proceed\n",
						i);
				return AVERROR_INVALIDDATA;
			} else {
				/* if this stream must be remuxed */
				ret = avcodec_copy_context(ofmt_ctx->streams[i]->codec,
						ifmt_ctx->streams[i]->codec);
				if (ret < 0) {
					av_log(NULL, AV_LOG_ERROR,
							"Copying stream context failed\n");
					return ret;
				}
			}

			if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
				enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

		}
		av_dump_format(ofmt_ctx, 0, outfile, 1);

		if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
			ret = avio_open(&ofmt_ctx->pb, outfile, AVIO_FLAG_WRITE);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'",
						outfile);
				return ret;
			}
		}

		/* init muxer, write output file header */
		ret = avformat_write_header(ofmt_ctx, NULL);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR,
					"Error occurred when opening output file\n");
			return ret;
		}
	}

	/* read all packets */
	while (1) {
		if ((ret = av_read_frame(ifmt_ctx, &packet)) < 0)
			break;
		stream_index = packet.stream_index;
		type = ifmt_ctx->streams[packet.stream_index]->codec->codec_type;

		frame = av_frame_alloc();
		if (!frame) {
			ret = AVERROR(ENOMEM);
			break;
		}
		av_packet_rescale_ts(&packet,
				ifmt_ctx->streams[stream_index]->time_base,
				ifmt_ctx->streams[stream_index]->codec->time_base);
		dec_func =
				(type == AVMEDIA_TYPE_VIDEO) ?
						avcodec_decode_video2 : avcodec_decode_audio4;
		ret = dec_func(ifmt_ctx->streams[stream_index]->codec, frame,
				&got_frame, &packet);
		if (ret < 0) {
			av_frame_free(&frame);
			av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
			break;
		}

		if (got_frame) {
			frame->pts = av_frame_get_best_effort_timestamp(frame);
			if (1) {
				int ret;
				int got_frame_local;
				AVPacket enc_pkt;
				int (*enc_func)(AVCodecContext *, AVPacket *, const AVFrame *, int *) =
				(ifmt_ctx->streams[stream_index]->codec->codec_type ==
						AVMEDIA_TYPE_VIDEO) ? avcodec_encode_video2 : avcodec_encode_audio2;

				if (!got_frame)
					got_frame = &got_frame_local;

				/* encode filtered frame */
				enc_pkt.data = NULL;
				enc_pkt.size = 0;
				av_init_packet(&enc_pkt);
				ret = enc_func(ofmt_ctx->streams[stream_index]->codec, &enc_pkt, frame,
						got_frame);
				av_frame_free(&frame);
				if (ret < 0)
					return ret;
				if (!(*got_frame))
					return 0;

				/* prepare packet for muxing */
				enc_pkt.stream_index = stream_index;
				av_packet_rescale_ts(&enc_pkt,
						ofmt_ctx->streams[stream_index]->codec->time_base,
						ofmt_ctx->streams[stream_index]->time_base);

				/* mux encoded frame */
				ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
			}
			av_frame_free(&frame);
			if (ret < 0)
				goto end;
		} else {
			av_frame_free(&frame);
		}
	}

	av_free_packet(&packet);

	/* flush filters and encoders */
	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		/* flush encoder */
		if (1) {
			int ret;
			int got_frame;

			if (!(ofmt_ctx->streams[stream_index]->codec->codec->capabilities &
			AV_CODEC_CAP_DELAY))
				return 0;

			while (1) {
				ret = encode_write_frame(NULL, stream_index, &got_frame);
				if (ret < 0)
					break;
				if (!got_frame)
					return 0;
			}
		}
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Flushing encoder failed\n");
			goto end;
		}
	}

	av_write_trailer(ofmt_ctx);
	end: av_free_packet(&packet);
	av_frame_free(&frame);
	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		avcodec_close(ifmt_ctx->streams[i]->codec);
		if (ofmt_ctx && ofmt_ctx->nb_streams > i && ofmt_ctx->streams[i]
				&& ofmt_ctx->streams[i]->codec)
			avcodec_close(ofmt_ctx->streams[i]->codec);
		if (filter_ctx && filter_ctx[i].filter_graph)
			avfilter_graph_free(&filter_ctx[i].filter_graph);
	}
	av_free(filter_ctx);
	avformat_close_input(&ifmt_ctx);
	if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
		avio_closep(&ofmt_ctx->pb);
	avformat_free_context(ofmt_ctx);

	if (ret < 0)
		av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_err2str(ret));

	return ret ? 1 : 0;
}
