#include "transcode.h"

int main(int argc, char **argv) {
	if (argc != 3) {
		av_log(NULL, AV_LOG_ERROR, "Usage: %s <input file> <output file>\n",
				argv[0]);
		return 1;
	}
	const char *src_file = argv[1];
	const char *dst_file = argv[2];

	int ret = transcode(src_file, dst_file);
	return ret;
}
