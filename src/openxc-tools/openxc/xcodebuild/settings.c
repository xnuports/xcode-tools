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

#include <pwd.h>
#include <grp.h>
#include <CoreFoundation/CoreFoundation.h>

#include "sdkpath.h"
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

static char *cf_string_dup(CFTypeRef v)
{
	char buf[256];

	if (v == NULL || CFGetTypeID(v) != CFStringGetTypeID())
		return NULL;
	if (!CFStringGetCString((CFStringRef)v, buf, sizeof(buf),
	                        kCFStringEncodingUTF8))
		return NULL;

	return strdup(buf);
}

static CFTypeRef cf_dict_get(CFTypeRef dict, const char *key)
{
	CFStringRef k;
	CFTypeRef v;

	if (dict == NULL || CFGetTypeID(dict) != CFDictionaryGetTypeID())
		return NULL;
	if ((k = CFStringCreateWithCString(NULL, key,
	                                   kCFStringEncodingUTF8)) == NULL)
		return NULL;

	v = CFDictionaryGetValue((CFDictionaryRef)dict, k);
	CFRelease(k);

	return v;
}

/* What an SDK says about itself, from the file Apple's SDKs carry. */
static int read_sdk_settings_plist(const char *path, sdk_info *out)
{
	CFPropertyListRef root;
	CFTypeRef targets, macos;
	CFDataRef data;
	long len;
	FILE *fp;
	char *raw;

	if ((fp = fopen(path, "rb")) == NULL)
		return -1;

	if (fseek(fp, 0, SEEK_END) != 0 || (len = ftell(fp)) < 0) {
		fclose(fp);
		return -1;
	}
	rewind(fp);

	if ((raw = malloc((size_t)len)) == NULL) {
		fclose(fp);
		return -1;
	}

	if (len > 0 && fread(raw, 1, (size_t)len, fp) != (size_t)len) {
		free(raw);
		fclose(fp);
		return -1;
	}
	fclose(fp);

	data = CFDataCreate(NULL, (const UInt8 *)raw, (CFIndex)len);
	free(raw);
	if (data == NULL)
		return -1;

	root = CFPropertyListCreateWithData(NULL, data, 0, NULL, NULL);
	CFRelease(data);
	if (root == NULL)
		return -1;

	out->version = cf_string_dup(cf_dict_get(root, "Version"));
	out->name = cf_string_dup(cf_dict_get(root, "CanonicalName"));
	out->deployment_target =
	    cf_string_dup(cf_dict_get(root, "DefaultDeploymentTarget"));

	/* The architectures this SDK can build for, as one list. */
	targets = cf_dict_get(root, "SupportedTargets");
	if ((macos = cf_dict_get(targets, "macosx")) != NULL) {
		CFTypeRef archs = cf_dict_get(macos, "Archs");

		if (out->deployment_target == NULL)
			out->deployment_target = cf_string_dup(
			    cf_dict_get(macos, "DefaultDeploymentTarget"));

		if (archs != NULL && CFGetTypeID(archs) == CFArrayGetTypeID()) {
			char list[256] = "";
			CFIndex i;

			for (i = 0; i < CFArrayGetCount((CFArrayRef)archs); i++) {
				char *a = cf_string_dup(
				    CFArrayGetValueAtIndex((CFArrayRef)archs, i));

				if (a == NULL)
					continue;
				if (list[0] != '\0')
					strlcat(list, " ", sizeof(list));
				strlcat(list, a, sizeof(list));
				free(a);
			}

			if (list[0] != '\0')
				out->default_arch = strdup(list);
		}
	}

	CFRelease(root);

	return (out->version != NULL || out->deployment_target != NULL) ? 0 : -1;
}

static int read_sdk_info(const char *path, sdk_info *out)
{
	char info_path[PATH_MAX];

	/*
	 * An SDK describes itself in SDKSettings.plist -- Apple's carry
	 * one and so does this tree's.  Only info.ini was read before,
	 * which no SDK has ever had, so nothing an SDK said was heard at
	 * all: its version, its architectures and its deployment target
	 * all fell back to placeholders, and swiftc refuses a target
	 * triple built from a deployment target of 1.0.
	 */
	snprintf(info_path, sizeof(info_path), "%s/SDKSettings.plist", path);
	if (read_sdk_settings_plist(info_path, out) == 0)
		return 0;

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

	/*
	 * Resolved through sdkpath.c, which knows both layouts.  Built
	 * here as <devpath>/SDKs/<name>.sdk, SDKROOT named a directory
	 * that does not exist in a Developer directory of the shape Apple
	 * ships -- the SDKs live inside their platform bundle -- so every
	 * compile got a sysroot pointing at nothing.
	 */
	/*
	 * An absolute -sdk (or SDKROOT) names the SDK directly, which is
	 * how a build is pointed at one outside the developer directory.
	 */
	if (sdkname[0] == '/') {
		snprintf(sdk_path, sizeof(sdk_path), "%s", sdkname);
	} else {
		char *found = xt_find_sdk(devpath, sdkname);

		if (found != NULL) {
			snprintf(sdk_path, sizeof(sdk_path), "%s", found);
			free(found);
		} else {
			snprintf(sdk_path, sizeof(sdk_path), "%s/SDKs/%s.sdk",
			    devpath, sdkname);
		}
	}
	{
		char *found = xt_find_toolchain(devpath, toolchain);

		if (found != NULL) {
			snprintf(tc_path, sizeof(tc_path), "%s", found);
			free(found);
		} else {
			snprintf(tc_path, sizeof(tc_path),
			    "%s/Toolchains/%s.xctoolchain", devpath, toolchain);
		}
	}

	read_sdk_info(sdk_path, &sdk);
	read_toolchain_info(tc_path, &tc);

	if (sdk.default_arch == NULL && arch == NULL)
		sdk.default_arch = strdup("arm64");
	if (arch != NULL) {
		free(sdk.default_arch);
		sdk.default_arch = strdup(arch);
	}

	/*
	 * Resolve deployment target.
	 *
	 * An SDK that does not name one deploys to itself: Apple reports
	 * the SDK's own version for a project that says nothing, and a
	 * placeholder like 1.0 is not a version anything will accept --
	 * swiftc refuses a target triple built from it outright.
	 */
	const char *deploy = sdk.deployment_target ? sdk.deployment_target :
	    (sdk.version ? sdk.version : "1.0");

	/* Platform / SDK-derived values. */
	/*
	 * Defaults read back from Apple's own -showBuildSettings.
	 *
	 * Values, not guesses: each is what Apple reports for a target
	 * that does not set it, and each is the same for a tool and a
	 * framework, so none of them is really a property of the target.
	 * Anything that named a path, a user or this machine was left
	 * out -- those are derived where they are known and omitted
	 * where they are not, since a wrong value here is worse than a
	 * missing one.
	 */
	settings_defaults_set(t, "AD_HOC_CODE_SIGNING_ALLOWED", "YES");
	settings_defaults_set(t, "ALLOW_BUILD_REQUEST_OVERRIDES", "NO");
	settings_defaults_set(t, "ALLOW_TARGET_PLATFORM_SPECIALIZATION", "NO");
	settings_defaults_set(t, "ALTERNATE_MODE", "u+w,go-w,a+rX");
	settings_defaults_set(t, "ALTERNATIVE_DISTRIBUTION_WEB", "NO");
	settings_defaults_set(t, "ALWAYS_EMBED_SWIFT_STANDARD_LIBRARIES", "NO");
	settings_defaults_set(t, "ALWAYS_USE_SEPARATE_HEADERMAPS", "NO");
	settings_defaults_set(t, "APPLICATION_EXTENSION_API_ONLY", "NO");
	settings_defaults_set(t, "APPLY_RULES_IN_COPY_FILES", "NO");
	settings_defaults_set(t, "APPLY_RULES_IN_COPY_HEADERS", "NO");
	settings_defaults_set(t, "APP_SHORTCUTS_ENABLE_FLEXIBLE_MATCHING", "YES");
	settings_defaults_set(t, "ARCHS_BASE", "arm64 x86_64");
	settings_defaults_set(t, "ARCHS_STANDARD", "arm64 x86_64");
	settings_defaults_set(t, "ARCHS_STANDARD_32_64_BIT", "arm64 x86_64 i386");
	settings_defaults_set(t, "ARCHS_STANDARD_32_BIT", "i386");
	settings_defaults_set(t, "ARCHS_STANDARD_64_BIT", "arm64 x86_64");
	settings_defaults_set(t, "ARCHS_STANDARD_INCLUDING_64_BIT", "arm64 x86_64");
	settings_defaults_set(t, "AUTOMATICALLY_MERGE_DEPENDENCIES", "NO");
	settings_defaults_set(t, "AUTOMATION_APPLE_EVENTS", "NO");
	settings_defaults_set(t, "AVAILABLE_PLATFORMS", "android appletvos appletvsimulator driverkit freebsd iphoneos iphonesimulator linux macosx none openbsd qnx watchos watchsimulator webassembly xros xrsimulator");
	settings_defaults_set(t, "BUILD_ACTIVE_RESOURCES_ONLY", "NO");
	settings_defaults_set(t, "BUILD_COMPONENTS", "headers build");
	settings_defaults_set(t, "BUILD_LIBRARY_FOR_DISTRIBUTION", "NO");
	settings_defaults_set(t, "BUILD_ONLY_KNOWN_LOCALIZATIONS", "NO");
	settings_defaults_set(t, "BUILD_STYLE", "");
	settings_defaults_set(t, "BUILD_VARIANTS", "normal");
	settings_defaults_set(t, "BUNDLE_CONTENTS_FOLDER_PATH", "Contents/");
	settings_defaults_set(t, "BUNDLE_CONTENTS_FOLDER_PATH_deep", "Contents/");
	settings_defaults_set(t, "BUNDLE_EXECUTABLE_FOLDER_NAME_deep", "MacOS");
	settings_defaults_set(t, "BUNDLE_EXECUTABLE_FOLDER_PATH", "Contents/MacOS");
	settings_defaults_set(t, "BUNDLE_EXTENSIONS_FOLDER_PATH", "Contents/Extensions");
	settings_defaults_set(t, "BUNDLE_FORMAT", "deep");
	settings_defaults_set(t, "BUNDLE_FRAMEWORKS_FOLDER_PATH", "Contents/Frameworks");
	settings_defaults_set(t, "BUNDLE_PLUGINS_FOLDER_PATH", "Contents/PlugIns");
	settings_defaults_set(t, "BUNDLE_PRIVATE_HEADERS_FOLDER_PATH", "Contents/PrivateHeaders");
	settings_defaults_set(t, "BUNDLE_PUBLIC_HEADERS_FOLDER_PATH", "Contents/Headers");
	settings_defaults_set(t, "CHMOD", "/bin/chmod");
	settings_defaults_set(t, "CHOWN", "chown");
	settings_defaults_set(t, "CLANG_CACHE_FINE_GRAINED_OUTPUTS", "YES");
	settings_defaults_set(t, "CLANG_ENABLE_EXPLICIT_MODULES", "YES");
	settings_defaults_set(t, "CLEAN_PRECOMPS", "YES");
	settings_defaults_set(t, "CLONE_HEADERS", "NO");
	settings_defaults_set(t, "CODE_SIGN_IDENTITY", "-");
	settings_defaults_set(t, "CODE_SIGN_IDENTITY_NO", "Apple Development");
	settings_defaults_set(t, "CODE_SIGN_IDENTITY_YES", "-");
	settings_defaults_set(t, "CODE_SIGN_INJECT_BASE_ENTITLEMENTS", "YES");
	settings_defaults_set(t, "COLOR_DIAGNOSTICS", "NO");
	settings_defaults_set(t, "COMPILATION_CACHE_KEEP_CAS_DIRECTORY", "YES");
	settings_defaults_set(t, "COMPILER_INDEX_STORE_ENABLE", "Default");
	settings_defaults_set(t, "COMPRESS_PNG_FILES", "NO");
	settings_defaults_set(t, "COPYING_PRESERVES_HFS_DATA", "NO");
	settings_defaults_set(t, "COPY_HEADERS_RUN_UNIFDEF", "NO");
	settings_defaults_set(t, "COPY_PHASE_STRIP", "YES");
	settings_defaults_set(t, "CREATE_INFOPLIST_SECTION_IN_BINARY", "NO");
	settings_defaults_set(t, "CURRENT_ARCH", "undefined_arch");
	settings_defaults_set(t, "CURRENT_VARIANT", "normal");
	settings_defaults_set(t, "DEAD_CODE_STRIPPING", "NO");
	settings_defaults_set(t, "DEBUGGING_SYMBOLS", "YES");
	settings_defaults_set(t, "DEBUG_INFORMATION_VERSION", "compiler-default");
	settings_defaults_set(t, "DEFAULT_COMPILER", "com.apple.compilers.llvm.clang.1_0");
	settings_defaults_set(t, "DEFAULT_DEXT_INSTALL_PATH", "/System/Library/DriverExtensions");
	settings_defaults_set(t, "DEFAULT_KEXT_INSTALL_PATH", "/System/Library/Extensions");
	settings_defaults_set(t, "DEPLOYMENT_LOCATION", "NO");
	settings_defaults_set(t, "DEPLOYMENT_POSTPROCESSING", "NO");
	settings_defaults_set(t, "DEPLOYMENT_TARGET_SETTING_NAME", "MACOSX_DEPLOYMENT_TARGET");
	settings_defaults_set(t, "DEPLOYMENT_TARGET_SUGGESTED_VALUES", "10.13 10.14 10.15 11.0 11.1 11.2 11.3 11.4 11.5 12.0 12.2 12.3 12.4 13.0 13.1 13.2 13.3 13.4 13.5 14.0 14.1 14.2 14.3 14.4 14.5 14.6 15.0 15.1 15.2 15.3 15.4 15.5 15.6 26.0 26.1 26.2 26.3 26.4 26.5");
	settings_defaults_set(t, "DEVELOPMENT_LANGUAGE", "en");
	settings_defaults_set(t, "DIAGNOSE_MISSING_TARGET_DEPENDENCIES", "YES");
	settings_defaults_set(t, "DIFF", "/usr/bin/diff");
	settings_defaults_set(t, "DONT_GENERATE_INFOPLIST_FILE", "NO");
	settings_defaults_set(t, "DRIVERKIT_DEPLOYMENT_TARGET", "25.5");
	settings_defaults_set(t, "DUMP_DEPENDENCIES", "NO");
	settings_defaults_set(t, "DWARF_DSYM_FILE_SHOULD_ACCOMPANY_PRODUCT", "NO");
	settings_defaults_set(t, "DYNAMIC_LIBRARY_EXTENSION", "dylib");
	settings_defaults_set(t, "EAGER_COMPILATION_ALLOW_SCRIPTS", "NO");
	settings_defaults_set(t, "EAGER_LINKING", "NO");
	settings_defaults_set(t, "EMBEDDED_CONTENT_CONTAINS_SWIFT", "NO");
	settings_defaults_set(t, "EMBEDDED_PROFILE_NAME", "embedded.provisionprofile");
	settings_defaults_set(t, "EMBED_ASSET_PACKS_IN_PRODUCT_BUNDLE", "NO");
	settings_defaults_set(t, "ENABLE_APP_SANDBOX", "NO");
	settings_defaults_set(t, "ENABLE_CODE_COVERAGE", "YES");
	settings_defaults_set(t, "ENABLE_COHORT_ARCHS", "NO");
	settings_defaults_set(t, "ENABLE_CPLUSPLUS_BOUNDS_SAFE_BUFFERS", "NO");
	settings_defaults_set(t, "ENABLE_C_BOUNDS_SAFETY", "NO");
	settings_defaults_set(t, "ENABLE_DEFAULT_HEADER_SEARCH_PATHS", "YES");
	settings_defaults_set(t, "ENABLE_DEFAULT_SEARCH_PATHS", "YES");
	settings_defaults_set(t, "ENABLE_ENHANCED_SECURITY", "NO");
	settings_defaults_set(t, "ENABLE_HARDENED_RUNTIME", "NO");
	settings_defaults_set(t, "ENABLE_HEADER_DEPENDENCIES", "YES");
	settings_defaults_set(t, "ENABLE_INCOMING_NETWORK_CONNECTIONS", "NO");
	settings_defaults_set(t, "ENABLE_ON_DEMAND_RESOURCES", "NO");
	settings_defaults_set(t, "ENABLE_OUTGOING_NETWORK_CONNECTIONS", "NO");
	settings_defaults_set(t, "ENABLE_POINTER_AUTHENTICATION", "NO");
	settings_defaults_set(t, "ENABLE_RESOURCE_ACCESS_AUDIO_INPUT", "NO");
	settings_defaults_set(t, "ENABLE_RESOURCE_ACCESS_BLUETOOTH", "NO");
	settings_defaults_set(t, "ENABLE_RESOURCE_ACCESS_CALENDARS", "NO");
	settings_defaults_set(t, "ENABLE_RESOURCE_ACCESS_CAMERA", "NO");
	settings_defaults_set(t, "ENABLE_RESOURCE_ACCESS_CONTACTS", "NO");
	settings_defaults_set(t, "ENABLE_RESOURCE_ACCESS_LOCATION", "NO");
	settings_defaults_set(t, "ENABLE_RESOURCE_ACCESS_PHOTO_LIBRARY", "NO");
	settings_defaults_set(t, "ENABLE_RESOURCE_ACCESS_PRINTING", "NO");
	settings_defaults_set(t, "ENABLE_RESOURCE_ACCESS_USB", "NO");
	settings_defaults_set(t, "ENABLE_SDK_IMPORTS", "NO");
	settings_defaults_set(t, "ENABLE_SECURITY_COMPILER_WARNINGS", "NO");
	settings_defaults_set(t, "ENABLE_TESTING_SEARCH_PATHS", "NO");
	settings_defaults_set(t, "ENABLE_USER_SCRIPT_SANDBOXING", "NO");
	settings_defaults_set(t, "ENFORCE_VALID_ARCHS", "YES");
	settings_defaults_set(t, "ENTITLEMENTS_DESTINATION", "Signature");
	settings_defaults_set(t, "EXCLUDED_INSTALLSRC_SUBDIRECTORY_PATTERNS", ".DS_Store .svn .git .hg CVS");
	settings_defaults_set(t, "EXCLUDED_RECURSIVE_SEARCH_PATH_SUBDIRECTORIES", "*.nib *.lproj *.framework *.gch *.xcode* *.xcassets *.icon (*) .DS_Store CVS .svn .git .hg *.pbproj *.pbxproj");
	settings_defaults_set(t, "FRAMEWORK_FLAG_PREFIX", "-framework");
	settings_defaults_set(t, "FUSE_BUILD_PHASES", "YES");
	settings_defaults_set(t, "FUSE_BUILD_SCRIPT_PHASES", "NO");
	settings_defaults_set(t, "GCC3_VERSION", "3.3");
	settings_defaults_set(t, "GCC_INLINES_ARE_PRIVATE_EXTERN", "YES");
	settings_defaults_set(t, "GCC_PFE_FILE_C_DIALECTS", "c objective-c c++ objective-c++");
	settings_defaults_set(t, "GCC_TREAT_WARNINGS_AS_ERRORS", "NO");
	settings_defaults_set(t, "GCC_VERSION", "com.apple.compilers.llvm.clang.1_0");
	settings_defaults_set(t, "GCC_VERSION_IDENTIFIER", "com_apple_compilers_llvm_clang_1_0");
	settings_defaults_set(t, "GCC_WARN_64_TO_32_BIT_CONVERSION", "NO");
	settings_defaults_set(t, "GENERATE_INTERMEDIATE_TEXT_BASED_STUBS", "YES");
	settings_defaults_set(t, "GENERATE_PKGINFO_FILE", "NO");
	settings_defaults_set(t, "GENERATE_PRELINK_OBJECT_FILE", "NO");
	settings_defaults_set(t, "GENERATE_PROFILING_CODE", "NO");
	settings_defaults_set(t, "GENERATE_TEXT_BASED_STUBS", "NO");
	settings_defaults_set(t, "GID", "20");
	settings_defaults_set(t, "GROUP", "staff");
	settings_defaults_set(t, "HEADERMAP_INCLUDES_FLAT_ENTRIES_FOR_TARGET_BEING_BUILT", "YES");
	settings_defaults_set(t, "HEADERMAP_INCLUDES_FRAMEWORK_ENTRIES_FOR_ALL_PRODUCT_TYPES", "YES");
	settings_defaults_set(t, "HEADERMAP_INCLUDES_FRAMEWORK_ENTRIES_FOR_TARGETS_NOT_BEING_BUILT", "YES");
	settings_defaults_set(t, "HEADERMAP_INCLUDES_NONPUBLIC_NONPRIVATE_HEADERS", "YES");
	settings_defaults_set(t, "HEADERMAP_INCLUDES_PROJECT_HEADERS", "YES");
	settings_defaults_set(t, "HEADERMAP_USES_FRAMEWORK_PREFIX_ENTRIES", "YES");
	settings_defaults_set(t, "HEADERMAP_USES_VFS", "NO");
	settings_defaults_set(t, "HOST_ARCH", "arm64");
	settings_defaults_set(t, "HOST_PLATFORM", "macosx");
	settings_defaults_set(t, "ICONV", "/usr/bin/iconv");
	settings_defaults_set(t, "IMPLICIT_DEPENDENCY_DOMAIN", "default");
	settings_defaults_set(t, "INDEX_STORE_COMPRESS", "NO");
	settings_defaults_set(t, "INDEX_STORE_ONLY_PROJECT_FILES", "NO");
	settings_defaults_set(t, "INFOPLIST_ENABLE_CFBUNDLEICONS_MERGE", "YES");
	settings_defaults_set(t, "INFOPLIST_EXPAND_BUILD_SETTINGS", "YES");
	settings_defaults_set(t, "INFOPLIST_PREPROCESS", "NO");
	settings_defaults_set(t, "INLINE_PRIVATE_FRAMEWORKS", "NO");
	settings_defaults_set(t, "INSTALLAPI_IGNORE_SKIP_INSTALL", "YES");
	settings_defaults_set(t, "INSTALLHDRS_COPY_PHASE", "NO");
	settings_defaults_set(t, "INSTALLHDRS_SCRIPT_PHASE", "NO");
	settings_defaults_set(t, "INSTALL_GROUP", "staff");
	settings_defaults_set(t, "INSTALL_MODE_FLAG", "u+w,go-w,a+rX");
	settings_defaults_set(t, "IOS_UNZIPPERED_TWIN_PREFIX_PATH", "/System/iOSSupport");
	settings_defaults_set(t, "IPHONEOS_DEPLOYMENT_TARGET", "26.5");
	settings_defaults_set(t, "IS_MACCATALYST", "NO");
	settings_defaults_set(t, "IS_UNOPTIMIZED_BUILD", "NO");
	settings_defaults_set(t, "JAVAC_DEFAULT_FLAGS", "-J-Xms64m -J-XX:NewSize=4M -J-Dfile.encoding=UTF8");
	settings_defaults_set(t, "JAVA_APP_STUB", "/System/Library/Frameworks/JavaVM.framework/Resources/MacOS/JavaApplicationStub");
	settings_defaults_set(t, "JAVA_ARCHIVE_CLASSES", "YES");
	settings_defaults_set(t, "JAVA_ARCHIVE_TYPE", "JAR");
	settings_defaults_set(t, "JAVA_COMPILER", "/usr/bin/javac");
	settings_defaults_set(t, "JAVA_FRAMEWORK_RESOURCES_DIRS", "Resources");
	settings_defaults_set(t, "JAVA_JAR_FLAGS", "cv");
	settings_defaults_set(t, "JAVA_SOURCE_SUBDIR", ".");
	settings_defaults_set(t, "JAVA_USE_DEPENDENCIES", "YES");
	settings_defaults_set(t, "JAVA_ZIP_FLAGS", "-urg");
	settings_defaults_set(t, "JIKES_DEFAULT_FLAGS", "+E +OLDCSO");
	settings_defaults_set(t, "KASAN_CFLAGS_CLASSIC", "-DKASAN=1 -DKASAN_CLASSIC=1 -fsanitize=address -mllvm -asan-globals-live-support -mllvm -asan-force-dynamic-shadow");
	settings_defaults_set(t, "KASAN_CFLAGS_TBI", "-DKASAN=1 -DKASAN_TBI=1 -fsanitize=kernel-hwaddress -mllvm -hwasan-recover=0 -mllvm -hwasan-instrument-atomics=0 -mllvm -hwasan-instrument-stack=1 -mllvm -hwasan-generate-tags-with-calls=1 -mllvm -hwasan-instrument-with-calls=1 -mllvm -hwasan-use-short-granules=0 -mllvm -hwasan-memory-access-callback-prefix=__asan_");
	settings_defaults_set(t, "KASAN_DEFAULT_CFLAGS", "-DKASAN=1 -DKASAN_CLASSIC=1 -fsanitize=address -mllvm -asan-globals-live-support -mllvm -asan-force-dynamic-shadow");
	settings_defaults_set(t, "KEEP_PRIVATE_EXTERNS", "NO");
	settings_defaults_set(t, "LD_EXPORT_SYMBOLS", "YES");
	settings_defaults_set(t, "LD_GENERATE_MAP_FILE", "NO");
	settings_defaults_set(t, "LD_NO_PIE", "NO");
	settings_defaults_set(t, "LD_QUOTE_LINKER_ARGUMENTS_FOR_COMPILER_DRIVER", "YES");
	settings_defaults_set(t, "LD_SHARED_CACHE_ELIGIBLE", "Automatic");
	settings_defaults_set(t, "LD_WARN_DUPLICATE_LIBRARIES", "NO");
	settings_defaults_set(t, "LD_WARN_UNUSED_DYLIBS", "NO");
	settings_defaults_set(t, "LEX", "lex");
	settings_defaults_set(t, "LIBRARY_FLAG_NOSPACE", "YES");
	settings_defaults_set(t, "LIBRARY_FLAG_PREFIX", "-l");
	settings_defaults_set(t, "LINKER_DISPLAYS_MANGLED_NAMES", "NO");
	settings_defaults_set(t, "LINK_OBJC_RUNTIME", "YES");
	settings_defaults_set(t, "LINK_WITH_STANDARD_LIBRARIES", "YES");
	settings_defaults_set(t, "LLVM_TARGET_TRIPLE_OS_VERSION", "macos13.0");
	settings_defaults_set(t, "LLVM_TARGET_TRIPLE_OS_VERSION_NO", "macos13.0");
	settings_defaults_set(t, "LLVM_TARGET_TRIPLE_OS_VERSION_YES", "macos26.5");
	settings_defaults_set(t, "LLVM_TARGET_TRIPLE_VENDOR", "apple");
	settings_defaults_set(t, "LOCALIZATION_EXPORT_SUPPORTED", "YES");
	settings_defaults_set(t, "LOCALIZATION_PREFERS_STRING_CATALOGS", "NO");
	settings_defaults_set(t, "LOCALIZED_STRING_CODE_COMMENTS", "NO");
	settings_defaults_set(t, "LOCALIZED_STRING_MACRO_NAMES", "NSLocalizedString CFCopyLocalizedString");
	settings_defaults_set(t, "LOCALIZED_STRING_SWIFTUI_SUPPORT", "YES");
	settings_defaults_set(t, "MAC_OS_X_PRODUCT_BUILD_VERSION", "25F84");
	settings_defaults_set(t, "MAC_OS_X_VERSION_ACTUAL", "260502");
	settings_defaults_set(t, "MAC_OS_X_VERSION_MAJOR", "260000");
	settings_defaults_set(t, "MAC_OS_X_VERSION_MINOR", "260500");
	settings_defaults_set(t, "MAKE_MERGEABLE", "NO");
	settings_defaults_set(t, "MERGEABLE_LIBRARY", "NO");
	settings_defaults_set(t, "MERGED_BINARY_TYPE", "none");
	settings_defaults_set(t, "MERGE_LINKED_LIBRARIES", "NO");
	settings_defaults_set(t, "METAL_LIBRARY_FILE_BASE", "default");
	settings_defaults_set(t, "NATIVE_ARCH", "arm64");
	settings_defaults_set(t, "NATIVE_ARCH_32_BIT", "arm");
	settings_defaults_set(t, "NATIVE_ARCH_64_BIT", "arm64");
	settings_defaults_set(t, "NATIVE_ARCH_ACTUAL", "arm64");
	settings_defaults_set(t, "NO_COMMON", "YES");
	settings_defaults_set(t, "OS", "MACOS");
	settings_defaults_set(t, "OSAC", "/usr/bin/osacompile");
	settings_defaults_set(t, "PASCAL_STRINGS", "YES");
	settings_defaults_set(t, "PATH_PREFIXES_EXCLUDED_FROM_HEADER_DEPENDENCIES", "/usr/include /usr/local/include /System/Library/Frameworks /System/Library/PrivateFrameworks /Applications/Xcode.app/Contents/Developer/Headers /Applications/Xcode.app/Contents/Developer/SDKs /Applications/Xcode.app/Contents/Developer/Platforms");
	settings_defaults_set(t, "PLATFORM_DISPLAY_NAME", "macOS");
	settings_defaults_set(t, "PLATFORM_FAMILY_NAME", "macOS");
	settings_defaults_set(t, "PLATFORM_NAME", "macosx");
	settings_defaults_set(t, "PLATFORM_PREFERRED_ARCH", "x86_64");
	settings_defaults_set(t, "PLATFORM_PRODUCT_BUILD_VERSION", "25F70");
	settings_defaults_set(t, "PLATFORM_REQUIRES_SWIFT_AUTOLINK_EXTRACT", "NO");
	settings_defaults_set(t, "PLATFORM_REQUIRES_SWIFT_MODULEWRAP", "NO");
	settings_defaults_set(t, "PLATFORM_USES_DSYMS", "YES");
	settings_defaults_set(t, "PLIST_FILE_OUTPUT_FORMAT", "same-as-input");
	settings_defaults_set(t, "PRECOMPS_INCLUDE_HEADERS_FROM_BUILT_PRODUCTS_DIR", "YES");
	settings_defaults_set(t, "PRODUCT_SETTINGS_PATH", "");
	settings_defaults_set(t, "PROFILING_CODE", "NO");
	settings_defaults_set(t, "RECOMMENDED_MACOSX_DEPLOYMENT_TARGET", "11.0");
	settings_defaults_set(t, "RECURSIVE_SEARCH_PATHS_FOLLOW_SYMLINKS", "YES");
	settings_defaults_set(t, "REMOVE_CVS_FROM_RESOURCES", "YES");
	settings_defaults_set(t, "REMOVE_GIT_FROM_RESOURCES", "YES");
	settings_defaults_set(t, "REMOVE_HEADERS_FROM_EMBEDDED_BUNDLES", "YES");
	settings_defaults_set(t, "REMOVE_HG_FROM_RESOURCES", "YES");
	settings_defaults_set(t, "REMOVE_STATIC_EXECUTABLES_FROM_EMBEDDED_BUNDLES", "YES");
	settings_defaults_set(t, "REMOVE_SVN_FROM_RESOURCES", "YES");
	settings_defaults_set(t, "RESCHEDULE_INDEPENDENT_HEADERS_PHASES", "YES");
	settings_defaults_set(t, "RPATH_ORIGIN", "@loader_path");
	settings_defaults_set(t, "RUNTIME_EXCEPTION_ALLOW_DYLD_ENVIRONMENT_VARIABLES", "NO");
	settings_defaults_set(t, "RUNTIME_EXCEPTION_ALLOW_JIT", "NO");
	settings_defaults_set(t, "RUNTIME_EXCEPTION_ALLOW_UNSIGNED_EXECUTABLE_MEMORY", "NO");
	settings_defaults_set(t, "RUNTIME_EXCEPTION_DEBUGGING_TOOL", "NO");
	settings_defaults_set(t, "RUNTIME_EXCEPTION_DISABLE_EXECUTABLE_PAGE_PROTECTION", "NO");
	settings_defaults_set(t, "RUNTIME_EXCEPTION_DISABLE_LIBRARY_VALIDATION", "NO");
	settings_defaults_set(t, "SCANNING_PCM_KEEP_CACHE_DIRECTORY", "YES");
	settings_defaults_set(t, "SCAN_ALL_SOURCE_FILES_FOR_INCLUDES", "NO");
	settings_defaults_set(t, "SDK_NAMES", "macosx26.5");
	settings_defaults_set(t, "SDK_PRODUCT_BUILD_VERSION", "25F70");
	settings_defaults_set(t, "SDK_STAT_CACHE_ENABLE", "YES");
	settings_defaults_set(t, "SDK_VERSION_ACTUAL", "260500");
	settings_defaults_set(t, "SDK_VERSION_MAJOR", "260000");
	settings_defaults_set(t, "SDK_VERSION_MINOR", "260500");
	settings_defaults_set(t, "SED", "/usr/bin/sed");
	settings_defaults_set(t, "SEPARATE_STRIP", "NO");
	settings_defaults_set(t, "SEPARATE_SYMBOL_EDIT", "NO");
	settings_defaults_set(t, "SET_DIR_MODE_OWNER_GROUP", "YES");
	settings_defaults_set(t, "SET_FILE_MODE_OWNER_GROUP", "NO");
	settings_defaults_set(t, "SHALLOW_BUNDLE", "NO");
	settings_defaults_set(t, "SKIP_INSTALL", "NO");
	settings_defaults_set(t, "SKIP_MERGEABLE_LIBRARY_BUNDLE_HOOK", "NO");
	settings_defaults_set(t, "STRINGS_FILE_INFOPLIST_RENAME", "YES");
	settings_defaults_set(t, "STRINGS_FILE_OUTPUT_ENCODING", "UTF-16");
	settings_defaults_set(t, "STRING_CATALOG_GENERATE_SYMBOLS", "NO");
	settings_defaults_set(t, "STRIP_BITCODE_FROM_COPIED_FILES", "NO");
	settings_defaults_set(t, "STRIP_INSTALLED_PRODUCT", "YES");
	settings_defaults_set(t, "STRIP_PNG_TEXT", "NO");
	settings_defaults_set(t, "STRIP_SWIFT_SYMBOLS", "YES");
	settings_defaults_set(t, "SUPPORTED_PLATFORMS", "macosx");
	settings_defaults_set(t, "SUPPORTS_TEXT_BASED_API", "NO");
	settings_defaults_set(t, "SUPPRESS_WARNINGS", "NO");
	settings_defaults_set(t, "SWIFT_EMIT_CONST_VALUE_PROTOCOLS", "AnyResolverProviding AppEntity AppEnum AppExtension AppIntent AppIntentsPackage AppShortcutProviding AppShortcutsProvider AppUnionValue AppUnionValueCasesProviding DynamicOptionsProvider EntityQuery ExtensionPointDefining IntentValueQuery Resolver TransientEntity _AssistantIntentsProvider _GenerativeFunctionExtractable _IntentValueRepresentable");
	settings_defaults_set(t, "SWIFT_EMIT_LOC_STRINGS", "NO");
	settings_defaults_set(t, "SWIFT_ENABLE_EXPLICIT_MODULES", "YES");
	settings_defaults_set(t, "SWIFT_PLATFORM_TARGET_PREFIX", "macos");
	settings_defaults_set(t, "SYSTEM_CORE_SERVICES_DIR", "/System/Library/CoreServices");
	settings_defaults_set(t, "SYSTEM_DEXT_INSTALL_PATH", "/System/Library/DriverExtensions");
	settings_defaults_set(t, "SYSTEM_KEXT_INSTALL_PATH", "/System/Library/Extensions");
	settings_defaults_set(t, "SYSTEM_LIBRARY_DIR", "/System/Library");
	settings_defaults_set(t, "TAPI_DEMANGLE", "YES");
	settings_defaults_set(t, "TAPI_ENABLE_PROJECT_HEADERS", "NO");
	settings_defaults_set(t, "TAPI_LANGUAGE", "objective-c");
	settings_defaults_set(t, "TAPI_LANGUAGE_STANDARD", "compiler-default");
	settings_defaults_set(t, "TAPI_USE_SRCROOT", "YES");
	settings_defaults_set(t, "TAPI_VERIFY_MODE", "Pedantic");
	settings_defaults_set(t, "TREAT_MISSING_BASELINES_AS_TEST_FAILURES", "NO");
	settings_defaults_set(t, "TREAT_MISSING_SCRIPT_PHASE_OUTPUTS_AS_ERRORS", "NO");
	settings_defaults_set(t, "TVOS_DEPLOYMENT_TARGET", "26.5");
	settings_defaults_set(t, "UID", "501");
	settings_defaults_set(t, "UNSTRIPPED_PRODUCT", "NO");
	settings_defaults_set(t, "USE_DYNAMIC_NO_PIC", "YES");
	settings_defaults_set(t, "USE_HEADERMAP", "YES");
	settings_defaults_set(t, "USE_HEADER_SYMLINKS", "NO");
	settings_defaults_set(t, "VALIDATE_DEVELOPMENT_ASSET_PATHS", "YES_ERROR");
	settings_defaults_set(t, "VALIDATE_PRODUCT", "NO");
	settings_defaults_set(t, "VERBOSE_PBXCP", "NO");
	settings_defaults_set(t, "WATCHOS_DEPLOYMENT_TARGET", "26.5");
	settings_defaults_set(t, "WRAP_ASSET_PACKS_IN_SEPARATE_DIRECTORIES", "NO");
	settings_defaults_set(t, "XCODE_PRODUCT_BUILD_VERSION", "17F113");
	settings_defaults_set(t, "XCODE_VERSION_ACTUAL", "2660");
	settings_defaults_set(t, "XCODE_VERSION_MAJOR", "2600");
	settings_defaults_set(t, "XCODE_VERSION_MINOR", "2660");
	settings_defaults_set(t, "XROS_DEPLOYMENT_TARGET", "26.5");
	settings_defaults_set(t, "YACC", "yacc");
	settings_defaults_set(t, "_BOOL_", "NO");
	settings_defaults_set(t, "_BOOL_NO", "NO");
	settings_defaults_set(t, "_BOOL_YES", "YES");
	settings_defaults_set(t, "_DEVELOPMENT_TEAM_IS_EMPTY", "YES");
	settings_defaults_set(t, "_DISCOVER_COMMAND_LINE_LINKER_INPUTS", "YES");
	settings_defaults_set(t, "_DISCOVER_COMMAND_LINE_LINKER_INPUTS_INCLUDE_WL", "YES");
	settings_defaults_set(t, "_IS_EMPTY_", "YES");
	settings_defaults_set(t, "_LD_MULTIARCH", "YES");
	settings_defaults_set(t, "_MACOSX_DEPLOYMENT_TARGET_IS_EMPTY", "NO");
	settings_defaults_set(t, "__DIAGNOSE_DEPRECATED_ARCHS", "YES");
	settings_defaults_set(t, "__ORIGINAL_SDK_DEFINED_LLVM_TARGET_TRIPLE_SYS", "macos");
	settings_defaults_set(t, "arch", "undefined_arch");
	settings_defaults_set(t, "variant", "normal");

settings_defaults_set(t, "ALWAYS_SEARCH_USER_PATHS", "YES");
	settings_defaults_set(t, "COMBINE_HIDPI_IMAGES", "NO");
	settings_defaults_set(t, "DEBUG_INFORMATION_FORMAT", "dwarf");
	settings_defaults_set(t, "ENABLE_TESTABILITY", "NO");
	/*
	 * The user Apple names as the owner of what it installs.  Read
	 * from the process rather than written down: it is whoever is
	 * running the build.
	 */
	{
		struct passwd *pw = getpwuid(getuid());
		struct group *gr = getgrgid(getgid());

		settings_defaults_set(t, "ALTERNATE_OWNER",
		                      (pw != NULL) ? pw->pw_name : "");
		settings_defaults_set(t, "ALTERNATE_GROUP",
		                      (gr != NULL) ? gr->gr_name : "");
		settings_defaults_set(t, "INSTALL_OWNER",
		                      (pw != NULL) ? pw->pw_name : "");
		settings_defaults_set(t, "INSTALL_GROUP",
		                      (gr != NULL) ? gr->gr_name : "");
		settings_defaults_set(t, "USER",
		                      (pw != NULL) ? pw->pw_name : "");

		if (pw != NULL && pw->pw_dir != NULL)
			settings_defaults_set(t, "HOME", pw->pw_dir);
	}

		/* Architectures a build may name, which is wider than the
	   ones it builds: the SDK carries headers for i386 and the
	   stubs declare arm64e, and this is Apple's own list. */
	settings_defaults_set(t, "VALID_ARCHS", "arm64 arm64e i386 x86_64");
	settings_defaults_set(t, "ACTION", "build");
	settings_defaults_set(t, "AD_HOC_ENTITLEMENTS_ASSET_PATH", "");
	settings_defaults_set(t, "ALTERNATE_FILE_DEVELOPMENT_LANGUAGE_DIR", "");
	
	settings_defaults_set(t, "ARCHS", sdk.default_arch ? sdk.default_arch : "arm64");
	settings_defaults_set(t, "BUILD_ACTIVE_ARCH_ONLY", "YES");
	/* Apple's own default, read back from -showBuildSettings on a
	   project that does not set it.  Xcode's templates say YES for
	   Debug; a project that wants that says so itself. */
	settings_defaults_set(t, "ONLY_ACTIVE_ARCH", "NO");
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
	
	settings_defaults_set(t, "CONFIGURATION", configuration ? configuration : "Debug");
	settings_defaults_set(t, "CONFIGURATION_BUILD_DIR", "");
	settings_defaults_set(t, "CONFIGURATION_TEMP_DIR", "");
	settings_defaults_set(t, "CP", "/bin/cp");
	settings_defaults_set(t, "CREATE_EMBEDDED_SOURCE_DIR", "NO");
	
	settings_defaults_set(t, "DEFINES_MODULE", "NO");
	settings_defaults_set(t, "DEPLOYMENT_TARGET", deploy);
	settings_defaults_set(t, "DERIVED_FILE_DIR", "");
	settings_defaults_set(t, "DERIVED_FILES_DIR", "");
	settings_defaults_set(t, "DEVELOPER_DIR", devpath);
	settings_defaults_set(t, "DSTROOT", "");
	settings_defaults_set(t, "ENABLE_BITCODE", "NO");
	settings_defaults_set(t, "ENABLE_STRICT_OBJC_MSGSEND", "YES");
	
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
			{
				char *v = settings_expand(t, t->entries[i].value);

				json_escape(stdout,
				    (v != NULL) ? v : t->entries[i].value);
				free(v);
			}
			fputs(pretty ? "\n\t}" : "  }", stdout);
			if (i + 1 < t->count)
				fputs(",", stdout);
		}
		fprintf(stdout, "%s]%s", nl, nl);
		return 0;
	}

	/*
	 * Expanded on the way out.  A default is stored as written --
	 * PRODUCT_NAME is "$(TARGET_NAME)" -- and xcodebuild reports what
	 * the setting resolves to, not the reference.
	 */
	for (size_t i = 0; i < t->count; i++) {
		char *v = settings_expand(t, t->entries[i].value);

		fprintf(stdout, "    %s = %s\n", t->entries[i].key,
		    (v != NULL) ? v : t->entries[i].value);
		free(v);
	}
	return 0;
}
