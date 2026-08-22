/*
 * cs_file.c - File I/O helpers.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "codesign.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

uint8_t *
cs_read_file(const char *path, size_t *size)
{
	FILE *fp = fopen(path, "rb");
	if (!fp)
		return NULL;

	if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return NULL; }
	long sz = ftell(fp);
	if (sz < 0) { fclose(fp); return NULL; }
	rewind(fp);

	uint8_t *buf = malloc((size_t)sz);
	if (!buf) { fclose(fp); return NULL; }

	size_t rd = fread(buf, 1, (size_t)sz, fp);
	fclose(fp);

	if (rd != (size_t)sz) {
		free(buf);
		return NULL;
	}

	*size = (size_t)sz;
	return buf;
}

int
cs_write_file(const char *path, const uint8_t *data, size_t size)
{
	FILE *fp = fopen(path, "wb");
	if (!fp)
		return -1;

	size_t wr = fwrite(data, 1, size, fp);
	fclose(fp);

	return (wr == size) ? 0 : -1;
}

int
cs_file_exists(const char *path)
{
	FILE *fp = fopen(path, "rb");
	if (!fp)
		return 0;
	fclose(fp);
	return 1;
}

int
cs_file_size(const char *path, size_t *size)
{
	struct stat st;
	if (stat(path, &st) != 0)
		return -1;
	*size = (size_t)st.st_size;
	return 0;
}

int
cs_is_directory(const char *path)
{
	struct stat st;
	if (stat(path, &st) != 0)
		return 0;
	return S_ISDIR(st.st_mode);
}

char *
cs_basename(const char *path)
{
	const char *slash = strrchr(path, '/');
	if (!slash)
		return strdup(path);
	return strdup(slash + 1);
}
