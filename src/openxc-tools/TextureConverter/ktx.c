/*
 * ktx.c -- reading Khronos texture containers.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <string.h>

#include "ktx.h"

static const uint8_t ktx1_id[12] = {
	0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};
static const uint8_t ktx2_id[12] = {
	0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};

static uint32_t
le32(const uint8_t *p)
{
	return ((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	    ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

/*
 * Both versions store their key/value block the same way: a length, then
 * that many bytes of a NUL-terminated key followed by its value, then
 * padding to the next multiple of four.
 */
static void
read_kv(struct ktx *k, const uint8_t *p, size_t len)
{
	size_t off = 0;

	while (off + 4 <= len) {
		uint32_t n = le32(p + off);
		const uint8_t *blob = p + off + 4;
		const void *nul;
		struct ktx_kv *nkv;
		size_t klen;

		off += 4;
		if (n == 0 || n > len - off)
			break;
		off += n;
		off = (off + 3) & ~(size_t)3;

		if ((nul = memchr(blob, '\0', n)) == NULL)
			continue;
		klen = (size_t)((const uint8_t *)nul - blob);
		if ((nkv = realloc(k->kv, (k->nkv + 1) * sizeof(*nkv))) == NULL)
			return;
		k->kv = nkv;
		nkv = &k->kv[k->nkv];
		nkv->value_len = n - klen - 1;
		if ((nkv->key = malloc(klen + 1)) == NULL)
			return;
		memcpy(nkv->key, blob, klen);
		nkv->key[klen] = '\0';
		if ((nkv->value = malloc(nkv->value_len + 1)) == NULL) {
			free(nkv->key);
			return;
		}
		memcpy(nkv->value, blob + klen + 1, nkv->value_len);
		nkv->value[nkv->value_len] = '\0';
		k->nkv++;
	}
}

bool
ktx_parse(const void *bytes, size_t len, struct ktx *out)
{
	const uint8_t *p = bytes;

	memset(out, 0, sizeof(*out));
	if (len >= 64 && memcmp(p, ktx1_id, sizeof(ktx1_id)) == 0) {
		uint32_t kvlen;

		out->version = 1;
		/* p[12] is the endianness marker; only little is written. */
		out->gl_internal_format = le32(p + 28);
		out->width = le32(p + 36);
		out->height = le32(p + 40);
		out->depth = le32(p + 44);
		out->layers = le32(p + 48);
		out->faces = le32(p + 52);
		out->levels = le32(p + 56);
		kvlen = le32(p + 60);
		if (kvlen <= len - 64)
			read_kv(out, p + 64, kvlen);
		return (true);
	}
	if (len >= 80 && memcmp(p, ktx2_id, sizeof(ktx2_id)) == 0) {
		uint32_t kvoff, kvlen;

		out->version = 2;
		out->vk_format = le32(p + 12);
		out->width = le32(p + 20);
		out->height = le32(p + 24);
		out->depth = le32(p + 28);
		out->layers = le32(p + 32);
		out->faces = le32(p + 36);
		out->levels = le32(p + 40);
		/*
		 * The index is four 32-bit words -- the descriptor's offset
		 * and length, then the key/value block's -- before the two
		 * 64-bit ones for supercompression.
		 */
		kvoff = le32(p + 56);
		kvlen = le32(p + 60);
		if (kvoff < len && kvlen <= len - kvoff)
			read_kv(out, p + kvoff, (size_t)kvlen);
		return (true);
	}
	return (false);
}

void
ktx_free(struct ktx *k)
{
	size_t i;

	for (i = 0; i < k->nkv; i++) {
		free(k->kv[i].key);
		free(k->kv[i].value);
	}
	free(k->kv);
	memset(k, 0, sizeof(*k));
}

const char *
ktx_value(const struct ktx *k, const char *key)
{
	size_t i;

	for (i = 0; i < k->nkv; i++) {
		if (strcmp(k->kv[i].key, key) == 0)
			return (k->kv[i].value);
	}
	return (NULL);
}
