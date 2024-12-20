/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <unistd.h>
#include <fcntl.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#define EXPECTED_MAGIC 0x96f3b83d
#define LZMA_HEADER_SIZE 2
#define FLAG_LZMA2 0x400
#define FLAG_ARM_THUMB 0x800

struct __attribute__((__packed__))  image_header {
	uint32_t magic;
	uint32_t unused0;
	uint16_t hdr_size;
	uint16_t unused1;
	uint32_t img_size;
	uint32_t flags;
};

int main(int argc, char *argv[])
{
	if (argc != 3) {
		printf("needs 2 parameters: signed image file and application binary file\n\r");
		return EXIT_FAILURE;
	}

	int app_fd = open(argv[2], O_NONBLOCK);

	if (app_fd < 0) {
		printf("Opening signed image failed.\n\r");
		return EXIT_FAILURE;
	}
	int signed_fd = open(argv[1], O_NONBLOCK);

	if (signed_fd < 0) {
		printf("Opening signed image failed.\n\r");
		return EXIT_FAILURE;
	}
	system("mkdir -p tmp; rm -rf tmp/stream*");
	struct image_header ih;
	size_t rc = pread(signed_fd, &ih, sizeof(struct image_header), 0);

	if (ih.magic != EXPECTED_MAGIC) {
		printf("Expected magic value at the start of signed image.\n\r");
		printf("Input files in wrong order?\n\r");
		return EXIT_FAILURE;
	}
	if (!ih.flags & FLAG_LZMA2) {
		printf("Signed image is not compressed with LZMA2.\n\r");
		return EXIT_FAILURE;
	}
	int lzma_stream_size = ih.img_size - LZMA_HEADER_SIZE;
	int lzma_stream_offset = ih.hdr_size + LZMA_HEADER_SIZE;
	uint8_t *lzma_buf = (uint8_t *)malloc(lzma_stream_size);

	rc = pread(signed_fd, lzma_buf, lzma_stream_size, lzma_stream_offset);
	if (rc != lzma_stream_size) {
		printf("Error while reading compressed stream from signed image.\n\r");
		return EXIT_FAILURE;
	}
	int lzma_fd = creat("tmp/stream.lzma", 0600);

	write(lzma_fd, lzma_buf, lzma_stream_size);
	close(lzma_fd);
	if (ih.flags & FLAG_ARM_THUMB) {
		system("unlzma --armthumb --lzma2 --format=raw --suffix=.lzma tmp/stream.lzma");
	} else {
		system("unlzma --lzma2 --format=raw --suffix=.lzma tmp/stream.lzma");
	}
	int unlzma_fd = open("tmp/stream", O_NONBLOCK);
	int unlzma_size = lseek(unlzma_fd, 0L, SEEK_END);
	int app_size = lseek(app_fd, 0L, SEEK_END);

	if (app_size != unlzma_size) {
		printf("Decompressed stream size and application size mismatch.\n\r");
		return EXIT_FAILURE;
	}
	uint8_t *unlzma_buf = (uint8_t *)malloc(unlzma_size);
	uint8_t *app_buf = (uint8_t *)malloc(app_size);

	rc = pread(app_fd, app_buf, app_size, 0);
	if (rc != app_size) {
		printf("Error while loading application binary.\n\r");
		return EXIT_FAILURE;
	}
	rc = pread(unlzma_fd, unlzma_buf, unlzma_size, 0);
	if (rc != unlzma_size) {
		printf("Error while loading decompressed stream.\n\r");
		return EXIT_FAILURE;
	}
	for (int i = 0; i < app_size; i++) {
		if (app_buf[i] != unlzma_buf[i]) {
			printf("Diff at %d\r\n", i);
			return EXIT_FAILURE;
		}
	}
	close(unlzma_fd);
	close(app_fd);
	printf("All checks OK.\n\r");
	return 0;
}
