#include "demux.h"

int main(int argc, char **argv) {
	int ret = 0;

	if (argc != 4 && argc != 5) {
		fprintf(stderr,
				"usage: %s [-refcount=<old|new_norefcount|new_refcount>] "
						"input_file video_output_file audio_output_file\n"
						"API example program to show how to read frames from an input file.\n"
						"This program reads frames from a file, decodes them, and writes decoded\n"
						"video frames to a rawvideo file named video_output_file, and decoded\n"
						"audio frames to a rawaudio file named audio_output_file.\n\n"
						"If the -refcount option is specified, the program use the\n"
						"reference counting frame system which allows keeping a copy of\n"
						"the data for longer than one decode call. If unset, it's using\n"
						"the classic old method.\n"
						"\n", argv[0]);
		exit(1);
	}
//	if (argc == 5) {
//		const char *mode = argv[1] + strlen("-refcount=");
//		if (!strcmp(mode, "old"))
//			api_mode = API_MODE_OLD;
//		else if (!strcmp(mode, "new_norefcount"))
//			api_mode = API_MODE_NEW_API_NO_REF_COUNT;
//		else if (!strcmp(mode, "new_refcount"))
//			api_mode = API_MODE_NEW_API_REF_COUNT;
//		else {
//			fprintf(stderr, "unknow mode '%s'\n", mode);
//			exit(1);
//		}
//		argv++;
//	}
	const char *src_file = argv[1];
	const char *video_dst_file = argv[2];
	const char *audio_dst_file = argv[3];

	ret = demux(src_file, video_dst_file, audio_dst_file);

	return ret < 0;
}
