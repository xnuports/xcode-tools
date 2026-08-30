/*
 * cs_blob.c - Code signature blob construction.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "codesign.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/cms.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>
#include <openssl/asn1.h>
#include <openssl/obj_mac.h>
#include <openssl/bio.h>
#include <openssl/obj_mac.h>
#include <openssl/bio.h>
#include <openssl/err.h>

/* ---- Big-endian helpers ---- */

void
be_write32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 24);
	p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >> 8);
	p[3] = (uint8_t)(v);
}

uint32_t
be_read32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	       ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

void
be_write64(uint8_t *p, uint64_t v)
{
	p[0] = (uint8_t)(v >> 56);
	p[1] = (uint8_t)(v >> 48);
	p[2] = (uint8_t)(v >> 40);
	p[3] = (uint8_t)(v >> 32);
	p[4] = (uint8_t)(v >> 24);
	p[5] = (uint8_t)(v >> 16);
	p[6] = (uint8_t)(v >> 8);
	p[7] = (uint8_t)(v);
}

uint64_t
be_read64(const uint8_t *p)
{
	return ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48) |
	       ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32) |
	       ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) |
	       ((uint64_t)p[6] << 8) | (uint64_t)p[7];
}

/* ---- Hash helpers ---- */

void
sha1_raw(const uint8_t *data, size_t len, uint8_t *out)
{
	SHA1(data, len, out);
}

void
sha256_raw(const uint8_t *data, size_t len, uint8_t *out)
{
	SHA256(data, len, out);
}

/* ---- SuperBlob builder ---- */

void
sbb_init(struct superblob_builder *b)
{
	memset(b, 0, sizeof(*b));
}

int
sbb_add(struct superblob_builder *b, uint32_t type,
    const uint8_t *data, size_t len)
{
	if (b->count >= CSB_MAX_BLOBS)
		return -1;

	b->blobs[b->count].type = type;
	b->blobs[b->count].data = data;
	b->blobs[b->count].data_len = len;
	b->count++;
	b->total_len += len;
	return 0;
}

/*
 * Order the index by slot type.
 *
 * The embedded-signature index has to be sorted ascending by type: the
 * loader looks slots up by binary search, so an unsorted index is not
 * merely untidy, it makes the whole signature unreadable -- macOS
 * reports "code object is not signed at all" even though every blob is
 * present and well formed.
 *
 * Sorting here rather than relying on the order sbb_add() is called in
 * means a caller cannot reintroduce the problem by adding a slot in the
 * wrong place.  That is exactly how it arose: the alternate SHA-1
 * CodeDirectory (0x1000) was added straight after the primary one, so
 * it landed ahead of requirements (2) and entitlements (5).  Ad-hoc
 * signing has no alternate directory, which is why only certificate
 * signing was affected.
 */
static void
sbb_sort(struct superblob_builder *b)
{
	int i, j;

	for (i = 1; i < b->count; i++) {
		struct cs_blob_entry key = b->blobs[i];

		for (j = i - 1; j >= 0 && b->blobs[j].type > key.type; j--)
			b->blobs[j + 1] = b->blobs[j];
		b->blobs[j + 1] = key;
	}
}

int
sbb_emit(struct superblob_builder *b, uint8_t *out, size_t *out_len)
{
	size_t hdr_len = 12 + (size_t)b->count * 8;
	size_t total = hdr_len + b->total_len;
	size_t off = hdr_len;
	int i;

	if (*out_len < total)
		return -1;

	sbb_sort(b);

	be_write32(out, CSMAGIC_EMBEDDED_SIGNATURE);
	be_write32(out + 4, (uint32_t)total);
	be_write32(out + 8, b->count);

	for (i = 0; i < b->count; i++) {
		be_write32(out + 12 + (size_t)i * 8, b->blobs[i].type);
		be_write32(out + 12 + (size_t)i * 8 + 4, (uint32_t)off);
		off += b->blobs[i].data_len;
	}

	off = hdr_len;
	for (i = 0; i < b->count; i++) {
		if (b->blobs[i].data && b->blobs[i].data_len > 0)
			memcpy(out + off, b->blobs[i].data, b->blobs[i].data_len);
		off += b->blobs[i].data_len;
	}

	*out_len = total;
	return 0;
}

/* ---- CodeDirectory builder ---- */

size_t
build_code_directory(struct code_directory *cd, uint8_t *out, size_t out_len)
{
size_t header_len = 88;
	size_t ident_len = strlen(cd->identifier) + 1;
	size_t team_len = cd->team_id ? (strlen(cd->team_id) + 1) : 0;
	size_t special_len = (size_t)cd->n_special_slots * cd->hash_size;
	size_t code_len = (size_t)cd->n_code_slots * cd->hash_size;
	size_t total = header_len + ident_len + team_len +
	               special_len + code_len;

	if (out_len < total)
		return 0;

	memset(out, 0, total);

	be_write32(out, CSMAGIC_CODEDIRECTORY);
	be_write32(out + 4, (uint32_t)total);
	be_write32(out + 8, CS_CD_VERSION);
	be_write32(out + 12, cd->flags);
	be_write32(out + 16, 0);                         /* hashOffset (filled later) */
	be_write32(out + 20, (uint32_t)header_len);      /* identOffset */
	be_write32(out + 24, cd->n_special_slots);
	be_write32(out + 28, cd->n_code_slots);
	be_write32(out + 32, (uint32_t)cd->code_limit);
	out[36] = (uint8_t)cd->hash_size;
	out[37] = (uint8_t)cd->hash_type;
	out[38] = 0;                                      /* spare1 */
	out[39] = (uint8_t)cd->page_size;
	be_write32(out + 40, 0);                          /* spare2 */
	be_write32(out + 44, 0);                          /* scatterOffset */
	be_write32(out + 48, cd->team_id ?
	           (uint32_t)(header_len + ident_len) : 0); /* teamOffset */
	be_write32(out + 52, 0);                          /* spare3 */
	be_write64(out + 56, cd->code_limit64);           /* codeLimit64 */
	be_write64(out + 64, cd->exec_seg_base);          /* execSegBase */
	be_write64(out + 72, cd->exec_seg_limit);         /* execSegLimit */
	be_write64(out + 80, cd->exec_seg_flags);         /* execSegFlags */

	memcpy(out + header_len, cd->identifier, ident_len);
	if (cd->team_id)
		memcpy(out + header_len + ident_len, cd->team_id, team_len);

	size_t hash_off = header_len + ident_len + team_len;
	size_t code_hash_off = hash_off + special_len;
		be_write32(out + 16, (uint32_t)code_hash_off);

	if (cd->n_special_slots > 0 && cd->special_hash)
		memcpy(out + hash_off, cd->special_hash, special_len);

	if (cd->n_code_slots > 0 && cd->code_hash)
		memcpy(out + code_hash_off, cd->code_hash, code_len);

	return total;
}

/* ---- Requirements blob ---- */

/*
 * Build a requirements vector.  When cert_cn is NULL or empty,
 * the requirements are empty (count=0).
 *
 * Designated requirement for certificate signing:
 *   identifier "bundleId" and anchor apple generic
 *     and certificate leaf[subject.CN] = "CN"
 *     and certificate 1[field.1.2.840.113635.100.6.2.1] (exists)
 */
static void
append_be32(uint8_t *buf, size_t *off, uint32_t v)
{
	be_write32(buf + *off, v);
	*off += 4;
}

static void
append_padded_str(uint8_t *buf, size_t *off, const char *s)
{
	uint32_t len = (uint32_t)strlen(s);
	append_be32(buf, off, len);
	memcpy(buf + *off, s, len);
	*off += len;
	size_t pad = (4 - (len % 4)) % 4;
	if (pad) { memset(buf + *off, 0, pad); *off += pad; }
}

static void
append_padded_bytes(uint8_t *buf, size_t *off,
    const uint8_t *s, uint32_t len)
{
	append_be32(buf, off, len);
	memcpy(buf + *off, s, len);
	*off += len;
	size_t pad = (4 - (len % 4)) % 4;
	if (pad) { memset(buf + *off, 0, pad); *off += pad; }
}

size_t
build_requirements_blob(const char *bundle_id,
    const char *cert_cn, uint8_t *out, size_t out_len)
{
	if (!cert_cn || !*cert_cn) {
		size_t total = 12;
		if (out_len < total)
			return 0;
		be_write32(out, CSMAGIC_REQUIREMENTS);
		be_write32(out + 4, 12);
		be_write32(out + 8, 0);
		return total;
	}

	uint8_t expr[2048];
	size_t eo = 0;

	append_be32(expr, &eo, kReqOpAnd);
	append_be32(expr, &eo, kReqOpIdent);
	append_padded_str(expr, &eo, bundle_id);
	append_be32(expr, &eo, kReqOpAnd);
	append_be32(expr, &eo, kReqOpAppleGenericAnchor);
	append_be32(expr, &eo, kReqOpAnd);
	append_be32(expr, &eo, kReqOpCertField);
	append_be32(expr, &eo, 0);  /* leaf */
	append_padded_str(expr, &eo, "subject.CN");
	append_be32(expr, &eo, kReqMatchEqual);
	append_padded_str(expr, &eo, cert_cn);
	append_be32(expr, &eo, kReqOpCertGeneric);
	append_be32(expr, &eo, 1);  /* intermediate */
	{
		static const uint8_t oid[] = {
			0x2A, 0x86, 0x48, 0x86, 0xF7, 0x63, 0x64, 0x06, 0x02, 0x01
		};
		append_padded_bytes(expr, &eo, oid, sizeof(oid));
	}
	append_be32(expr, &eo, kReqMatchExists);

	/*
	 * A requirement blob is magic, length, kind, then the expression.
	 * Without the kind word the first operator is read as the kind
	 * and the whole requirement is rejected as an unsupported type.
	 */
	size_t inner_len = 12 + eo;
	size_t total = 20 + inner_len;

	if (out_len < total)
		return 0;

	be_write32(out, CSMAGIC_REQUIREMENTS);
	be_write32(out + 4, (uint32_t)total);
	be_write32(out + 8, 1);
	be_write32(out + 12, kSecDesignatedRequirementType);
	be_write32(out + 16, 20);
	be_write32(out + 20, CSMAGIC_REQUIREMENT);
	be_write32(out + 24, (uint32_t)inner_len);
	be_write32(out + 28, kReqKindExpression);
	memcpy(out + 32, expr, eo);

	return total;
}

/* ---- Entitlements (XML) ---- */

size_t
build_entitlements_xml(const char *plist_xml, uint8_t *out, size_t out_len)
{
	size_t xml_len = strlen(plist_xml);
	size_t total = 8 + xml_len;
	if (out_len < total)
		return 0;

	be_write32(out, CSMAGIC_EMBEDDED_ENTITLEMENTS);
	be_write32(out + 4, (uint32_t)total);
	memcpy(out + 8, plist_xml, xml_len);
	return total;
}

/* ---- Entitlements (DER) ---- */

static size_t
der_len_size(size_t len)
{
	if (len < 0x80) return 1;
	if (len < 0x100) return 2;
	if (len < 0x10000) return 3;
	return 4;
}

static void
der_put_len(uint8_t *buf, size_t *off, size_t len)
{
	if (len < 0x80) {
		buf[(*off)++] = (uint8_t)len;
	} else if (len < 0x100) {
		buf[(*off)++] = 0x81;
		buf[(*off)++] = (uint8_t)len;
	} else if (len < 0x10000) {
		buf[(*off)++] = 0x82;
		buf[(*off)++] = (uint8_t)(len >> 8);
		buf[(*off)++] = (uint8_t)len;
	} else {
		buf[(*off)++] = 0x83;
		buf[(*off)++] = (uint8_t)(len >> 16);
		buf[(*off)++] = (uint8_t)(len >> 8);
		buf[(*off)++] = (uint8_t)len;
	}
}

struct plist_entry {
	char *key;
	char *val;
	int is_bool;
	int bool_val;
};

static void
parse_plist_entries(const char *plist, struct plist_entry *entries,
    int max_entries, int *count)
{
	const char *p = plist;
	int n = 0;

	while (*p && n < max_entries) {
		if (strncmp(p, "<key>", 5) == 0) {
			p += 5;
			const char *ks = p;
			while (*p && strncmp(p, "</key>", 6) != 0)
				p++;
			if (!*p) break;

			char *k = malloc(p - ks + 1);
			memcpy(k, ks, p - ks);
			k[p - ks] = '\0';
			p += 6;

			entries[n].key = k;

			while (*p == ' ' || *p == '\n' || *p == '\t' || *p == '\r')
				p++;

			if (strncmp(p, "<true/>", 7) == 0) {
				entries[n].is_bool = 1;
				entries[n].bool_val = 1;
				entries[n].val = NULL;
				p += 7;
				n++;
			} else if (strncmp(p, "<false/>", 8) == 0) {
				entries[n].is_bool = 1;
				entries[n].bool_val = 0;
				entries[n].val = NULL;
				p += 8;
				n++;
			} else if (strncmp(p, "<string>", 8) == 0) {
				p += 8;
				const char *vs = p;
				while (*p && strncmp(p, "</string>", 9) != 0)
					p++;
				if (!*p) { free(k); break; }
				entries[n].is_bool = 0;
				entries[n].val = malloc(p - vs + 1);
				memcpy(entries[n].val, vs, p - vs);
				entries[n].val[p - vs] = '\0';
				p += 9;
				n++;
			} else if (strncmp(p, "<integer>", 9) == 0) {
				p += 9;
				const char *vs = p;
				while (*p && strncmp(p, "</integer>", 10) != 0)
					p++;
				if (!*p) { free(k); break; }
				entries[n].is_bool = 0;
				entries[n].val = malloc(p - vs + 1);
				memcpy(entries[n].val, vs, p - vs);
				entries[n].val[p - vs] = '\0';
				p += 10;
				n++;
			} else {
				free(k);
				entries[n].key = NULL;
				while (*p && *p != '<') p++;
			}
		} else {
			p++;
		}
	}

	*count = n;
}

/*
 * Build a DER-encoded entitlements blob.
 * Format (matching Apple):
 *   SET[0] {
 *     INTEGER(1)           // version
 *     [0] {                 // each key-value pair
 *       SEQUENCE {
 *         UTF8String(key)
 *         BOOLEAN(value) | UTF8String(value)
 *       }
 *     }
 *   }
 */
size_t
build_entitlements_der(const char *plist_xml, uint8_t *out, size_t out_len)
{
	struct plist_entry entries[64];
	int count = 0;
	int i;

	parse_plist_entries(plist_xml, entries, 64, &count);

	size_t body_size = 3;  /* INTEGER(1) */
	for (i = 0; i < count; i++) {
		size_t key_len = strlen(entries[i].key);
		size_t key_der = 1 + der_len_size(key_len) + key_len;

		size_t val_der;
		if (entries[i].is_bool) {
			val_der = 3;
		} else {
			size_t vlen = strlen(entries[i].val);
			val_der = 1 + der_len_size(vlen) + vlen;
		}

		size_t seq_content = key_der + val_der;
		size_t seq_total = 1 + der_len_size(seq_content) + seq_content;
		size_t ctx_total = 1 + der_len_size(seq_total) + seq_total;
		body_size += ctx_total;
	}

	size_t wrapper_len = 1 + der_len_size(body_size) + body_size;
	size_t total = 8 + wrapper_len;

	if (out_len < total) {
		for (i = 0; i < count; i++) {
			if (entries[i].key) free(entries[i].key);
			if (entries[i].val) free(entries[i].val);
		}
		return 0;
	}

	be_write32(out, CSMAGIC_EMBEDDED_DER_ENTITLEMENTS);
	be_write32(out + 4, (uint32_t)total);

	size_t off = 8;
	out[off++] = 0x70;
	der_put_len(out, &off, body_size);

	out[off++] = 0x02;
	out[off++] = 0x01;
	out[off++] = 0x01;

	for (i = 0; i < count; i++) {
		size_t key_len = strlen(entries[i].key);
		size_t val_len;
		if (entries[i].is_bool)
			val_len = 0;
		else
			val_len = strlen(entries[i].val);

		/* Build key (UTF8String) */
		uint8_t key_buf[256 + 3];
		size_t key_off = 0;
		key_buf[key_off++] = 0x0C;
		der_put_len(key_buf, &key_off, key_len);
		memcpy(key_buf + key_off, entries[i].key, key_len);
		key_off += key_len;

		/* Build value (BOOLEAN or UTF8String) */
		uint8_t val_buf[256 + 3];
		size_t val_off = 0;
		if (entries[i].is_bool) {
			val_buf[val_off++] = 0x01;
			val_buf[val_off++] = 0x01;
			val_buf[val_off++] = entries[i].bool_val ? 0xFF : 0x00;
		} else {
			val_buf[val_off++] = 0x0C;
			der_put_len(val_buf, &val_off, val_len);
			memcpy(val_buf + val_off, entries[i].val, val_len);
			val_off += val_len;
		}

		size_t seq_content = key_off + val_off;
		size_t seq_total = 1 + der_len_size(seq_content) + seq_content;

		/* Write [0] IMPLICIT CONSTRUCTED wrapper */
		out[off++] = 0xb0;
		der_put_len(out, &off, seq_total);

		/* Write SEQUENCE */
		out[off++] = 0x30;
		der_put_len(out, &off, seq_content);

		/* Write key + value */
		memcpy(out + off, key_buf, key_off);
		off += key_off;
		memcpy(out + off, val_buf, val_off);
		off += val_off;
	}

	for (i = 0; i < count; i++) {
		if (entries[i].key) free(entries[i].key);
		if (entries[i].val) free(entries[i].val);
	}

 	return total;
 }

 /* ---- CMS signature ---- */

 static int
 load_identity(const char *cert_file, const char *key_file,
    const char *p12_file, const char *key_password,
    EVP_PKEY **out_pkey, X509 **out_cert, STACK_OF(X509) **out_certs)
{
	EVP_PKEY *pkey = NULL;
	X509 *cert = NULL;
	STACK_OF(X509) *cas = NULL;
	int ret = -1;

	if (p12_file && *p12_file) {
		FILE *fp = fopen(p12_file, "r");
		if (!fp) {
			fprintf(stderr, "codesign: cannot open identity file: %s\n", p12_file);
			return -1;
		}
		PKCS12 *p12 = d2i_PKCS12_fp(fp, NULL);
		fclose(fp);
		if (!p12) {
			fprintf(stderr, "codesign: failed to parse PKCS#12 file\n");
			return -1;
		}
		if (!PKCS12_parse(p12, key_password, &pkey, &cert, &cas)) {
			fprintf(stderr, "codesign: failed to extract identity from PKCS#12\n");
			PKCS12_free(p12);
			return -1;
		}
		PKCS12_free(p12);
		ret = 0;
	} else if (cert_file && key_file) {
		FILE *cfp = fopen(cert_file, "r");
		if (!cfp) {
			fprintf(stderr, "codesign: cannot open certificate: %s\n", cert_file);
			return -1;
		}
		cert = PEM_read_X509(cfp, NULL, NULL, NULL);
		if (!cert) {
			rewind(cfp);
			cert = d2i_X509_fp(cfp, NULL);
		}
		fclose(cfp);

		FILE *kfp = fopen(key_file, "r");
		if (!kfp) {
			fprintf(stderr, "codesign: cannot open key: %s\n", key_file);
			goto cleanup;
		}
		pkey = PEM_read_PrivateKey(kfp, NULL, NULL,
		    (void *)(key_password ? key_password : ""));
		if (!pkey) {
		rewind(kfp);
		pkey = PEM_read_PrivateKey(kfp, NULL, NULL,
		    (void *)(key_password ? key_password : ""));
		}
		fclose(kfp);

		if (!pkey || !cert) {
			fprintf(stderr, "codesign: failed to load cert/key pair\n");
			goto cleanup;
		}
		if (!X509_check_private_key(cert, pkey)) {
			fprintf(stderr, "codesign: certificate and key do not match\n");
			goto cleanup;
		}
		ret = 0;
	} else {
		fprintf(stderr, "codesign: must specify -s identity or use --cert/--key files\n");
		return -1;
	}

cleanup:
	if (ret == 0) {
		*out_pkey = pkey;
		*out_cert = cert;
		*out_certs = cas;
	} else {
		if (pkey) EVP_PKEY_free(pkey);
		if (cert) X509_free(cert);
		if (cas) sk_X509_pop_free(cas, X509_free);
	}
	return ret;
}

/*
 * Pull the display name and team identifier out of a signing
 * certificate.  Apple carries the team identifier in the subject's
 * Organizational Unit, and codesign(1) reports "notset" when the
 * certificate has none.
 */
void
cert_copy_names(X509 *cert, char *team_id_out, size_t team_id_len,
    char *cert_cn_out, size_t cert_cn_len)
{
	static const struct {
		int nid;
		int is_team;
	} fields[] = {
		{ NID_commonName, 0 },
		{ NID_organizationalUnitName, 1 },
	};

	if (cert_cn_out != NULL && cert_cn_len > 0)
		cert_cn_out[0] = '\0';
	if (team_id_out != NULL && team_id_len > 0)
		team_id_out[0] = '\0';

	for (size_t i = 0; cert != NULL && i < sizeof(fields) / sizeof(fields[0]); i++) {
		char *dst = fields[i].is_team ? team_id_out : cert_cn_out;
		size_t dstlen = fields[i].is_team ? team_id_len : cert_cn_len;
		X509_NAME *name;
		X509_NAME_ENTRY *entry;
		ASN1_STRING *str;
		int idx, slen;

		if (dst == NULL || dstlen == 0)
			continue;

		name = X509_get_subject_name(cert);
		idx = X509_NAME_get_index_by_NID(name, fields[i].nid, -1);
		if (idx < 0)
			continue;
		if ((entry = X509_NAME_get_entry(name, idx)) == NULL)
			continue;
		if ((str = X509_NAME_ENTRY_get_data(entry)) == NULL)
			continue;

		slen = ASN1_STRING_length(str);
		if (slen > 0 && (size_t)slen < dstlen) {
			memcpy(dst, ASN1_STRING_get0_data(str), (size_t)slen);
			dst[slen] = '\0';
		}
	}

	if (team_id_out != NULL && team_id_len > 0 && team_id_out[0] == '\0')
		snprintf(team_id_out, team_id_len, "notset");
}

int
load_identity_info(const char *keychain_id,
    const char *cert_file, const char *key_file,
    const char *p12_file, const char *key_password,
    char *team_id_out, size_t team_id_len,
    char *cert_cn_out, size_t cert_cn_len)
{
	EVP_PKEY *pkey = NULL;
	X509 *cert = NULL;
	STACK_OF(X509) *cas = NULL;
	int ret = -1;

	/* A keychain identity is described by Security, not OpenSSL. */
	if (keychain_id != NULL)
		return keychain_identity_info(keychain_id,
		    team_id_out, team_id_len, cert_cn_out, cert_cn_len);

	if (load_identity(cert_file, key_file, p12_file, key_password,
	    &pkey, &cert, &cas) != 0)
		return -1;

	cert_copy_names(cert, team_id_out, team_id_len,
	    cert_cn_out, cert_cn_len);

	ret = 0;
	if (pkey) EVP_PKEY_free(pkey);
	if (cert) X509_free(cert);
	if (cas) sk_X509_pop_free(cas, X509_free);
	return ret;
}

int
build_cms_signature(const uint8_t *cd_blob,
    size_t cd_blob_len,
    const char *cdhashes_plist, const char *cd_sha256_hex,
    const char *keychain_id,
    const char *cert_file, const char *key_file,
    const char *p12_file, const char *key_password,
    uint8_t *out, size_t *out_len, char *team_id_out, size_t team_id_len,
    char *cert_cn_out, size_t cert_cn_len)
{
	EVP_PKEY *pkey = NULL;
	X509 *cert = NULL;
	STACK_OF(X509) *other_certs = NULL;
	BIO *bio_in = NULL, *bio_out = NULL;
	CMS_ContentInfo *cms = NULL;
	CMS_SignerInfo *si = NULL;
	ASN1_OBJECT *obj1 = NULL, *obj2 = NULL;
	int ret = -1;

	/*
	 * An identity from the keychain is signed with by Security,
	 * which can use a private key that cannot be copied out of it.
	 */
	if (keychain_id != NULL)
		return keychain_cms_sign(keychain_id, cd_blob, cd_blob_len,
		    cdhashes_plist, out, out_len, team_id_out, team_id_len,
		    cert_cn_out, cert_cn_len);

	if (load_identity(cert_file, key_file, p12_file, key_password,
	    &pkey, &cert, &other_certs) != 0)
		return -1;

	cert_copy_names(cert, team_id_out, team_id_len,
	    cert_cn_out, cert_cn_len);

	bio_in = BIO_new_mem_buf(cd_blob, (int)cd_blob_len);
	if (!bio_in) goto cleanup;

	int flags = CMS_PARTIAL | CMS_DETACHED | CMS_NOSMIMECAP | CMS_BINARY;
	cms = CMS_sign(NULL, NULL, other_certs, NULL, flags);
	if (!cms) goto cleanup;

	si = CMS_add1_signer(cms, cert, pkey, EVP_sha256(), flags);
	if (!si) goto cleanup;

	obj1 = OBJ_txt2obj(OID_CDHASHES, 1);
	if (!obj1) goto cleanup;
	if (!CMS_signed_add1_attr_by_OBJ(si, obj1, V_ASN1_UTF8STRING,
	    (const unsigned char *)cdhashes_plist, (int)strlen(cdhashes_plist)))
		goto cleanup;

	obj2 = OBJ_txt2obj(OID_CDHASHES2, 1);
	if (!obj2) goto cleanup;
	if (!CMS_signed_add1_attr_by_OBJ(si, obj2, V_ASN1_OCTET_STRING,
	    (const unsigned char *)cd_sha256_hex, (int)strlen(cd_sha256_hex)))
		goto cleanup;

	if (!CMS_final(cms, bio_in, NULL, flags)) goto cleanup;

	bio_out = BIO_new(BIO_s_mem());
	if (!bio_out) goto cleanup;
	if (!i2d_CMS_bio(bio_out, cms)) goto cleanup;

	BUF_MEM *bptr;
	BIO_get_mem_ptr(bio_out, &bptr);
	if (!bptr || !bptr->data || bptr->length <= 0) goto cleanup;

	/*
	 * Wrap the CMS in a blob header.  Every member of the embedded
	 * signature carries its own magic and length -- the loader reads
	 * the header to find the payload -- so handing back bare DER
	 * produces a signature macOS cannot parse at all, reported as
	 * "code object is not signed at all" rather than as a bad
	 * signature.  build_adhoc_wrapper() writes the same header for the
	 * empty ad-hoc case, which is why only certificate signing was
	 * affected.
	 */
	if (*out_len < (size_t)bptr->length + 8) goto cleanup;
	be_write32(out, CSMAGIC_BLOBWRAPPER);
	be_write32(out + 4, (uint32_t)(bptr->length + 8));
	memcpy(out + 8, bptr->data, bptr->length);
	*out_len = (size_t)bptr->length + 8;
	ret = 0;

cleanup:
	if (bio_in) BIO_free(bio_in);
	if (bio_out) BIO_free(bio_out);
	if (cms) CMS_ContentInfo_free(cms);
	if (obj1) ASN1_OBJECT_free(obj1);
	if (obj2) ASN1_OBJECT_free(obj2);
	if (pkey) EVP_PKEY_free(pkey);
	if (cert) X509_free(cert);
	if (other_certs) sk_X509_pop_free(other_certs, X509_free);
	return ret;
}

int
build_adhoc_wrapper(uint8_t *out, size_t *out_len)
{
	if (*out_len < 8)
		return -1;
	be_write32(out, CSMAGIC_BLOBWRAPPER);
	be_write32(out + 4, 8);
	*out_len = 8;
	return 0;
}
