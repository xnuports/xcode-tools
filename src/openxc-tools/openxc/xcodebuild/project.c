/* xcodebuild -- open source reimplementation of Apple's xcodebuild utility
 *
 * Project / workspace introspection implementation.
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
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>

#include "xcodebuild.h"
#include "plist.h"
#include "project.h"
#include "sdkpath.h"

static const char *pbxproj_name = "project.pbxproj";

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

static char *file_read_all(const char *path, size_t *out_len)
{
	FILE *fp = fopen(path, "rb");
	if (fp == NULL)
		return NULL;
	if (fseek(fp, 0, SEEK_END) != 0) {
		fclose(fp);
		return NULL;
	}
	long sz = ftell(fp);
	if (sz < 0) {
		fclose(fp);
		return NULL;
	}
	rewind(fp);
	char *buf = (char *)malloc((size_t)sz + 1);
	if (buf == NULL) {
		fclose(fp);
		return NULL;
	}
	size_t rd = fread(buf, 1, (size_t)sz, fp);
	fclose(fp);
	buf[rd] = '\0';
	if (out_len)
		*out_len = rd;
	return buf;
}

static int endswith(const char *s, const char *suffix)
{
	size_t ls = strlen(s);
	size_t lss = strlen(suffix);
	return ls >= lss && strcmp(s + ls - lss, suffix) == 0;
}

static int is_dir(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* ------------------------------------------------------------------ */
/* .pbxproj location + loading                                          */
/* ------------------------------------------------------------------ */

char *project_pbxproj_path(const char *project)
{
	if (project == NULL)
		return NULL;
	char full[PATH_MAX];
	if (endswith(project, ".pbxproj")) {
		snprintf(full, sizeof(full), "%s", project);
		return access(full, R_OK) == 0 ? strdup(full) : NULL;
	}
	if (endswith(project, ".xcodeproj")) {
		char candidate[PATH_MAX];
		snprintf(candidate, sizeof(candidate), "%s/%s", project, pbxproj_name);
		return access(candidate, R_OK) == 0 ? strdup(candidate) : NULL;
	}
	/* Treat as a directory containing project.pbxproj. */
	char candidate[PATH_MAX];
	snprintf(candidate, sizeof(candidate), "%s/%s", project, pbxproj_name);
	if (access(candidate, R_OK) == 0)
		return strdup(candidate);
	return NULL;
}

plist_node *project_load_pbxproj(const char *project)
{
	char *path = project_pbxproj_path(project);
	if (path == NULL)
		return NULL;
	size_t len = 0;
	char *text = file_read_all(path, &len);
	free(path);
	if (text == NULL)
		return NULL;
	plist_node *root = plist_parse(text, len);
	free(text);
	return root;
}

/* ------------------------------------------------------------------ */
/* Build-settings extraction                                            */
/* ------------------------------------------------------------------ */

static plist_node *get_objects_dict(plist_node *root, plist_node **out_root_obj_id)
{
	*out_root_obj_id = NULL;
	if (root == NULL || root->type != PLIST_DICT)
		return NULL;
	plist_node *objects = plist_dict_get(root, "objects");
	if (objects == NULL || objects->type != PLIST_DICT)
		return NULL;
	plist_node *root_id = plist_dict_get(root, "rootObject");
	if (root_id == NULL)
		return NULL;
	*out_root_obj_id = root_id;
	return objects;
}

plist_node *project_get_project_object(const plist_node *root)
{
	plist_node *root_id = NULL;
	plist_node *objects = get_objects_dict((plist_node *)root, &root_id);
	if (objects == NULL || root_id == NULL || root_id->type != PLIST_STRING)
		return NULL;
	return plist_dict_get(objects, root_id->string);
}

plist_node *project_find_buildsettings(plist_node *root, const char *target,
                                       const char *configuration)
{
	plist_node *root_id = NULL;
	plist_node *objects = get_objects_dict(root, &root_id);
	if (objects == NULL || root_id == NULL || root_id->type != PLIST_STRING)
		return NULL;

	plist_node *project_obj = plist_dict_get(objects, root_id->string);
	if (project_obj == NULL || project_obj->type != PLIST_DICT)
		return NULL;

	plist_node *targets = plist_dict_get(project_obj, "targets");
	if (targets == NULL || targets->type != PLIST_ARRAY)
		return NULL;

	plist_node *chosen = NULL;
	for (size_t i = 0; i < targets->count; i++) {
		plist_node *tid = plist_array_at(targets, i);
		if (tid == NULL || tid->type != PLIST_STRING)
			continue;
		plist_node *tobj = plist_dict_get(objects, tid->string);
		if (tobj == NULL || tobj->type != PLIST_DICT)
			continue;
		plist_node *name = plist_dict_get(tobj, "name");
		if (target != NULL && name != NULL && name->type == PLIST_STRING &&
		    strcmp(name->string, target) == 0) {
			chosen = tobj;
			break;
		}
		if (target == NULL && chosen == NULL)
			chosen = tobj;
	}
	if (chosen == NULL)
		return NULL;

	plist_node *clistid = plist_dict_get(chosen, "buildConfigurationList");
	if (clistid == NULL || clistid->type != PLIST_STRING)
		return NULL;
	plist_node *clist = plist_dict_get(objects, clistid->string);
	if (clist == NULL || clist->type != PLIST_DICT)
		return NULL;
	plist_node *configs = plist_dict_get(clist, "buildConfigurations");
	if (configs == NULL || configs->type != PLIST_ARRAY)
		return NULL;

	plist_node *first_cfg = NULL;
	for (size_t i = 0; i < configs->count; i++) {
		plist_node *cid = plist_array_at(configs, i);
		if (cid == NULL || cid->type != PLIST_STRING)
			continue;
		plist_node *cfg = plist_dict_get(objects, cid->string);
		if (cfg == NULL || cfg->type != PLIST_DICT)
			continue;
		plist_node *cname = plist_dict_get(cfg, "name");
		if (cname != NULL && cname->type == PLIST_STRING) {
			if (first_cfg == NULL)
				first_cfg = cfg;
			if (configuration != NULL && strcmp(cname->string, configuration) == 0)
				return plist_dict_get(cfg, "buildSettings");
		}
	}
	if (configuration == NULL && first_cfg != NULL)
		return plist_dict_get(first_cfg, "buildSettings");
	return NULL;
}

/* ------------------------------------------------------------------ */
/* String collection helpers (for listing)                              */
/* ------------------------------------------------------------------ */

typedef struct {
	char **items;
	size_t count;
	size_t capacity;
} strvec;

static int strvec_push(strvec *v, const char *s)
{
	if (v->count == v->capacity) {
		size_t cap = v->capacity ? v->capacity * 2 : 8;
		char **items = (char **)realloc(v->items, sizeof(char *) * cap);
		if (items == NULL)
			return -1;
		v->items = items;
		v->capacity = cap;
	}
	v->items[v->count++] = strdup(s);
	if (v->items[v->count - 1] == NULL)
		return -1;
	return 0;
}

static int strvec_present(strvec *v, const char *s)
{
	for (size_t i = 0; i < v->count; i++)
		if (strcmp(v->items[i], s) == 0)
			return 1;
	return 0;
}

static int strvec_push_unique(strvec *v, const char *s)
{
	if (strvec_present(v, s))
		return 0;
	return strvec_push(v, s);
}

static void strvec_free(strvec *v)
{
	for (size_t i = 0; i < v->count; i++)
		free(v->items[i]);
	free(v->items);
	v->items = NULL;
	v->count = v->capacity = 0;
}

/* ------------------------------------------------------------------ */
/* Listing                                                             */
/* ------------------------------------------------------------------ */

static void collect_all_config_names(plist_node *objects, plist_node *target_ids,
                                     strvec *out)
{
	if (objects == NULL || target_ids == NULL)
		return;

	/* Collect from each target's buildConfigurationList. */
	for (size_t i = 0; i < target_ids->count; i++) {
		plist_node *tid = plist_array_at(target_ids, i);
		if (tid == NULL || tid->type != PLIST_STRING)
			continue;
		plist_node *tobj = plist_dict_get(objects, tid->string);
		if (tobj == NULL)
			continue;
		plist_node *clistid = plist_dict_get(tobj, "buildConfigurationList");
		if (clistid == NULL || clistid->type != PLIST_STRING)
			continue;
		plist_node *clist = plist_dict_get(objects, clistid->string);
		if (clist == NULL)
			continue;
		plist_node *configs = plist_dict_get(clist, "buildConfigurations");
		if (configs == NULL || configs->type != PLIST_ARRAY)
			continue;
		for (size_t j = 0; j < configs->count; j++) {
			plist_node *cid = plist_array_at(configs, j);
			if (cid == NULL || cid->type != PLIST_STRING)
				continue;
			plist_node *cfg = plist_dict_get(objects, cid->string);
			if (cfg == NULL)
				continue;
			plist_node *cname = plist_dict_get(cfg, "name");
			if (cname != NULL && cname->type == PLIST_STRING)
				strvec_push_unique(out, cname->string);
		}
	}
}

int project_list(const char *project, const char *workspace, const xcodebuild_opts *opts)
{
	(void)opts;
	plist_node *root = project_load_pbxproj(project ? project : workspace);
	if (root == NULL) {
		fprintf(stderr, "xcodebuild: error: could not find project at '%s'\n",
		        project ? project : (workspace ? workspace : "."));
		return 1;
	}

	plist_node *root_id = NULL;
	plist_node *objects = get_objects_dict(root, &root_id);
	if (objects == NULL || root_id == NULL || root_id->type != PLIST_STRING) {
		fprintf(stderr, "xcodebuild: error: project is missing a rootObject\n");
		plist_free(root);
		return 1;
	}

	plist_node *project_obj = plist_dict_get(objects, root_id->string);
	if (project_obj == NULL) {
		plist_free(root);
		return 1;
	}

	/* Print a banner. */
	plist_node *name = plist_dict_get(project_obj, "name");
	const char *proj_name = (name && name->type == PLIST_STRING) ? name->string : "project";
	printf("Information about project %s:\n", proj_name);

	/* Targets. */
	strvec targets = {0};
	plist_node *tarr = plist_dict_get(project_obj, "targets");
	if (tarr != NULL && tarr->type == PLIST_ARRAY) {
		for (size_t i = 0; i < tarr->count; i++) {
			plist_node *tid = plist_array_at(tarr, i);
			plist_node *tobj = tid ? plist_dict_get(objects, tid->string) : NULL;
			plist_node *tname = tobj ? plist_dict_get(tobj, "name") : NULL;
			if (tname != NULL && tname->type == PLIST_STRING)
				strvec_push(&targets, tname->string);
		}
	}
	if (targets.count > 0) {
		printf("    Targets:\n");
		for (size_t i = 0; i < targets.count; i++)
			printf("        %s\n", targets.items[i]);
	}
	strvec_free(&targets);

	/* Configurations. */
	strvec configs = {0};
	collect_all_config_names(objects, tarr, &configs);
	if (configs.count > 0) {
		printf("    Configurations:\n");
		for (size_t i = 0; i < configs.count; i++)
			printf("        %s\n", configs.items[i]);
	}
	strvec_free(&configs);

	/* Schemes (from the project's shared schemes). */
	char proj_dir[PATH_MAX];
	snprintf(proj_dir, sizeof(proj_dir), "%s", project ? project : (workspace ? workspace : "."));
	char base[PATH_MAX];
	snprintf(base, sizeof(base), "%s", proj_dir);
	printf("    Schemes:\n");
	int printed = 0;
	char sdir[PATH_MAX];
	snprintf(sdir, sizeof(sdir), "%s/xcshareddata/xcschemes", base);
	DIR *d = opendir(sdir);
	if (d != NULL) {
		struct dirent *e;
		while ((e = readdir(d)) != NULL) {
			if (endswith(e->d_name, ".xcscheme")) {
				char namebuf[256];
				snprintf(namebuf, sizeof(namebuf), "%s", e->d_name);
				char *dot = strstr(namebuf, ".xcscheme");
				if (dot) *dot = '\0';
				printf("        %s\n", namebuf);
				printed = 1;
			}
		}
		closedir(d);
	}
	if (!printed)
		printf("        (no schemes found)\n");

	plist_free(root);
	return 0;
}

/* ------------------------------------------------------------------ */
/* SDK / toolchain scanning                                           */
/* ------------------------------------------------------------------ */

static void list_dir(const char *base, const char *sub, const char *suffix)
{
	char dir[PATH_MAX];
	snprintf(dir, sizeof(dir), "%s/%s", base, sub);
	DIR *d = opendir(dir);
	if (d == NULL)
		return;
	struct dirent *e;
	while ((e = readdir(d)) != NULL) {
		if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
			continue;
		if (!endswith(e->d_name, suffix))
			continue;
		printf("    %s\n", e->d_name);
	}
	closedir(d);
}

/*
 * -showsdks, in the shape Apple's prints it: one group per platform,
 * each entry the SDK's DisplayName padded out, then the -sdk flag that
 * selects it, which is its CanonicalName.
 *
 * The old implementation listed the flat <dev>/SDKs directory and
 * labelled everything "iOS SDKs" regardless of what it found.
 */

typedef struct {
	char platform[64];
	char display[128];
	char canonical[128];
} sdk_entry;

typedef struct {
	sdk_entry *items;
	size_t count;
	size_t cap;
} sdk_list;

static void collect_sdk(const char *platform, const char *sdkpath, void *ctx)
{
	sdk_list *list = (sdk_list *)ctx;
	char *display, *canonical;
	sdk_entry *e;

	if (list->count == list->cap) {
		size_t cap = list->cap ? list->cap * 2 : 16;
		sdk_entry *items = realloc(list->items, cap * sizeof(*items));

		if (items == NULL)
			return;
		list->items = items;
		list->cap = cap;
	}

	e = &list->items[list->count];
	memset(e, 0, sizeof(*e));

	snprintf(e->platform, sizeof(e->platform), "%s",
		 (platform != NULL && *platform) ? platform : "Unknown");

	display = xt_sdk_setting(sdkpath, "DisplayName");
	canonical = xt_sdk_setting(sdkpath, "CanonicalName");

	if (display != NULL) {
		snprintf(e->display, sizeof(e->display), "%s", display);
		free(display);
	} else {
		/* No settings file: fall back to the directory name. */
		const char *base = strrchr(sdkpath, '/');

		snprintf(e->display, sizeof(e->display), "%s",
			 base != NULL ? base + 1 : sdkpath);
	}

	if (canonical != NULL) {
		snprintf(e->canonical, sizeof(e->canonical), "%s", canonical);
		free(canonical);
	}

	list->count++;
}

static int sdk_entry_cmp(const void *a, const void *b)
{
	const sdk_entry *x = (const sdk_entry *)a;
	const sdk_entry *y = (const sdk_entry *)b;
	int c = strcmp(x->platform, y->platform);

	return (c != 0) ? c : strcmp(x->display, y->display);
}

void project_show_sdks(const char *devpath)
{
	sdk_list list = { NULL, 0, 0 };
	const char *group = NULL;
	size_t i;

	if (!is_dir(devpath)) {
		fprintf(stderr, "xcodebuild: error: developer directory not found at '%s'\n", devpath);
		return;
	}

	xt_foreach_sdk(devpath, collect_sdk, &list);
	if (list.count == 0) {
		printf("No SDKs found in '%s'.\n", devpath);
		return;
	}

	qsort(list.items, list.count, sizeof(*list.items), sdk_entry_cmp);

	for (i = 0; i < list.count; i++) {
		sdk_entry *e = &list.items[i];

		if (group == NULL || strcmp(group, e->platform) != 0) {
			if (group != NULL)
				printf("\n");
			printf("%s SDKs:\n", e->platform);
			group = e->platform;
		}

		if (e->canonical[0] != '\0')
			printf("\t%-30s\t-sdk %s\n", e->display, e->canonical);
		else
			printf("\t%s\n", e->display);
	}

	free(list.items);
}

void project_show_toolchains(const char *devpath)
{
	if (!is_dir(devpath)) {
		fprintf(stderr, "xcodebuild: error: developer directory not found at '%s'\n", devpath);
		return;
	}
	printf("Available toolchains:\n");
	/* Apple's layout first, then the flat one this project used before. */
	list_dir(devpath, "Toolchains", ".xctoolchain");
	list_dir(devpath, "Toolchains", ".toolchain");
}
