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

#include <dirent.h>
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
 * A project states this two ways.  The explicit form is a
 * PBXTargetDependency naming another target, and is always followed.
 * The implicit form is a target linking another target's product;
 * xcodebuild finds those only when it builds a scheme, which is where
 * the option to look for them lives, and building -target A against a
 * project whose B it links but does not depend on fails the same way
 * Apple's does.
 *
 * A dependency on a target in another project is recognised and
 * reported rather than followed: building it means building that
 * project too, which is a larger thing than this does.
 */
static void
collect_deps(CFTypeRef objects, CFTypeRef project_obj, CFTypeRef target,
    struct idlist *out, int *foreign, int follow_implicit)
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

	if (!follow_implicit)
		return;

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
    struct idlist *order, struct idlist *stack, int *foreign,
    int follow_implicit)
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
	collect_deps(objects, project_obj, target, &deps, foreign,
	    follow_implicit);

	for (i = 0; i < deps.count && rc == 0; i++)
		rc = order_targets(objects, project_obj, deps.ids[i], order,
		    stack, foreign, follow_implicit);

	idlist_free(&deps);
	idlist_drop_last(stack);

	if (rc == 0 && !idlist_has(order, id))
		idlist_add(order, id);

	return rc;
}

/* ------------------------------------------------------------------ */
/* link inputs                                                          */
/* ------------------------------------------------------------------ */

/*
 * An attribute on a build file, set by the phase that holds it: Weak
 * on something linked, Public or Private on a header.
 */
static int
build_file_has_attr(CFTypeRef bf, const char *want)
{
	CFTypeRef attrs = bget(bget(bf, "settings"), "ATTRIBUTES");
	CFIndex i;

	for (i = 0; i < bcount(attrs); i++) {
		char buf[64];
		const char *a = bstr(bat(attrs, i), buf, sizeof(buf));

		if (a != NULL && strcasecmp(a, want) == 0)
			return 1;
	}

	return 0;
}

static int
build_file_is_weak(CFTypeRef bf)
{
	return build_file_has_attr(bf, "Weak");
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
 * A default, where a value that is present but empty does not count as
 * set.  The settings table starts with some keys already there and
 * empty -- EXECUTABLE_NAME is one -- and settings_defaults_set would
 * leave those empty for ever.
 */
static void
settings_default_if_empty(settings_table *t, const char *key,
    const char *value)
{
	const char *cur = settings_get(t, key);

	if (cur == NULL || *cur == '\0')
		settings_set(t, key, value);
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

	/*
	 * Where the product installs, where its binary sits inside it,
	 * and -- for a dylib -- the name it records for whoever links it.
	 *
	 * Defaults, so a project that says otherwise keeps what it said.
	 * Read back from Apple's -showBuildSettings: a tool installs to
	 * /usr/local/bin and a library to /usr/local/lib, a framework to
	 * /Library/Frameworks with an executable path of
	 * Foo.framework/Versions/A/Foo, and DYLIB_INSTALL_NAME_BASE is
	 * the install path -- not @rpath, which is what a project sets
	 * when it wants one that can be found anywhere.
	 */
	{
		const char *ver = settings_get(t, "FRAMEWORK_VERSION");
		char exe[PATH_MAX], epath[PATH_MAX];
		int framework;

		if (ver == NULL || *ver == '\0')
			ver = "A";

		framework = (prod.kind == PRODUCT_BUNDLE &&
		    prod.wrapper != NULL &&
		    strcmp(prod.wrapper, "framework") == 0);

		snprintf(exe, sizeof(exe), "%s%s%s", prod.prefix, pn,
		    prod.suffix);
		snprintf(epath, sizeof(epath), "%s", exe);

		/* Reported for every target, framework or not, as Apple does. */
		settings_default_if_empty(t, "FRAMEWORK_VERSION", ver);

		if (framework) {
			settings_default_if_empty(t, "INSTALL_PATH",
			    "/Library/Frameworks");
			snprintf(epath, sizeof(epath), "%s/Versions/%s/%s",
			    full, ver, exe);
		} else if (prod.kind == PRODUCT_BUNDLE) {
			settings_default_if_empty(t, "INSTALL_PATH", "/Applications");
			snprintf(epath, sizeof(epath), "%s/Contents/MacOS/%s",
			    full, exe);
		} else if (prod.kind == PRODUCT_TOOL) {
			settings_default_if_empty(t, "INSTALL_PATH", "/usr/local/bin");
		} else {
			settings_default_if_empty(t, "INSTALL_PATH", "/usr/local/lib");
		}

		settings_default_if_empty(t, "EXECUTABLE_NAME", exe);
		settings_default_if_empty(t, "EXECUTABLE_PATH", epath);

		if (framework || prod.kind == PRODUCT_DYNAMIC_LIB) {
			const char *base = settings_get(t, "INSTALL_PATH");
			char iname[PATH_MAX];

			if (base != NULL)
				settings_default_if_empty(t,
				    "DYLIB_INSTALL_NAME_BASE", base);

			base = settings_get(t, "DYLIB_INSTALL_NAME_BASE");
			if (base != NULL) {
				snprintf(iname, sizeof(iname), "%s/%s", base,
				    epath);
				settings_default_if_empty(t,
				    "LD_DYLIB_INSTALL_NAME", iname);
			}
		}
	}
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

/* A string into a property list, left out entirely when there is none. */
static void
plist_set_str(CFMutableDictionaryRef d, CFStringRef key, const char *value)
{
	CFStringRef v;

	if (value == NULL || *value == '\0')
		return;

	v = CFStringCreateWithCString(NULL, value, kCFStringEncodingUTF8);
	if (v != NULL) {
		CFDictionarySetValue(d, key, v);
		CFRelease(v);
	}
}

/*
 * An Info.plist built from the settings.
 *
 * Only what the settings actually say.  Apple's generated plist also
 * carries the build's provenance -- DTXcode, the SDK's build number
 * and so on -- which this leaves out rather than claiming to be a
 * version of Xcode it is not.  A key the project has not set is
 * omitted, the identifier above all: a bundle identifier is identity,
 * and inventing one from the target name gives the bundle a name
 * nothing agreed to, which anything looking a bundle up by identifier
 * would then believe.
 */
static int
write_bundle_infoplist(const char *path, const char *exe_name,
    const char *package_type, settings_table *t)
{
	CFMutableDictionaryRef info;
	CFMutableArrayRef platforms;
	char buf[PATH_MAX];
	const char *region;
	CFDataRef data;
	FILE *fp;
	int rc = 0;

	if (path == NULL || exe_name == NULL)
		return 1;

	info = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	if (info == NULL)
		return 1;

	plist_set_str(info, CFSTR("CFBundleExecutable"), exe_name);
	plist_set_str(info, CFSTR("CFBundleName"), exe_name);
	plist_set_str(info, CFSTR("CFBundlePackageType"), package_type);
	CFDictionarySetValue(info, CFSTR("CFBundleInfoDictionaryVersion"),
	    CFSTR("6.0"));

	plist_set_str(info, CFSTR("CFBundleIdentifier"),
	    setting(t, "PRODUCT_BUNDLE_IDENTIFIER", buf, sizeof(buf)));

	if ((region = setting(t, "DEVELOPMENT_LANGUAGE", buf, sizeof(buf)))
	    == NULL)
		region = "en";
	plist_set_str(info, CFSTR("CFBundleDevelopmentRegion"), region);

	plist_set_str(info, CFSTR("LSMinimumSystemVersion"),
	    setting(t, "MACOSX_DEPLOYMENT_TARGET", buf, sizeof(buf)));

	/* This tree builds for macOS, and says so rather than guessing. */
	platforms = CFArrayCreateMutable(NULL, 1, &kCFTypeArrayCallBacks);
	if (platforms != NULL) {
		CFArrayAppendValue(platforms, CFSTR("MacOSX"));
		CFDictionarySetValue(info, CFSTR("CFBundleSupportedPlatforms"),
		    platforms);
		CFRelease(platforms);
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

/* ------------------------------------------------------------------ */
/* frameworks                                                           */
/* ------------------------------------------------------------------ */

static int
copy_file(const char *src, const char *dst)
{
	char buf[65536];
	FILE *in, *out;
	size_t n;
	int rc = 0;

	if ((in = fopen(src, "rb")) == NULL)
		return -1;
	if ((out = fopen(dst, "wb")) == NULL) {
		fclose(in);
		return -1;
	}

	while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
		if (fwrite(buf, 1, n, out) != n) {
			rc = -1;
			break;
		}
	}
	if (ferror(in))
		rc = -1;

	fclose(in);
	fclose(out);
	return rc;
}

/* A file, or a directory and everything under it. */
static int
copy_tree(const char *src, const char *dst)
{
	struct stat st;
	struct dirent *e;
	DIR *d;
	int rc = 0;

	if (stat(src, &st) != 0)
		return -1;

	if (!S_ISDIR(st.st_mode))
		return copy_file(src, dst);

	if (mkdirs(dst) != 0)
		return -1;

	if ((d = opendir(src)) == NULL)
		return -1;

	while (rc == 0 && (e = readdir(d)) != NULL) {
		char s[PATH_MAX], t[PATH_MAX];

		if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
			continue;

		snprintf(s, sizeof(s), "%s/%s", src, e->d_name);
		snprintf(t, sizeof(t), "%s/%s", dst, e->d_name);
		rc = copy_tree(s, t);
	}

	closedir(d);
	return rc;
}

/*
 * A .strings file, in the encoding a bundle expects.
 *
 * These are converted rather than copied.
 * STRINGS_FILE_OUTPUT_ENCODING says to what, and has been UTF-16 for
 * as long as anyone has been writing them: little-endian, with a byte
 * order mark, which is what Apple emits and what CFBundle reads back.
 * A file that already has a mark is already UTF-16 and is read as
 * such; anything else is read as UTF-8.
 */
static int
copy_strings_file(const char *src, const char *dst, settings_table *t)
{
	char encbuf[64];
	const char *want = setting(t, "STRINGS_FILE_OUTPUT_ENCODING", encbuf,
	    sizeof(encbuf));
	CFStringEncoding in = kCFStringEncodingUTF8;
	CFDataRef data, out;
	CFStringRef str;
	unsigned char *buf;
	long len;
	FILE *fp;
	int utf8, rc = 0;

	if ((fp = fopen(src, "rb")) == NULL)
		return -1;

	if (fseek(fp, 0, SEEK_END) != 0 || (len = ftell(fp)) < 0) {
		fclose(fp);
		return -1;
	}
	rewind(fp);

	if ((buf = malloc((size_t)len + 1)) == NULL) {
		fclose(fp);
		return -1;
	}
	if (len > 0 && fread(buf, 1, (size_t)len, fp) != (size_t)len) {
		free(buf);
		fclose(fp);
		return -1;
	}
	fclose(fp);

	if (len >= 2 && ((buf[0] == 0xff && buf[1] == 0xfe) ||
	    (buf[0] == 0xfe && buf[1] == 0xff)))
		in = kCFStringEncodingUnicode;

	data = CFDataCreate(NULL, buf, (CFIndex)len);
	free(buf);
	if (data == NULL)
		return -1;

	str = CFStringCreateFromExternalRepresentation(NULL, data, in);
	CFRelease(data);
	if (str == NULL) {
		fprintf(stderr, "xcodebuild: error: cannot read %s as text\n",
		    src);
		return -1;
	}

	utf8 = (want != NULL && strcasecmp(want, "UTF-8") == 0);
	out = CFStringCreateExternalRepresentation(NULL, str,
	    utf8 ? kCFStringEncodingUTF8 : kCFStringEncodingUTF16LE, 0);
	CFRelease(str);
	if (out == NULL)
		return -1;

	if ((fp = fopen(dst, "wb")) == NULL) {
		rc = -1;
	} else {
		if (!utf8)
			fwrite("\xff\xfe", 1, 2, fp);
		fwrite(CFDataGetBytePtr(out), 1, (size_t)CFDataGetLength(out),
		    fp);
		fclose(fp);
	}

	CFRelease(out);
	return rc;
}

/*
 * A resource that is built rather than copied.
 *
 * An interface file, an asset catalogue and a data model are compiled
 * by tools this tree does not have.  Copying one in uncompiled would
 * produce a bundle that looks built and does not work, so the build
 * stops and says which tool is missing.
 */
static const char *
resource_needs_tool(const char *path)
{
	if (has_ext(path, ".xib") || has_ext(path, ".storyboard"))
		return "ibtool";
	if (has_ext(path, ".xcassets"))
		return "actool";
	if (has_ext(path, ".xcdatamodel") || has_ext(path, ".xcdatamodeld"))
		return "momc";
	if (has_ext(path, ".intentdefinition"))
		return "intentbuilderc";

	return NULL;
}

/*
 * Install one resource.
 *
 * Resources are flattened into one directory: a project's groups are
 * a way of organising the navigator and are not reproduced in the
 * bundle.  A .lproj directory is the exception -- it names the
 * language a resource is for, and the bundle finds it by that name --
 * and a reference to a directory is copied whole.
 */
static int
install_one_resource(const struct pathmap *map, const char *ref,
    const char *build_dir, const char *sdkroot, const char *devpath,
    const char *res_dir, settings_table *t, int *ncopied)
{
	char resbuf[PATH_MAX], dir[PATH_MAX], dst[PATH_MAX];
	const char *src, *base, *tool;
	struct stat st;

	src = pathmap_resolve(map, ref, build_dir, sdkroot, devpath, resbuf,
	    sizeof(resbuf));
	if (src == NULL) {
		fprintf(stderr, "xcodebuild: error: cannot find the resource"
		    " '%s'\n", ref);
		return -1;
	}

	if ((tool = resource_needs_tool(src)) != NULL) {
		fprintf(stderr, "xcodebuild: error: %s has to be compiled by"
		    " %s, which this tree does not build yet\n", src, tool);
		return -1;
	}

	if (stat(src, &st) != 0) {
		fprintf(stderr, "xcodebuild: error: no resource at %s\n", src);
		return -1;
	}

	base = strrchr(src, '/');
	base = (base != NULL) ? base + 1 : src;

	/* Keep the .lproj a resource sits in, and nothing above it. */
	snprintf(dir, sizeof(dir), "%s", res_dir);
	if (base > src) {
		char parent[PATH_MAX];
		const char *pbase;

		snprintf(parent, sizeof(parent), "%.*s",
		    (int)(base - src - 1), src);
		pbase = strrchr(parent, '/');
		pbase = (pbase != NULL) ? pbase + 1 : parent;

		if (has_ext(pbase, ".lproj"))
			snprintf(dir, sizeof(dir), "%s/%s", res_dir, pbase);
	}

	if (mkdirs(dir) != 0) {
		fprintf(stderr, "xcodebuild: error: cannot create %s\n", dir);
		return -1;
	}

	snprintf(dst, sizeof(dst), "%s/%s", dir, base);

	if (!S_ISDIR(st.st_mode) && has_ext(src, ".strings")) {
		if (copy_strings_file(src, dst, t) != 0) {
			fprintf(stderr, "xcodebuild: error: cannot convert"
			    " %s\n", src);
			return -1;
		}

		(*ncopied)++;
		return 0;
	}

	if (copy_tree(src, dst) != 0) {
		fprintf(stderr, "xcodebuild: error: cannot copy %s\n", src);
		return -1;
	}

	(*ncopied)++;
	return 0;
}

/*
 * What a bundle carries besides its binary.
 *
 * A variant group stands for one resource in several languages, and
 * holds a file per language; the phase names the group, so each of its
 * children is installed in turn.
 */
static int
install_bundle_resources(CFTypeRef objects, CFTypeRef target,
    const struct pathmap *map, const char *build_dir, const char *sdkroot,
    const char *devpath, const char *res_dir, settings_table *t,
    int *ncopied)
{
	CFTypeRef phases = bget(target, "buildPhases");
	CFIndex p;
	int rc = 0;

	*ncopied = 0;

	for (p = 0; p < bcount(phases) && rc == 0; p++) {
		CFTypeRef phase = bderef(objects, bat(phases, p));
		char isabuf[64];
		const char *isa = bstr(bget(phase, "isa"), isabuf,
		    sizeof(isabuf));
		CFTypeRef files;
		CFIndex f;

		if (isa == NULL || strcmp(isa, "PBXResourcesBuildPhase") != 0)
			continue;

		files = bget(phase, "files");
		for (f = 0; f < bcount(files) && rc == 0; f++) {
			CFTypeRef bf = bderef(objects, bat(files, f));
			CFTypeRef fref;
			char refbuf[512], rabuf[64];
			const char *ref = bstr(bget(bf, "fileRef"), refbuf,
			    sizeof(refbuf));
			const char *rasa;

			if (ref == NULL)
				continue;

			fref = bget(objects, ref);
			rasa = bstr(bget(fref, "isa"), rabuf, sizeof(rabuf));

			if (rasa != NULL && strstr(rasa, "Group") != NULL) {
				CFTypeRef kids = bget(fref, "children");
				CFIndex k;

				for (k = 0; k < bcount(kids) && rc == 0; k++) {
					char kb[512];
					const char *kid = bstr(bat(kids, k), kb,
					    sizeof(kb));

					if (kid != NULL)
						rc = install_one_resource(map,
						    kid, build_dir, sdkroot,
						    devpath, res_dir, t,
						    ncopied);
				}
				continue;
			}

			rc = install_one_resource(map, ref, build_dir, sdkroot,
			    devpath, res_dir, t, ncopied);
		}
	}

	return rc;
}

/* A symlink inside a bundle, replaced if one is already there. */
static int
relink(const char *dir, const char *name, const char *target)
{
	char path[PATH_MAX];

	snprintf(path, sizeof(path), "%s/%s", dir, name);
	unlink(path);

	return symlink(target, path);
}

/*
 * The headers a framework publishes.
 *
 * A header in the headers phase carries an attribute saying how it is
 * exposed: Public goes in Headers, Private in PrivateHeaders, and one
 * with neither is the target's own business and is not installed.
 */
static int
install_framework_headers(CFTypeRef objects, CFTypeRef target,
    const struct pathmap *map, const char *build_dir, const char *sdkroot,
    const char *devpath, const char *version_dir, int *npublic, int *nprivate)
{
	CFTypeRef phases = bget(target, "buildPhases");
	CFIndex p;
	int rc = 0;

	*npublic = 0;
	*nprivate = 0;

	for (p = 0; p < bcount(phases) && rc == 0; p++) {
		CFTypeRef phase = bderef(objects, bat(phases, p));
		char isabuf[64];
		const char *isa = bstr(bget(phase, "isa"), isabuf,
		    sizeof(isabuf));
		CFTypeRef files;
		CFIndex f;

		if (isa == NULL || strcmp(isa, "PBXHeadersBuildPhase") != 0)
			continue;

		files = bget(phase, "files");
		for (f = 0; f < bcount(files) && rc == 0; f++) {
			CFTypeRef bf = bderef(objects, bat(files, f));
			char refbuf[512], resbuf[PATH_MAX];
			char dir[PATH_MAX], dst[PATH_MAX];
			const char *ref = bstr(bget(bf, "fileRef"), refbuf,
			    sizeof(refbuf));
			const char *src, *base, *sub;

			if (ref == NULL)
				continue;

			if (build_file_has_attr(bf, "Public"))
				sub = "Headers";
			else if (build_file_has_attr(bf, "Private"))
				sub = "PrivateHeaders";
			else
				continue;

			src = pathmap_resolve(map, ref, build_dir, sdkroot,
			    devpath, resbuf, sizeof(resbuf));
			if (src == NULL)
				continue;

			snprintf(dir, sizeof(dir), "%s/%s", version_dir, sub);
			if (mkdirs(dir) != 0) {
				rc = -1;
				break;
			}

			base = strrchr(src, '/');
			base = (base != NULL) ? base + 1 : src;
			snprintf(dst, sizeof(dst), "%s/%s", dir, base);

			if (copy_file(src, dst) != 0) {
				fprintf(stderr, "xcodebuild: error: cannot"
				    " install the header %s\n", src);
				rc = -1;
				break;
			}

			if (strcmp(sub, "Headers") == 0)
				(*npublic)++;
			else
				(*nprivate)++;
		}
	}

	return rc;
}

/*
 * Turn a version directory into a framework.
 *
 * A macOS framework is versioned: the binary, its headers and its
 * resources live under Versions/A, and the top of the bundle is
 * symlinks naming whichever version is current.  Without them the
 * directory holds everything a framework has and still cannot be
 * linked against, because -framework looks for the binary at the top.
 */
static int
framework_finalize(const char *bundle, const char *version,
    const char *exe_name, int have_headers, int have_private,
    int have_resources)
{
	char versions[PATH_MAX], target[PATH_MAX];

	snprintf(versions, sizeof(versions), "%s/Versions", bundle);

	if (relink(versions, "Current", version) != 0)
		return -1;

	snprintf(target, sizeof(target), "Versions/Current/%s", exe_name);
	if (relink(bundle, exe_name, target) != 0)
		return -1;

	if (have_resources &&
	    relink(bundle, "Resources", "Versions/Current/Resources") != 0)
		return -1;

	if (have_headers &&
	    relink(bundle, "Headers", "Versions/Current/Headers") != 0)
		return -1;

	if (have_private &&
	    relink(bundle, "PrivateHeaders",
	    "Versions/Current/PrivateHeaders") != 0)
		return -1;

	return 0;
}

/*
 * The eight bytes an application carries beside its Info.plist: its
 * package type and its creator signature, written without a newline.
 *
 * A creator code is four characters registered with Apple in an
 * arrangement that has not meant anything for years, so "????" -- the
 * "no creator" value, and what Apple writes -- stands unless the
 * project's own Info.plist names one.  A framework gets no PkgInfo.
 */
static int
write_pkginfo(const char *dst, const char *package_type, settings_table *t)
{
	char buf[64];
	const char *gen;
	FILE *fp;

	gen = setting(t, "GENERATE_PKGINFO_FILE", buf, sizeof(buf));
	if (gen != NULL && strcasecmp(gen, "NO") == 0)
		return 0;

	if ((fp = fopen(dst, "wb")) == NULL) {
		fprintf(stderr, "xcodebuild: error: cannot write %s\n", dst);
		return 1;
	}

	fprintf(fp, "%s????", package_type);
	fclose(fp);

	return 0;
}

/*
 * Whether the bundle gets an Info.plist at all.
 *
 * A project supplies one with INFOPLIST_FILE, or asks for one to be
 * written with GENERATE_INFOPLIST_FILE.  With neither, none is: a
 * bundle built here has what the project asked for and nothing else.
 * Apple refuses to code sign a bundle without one, which is a matter
 * for whatever signs it rather than a reason to invent the file.
 */
static int
infoplist_wanted(settings_table *t)
{
	char buf[PATH_MAX];
	const char *gen;

	if (setting(t, "INFOPLIST_FILE", buf, sizeof(buf)) != NULL)
		return 1;

	gen = setting(t, "GENERATE_INFOPLIST_FILE", buf, sizeof(buf));

	return gen != NULL && strcasecmp(gen, "YES") == 0;
}

/*
 * The bundle's Info.plist.
 *
 * A project that supplies one means it: it carries the identifier, the
 * version and everything else the bundle is meant to say about itself,
 * none of which a synthesised one knows.  Only when there is no such
 * file is one written from the settings.
 */
static int
install_infoplist(const char *dst, const char *exe_name,
    const char *package_type, settings_table *t, const char *srcroot)
{
	char ibuf[PATH_MAX], src[PATH_MAX];
	const char *given = setting(t, "INFOPLIST_FILE", ibuf, sizeof(ibuf));

	if (given == NULL)
		return write_bundle_infoplist(dst, exe_name, package_type, t);

	if (given[0] == '/')
		snprintf(src, sizeof(src), "%s", given);
	else
		snprintf(src, sizeof(src), "%s/%s", srcroot, given);

	if (copy_file(src, dst) != 0) {
		fprintf(stderr, "xcodebuild: error: cannot read the Info.plist"
		    " at %s\n", src);
		return 1;
	}

	return 0;
}

/*
 * Split a build setting into arguments, each prefixed with a flag.
 *
 * Search paths and preprocessor definitions are single settings holding
 * several whitespace-separated values, quoted where a value contains a
 * space.  A relative path is relative to the project, which is what a
 * project means when it writes DCE/include.
 */
/*
 * The next whitespace-separated word of a setting, honouring the quotes
 * around a value that contains a space.  Returns 0 when none are left.
 */
static int
next_word(const char **pp, char *word, size_t len)
{
	const char *p = *pp;
	size_t n = 0;
	char quote = '\0';

	while (*p == ' ' || *p == '\t')
		p++;

	if (*p == '\0') {
		*pp = p;
		return 0;
	}

	if (*p == '"' || *p == '\'')
		quote = *p++;

	while (*p != '\0' && n + 1 < len) {
		if (quote != '\0' && *p == quote) {
			p++;
			break;
		}
		if (quote == '\0' && (*p == ' ' || *p == '\t'))
			break;
		word[n++] = *p++;
	}

	word[n] = '\0';
	*pp = p;
	return 1;
}

static int
add_setting_args(char *argv[], int a, int max, const char *value,
    const char *flag, const char *srcroot)
{
	const char *p = value;
	char word[PATH_MAX];

	if (value == NULL || *value == '\0')
		return a;

	while (a + 2 < max && next_word(&p, word, sizeof(word))) {
		if (word[0] == '\0')
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

/*
 * A flag meant for the linker itself.
 *
 * clang takes several of these directly, but swiftc -- which links a
 * target that has Swift in it -- takes none of them and stops on the
 * first.  -Xlinker is understood by both, so everything aimed at the
 * linker goes through it whichever one is doing the linking.
 */
static int
add_linker_arg(char *argv[], int a, int max, const char *flag,
    const char *value)
{
	if (a + 4 >= max)
		return a;

	argv[a++] = strdup("-Xlinker");
	argv[a++] = strdup(flag);

	if (value != NULL) {
		argv[a++] = strdup("-Xlinker");
		argv[a++] = strdup(value);
	}

	return a;
}

/* Where the binary looks for the dylibs it links, one -rpath each. */
static int
add_rpath_args(char *argv[], int a, int max, const char *value)
{
	const char *p = value;
	char word[PATH_MAX];

	if (value == NULL || *value == '\0')
		return a;

	while (a + 4 < max && next_word(&p, word, sizeof(word))) {
		if (word[0] == '\0')
			continue;
		a = add_linker_arg(argv, a, max, "-rpath", word);
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
	const char *product_name, *configuration, *sdkroot, *srcroot, *full;
	const char *fw_version;
	struct product prod;
	char cfgbuf[128], pnbuf[256], sdkbuf[PATH_MAX];
	char srbuf[PATH_MAX], fullbuf[PATH_MAX], fwbuf[64], rpbuf[PATH_MAX];
	char instname[PATH_MAX];
	int is_framework, dylib;
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

	/*
	 * One place decides where the products go, so that what a project
	 * reads back from $(BUILT_PRODUCTS_DIR) is where they actually
	 * land -- and a project that redirects the build is followed.
	 */
	if (setting(t, "CONFIGURATION_BUILD_DIR", build_dir,
	    sizeof(build_dir)) == NULL)
		snprintf(build_dir, sizeof(build_dir), "%s/build/%s",
		    source_root, configuration);
	snprintf(obj_dir, sizeof(obj_dir), "%s/build/%s.build", source_root,
	    configuration);
	is_framework = (prod.kind == PRODUCT_BUNDLE && prod.wrapper != NULL &&
	    strcmp(prod.wrapper, "framework") == 0);

	/* A framework is a dylib inside a bundle, and links as one. */
	dylib = (prod.kind == PRODUCT_DYNAMIC_LIB) || is_framework;

	if ((fw_version = setting(t, "FRAMEWORK_VERSION", fwbuf,
	    sizeof(fwbuf))) == NULL)
		fw_version = "A";

	if ((full = setting(t, "FULL_PRODUCT_NAME", fullbuf,
	    sizeof(fullbuf))) == NULL)
		full = product_name;

	/*
	 * A bundle's binary lives inside it.  On macOS that is
	 * Contents/MacOS for an application, and Versions/<version> for a
	 * framework, which keeps every version it has ever had beside
	 * each other under one name.
	 */
	if (is_framework)
		snprintf(product, sizeof(product), "%s/%s/Versions/%s/%s",
		    build_dir, full, fw_version, product_name);
	else if (prod.kind == PRODUCT_BUNDLE)
		snprintf(product, sizeof(product), "%s/%s/Contents/MacOS/%s",
		    build_dir, full, product_name);
	else
		snprintf(product, sizeof(product), "%s/%s", build_dir, full);

	/*
	 * What a dylib records as its own name, which is what whoever
	 * links it looks for at run time.  Settled with the rest of the
	 * product's settings, so what a build writes and what
	 * -showBuildSettings reports are the same name.
	 */
	instname[0] = '\0';
	if (dylib)
		setting(t, "LD_DYLIB_INSTALL_NAME", instname, sizeof(instname));

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
					if (dylib)
						argv[a++] = (char *)"-emit-library";
					if (sdkroot != NULL && *sdkroot != '\0') {
						argv[a++] = (char *)"-sdk";
						argv[a++] = (char *)sdkroot;
					}
					goto have_linker;
				}

				argv[a++] = clang;
				if (dylib)
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

				if (dylib && instname[0] != '\0')
					a = add_linker_arg(argv, a,
					    (int)(sizeof(argv) /
					    sizeof(argv[0])) - 1,
					    "-install_name", instname);

				/*
				 * Where this binary looks for the dylibs it
				 * links.  An install name beginning @rpath
				 * means nothing without one.
				 */
				a = add_rpath_args(argv, a,
				    (int)(sizeof(argv) / sizeof(argv[0])) - 1,
				    setting(t, "LD_RUNPATH_SEARCH_PATHS", rpbuf,
				    sizeof(rpbuf)));

				argv[a] = NULL;

				printf("Ld %s\n", product);
			}

			if (rc == 0 && run(argv, opts->verbose) != 0) {
				fprintf(stderr, "xcodebuild: error: link failed\n");
				rc = 1;
			}

			/*
			 * A bundle is a directory with a shape.  Without an
			 * Info.plist naming its executable the binary is
			 * there and nothing will load it, and a framework
			 * needs the links at the top of it besides.
			 */
			if (rc == 0 && prod.kind == PRODUCT_BUNDLE &&
			    prod.wrapper != NULL) {
				char bundle[PATH_MAX], plist[PATH_MAX];

				snprintf(bundle, sizeof(bundle), "%s/%s",
				    build_dir, full);

				if (is_framework) {
					char vdir[PATH_MAX], res[PATH_MAX];
					int npub = 0, npriv = 0, nres = 0;
					int want = infoplist_wanted(t);

					snprintf(vdir, sizeof(vdir),
					    "%s/Versions/%s", bundle,
					    fw_version);
					snprintf(res, sizeof(res),
					    "%s/Resources", vdir);

					if (want) {
						if (mkdirs(res) != 0) {
							fprintf(stderr,
							    "xcodebuild: error:"
							    " cannot create"
							    " %s\n", res);
							rc = 1;
						}

						if (rc == 0) {
							snprintf(plist,
							    sizeof(plist),
							    "%s/Info.plist",
							    res);
							rc = install_infoplist(
							    plist,
							    product_name,
							    "FMWK", t,
							    srcroot);
						}
					}

					if (rc == 0 &&
					    install_bundle_resources(objects,
					    chosen, map, build_dir, sdkroot,
					    devpath, res, t, &nres) != 0)
						rc = 1;

					if (rc == 0 &&
					    install_framework_headers(objects,
					    chosen, map, build_dir, sdkroot,
					    devpath, vdir, &npub,
					    &npriv) != 0)
						rc = 1;

					if (rc == 0 && framework_finalize(
					    bundle, fw_version, product_name,
					    npub > 0, npriv > 0,
					    want || nres > 0) != 0) {
						fprintf(stderr, "xcodebuild:"
						    " error: cannot assemble"
						    " %s\n", bundle);
						rc = 1;
					}
				} else if (strcmp(prod.wrapper, "app") == 0) {
					char res[PATH_MAX];
					int nres = 0;

					if (infoplist_wanted(t)) {
						snprintf(plist, sizeof(plist),
						    "%s/Contents/Info.plist",
						    bundle);
						rc = install_infoplist(plist,
						    product_name, "APPL", t,
						    srcroot);
					}

					if (rc == 0) {
						char pkg[PATH_MAX];

						snprintf(pkg, sizeof(pkg),
						    "%s/Contents/PkgInfo",
						    bundle);
						rc = write_pkginfo(pkg, "APPL",
						    t);
					}

					snprintf(res, sizeof(res),
					    "%s/Contents/Resources", bundle);

					if (rc == 0 &&
					    install_bundle_resources(objects,
					    chosen, map, build_dir, sdkroot,
					    devpath, res, t, &nres) != 0)
						rc = 1;
				}
			}
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

	/*
	 * Implicit dependencies belong to a scheme: the option to look
	 * for them is a scheme's, and xcodebuild does not apply it to a
	 * target built by name.
	 */
	rc = order_targets(objects, project_obj, chosen_id, &order, &stack,
	    &foreign, opts->scheme != NULL);

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
