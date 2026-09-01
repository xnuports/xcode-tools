/*
 * build - compile and link a target's sources.
 *
 * Enough of a build to turn a native target's sources into its product.
 * What used to stand here handed the action to xcrun as though "build"
 * were a tool to run, which failed looking for a program of that name,
 * so nothing this tool was pointed at was ever built.
 *
 * Apple drives builds through XCBBuildService, which schedules every
 * phase of an arbitrary project.  This does the part that matters for a
 * tool or a static library: find the sources, compile each one, link
 * the result.  Anything it cannot do it says so and stops, rather than
 * reporting a success it did not achieve.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <CoreFoundation/CoreFoundation.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "xcodebuild.h"
#include "project.h"

/* ------------------------------------------------------------------ */
/* property list access, as in project.c                                */
/* ------------------------------------------------------------------ */

static CFTypeRef
bget(CFTypeRef dict, const char *key)
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

static CFIndex
bcount(CFTypeRef array)
{
	if (array == NULL || CFGetTypeID(array) != CFArrayGetTypeID())
		return 0;
	return CFArrayGetCount((CFArrayRef)array);
}

static CFTypeRef
bat(CFTypeRef array, CFIndex i)
{
	if (i < 0 || i >= bcount(array))
		return NULL;
	return CFArrayGetValueAtIndex((CFArrayRef)array, i);
}

static const char *
bstr(CFTypeRef v, char *buf, size_t len)
{
	if (v == NULL || CFGetTypeID(v) != CFStringGetTypeID())
		return NULL;
	if (!CFStringGetCString((CFStringRef)v, buf, (CFIndex)len,
	    kCFStringEncodingUTF8))
		return NULL;
	return buf;
}

static CFTypeRef
bderef(CFTypeRef objects, CFTypeRef id_value)
{
	char id[512];

	if (bstr(id_value, id, sizeof(id)) == NULL)
		return NULL;
	return bget(objects, id);
}

/* ------------------------------------------------------------------ */
/* source resolution                                                    */
/* ------------------------------------------------------------------ */

/*
 * Where a file reference lives, and what its path is measured from.
 *
 * A group-relative path can only be resolved while the tree is walked,
 * since nothing else knows the enclosing groups, so the walk stores it
 * resolved.  The other roots name somewhere that is not known until a
 * target is actually being built -- built products land in a directory
 * that depends on the configuration -- so those keep the path the
 * project wrote and are resolved on use.
 */
struct pathmap {
	char **ids;
	char **paths;
	char **trees;
	size_t count;
	size_t cap;
};

static void
pathmap_add(struct pathmap *m, const char *id, const char *path,
    const char *tree)
{
	if (m->count == m->cap) {
		size_t cap = (m->cap == 0) ? 64 : m->cap * 2;
		char **ids = realloc(m->ids, cap * sizeof(*ids));
		char **paths = realloc(m->paths, cap * sizeof(*paths));
		char **trees = realloc(m->trees, cap * sizeof(*trees));

		if (ids != NULL)
			m->ids = ids;
		if (paths != NULL)
			m->paths = paths;
		if (trees != NULL)
			m->trees = trees;
		if (ids == NULL || paths == NULL || trees == NULL)
			return;
		m->cap = cap;
	}

	if ((m->ids[m->count] = strdup(id)) == NULL)
		return;
	if ((m->paths[m->count] = strdup(path)) == NULL) {
		free(m->ids[m->count]);
		return;
	}
	if ((m->trees[m->count] = strdup((tree != NULL) ? tree : "")) == NULL) {
		free(m->ids[m->count]);
		free(m->paths[m->count]);
		return;
	}
	m->count++;
}

static const char *
pathmap_get(const struct pathmap *m, const char *id)
{
	size_t i;

	for (i = 0; i < m->count; i++)
		if (strcmp(m->ids[i], id) == 0)
			return m->paths[i];
	return NULL;
}

static const char *
pathmap_tree(const struct pathmap *m, const char *id)
{
	size_t i;

	for (i = 0; i < m->count; i++)
		if (strcmp(m->ids[i], id) == 0)
			return m->trees[i];
	return NULL;
}

/*
 * A reference as a path on disk.  Returns NULL when the reference is
 * unknown, or names a root this build has not got.
 */
static const char *
pathmap_resolve(const struct pathmap *m, const char *id, const char *build_dir,
    const char *sdkroot, const char *devpath, char *buf, size_t len)
{
	const char *path = pathmap_get(m, id);
	const char *tree = pathmap_tree(m, id);

	if (path == NULL)
		return NULL;
	if (path[0] == '/' || tree == NULL || *tree == '\0')
		return path;

	if (strcmp(tree, "BUILT_PRODUCTS_DIR") == 0) {
		if (build_dir == NULL)
			return NULL;
		snprintf(buf, len, "%s/%s", build_dir, path);
	} else if (strcmp(tree, "SDKROOT") == 0) {
		if (sdkroot == NULL)
			return NULL;
		snprintf(buf, len, "%s/%s", sdkroot, path);
	} else if (strcmp(tree, "DEVELOPER_DIR") == 0) {
		if (devpath == NULL)
			return NULL;
		snprintf(buf, len, "%s/%s", devpath, path);
	} else {
		return path;
	}

	return buf;
}

static void
pathmap_free(struct pathmap *m)
{
	size_t i;

	for (i = 0; i < m->count; i++) {
		free(m->ids[i]);
		free(m->paths[i]);
		free(m->trees[i]);
	}
	free(m->ids);
	free(m->paths);
	free(m->trees);
}

/*
 * Walk the group tree, recording where every file reference lives.
 *
 * A file's path is relative to the group holding it, and a group
 * contributes to that path only when it has one of its own -- a group
 * with just a name is a folder in the navigator and nothing on disk.
 * sourceTree says what the path is relative to; SOURCE_ROOT and an
 * absolute path escape the enclosing group, which is why the prefix is
 * not simply accumulated.
 */
static void
walk_group(CFTypeRef objects, CFTypeRef group, const char *prefix,
    const char *source_root, struct pathmap *map)
{
	CFTypeRef children = bget(group, "children");
	CFIndex i;

	for (i = 0; i < bcount(children); i++) {
		CFTypeRef child_id = bat(children, i);
		CFTypeRef child = bderef(objects, child_id);
		char idbuf[512], pathbuf[512], treebuf[64], isabuf[64];
		const char *path, *tree, *isa, *id;
		char full[PATH_MAX];

		if (child == NULL)
			continue;

		id = bstr(child_id, idbuf, sizeof(idbuf));
		isa = bstr(bget(child, "isa"), isabuf, sizeof(isabuf));
		path = bstr(bget(child, "path"), pathbuf, sizeof(pathbuf));
		tree = bstr(bget(child, "sourceTree"), treebuf, sizeof(treebuf));

		if (path == NULL) {
			/* A named group with no path of its own. */
			if (isa != NULL && strstr(isa, "Group") != NULL)
				walk_group(objects, child, prefix, source_root, map);
			continue;
		}

		if (path[0] == '/')
			snprintf(full, sizeof(full), "%s", path);
		else if (tree != NULL && strcmp(tree, "SOURCE_ROOT") == 0)
			snprintf(full, sizeof(full), "%s/%s", source_root, path);
		else if (tree != NULL &&
		    (strcmp(tree, "BUILT_PRODUCTS_DIR") == 0 ||
		     strcmp(tree, "SDKROOT") == 0 ||
		     strcmp(tree, "DEVELOPER_DIR") == 0))
			snprintf(full, sizeof(full), "%s", path);
		else
			snprintf(full, sizeof(full), "%s/%s", prefix, path);

		if (isa != NULL && strstr(isa, "Group") != NULL)
			walk_group(objects, child, full, source_root, map);
		else if (id != NULL)
			pathmap_add(map, id, full, tree);
	}
}

/* ------------------------------------------------------------------ */
/* targets, and the order to build them in                              */
/* ------------------------------------------------------------------ */

struct idlist {
	char **ids;
	size_t count;
	size_t cap;
};

static int
idlist_has(const struct idlist *l, const char *id)
{
	size_t i;

	for (i = 0; i < l->count; i++)
		if (strcmp(l->ids[i], id) == 0)
			return 1;
	return 0;
}

static void
idlist_add(struct idlist *l, const char *id)
{
	if (l->count == l->cap) {
		size_t cap = (l->cap == 0) ? 16 : l->cap * 2;
		char **ids = realloc(l->ids, cap * sizeof(*ids));

		if (ids == NULL)
			return;
		l->ids = ids;
		l->cap = cap;
	}

	if ((l->ids[l->count] = strdup(id)) != NULL)
		l->count++;
}

static void
idlist_drop_last(struct idlist *l)
{
	if (l->count > 0)
		free(l->ids[--l->count]);
}

static void
idlist_free(struct idlist *l)
{
	size_t i;

	for (i = 0; i < l->count; i++)
		free(l->ids[i]);
	free(l->ids);
	memset(l, 0, sizeof(*l));
}

/* The id of the target a build file's fileRef names, if it is a product. */
static const char *
target_id_for_product(CFTypeRef objects, CFTypeRef targets, const char *ref,
    char *buf, size_t len)
{
	CFIndex i;

	for (i = 0; i < bcount(targets); i++) {
		CFTypeRef tid = bat(targets, i);
		CFTypeRef tobj = bderef(objects, tid);
		char prbuf[512];
		const char *pr = bstr(bget(tobj, "productReference"), prbuf,
		    sizeof(prbuf));

		if (pr != NULL && strcmp(pr, ref) == 0)
			return bstr(tid, buf, len);
	}

	return NULL;
}

/*
 * The targets a target depends on.
 *
 * A project states this twice over.  The explicit form is a
 * PBXTargetDependency naming another target; the implicit form is a
 * target linking another target's product, which is what Xcode means
 * by finding implicit dependencies.  Both are followed, because a
 * project that builds in Xcode may have written only one of them.
 *
 * A dependency on a target in another project is recognised and
 * reported rather than followed: building it means building that
 * project too, which is a larger thing than this does.
 */
static void
collect_deps(CFTypeRef objects, CFTypeRef project_obj, CFTypeRef target,
    struct idlist *out, int *foreign)
{
	CFTypeRef targets = bget(project_obj, "targets");
	CFTypeRef deps = bget(target, "dependencies");
	CFTypeRef phases = bget(target, "buildPhases");
	CFIndex i;

	for (i = 0; i < bcount(deps); i++) {
		CFTypeRef dep = bderef(objects, bat(deps, i));
		char idbuf[512];
		const char *id = bstr(bget(dep, "target"), idbuf, sizeof(idbuf));

		if (id == NULL) {
			CFTypeRef proxy = bderef(objects,
			    bget(dep, "targetProxy"));
			char pbuf[512];
			const char *remote;

			remote = bstr(bget(proxy, "remoteGlobalIDString"), pbuf,
			    sizeof(pbuf));

			/*
			 * A proxy names its target by an id that means
			 * something in the project holding it.  One that
			 * resolves here is a target of this project; one
			 * that does not belongs to a subproject, and
			 * building it means building that project too.
			 */
			if (remote != NULL && bget(objects, remote) != NULL)
				id = remote;
			else if (remote != NULL) {
				if (foreign != NULL)
					(*foreign)++;
				continue;
			}
		}

		if (id != NULL && bget(objects, id) != NULL &&
		    !idlist_has(out, id))
			idlist_add(out, id);
	}

	/* Linking another target's product is a dependency on it. */
	for (i = 0; i < bcount(phases); i++) {
		CFTypeRef phase = bderef(objects, bat(phases, i));
		char isabuf[64];
		const char *isa = bstr(bget(phase, "isa"), isabuf,
		    sizeof(isabuf));
		CFTypeRef files;
		CFIndex f;

		if (isa == NULL || strcmp(isa, "PBXFrameworksBuildPhase") != 0)
			continue;

		files = bget(phase, "files");
		for (f = 0; f < bcount(files); f++) {
			CFTypeRef bf = bderef(objects, bat(files, f));
			char refbuf[512], tidbuf[512];
			const char *ref = bstr(bget(bf, "fileRef"), refbuf,
			    sizeof(refbuf));
			const char *tid;

			if (ref == NULL)
				continue;

			tid = target_id_for_product(objects, targets, ref,
			    tidbuf, sizeof(tidbuf));
			if (tid != NULL && !idlist_has(out, tid))
				idlist_add(out, tid);
		}
	}
}

/*
 * Depth-first, dependencies before dependents.
 *
 * `stack` holds the targets on the current path, so a project that
 * declares a cycle is named and refused rather than followed until
 * this runs out of stack.
 */
static int
order_targets(CFTypeRef objects, CFTypeRef project_obj, const char *id,
    struct idlist *order, struct idlist *stack, int *foreign)
{
	CFTypeRef target = bget(objects, id);
	struct idlist deps;
	char namebuf[512];
	const char *name;
	size_t i;
	int rc = 0;

	if (target == NULL || idlist_has(order, id))
		return 0;

	name = bstr(bget(target, "name"), namebuf, sizeof(namebuf));

	if (idlist_has(stack, id)) {
		fprintf(stderr, "xcodebuild: error: target '%s' is part of a"
		    " dependency cycle\n", (name != NULL) ? name : id);
		return 1;
	}

	idlist_add(stack, id);

	memset(&deps, 0, sizeof(deps));
	collect_deps(objects, project_obj, target, &deps, foreign);

	for (i = 0; i < deps.count && rc == 0; i++)
		rc = order_targets(objects, project_obj, deps.ids[i], order,
		    stack, foreign);

	idlist_free(&deps);
	idlist_drop_last(stack);

	if (rc == 0 && !idlist_has(order, id))
		idlist_add(order, id);

	return rc;
}

/* ------------------------------------------------------------------ */
/* link inputs                                                          */
/* ------------------------------------------------------------------ */

/* A build file marked Weak in the phase that holds it. */
static int
build_file_is_weak(CFTypeRef bf)
{
	CFTypeRef attrs = bget(bget(bf, "settings"), "ATTRIBUTES");
	CFIndex i;

	for (i = 0; i < bcount(attrs); i++) {
		char buf[64];
		const char *a = bstr(bat(attrs, i), buf, sizeof(buf));

		if (a != NULL && strcasecmp(a, "Weak") == 0)
			return 1;
	}

	return 0;
}

static int
has_ext(const char *path, const char *ext)
{
	const char *dot = strrchr(path, '.');

	return dot != NULL && strcasecmp(dot, ext) == 0;
}

/*
 * What a target links against, as linker arguments.
 *
 * Everything in the frameworks phase, resolved to a path and turned
 * into the argument the linker wants: a framework is named with
 * -framework and found with -F, while an archive, a dylib and a stub
 * are handed over by path.  A reference that cannot be resolved is
 * reported -- linking without it would fail later with an undefined
 * symbol that says nothing about the missing input.
 */
static int
add_link_inputs(char *argv[], int a, int max, CFTypeRef objects,
    CFTypeRef target, const struct pathmap *map, const char *build_dir,
    const char *sdkroot, const char *devpath, int *unresolved)
{
	CFTypeRef phases = bget(target, "buildPhases");
	CFIndex p;

	for (p = 0; p < bcount(phases); p++) {
		CFTypeRef phase = bderef(objects, bat(phases, p));
		char isabuf[64];
		const char *isa = bstr(bget(phase, "isa"), isabuf,
		    sizeof(isabuf));
		CFTypeRef files;
		CFIndex f;

		if (isa == NULL || strcmp(isa, "PBXFrameworksBuildPhase") != 0)
			continue;

		files = bget(phase, "files");
		for (f = 0; f < bcount(files) && a + 4 < max; f++) {
			CFTypeRef bf = bderef(objects, bat(files, f));
			char refbuf[512], resbuf[PATH_MAX];
			const char *ref = bstr(bget(bf, "fileRef"), refbuf,
			    sizeof(refbuf));
			const char *path;

			if (ref == NULL)
				continue;

			path = pathmap_resolve(map, ref, build_dir, sdkroot,
			    devpath, resbuf, sizeof(resbuf));
			if (path == NULL) {
				const char *raw = pathmap_get(map, ref);

				fprintf(stderr, "xcodebuild: warning: cannot"
				    " find the library '%s' this target links"
				    " against\n",
				    (raw != NULL) ? raw : ref);
				if (unresolved != NULL)
					(*unresolved)++;
				continue;
			}

			if (has_ext(path, ".framework")) {
				char dir[PATH_MAX], name[256];
				const char *base = strrchr(path, '/');
				const char *dot;

				base = (base != NULL) ? base + 1 : path;
				dot = strrchr(base, '.');
				snprintf(name, sizeof(name), "%.*s",
				    (int)(dot - base), base);

				if (base > path) {
					snprintf(dir, sizeof(dir), "%.*s",
					    (int)(base - path - 1), path);
					argv[a++] = strdup("-F");
					argv[a++] = strdup(dir);
				}

				argv[a++] = strdup(build_file_is_weak(bf) ?
				    "-weak_framework" : "-framework");
				argv[a++] = strdup(name);
			} else {
				if (build_file_is_weak(bf))
					argv[a++] = strdup("-weak_library");
				argv[a++] = strdup(path);
			}
		}
	}

	return a;
}

/* ------------------------------------------------------------------ */
/* product types                                                        */
/*                                                                      */
/* What a target produces, and what it is called.  Verified against     */
/* Apple's -showBuildSettings for a tool (mh_execute, no affixes,        */
/* FULL_PRODUCT_NAME the target name) and a static library (staticlib,  */
/* "lib" and ".a", libDCE.a).  The dynamic library and bundle forms are  */
/* the long-standing Xcode conventions; they could not be read back      */
/* here, since Apple needs a platform it can resolve to answer at all.  */
/* ------------------------------------------------------------------ */

enum product_kind {
	PRODUCT_TOOL,
	PRODUCT_STATIC_LIB,
	PRODUCT_DYNAMIC_LIB,
	PRODUCT_BUNDLE		/* application or framework */
};

struct product {
	enum product_kind kind;
	const char *macho_type;
	const char *prefix;
	const char *suffix;
	const char *wrapper;	/* bundle extension, without the dot */
};

static void
classify_product(const char *product_type, struct product *out)
{
	out->kind = PRODUCT_TOOL;
	out->macho_type = "mh_execute";
	out->prefix = "";
	out->suffix = "";
	out->wrapper = NULL;

	if (product_type == NULL)
		return;

	if (strstr(product_type, "library.static") != NULL) {
		out->kind = PRODUCT_STATIC_LIB;
		out->macho_type = "staticlib";
		out->prefix = "lib";
		out->suffix = ".a";
	} else if (strstr(product_type, "library.dynamic") != NULL) {
		out->kind = PRODUCT_DYNAMIC_LIB;
		out->macho_type = "mh_dylib";
		out->prefix = "lib";
		out->suffix = ".dylib";
	} else if (strstr(product_type, "framework") != NULL) {
		out->kind = PRODUCT_BUNDLE;
		out->macho_type = "mh_dylib";
		out->wrapper = "framework";
	} else if (strstr(product_type, "application") != NULL) {
		out->kind = PRODUCT_BUNDLE;
		out->wrapper = "app";
	}
}

/*
 * Record what a target produces.  Called while settings are resolved,
 * so -showBuildSettings reports the same names a build writes -- the
 * classification is a property of the target, not of building it.
 */
void
build_apply_product_settings(settings_table *t, const char *product_type)
{
	struct product prod;
	const char *pn;
	char full[PATH_MAX];

	if (t == NULL || product_type == NULL || *product_type == '\0')
		return;

	classify_product(product_type, &prod);

	settings_set(t, "PRODUCT_TYPE", product_type);
	settings_set(t, "MACH_O_TYPE", prod.macho_type);
	settings_set(t, "EXECUTABLE_PREFIX", prod.prefix);
	settings_set(t, "EXECUTABLE_SUFFIX", prod.suffix);

	if ((pn = settings_get(t, "PRODUCT_NAME")) == NULL || *pn == '\0')
		pn = "product";

	if (prod.wrapper != NULL)
		snprintf(full, sizeof(full), "%s.%s", pn, prod.wrapper);
	else
		snprintf(full, sizeof(full), "%s%s%s", prod.prefix, pn,
		    prod.suffix);

	settings_set(t, "FULL_PRODUCT_NAME", full);
}

/*
 * A build setting, resolved.
 *
 * settings_get returns what was stored, and a default is stored as
 * written -- PRODUCT_MODULE_NAME is "$(TARGET_NAME)" until something
 * expands it.  Values merged from a project are expanded as they are
 * merged, so only some settings arrive ready to use; reading them all
 * through here removes the distinction.  Returns buf, or NULL if the
 * setting is unset or empty.
 */
static const char *
setting(const settings_table *t, const char *key, char *buf, size_t len)
{
	const char *raw = settings_get(t, key);
	char *expanded;

	if (raw == NULL || *raw == '\0')
		return NULL;

	if ((expanded = settings_expand(t, raw)) == NULL) {
		snprintf(buf, len, "%s", raw);
		return (buf[0] != '\0') ? buf : NULL;
	}

	snprintf(buf, len, "%s", expanded);
	free(expanded);

	return (buf[0] != '\0') ? buf : NULL;
}

/* ------------------------------------------------------------------ */
/* running a command                                                    */
/* ------------------------------------------------------------------ */

static int
run(char *const argv[], int echo)
{
	pid_t pid;
	int status = 0;

	if (echo) {
		int i;

		for (i = 0; argv[i] != NULL; i++)
			printf("%s%s", (i > 0) ? " " : "", argv[i]);
		printf("\n");
	}

	if ((pid = fork()) == 0) {
		execv(argv[0], argv);
		_exit(127);
	}
	if (pid < 0)
		return -1;

	waitpid(pid, &status, 0);
	return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

static int
mkdirs(const char *path)
{
	char buf[PATH_MAX];
	size_t i;

	if (snprintf(buf, sizeof(buf), "%s", path) >= (int)sizeof(buf))
		return -1;

	for (i = 1; buf[i] != '\0'; i++) {
		if (buf[i] != '/')
			continue;
		buf[i] = '\0';
		if (mkdir(buf, 0777) != 0 && errno != EEXIST)
			return -1;
		buf[i] = '/';
	}

	return (mkdir(buf, 0777) == 0 || errno == EEXIST) ? 0 : -1;
}

static int
is_swift(const char *path)
{
	const char *dot = strrchr(path, '.');

	return dot != NULL && strcmp(dot, ".swift") == 0;
}

static int
is_compilable(const char *path)
{
	const char *dot = strrchr(path, '.');

	if (dot == NULL)
		return 0;

	return strcmp(dot, ".c") == 0 || strcmp(dot, ".m") == 0 ||
	    strcmp(dot, ".cc") == 0 || strcmp(dot, ".cpp") == 0 ||
	    strcmp(dot, ".cxx") == 0 || strcmp(dot, ".mm") == 0 ||
	    strcmp(dot, ".s") == 0 || strcmp(dot, ".S") == 0;
}

/*
 * The minimum an application bundle needs to be one: the name of its
 * executable and an identifier.  Written with CoreFoundation, as every
 * property list this tree emits now is.
 */
static int
write_app_infoplist(const char *build_dir, const char *full_name,
    const char *exe_name, settings_table *t)
{
	CFMutableDictionaryRef info;
	char path[PATH_MAX];
	const char *bundle_id;
	CFDataRef data;
	FILE *fp;
	int rc = 0;

	if (full_name == NULL || exe_name == NULL)
		return 1;

	snprintf(path, sizeof(path), "%s/%s/Contents/Info.plist", build_dir,
	    full_name);

	info = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	if (info == NULL)
		return 1;

	bundle_id = settings_get(t, "PRODUCT_BUNDLE_IDENTIFIER");
	if (bundle_id == NULL || *bundle_id == '\0')
		bundle_id = exe_name;

	{
		CFStringRef k, v;

		k = CFSTR("CFBundleExecutable");
		v = CFStringCreateWithCString(NULL, exe_name, kCFStringEncodingUTF8);
		if (v != NULL) { CFDictionarySetValue(info, k, v); CFRelease(v); }

		k = CFSTR("CFBundleIdentifier");
		v = CFStringCreateWithCString(NULL, bundle_id, kCFStringEncodingUTF8);
		if (v != NULL) { CFDictionarySetValue(info, k, v); CFRelease(v); }

		k = CFSTR("CFBundleName");
		v = CFStringCreateWithCString(NULL, exe_name, kCFStringEncodingUTF8);
		if (v != NULL) { CFDictionarySetValue(info, k, v); CFRelease(v); }

		CFDictionarySetValue(info, CFSTR("CFBundlePackageType"),
		    CFSTR("APPL"));
		CFDictionarySetValue(info, CFSTR("CFBundleInfoDictionaryVersion"),
		    CFSTR("6.0"));
	}

	data = CFPropertyListCreateData(NULL, info, kCFPropertyListXMLFormat_v1_0,
	    0, NULL);
	CFRelease(info);
	if (data == NULL)
		return 1;

	if ((fp = fopen(path, "wb")) == NULL) {
		fprintf(stderr, "xcodebuild: error: cannot write %s\n", path);
		rc = 1;
	} else {
		fwrite(CFDataGetBytePtr(data), 1, (size_t)CFDataGetLength(data), fp);
		fclose(fp);
	}

	CFRelease(data);
	return rc;
}

/*
 * Split a build setting into arguments, each prefixed with a flag.
 *
 * Search paths and preprocessor definitions are single settings holding
 * several whitespace-separated values, quoted where a value contains a
 * space.  A relative path is relative to the project, which is what a
 * project means when it writes DCE/include.
 */
static int
add_setting_args(char *argv[], int a, int max, const char *value,
    const char *flag, const char *srcroot)
{
	const char *p = value;

	if (value == NULL || *value == '\0')
		return a;

	while (*p != '\0' && a + 2 < max) {
		char word[PATH_MAX];
		size_t n = 0;
		char quote = '\0';

		while (*p == ' ' || *p == '\t')
			p++;
		if (*p == '\0')
			break;

		if (*p == '"' || *p == '\'')
			quote = *p++;

		while (*p != '\0' && n + 1 < sizeof(word)) {
			if (quote != '\0' && *p == quote) {
				p++;
				break;
			}
			if (quote == '\0' && (*p == ' ' || *p == '\t'))
				break;
			word[n++] = *p++;
		}
		word[n] = '\0';
		if (n == 0)
			continue;

		argv[a++] = strdup(flag);
		if (word[0] == '/' || srcroot == NULL) {
			argv[a++] = strdup(word);
		} else {
			char full[PATH_MAX];

			snprintf(full, sizeof(full), "%s/%s", srcroot, word);
			argv[a++] = strdup(full);
		}
	}

	return a;
}

/* ------------------------------------------------------------------ */

/*
 * One target: compile its sources, then link its product.
 */
static int
build_one_target(CFTypeRef objects, CFTypeRef chosen, const char *source_root,
    const struct pathmap *map, settings_table *t, const xcodebuild_opts *opts,
    const char *devpath)
{
	char clang[PATH_MAX];
	char build_dir[PATH_MAX], obj_dir[PATH_MAX], product[PATH_MAX];
	const char *product_name, *configuration, *sdkroot, *srcroot;
	struct product prod;
	char cfgbuf[128], pnbuf[256], sdkbuf[PATH_MAX];
	char srbuf[PATH_MAX], fullbuf[PATH_MAX];
	CFIndex i;
	int rc = 0, nsources = 0;

	/* What this target builds, and under what name. */
	{
		char ptbuf[128];

		classify_product(bstr(bget(chosen, "productType"), ptbuf,
		    sizeof(ptbuf)), &prod);
	}

	configuration = setting(t, "CONFIGURATION", cfgbuf, sizeof(cfgbuf));
	if (configuration == NULL)
		configuration = "Release";
	product_name = setting(t, "PRODUCT_NAME", pnbuf, sizeof(pnbuf));
	if (product_name == NULL)
		product_name = "product";

	snprintf(build_dir, sizeof(build_dir), "%s/build/%s", source_root,
	    configuration);
	snprintf(obj_dir, sizeof(obj_dir), "%s/build/%s.build", source_root,
	    configuration);
	{
		const char *full = setting(t, "FULL_PRODUCT_NAME", fullbuf,
		    sizeof(fullbuf));

		if (full == NULL)
			full = product_name;

		/*
		 * A bundle's binary lives inside it.  On macOS that is
		 * Contents/MacOS for an application and Versions/A for a
		 * framework; only the application form is assembled here.
		 */
		if (prod.kind == PRODUCT_BUNDLE)
			snprintf(product, sizeof(product),
			    "%s/%s/Contents/MacOS/%s", build_dir, full,
			    product_name);
		else
			snprintf(product, sizeof(product), "%s/%s", build_dir,
			    full);
	}

	if (mkdirs(build_dir) != 0 || mkdirs(obj_dir) != 0) {
		fprintf(stderr, "xcodebuild: error: cannot create %s\n", build_dir);
		rc = 1;
		goto out;
	}

	snprintf(clang, sizeof(clang),
	    "%s/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang", devpath);
	if (access(clang, X_OK) != 0) {
		fprintf(stderr, "xcodebuild: error: no clang at %s\n", clang);
		rc = 1;
		goto out;
	}

	/*
	 * SDKROOT may be a path or a name -- a project commonly sets it to
	 * "auto", meaning "whichever SDK the platform provides" -- so a
	 * value that is not a path is resolved, and SDK_DIR, which is
	 * always the resolved default, is the fallback.
	 */
	srcroot = setting(t, "SRCROOT", srbuf, sizeof(srbuf));
	if (srcroot == NULL)
		srcroot = source_root;

	sdkroot = setting(t, "SDKROOT", sdkbuf, sizeof(sdkbuf));
	if (sdkroot == NULL || sdkroot[0] != '/')
		sdkroot = setting(t, "SDK_DIR", sdkbuf, sizeof(sdkbuf));
	if (sdkroot != NULL && sdkroot[0] != '/')
		sdkroot = NULL;

	/*
	 * The SDK bundles this tree emits carry no headers yet, and a
	 * sysroot without them fails on the first #include with an error
	 * about stdlib.h that says nothing about why.  Name the actual
	 * problem instead.
	 */
	if (sdkroot != NULL && *sdkroot != '\0') {
		char inc[PATH_MAX];
		struct stat st;

		/*
		 * Tested by a header that must be there, not by the
		 * directory: the bundles this tree emits contain an empty
		 * usr/include, so its presence proves nothing.
		 */
		snprintf(inc, sizeof(inc), "%s/usr/include/stdlib.h", sdkroot);
		if (stat(inc, &st) != 0) {
			fprintf(stderr, "xcodebuild: error: the SDK at %s has"
			    " no headers.\n", sdkroot);
			fprintf(stderr, "xcodebuild: error: this tree emits SDK"
			    " bundles without contents; point SDKROOT or\n"
			    "xcodebuild: error: DEVELOPER_DIR at an SDK that"
			    " has them.\n");
			rc = 1;
			goto out;
		}
	}

	/* Compile every source in the target's sources phase. */
	{
		CFTypeRef phases = bget(chosen, "buildPhases");
		char *objs[256];
		char *swifts[256];
		int nobjs = 0, nswift = 0;
		CFIndex p;

		for (p = 0; p < bcount(phases) && rc == 0; p++) {
			CFTypeRef phase = bderef(objects, bat(phases, p));
			char isabuf[64];
			const char *isa = bstr(bget(phase, "isa"), isabuf,
			    sizeof(isabuf));
			CFTypeRef files;
			CFIndex f;

			if (isa == NULL ||
			    strcmp(isa, "PBXSourcesBuildPhase") != 0)
				continue;

			files = bget(phase, "files");
			for (f = 0; f < bcount(files) && rc == 0; f++) {
				CFTypeRef bf = bderef(objects, bat(files, f));
				char refbuf[512];
				const char *ref = bstr(bget(bf, "fileRef"),
				    refbuf, sizeof(refbuf));
				const char *src;
				char obj[PATH_MAX], *argv[256];
				const char *base;
				int a = 0;

				if (ref == NULL)
					continue;
				if ((src = pathmap_get(map, ref)) == NULL)
					continue;
				/*
				 * Swift is compiled as a module, not a
				 * file at a time, so its sources are
				 * gathered and handed to swiftc together
				 * once the phase has been walked.
				 */
				if (is_swift(src)) {
					if (nswift < (int)(sizeof(swifts) /
					    sizeof(swifts[0])))
						swifts[nswift++] = strdup(src);
					nsources++;
					continue;
				}

				if (!is_compilable(src))
					continue;

				base = strrchr(src, '/');
				base = (base != NULL) ? base + 1 : src;
				snprintf(obj, sizeof(obj), "%s/%s.o", obj_dir, base);

				argv[a++] = clang;
				argv[a++] = (char *)"-c";
				if (sdkroot != NULL && *sdkroot != '\0') {
					argv[a++] = (char *)"-isysroot";
					argv[a++] = (char *)sdkroot;
				}

				/* What the project asks the compiler for. */
				a = add_setting_args(argv, a, 240,
				    settings_get(t, "HEADER_SEARCH_PATHS"),
				    "-I", srcroot);
				a = add_setting_args(argv, a, 240,
				    settings_get(t, "USER_HEADER_SEARCH_PATHS"),
				    "-I", srcroot);
				a = add_setting_args(argv, a, 240,
				    settings_get(t, "FRAMEWORK_SEARCH_PATHS"),
				    "-F", srcroot);
				a = add_setting_args(argv, a, 240,
				    settings_get(t, "GCC_PREPROCESSOR_DEFINITIONS"),
				    "-D", NULL);

				argv[a++] = (char *)"-o";
				argv[a++] = obj;
				argv[a++] = (char *)src;
				argv[a] = NULL;

				printf("CompileC %s\n", src);
				if (run(argv, opts->verbose) != 0) {
					fprintf(stderr, "xcodebuild: error:"
					    " failed to compile %s\n", src);
					rc = 1;
					break;
				}

				if (nobjs < (int)(sizeof(objs) / sizeof(objs[0])))
					objs[nobjs++] = strdup(obj);
				nsources++;
			}
		}

		/*
		 * The Swift half of the target, compiled whole.  A Swift
		 * module is one translation unit however many files it is
		 * written across -- the files can refer to each other
		 * without declarations -- so they go to swiftc together
		 * and come back as a single object.
		 */
		if (rc == 0 && nswift > 0) {
			char swiftc[PATH_MAX], obj[PATH_MAX], modbuf[256];
			char *argv[280];
			const char *module;
			int a = 0, j;

			snprintf(swiftc, sizeof(swiftc),
			    "%s/Toolchains/XcodeDefault.xctoolchain/usr/bin/swiftc",
			    devpath);

			if (access(swiftc, X_OK) != 0) {
				fprintf(stderr, "xcodebuild: error: this target"
				    " has Swift sources but there is no swiftc"
				    " at %s\n", swiftc);
				rc = 1;
			} else {
				module = setting(t, "PRODUCT_MODULE_NAME",
				    modbuf, sizeof(modbuf));
				if (module == NULL)
					module = product_name;

				snprintf(obj, sizeof(obj), "%s/%s.swift.o",
				    obj_dir, module);

				argv[a++] = swiftc;
				if (sdkroot != NULL && *sdkroot != '\0') {
					argv[a++] = (char *)"-sdk";
					argv[a++] = (char *)sdkroot;
				}
				argv[a++] = (char *)"-module-name";
				argv[a++] = (char *)module;
				argv[a++] = (char *)"-wmo";
				argv[a++] = (char *)"-emit-object";
				argv[a++] = (char *)"-o";
				argv[a++] = obj;
				for (j = 0; j < nswift && a < 270; j++)
					argv[a++] = swifts[j];
				argv[a] = NULL;

				printf("CompileSwift %s (%d file%s)\n", module,
				    nswift, (nswift == 1) ? "" : "s");
				if (run(argv, opts->verbose) != 0) {
					fprintf(stderr, "xcodebuild: error:"
					    " failed to compile the Swift"
					    " sources of %s\n", module);
					rc = 1;
				} else if (nobjs < (int)(sizeof(objs) /
				    sizeof(objs[0]))) {
					objs[nobjs++] = strdup(obj);
				}
			}
		}

		if (rc == 0 && nobjs > 0) {
			char *argv[512];
			char libtool[PATH_MAX];
			int a = 0, j;

			/* A bundle's binary sits in a directory of its own. */
			if (prod.kind == PRODUCT_BUNDLE) {
				char dir[PATH_MAX];
				const char *sl = strrchr(product, '/');

				snprintf(dir, sizeof(dir), "%.*s",
				    (int)(sl - product), product);
				if (mkdirs(dir) != 0) {
					fprintf(stderr, "xcodebuild: error:"
					    " cannot create %s\n", dir);
					rc = 1;
				}
			}

			if (rc == 0 && prod.kind == PRODUCT_STATIC_LIB) {
				/*
				 * An archive, not a link.  libtool is what
				 * Xcode runs for a static library, and it is
				 * in the toolchain beside clang.
				 */
				snprintf(libtool, sizeof(libtool),
				    "%s/Toolchains/XcodeDefault.xctoolchain"
				    "/usr/bin/libtool", devpath);

				argv[a++] = libtool;
				argv[a++] = (char *)"-static";
				argv[a++] = (char *)"-o";
				argv[a++] = product;
				for (j = 0; j < nobjs; j++)
					argv[a++] = objs[j];
				argv[a] = NULL;

				printf("Libtool %s\n", product);
			} else if (rc == 0) {
				/*
				 * A target with Swift in it is linked by
				 * swiftc, which knows to bring in the Swift
				 * runtime and the standard library; clang
				 * would leave those symbols undefined.
				 */
				char swiftc[PATH_MAX];

				if (nswift > 0) {
					snprintf(swiftc, sizeof(swiftc),
					    "%s/Toolchains/XcodeDefault"
					    ".xctoolchain/usr/bin/swiftc",
					    devpath);
					argv[a++] = swiftc;
					if (prod.kind == PRODUCT_DYNAMIC_LIB)
						argv[a++] = (char *)"-emit-library";
					if (sdkroot != NULL && *sdkroot != '\0') {
						argv[a++] = (char *)"-sdk";
						argv[a++] = (char *)sdkroot;
					}
					goto have_linker;
				}

				argv[a++] = clang;
				if (prod.kind == PRODUCT_DYNAMIC_LIB)
					argv[a++] = (char *)"-dynamiclib";
				if (sdkroot != NULL && *sdkroot != '\0') {
					argv[a++] = (char *)"-isysroot";
					argv[a++] = (char *)sdkroot;
				}
have_linker:
				argv[a++] = (char *)"-o";
				argv[a++] = product;
				for (j = 0; j < nobjs; j++)
					argv[a++] = objs[j];

				/*
				 * What the project says this target links
				 * against, after its own objects: a static
				 * library only resolves symbols for objects
				 * already on the line.
				 */
				a = add_link_inputs(argv, a,
				    (int)(sizeof(argv) / sizeof(argv[0])) - 1,
				    objects, chosen, map, build_dir, sdkroot,
				    devpath, NULL);
				argv[a] = NULL;

				printf("Ld %s\n", product);
			}

			if (rc == 0 && run(argv, opts->verbose) != 0) {
				fprintf(stderr, "xcodebuild: error: link failed\n");
				rc = 1;
			}

			/*
			 * An application is a bundle: without an Info.plist
			 * naming its executable, the binary is there but
			 * nothing will launch it.
			 */
			if (rc == 0 && prod.kind == PRODUCT_BUNDLE &&
			    prod.wrapper != NULL &&
			    strcmp(prod.wrapper, "app") == 0)
				rc = write_app_infoplist(build_dir,
				    settings_get(t, "FULL_PRODUCT_NAME"),
				    product_name, t);
		}

		for (i = 0; i < nobjs; i++)
			free(objs[i]);
		for (i = 0; i < nswift; i++)
			free(swifts[i]);
	}

	if (rc == 0 && nsources == 0) {
		fprintf(stderr, "xcodebuild: error: target has no sources this"
		    " tool can compile\n");
		rc = 1;
	}

out:
	return rc;
}

/*
 * Build a target and everything it depends on.
 *
 * The dependencies are built first and in order, so that a target
 * linking another target's product finds it already made.  Each is
 * built with its own settings: what a target produces and what it is
 * called are its own, not the settings of whatever asked for it.
 */
int build_run(const char *project, settings_table *t,
    const xcodebuild_opts *opts, const char *devpath)
{
	CFTypeRef root, objects = NULL, root_id = NULL, project_obj, targets;
	CFTypeRef chosen = NULL;
	struct pathmap map;
	struct idlist order, stack;
	char source_root[PATH_MAX], chosen_id[512] = "";
	const char *slash;
	char name_buf[512];
	CFIndex i;
	size_t k;
	int rc = 0, foreign = 0;

	if (project == NULL) {
		fprintf(stderr, "xcodebuild: error: no project to build\n");
		return 1;
	}

	if ((root = project_load_pbxproj(project)) == NULL) {
		fprintf(stderr, "xcodebuild: error: could not read project '%s'\n",
		    project);
		return 1;
	}

	memset(&map, 0, sizeof(map));
	memset(&order, 0, sizeof(order));
	memset(&stack, 0, sizeof(stack));

	/* Paths in a project are relative to the directory holding it. */
	snprintf(source_root, sizeof(source_root), "%s", project);
	if ((slash = strrchr(source_root, '/')) != NULL)
		*(char *)slash = '\0';

	objects = bget(root, "objects");
	root_id = bget(root, "rootObject");
	project_obj = bderef(objects, root_id);
	targets = bget(project_obj, "targets");

	/* The named target, or the first one, as everywhere else. */
	for (i = 0; i < bcount(targets); i++) {
		CFTypeRef tid = bat(targets, i);
		CFTypeRef tobj = bderef(objects, tid);
		const char *name = bstr(bget(tobj, "name"), name_buf,
		    sizeof(name_buf));

		if (opts->target != NULL && name != NULL &&
		    strcmp(name, opts->target) == 0) {
			chosen = tobj;
			bstr(tid, chosen_id, sizeof(chosen_id));
			break;
		}
		if (opts->target == NULL && chosen == NULL) {
			chosen = tobj;
			bstr(tid, chosen_id, sizeof(chosen_id));
		}
	}

	if (chosen == NULL) {
		fprintf(stderr, "xcodebuild: error: target '%s' not found\n",
		    (opts->target != NULL) ? opts->target : "(default)");
		CFRelease(root);
		return 1;
	}

	walk_group(objects, bderef(objects, bget(project_obj, "mainGroup")),
	    source_root, source_root, &map);

	rc = order_targets(objects, project_obj, chosen_id, &order, &stack,
	    &foreign);

	if (rc == 0 && foreign > 0)
		fprintf(stderr, "xcodebuild: warning: %d dependenc%s on a"
		    " target in another project %s not built here\n",
		    foreign, (foreign == 1) ? "y" : "ies",
		    (foreign == 1) ? "was" : "were");

	for (k = 0; k < order.count && rc == 0; k++) {
		CFTypeRef tobj = bget(objects, order.ids[k]);
		settings_table *ts = t;
		char isabuf[64];
		const char *isa = bstr(bget(tobj, "isa"), isabuf,
		    sizeof(isabuf));
		const char *name = bstr(bget(tobj, "name"), name_buf,
		    sizeof(name_buf));

		/*
		 * An aggregate target builds nothing of its own; it exists
		 * to gather the targets it depends on, which have been
		 * built by the time this reaches it.
		 */
		if (isa != NULL && strcmp(isa, "PBXNativeTarget") != 0)
			continue;

		if (order.count > 1 && name != NULL)
			printf("=== BUILD TARGET %s ===\n", name);

		/*
		 * The settings in hand belong to the target that was
		 * asked for; a dependency needs its own.
		 */
		if (strcmp(order.ids[k], chosen_id) != 0 && name != NULL) {
			ts = xbuild_settings_for_target(opts, devpath, name);
			if (ts == NULL) {
				fprintf(stderr, "xcodebuild: error: cannot"
				    " resolve the settings of '%s'\n", name);
				rc = 1;
				break;
			}
		}

		rc = build_one_target(objects, tobj, source_root, &map, ts,
		    opts, devpath);

		if (ts != t)
			settings_destroy(ts);
	}

	if (rc == 0)
		printf("\n** BUILD SUCCEEDED **\n\n");
	else
		printf("\n** BUILD FAILED **\n\n");

	idlist_free(&order);
	idlist_free(&stack);
	pathmap_free(&map);
	CFRelease(root);
	return rc;
}
