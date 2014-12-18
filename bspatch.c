#include <stdio.h>
#include "utils.h"

int main(int argc, char **argv) {
	if (argc < 4) {
		printf("usage: %s oldfile newfile patchfile\n", argv[0]);
		return 0;
	}
	return patch(argv[1], argv[2], argv[3]);
}