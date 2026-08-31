/*
 * xar - minimal xar(1) archive builder/reader for pkgbuild.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Produces pkgutil-valid .pkg archives. The header layout and the
 * heap-relative <offset> scheme below reproduce a real pkgbuild archive:
 *
 *   [28-byte header] [toc_z] [sha1(toc_z)] [entry data...]
 *
 * The archive checksum is sha1(toc_z) and lives at heap offset 0 (TOC
 * <checksum style="sha1"> offset=0 size=20). pkgutil enforces this.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>
#include <CommonCrypto/CommonDigest.h>

#include "xar.h"

/* --- growable text buffer (for TOC / PackageInfo construction) --- */

struct sbuf {
	unsigned char *buf;
	size_t len;
	size_t cap;
};

static int
sbuf_init(struct sbuf *s)
{
	s->buf = NULL;
	s->len = 0;
	s->cap = 0;
	return 0;
}

static int
sbuf_cat(struct sbuf *s, const void *in, size_t n)
{
	if (s->len + n + 1 > s->cap) {
		size_t cap = s->cap ? s->cap : 512;
		while (cap < s->len + n + 1)
			cap *= 2;
		unsigned char *nd = realloc(s->buf, cap);
		if (nd == NULL)
			return -1;
		s->buf = nd;
		s->cap = cap;
	}
	memcpy(s->buf + s->len, in, n);
	s->len += n;
	s->buf[s->len] = '\0';
	return 0;
}

static int
sbuf_printf(struct sbuf *s, const char *fmt, ...)
{
	va_list ap;
	char *tmp;
	int r;
	va_start(ap, fmt);
	r = vasprintf(&tmp, fmt, ap);
	va_end(ap);
	if (r < 0)
		return -1;
	int rc = sbuf_cat(s, tmp, (size_t)r);
	free(tmp);
	return rc;
}

static void
sbuf_free(struct sbuf *s)
{
	free(s->buf);
	s->buf = NULL;
	s->len = 0;
	s->cap = 0;
}

/* --- big-endian writers --- */

static void
put_be16(uint8_t *p, uint16_t v)
{
	p[0] = (uint8_t)(v >> 8);
	p[1] = (uint8_t)(v);
}

static void
put_be32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 24);
	p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >> 8);
	p[3] = (uint8_t)(v);
}

static uint32_t
get_be32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	    ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* --- compression helpers --- */

static int
zlib_compress(const void *in, size_t n, unsigned char **out, size_t *out_len)
{
	uLong bound = compressBound((uLong)n);
	unsigned char *buf = malloc(bound);
	if (buf == NULL)
		return -1;
	uLongf blen = bound;
	int rc = compress2(buf, &blen, (const Bytef *)in, (uLong)n,
	    Z_DEFAULT_COMPRESSION);
	if (rc != Z_OK) {
		free(buf);
		return -1;
	}
	*out = buf;
	*out_len = (size_t)blen;
	return 0;
}

static void
sha1_hex(const void *data, size_t len, char out[41])
{
	unsigned char digest[20];
	CC_SHA1(data, (CC_LONG)len, digest);
	static const char hex[] = "0123456789abcdef";
	for (int i = 0; i < 20; i++) {
		out[i * 2] = hex[digest[i] >> 4];
		out[i * 2 + 1] = hex[digest[i] & 0xf];
	}
	out[40] = '\0';
}

static void
utc_timestamp(char *out, size_t n)
{
	time_t t = time(NULL);
	struct tm tm;
	gmtime_r(&t, &tm);
	strftime(out, n, "%Y-%m-%dT%H:%M:%SZ", &tm);
}

/* --- public API --- */

unsigned char *
xar_build(const struct xar_entry *entries, int n, size_t *out_len)
{
	if (n < 1) {
		*out_len = 0;
		return NULL;
	}

	unsigned char **comp = calloc((size_t)n, sizeof(*comp));
	size_t *clen = calloc((size_t)n, sizeof(*clen));
	size_t *hoff = calloc((size_t)n, sizeof(*hoff));
	if (comp == NULL || clen == NULL || hoff == NULL)
		goto fail;

	size_t heap_off = 20;	/* 20-byte sha1(toc_z) checksum lives first */
	for (int i = 0; i < n; i++) {
		const struct xar_entry *e = &entries[i];
		if (e->compressed) {
			if (zlib_compress(e->data, e->size, &comp[i], &clen[i]) != 0)
				goto fail;
		} else {
			comp[i] = malloc(e->size);
			if (comp[i] == NULL)
				goto fail;
			memcpy(comp[i], e->data, e->size);
			clen[i] = e->size;
		}
		hoff[i] = heap_off;
		heap_off += clen[i];
	}

	/* Build the table of contents. */
	char ts[32];
	utc_timestamp(ts, sizeof ts);

	struct sbuf toc;
	unsigned char *toc_z = NULL;
	size_t toc_z_len = 0;
	sbuf_init(&toc);
	if (sbuf_printf(&toc,
	    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	    "<xar>\n"
	    " <toc>\n"
	    "  <creation-time>%s</creation-time>\n"
	    "  <checksum style=\"sha1\"><offset>0</offset><size>20</size></checksum>\n",
	    ts) != 0)
		goto fail_toc;

	for (int i = 0; i < n; i++) {
		const struct xar_entry *e = &entries[i];
		char ext[41], arc[41];
		sha1_hex(e->data, e->size, ext);
		sha1_hex(comp[i], clen[i], arc);
		if (sbuf_printf(&toc,
		    "  <file id=\"%d\"><data><length>%zu</length><offset>%zu</offset>"
		    "<size>%zu</size><encoding style=\"%s\"/>"
		    "<extracted-checksum style=\"sha1\">%s</extracted-checksum>"
		    "<archived-checksum style=\"sha1\">%s</archived-checksum>"
		    "</data><mode>0644</mode><type>file</type><name>%s</name></file>\n",
		    i + 1, clen[i], hoff[i], e->size, e->encoding, ext, arc,
		    e->name) != 0)
			goto fail_toc;
	}
	if (sbuf_cat(&toc, " </toc>\n</xar>\n", strlen(" </toc>\n</xar>\n")) != 0)
		goto fail_toc;

	/* Compress the TOC; the archive checksum is sha1 of the compressed TOC. */
	if (zlib_compress(toc.buf, toc.len, &toc_z, &toc_z_len) != 0)
		goto fail_toc;

	unsigned char chk[20];
	CC_SHA1(toc_z, (CC_LONG)toc_z_len, chk);

	size_t arch_len = 28 + toc_z_len + 20 + heap_off - 20;
	unsigned char *arch = malloc(arch_len);
	if (arch == NULL)
		goto fail_arch;

	uint32_t toclen_u = (uint32_t)toc.len;
	uint32_t toczlen_u = (uint32_t)toc_z_len;
	size_t p = 0;
	memcpy(arch + p, "xar!", 4); p += 4;
	put_be16(arch + p, 28); p += 2;		/* header_size */
	put_be16(arch + p, 1); p += 2;		/* version */
	put_be32(arch + p, 0); p += 4;		/* ? */
	put_be32(arch + p, toczlen_u); p += 4;	/* toc_length_compressed */
	put_be32(arch + p, 0); p += 4;
	put_be32(arch + p, toclen_u); p += 4;	/* toc_length_uncompressed */
	put_be32(arch + p, 1); p += 4;		/* checksum style: sha1 */

	memcpy(arch + p, toc_z, toc_z_len); p += toc_z_len;
	memcpy(arch + p, chk, 20); p += 20;
	for (int i = 0; i < n; i++) {
		memcpy(arch + p, comp[i], clen[i]);
		p += clen[i];
	}

	*out_len = arch_len;

	for (int i = 0; i < n; i++)
		free(comp[i]);
	free(comp); free(clen); free(hoff);
	sbuf_free(&toc);
	free(toc_z);
	return arch;

fail_arch:
	free(arch);
fail_toc:
	free(toc_z);
	sbuf_free(&toc);
fail:
	for (int i = 0; i < n; i++)
		free(comp[i]);
	free(comp); free(clen); free(hoff);
	*out_len = 0;
	return NULL;
}

int
xar_dump(const unsigned char *pkg, size_t len)
{
	if (len < 28 || memcmp(pkg, "xar!", 4) != 0) {
		fprintf(stderr, "xar: not a xar archive\n");
		return -1;
	}
	uint32_t tocz_len = get_be32(pkg + 12);
	uint32_t toc_len = get_be32(pkg + 20);
	if (tocz_len == 0 || 28 + tocz_len > len) {
		fprintf(stderr, "xar: corrupt table of contents\n");
		return -1;
	}
	unsigned char *toc = NULL;
	uLongf tlen = (uLongf)toc_len;
	if (tlen == 0)
		tlen = (uLongf)(len - 28);
	toc = malloc(tlen > 1024 ? tlen : 1024);
	if (toc == NULL)
		return -1;
	uLongf blen = tlen;
	if (uncompress(toc, &blen, pkg + 28, tocz_len) != Z_OK) {
		free(toc);
		fprintf(stderr, "xar: failed to decompress table of contents\n");
		return -1;
	}

	/* Naive scan for <name>...</name> entries. */
	char *toc_s = (char *)toc;
	char *p = toc_s;
	for (;;) {
		char *n = strstr(p, "<name>");
		if (n == NULL)
			break;
		n += 6;
		char *e = strstr(n, "</name>");
		if (e == NULL)
			break;
		*e = '\0';
		printf("%s\n", n);
		p = e + 7;
	}
	free(toc);
	return 0;
}
