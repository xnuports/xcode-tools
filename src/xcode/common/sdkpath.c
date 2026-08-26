/*
 * sdkpath.c -- locate SDKs and toolchains inside a Developer directory.
 *
 * Copyright (c) 2026 Sunneva N. Mariu
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "plist.h"
#include "sdkpath.h"

static int
is_dir(const char *path)
{
	struct stat st;

	return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

static char *
build_path(const char *a, const char *b, const char *c, const char *d)
{
	char buf[PATH_MAX];

	snprintf(buf, sizeof(buf), "%s%s%s%s",
		 a ? a : "", b ? b : "", c ? c : "", d ? d : "");
	return strdup(buf);
}

/*
 * Apple keeps each SDK inside its platform bundle, so finding one by
 * name means walking the platforms rather than looking in a single
 * directory.
 */
static char *
find_sdk_in_platforms(const char *devdir, const char *name)
{
	char platforms[PATH_MAX], candidate[PATH_MAX];
	struct dirent *e;
	DIR *d;
	char *found = NULL;

	snprintf(platforms, sizeof(platforms), "%s/Platforms", devdir);
	if ((d = opendir(platforms)) == NULL)
		return NULL;

	while ((e = readdir(d)) != NULL) {
		size_t len = strlen(e->d_name);

		if (len < 10 || strcmp(e->d_name + len - 9, ".platform") != 0)
			continue;

		snprintf(candidate, sizeof(candidate),
			 "%s/%s/Developer/SDKs/%s.sdk",
			 platforms, e->d_name, name);
		if (is_dir(candidate)) {
			found = strdup(candidate);
			break;
		}
	}

	closedir(d);
	return found;
}

char *
xt_find_sdk(const char *devdir, const char *name)
{
	char *path;

	if (devdir == NULL || name == NULL)
		return NULL;

	if ((path = find_sdk_in_platforms(devdir, name)) != NULL)
		return path;

	/* The flat layout this project used before. */
	path = build_path(devdir, "/SDKs/", name, ".sdk");
	if (path != NULL && is_dir(path))
		return path;
	free(path);

	return NULL;
}

char *
xt_find_toolchain(const char *devdir, const char *name)
{
	char *path;

	if (devdir == NULL || name == NULL)
		return NULL;

	path = build_path(devdir, "/Toolchains/", name, ".xctoolchain");
	if (path != NULL && is_dir(path))
		return path;
	free(path);

	path = build_path(devdir, "/Toolchains/", name, ".toolchain");
	if (path != NULL && is_dir(path))
		return path;
	free(path);

	return NULL;
}

/*
 * Used to pick a default when nothing has been selected.
 *
 * The host platform comes first: a Developer directory holds SDKs for
 * every platform Xcode supports, and taking whichever the filesystem
 * happens to return picks something arbitrary -- AppleTVOS, as it turns
 * out, where Apple's xcrun would say macosx.  Only if there is no macOS
 * SDK does anything else get considered.
 */
char *
xt_first_sdk_name(const char *devdir)
{
	static const char *preferred[] = { "MacOSX", NULL };
	size_t p;

	for (p = 0; preferred[p] != NULL; p++) {
		char *path = xt_find_sdk(devdir, preferred[p]);

		if (path != NULL) {
			free(path);
			return strdup(preferred[p]);
		}
	}

	return xt_any_sdk_name(devdir);
}

/*
 * Any SDK at all, platforms first so the answer matches xt_find_sdk's
 * preference.  Versioned names (MacOSX26.5.sdk) are returned as they
 * appear; callers pass them straight back to xt_find_sdk.
 */
char *
xt_any_sdk_name(const char *devdir)
{
	char dirpath[PATH_MAX], inner[PATH_MAX];
	struct dirent *e, *e2;
	DIR *d, *d2;
	char *name = NULL;

	if (devdir == NULL)
		return NULL;

	snprintf(dirpath, sizeof(dirpath), "%s/Platforms", devdir);
	if ((d = opendir(dirpath)) != NULL) {
		while (name == NULL && (e = readdir(d)) != NULL) {
			size_t len = strlen(e->d_name);

			if (len < 10 || strcmp(e->d_name + len - 9, ".platform") != 0)
				continue;

			snprintf(inner, sizeof(inner), "%s/%s/Developer/SDKs",
				 dirpath, e->d_name);
			if ((d2 = opendir(inner)) == NULL)
				continue;

			while ((e2 = readdir(d2)) != NULL) {
				size_t l2 = strlen(e2->d_name);
				char *dot;

				if (l2 < 5 || strcmp(e2->d_name + l2 - 4, ".sdk") != 0)
					continue;
				name = strdup(e2->d_name);
				if (name != NULL && (dot = strstr(name, ".sdk")) != NULL)
					*dot = '\0';
				break;
			}
			closedir(d2);
		}
		closedir(d);
	}
	if (name != NULL)
		return name;

	snprintf(dirpath, sizeof(dirpath), "%s/SDKs", devdir);
	if ((d = opendir(dirpath)) != NULL) {
		while ((e = readdir(d)) != NULL) {
			size_t len = strlen(e->d_name);
			char *dot;

			if (len < 5 || strcmp(e->d_name + len - 4, ".sdk") != 0)
				continue;
			name = strdup(e->d_name);
			if (name != NULL && (dot = strstr(name, ".sdk")) != NULL)
				*dot = '\0';
			break;
		}
		closedir(d);
	}

	return name;
}

/*
 * Walk every SDK in the directory: each platform bundle's SDKs first,
 * then the flat layout.  Symlinked SDKs (MacOSX26.sdk -> MacOSX.sdk, as
 * a stock Xcode ships) are reported like any other, which is what
 * xcodebuild -showsdks lists.
 */
int
xt_foreach_sdk(const char *devdir, xt_sdk_cb cb, void *ctx)
{
	char dirpath[PATH_MAX], inner[PATH_MAX], sdkpath[PATH_MAX];
	struct dirent *e, *e2;
	DIR *d, *d2;
	int found = 0;

	if (devdir == NULL || cb == NULL)
		return 0;

	snprintf(dirpath, sizeof(dirpath), "%s/Platforms", devdir);
	if ((d = opendir(dirpath)) != NULL) {
		while ((e = readdir(d)) != NULL) {
			size_t len = strlen(e->d_name);
			char platform[PATH_MAX];

			if (len < 10 || strcmp(e->d_name + len - 9, ".platform") != 0)
				continue;

			snprintf(platform, sizeof(platform), "%.*s",
				 (int)(len - 9), e->d_name);
			snprintf(inner, sizeof(inner), "%s/%s/Developer/SDKs",
				 dirpath, e->d_name);
			if ((d2 = opendir(inner)) == NULL)
				continue;

			while ((e2 = readdir(d2)) != NULL) {
				size_t l2 = strlen(e2->d_name);

				if (l2 < 5 || strcmp(e2->d_name + l2 - 4, ".sdk") != 0)
					continue;
				snprintf(sdkpath, sizeof(sdkpath), "%s/%s",
					 inner, e2->d_name);
				cb(platform, sdkpath, ctx);
				found++;
			}
			closedir(d2);
		}
		closedir(d);
	}

	snprintf(dirpath, sizeof(dirpath), "%s/SDKs", devdir);
	if ((d = opendir(dirpath)) != NULL) {
		while ((e = readdir(d)) != NULL) {
			size_t len = strlen(e->d_name);

			if (len < 5 || strcmp(e->d_name + len - 4, ".sdk") != 0)
				continue;
			snprintf(sdkpath, sizeof(sdkpath), "%s/%s", dirpath, e->d_name);
			cb("", sdkpath, ctx);
			found++;
		}
		closedir(d);
	}

	return found;
}

/*
 * Read a plist wholesale.  These files are small -- a few kilobytes at
 * most -- so there is no reason to stream them.
 */
static plist_node *
read_plist(const char *path)
{
	plist_node *root;
	struct stat st;
	char *text;
	size_t got;
	FILE *fp;

	if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
		return NULL;
	if ((fp = fopen(path, "r")) == NULL)
		return NULL;
	if ((text = malloc((size_t)st.st_size + 1)) == NULL) {
		fclose(fp);
		return NULL;
	}

	got = fread(text, 1, (size_t)st.st_size, fp);
	fclose(fp);
	text[got] = '\0';

	root = plist_parse_any(text, got);
	free(text);

	return root;
}

static char *
sdk_string(const char *sdkpath, const char *section, const char *key)
{
	char path[PATH_MAX];
	plist_node *root, *dict, *node;
	char *value = NULL;

	if (sdkpath == NULL || key == NULL)
		return NULL;

	snprintf(path, sizeof(path), "%s/SDKSettings.plist", sdkpath);
	if ((root = read_plist(path)) == NULL)
		return NULL;

	dict = root;
	if (section != NULL && (dict = plist_dict_get(root, section)) == NULL) {
		plist_free(root);
		return NULL;
	}

	if ((node = plist_dict_get(dict, key)) != NULL && node->string != NULL)
		value = strdup(node->string);

	plist_free(root);
	return value;
}

char *
xt_sdk_setting(const char *sdkpath, const char *key)
{
	return sdk_string(sdkpath, NULL, key);
}

char *
xt_sdk_default_property(const char *sdkpath, const char *key)
{
	return sdk_string(sdkpath, "DefaultProperties", key);
}

char *
xt_toolchain_identifier(const char *tcpath)
{
	char path[PATH_MAX];
	plist_node *root, *node;
	char *value = NULL;

	if (tcpath == NULL)
		return NULL;

	snprintf(path, sizeof(path), "%s/ToolchainInfo.plist", tcpath);
	if ((root = read_plist(path)) == NULL)
		return NULL;

	if ((node = plist_dict_get(root, "Identifier")) != NULL &&
	    node->string != NULL)
		value = strdup(node->string);

	plist_free(root);
	return value;
}
