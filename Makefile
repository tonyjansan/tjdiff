# Makefile for btdiff

CC := gcc
CFLAGS += -O2 -Wall
# Without -g
# CFLAGS += -fomit-frame-pointer

BZIP_DIR := ./libbzip2
OUT_DIR := ./bin

BZIP_OBJS := blocksort.o bzlib.o compress.o crctable.o\
			 decompress.o huffman.o randtable.o
ALL_OBJS := btutils.o $(BZIP_OBJS)

test: $(ALL_OBJS) btdiff.c btpatch.c
	test -d $(OUT_DIR) || mkdir $(OUT_DIR)
	$(CC) $(CFLAGS) $(ALL_OBJS) btdiff.c -o $(OUT_DIR)/btdiff
	$(CC) $(CFLAGS) $(ALL_OBJS) btpatch.c -o $(OUT_DIR)/btpatch

blocksort.o: $(BZIP_DIR)/blocksort.c $(BZIP_DIR)/bzlib_private.h
	$(CC) $(CFLAGS) -c $(BZIP_DIR)/blocksort.c
bzlib.o: $(BZIP_DIR)/bzlib.c $(BZIP_DIR)/bzlib_private.h
	$(CC) $(CFLAGS) -c $(BZIP_DIR)/bzlib.c
compress.o: $(BZIP_DIR)/compress.c $(BZIP_DIR)/bzlib_private.h
	$(CC) $(CFLAGS) -c $(BZIP_DIR)/compress.c
crctable.o: $(BZIP_DIR)/crctable.c $(BZIP_DIR)/bzlib_private.h
	$(CC) $(CFLAGS) -c $(BZIP_DIR)/crctable.c
decompress.o: $(BZIP_DIR)/decompress.c $(BZIP_DIR)/bzlib_private.h
	$(CC) $(CFLAGS) -c $(BZIP_DIR)/decompress.c
huffman.o: $(BZIP_DIR)/huffman.c $(BZIP_DIR)/bzlib_private.h
	$(CC) $(CFLAGS) -c $(BZIP_DIR)/huffman.c
randtable.o: $(BZIP_DIR)/randtable.c $(BZIP_DIR)/bzlib_private.h
	$(CC) $(CFLAGS) -c $(BZIP_DIR)/randtable.c

btutils.o: $(BZIP_DIR)/bzlib_private.h $(BZIP_DIR)/bzlib.h btutils.h
	$(CC) $(CFLAGS) -c btutils.c

.PHONY: clean test
clean:
	rm -r -f $(OUT_DIR) $(ALL_OBJS)