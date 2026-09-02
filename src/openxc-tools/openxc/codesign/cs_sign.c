/*
 * cs_sign.c - Code signing operations.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "codesign.h"
#include <CoreFoundation/CoreFoundation.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <dirent.h>
#include <openssl/evp.h>

int g_verbose = 0;
int g_continue_on_error = 0;

/* ---- Bundle helpers ---- */

static char *
find_bundle_info_value(const char *bundle_path, const char *key)
{
	char info_path[4096];
	snprintf(info_path, sizeof(info_path), "%s/Contents/Info.plist", bundle_path);

	size_t sz;
	uint8_t *data = cs_read_file(info_path, &sz);
	if (!data)
		return NULL;

	char search[256];
	snprintf(search, sizeof(search), "<key>%s</key>", key);
	const char *needle = strstr((const char *)data, search);
	if (!needle) {
		free(data);
		return NULL;
	}

	needle += strlen(search);
	const char *vstart = strstr(needle, "<string>");
	if (!vstart) {
		free(data);
		return NULL;
	}
	vstart += 8;
	const char *vend = strstr(vstart, "</string>");
	if (!vend) {
		free(data);
		return NULL;
	}

	size_t vlen = vend - vstart;
	char *val = malloc(vlen + 1);
	memcpy(val, vstart, vlen);
	val[vlen] = '\0';
	free(data);
	return val;
}

char *
find_bundle_executable(const char *bundle_path)
{
	return find_bundle_info_value(bundle_path, "CFBundleExecutable");
}

char *
find_bundle_identifier(const char *bundle_path)
{
	return find_bundle_info_value(bundle_path, "CFBundleIdentifier");
}

/* ---- Core signing ---- */

/* ------------------------------------------------------------------ */
/* the bundle seal                                                      */
/* ------------------------------------------------------------------ */

/*
 * A bundle is signed by signing the executable inside it, but the
 * signature has to cover the rest of the bundle too or the bundle
 * could be filled with anything afterwards.  That cover is
 * Contents/_CodeSignature/CodeResources: every resource with its
 * hash, sealed in turn by slot -3 of the code directory.
 *
 * The seal is a property of the bundle rather than of any one
 * architecture inside it, so it is worked out once and every slice
 * signs the same one.
 */
static uint8_t g_resource_seal[CS_SHA256_LEN];
static int g_have_resource_seal;

static void
cf_set_data(CFMutableDictionaryRef d, const char *key, const uint8_t *bytes,
    size_t len)
{
	CFStringRef k = CFStringCreateWithCString(NULL, key,
	    kCFStringEncodingUTF8);
	CFDataRef v = CFDataCreate(NULL, bytes, (CFIndex)len);

	if (k != NULL && v != NULL)
		CFDictionarySetValue(d, k, v);
	if (k != NULL)
		CFRelease(k);
	if (v != NULL)
		CFRelease(v);
}

/* Every file under a bundle's Resources, with the path it is sealed by. */
static void
seal_directory(const char *root, const char *rel, CFMutableDictionaryRef files,
    CFMutableDictionaryRef files2)
{
	char path[4096];
	struct dirent *e;
	DIR *d;

	snprintf(path, sizeof(path), "%s/%s", root, rel);
	if ((d = opendir(path)) == NULL)
		return;

	while ((e = readdir(d)) != NULL) {
		char sub[4096], full[4096];
		uint8_t sha1[CS_SHA1_LEN], sha256[CS_SHA256_LEN];
		struct stat st;
		uint8_t *buf;
		long len;
		FILE *fp;

		if (e->d_name[0] == '.')
			continue;

		snprintf(sub, sizeof(sub), "%s/%s", rel, e->d_name);
		snprintf(full, sizeof(full), "%s/%s", root, sub);

		if (lstat(full, &st) != 0)
			continue;

		if (S_ISDIR(st.st_mode)) {
			seal_directory(root, sub, files, files2);
			continue;
		}

		if ((fp = fopen(full, "rb")) == NULL)
			continue;
		if (fseek(fp, 0, SEEK_END) != 0 || (len = ftell(fp)) < 0) {
			fclose(fp);
			continue;
		}
		rewind(fp);
		if ((buf = malloc((size_t)len + 1)) == NULL) {
			fclose(fp);
			continue;
		}
		if (len > 0 && fread(buf, 1, (size_t)len, fp) != (size_t)len) {
			free(buf);
			fclose(fp);
			continue;
		}
		fclose(fp);

		sha1_raw(buf, (size_t)len, sha1);
		sha256_raw(buf, (size_t)len, sha256);
		free(buf);

		/*
		 * A resource inside a .lproj is one translation of it, and
		 * a bundle is complete without any given language, so it
		 * is sealed as optional -- which is what Apple marks.
		 */
		if (strstr(sub, ".lproj/") != NULL) {
			CFMutableDictionaryRef e1, e2;
			CFStringRef key = CFStringCreateWithCString(NULL, sub,
			    kCFStringEncodingUTF8);

			e1 = CFDictionaryCreateMutable(NULL, 0,
			    &kCFTypeDictionaryKeyCallBacks,
			    &kCFTypeDictionaryValueCallBacks);
			e2 = CFDictionaryCreateMutable(NULL, 0,
			    &kCFTypeDictionaryKeyCallBacks,
			    &kCFTypeDictionaryValueCallBacks);

			if (key != NULL && e1 != NULL && e2 != NULL) {
				cf_set_data(e1, "hash", sha1, sizeof(sha1));
				CFDictionarySetValue(e1, CFSTR("optional"),
				    kCFBooleanTrue);
				cf_set_data(e2, "hash2", sha256, sizeof(sha256));
				CFDictionarySetValue(e2, CFSTR("optional"),
				    kCFBooleanTrue);
				CFDictionarySetValue(files, key, e1);
				CFDictionarySetValue(files2, key, e2);
			}

			if (key != NULL)
				CFRelease(key);
			if (e1 != NULL)
				CFRelease(e1);
			if (e2 != NULL)
				CFRelease(e2);
		} else {
			CFMutableDictionaryRef e2 =
			    CFDictionaryCreateMutable(NULL, 0,
			    &kCFTypeDictionaryKeyCallBacks,
			    &kCFTypeDictionaryValueCallBacks);
			CFStringRef key = CFStringCreateWithCString(NULL, sub,
			    kCFStringEncodingUTF8);

			if (key != NULL && e2 != NULL) {
				cf_set_data(files, sub, sha1, sizeof(sha1));
				cf_set_data(e2, "hash2", sha256,
				    sizeof(sha256));
				CFDictionarySetValue(files2, key, e2);
			}

			if (key != NULL)
				CFRelease(key);
			if (e2 != NULL)
				CFRelease(e2);
		}
	}

	closedir(d);
}

/* One entry of the rule table: a flag or two and a weight. */
static void
seal_rule(CFMutableDictionaryRef rules, const char *pattern, const char *flag,
    double weight)
{
	CFMutableDictionaryRef v;
	CFStringRef key;

	key = CFStringCreateWithCString(NULL, pattern, kCFStringEncodingUTF8);
	if (key == NULL)
		return;

	if (flag == NULL && weight == 0.0) {
		CFDictionarySetValue(rules, key, kCFBooleanTrue);
		CFRelease(key);
		return;
	}

	v = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	if (v != NULL) {
		if (flag != NULL) {
			CFStringRef f = CFStringCreateWithCString(NULL, flag,
			    kCFStringEncodingUTF8);

			if (f != NULL) {
				CFDictionarySetValue(v, f, kCFBooleanTrue);
				CFRelease(f);
			}
		}

		if (weight != 0.0) {
			CFNumberRef w = CFNumberCreate(NULL, kCFNumberDoubleType,
			    &weight);

			if (w != NULL) {
				CFDictionarySetValue(v, CFSTR("weight"), w);
				CFRelease(w);
			}
		}

		CFDictionarySetValue(rules, key, v);
		CFRelease(v);
	}

	CFRelease(key);
}

/*
 * What a bundle seals, and what it deliberately does not.
 *
 * Read back from a bundle Apple signed rather than invented.  The two
 * that matter most are the omissions: Info.plist and PkgInfo are
 * covered by the code directory's own slots, and everything under
 * MacOS is code in its own right and signed as such, so sealing any
 * of them here would seal them twice and verification would object.
 */
static CFMutableDictionaryRef
seal_rules(int modern)
{
	CFMutableDictionaryRef r = CFDictionaryCreateMutable(NULL, 0,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	if (r == NULL)
		return NULL;

	if (!modern) {
		seal_rule(r, "^Resources/", NULL, 0.0);
		seal_rule(r, "^Resources/.*\\.lproj/", "optional", 1000.0);
		seal_rule(r, "^Resources/.*\\.lproj/locversion.plist$", "omit",
		    1100.0);
		seal_rule(r, "^Resources/Base\\.lproj/", NULL, 1010.0);
		seal_rule(r, "^version.plist$", NULL, 0.0);

		return r;
	}

	seal_rule(r, "^.*", NULL, 0.0);
	seal_rule(r, ".*\\.dSYM($|/)", NULL, 11.0);
	seal_rule(r, "^(.*/)?\\.DS_Store$", "omit", 2000.0);
	seal_rule(r, "^(Frameworks|SharedFrameworks|PlugIns|Plug-ins|"
	    "XPCServices|Helpers|MacOS|Library/(Automator|Spotlight|"
	    "LoginItems))/", "nested", 10.0);
	seal_rule(r, "^Info\\.plist$", "omit", 20.0);
	seal_rule(r, "^PkgInfo$", "omit", 20.0);
	seal_rule(r, "^Resources/", NULL, 20.0);
	seal_rule(r, "^Resources/.*\\.lproj/", "optional", 1000.0);
	seal_rule(r, "^Resources/.*\\.lproj/locversion.plist$", "omit", 1100.0);
	seal_rule(r, "^Resources/Base\\.lproj/", NULL, 1010.0);
	seal_rule(r, "^[^/]+$", "nested", 10.0);
	seal_rule(r, "^embedded\\.provisionprofile$", NULL, 20.0);
	seal_rule(r, "^version\\.plist$", NULL, 20.0);

	return r;
}

/*
 * Write the bundle's seal and remember its hash for slot -3.
 */
static int
build_code_resources(const char *bundle, const char *contents)
{
	CFMutableDictionaryRef root, files, files2, rules, rules2;
	char dir[4096], path[4096];
	CFDataRef data;
	FILE *fp;
	int rc = -1;

	root = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	files = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	files2 = CFDictionaryCreateMutable(NULL, 0,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	if (root == NULL || files == NULL || files2 == NULL)
		goto out;

	snprintf(dir, sizeof(dir), "%s", contents);
	seal_directory(dir, "Resources", files, files2);

	CFDictionarySetValue(root, CFSTR("files"), files);
	CFDictionarySetValue(root, CFSTR("files2"), files2);

	if ((rules = seal_rules(0)) != NULL) {
		CFDictionarySetValue(root, CFSTR("rules"), rules);
		CFRelease(rules);
	}
	if ((rules2 = seal_rules(1)) != NULL) {
		CFDictionarySetValue(root, CFSTR("rules2"), rules2);
		CFRelease(rules2);
	}

	data = CFPropertyListCreateData(NULL, root, kCFPropertyListXMLFormat_v1_0,
	    0, NULL);
	if (data == NULL)
		goto out;

	snprintf(path, sizeof(path), "%s/_CodeSignature", contents);
	mkdir(path, 0777);
	snprintf(path, sizeof(path), "%s/_CodeSignature/CodeResources",
	    contents);

	if ((fp = fopen(path, "wb")) != NULL) {
		fwrite(CFDataGetBytePtr(data), 1,
		    (size_t)CFDataGetLength(data), fp);
		fclose(fp);

		sha256_raw(CFDataGetBytePtr(data),
		    (size_t)CFDataGetLength(data), g_resource_seal);
		g_have_resource_seal = 1;
		rc = 0;
	}

	CFRelease(data);
out:
	if (files != NULL)
		CFRelease(files);
	if (files2 != NULL)
		CFRelease(files2);
	if (root != NULL)
		CFRelease(root);

	return rc;
}

static int
sign_arch(struct arch_info *ai,
    const char *identifier, const char *ent_xml, size_t ent_xml_len,
    int adhoc, const struct signer_info *si,
    uint32_t cd_flags, uint32_t page_size_log2,
    const char *keychain_id,
    const char *cert_file, const char *key_file,
    const char *p12_file, const char *key_password,
    uint8_t *sig_out, size_t *sig_len, size_t sig_out_cap)
{
	uint64_t code_limit = ai->code_limit;
	uint64_t code_limit64 = 0;
	uint32_t page_size = page_size_log2 ? (1u << page_size_log2) : 0;

	if (code_limit > 0xFFFFFFFFULL) {
		code_limit64 = code_limit;
		code_limit = 0;
	}

	uint32_t n_pages;
	if (page_size > 0) {
		n_pages = (uint32_t)(code_limit / page_size);
		if (code_limit % page_size > 0)
			n_pages++;
	} else {
		n_pages = (code_limit > 0) ? 1 : 0;
	}

	/* Requirements blob */
	uint8_t req_blob[4096];
	size_t req_len = build_requirements_blob(identifier, si,
	    req_blob, sizeof(req_blob));

	uint8_t empty_sha256[CS_SHA256_LEN];
	memset(empty_sha256, 0, CS_SHA256_LEN);

	/* Entitlements blobs — only when user explicitly specified them */
	uint8_t ent_xml_blob[65536];
	size_t ent_xml_blob_len = 0;
	uint8_t ent_der_blob[65536];
	size_t ent_der_blob_len = 0;

	if (ent_xml && ent_xml_len > 0) {
		if (ai->file_type == MH_EXECUTE ||
		    ai->file_type == MH_BUNDLE) {
			ent_xml_blob_len = build_entitlements_xml(
			    ent_xml, ent_xml_blob, sizeof(ent_xml_blob));
		}
		ent_der_blob_len = build_entitlements_der(
		    ent_xml, ent_der_blob, sizeof(ent_der_blob));
	}

	/* Build special slots from most-negative to slot -1.
	 * For executables, Apple includes "empty" placeholder slots
	 * after DER and XML entitlements (slots -6 and -4). */
	#define MAX_SPECIAL 8
	uint8_t special_data[MAX_SPECIAL * CS_SHA256_LEN];
	uint32_t n_special = 0;
	/* For executables: DER Entitlements, then Empty placeholder */
	if (ai->file_type == MH_EXECUTE) {
		if (ent_der_blob_len > 0)
			sha256_raw(ent_der_blob, ent_der_blob_len,
			    special_data + n_special * CS_SHA256_LEN);
		else
			memset(special_data + n_special * CS_SHA256_LEN, 0,
			    CS_SHA256_LEN);
		n_special++;
		/* Empty placeholder */
		memset(special_data + n_special * CS_SHA256_LEN, 0,
		    CS_SHA256_LEN);
		n_special++;
	}

	/* XML Entitlements */
	if (ent_xml_blob_len > 0)
		sha256_raw(ent_xml_blob, ent_xml_blob_len,
		    special_data + n_special * CS_SHA256_LEN);
	else
		memset(special_data + n_special * CS_SHA256_LEN, 0,
		    CS_SHA256_LEN);
	n_special++;

	/* Empty placeholder */
	memset(special_data + n_special * CS_SHA256_LEN, 0, CS_SHA256_LEN);
	n_special++;

	/* CodeResources (slot -3): the bundle's seal, or zero for a bare
	   Mach-O, which has no bundle around it to seal. */
	if (g_have_resource_seal)
		memcpy(special_data + n_special * CS_SHA256_LEN,
		    g_resource_seal, CS_SHA256_LEN);
	else
		memset(special_data + n_special * CS_SHA256_LEN, 0,
		    CS_SHA256_LEN);
	n_special++;

	/* Requirements (slot -2) */
	if (req_len > 0)
		sha256_raw(req_blob, req_len,
		    special_data + n_special * CS_SHA256_LEN);
	else
		memset(special_data + n_special * CS_SHA256_LEN, 0,
		    CS_SHA256_LEN);
	n_special++;

	/* Info.plist (slot -1, zero for bare Mach-O) */
	memset(special_data + n_special * CS_SHA256_LEN, 0, CS_SHA256_LEN);
	n_special++;

	/* Trim leading zeros (most-negative slots) */
	while (n_special > 0 &&
	    memcmp(special_data, empty_sha256, CS_SHA256_LEN) == 0) {
		memmove(special_data, special_data + CS_SHA256_LEN,
		    (n_special - 1) * CS_SHA256_LEN);
		n_special--;
	}

	/* Add or update LC_CODE_SIGNATURE so it's present when
	 * computing code page hashes. Page 0 includes the LC,
	 * so it must be written with estimated values before hashing. */
	size_t ident_len = strlen(identifier) + 1;
	const char *team_id = (si != NULL && si->team_id[0] &&
	    strcmp(si->team_id, "notset") != 0) ? si->team_id : NULL;
	size_t team_len = team_id ? (strlen(team_id) + 1) : 0;

	/* Initial estimate of signature size (for LC pre-write).
	 * Uses current n_pages; will be recomputed if n_pages
	 * changes after code_limit adjustment. */
	size_t cd_len = 88 + ident_len + team_len +
	    n_special * CS_SHA256_LEN + n_pages * CS_SHA256_LEN;
	uint32_t n_blobs = 3; /* CD, REQ, SIG */
	if (ent_xml_blob_len > 0)
		n_blobs++;
	if (ent_der_blob_len > 0)
		n_blobs++;
	size_t sb_hdr = 12 + (size_t)n_blobs * 8;
	size_t sig_blob_est = adhoc ? 8 : 8192;
	size_t sig_len_est = sb_hdr + cd_len + req_len + sig_blob_est;
	if (ent_xml_blob_len > 0)
		sig_len_est += ent_xml_blob_len;
	if (ent_der_blob_len > 0)
		sig_len_est += ent_der_blob_len;

	/* If no existing LC, add one now.
	 * The code hashes must NOT include the signature data,
	 * so code_limit must equal dataoff. */
	long lc_off = macho_find_codesig_lc(ai);
	if (lc_off < 0) {
		uint64_t new_dataoff = (uint64_t)code_limit;
		new_dataoff = (new_dataoff + 255) & ~(uint64_t)255;
		ai->dataoff = (uint32_t)new_dataoff;
		ai->code_limit = ai->dataoff;
		code_limit = ai->dataoff;

		if (code_limit > 0xFFFFFFFFULL) {
			code_limit64 = code_limit;
			code_limit = 0;
			n_pages = 0;
		} else if (page_size > 0) {
			n_pages = (uint32_t)(code_limit / page_size);
			if (code_limit % page_size > 0)
				n_pages++;
		}

		/* Recompute estimate with corrected n_pages */
		cd_len = 88 + ident_len + team_len +
		    n_special * CS_SHA256_LEN + n_pages * CS_SHA256_LEN;
		sig_len_est = sb_hdr + cd_len + req_len + sig_blob_est;
		if (ent_xml_blob_len > 0)
			sig_len_est += ent_xml_blob_len;
		if (ent_der_blob_len > 0)
			sig_len_est += ent_der_blob_len;

		if (macho_update_codesig_lc(ai, ai->dataoff,
		    (uint32_t)sig_len_est,
		    (uint8_t *)ai->base, ai->size) != 0)
			return -1;
		lc_off = macho_find_codesig_lc(ai);
	}

	/* Pre-write LC with estimated values (page 0 must include LC
	 * with correct values before hashing) */
	if (lc_off >= 0) {
		uint32_t est_dsize = (uint32_t)sig_len_est;
		if (ai->is_le) {
			memcpy((uint8_t *)ai->base + lc_off + 8, &ai->dataoff, 4);
			memcpy((uint8_t *)ai->base + lc_off + 12, &est_dsize, 4);
		} else {
			be_write32((uint8_t *)ai->base + lc_off + 8, ai->dataoff);
			be_write32((uint8_t *)ai->base + lc_off + 12, est_dsize);
		}
	}

	/* Pre-update __LINKEDIT filesize so page 0 hash includes it */
	macho_update_linkedit_seg(ai, ai->dataoff, (uint32_t)sig_len_est);

	/* Now compute code hashes — the LC is present with estimated values */
	uint8_t *sha256_hashes = NULL;
	uint8_t *sha1_hashes = NULL;

	if (n_pages > 0) {
		sha256_hashes = malloc(n_pages * CS_SHA256_LEN);
		sha1_hashes = malloc(n_pages * CS_SHA1_LEN);
		if (!sha256_hashes || !sha1_hashes) {
			free(sha256_hashes);
			free(sha1_hashes);
			return -1;
		}

		for (uint32_t i = 0; i < n_pages; i++) {
			uint32_t start = i * page_size;
			uint32_t len = page_size;
			if (start + len > code_limit)
				len = (uint32_t)(code_limit - start);
			sha256_raw(ai->base + start, len,
			    sha256_hashes + i * CS_SHA256_LEN);
			sha1_raw(ai->base + start, len,
			    sha1_hashes + i * CS_SHA1_LEN);
		}
	}
	uint32_t cd_flags_val = cd_flags;
	if (adhoc)
		cd_flags_val |= CS_ADHOC;

	uint64_t exec_seg_flags = ai->text_exec_flags;
	if (adhoc && ent_xml && ent_xml_len > 0)
		exec_seg_flags |= CS_EXECSEG_ALLOW_UNSIGNED;
	if (cd_flags & CS_ALLOW_UNSIGNED)
		exec_seg_flags |= CS_EXECSEG_ALLOW_UNSIGNED;

	/* SHA256 CodeDirectory (primary) */
	struct code_directory cd256 = {0};
	cd256.magic = CSMAGIC_CODEDIRECTORY;
	cd256.version = CS_CD_VERSION;
	cd256.flags = cd_flags_val;
	cd256.hash_size = CS_SHA256_LEN;
	cd256.hash_type = CS_HASHTYPE_SHA256;
	cd256.platform = 0;
	cd256.page_size = page_size_log2;
	cd256.n_special_slots = n_special;
	cd256.n_code_slots = n_pages;
	cd256.code_limit = (uint32_t)code_limit;
	cd256.code_limit64 = code_limit64;
	cd256.exec_seg_base = 0;
	cd256.exec_seg_limit = ai->text_vmsize;
	cd256.exec_seg_flags = exec_seg_flags;
	cd256.identifier = identifier;
	cd256.team_id = team_id;
	cd256.special_hash = special_data;
	cd256.code_hash = sha256_hashes;

	uint8_t cd256_buf[16384];
	size_t cd256_len = build_code_directory(&cd256, cd256_buf, sizeof(cd256_buf));
	if (cd256_len == 0) {
		free(sha256_hashes);
		free(sha1_hashes);
		return -1;
	}

	/* SHA1 CodeDirectory (alternate, only for non-adhoc) */
	uint8_t cd1_buf[16384];
	size_t cd1_len = 0;

	if (!adhoc && sha1_hashes) {
		struct code_directory cd1 = {0};
		cd1.magic = CSMAGIC_CODEDIRECTORY;
		cd1.version = CS_CD_VERSION;
		cd1.flags = cd_flags_val;
		cd1.hash_size = CS_SHA1_LEN;
		cd1.hash_type = CS_HASHTYPE_SHA1;
		cd1.platform = 0;
		cd1.page_size = page_size_log2;
		cd1.n_special_slots = n_special;
		cd1.n_code_slots = n_pages;
		cd1.code_limit = (uint32_t)code_limit;
		cd1.code_limit64 = code_limit64;
		cd1.exec_seg_base = 0;
		cd1.exec_seg_limit = ai->text_vmsize;
		cd1.exec_seg_flags = exec_seg_flags;
		cd1.identifier = identifier;
		cd1.team_id = team_id;
		cd1.special_hash = special_data;
		cd1.code_hash = sha1_hashes;

		cd1_len = build_code_directory(&cd1, cd1_buf, sizeof(cd1_buf));
		if (cd1_len == 0) {
			free(sha256_hashes);
			free(sha1_hashes);
			return -1;
		}
	}

	/* Signature slot */
	uint8_t sig_blob_buf[65536];
	size_t sig_blob_len = 0;

	if (adhoc) {
		sig_blob_len = sizeof(sig_blob_buf);
		if (build_adhoc_wrapper(sig_blob_buf, &sig_blob_len) != 0) {
			free(sha256_hashes);
			free(sha1_hashes);
			return -1;
		}
	} else {
		uint8_t cd256_hash[CS_SHA256_LEN];
		sha256_raw(cd256_buf, cd256_len, cd256_hash);
		/*
		 * Hash agility carries each CodeDirectory hash truncated
		 * to twenty bytes -- the same value codesign(1) prints as
		 * CandidateCDHash -- inside a complete property list.  A
		 * bare dict fragment is not a plist and makes the whole
		 * attribute, and with it the signature, unreadable.
		 */
		char cdhash_b64[64];
		EVP_EncodeBlock((unsigned char *)cdhash_b64, cd256_hash,
		    CS_SHA1_LEN);
		char plist_buf[640];
		snprintf(plist_buf, sizeof(plist_buf),
		    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		    "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
		    "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
		    "<plist version=\"1.0\">\n"
		    "<dict>\n"
		    "\t<key>cdhashes</key>\n"
		    "\t<array>\n"
		    "\t\t<data>\n"
		    "\t\t%s\n"
		    "\t\t</data>\n"
		    "\t</array>\n"
		    "</dict>\n"
		    "</plist>",
		    cdhash_b64);

		/*
		 * The CMS covers the CodeDirectory itself, detached; its
		 * message digest is what codesign(1) reports as CMSDigest
		 * and compares against the CodeDirectory hash.
		 */
		sig_blob_len = sizeof(sig_blob_buf);
		if (build_cms_signature(cd256_buf, cd256_len,
		    plist_buf, keychain_id,
		    cert_file, key_file, p12_file, key_password,
		    sig_blob_buf, &sig_blob_len,
		    NULL, 0, NULL, 0) != 0) {
			free(sha256_hashes);
			free(sha1_hashes);
			return -1;
		}
	}

	/* Assemble SuperBlob */
	struct superblob_builder sbb;
	sbb_init(&sbb);
	sbb_add(&sbb, CSSLOT_CODEDIRECTORY, cd256_buf, cd256_len);
	/*
	 * One SHA-256 CodeDirectory, as codesign(1) itself emits.  A
	 * SHA-1 directory in the alternate slot becomes the one the CMS
	 * is expected to cover, and nothing on a current system asks for
	 * it.  The SHA-1 digests stay in the hash-agility attribute.
	 */
	sbb_add(&sbb, CSSLOT_REQUIREMENTS, req_blob, req_len);

	if (ent_xml_blob_len > 0)
		sbb_add(&sbb, CSSLOT_ENTITLEMENTS, ent_xml_blob, ent_xml_blob_len);
	if (ent_der_blob_len > 0)
		sbb_add(&sbb, CSSLOT_DER_ENTITLEMENTS, ent_der_blob, ent_der_blob_len);

	sbb_add(&sbb, CSSLOT_SIGNATURESLOT, sig_blob_buf, sig_blob_len);

	/* Emit */
	if (sbb_emit(&sbb, sig_out, sig_len) != 0) {
		free(sha256_hashes);
		free(sha1_hashes);
		return -1;
	}

	/*
	 * The load command was written with the estimated size before the
	 * code pages were hashed, so page zero -- which contains it --
	 * seals that number.  Hand back a blob of exactly that size,
	 * zero-padded past the superblob, rather than shrinking the load
	 * command afterwards and invalidating the hash we just sealed.
	 */
	if (sig_len_est >= *sig_len && sig_len_est <= sig_out_cap) {
		memset(sig_out + *sig_len, 0, sig_len_est - *sig_len);
		*sig_len = sig_len_est;
	} else if (sig_len_est < *sig_len) {
		fprintf(stderr, "codesign: signature larger than reserved space "
		    "(%zu > %zu)\n", *sig_len, sig_len_est);
		free(sha256_hashes);
		free(sha1_hashes);
		return -1;
	}

	free(sha256_hashes);
	free(sha1_hashes);
	return 0;
}

/* ---- Sign Mach-O file ---- */


/*
 * Fat header fields are big-endian; the byte-swapped variant is read the
 * other way round.
 */
static uint32_t
fat_read32(const uint8_t *p, int is_le)
{
	if (is_le)
		return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
		    ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
	return be_read32(p);
}

static void
fat_write32(uint8_t *p, uint32_t v, int is_le)
{
	if (is_le) {
		p[0] = (uint8_t)v;
		p[1] = (uint8_t)(v >> 8);
		p[2] = (uint8_t)(v >> 16);
		p[3] = (uint8_t)(v >> 24);
		return;
	}
	be_write32(p, v);
}

/*
 * Sign every slice of a universal file.
 *
 * A slice is signed as the thin Mach-O it is: its code limit, the
 * signature offset in its load command, and the segment sizes are all
 * relative to the start of the slice, not the file.  So each slice is
 * signed in a buffer of its own and the file is then laid out again
 * around the results, since a slice that has grown pushes the ones
 * after it and the offsets in the fat header no longer hold.
 */
static int
sign_fat(const char *path, uint8_t *file_data, size_t file_sz,
    struct macho_file *mf, const char *final_id, const char *ent_xml,
    size_t ent_xml_len, int adhoc, const struct signer_info *si,
    uint32_t cd_flags, uint32_t page_size_log2, const char *keychain_id,
    const char *cert_file, const char *key_file, const char *p12_file,
    const char *key_password, int force)
{
	struct slice {
		uint8_t *buf;
		size_t len;
		uint32_t cputype;
		uint32_t cpusubtype;
		uint32_t align;
	} slices[16];
	int is_le = (be_read32(file_data) == 0xbebafeca);
	uint8_t *out = NULL;
	size_t out_len, cursor;
	int n = mf->n_archs;
	int result = 0;
	int i;

	for (i = 0; i < 16; i++)
		slices[i].buf = NULL;

	for (i = 0; i < n; i++) {
		size_t hdr = 8 + (size_t)i * 20;
		uint32_t off = fat_read32(file_data + hdr + 8, is_le);
		uint32_t sz = fat_read32(file_data + hdr + 12, is_le);
		struct macho_file thin;
		struct arch_info *ai;
		uint8_t sig_buf[65536];
		size_t sig_len = sizeof(sig_buf);
		size_t cap;

		slices[i].cputype = fat_read32(file_data + hdr, is_le);
		slices[i].cpusubtype = fat_read32(file_data + hdr + 4, is_le);
		slices[i].align = fat_read32(file_data + hdr + 16, is_le);

		/*
		 * Room for the slice plus the signature that is about to
		 * be appended to it, since sign_arch writes through the
		 * buffer it is given.
		 */
		cap = (size_t)sz + sizeof(sig_buf) + 4096;
		if ((slices[i].buf = calloc(1, cap)) == NULL) {
			result = -1;
			goto out;
		}
		memcpy(slices[i].buf, file_data + off, sz);

		/*
		 * Parse at the slice's real length: the slack beyond it
		 * is room to write into, not content, and counting it
		 * would put the code limit past the end of the code.
		 */
		if (macho_parse(slices[i].buf, sz, &thin) != 0) {
			fprintf(stderr, "codesign: %s: cannot read %s slice\n",
			    path, macho_arch_name(slices[i].cputype));
			result = -1;
			goto out;
		}
		ai = &thin.archs[0];

		if (macho_has_codesig(ai) && !force) {
			fprintf(stderr, "codesign: %s: is already signed\n", path);
			result = 1;
			goto out;
		}

		if (sign_arch(ai, final_id, ent_xml, ent_xml_len, adhoc,
		    si, cd_flags, page_size_log2, keychain_id,
		    cert_file, key_file, p12_file, key_password,
		    sig_buf, &sig_len, sizeof(sig_buf)) != 0) {
			fprintf(stderr, "codesign: signing failed for %s (%s)\n",
			    path, macho_arch_name(slices[i].cputype));
			result = 1;
			goto out;
		}

		if ((size_t)ai->dataoff + sig_len > cap) {
			result = -1;
			goto out;
		}
		memcpy(slices[i].buf + ai->dataoff, sig_buf, sig_len);
		ai->datasize = (uint32_t)sig_len;
		macho_update_codesig_lc(ai, ai->dataoff, (uint32_t)sig_len,
		    slices[i].buf, cap);
		macho_update_linkedit_seg(ai, ai->dataoff, (uint32_t)sig_len);

		slices[i].len = (size_t)ai->dataoff + sig_len;
	}

	/* Lay the file out again around the signed slices. */
	cursor = 8 + (size_t)n * 20;
	out_len = cursor;
	for (i = 0; i < n; i++) {
		size_t a = (size_t)1 << slices[i].align;

		out_len = (out_len + a - 1) & ~(a - 1);
		out_len += slices[i].len;
	}

	if ((out = calloc(1, out_len)) == NULL) {
		result = -1;
		goto out;
	}

	memcpy(out, file_data, 8);		/* magic and count */
	for (i = 0; i < n; i++) {
		size_t hdr = 8 + (size_t)i * 20;
		size_t a = (size_t)1 << slices[i].align;

		cursor = (cursor + a - 1) & ~(a - 1);

		fat_write32(out + hdr, slices[i].cputype, is_le);
		fat_write32(out + hdr + 4, slices[i].cpusubtype, is_le);
		fat_write32(out + hdr + 8, (uint32_t)cursor, is_le);
		fat_write32(out + hdr + 12, (uint32_t)slices[i].len, is_le);
		fat_write32(out + hdr + 16, slices[i].align, is_le);

		memcpy(out + cursor, slices[i].buf, slices[i].len);
		cursor += slices[i].len;
	}

	if (cs_write_file(path, out, out_len) != 0) {
		fprintf(stderr, "codesign: cannot write: %s\n", path);
		result = -1;
		goto out;
	}

out:
	free(out);
	for (i = 0; i < 16; i++)
		free(slices[i].buf);
	(void)file_sz;
	return result;
}

int
sign_macho(const char *path, const char *identity, int force,
    int adhoc, const char *identifier,
    const char *entitlements_file, uint32_t cd_flags,
    uint32_t page_size_log2, const char *cert_file,
    const char *key_file, const char *p12_file,
    const char *key_password, const char *req_str)
{
	size_t file_sz;
	uint8_t *file_data = cs_read_file(path, &file_sz);
	if (!file_data) {
		fprintf(stderr, "codesign: cannot read: %s\n", path);
		return -1;
	}

	struct macho_file mf;
	if (macho_parse(file_data, file_sz, &mf) != 0) {
		fprintf(stderr, "codesign: not a Mach-O file: %s\n", path);
		free(file_data);
		return -1;
	}

	/* Determine identifier */
	const char *final_id = identifier;
	char id_buf[256];
	if (!final_id) {
		const char *bn = cs_basename(path);
		snprintf(id_buf, sizeof(id_buf), "com.apple.code-sign.%s", bn);
		free((void *)bn);
		final_id = id_buf;
	}

	/* Read entitlements */
	char *ent_xml = NULL;
	size_t ent_xml_len = 0;
	if (entitlements_file) {
		ent_xml = (char *)cs_read_file(entitlements_file, &ent_xml_len);
		if (!ent_xml) {
			fprintf(stderr, "codesign: cannot read entitlements: %s\n",
			    entitlements_file);
			free(file_data);
			return -1;
		}
		/* Strip blob header if present */
		if (ent_xml_len >= 8) {
			uint32_t magic = be_read32((uint8_t *)ent_xml);
			if (magic == CSMAGIC_EMBEDDED_ENTITLEMENTS) {
				size_t xm_len = ent_xml_len - 8;
				memmove(ent_xml, ent_xml + 8, xm_len);
				ent_xml[xm_len] = '\0';
				ent_xml_len = xm_len;
			}
		}
	} else {
		ent_xml = NULL;
		ent_xml_len = 0;
	}

	/* Cert CN and team ID */
	struct signer_info signer;

	memset(&signer, 0, sizeof(signer));

	/*
	 * With no certificate or key file to read, a non-ad-hoc identity
	 * names something in the keychain; codesign(1) has already
	 * checked that the name matches.
	 */
	const char *keychain_id = NULL;
	if (!adhoc && identity != NULL && p12_file == NULL && cert_file == NULL)
		keychain_id = identity;

	if (!adhoc && (keychain_id || p12_file || (cert_file && key_file))) {
		if (load_identity_info(keychain_id, cert_file, key_file, p12_file,
		    key_password, &signer) != 0) {
			if (!g_continue_on_error) {
				if (ent_xml) free(ent_xml);
				free(file_data);
				return -1;
			}
		}
	}

	/*
	 * A universal file is laid out again around its signed slices;
	 * the thin case can sign in place.
	 */
	if (mf.is_fat) {
		int r = sign_fat(path, file_data, file_sz, &mf, final_id,
		    ent_xml, ent_xml_len, adhoc, &signer, cd_flags, page_size_log2,
		    keychain_id, cert_file, key_file, p12_file, key_password,
		    force);

		if (ent_xml)
			free(ent_xml);
		free(file_data);
		return r;
	}

	/* Save arch base offsets (they don't change, only file may grow) */
	ptrdiff_t arch_offsets[16];
	for (int i = 0; i < mf.n_archs; i++) {
		arch_offsets[i] = (uint8_t *)mf.archs[i].base - file_data;
	}

	/* Track actual file size needed across all architectures */
	size_t actual_end = 0;

	/* Sign each architecture */
	int result = 0;
	for (int i = 0; i < mf.n_archs; i++) {
		struct arch_info *ai = &mf.archs[i];
		ai->base = file_data + arch_offsets[i];

		int has_sig = macho_has_codesig(ai);
		if (has_sig && !force) {
			fprintf(stderr, "codesign: %s: is already signed\n", path);
			result = 1;
			goto sign_cleanup;
		}

		uint8_t sig_buf[65536];
		size_t sig_len = sizeof(sig_buf);

		if (sign_arch(ai, final_id, ent_xml, ent_xml_len,
		    adhoc, &signer,
		    cd_flags, page_size_log2,
		    keychain_id, cert_file, key_file, p12_file, key_password,
		    sig_buf, &sig_len, sizeof(sig_buf)) != 0) {
			fprintf(stderr, "codesign: signing failed for %s\n", path);
			result = 1;
			goto sign_cleanup;
		}

		/* Write signature into file data */
		uint32_t write_dataoff;
		if (has_sig) {
			write_dataoff = ai->dataoff;
			size_t abs_end = arch_offsets[i] + write_dataoff + sig_len;
			if (abs_end > file_sz) {
				uint8_t *nd = realloc(file_data, abs_end);
				if (!nd) { result = -1; goto sign_cleanup; }
				file_data = nd;
			}
			if (abs_end > actual_end)
				actual_end = abs_end;
		} else {
			/* Append signature, extend file */
			size_t new_dataoff = (size_t)file_sz;
			new_dataoff = (new_dataoff + 255) & ~(size_t)255;
			size_t new_size = new_dataoff + sig_len;
			uint8_t *nd = realloc(file_data, new_size);
			if (!nd) { result = -1; goto sign_cleanup; }
			file_data = nd;
			file_sz = new_size;
			write_dataoff = (uint32_t)new_dataoff;
			if (new_size > actual_end)
				actual_end = new_size;
		}

		/* Fix up all arch base pointers after potential realloc */
		for (int j = 0; j < mf.n_archs; j++) {
			mf.archs[j].base = file_data + arch_offsets[j];
		}
		ai->base = file_data + arch_offsets[i];

		memcpy((uint8_t *)ai->base + write_dataoff, sig_buf, sig_len);

		ai->dataoff = write_dataoff;
		ai->datasize = (uint32_t)sig_len;
		macho_update_codesig_lc(ai, ai->dataoff, (uint32_t)sig_len,
		    file_data, file_sz);
		macho_update_linkedit_seg(ai, ai->dataoff, (uint32_t)sig_len);
	}

	/* Truncate to the actual used size (removes trailing garbage from
	 * old, larger signatures that were replaced) */
	file_sz = actual_end;

	if (cs_write_file(path, file_data, file_sz) != 0) {
		fprintf(stderr, "codesign: cannot write: %s\n", path);
		result = -1;
	}

	if (g_verbose)
		fprintf(stderr, "%s: signed\n", path);

sign_cleanup:
	if (entitlements_file && ent_xml) free(ent_xml);
	free(file_data);
	return result;
}

/* ---- Bundle signing ---- */

int
sign_bundle(const char *path, const char *identity, int force,
    int adhoc, const char *identifier,
    const char *entitlements_file, uint32_t cd_flags,
    uint32_t page_size_log2, const char *cert_file,
    const char *key_file, const char *p12_file,
    const char *key_password, const char *req_str)
{
	char exe_path[4096], contents[4096];
	char *exe_name = find_bundle_executable(path);
	struct stat st;

	/*
	 * Where a bundle keeps things depends on what kind it is.  An
	 * application has Contents, with the binary under MacOS; a
	 * framework is versioned, and its binary sits in the version
	 * directory beside its Resources.
	 */
	snprintf(contents, sizeof(contents), "%s/Contents", path);

	if (stat(contents, &st) != 0 || !S_ISDIR(st.st_mode))
		snprintf(contents, sizeof(contents), "%s/Versions/Current",
		    path);

	if (!exe_name) {
		/*
		 * A bundle with no Info.plist to name its executable --
		 * a framework built without one -- is named after itself.
		 */
		const char *base = strrchr(path, '/');
		char *dot;

		base = (base != NULL) ? base + 1 : path;
		exe_name = strdup(base);

		if (exe_name != NULL && (dot = strrchr(exe_name, '.')) != NULL)
			*dot = '\0';

		if (exe_name == NULL) {
			fprintf(stderr, "codesign: cannot find"
			    " CFBundleExecutable\n");
			return -1;
		}
	}

	snprintf(exe_path, sizeof(exe_path), "%s/MacOS/%s", contents, exe_name);

	if (!cs_file_exists(exe_path))
		snprintf(exe_path, sizeof(exe_path), "%s/%s", contents,
		    exe_name);

	free(exe_name);

	if (!cs_file_exists(exe_path)) {
		fprintf(stderr, "codesign: executable not found: %s\n", exe_path);
		return -1;
	}

	const char *final_id = identifier;
	char id_buf[256];
	if (!final_id) {
		final_id = find_bundle_identifier(path);
		if (!final_id) {
			snprintf(id_buf, sizeof(id_buf), "com.apple.code-sign.%s", path);
			final_id = id_buf;
		}
	}

	/*
	 * Seal the bundle before signing what is inside it: the code
	 * directory carries the seal's hash, so it has to exist first.
	 */
	if (build_code_resources(path, contents) != 0)
		fprintf(stderr, "codesign: warning: cannot write the resource"
		    " seal for %s\n", path);

	int result = sign_macho(exe_path, identity, force, adhoc,
	    final_id, entitlements_file, cd_flags,
	    page_size_log2, cert_file, key_file, p12_file,
	    key_password, req_str);

	/* Create/overwrite CodeResources */
	char cre_path[4096];
	snprintf(cre_path, sizeof(cre_path), "%s/_CodeSignature/CodeResources", path);
	FILE *cfp = fopen(cre_path, "wb");
	if (cfp) {
		fprintf(cfp,
		    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		    "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
		    "<plist version=\"1.0\">\n"
		    "<dict>\n"
		    "  <key>files</key>\n  <dict/>\n"
		    "  <key>files2</key>\n  <dict/>\n"
		    "  <key>rules</key>\n  <dict/>\n"
		    "  <key>rules2</key>\n  <dict/>\n"
		    "</dict>\n"
		    "</plist>\n");
		fclose(cfp);
	}

	if (final_id != identifier && final_id != id_buf)
		free((char *)final_id);

	if (result == 0 && g_verbose)
		fprintf(stderr, "%s: signed\n", path);


	g_have_resource_seal = 0;
	return result;
}

/* ---- Remove signature ---- */

int
remove_signature(const char *path)
{
	if (cs_is_directory(path)) {
		char exe_path[4096];
		char *exe_name = find_bundle_executable(path);
		if (!exe_name)
			return -1;
		snprintf(exe_path, sizeof(exe_path), "%s/Contents/MacOS/%s",
		    path, exe_name);
		free(exe_name);
		return remove_signature(exe_path);
	}

	size_t file_sz;
	uint8_t *data = cs_read_file(path, &file_sz);
	if (!data) {
		fprintf(stderr, "codesign: cannot read: %s\n", path);
		return -1;
	}

	struct macho_file mf;
	if (macho_parse(data, file_sz, &mf) != 0) {
		free(data);
		return -1;
	}

	int found = 0;
	for (int i = 0; i < mf.n_archs; i++) {
		if (macho_has_codesig(&mf.archs[i]))
			found = 1;
	}

	if (!found) {
		fprintf(stderr, "codesign: %s: not signed\n", path);
		free(data);
		return 1;
	}

	if (cs_write_file(path, data, file_sz) != 0) {
		fprintf(stderr, "codesign: cannot write: %s\n", path);
		free(data);
		return -1;
	}

	free(data);
	return 0;
}
