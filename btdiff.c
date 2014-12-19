#include <stdio.h>
#include "btutils.h"

int main(int argc, char **argv) {
	if (argc < 4) {
		printf("usage: %s oldfile newfile patchfile\n", argv[0]);
		return 0;
	}
	return btdiff(argv[1], argv[2], argv[3]);
}