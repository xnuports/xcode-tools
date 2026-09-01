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

#include <CoreFoundation/CoreFoundation.h>

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


/* ------------------------------------------------------------------ */
/* Property list access                                                 */
/*                                                                      */
/* A .pbxproj is an OpenStep-format property list, and Apple's          */
/* xcodebuild reads it with CoreFoundation, which parses that format    */
/* directly.  This tree used to parse it by hand and would give up on   */
/* real projects -- a stock Xcode template reported "missing a          */
/* rootObject" that CF finds without trouble.                           */
/*                                                                      */
/* These wrappers keep the walking code reading the way it did: fetch   */
/* by key, index an array, ask for a string, with the type checked and  */
/* a NULL container tolerated at every step.                            */
/* ------------------------------------------------------------------ */

static CFTypeRef
pget(CFTypeRef dict, const char *key)
{
	CFStringRef k;
	CFTypeRef v;

	if (dict == NULL || CFGetTypeID(dict) != CFDictionaryGetTypeID())
		return NULL;

	if ((k = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8)) == NULL)
		return NULL;

	v = CFDictionaryGetValue((CFDictionaryRef)dict, k);
	CFRelease(k);
	return v;
}

static CFTypeRef
pat(CFTypeRef array, CFIndex i)
{
	if (array == NULL || CFGetTypeID(array) != CFArrayGetTypeID())
		return NULL;
	if (i < 0 || i >= CFArrayGetCount((CFArrayRef)array))
		return NULL;

	return CFArrayGetValueAtIndex((CFArrayRef)array, i);
}

static CFIndex
pcount(CFTypeRef array)
{
	if (array == NULL || CFGetTypeID(array) != CFArrayGetTypeID())
		return 0;

	return CFArrayGetCount((CFArrayRef)array);
}

static int
pis_dict(CFTypeRef v)
{
	return v != NULL && CFGetTypeID(v) == CFDictionaryGetTypeID();
}

/*
 * A property list string as C.  Copies into the caller's buffer, since
 * a CFString need not hold one -- returns NULL for anything that is not
 * a string, which is what every caller checks.
 */
static const char *
pstr(CFTypeRef v, char *buf, size_t len)
{
	if (v == NULL || CFGetTypeID(v) != CFStringGetTypeID())
		return NULL;
	if (!CFStringGetCString((CFStringRef)v, buf, (CFIndex)len,
	    kCFStringEncodingUTF8))
		return NULL;

	return buf;
}

/* Member of objects named by the string at key, when both are present. */
static CFTypeRef
pderef(CFTypeRef objects, CFTypeRef id_value)
{
	char id[512];

	if (pstr(id_value, id, sizeof(id)) == NULL)
		return NULL;

	return pget(objects, id);
}

CFTypeRef project_load_pbxproj(const char *project)
{
	char *path = project_pbxproj_path(project);
	if (path == NULL)
		return NULL;
	size_t len = 0;
	char *text = file_read_all(path, &len);
	free(path);
	if (text == NULL)
		return NULL;

	CFDataRef data = CFDataCreate(NULL, (const UInt8 *)text, (CFIndex)len);
	free(text);
	if (data == NULL)
		return NULL;

	CFPropertyListRef root = CFPropertyListCreateWithData(NULL, data,
	    kCFPropertyListImmutable, NULL, NULL);
	CFRelease(data);

	if (root != NULL && !pis_dict(root)) {
		CFRelease(root);
		return NULL;
	}

	return root;
}

/* ------------------------------------------------------------------ */
/* Build-settings extraction                                            */
/* ------------------------------------------------------------------ */

static CFTypeRef get_objects_dict(CFTypeRef root, CFTypeRef *out_root_obj_id)
{
	*out_root_obj_id = NULL;
	if (!pis_dict(root))
		return NULL;

	CFTypeRef objects = pget(root, "objects");
	if (!pis_dict(objects))
		return NULL;

	CFTypeRef root_id = pget(root, "rootObject");
	if (root_id == NULL)
		return NULL;

	*out_root_obj_id = root_id;
	return objects;
}

CFTypeRef project_get_project_object(CFTypeRef root)
{
	CFTypeRef root_id = NULL;
	CFTypeRef objects = get_objects_dict(root, &root_id);

	if (objects == NULL || root_id == NULL)
		return NULL;

	return pderef(objects, root_id);
}

CFTypeRef project_find_buildsettings(CFTypeRef root, const char *target,
                                     const char *configuration)
{
	CFTypeRef root_id = NULL;
	CFTypeRef objects = get_objects_dict(root, &root_id);
	char name_buf[512];

	if (objects == NULL || root_id == NULL)
		return NULL;

	CFTypeRef project_obj = pderef(objects, root_id);
	if (!pis_dict(project_obj))
		return NULL;

	CFTypeRef targets = pget(project_obj, "targets");
	CFTypeRef chosen = NULL;

	for (CFIndex i = 0; i < pcount(targets); i++) {
		CFTypeRef tobj = pderef(objects, pat(targets, i));

		if (!pis_dict(tobj))
			continue;

		const char *name = pstr(pget(tobj, "name"), name_buf,
		    sizeof(name_buf));

		if (target != NULL && name != NULL &&
		    strcmp(name, target) == 0) {
			chosen = tobj;
			break;
		}
		if (target == NULL && chosen == NULL)
			chosen = tobj;
	}
	if (chosen == NULL)
		return NULL;

	CFTypeRef clist = pderef(objects, pget(chosen, "buildConfigurationList"));
	if (!pis_dict(clist))
		return NULL;

	CFTypeRef configs = pget(clist, "buildConfigurations");
	CFTypeRef first_cfg = NULL;

	for (CFIndex i = 0; i < pcount(configs); i++) {
		CFTypeRef cfg = pderef(objects, pat(configs, i));

		if (!pis_dict(cfg))
			continue;

		const char *cname = pstr(pget(cfg, "name"), name_buf,
		    sizeof(name_buf));

		if (cname == NULL)
			continue;
		if (first_cfg == NULL)
			first_cfg = cfg;
		if (configuration != NULL && strcmp(cname, configuration) == 0)
			return pget(cfg, "buildSettings");
	}

	if (configuration == NULL && first_cfg != NULL)
		return pget(first_cfg, "buildSettings");

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

/* Scheme names sort without regard to case, as xcodebuild prints them. */
static int scheme_name_cmp(const void *a, const void *b)
{
	return strcasecmp(*(const char *const *)a, *(const char *const *)b);
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

static void collect_all_config_names(CFTypeRef objects, CFTypeRef target_ids,
                                     strvec *out)
{
	char name_buf[512];

	if (objects == NULL || target_ids == NULL)
		return;

	/* Collect from each target's buildConfigurationList. */
	for (CFIndex i = 0; i < pcount(target_ids); i++) {
		CFTypeRef tobj = pderef(objects, pat(target_ids, i));
		CFTypeRef clist = pderef(objects,
		    pget(tobj, "buildConfigurationList"));
		CFTypeRef configs = pget(clist, "buildConfigurations");

		for (CFIndex j = 0; j < pcount(configs); j++) {
			CFTypeRef cfg = pderef(objects, pat(configs, j));
			const char *cname = pstr(pget(cfg, "name"), name_buf,
			    sizeof(name_buf));

			if (cname != NULL)
				strvec_push_unique(out, cname);
		}
	}
}

/*
 * Targets of the projects this one references.
 *
 * A project may embed others through projectReferences, and Xcode
 * creates a scheme for each of their targets too -- so xcodebuild -list
 * shows them under Schemes while Targets stays the main project's own.
 * The referenced path is relative to the directory holding the
 * .xcodeproj, which is what it is resolved against here.
 */
static void
collect_subproject_targets(CFTypeRef objects, CFTypeRef project_obj,
    const char *project_path, strvec *out)
{
	CFTypeRef refs = pget(project_obj, "projectReferences");
	char dir[PATH_MAX], name_buf[512];
	const char *slash;

	if (refs == NULL || project_path == NULL)
		return;

	snprintf(dir, sizeof(dir), "%s", project_path);
	if ((slash = strrchr(dir, '/')) != NULL)
		*(char *)slash = '\0';
	else
		snprintf(dir, sizeof(dir), ".");

	for (CFIndex i = 0; i < pcount(refs); i++) {
		CFTypeRef entry = pat(refs, i);
		CFTypeRef fileref = pderef(objects, pget(entry, "ProjectRef"));
		const char *rel = pstr(pget(fileref, "path"), name_buf,
		    sizeof(name_buf));
		char sub[PATH_MAX];
		CFTypeRef subroot, subobjects, subproj, subtargets;
		CFTypeRef subroot_id = NULL;

		if (rel == NULL)
			continue;

		if (rel[0] == '/')
			snprintf(sub, sizeof(sub), "%s", rel);
		else
			snprintf(sub, sizeof(sub), "%s/%s", dir, rel);

		if ((subroot = project_load_pbxproj(sub)) == NULL)
			continue;

		subobjects = get_objects_dict(subroot, &subroot_id);
		subproj = pderef(subobjects, subroot_id);
		subtargets = pget(subproj, "targets");

		for (CFIndex j = 0; j < pcount(subtargets); j++) {
			CFTypeRef tobj = pderef(subobjects, pat(subtargets, j));
			char tbuf[512];
			const char *tname = pstr(pget(tobj, "name"), tbuf,
			    sizeof(tbuf));

			if (tname != NULL)
				strvec_push_unique(out, tname);
		}

		CFRelease(subroot);
	}
}

int project_list(const char *project, const char *workspace, const xcodebuild_opts *opts)
{
	(void)opts;
	CFTypeRef root = project_load_pbxproj(project ? project : workspace);
	char name_buf[512];

	if (root == NULL) {
		fprintf(stderr, "xcodebuild: error: could not find project at '%s'\n",
		        project ? project : (workspace ? workspace : "."));
		return 1;
	}

	CFTypeRef root_id = NULL;
	CFTypeRef objects = get_objects_dict(root, &root_id);
	if (objects == NULL || root_id == NULL) {
		fprintf(stderr, "xcodebuild: error: project is missing a rootObject\n");
		CFRelease(root);
		return 1;
	}

	CFTypeRef project_obj = pderef(objects, root_id);
	if (project_obj == NULL) {
		CFRelease(root);
		return 1;
	}

	/*
	 * The project's name is its bundle's, not a key in the file: a
	 * PBXProject carries no "name", so reading one there left every
	 * real project reported as "project".
	 */
	const char *proj_name = pstr(pget(project_obj, "name"), name_buf,
	    sizeof(name_buf));

	if (proj_name == NULL) {
		const char *src = project ? project : workspace;

		if (src != NULL) {
			const char *slash = strrchr(src, '/');
			char *dot;

			snprintf(name_buf, sizeof(name_buf), "%s",
			    (slash != NULL) ? slash + 1 : src);
			if ((dot = strrchr(name_buf, '.')) != NULL &&
			    dot != name_buf)
				*dot = '\0';
			proj_name = name_buf;
		}
	}

	printf("Information about project \"%s\":\n",
	    (proj_name != NULL) ? proj_name : "project");

	/* Targets. */
	strvec targets = {0};
	CFTypeRef tarr = pget(project_obj, "targets");
	for (CFIndex i = 0; i < pcount(tarr); i++) {
		CFTypeRef tobj = pderef(objects, pat(tarr, i));
		const char *tname = pstr(pget(tobj, "name"), name_buf,
		    sizeof(name_buf));

		if (tname != NULL)
			strvec_push(&targets, tname);
	}
	if (targets.count > 0) {
		printf("    Targets:\n");
		for (size_t i = 0; i < targets.count; i++)
			printf("        %s\n", targets.items[i]);
		printf("\n");
	}

	/*
	 * Build configurations, with the note xcodebuild prints about
	 * which one it will pick.  The blank lines and the wording are
	 * part of the output anything parsing -list has to read.
	 */
	strvec configs = {0};
	collect_all_config_names(objects, tarr, &configs);
	if (configs.count > 0) {
		printf("    Build Configurations:\n");
		for (size_t i = 0; i < configs.count; i++)
			printf("        %s\n", configs.items[i]);
		printf("\n");
		printf("    If no build configuration is specified and"
		    " -scheme is not passed then \"Release\" is used.\n");
		printf("\n");
	}
	strvec_free(&configs);

	/*
	 * Schemes.  A project's shared schemes are files on disk, but
	 * Xcode also creates one per target on demand, and xcodebuild
	 * lists those too -- a project with a single shared scheme still
	 * reports one per target.  The two sets are merged, and sorted
	 * without regard to case, which is the order they are printed in.
	 */
	strvec schemes = {0};
	char base[PATH_MAX], sdir[PATH_MAX];
	DIR *d;

	snprintf(base, sizeof(base), "%s",
	    project ? project : (workspace ? workspace : "."));
	snprintf(sdir, sizeof(sdir), "%s/xcshareddata/xcschemes", base);

	if ((d = opendir(sdir)) != NULL) {
		struct dirent *e;

		while ((e = readdir(d)) != NULL) {
			char namebuf[256];
			char *dot;

			if (!endswith(e->d_name, ".xcscheme"))
				continue;

			snprintf(namebuf, sizeof(namebuf), "%s", e->d_name);
			if ((dot = strstr(namebuf, ".xcscheme")) != NULL)
				*dot = '\0';
			strvec_push_unique(&schemes, namebuf);
		}
		closedir(d);
	}

	for (size_t i = 0; i < targets.count; i++)
		strvec_push_unique(&schemes, targets.items[i]);

	collect_subproject_targets(objects, project_obj, base, &schemes);

	printf("    Schemes:\n");
	if (schemes.count > 0) {
		qsort(schemes.items, schemes.count, sizeof(*schemes.items),
		    scheme_name_cmp);
		for (size_t i = 0; i < schemes.count; i++)
			printf("        %s\n", schemes.items[i]);
		printf("\n");
	} else {
		printf("        (no schemes found)\n");
	}

	strvec_free(&schemes);
	strvec_free(&targets);

	CFRelease(root);
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
