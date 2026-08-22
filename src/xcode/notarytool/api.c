/*
 * api - HTTP client and Apple Notary Service v2 API implementation.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Communicates with https://appstoreconnect.apple.com/notary/v2/
 * using libcurl. JWT authentication is generated via jwt.c (ES256).
 * JSON response parsing is handled by json.c.
 *
 * Submit flow:
 *   1. POST /submissions with file metadata -> submissionId + upload URL
 *   2. PUT file contents to the upload URL (S3 presigned)
 *   3. Poll GET /submissions/{id} for status until terminal
 *
 * Apple ID + app-specific password auth is supported via a simpler
 * token exchange (not JWT), delegated through Apple's auth service.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <limits.h>
#include <sys/stat.h>
#include <openssl/evp.h>
#include <curl/curl.h>

#include "api.h"
#include "jwt.h"
#include "json.h"

/* --- growable response buffer --- */

struct resp_buf {
	char *data;
	size_t len;
	size_t cap;
};

static size_t
resp_write_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	struct resp_buf *rb = (struct resp_buf *)userdata;
	size_t n = size * nmemb;
	if (rb->len + n + 1 > rb->cap) {
		size_t cap = rb->cap ? rb->cap : 16384;
		while (cap < rb->len + n + 1)
			cap *= 2;
		char *nd = realloc(rb->data, cap);
		if (nd == NULL)
			return 0;
		rb->data = nd;
		rb->cap = cap;
	}
	memcpy(rb->data + rb->len, ptr, n);
	rb->len += n;
	rb->data[rb->len] = '\0';
	return n;
}

static void
resp_init(struct resp_buf *rb)
{
	rb->data = NULL;
	rb->len = 0;
	rb->cap = 0;
}

static void
resp_free(struct resp_buf *rb)
{
	free(rb->data);
	rb->data = NULL;
	rb->len = 0;
	rb->cap = 0;
}

/* --- base64 encoding (standard, for Apple ID auth) --- */

static char *
base64_encode(const unsigned char *data, size_t len, size_t *out_len)
{
	static const char b64[] =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
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
		out[o++] = b64[(val >> 18) & 0x3f];
		out[o++] = b64[(val >> 12) & 0x3f];
		out[o++] = (nbytes > 1) ? b64[(val >> 6) & 0x3f] : '=';
		out[o++] = (nbytes > 2) ? b64[val & 0x3f] : '=';
	}
	out[o] = '\0';
	*out_len = o;
	return out;
}

/* --- file checksums --- */

static char *
file_md5_base64(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (f == NULL)
		return NULL;

	EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
	if (mdctx == NULL) {
		fclose(f);
		return NULL;
	}

	if (EVP_DigestInit_ex(mdctx, EVP_md5(), NULL) != 1) {
		EVP_MD_CTX_free(mdctx);
		fclose(f);
		return NULL;
	}

	unsigned char buf[65536];
	size_t n;
	while ((n = fread(buf, 1, sizeof buf, f)) > 0)
		EVP_DigestUpdate(mdctx, buf, n);
	fclose(f);

	unsigned char digest[EVP_MAX_MD_SIZE];
	unsigned int digest_len = 0;
	if (EVP_DigestFinal_ex(mdctx, digest, &digest_len) != 1) {
		EVP_MD_CTX_free(mdctx);
		return NULL;
	}
	EVP_MD_CTX_free(mdctx);

	size_t b64_len = 0;
	char *out = (char *)base64_encode(digest, digest_len, &b64_len);
	return out;
}

static char *
file_sha256_hex(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (f == NULL)
		return NULL;

	EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
	if (mdctx == NULL) {
		fclose(f);
		return NULL;
	}

	if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1) {
		EVP_MD_CTX_free(mdctx);
		fclose(f);
		return NULL;
	}

	unsigned char buf[65536];
	size_t n;
	while ((n = fread(buf, 1, sizeof buf, f)) > 0)
		EVP_DigestUpdate(mdctx, buf, n);
	fclose(f);

	unsigned char digest[EVP_MAX_MD_SIZE];
	unsigned int digest_len = 0;
	if (EVP_DigestFinal_ex(mdctx, digest, &digest_len) != 1) {
		EVP_MD_CTX_free(mdctx);
		return NULL;
	}
	EVP_MD_CTX_free(mdctx);

	char *out = malloc(digest_len * 2 + 1);
	if (out == NULL)
		return NULL;
	static const char hex[] = "0123456789abcdef";
	for (unsigned int i = 0; i < digest_len; i++) {
		out[i * 2] = hex[digest[i] >> 4];
		out[i * 2 + 1] = hex[digest[i] & 0xf];
	}
	out[digest_len * 2] = '\0';
	return out;
}

static size_t
file_size(const char *path)
{
	struct stat st;
	if (stat(path, &st) != 0)
		return 0;
	return (size_t)st.st_size;
}

/* --- credential management --- */

void
creds_free(struct creds *c)
{
	if (c == NULL)
		return;
	free(c->key_path);
	free(c->key_id);
	free(c->issuer_id);
	free(c->team_id);
	free(c->apple_id);
	free(c->app_specific_password);
	free(c->profile_name);
	free(c->keychain_path);
	free(c);
}

static struct curl_slist *
build_auth_headers(const char *jwt)
{
	struct curl_slist *hdrs = NULL;
	hdrs = curl_slist_append(hdrs, "Accept: application/json");
	hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
	char auth[1024];
	snprintf(auth, sizeof auth, "Authorization: Bearer %s", jwt);
	hdrs = curl_slist_append(hdrs, auth);
	return hdrs;
}

/*
 * Authenticate and return a JWT or token string.
 * For API key auth, generates an ES256 JWT.
 * For Apple ID auth, base64-encodes the credentials.
 */
char *
api_authenticate(struct creds *c)
{
	if (c->type == CREDS_KEY)
		return jwt_build(c->key_path, c->key_id, c->issuer_id, NULL);

	if (c->type == CREDS_ASP && c->apple_id &&
	    c->app_specific_password) {
		size_t ul = strlen(c->apple_id) + 2 +
		    strlen(c->app_specific_password) + 1;
		char *combo = malloc(ul);
		if (combo == NULL)
			return NULL;
		snprintf(combo, ul, "%s:%s", c->apple_id,
		    c->app_specific_password);

		size_t b64_len = 0;
		char *b64 = (char *)base64_encode(
		    (const unsigned char *)combo, strlen(combo), &b64_len);
		free(combo);
		return b64;
	}

	return NULL;
}

/* --- HTTP helper --- */

static char *
http_request(struct creds *c, const char *url, const char *method,
    const char *body, long *out_status, int verbose)
{
	CURL *curl = curl_easy_init();
	if (curl == NULL)
		return NULL;

	char *jwt = api_authenticate(c);
	if (jwt == NULL) {
		curl_easy_cleanup(curl);
		return NULL;
	}

	struct curl_slist *hdrs = build_auth_headers(jwt);
	free(jwt);

	struct resp_buf rb;
	resp_init(&rb);

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, resp_write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &rb);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	if (strcmp(method, "POST") == 0 || strcmp(method, "PUT") == 0) {
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
		if (body != NULL) {
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
			    (long)strlen(body));
		}
	} else {
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
	}

	if (verbose)
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

	CURLcode rc = curl_easy_perform(curl);
	long status = 0;
	if (rc == CURLE_OK)
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

	curl_slist_free_all(hdrs);
	curl_easy_cleanup(curl);

	if (out_status)
		*out_status = status;

	if (rc != CURLE_OK || rb.data == NULL) {
		resp_free(&rb);
		return NULL;
	}
	return rb.data;
}

/* --- file upload to S3 presigned URL --- */

static long
http_upload_file(struct creds *c, const char *upload_url,
    const char *file_path, const char *content_type, int verbose)
{
	CURL *curl = curl_easy_init();
	if (curl == NULL)
		return -1;

	struct resp_buf rb;
	resp_init(&rb);

	FILE *f = fopen(file_path, "rb");
	if (f == NULL) {
		curl_easy_cleanup(curl);
		return -1;
	}

	curl_easy_setopt(curl, CURLOPT_URL, upload_url);
	curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(curl, CURLOPT_READDATA, f);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, resp_write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &rb);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

	struct curl_slist *hdrs = NULL;
	hdrs = curl_slist_append(hdrs, "Expect:");
	hdrs = curl_slist_append(hdrs, "Accept:");

	if (verbose) {
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
		fprintf(stderr, "notarytool: uploading %s to S3\n", file_path);
	}

	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);

	CURLcode rc = curl_easy_perform(curl);
	long status = 0;
	if (rc == CURLE_OK)
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

	curl_slist_free_all(hdrs);
	curl_easy_cleanup(curl);
	fclose(f);
	resp_free(&rb);

	if (rc != CURLE_OK)
		return -1;
	return status;
}

/* --- API endpoints --- */

static char *
url_join(const char *base, const char *path)
{
	char *url = malloc(strlen(base) + strlen(path) + 1);
	if (url == NULL)
		return NULL;
	sprintf(url, "%s%s", base, path);
	return url;
}

struct submission_status *
api_submit(struct creds *c, const char *file_path,
    const char *content_type, int use_s3_ta, int force)
{
	const char *basename = strrchr(file_path, '/');
	basename = basename ? basename + 1 : file_path;

	size_t fsize = file_size(file_path);
	char *md5_b64 = file_md5_base64(file_path);
	char *sha_hex = file_sha256_hex(file_path);
	if (md5_b64 == NULL || sha_hex == NULL) {
		free(md5_b64);
		free(sha_hex);
		return NULL;
	}

	/* Build JSON request body in JSON:API format */
	char body[4096];
	snprintf(body, sizeof body,
	    "{\"data\":{\"type\":\"submissions\",\"attributes\":{"
	    "\"archived\":false,"
	    "\"contentType\":\"%s\","
	    "\"fileSize\":%zu,"
	    "\"md5\":\"%s\","
	    "\"sha256\":\"%s\","
	    "\"submissionName\":\"%s\","
	    "\"teamId\":\"%s\""
	    "}}}",
	    content_type ? content_type : "application/octet-stream",
	    fsize,
	    md5_b64,
	    sha_hex,
	    basename,
	    c->team_id ? c->team_id : "");

	free(md5_b64);
	free(sha_hex);

	char *url = url_join(NOTARY_API_BASE, "submissions");
	if (url == NULL)
		return NULL;

	long status = 0;
	char *resp = http_request(c, url, "POST", body, &status, 0);
	free(url);

	if (resp == NULL || status != 201) {
		if (resp) {
			fprintf(stderr, "notarytool: submit failed: %s\n", resp);
			free(resp);
		}
		if (status == 0)
			fprintf(stderr, "notarytool: submit request failed\n");
		else
			fprintf(stderr, "notarytool: submit returned HTTP %ld\n",
			    status);
		return NULL;
	}

	/* Parse response */
	struct submission_status *s = malloc(sizeof(*s));
	if (s == NULL) {
		free(resp);
		return NULL;
	}
	memset(s, 0, sizeof(*s));

	s->id = json_get_nested_string(resp, "data", "id");
	if (s->id == NULL)
		s->id = json_extract_value(resp, "id");

	s->status = json_get_nested_string(resp, "data", "status");
	if (s->status == NULL)
		s->status = json_extract_value(resp, "status");

	s->upload_url = json_get_nested_string(resp, "data", "url");
	if (s->upload_url == NULL)
		s->upload_url = json_extract_value(resp, "url");

	/* Also check attributes for status and url */
	if (s->status == NULL) {
		free(s->status);
		s->status = json_get_nested_string(resp, "attributes", "status");
	}
	if (s->upload_url == NULL) {
		free(s->upload_url);
		s->upload_url = json_get_nested_string(resp, "attributes", "url");
	}

	s->log_url = json_get_nested_string(resp, "attributes",
	    "developerLogUrl");
	if (s->log_url == NULL)
		s->log_url = json_extract_value(resp, "developerLogUrl");

	s->submission_name = json_get_nested_string(resp, "attributes",
	    "submissionName");

	s->done = (s->status != NULL &&
	    (strcmp(s->status, NOTARY_STATUS_ACCEPTED) == 0 ||
	     strcmp(s->status, NOTARY_STATUS_INVALID) == 0 ||
	     strcmp(s->status, NOTARY_STATUS_REJECTED) == 0));

 	free(resp);

 	if (s->id == NULL) {
 		submission_status_free(s);
 		return NULL;
 	}

 	/* Upload the file to the presigned S3 URL if provided */
 	if (s->upload_url != NULL) {
 		long upload_status = http_upload_file(c, s->upload_url,
 		    file_path, content_type, 0);
 		if (upload_status < 200 || upload_status >= 300)
 			fprintf(stderr,
 			    "notarytool: file upload failed (HTTP %ld)\n",
 			    upload_status);
 	}

 	return s;
 }

struct submission_status *
api_get_status(struct creds *c, const char *submission_id)
{
	char url[512];
	snprintf(url, sizeof url, "%ssubmissions/%s",
	    NOTARY_API_BASE, submission_id);

	long status = 0;
	char *resp = http_request(c, url, "GET", NULL, &status, 0);
	if (resp == NULL || status != 200) {
		if (resp)
			fprintf(stderr, "notarytool: status check failed: %s\n",
			    resp);
		free(resp);
		return NULL;
	}

	struct submission_status *s = malloc(sizeof(*s));
	if (s == NULL) {
		free(resp);
		return NULL;
	}
	memset(s, 0, sizeof(*s));

	s->id = json_get_nested_string(resp, "data", "id");
	if (s->id == NULL)
		s->id = strdup(submission_id);

	s->status = json_get_nested_string(resp, "data", "status");
	if (s->status == NULL) {
		s->status = json_get_nested_string(resp, "attributes",
		    "status");
	}
	if (s->status == NULL)
		s->status = json_extract_value(resp, "status");

	s->upload_url = json_get_nested_string(resp, "data", "url");
	if (s->upload_url == NULL)
		s->upload_url = json_get_nested_string(resp, "attributes",
		    "url");

	s->log_url = json_get_nested_string(resp, "attributes",
	    "developerLogUrl");
	if (s->log_url == NULL)
		s->log_url = json_extract_value(resp, "developerLogUrl");

	s->submission_name = json_get_nested_string(resp, "attributes",
	    "submissionName");

	s->done = (s->status != NULL &&
	    (strcmp(s->status, NOTARY_STATUS_ACCEPTED) == 0 ||
	     strcmp(s->status, NOTARY_STATUS_INVALID) == 0 ||
	     strcmp(s->status, NOTARY_STATUS_REJECTED) == 0));

	free(resp);
	return s;
}

char *
api_get_log(struct creds *c, const char *submission_id)
{
	char url[512];
	snprintf(url, sizeof url, "%ssubmissions/%s/log",
	    NOTARY_API_BASE, submission_id);

	long status = 0;
	char *resp = http_request(c, url, "GET", NULL, &status, 0);
	if (resp == NULL || status != 200) {
		if (resp)
			fprintf(stderr, "notarytool: log request failed: %s\n",
			    resp);
		free(resp);
		return NULL;
	}

	/* Response contains a developerLogUrl to fetch the actual log */
	char *log_url = NULL;

	/* Try nested extraction */
	log_url = json_get_nested_string(resp, "attributes",
	    "developerLogUrl");
	if (log_url == NULL)
		log_url = json_extract_value(resp, "developerLogUrl");

	free(resp);

	if (log_url == NULL)
		return NULL;

	/* Fetch the log JSON from the log URL */
	CURL *curl = curl_easy_init();
	if (curl == NULL) {
		free(log_url);
		return NULL;
	}

	struct resp_buf rb;
	resp_init(&rb);

	curl_easy_setopt(curl, CURLOPT_URL, log_url);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, resp_write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &rb);

	CURLcode rc = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	free(log_url);

	if (rc != CURLE_OK) {
		resp_free(&rb);
		return NULL;
	}
	return rb.data;
}

char *
api_get_history(struct creds *c)
{
	char *url = url_join(NOTARY_API_BASE, "submissions");
	if (url == NULL)
		return NULL;

	long status = 0;
	char *resp = http_request(c, url, "GET", NULL, &status, 0);
	free(url);

	if (resp == NULL || status != 200) {
		free(resp);
		return NULL;
	}
	return resp;
}

int
api_wait(struct creds *c, const char *submission_id,
    long timeout_seconds, int verbose)
{
	long start_time = (long)time(NULL);
	long elapsed = 0;

	while (elapsed < timeout_seconds) {
		struct submission_status *s = api_get_status(c, submission_id);
		if (s == NULL) {
			fprintf(stderr,
			    "notarytool: failed to get submission status\n");
			return -1;
		}

		if (s->status) {
			if (verbose)
				fprintf(stderr, "notarytool: submission %s: %s\n",
				    s->id, s->status);
			else
				fprintf(stderr, "notarytool: %s\n", s->status);
		}

		if (s->done) {
			int accepted = (s->status &&
			    strcmp(s->status, NOTARY_STATUS_ACCEPTED) == 0);
			submission_status_free(s);
			return accepted ? 0 : 1;
		}

		submission_status_free(s);

		long remaining = timeout_seconds - elapsed;
		long sleep_time = NOTARY_POLL_INTERVAL;
		if (sleep_time > remaining)
			sleep_time = remaining;
		if (sleep_time <= 0)
			break;

		if (verbose)
			fprintf(stderr, "notarytool: polling again in %lds\n",
			    sleep_time);
		sleep((unsigned int)sleep_time);
		elapsed = (long)time(NULL) - start_time;
	}

	fprintf(stderr, "notarytool: timed out waiting for submission\n");
	return -1;
}

void
submission_status_free(struct submission_status *s)
{
	if (s == NULL)
		return;
	free(s->id);
	free(s->status);
	free(s->submission_name);
	free(s->upload_url);
	free(s->log_url);
	free(s->sha256);
	free(s);
}
