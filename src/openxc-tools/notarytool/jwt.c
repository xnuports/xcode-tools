/*
 * jwt - ES256 JWT generation for App Store Connect API auth.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Builds a JWT signed with ES256 (ECDSA P-256 + SHA-256). The private
 * key is expected in PKCS#8 PEM format (as downloaded from App Store
 * Connect). The JWT signature is in the raw R||S format required by
 * RFC 7518, not the DER format produced by OpenSSL's ECDSA_sign directly.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include "jwt.h"

/* --- base64url encoding (no padding) --- */

static const char b64url_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static char *
base64url_encode(const unsigned char *data, size_t len, size_t *out_len)
{
	size_t cap = ((len + 2) / 3) * 4 + 1;
	char *out = malloc(cap);
	if (out == NULL)
		return NULL;
	size_t o = 0;
	for (size_t i = 0; i < len; i += 3) {
		unsigned int val = data[i] << 16;
		int nbytes = 1;
		if (i + 1 < len) {
			val |= data[i + 1] << 8;
			nbytes = 2;
			if (i + 2 < len) {
				val |= data[i + 2];
				nbytes = 3;
			}
		}
		out[o++] = b64url_chars[(val >> 18) & 0x3f];
		out[o++] = b64url_chars[(val >> 12) & 0x3f];
		if (nbytes > 1)
			out[o++] = b64url_chars[(val >> 6) & 0x3f];
		else
			out[o++] = '=';
		if (nbytes > 2)
			out[o++] = b64url_chars[val & 0x3f];
		else
			out[o++] = '=';
	}
	out[o] = '\0';
	*out_len = o;
	return out;
}

/*
 * Remove padding from base64url output.
 */
static void
b64url_strip_pad(char *s, size_t *len)
{
	size_t l = strlen(s);
	while (l > 0 && s[l - 1] == '=') {
		s[--l] = '\0';
	}
	*len = l;
}

/*
 * Convert ASN.1 DER-encoded ECDSA signature to raw R||S format.
 * ES256 expects two 32-byte integers (r and s) concatenated,
 * but OpenSSL produces DER encoding. This parses the DER and
 * extracts the raw r and s values.
 */
static int
der_to_raw_ecdsa(const unsigned char *der, size_t der_len,
    unsigned char *raw, size_t raw_len)
{
	if (raw_len < 64 || der_len < 8)
		return -1;

	/*
	 * Parse ASN.1 DER: SEQUENCE { INTEGER r, INTEGER s }
	 * ECDSA-Sig-Value ::= SEQUENCE { r INTEGER, s INTEGER }
	 */
	if (der[0] != 0x30)
		return -1;
	size_t seq_len = der[1];
	if (seq_len & 0x80)
		return -1;
	size_t pos = 2;
	if (pos + 2 > der_len)
		return -1;

	/* First INTEGER (r) */
	if (der[pos] != 0x02)
		return -1;
	size_t r_len = der[pos + 1];
	pos += 2;
	if (pos + r_len > der_len)
		return -1;

	/* Skip leading zero byte (if present, as r is positive) */
	if (r_len > 0 && der[pos] == 0x00) {
		pos++;
		r_len--;
	}
	if (r_len > 32)
		return -1;
	/* Right-justify r into 32-byte buffer */
	memset(raw, 0, 32);
	memcpy(raw + 32 - r_len, der + pos, r_len);
	pos += r_len;

	/* Second INTEGER (s) */
	if (pos + 2 > der_len)
		return -1;
	if (der[pos] != 0x02)
		return -1;
	size_t s_len = der[pos + 1];
	pos += 2;
	if (pos + s_len > der_len)
		return -1;
	if (s_len > 0 && der[pos] == 0x00) {
		pos++;
		s_len--;
	}
	if (s_len > 32)
		return -1;
	memset(raw + 32, 0, 32);
	memcpy(raw + 32 + 32 - s_len, der + pos, s_len);

	return 0;
}

char *
jwt_build(const char *key_path, const char *key_id,
    const char *issuer_id, size_t *out_len)
{
	FILE *f = fopen(key_path, "r");
	if (f == NULL)
		return NULL;

	EVP_PKEY *pkey = PEM_read_PrivateKey(f, NULL, NULL, NULL);
	fclose(f);
	if (pkey == NULL)
		return NULL;

	time_t now = time(NULL);
	char iat_str[32], exp_str[32];
	snprintf(iat_str, sizeof iat_str, "%ld", (long)now);
	snprintf(exp_str, sizeof exp_str, "%ld", (long)(now + 1200));

	/* Build header */
	char header_json[512];
	snprintf(header_json, sizeof header_json,
	    "{\"alg\":\"ES256\",\"kid\":\"%s\",\"typ\":\"JWT\"}",
	    key_id ? key_id : "");

	/* Build payload */
	char payload_json[1024];
	snprintf(payload_json, sizeof payload_json,
	    "{\"iss\":\"%s\",\"iat\":%s,\"exp\":%s,\"aud\":\"appstoreconnect-v1\"}",
	    issuer_id ? issuer_id : "", iat_str, exp_str);

	/* Base64url encode header and payload */
	size_t h_b64_len = 0;
	char *h_b64 = base64url_encode((const unsigned char *)header_json,
	    strlen(header_json), &h_b64_len);
	size_t h_strip = 0;
	b64url_strip_pad(h_b64, &h_strip);

	size_t p_b64_len = 0;
	char *p_b64 = base64url_encode((const unsigned char *)payload_json,
	    strlen(payload_json), &p_b64_len);
	size_t p_strip = 0;
	b64url_strip_pad(p_b64, &p_strip);

	/* Sign "header.payload" */
	char signing_input[2048];
	snprintf(signing_input, sizeof signing_input, "%s.%s", h_b64, p_b64);
	size_t si_len = strlen(signing_input);

	/* Compute SHA-256 of signing input, then sign with ES256 */
	unsigned char hash[32];
	EVP_Digest(signing_input, si_len, hash, NULL, EVP_sha256(), NULL);

	EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
	const EVP_MD *md = EVP_sha256();
	if (EVP_DigestSignInit(mdctx, NULL, md, NULL, pkey) != 1) {
		free(h_b64);
		free(p_b64);
		EVP_PKEY_free(pkey);
		EVP_MD_CTX_free(mdctx);
		return NULL;
	}

	size_t sig_len = 0;
	if (EVP_DigestSign(mdctx, NULL, &sig_len, hash, sizeof hash) != 1) {
		free(h_b64);
		free(p_b64);
		EVP_PKEY_free(pkey);
		EVP_MD_CTX_free(mdctx);
		return NULL;
	}

	unsigned char *der_sig = malloc(sig_len);
	if (der_sig == NULL) {
		free(h_b64);
		free(p_b64);
		EVP_PKEY_free(pkey);
		EVP_MD_CTX_free(mdctx);
		return NULL;
	}

	if (EVP_DigestSign(mdctx, der_sig, &sig_len, hash, sizeof hash) != 1) {
		free(der_sig);
		free(h_b64);
		free(p_b64);
		EVP_PKEY_free(pkey);
		EVP_MD_CTX_free(mdctx);
		return NULL;
	}

	EVP_MD_CTX_free(mdctx);
	EVP_PKEY_free(pkey);

	/* Convert DER signature to raw R||S (64 bytes) */
	unsigned char raw_sig[64];
	if (der_to_raw_ecdsa(der_sig, sig_len, raw_sig, sizeof raw_sig) != 0) {
		free(der_sig);
		free(h_b64);
		free(p_b64);
		return NULL;
	}
	free(der_sig);

	/* Base64url encode the raw signature */
	size_t s_b64_len = 0;
	char *s_b64 = base64url_encode(raw_sig, sizeof raw_sig, &s_b64_len);
	size_t s_strip = 0;
	b64url_strip_pad(s_b64, &s_strip);

	/* Assemble JWT */
	size_t total_len = h_strip + 1 + p_strip + 1 + s_strip + 1;
	char *jwt = malloc(total_len);
	if (jwt == NULL) {
		free(h_b64);
		free(p_b64);
		free(s_b64);
		return NULL;
	}
	snprintf(jwt, total_len, "%s.%s.%s", h_b64, p_b64, s_b64);
	*out_len = strlen(jwt);

	free(h_b64);
	free(p_b64);
	free(s_b64);
	return jwt;
}
