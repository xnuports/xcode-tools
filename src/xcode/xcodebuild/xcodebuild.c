/* xcodebuild -- open source reimplementation of Apple's xcodebuild utility
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
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>

#include "xcodebuild.h"
#include "devpath.h"
#include "ini.h"
#include "plist.h"
#include "project.h"

#define XCRUN_DEFAULT_CFG "/usr/local/etc/xcrun.ini"

static const char *progname = "xcodebuild";

static const char *actions[] = {
	"build", "clean", "test", "test-without-building", "analyze",
	"archive", "install", "installsrc", "run", "bench", NULL
};

static int is_action(const char *s)
{
	if (s == NULL)
		return 0;
	for (size_t i = 0; actions[i] != NULL; i++)
		if (strcmp(s, actions[i]) == 0)
			return 1;
	return 0;
}

static int endswith(const char *s, const char *suffix)
{
	size_t ls = strlen(s);
	size_t lss = strlen(suffix);
	return ls >= lss && strcmp(s + ls - lss, suffix) == 0;
}

static char *read_file_all(const char *path)
{
	FILE *fp = fopen(path, "rb");
	if (fp == NULL)
		return NULL;
	if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return NULL; }
	long sz = ftell(fp);
	if (sz < 0) { fclose(fp); return NULL; }
	rewind(fp);
	char *buf = (char *)malloc((size_t)sz + 1);
	if (buf == NULL) { fclose(fp); return NULL; }
	size_t rd = fread(buf, 1, (size_t)sz, fp);
	fclose(fp);
	buf[rd] = '\0';
	return buf;
}

static void free_strv(char **v, size_t n)
{
	for (size_t i = 0; i < n; i++)
		free(v[i]);
}

/* Recursive rmdir. */
static int rmtree(const char *path)
{
	struct stat st;
	if (lstat(path, &st) != 0)
		return -1;
	if (S_ISDIR(st.st_mode)) {
		DIR *d = opendir(path);
		if (d == NULL)
			return -1;
		struct dirent *e;
		int rc = 0;
		while ((e = readdir(d)) != NULL) {
			if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
				continue;
			char sub[PATH_MAX];
			snprintf(sub, sizeof(sub), "%s/%s", path, e->d_name);
			if (rmtree(sub) != 0)
				rc = -1;
		}
		closedir(d);
		return rmdir(path) == 0 ? rc : -1;
	}
	return unlink(path) == 0 ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* opts create/free/add                                                */
/* ------------------------------------------------------------------ */

xcodebuild_opts *xbuild_opts_create(void)
{
	return (xcodebuild_opts *)calloc(1, sizeof(xcodebuild_opts));
}

void xbuild_opts_free(xcodebuild_opts *o)
{
	if (o == NULL)
		return;
	free(o->project); free(o->workspace); free(o->scheme);
	free(o->target); free(o->configuration); free(o->sdk);
	free(o->arch); free(o->toolchain); free(o->destination);
	free(o->xcconfig); free(o->derived_data_path);
	free(o->archive_path); free(o->export_path);
	free(o->export_options_plist); free(o->project_dir);
	free(o->action); free(o->result_bundle_path);
	free_strv(o->overrides, o->n_overrides);
	free(o->overrides);
	free(o->argv);
	free(o);
}

void xbuild_opt_add_override(xcodebuild_opts *o, const char *kv)
{
	if (o == NULL || kv == NULL)
		return;
	char **grow = (char **)realloc(o->overrides, sizeof(char *) * (o->n_overrides + 1));
	if (grow == NULL)
		return;
	o->overrides = grow;
	o->overrides[o->n_overrides++] = strdup(kv);
}

/* ------------------------------------------------------------------ */
/* ini value readback                                                  */
/* ------------------------------------------------------------------ */

typedef struct {
	const char *section;
	const char *key;
	char *out;
	size_t outsz;
	int found;
} ini_val_ctx;

static int ini_val_handler(void *user, const char *section, const char *name, const char *value)
{
	ini_val_ctx *c = (ini_val_ctx *)user;
	if (!c->found && strcmp(section, c->section) == 0 && strcmp(name, c->key) == 0) {
		snprintf(c->out, c->outsz, "%s", value);
		c->found = 1;
	}
	return 1;
}

/* Read a scalar `key` from `section` of an ini file. Returns 1 if found,
 * 0 if not, -1 if the file could not be opened. */
static int ini_get_value(const char *path, const char *section, const char *key,
                         char *out, size_t outsz)
{
	ini_val_ctx c = { section, key, out, outsz, 0 };
	int rc = ini_parse(path, ini_val_handler, &c);
	return rc == -1 ? -1 : c.found;
}

/* ------------------------------------------------------------------ */
/* Usage / version                                                     */
/* ------------------------------------------------------------------ */

static void usage(FILE *fp, int code)
{
	fprintf(fp, "Usage: %s [options] [action]\n", progname);
	fputs("A reimplementation of Apple's xcodebuild utility.\n\n"
	      "Action (defaults to 'build'): build, clean, test, test-without-building,\n"
	      "  analyze, archive, install, installsrc, run, bench.\n\n"
	      "Options:\n"
	      "  -project <project>            specify the project to operate on\n"
	      "  -workspace <workspace>        specify the workspace\n"
	      "  -scheme <scheme>             build the specified scheme\n"
	      "  -target <target>             build the specified target\n"
	      "  -configuration <cfg>         use the named build configuration\n"
	      "  -sdk <sdk>                   use the specified SDK (name or path)\n"
	      "  -arch <arch>                 build for the specified architecture\n"
	      "  -toolchain <name>            use the specified toolchain\n"
	      "  -destination <dest>          select the destination device/runner\n"
	      "  -xcconfig <file>             apply build settings from this file\n"
	      "  -derivedDataPath <path>      where to put derived data\n"
	      "  -archivePath <path>          specify the archive path\n"
	      "  -exportPath <path>           where to write the exported product\n"
	      "  -exportOptionsPlist <file>   plist describing the export\n"
	      "  -projectDir <dir>            path to the project directory\n"
	      "  -resultBundlePath <path>\n"
	      "  -alltargets                  build all targets\n"
	      "  -parallelizeTargets          parallelize target builds\n"
	      "  -jobs <n>                    number of build jobs\n"
	      "  -allowProvisioningUpdates\n"
	      "  -allowProvisioningDeviceRegistration\n"
	      "  -quiet                       do not print console output\n"
	      "  -verbose / -v                provide additional status output\n"
	      "  -dry-run                     print commands without executing\n"
	      "  -kill-tests                  delete runs from the test plan\n"
	      "  -json                        output JSON (with -showBuildSettings)\n"
	      "  -pretty                      pretty-print JSON\n"
	      "  -list                        list project information\n"
	      "  -showBuildSettings           print build settings\n"
	      "  -showsdks                    list available SDKs\n"
	      "  -showBuildableProducts       list buildable products\n"
	      "  -exportArchive               export an archive\n"
	      "  -h, -help, --help            show this help\n"
	      "  --version / -version         show version information\n\n"
	      "Build settings may also be specified on the command line as KEY=VALUE.\n",
	      fp);
	exit(code);
}

static void version_print(int verbose)
{
	fprintf(stdout, "xcodebuild %s\n", XCODEBUILD_VERSION);
	if (verbose) {
		char *devpath = xbuild_get_developer_path();
		if (devpath != NULL) {
			fprintf(stdout, "Build version 1.0\n");
			fprintf(stdout, "Developer path: %s\n", devpath);
			free(devpath);
		}
	}
}

/* ------------------------------------------------------------------ */
/* Developer path resolution                                           */
/* ------------------------------------------------------------------ */

char *xbuild_get_developer_path(void)
{
	const char *value = getenv("DEVELOPER_DIR");
	if (value != NULL && *value)
		return strdup(value);

	char *home = getenv("HOME");
	if (home == NULL) {
		fprintf(stderr, "xcodebuild: error: unable to determine HOME directory\n");
		return NULL;
	}

	char cfg_path[PATH_MAX];
	snprintf(cfg_path, sizeof(cfg_path), "%s/%s", home, SDK_CFG);
	FILE *fp = fopen(cfg_path, "r");
	if (fp != NULL) {
		char devpath[PATH_MAX];
		memset(devpath, 0, sizeof(devpath));
		size_t n = fread(devpath, 1, PATH_MAX - 1, fp);
		fclose(fp);
		devpath[n] = '\0';
		while (n > 0 && (devpath[n - 1] == '\n' || devpath[n - 1] == '\r' ||
		                 devpath[n - 1] == ' ' || devpath[n - 1] == '\t'))
			devpath[--n] = '\0';
		if (devpath[0] != '\0')
			return strdup(devpath);
	}

	struct stat st;
	/*
	 * Prefer the Developer directory this binary lives in, so a
	 * relocated or freshly built release tree works with no
	 * configuration; the compiled-in system default is the fallback.
	 */
	{
		const char *self = xt_default_developer_dir();

		if (self != NULL)
			return strdup(self);
	}

	if (stat(XCODEBUILD_DEFAULT_DEVELOPER_DIR, &st) == 0 && S_ISDIR(st.st_mode))
		return strdup(XCODEBUILD_DEFAULT_DEVELOPER_DIR);

	fprintf(stderr, "xcodebuild: error: unable to locate a developer directory.\n"
	                "xcodebuild: error: run `xcode-select --switch <path>` or set DEVELOPER_DIR.\n");
	return NULL;
}

/* ------------------------------------------------------------------ */
/* SDK / toolchain name resolution                                     */
/* ------------------------------------------------------------------ */

const char *xbuild_resolve_sdk_name(const xcodebuild_opts *opts, const char *devpath)
{
	static char sdk[PATH_MAX];

	if (opts != NULL && opts->sdk != NULL) {
		if (opts->sdk[0] == '/') {
			const char *base = strrchr(opts->sdk, '/');
			const char *start = base ? base + 1 : opts->sdk;
			snprintf(sdk, sizeof(sdk), "%s", start);
			char *dot = strstr(sdk, ".sdk");
			if (dot) *dot = '\0';
			return sdk;
		}
		snprintf(sdk, sizeof(sdk), "%s", opts->sdk);
		return sdk;
	}

	const char *env = getenv("SDKROOT");
	if (env != NULL && *env) {
		if (env[0] == '/') {
			const char *base = strrchr(env, '/');
			const char *start = base ? base + 1 : env;
			snprintf(sdk, sizeof(sdk), "%s", start);
			char *dot = strstr(sdk, ".sdk");
			if (dot) *dot = '\0';
			return sdk;
		}
		snprintf(sdk, sizeof(sdk), "%s", env);
		return sdk;
	}

	if (devpath != NULL) {
		char ini_path[PATH_MAX];
		{
			const char *self = xt_default_developer_dir();
			struct stat cst;

			/* Prefer the copy inside the developer directory. */
			if (self != NULL) {
				snprintf(ini_path, sizeof(ini_path),
					 "%s/usr/share/xcrun.ini", self);
				if (stat(ini_path, &cst) == 0 && S_ISREG(cst.st_mode))
					goto have_ini;
			}
		}
		snprintf(ini_path, sizeof(ini_path), "%s", XCRUN_DEFAULT_CFG);
have_ini:;
		char name[256] = {0};
		if (ini_get_value(ini_path, "SDK", "name", name, sizeof(name)) == 1 && name[0]) {
			snprintf(sdk, sizeof(sdk), "%s", name);
			return sdk;
		}
		char sdks[PATH_MAX];
		snprintf(sdks, sizeof(sdks), "%s/SDKs", devpath);
		DIR *d = opendir(sdks);
		if (d != NULL) {
			struct dirent *e;
			while ((e = readdir(d)) != NULL) {
				if (endswith(e->d_name, ".sdk")) {
					snprintf(sdk, sizeof(sdk), "%s", e->d_name);
					char *dot = strstr(sdk, ".sdk");
					if (dot) *dot = '\0';
					closedir(d);
					return sdk;
				}
			}
			closedir(d);
		}
	}
	snprintf(sdk, sizeof(sdk), "DarwinARM");
	return sdk;
}

const char *xbuild_resolve_toolchain_name(const xcodebuild_opts *opts, const char *devpath, const char *sdkname)
{
	static char tc[PATH_MAX];

	if (opts != NULL && opts->toolchain != NULL) {
		snprintf(tc, sizeof(tc), "%s", opts->toolchain);
		return tc;
	}

	const char *env = getenv("TOOLCHAINS");
	if (env != NULL && *env) {
		const char *base = strrchr(env, '/');
		const char *start = base ? base + 1 : env;
		const char *dot = strstr(start, ".toolchain");
		size_t len = dot ? (size_t)(dot - start) : strlen(start);
		snprintf(tc, sizeof(tc), "%.*s", (int)len, start);
		return tc;
	}

	if (devpath != NULL && sdkname != NULL) {
		char sdk_path[PATH_MAX];
		snprintf(sdk_path, sizeof(sdk_path), "%s/SDKs/%s.sdk", devpath, sdkname);
		char name[256] = {0};
		if (ini_get_value(sdk_path, "SDK", "toolchain", name, sizeof(name)) == 1 && name[0]) {
			snprintf(tc, sizeof(tc), "%s", name);
			return tc;
		}
	}
	snprintf(tc, sizeof(tc), "%s", sdkname ? sdkname : "DarwinARM");
	return tc;
}

static char *path_join(const char *a, const char *b)
{
	char *p = (char *)malloc(PATH_MAX);
	if (p)
		snprintf(p, PATH_MAX, "%s/%s", a, b);
	return p;
}

static char *detect_project(const xcodebuild_opts *opts, const char *project_dir)
{
	if (opts != NULL && opts->project != NULL)
		return strdup(opts->project);
	const char *dir = project_dir ? project_dir : ".";
	DIR *d = opendir(dir);
	if (d == NULL)
		return NULL;
	struct dirent *e;
	char *found = NULL;
	while ((e = readdir(d)) != NULL) {
		if (endswith(e->d_name, ".xcodeproj")) {
			found = path_join(dir, e->d_name);
			break;
		}
	}
	closedir(d);
	return found;
}

/* ------------------------------------------------------------------ */
/* Settings orchestration                                            */
/* ------------------------------------------------------------------ */

static settings_table *resolve_settings(xcodebuild_opts *opts, const char *devpath)
{
	const char *sdkname = xbuild_resolve_sdk_name(opts, devpath);
	const char *tcname = xbuild_resolve_toolchain_name(opts, devpath, sdkname);
	const char *configuration = opts->configuration ? opts->configuration : "Debug";
	const char *arch = opts->arch;

	char *project = detect_project(opts, opts->project_dir);

	settings_table *t = settings_create();
	if (t == NULL)
		return NULL;

	if (settings_load_defaults(t, devpath, sdkname, tcname, configuration, arch) != 0)
		fprintf(stderr, "xcodebuild: warning: could not load SDK info for '%s'\n", sdkname);

	if (opts->target != NULL)
		settings_set(t, "TARGET_NAME", opts->target);

	if (project != NULL) {
		plist_node *root = project_load_pbxproj(project);
		if (root != NULL) {
			plist_node *pobj = project_get_project_object(root);
			if (pobj != NULL) {
				plist_node *pname = plist_dict_get(pobj, "name");
				if (pname != NULL && pname->type == PLIST_STRING)
					settings_set(t, "PROJECT_NAME", pname->string);
			}
			plist_node *bs = project_find_buildsettings(root, opts->target, configuration);
			if (bs != NULL)
				settings_merge_plist_dict(t, bs);
			plist_free(root);
		} else if (opts->verbose) {
			fprintf(stderr, "xcodebuild: warning: could not parse project '%s'\n", project);
		}
	}
	free(project);

	if (opts->xcconfig != NULL) {
		if (settings_load_xcconfig(t, opts->xcconfig) != 0)
			fprintf(stderr, "xcodebuild: warning: could not read xcconfig '%s'\n", opts->xcconfig);
	}

	for (size_t i = 0; i < opts->n_overrides; i++) {
		char *eq = strchr(opts->overrides[i], '=');
		if (eq != NULL) {
			*eq = '\0';
			char *expanded = settings_expand(t, eq + 1);
			settings_set(t, opts->overrides[i], expanded);
			*eq = '=';
			free(expanded);
		}
	}

	settings_set(t, "ACTION", opts->action ? opts->action : "build");
	return t;
}

/* ------------------------------------------------------------------ */
/* Delegation via xcrun (build / test / archive / install actions)    */
/* ------------------------------------------------------------------ */

static char *find_xcrun(const char *devpath)
{
	char candidate[PATH_MAX];
	snprintf(candidate, sizeof(candidate), "%s/usr/bin/xcrun", devpath);
	if (access(candidate, X_OK) == 0)
		return strdup(candidate);
	return strdup("xcrun");
}

/* Populate `envp` (caller-frees entries via the NULL terminator) with the
 * KEY=VALUE strings a delegated build driver expects. */
static void build_envp(settings_table *t, const char *devpath,
                       const char *sdkname, const char *tcname, char *envp[8])
{
	char *tc_parent = path_join(devpath, "Toolchains");
	char tc_full[PATH_MAX];
	snprintf(tc_full, sizeof(tc_full), "%s/%s.toolchain", tc_parent, tcname);
	free(tc_parent);
	char *tc_dir = strdup(tc_full);

	const char *sdkroot = settings_get(t, "SDKROOT");
	char *sdk_root_owned = NULL;
	if (sdkroot == NULL) {
		sdk_root_owned = path_join(devpath, "SDKs");
		sdkroot = sdk_root_owned;
	}

	const char *home = getenv("HOME");
	const char *oldpath = getenv("PATH");
	const char *triple = settings_get(t, "TARGET_TRIPLE");
	const char *deploy = settings_get(t, "MACOSX_DEPLOYMENT_TARGET");
	if (deploy == NULL)
		deploy = settings_get(t, "IOS_DEPLOYMENT_TARGET");

	int i = 0;
	char buf[PATH_MAX];
	snprintf(buf, sizeof(buf), "SDKROOT=%s", sdkroot);
	envp[i++] = strdup(buf);
	snprintf(buf, sizeof(buf), "PATH=%s/usr/bin:%s/usr/bin:%s", devpath, tc_dir, oldpath ? oldpath : "");
	envp[i++] = strdup(buf);
	snprintf(buf, sizeof(buf), "LD_LIBRARY_PATH=%s/usr/lib", tc_dir);
	envp[i++] = strdup(buf);
	snprintf(buf, sizeof(buf), "HOME=%s", home ? home : "");
	envp[i++] = strdup(buf);
	if (triple != NULL) {
		snprintf(buf, sizeof(buf), "TARGET_TRIPLE=%s", triple);
		envp[i++] = strdup(buf);
	}
	if (deploy != NULL) {
		snprintf(buf, sizeof(buf), "MACOSX_DEPLOYMENT_TARGET=%s", deploy);
		envp[i++] = strdup(buf);
	}
	envp[i] = NULL;
	free(tc_dir);
	free(sdk_root_owned);
}

static int exec_build_action(xcodebuild_opts *opts, const char *devpath,
                             const char *sdkname, const char *tcname,
                             settings_table *t, const char *action)
{
	char *xcrun = find_xcrun(devpath);
	const char *tool = action;
	if (strcmp(action, "test-without-building") == 0)
		tool = "test";

	size_t n = opts->n_overrides;
	char **argv = (char **)malloc(sizeof(char *) * (7 + n + 1));
	if (argv == NULL) { free(xcrun); return 1; }
	int j = 0;
	argv[j++] = xcrun;
	argv[j++] = strdup("-sdk");
	argv[j++] = strdup(sdkname);
	argv[j++] = strdup("-toolchain");
	argv[j++] = strdup(tcname);
	argv[j++] = strdup(tool);
	for (size_t i = 0; i < n; i++)
		argv[j++] = strdup(opts->overrides[i]);
	argv[j] = NULL;

	char *envp[8] = {0};
	build_envp(t, devpath, sdkname, tcname, envp);
	for (int k = 0; envp[k] != NULL; k++)
		putenv(envp[k]);

	if (opts->verbose || opts->dry_run) {
		fprintf(stderr, "xcodebuild: %s: \"" , opts->dry_run ? "dry-run" : "verbose");
		fprintf(stderr, "%s", argv[0]);
		for (int k = 1; argv[k] != NULL; k++)
			fprintf(stderr, " %s", argv[k]);
		fputs("\"\n", stderr);
	}

	int rc = 0;
	if (opts->dry_run) {
		rc = 0;
	} else {
		execvp(argv[0], argv);
		fprintf(stderr, "xcodebuild: error: failed to exec '%s': %s\n", argv[0], strerror(errno));
		rc = 1;
	}

	for (int k = 0; argv[k] != NULL; k++)
		free(argv[k]);
	free(argv);
	for (int k = 0; envp[k] != NULL; k++)
		free(envp[k]);
	return rc;
}

/* ------------------------------------------------------------------ */
/* clean                                                               */
/* ------------------------------------------------------------------ */

static int do_clean(const xcodebuild_opts *opts, settings_table *t)
{
	const char *products = settings_get(t, "BUILT_PRODUCTS_DIR");
	const char *build_dir = settings_get(t, "BUILD_DIR");
	const char *config_temp = settings_get(t, "CONFIGURATION_TEMP_DIR");

	if (opts->verbose)
		fprintf(stderr, "xcodebuild: cleaning build products\n");

	if (products != NULL && *products != '\0') {
		if (opts->dry_run)
			fprintf(stderr, "xcodebuild: rm -rf %s\n", products);
		else
			rmtree(products);
	}
	if (build_dir != NULL && *build_dir != '\0' && build_dir != products) {
		if (opts->dry_run)
			fprintf(stderr, "xcodebuild: rm -rf %s\n", build_dir);
		else
			rmtree(build_dir);
	}
	if (config_temp != NULL && *config_temp != '\0') {
		if (opts->dry_run)
			fprintf(stderr, "xcodebuild: rm -rf %s\n", config_temp);
		else
			rmtree(config_temp);
	}
	if (!opts->quiet)
		fprintf(stdout, "xcodebuild: clean complete\n");
	return 0;
}

/* ------------------------------------------------------------------ */
/* export options plist (minimal XML plist extraction)               */
/* ------------------------------------------------------------------ */

static char *xml_value(const char *s, const char *e)
{
	char *v = (char *)malloc((e - s) + 1);
	if (v == NULL)
		return NULL;
	memcpy(v, s, e - s);
	v[e - s] = '\0';
	return v;
}

static char *xmlplist_get(const char *text, const char *key)
{
	char keyname[512];
	snprintf(keyname, sizeof(keyname), "<key>%s</key>", key);
	const char *k = strstr(text, keyname);
	if (k == NULL)
		return NULL;
	const char *p = k + strlen(keyname);
	while (*p && isspace((unsigned char)*p)) p++;

	if (strncmp(p, "<string>", 8) == 0) {
		const char *s = p + 8;
		const char *e = strstr(s, "</string>");
		if (e == NULL) return NULL;
		return xml_value(s, e);
	}
	if (strncmp(p, "<true", 5) == 0)
		return strdup("YES");
	if (strncmp(p, "<false", 6) == 0)
		return strdup("NO");
	if (strncmp(p, "<integer>", 9) == 0) {
		const char *s = p + 9;
		const char *e = strstr(s, "</integer>");
		if (e == NULL) return NULL;
		return xml_value(s, e);
	}
	if (strncmp(p, "<real>", 6) == 0) {
		const char *s = p + 6;
		const char *e = strstr(s, "</real>");
		if (e == NULL) return NULL;
		return xml_value(s, e);
	}
	return NULL;
}

static int do_export_archive(const xcodebuild_opts *opts)
{
	if (opts->archive_path == NULL || opts->export_path == NULL || opts->export_options_plist == NULL) {
		fprintf(stderr, "xcodebuild: error: -exportArchive requires -archivePath, -exportPath and -exportOptionsPlist\n");
		return 1;
	}

	char *plist_text = read_file_all(opts->export_options_plist);
	if (plist_text == NULL) {
		fprintf(stderr, "xcodebuild: error: cannot read export options plist '%s'\n",
		        opts->export_options_plist);
		return 1;
	}

	char *method = xmlplist_get(plist_text, "method");
	char *destination = xmlplist_get(plist_text, "destination");
	char *team = xmlplist_get(plist_text, "teamID");
	char *compile = xmlplist_get(plist_text, "compileBitcode");
	char *strip = xmlplist_get(plist_text, "stripSwiftSymbols");
	free(plist_text);

	if (method == NULL) {
		fprintf(stderr, "xcodebuild: error: exportOptions.plist does not specify a 'method'\n");
		free(method); free(destination); free(team); free(compile); free(strip);
		return 1;
	}

	if (opts->verbose)
		fprintf(stderr, "xcodebuild: exporting archive '%s' -> '%s' (method=%s, destination=%s%s%s)\n",
		        opts->archive_path, opts->export_path, method,
		        destination ? destination : "not specified",
		        team ? ", team=" : "", team ? team : "");

	free(method);
	free(destination);
	free(team);
	free(compile);
	free(strip);
	return 0;
}

/* ------------------------------------------------------------------ */
/* Argument parsing                                                    */
/* ------------------------------------------------------------------ */

static char *consume_value(int *i, int argc, char **argv, const char *val)
{
	if (val != NULL)
		return strdup(val);
	if (*i + 1 >= argc) {
		fprintf(stderr, "xcodebuild: error: option requires an argument\n");
		exit(1);
	}
	(*i)++;
	return strdup(argv[*i]);
}

static void set_opt(char **slot, const char *val)
{
	free(*slot);
	*slot = strdup(val ? val : "");
}

static xcodebuild_opts *parse_args(int argc, char **argv)
{
	xcodebuild_opts *opts = xbuild_opts_create();
	if (opts == NULL)
		return NULL;

	for (int i = 1; i < argc; i++) {
		const char *arg = argv[i];
		if (arg[0] != '-' || arg[1] == '\0') {
			if (arg[0] == '-' && arg[1] == '\0')
				continue; /* "--" skip */
			if (opts->action == NULL && is_action(arg))
				opts->action = strdup(arg);
			else if (strchr(arg, '=') != NULL)
				xbuild_opt_add_override(opts, arg);
			continue;
		}

		char keybuf[64];
		char *eq = strchr(argv[i], '=');
		const char *key;
		const char *val = NULL;
		if (eq != NULL) {
			size_t kl = (size_t)(eq - argv[i]);
			if (kl >= sizeof(keybuf)) kl = sizeof(keybuf) - 1;
			memcpy(keybuf, argv[i], kl);
			keybuf[kl] = '\0';
			key = keybuf;
			val = eq + 1;
		} else {
			key = argv[i];
		}

		if (strcmp(key, "-h") == 0 || strcmp(key, "-help") == 0 || strcmp(key, "--help") == 0)
			opts->help = 1;
		else if (strcmp(key, "-version") == 0 || strcmp(key, "--version") == 0)
			opts->version = 1;
		else if (strcmp(key, "-list") == 0)
			opts->list_targets = 1;
		else if (strcmp(key, "-showBuildSettings") == 0)
			opts->show_build_settings = 1;
		else if (strcmp(key, "-showsdks") == 0)
			opts->show_sdks = 1;
		else if (strcmp(key, "-showBuildableProducts") == 0)
			opts->show_buildable_products = 1;
		else if (strcmp(key, "-showRuntimeSearchable") == 0)
			opts->show_runtime_searchable = 1;
		else if (strcmp(key, "-exportArchive") == 0)
			opts->export_archive = 1;
		else if (strcmp(key, "-alltargets") == 0)
			opts->all_targets = 1;
		else if (strcmp(key, "-parallelizeTargets") == 0)
			opts->parallel_targets = 1;
		else if (strcmp(key, "-json") == 0)
			opts->json = 1;
		else if (strcmp(key, "-pretty") == 0)
			opts->pretty = 1;
		else if (strcmp(key, "-quiet") == 0)
			opts->quiet = 1;
		else if (strcmp(key, "-dry-run") == 0)
			opts->dry_run = 1;
		else if (strcmp(key, "-verbose") == 0 || strcmp(key, "-v") == 0)
			opts->verbose = 1;
		else if (strcmp(key, "-allowProvisioningUpdates") == 0)
			opts->allow_provisioning_updates = 1;
		else if (strcmp(key, "-allowProvisioningDeviceRegistration") == 0)
			opts->allow_provisioning_device_registration = 1;
		else if (strcmp(key, "-kill-tests") == 0)
			opts->kill_tests = 1;
		else if (strcmp(key, "-project") == 0)
			set_opt(&opts->project, consume_value(&i, argc, argv, val));
		else if (strcmp(key, "-workspace") == 0)
			set_opt(&opts->workspace, consume_value(&i, argc, argv, val));
		else if (strcmp(key, "-scheme") == 0)
			set_opt(&opts->scheme, consume_value(&i, argc, argv, val));
		else if (strcmp(key, "-target") == 0)
			set_opt(&opts->target, consume_value(&i, argc, argv, val));
		else if (strcmp(key, "-configuration") == 0)
			set_opt(&opts->configuration, consume_value(&i, argc, argv, val));
		else if (strcmp(key, "-sdk") == 0)
			set_opt(&opts->sdk, consume_value(&i, argc, argv, val));
		else if (strcmp(key, "-arch") == 0)
			set_opt(&opts->arch, consume_value(&i, argc, argv, val));
		else if (strcmp(key, "-toolchain") == 0)
			set_opt(&opts->toolchain, consume_value(&i, argc, argv, val));
		else if (strcmp(key, "-destination") == 0)
			set_opt(&opts->destination, consume_value(&i, argc, argv, val));
		else if (strcmp(key, "-xcconfig") == 0)
			set_opt(&opts->xcconfig, consume_value(&i, argc, argv, val));
		else if (strcmp(key, "-derivedDataPath") == 0)
			set_opt(&opts->derived_data_path, consume_value(&i, argc, argv, val));
		else if (strcmp(key, "-archivePath") == 0)
			set_opt(&opts->archive_path, consume_value(&i, argc, argv, val));
		else if (strcmp(key, "-exportPath") == 0)
			set_opt(&opts->export_path, consume_value(&i, argc, argv, val));
		else if (strcmp(key, "-exportOptionsPlist") == 0)
			set_opt(&opts->export_options_plist, consume_value(&i, argc, argv, val));
		else if (strcmp(key, "-projectDir") == 0)
			set_opt(&opts->project_dir, consume_value(&i, argc, argv, val));
		else if (strcmp(key, "-resultBundlePath") == 0)
			set_opt(&opts->result_bundle_path, consume_value(&i, argc, argv, val));
		else if (strcmp(key, "-jobs") == 0) {
			const char *jv = val ? val : (i + 1 < argc ? argv[++i] : NULL);
			opts->jobs = jv ? atoi(jv) : 0;
		} else if (opts->verbose) {
			fprintf(stderr, "xcodebuild: warning: ignoring unknown option '%s'\n", key);
		}
	}

	return opts;
}

/* ------------------------------------------------------------------ */
/* Main dispatch                                                       */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
	xcodebuild_opts *opts = parse_args(argc, argv);
	if (opts == NULL)
		return 1;

	if (opts->help)
		usage(stdout, 0);
	if (argc < 2)
		usage(stderr, 1);
	if (opts->version) {
		version_print(opts->verbose);
		xbuild_opts_free(opts);
		return 0;
	}

	char *devpath = xbuild_get_developer_path();
	if (devpath == NULL) {
		xbuild_opts_free(opts);
		return 1;
	}

	if (opts->show_sdks) {
		project_show_sdks(devpath);
		free(devpath);
		xbuild_opts_free(opts);
		return 0;
	}

	if (opts->list_targets) {
		char *project = detect_project(opts, opts->project_dir);
		int r = project_list(project, opts->workspace, opts);
		free(project);
		free(devpath);
		xbuild_opts_free(opts);
		return r;
	}

	if (opts->show_build_settings) {
		settings_table *t = resolve_settings(opts, devpath);
		if (t == NULL) {
			free(devpath);
			xbuild_opts_free(opts);
			return 1;
		}
		int r = settings_emit(t, opts->json, opts->pretty);
		settings_destroy(t);
		free(devpath);
		xbuild_opts_free(opts);
		return r ? 1 : 0;
	}

	if (opts->show_buildable_products) {
		settings_table *t = resolve_settings(opts, devpath);
		if (t == NULL) {
			free(devpath);
			xbuild_opts_free(opts);
			return 1;
		}
		const char *name = settings_get(t, "PRODUCT_NAME");
		if (name != NULL && *name != '\0')
			printf("    %s\n", name);
		settings_destroy(t);
		free(devpath);
		xbuild_opts_free(opts);
		return 0;
	}

	if (opts->export_archive) {
		int r = do_export_archive(opts);
		free(devpath);
		xbuild_opts_free(opts);
		return r;
	}

	const char *action = opts->action ? opts->action : "build";

	if (strcmp(action, "clean") == 0) {
		settings_table *t = resolve_settings(opts, devpath);
		if (t == NULL) {
			free(devpath);
			xbuild_opts_free(opts);
			return 1;
		}
		int r = do_clean(opts, t);
		settings_destroy(t);
		free(devpath);
		xbuild_opts_free(opts);
		return r;
	}

	settings_table *t = resolve_settings(opts, devpath);
	if (t == NULL) {
		free(devpath);
		xbuild_opts_free(opts);
		return 1;
	}
	const char *sdkname = xbuild_resolve_sdk_name(opts, devpath);
	const char *tcname = xbuild_resolve_toolchain_name(opts, devpath, sdkname);
	int r = exec_build_action(opts, devpath, sdkname, tcname, t, action);
	settings_destroy(t);
	free(devpath);
	xbuild_opts_free(opts);
	return r;
}
