/*
 * payload - build a pkg Payload (cpio odc streamed through gzip).
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The cpio(1) copy-out is delegated to the system bsdcpio (/usr/bin/cpio)
 * exactly as `find <root> -print | cpio -o -H odc | gzip` would produce. The
 * odc record layout is fiddly; rather than re-implement it, we let cpio own it
 * and only own the gzip stream (via zlib) and the enclosing xar.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <zlib.h>

#include "payload.h"

/* --- growable string list --- */

struct svec {
	char **v;
	int n;
	int cap;
};

static int
svec_push(struct svec *s, const char *name)
{
	if (s->n + 1 > s->cap) {
		int cap = s->cap ? s->cap : 32;
		while (cap < s->n + 1)
			cap *= 2;
		char **nv = realloc(s->v, (size_t)cap * sizeof(*nv));
		if (nv == NULL)
			return -1;
		s->v = nv;
		s->cap = cap;
	}
	char *dup = strdup(name);
	if (dup == NULL)
		return -1;
	s->v[s->n++] = dup;
	return 0;
}

static void
svec_free(struct svec *s)
{
	for (int i = 0; i < s->n; i++)
		free(s->v[i]);
	free(s->v);
	s->v = NULL;
	s->n = 0;
	s->cap = 0;
}

/* --- growable byte buffer --- */

struct buf {
	unsigned char *v;
	size_t n;
	size_t cap;
};

static void
buf_init(struct buf *b)
{
	b->v = NULL;
	b->n = 0;
	b->cap = 0;
}

static int
buf_append(struct buf *b, const void *in, size_t n)
{
	if (b->n + n > b->cap) {
		size_t cap = b->cap ? b->cap : 8192;
		while (cap < b->n + n)
			cap *= 2;
		unsigned char *nv = realloc(b->v, cap);
		if (nv == NULL)
			return -1;
		b->v = nv;
		b->cap = cap;
	}
	memcpy(b->v + b->n, in, n);
	b->n += n;
	return 0;
}

static void
buf_free(struct buf *b)
{
	free(b->v);
	b->v = NULL;
	b->n = 0;
	b->cap = 0;
}

/* --- recursive walk producing "./rel" paths (dir listed before its children) */

static int
walk_path(struct svec *s, const char *abspath, const char *relpath)
{
	char name[PATH_MAX];

	if (relpath == NULL || relpath[0] == '\0')
		snprintf(name, sizeof name, ".");
	else
		snprintf(name, sizeof name, "./%s", relpath);

	if (svec_push(s, name) != 0)
		return -1;

	struct stat st;
	if (lstat(abspath, &st) != 0)
		return 0;	/* ignore paths that vanish mid-walk */
	if (!S_ISDIR(st.st_mode))
		return 0;	/* file or symlink: list, do not descend */

	DIR *d = opendir(abspath);
	if (d == NULL)
		return 0;

	struct dirent *de;
	while ((de = readdir(d)) != NULL) {
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
			continue;
		char child_abs[PATH_MAX];
		char child_rel[PATH_MAX];
		if (snprintf(child_abs, sizeof child_abs, "%s/%s", abspath, de->d_name)
		    >= (int)sizeof child_abs)
			continue;
		if (relpath == NULL || relpath[0] == '\0')
			snprintf(child_rel, sizeof child_rel, "%s", de->d_name);
		else
			snprintf(child_rel, sizeof child_rel, "%s/%s", relpath, de->d_name);
		if (walk_path(s, child_abs, child_rel) != 0) {
			closedir(d);
			return -1;
		}
	}
	closedir(d);
	return 0;
}

/* --- delegate cpio -o -H odc reading the name list from a temp file --- */

static int
run_cpio(const char *root, struct svec *names, struct buf *out)
{
	char tmpl[] = "/tmp/pkgbuild.XXXXXX";
	int tf = mkstemp(tmpl);
	if (tf < 0)
		return -1;
	for (int i = 0; i < names->n; i++) {
		if (write(tf, names->v[i], strlen(names->v[i])) < 0 ||
		    write(tf, "\n", 1) < 0) {
			close(tf);
			unlink(tmpl);
			return -1;
		}
	}
	close(tf);

	int pipefd[2];
	if (pipe(pipefd) != 0) {
		unlink(tmpl);
		return -1;
	}

	pid_t pid = fork();
	if (pid < 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		unlink(tmpl);
		return -1;
	}
	if (pid == 0) {
		/* child: feed the name list on stdin, capture cpio stdout */
		int in = open(tmpl, O_RDONLY);
		if (in >= 0) {
			dup2(in, STDIN_FILENO);
			close(in);
		}
		dup2(pipefd[1], STDOUT_FILENO);
		close(pipefd[0]);
		close(pipefd[1]);
		int dev = open("/dev/null", O_WRONLY);
		if (dev >= 0) {
			dup2(dev, STDERR_FILENO);
			close(dev);
		}
		if (chdir(root) != 0)
			_exit(127);
		execlp("cpio", "cpio", "-o", "-H", "odc", (char *)NULL);
		_exit(127);
	}

	/* parent: read the archive */
	close(pipefd[1]);
	char tmp[8192];
	ssize_t k;
	while ((k = read(pipefd[0], tmp, sizeof tmp)) > 0) {
		if (buf_append(out, tmp, (size_t)k) != 0) {
			close(pipefd[0]);
			waitpid(pid, NULL, 0);
			unlink(tmpl);
			return -1;
		}
	}
	close(pipefd[0]);
	int status = 0;
	waitpid(pid, &status, 0);
	unlink(tmpl);

	if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
		return 0;
	return -1;
}

/* --- gzip (RFC 1952) via raw deflate with the gzip wrapper --- */

static int
gzip_bytes(const void *in, size_t n, unsigned char **out, size_t *out_len)
{
	z_stream s;
	memset(&s, 0, sizeof s);
	if (deflateInit2(&s, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8,
	    Z_DEFAULT_STRATEGY) != Z_OK)
		return -1;

	struct buf b;
	buf_init(&b);

	s.next_in = (Bytef *)in;
	s.avail_in = (uInt)n;

	for (;;) {
		unsigned char chunk[16384];
		s.next_out = chunk;
		s.avail_out = (uInt)sizeof chunk;
		int flush = (s.avail_in == 0) ? Z_FINISH : Z_NO_FLUSH;
		int r = deflate(&s, flush);
		size_t have = sizeof chunk - s.avail_out;
		if (have > 0) {
			if (buf_append(&b, chunk, have) != 0) {
				deflateEnd(&s);
				buf_free(&b);
				return -1;
			}
		}
		if (r == Z_STREAM_END)
			break;
		if (r != Z_OK) {
			deflateEnd(&s);
			buf_free(&b);
			return -1;
		}
	}
	deflateEnd(&s);

	*out = b.v;
	*out_len = b.n;
	return 0;
}

int
payload_build(const char *root, struct payload *out)
{
	struct svec names;
	names.v = NULL;
	names.n = 0;
	names.cap = 0;

	if (walk_path(&names, root, NULL) != 0) {
		svec_free(&names);
		return -1;
	}

	struct buf odc;
	buf_init(&odc);
	if (run_cpio(root, &names, &odc) != 0) {
		buf_free(&odc);
		svec_free(&names);
		return -1;
	}

	unsigned char *gz = NULL;
	size_t gzlen = 0;
	if (gzip_bytes(odc.v, odc.n, &gz, &gzlen) != 0) {
		buf_free(&odc);
		svec_free(&names);
		return -1;
	}

	out->data = gz;
	out->size = gzlen;
	out->uncompressed = odc.n;
	out->file_count = names.n;
	buf_free(&odc);
	svec_free(&names);
	return 0;
}
