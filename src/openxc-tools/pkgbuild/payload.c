/*
 * payload - build a pkg Payload/ Scripts (cpio odc streamed through gzip).
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The cpio(1) copy-out is delegated to the system bsdcpio (/usr/bin/cpio)
 * exactly as `find <dir> -print | cpio -o -H odoc | gzip` would produce. The
 * odc record layout is fiddly; rather than re-implement it, we let cpio own it
 * and only own the gzip stream (via zlib) and the enclosing xar.
 *
 * To faithfully replicate Apple pkgbuild's `._*` AppleDouble sidecars (which it
 * emits for any source entry carrying an extended attribute or resource fork),
 * the source tree is staged into a private temp directory: regular files are
 * hard-linked, symlinks recreated, directories rebuilt, and for each
 * xattr-bearing entry a `._<name>` sidecar is materialised with copyfile(3)
 * COPYFILE_PACK. cpio then archives the staged tree.
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
#include <sys/param.h>
#include <sys/xattr.h>
#include <copyfile.h>
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

/* --- path helpers --- */

/* dirname of relpath (no leading "./"). "" for root or top-level entry. */
static void
dirname_of(const char *relpath, char *out, size_t n)
{
	const char *slash = strrchr(relpath, '/');
	if (slash == NULL) {
		out[0] = '\0';
	} else {
		size_t len = (size_t)(slash - relpath);
		memcpy(out, relpath, len);
		out[len] = '\0';
		if (len >= n)
			out[n - 1] = '\0';
	}
}

static const char *
basename_of(const char *relpath)
{
	const char *slash = strrchr(relpath, '/');
	return slash ? slash + 1 : relpath;
}

/* --- recursive staging with AppleDouble sidecars --- */

struct stage_ctx {
	const char *root;
	const char *stage;
	struct svec *names;
	int failed;
};

static int
stage_copy_file(const char *src, const char *dst)
{
	if (link(src, dst) == 0)
		return 0;
	if (errno == EXDEV || errno == ENOTSUP) {
		/* cross-device or unsupported: fall back to a plain copy */
		int in = open(src, O_RDONLY);
		if (in < 0)
			return -1;
		int outfd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (outfd < 0) {
			close(in);
			return -1;
		}
		unsigned char buf[65536];
		ssize_t k;
		int rc = 0;
		while ((k = read(in, buf, sizeof buf)) > 0) {
			if (write(outfd, buf, (size_t)k) != k) {
				rc = -1;
				break;
			}
		}
		close(in);
		close(outfd);
		return rc;
	}
	return -1;
}

static int
has_xattrs(const char *path)
{
	/* listxattr with XATTR_NOFOLLOW reports the entry's own attrs (not a
	 * symlink target's), avoiding spurious descent into linked trees. */
	ssize_t n = listxattr(path, NULL, 0, XATTR_NOFOLLOW);
	if (n < 0)
		return 0;	/* treat errors as "no sidecar" */
	return n > 0;
}

static void
stage_entry(struct stage_ctx *ctx, const char *relpath)
{
	char src[PATH_MAX];
	char dst[PATH_MAX];
	char name[PATH_MAX];

	if (snprintf(src, sizeof src, "%s/%s", ctx->root,
	    relpath[0] ? relpath : ".") >= (int)sizeof src ||
	    snprintf(dst, sizeof dst, "%s/%s", ctx->stage,
	    relpath[0] ? relpath : ".") >= (int)sizeof dst)
		goto fail;

	struct stat st;
	if (lstat(src, &st) != 0)
		return;		/* ignore vanished entries */

	const char *disp = relpath[0] ? relpath : ".";

	if (S_ISDIR(st.st_mode)) {
		if (mkdir(dst, st.st_mode & 0777) != 0 && errno != EEXIST)
			goto fail;
	} else if (S_ISLNK(st.st_mode)) {
		char target[PATH_MAX];
		ssize_t t = readlink(src, target, sizeof target - 1);
		if (t < 0)
			goto fail;
		target[t] = '\0';
		if (symlink(target, dst) != 0 && errno != EEXIST)
			goto fail;
	} else if (S_ISREG(st.st_mode)) {
		if (stage_copy_file(src, dst) != 0)
			goto fail;
	}

	if (relpath[0]) {
		if (snprintf(name, sizeof name, "./%s", disp) >= (int)sizeof name)
			goto fail;
	} else {
		snprintf(name, sizeof name, ".");
	}
	if (svec_push(ctx->names, name) != 0)
		goto fail;

	/* AppleDouble sidecar for xattr/resource-fork-bearing entries. */
	if (relpath[0] && (S_ISDIR(st.st_mode) || S_ISREG(st.st_mode)) &&
	    has_xattrs(src)) {
		char parent[PATH_MAX];
		const char *base = basename_of(relpath);
		dirname_of(relpath, parent, sizeof parent);

		char sid[PATH_MAX];
		snprintf(sid, sizeof sid, "._%s", base);

		char sdst[PATH_MAX];
		char sname[PATH_MAX];
		if (parent[0]) {
			snprintf(sdst, sizeof sdst, "%s/%s/%s", ctx->stage,
			    parent, sid);
			snprintf(sname, sizeof sname, "./%s/%s", parent, sid);
		} else {
			snprintf(sdst, sizeof sdst, "%s/%s", ctx->stage, sid);
			snprintf(sname, sizeof sname, "./%s", sid);
		}

		if (copyfile(src, sdst, NULL, COPYFILE_PACK) == 0) {
			if (svec_push(ctx->names, sname) != 0)
				goto fail;
		}
	}

	if (S_ISDIR(st.st_mode)) {
		/* descend in sorted order, like a real copy */
		DIR *d = opendir(src);
		if (d == NULL)
			return;
		struct dirent *de;
		while ((de = readdir(d)) != NULL) {
			if (strcmp(de->d_name, ".") == 0 ||
			    strcmp(de->d_name, "..") == 0)
				continue;
			char child[PATH_MAX];
			int clen = relpath[0] ?
			    snprintf(child, sizeof child, "%s/%s", relpath,
			        de->d_name) :
			    snprintf(child, sizeof child, "%s", de->d_name);
			if (clen < 0 || (size_t)clen >= sizeof child)
				continue;
			stage_entry(ctx, child);
			if (ctx->failed)
				break;
		}
		closedir(d);
	}
	return;

fail:
	ctx->failed = 1;
}

/* --- cpio -o -H odc reading the name list from a temp file --- */

static int
run_cpio(const char *stage, struct svec *names, struct buf *out)
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
		if (chdir(stage) != 0)
			_exit(127);
		execlp("cpio", "cpio", "-o", "-H", "odc", (char *)NULL);
		_exit(127);
	}

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

	return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

/* --- gzip (RFC 1952) via raw deflate (windowBits = 15 + 16) --- */

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

/* --- recursive remove --- */

static void
remove_tree(const char *path)
{
	struct stat st;
	if (lstat(path, &st) != 0)
		return;
	if (S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode)) {
		DIR *d = opendir(path);
		if (d != NULL) {
			struct dirent *de;
			while ((de = readdir(d)) != NULL) {
				if (strcmp(de->d_name, ".") == 0 ||
				    strcmp(de->d_name, "..") == 0)
					continue;
				char child[PATH_MAX];
				snprintf(child, sizeof child, "%s/%s", path, de->d_name);
				remove_tree(child);
			}
			closedir(d);
		}
		rmdir(path);
	} else {
		unlink(path);
	}
}

int
payload_build(const char *root, struct payload *out)
{
	char tmpl[] = "/tmp/pkgbuild-stage.XXXXXX";
	char *stage = mkdtemp(tmpl);
	if (stage == NULL)
		return -1;

	struct svec names;
	names.v = NULL;
	names.n = 0;
	names.cap = 0;

	struct stage_ctx ctx;
	ctx.root = root;
	ctx.stage = stage;
	ctx.names = &names;
	ctx.failed = 0;
	stage_entry(&ctx, "");

	int rc = -1;
	if (ctx.failed)
		goto done;
	if (names.n == 0)
		goto done;

	struct buf odc;
	buf_init(&odc);
	if (run_cpio(stage, &names, &odc) != 0)
		goto done;

	unsigned char *gz = NULL;
	size_t gzlen = 0;
	if (gzip_bytes(odc.v, odc.n, &gz, &gzlen) != 0)
		goto done;

	out->data = gz;
	out->size = gzlen;
	out->uncompressed = odc.n;
	out->file_count = names.n;
	rc = 0;

done:
	buf_free(&odc);
	svec_free(&names);
	remove_tree(stage);
	return rc;
}
