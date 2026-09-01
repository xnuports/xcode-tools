/* xcodebuild -- open source reimplementation of Apple's xcodebuild utility
 *
 * Build-settings table, .xcconfig parsing and -showBuildSettings rendering.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  3. Neither the name of the copyright holder nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <limits.h>
#include <libgen.h>
#include <unistd.h>

#include <CoreFoundation/CoreFoundation.h>

#include "xcodebuild.h"
#include "ini.h"
#include "plist.h"

/* ------------------------------------------------------------------ */
/* Settings table                                                       */
/* ------------------------------------------------------------------ */

static setting_entry *find_entry(settings_table *t, const char *key)
{
	if (t == NULL || key == NULL)
		return NULL;
	for (size_t i = 0; i < t->count; i++)
		if (strcmp(t->entries[i].key, key) == 0)
			return &t->entries[i];
	return NULL;
}

settings_table *settings_create(void)
{
	settings_table *t = (settings_table *)calloc(1, sizeof(settings_table));
	return t;
}

void settings_destroy(settings_table *t)
{
	if (t == NULL)
		return;
	for (size_t i = 0; i < t->count; i++) {
		free(t->entries[i].key);
		free(t->entries[i].value);
	}
	free(t->entries);
	free(t);
}

int settings_has(const settings_table *t, const char *key)
{
	return find_entry((settings_table *)t, key) != NULL;
}

const char *settings_get(const settings_table *t, const char *key)
{
	setting_entry *e = find_entry((settings_table *)t, key);
	return e ? e->value : NULL;
}

const char *settings_get_or(const settings_table *t, const char *key, const char *fallback)
{
	const char *v = settings_get(t, key);
	return v ? v : fallback;
}

void settings_set(settings_table *t, const char *key, const char *value)
{
	if (t == NULL || key == NULL)
		return;
	setting_entry *e = find_entry(t, key);
	if (e != NULL) {
		free(e->value);
		e->value = strdup(value ? value : "");
		return;
	}
	if (t->count == t->capacity) {
		size_t cap = t->capacity ? t->capacity * 2 : 64;
		setting_entry *entries = (setting_entry *)realloc(t->entries, sizeof(setting_entry) * cap);
		if (entries == NULL)
			return;
		t->entries = entries;
		t->capacity = cap;
	}
	t->entries[t->count].key = strdup(key);
	t->entries[t->count].value = strdup(value ? value : "");
	if (t->entries[t->count].key == NULL || t->entries[t->count].value == NULL)
		return;
	t->count++;
}

void settings_defaults_set(settings_table *t, const char *key, const char *value)
{
	if (find_entry(t, key) != NULL)
		return;
	settings_set(t, key, value);
}

/* Expand $(VAR), ${VAR} and ${VAR:-default} references. Unknown variables
 * are left verbatim. Returns a malloc'd string. */
/* Expand $(VAR) and ${VAR} (and ${VAR:-default}) references against `t`.
 * Unknown variables are left verbatim. Expansion is recursive with a depth
 * guard to prevent infinite loops on self-referential definitions. Returns a
 * malloc'd string. */
static char *expand_inner(const settings_table *t, const char *value, int depth);

static char *append_str(char *out, size_t *n, size_t *cap, const char *s)
{
	size_t slen = strlen(s);
	size_t need = *n + slen + 1;
	if (need > *cap) {
		while (need > *cap) *cap *= 2;
		char *tmp = (char *)realloc(out, *cap);
		if (tmp == NULL) { free(out); return NULL; }
		out = tmp;
	}
	memcpy(out + *n, s, slen);
	*n += slen;
	out[*n] = '\0';
	return out;
}

static char *expand_inner(const settings_table *t, const char *value, int depth)
{
	if (value == NULL)
		return strdup("");

	size_t cap = strlen(value) + 16;
	char *out = (char *)malloc(cap);
	if (out == NULL)
		return NULL;
	size_t n = 0;
	const char *p = value;

	while (*p) {
		if (*p == '$' && *(p + 1) == '(') {
			const char *end = strchr(p + 2, ')');
			if (end != NULL) {
				if (depth >= 32) {
					out = append_str(out, &n, &cap, p);
					if (out == NULL) return NULL;
					p = end + 1;
					continue;
				}
				size_t klen = end - (p + 2);
				char *key = strndup(p + 2, klen);
				const char *val = key ? settings_get(t, key) : NULL;
				if (val != NULL) {
					char *sub = expand_inner(t, val, depth + 1);
					if (sub != NULL) {
						out = append_str(out, &n, &cap, sub);
						free(sub);
						if (out == NULL) { free(key); return NULL; }
					}
				} else {
					char *lit = strndup(p, end - p + 1);
					out = append_str(out, &n, &cap, lit);
					free(lit);
					if (out == NULL) { free(key); return NULL; }
				}
				free(key);
				p = end + 1;
				continue;
			}
		}
		if (*p == '$' && *(p + 1) == '{') {
			const char *end = strchr(p + 2, '}');
			if (end != NULL) {
				if (depth >= 32) {
					out = append_str(out, &n, &cap, p);
					if (out == NULL) return NULL;
					p = end + 1;
					continue;
				}
				size_t klen = end - (p + 2);
				char *key = strndup(p + 2, klen);
				char *def = NULL;
				char *colon = key ? strstr(key, ":-") : NULL;
				if (colon != NULL) {
					*colon = '\0';
					def = colon + 2;
				}
				const char *val = (key && *key) ? settings_get(t, key) : NULL;
				if (val == NULL && def != NULL)
					val = def;
				if (val != NULL) {
					char *sub = expand_inner(t, val, depth + 1);
					if (sub != NULL) {
						out = append_str(out, &n, &cap, sub);
						free(sub);
						if (out == NULL) { free(key); return NULL; }
					}
				} else {
					char *lit = strndup(p, end - p + 1);
					out = append_str(out, &n, &cap, lit);
					free(lit);
					if (out == NULL) { free(key); return NULL; }
				}
				free(key);
				p = end + 1;
				continue;
			}
		}
		if (n + 1 >= cap) {
			cap *= 2;
			char *tmp = (char *)realloc(out, cap);
			if (tmp == NULL) { free(out); return NULL; }
			out = tmp;
		}
		out[n++] = *p++;
	}
	out[n] = '\0';
	return out;
}

char *settings_expand(const settings_table *t, const char *value)
{
	return expand_inner(t, value, 0);
}

/* ------------------------------------------------------------------ */
/* SDK / toolchain config (read from info.ini via inih)                */
/* ------------------------------------------------------------------ */

typedef struct {
	char *name;
	char *version;
	char *toolchain;
	char *default_arch;
	char *deployment_target;
} sdk_info;

typedef struct {
	char *name;
	char *version;
} toolchain_info;

static int sdk_ini_handler(void *user, const char *section, const char *name, const char *value)
{
	sdk_info *cfg = (sdk_info *)user;
	if (strcmp(section, "SDK") == 0) {
		if (strcmp(name, "name") == 0) cfg->name = strdup(value);
		else if (strcmp(name, "version") == 0) cfg->version = strdup(value);
		else if (strcmp(name, "toolchain") == 0) cfg->toolchain = strdup(value);
		else if (strcmp(name, "default_arch") == 0) cfg->default_arch = strdup(value);
		else if (strcmp(name, "macosx_deployment_target") == 0) cfg->deployment_target = strdup(value);
		else if (strcmp(name, "ios_deployment_target") == 0) cfg->deployment_target = strdup(value);
		else if (strcmp(name, "deployment_target") == 0) cfg->deployment_target = strdup(value);
	}
	return 1;
}

static int toolchain_ini_handler(void *user, const char *section, const char *name, const char *value)
{
	toolchain_info *cfg = (toolchain_info *)user;
	if (strcmp(section, "TOOLCHAIN") == 0) {
		if (strcmp(name, "name") == 0) cfg->name = strdup(value);
		else if (strcmp(name, "version") == 0) cfg->version = strdup(value);
	}
	return 1;
}

static int read_sdk_info(const char *path, sdk_info *out)
{
	char info_path[PATH_MAX];
	snprintf(info_path, sizeof(info_path), "%s/info.ini", path);
	return ini_parse(info_path, sdk_ini_handler, out) == -1 ? -1 : 0;
}

static int read_toolchain_info(const char *path, toolchain_info *out)
{
	char info_path[PATH_MAX];
	snprintf(info_path, sizeof(info_path), "%s/info.ini", path);
	return ini_parse(info_path, toolchain_ini_handler, out) == -1 ? -1 : 0;
}

/* Build a target triple from a deployment target version + arch, mirroring
 * xcrun's mapping to a darwin kernel version. */
static void target_triple(char *triple, size_t size, const char *ver, const char *arch)
{
	if (ver == NULL || arch == NULL) {
		snprintf(triple, size, "%s-unknown-darwin", arch ? arch : "unknown");
		return;
	}
	int xx = 0, yy = 0, where = 1;
	const char *s = ver;
	while (*s) {
		if (isdigit((unsigned char)*s)) {
			int d = *s - '0';
			if (where == 1) xx = xx * 10 + d;
			else if (where == 2) yy = yy * 10 + d;
		} else {
			where++;
		}
		s++;
	}
	int kern_ver = 9;
	switch (xx) {
		case 10: kern_ver = yy + 4; break;
		case 9: case 8: case 7: kern_ver = 14; break;
		case 6: kern_ver = 13; break;
		case 5: kern_ver = 11; break;
		case 4: kern_ver = (yy <= 2) ? 10 : 11; break;
		case 3: kern_ver = 10; break;
		case 2: case 1: default: kern_ver = 9; break;
	}
	snprintf(triple, size, "%s-apple-darwin%d", arch, kern_ver);
}

int settings_load_defaults(settings_table *t, const char *devpath,
                           const char *sdkname, const char *toolchain,
                           const char *configuration, const char *arch)
{
	sdk_info sdk = {0};
	toolchain_info tc = {0};
	char sdk_path[PATH_MAX];
	char tc_path[PATH_MAX];
	char triple[128];

	if (devpath == NULL || sdkname == NULL)
		return -1;

	snprintf(sdk_path, sizeof(sdk_path), "%s/SDKs/%s.sdk", devpath, sdkname);
	snprintf(tc_path, sizeof(tc_path), "%s/Toolchains/%s.toolchain", devpath,
	          toolchain ? toolchain : sdkname);

	read_sdk_info(sdk_path, &sdk);
	read_toolchain_info(tc_path, &tc);

	if (sdk.default_arch == NULL && arch == NULL)
		sdk.default_arch = strdup("arm64");
	if (arch != NULL) {
		free(sdk.default_arch);
		sdk.default_arch = strdup(arch);
	}

	/* Resolve deployment target. */
	const char *deploy = sdk.deployment_target ? sdk.deployment_target : "1.0";

	/* Platform / SDK-derived values. */
	settings_defaults_set(t, "ACTION", "build");
	settings_defaults_set(t, "AD_HOC_ENTITLEMENTS_ASSET_PATH", "");
	settings_defaults_set(t, "ALTERNATE_FILE_DEVELOPMENT_LANGUAGE_DIR", "");
	settings_defaults_set(t, "ALWAYS_SEARCH_USER_PATHS", "NO");
	settings_defaults_set(t, "ARCHS", sdk.default_arch ? sdk.default_arch : "arm64");
	settings_defaults_set(t, "BUILD_ACTIVE_ARCH_ONLY", "YES");
	settings_defaults_set(t, "BUILD_DIR", "");
	settings_defaults_set(t, "BUILD_ROOT", "");
	settings_defaults_set(t, "BUILT_PRODUCTS_DIR", "");
	settings_defaults_set(t, "CLANG_ENABLE_MODULES", "YES");
	settings_defaults_set(t, "CLANG_ENABLE_OBJC_ARC", "YES");
	settings_defaults_set(t, "CLANG_WARN_DEPRECATED_FUNCTION_DECL", "YES");
	settings_defaults_set(t, "CLANG_WARN_DOCUMENTATION_COMMENTS", "YES");
	settings_defaults_set(t, "CLEAR_KEEP_TIME_CODE", "NO");
	settings_defaults_set(t, "CODE_SIGNING_ALLOWED", "YES");
	settings_defaults_set(t, "CODE_SIGNING_REQUIRED", "YES");
	settings_defaults_set(t, "COMBINE_HIDPI_IMAGES", "YES");
	settings_defaults_set(t, "CONFIGURATION", configuration ? configuration : "Debug");
	settings_defaults_set(t, "CONFIGURATION_BUILD_DIR", "");
	settings_defaults_set(t, "CONFIGURATION_TEMP_DIR", "");
	settings_defaults_set(t, "CP", "/bin/cp");
	settings_defaults_set(t, "CREATE_EMBEDDED_SOURCE_DIR", "NO");
	settings_defaults_set(t, "DEBUG_INFORMATION_FORMAT", "dwarf-with-dsym");
	settings_defaults_set(t, "DEFINES_MODULE", "NO");
	settings_defaults_set(t, "DEPLOYMENT_TARGET", deploy);
	settings_defaults_set(t, "DERIVED_FILE_DIR", "");
	settings_defaults_set(t, "DERIVED_FILES_DIR", "");
	settings_defaults_set(t, "DEVELOPER_DIR", devpath);
	settings_defaults_set(t, "DSTROOT", "");
	settings_defaults_set(t, "ENABLE_BITCODE", "NO");
	settings_defaults_set(t, "ENABLE_STRICT_OBJC_MSGSEND", "YES");
	settings_defaults_set(t, "ENABLE_TESTABILITY", "YES");
	settings_defaults_set(t, "EXECUTABLE_NAME", "");
	settings_defaults_set(t, "EXECUTABLE_PREFIX", "");
	settings_defaults_set(t, "EXECUTABLE_SUFFIX", "");
	settings_defaults_set(t, "FILE_BASENAME", "");
	settings_defaults_set(t, "FRAMEWORK_SEARCH_PATHS", "");
	settings_defaults_set(t, "GENERATE_INFOPLIST_FILE", "NO");
	settings_defaults_set(t, "GCC_C_LANGUAGE_STANDARD", "gnu11");
	settings_defaults_set(t, "INFOPLIST_FILE", "");
	settings_defaults_set(t, "INFOPLIST_OUTPUT_FORMAT", "same-as-input");
	settings_defaults_set(t, "INFOPLIST_PREFIX_HEADER", "");
	settings_defaults_set(t, "INSTALL_DIR", "");
	settings_defaults_set(t, "INSTALL_ROOT", "");
	settings_defaults_set(t, "LD_RUNPATH_SEARCH_PATHS", "@executable_path/../Frameworks");
	settings_defaults_set(t, "LINK_WITH_STATICS", "YES");
	settings_defaults_set(t, "MACOSX_DEPLOYMENT_TARGET", deploy);
	settings_defaults_set(t, "MODULE_NAME", "");
	settings_defaults_set(t, "PRODUCT_BUNDLE_IDENTIFIER", "");
	settings_defaults_set(t, "PRODUCT_MODULE_NAME", "$(TARGET_NAME)");
	settings_defaults_set(t, "PRODUCT_NAME", "$(TARGET_NAME)");
	settings_defaults_set(t, "PRODUCT_TYPE", "");
	settings_defaults_set(t, "SCAN_ALL_SLICES", "NO");
	settings_defaults_set(t, "SDKROOT", sdk_path);
	settings_defaults_set(t, "SDK_NAME", sdk.name ? sdk.name : sdkname);
	settings_defaults_set(t, "SDK_VERSION", sdk.version ? sdk.version : "1.0.0");
	settings_defaults_set(t, "SDK_VENDOR", "com.apple");
	settings_defaults_set(t, "SDK_DIR", sdk_path);
	settings_defaults_set(t, "SOURCE_ROOT", "");
	settings_defaults_set(t, "SRCROOT", "$(SOURCE_ROOT)");
	settings_defaults_set(t, "SWIFT_ACTIVE_COMPILATION_CONDITIONS", "");
	settings_defaults_set(t, "SWIFT_OPTIMIZATION_LEVEL", "-Onone");
	settings_defaults_set(t, "SWIFT_VERSION", "5.0");
	settings_defaults_set(t, "TARGET_NAME", "");
	settings_defaults_set(t, "TARGET_TEMP_DIR", "");
	settings_defaults_set(t, "TOOLCHAINS", tc.name ? tc.name : (toolchain ? toolchain : sdkname));
	settings_defaults_set(t, "TOOLCHAIN_ROOT", tc_path);
	settings_defaults_set(t, "UNIVERSAL_BINARY", "NO");
	settings_defaults_set(t, "VALID_ARCHS", sdk.default_arch ? sdk.default_arch : "arm64");

	/* iOS-style deployment target if the SDK advertises one. */
	if (sdk.deployment_target != NULL) {
		settings_defaults_set(t, "IOS_DEPLOYMENT_TARGET", sdk.deployment_target);
	} else {
		settings_defaults_set(t, "IOS_DEPLOYMENT_TARGET", deploy);
	}

	target_triple(triple, sizeof(triple), deploy, sdk.default_arch ? sdk.default_arch : "arm64");
	settings_defaults_set(t, "TARGET_TRIPLE", triple);

	if (sdk.toolchain != NULL && tc.name == NULL)
		settings_defaults_set(t, "TOOLCHAINS", sdk.toolchain);

	return 0;
}

/* Merge the string members of a plist dict (as produced from a pbxproj
 * XCBuildConfiguration.buildSettings node) into the settings table. Array and
 * dictionary values are flattened into a comma-separated string. */
/*
 * Merge a build-settings dictionary into the table.
 *
 * An array value becomes its elements joined by spaces, which is how a
 * setting like OTHER_CFLAGS is written on a command line; a value that
 * is neither string nor array is recorded empty rather than dropped, so
 * the setting is still defined.
 */
static void
join_array(CFArrayRef array, char *buf, size_t len)
{
	CFIndex i;

	buf[0] = '\0';

	for (i = 0; i < CFArrayGetCount(array); i++) {
		CFTypeRef v = CFArrayGetValueAtIndex(array, i);
		char one[512];

		if (v == NULL || CFGetTypeID(v) != CFStringGetTypeID())
			continue;
		if (!CFStringGetCString((CFStringRef)v, one, sizeof(one),
		    kCFStringEncodingUTF8))
			continue;

		if (buf[0] != '\0')
			strlcat(buf, " ", len);
		strlcat(buf, one, len);
	}
}

struct merge_ctx {
	settings_table *table;
};

static void
merge_entry(const void *key, const void *value, void *ctx)
{
	settings_table *t = ((struct merge_ctx *)ctx)->table;
	char kbuf[512], vbuf[8192];

	if (key == NULL || CFGetTypeID(key) != CFStringGetTypeID())
		return;
	if (!CFStringGetCString((CFStringRef)key, kbuf, sizeof(kbuf),
	    kCFStringEncodingUTF8))
		return;

	if (value != NULL && CFGetTypeID(value) == CFStringGetTypeID()) {
		char raw[8192];

		if (CFStringGetCString((CFStringRef)value, raw, sizeof(raw),
		    kCFStringEncodingUTF8)) {
			char *expanded = settings_expand(t, raw);

			settings_set(t, kbuf, (expanded != NULL) ? expanded : "");
			free(expanded);
			return;
		}
	} else if (value != NULL && CFGetTypeID(value) == CFArrayGetTypeID()) {
		join_array((CFArrayRef)value, vbuf, sizeof(vbuf));
		settings_set(t, kbuf, vbuf);
		return;
	}

	settings_set(t, kbuf, "");
}

void settings_merge_plist_dict(settings_table *t, CFTypeRef dict)
{
	struct merge_ctx ctx;

	if (dict == NULL || CFGetTypeID(dict) != CFDictionaryGetTypeID())
		return;

	ctx.table = t;
	CFDictionaryApplyFunction((CFDictionaryRef)dict, merge_entry, &ctx);
}

/* ------------------------------------------------------------------ */
/* .xcconfig parsing                                                   */
/* ------------------------------------------------------------------ */

static char *trim(char *s)
{
	while (*s && isspace((unsigned char)*s))
		s++;
	if (*s == '\0')
		return s;
	char *end = s + strlen(s) - 1;
	while (end > s && isspace((unsigned char)*end))
		*end-- = '\0';
	return s;
}

/* Strip // line comments and block comments in place. */
static void strip_comments(char *s)
{
	char *out = s;
	while (*s) {
		if (*s == '/' && *(s + 1) == '/') {
			*s = '\0';
			break;
		}
		if (*s == '/' && *(s + 1) == '*') {
			s += 2;
			while (*s && !(*s == '*' && *(s + 1) == '/'))
				s++;
			if (*s)
				s += 2;
			continue;
		}
		*out++ = *s++;
	}
	*out = '\0';
}

static int dir_for(const char *path, char *out, size_t outsz)
{
	char tmp[PATH_MAX];
	snprintf(tmp, sizeof(tmp), "%s", path);
	char *d = dirname(tmp);
	snprintf(out, outsz, "%s", d);
	return 0;
}

int settings_load_xcconfig(settings_table *t, const char *path)
{
	FILE *fp = fopen(path, "r");
	if (fp == NULL)
		return -1;

	char line[4096];
	char base_dir[PATH_MAX];
	dir_for(path, base_dir, sizeof(base_dir));

	while (fgets(line, sizeof(line), fp) != NULL) {
		strip_comments(line);
		char *p = trim(line);
		if (*p == '\0')
			continue;

		if (strncmp(p, "#include?", 9) == 0) {
			char *arg = p + 9;
			while (*arg && isspace((unsigned char)*arg)) arg++;
			char inc[PATH_MAX];
			snprintf(inc, sizeof(inc), "%s/%s", base_dir, arg);
			if (access(inc, R_OK) == 0)
				settings_load_xcconfig(t, inc);
			continue;
		}
		if (strncmp(p, "#include", 8) == 0) {
			char *arg = p + 8;
			while (*arg && isspace((unsigned char)*arg)) arg++;
			char inc[PATH_MAX];
			snprintf(inc, sizeof(inc), "%s/%s", base_dir, arg);
			settings_load_xcconfig(t, inc);
			continue;
		}

		char *eq = strchr(p, '=');
		if (eq == NULL)
			continue;
		*eq = '\0';
		char *key = trim(p);
		if (*key == '\0')
			continue;
		char *rawval = trim(eq + 1);
		char *expanded = settings_expand(t, rawval);
		if (expanded != NULL) {
			settings_set(t, key, expanded);
			free(expanded);
		}
	}

	fclose(fp);
	return 0;
}

/* ------------------------------------------------------------------ */
/* -showBuildSettings rendering                                         */
/* ------------------------------------------------------------------ */

static void json_escape(FILE *fp, const char *s)
{
	fputc('"', fp);
	for (const char *p = s; *p; p++) {
		unsigned char c = (unsigned char)*p;
		if (c == '"' || c == '\\') {
			fputc('\\', fp);
			fputc(c, fp);
		} else if (c == '\n') {
			fputs("\\n", fp);
		} else if (c == '\r') {
			fputs("\\r", fp);
		} else if (c == '\t') {
			fputs("\\t", fp);
		} else if (c < 0x20) {
			fprintf(fp, "\\u%04x", c);
		} else {
			fputc(c, fp);
		}
	}
	fputc('"', fp);
}

int settings_emit(settings_table *t, int as_json, int pretty)
{
	if (t == NULL)
		return -1;
	if (as_json) {
		const char *nl = pretty ? "\n" : "";
		fprintf(stdout, "[%s", nl);
		for (size_t i = 0; i < t->count; i++) {
			if (pretty)
				fputs("\n\t{\n", stdout);
			else
				fputs("\n  {\n", stdout);
			fputs(pretty ? "\t\t\"name\": " : "    \"name\": ", stdout);
			json_escape(stdout, t->entries[i].key);
			fputs(", ", stdout);
			fputs(pretty ? "\n\t\t\"value\": " : "    \"value\": ", stdout);
			json_escape(stdout, t->entries[i].value);
			fputs(pretty ? "\n\t}" : "  }", stdout);
			if (i + 1 < t->count)
				fputs(",", stdout);
		}
		fprintf(stdout, "%s]%s", nl, nl);
		return 0;
	}

	for (size_t i = 0; i < t->count; i++)
		fprintf(stdout, "    %s = %s\n", t->entries[i].key, t->entries[i].value);
	return 0;
}
