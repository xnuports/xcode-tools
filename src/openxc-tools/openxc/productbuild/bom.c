/*
 * bom - generate a Bill Of Materials for a source root via mkbom(1).
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "bom.h"

unsigned char *
bom_build(const char *root, size_t *out_len)
{
	char tmpl[] = "/tmp/productbuild.bom.XXXXXX";
	int tf = mkstemp(tmpl);
	if (tf < 0)
		return NULL;

	pid_t pid = fork();
	if (pid < 0) {
		close(tf);
		unlink(tmpl);
		return NULL;
	}
	if (pid == 0) {
		close(tf);
		execlp("mkbom", "mkbom", root, tmpl, (char *)NULL);
		_exit(127);
	}
	int status = 0;
	waitpid(pid, &status, 0);
	if (!(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
		unlink(tmpl);
		return NULL;
	}

	int fd = open(tmpl, O_RDONLY);
	if (fd < 0) {
		unlink(tmpl);
		return NULL;
	}
	struct stat st;
	if (fstat(fd, &st) != 0 || st.st_size <= 0) {
		close(fd);
		unlink(tmpl);
		return NULL;
	}

	unsigned char *buf = malloc((size_t)st.st_size);
	if (buf == NULL) {
		close(fd);
		unlink(tmpl);
		return NULL;
	}
	size_t want = (size_t)st.st_size;
	size_t got = 0;
	while (got < want) {
		ssize_t r = read(fd, buf + got, want - got);
		if (r < 0) {
			free(buf);
			close(fd);
			unlink(tmpl);
			return NULL;
		}
		if (r == 0)
			break;
		got += (size_t)r;
	}
	close(fd);
	unlink(tmpl);

	if (got != want) {
		free(buf);
		return NULL;
	}
	*out_len = got;
	return buf;
}
