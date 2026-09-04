/*
 * productbuild - open reimplementation of Apple's productbuild(1).
 *
 * Builds pkgutil-valid product archives (.pkg) that wrap component
 * packages inside a xar archive alongside a synthesized Distribution
 * XML file. The xar(1) system tool is used for archive assembly (same
 * delegation pattern as pkgbuild uses cpio(1)/mkbom(1)).
 *
 * Five modes, mirroring Apple's tool:
 *
 *   1. --root <dir> <install-path> <output>
 *   2. --component <bundle> <install-path> <output>
 *   3. --content <dir> <output>
 *   4. --distribution <dist> [--package-path <path>] <output>
 *   5. --synthesize [--package <pkg>] <output>
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <dirent.h>
#include "xar.h"
#include "payload.h"
#include "bom.h"
#include "analyze.h"
#include "dist.h"

#define PRODUCTBUILD_VERSION "0.1.0"

static const char *program_name = "productbuild";

/* --- growable array of package paths (--package / --synthesize) --- */

struct pkg_list {
	const char **items;
	int count;
	int cap;
};

static int
pkg_list_add(struct pkg_list *l, const char *pkg)
{
	if (l->count + 1 > l->cap) {
		int cap = l->cap ? l->cap : 16;
		while (cap < l->count + 1)
			cap *= 2;
		const char **nv = realloc(l->items, (size_t)cap * sizeof(*nv));
		if (nv == NULL)
			return -1;
		l->items = nv;
		l->cap = cap;
	}
	l->items[l->count++] = pkg;
	return 0;
}

static void
pkg_list_free(struct pkg_list *l)
{
	free(l->items);
	l->items = NULL;
	l->count = 0;
	l->cap = 0;
}
/* --- file helpers --- */

static unsigned char *
read_file(const char *path, size_t *out_len)
{
	FILE *f = fopen(path, "rb");
	if (f == NULL)
		return NULL;
	struct stat st;
	if (fstat(fileno(f), &st) != 0) {
		fclose(f);
		return NULL;
	}
	size_t want = (size_t)st.st_size;
	unsigned char *buf = malloc(want ? want : 1);
	if (buf == NULL) {
		fclose(f);
		return NULL;
	}
	size_t got = fread(buf, 1, want, f);
	fclose(f);
	if (got != want) {
		free(buf);
		return NULL;
	}
	*out_len = got;
	return buf;
}

static int
write_file(const char *path, const void *data, size_t len)
{
	FILE *f = fopen(path, "wb");
	if (f == NULL)
		return -1;
	size_t wr = fwrite(data, 1, len, f);
	fclose(f);
	return (wr == len) ? 0 : -1;
}

/* --- recursive remove --- */

static void
remove_tree(const char *path)
{
	struct stat st;
	if (lstat(path, &st) != 0)
		return;
	if (S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode)) {
		DIR *d = opendir(path);
		if (d != NULL) {
			struct dirent *de;
			while ((de = readdir(d)) != NULL) {
				if (strcmp(de->d_name, ".") == 0 ||
				    strcmp(de->d_name, "..") == 0)
					continue;
				char child[PATH_MAX];
				snprintf(child, sizeof child, "%s/%s", path, de->d_name);
				remove_tree(child);
			}
			closedir(d);
		}
		rmdir(path);
	} else {
		unlink(path);
	}
}

/* --- sign an archive via codesign(1) --- */

static void
sign_package(const char *path, const char *identity)
{
	if (identity == NULL || strcmp(identity, "-") == 0)
		return;

	pid_t pid = fork();
	if (pid < 0)
		return;
	if (pid == 0) {
		execlp("codesign", "codesign", "--force", "--sign",
		    identity, path, (char *)NULL);
		_exit(127);
	}
	int status = 0;
	waitpid(pid, &status, 0);
	if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
		return;
	fprintf(stderr, "%s: codesign failed (exit %d)\n", program_name,
	    WIFEXITED(status) ? WEXITSTATUS(status) : -1);
}

/* --- build a component package's Bom/Payload/PackageInfo in memory --- */

static int
build_component_pkg(const char *root, const char *identifier,
    const char *version, const char *install_location,
    const char *scripts_dir,
    unsigned char **out_bom, size_t *out_bom_len,
    struct payload *out_pl,
    char **out_pkginfo, size_t *out_pkginfo_len,
    size_t *out_install_kbytes)
{
	struct payload pl;
	memset(&pl, 0, sizeof pl);
	if (payload_build(root, &pl) != 0) {
		fprintf(stderr, "%s: failed to build Payload for %s\n",
		    program_name, root);
		return -1;
	}

	size_t bom_len = 0;
	unsigned char *bom = bom_build(root, &bom_len);
	if (bom == NULL) {
		fprintf(stderr, "%s: failed to build BOM\n", program_name);
		free(pl.data);
		return -1;
	}

	size_t install_kbytes = (pl.uncompressed + 1023) / 1024;
	if (install_kbytes == 0)
		install_kbytes = 1;

	size_t pi_len = 0;
	char *pkginfo = pkginfo_build(identifier, version, install_location,
	    scripts_dir, pl.file_count, install_kbytes, &pi_len);
	if (pkginfo == NULL) {
		fprintf(stderr, "%s: failed to build PackageInfo\n", program_name);
		free(pl.data);
		free(bom);
		return -1;
	}

	*out_bom = bom;
	*out_bom_len = bom_len;
	*out_pl = pl;
	*out_pkginfo = pkginfo;
	*out_pkginfo_len = pi_len;
	*out_install_kbytes = install_kbytes;
	return 0;
}

/* --- assemble a product archive using our xar writer --- */

static int
assemble_product(const char *dist_data, size_t dist_len,
    unsigned char *bom_data, size_t bom_len,
    unsigned char *payload_data, size_t payload_len,
    char *pkginfo_data, size_t pkginfo_len,
    const char *out, const char *sign_identity)
{
	/* Build the xar archive with:
	 *   root.pkg/           (directory entry)
	 *   root.pkg/Bom        (file, zlib-compressed)
	 *   root.pkg/Payload    (file, stored raw — already gzip'd by payload.c)
	 *   root.pkg/PackageInfo (file, zlib-compressed)
	 *   Distribution        (file, zlib-compressed)
	 */
	int nent = 5;
	struct xar_entry *entries = calloc((size_t)nent, sizeof(*entries));
	if (entries == NULL)
		return -1;

	entries[0].name = "root.pkg";
	entries[0].is_dir = 1;

	entries[1].name = "root.pkg/Bom";
	entries[1].encoding = "application/x-gzip";
	entries[1].data = bom_data;
	entries[1].size = bom_len;
	entries[1].compressed = 1;

	entries[2].name = "root.pkg/Payload";
	entries[2].encoding = "application/octet-stream";
	entries[2].data = payload_data;
	entries[2].size = payload_len;
	entries[2].compressed = 0;

	entries[3].name = "root.pkg/PackageInfo";
	entries[3].encoding = "application/x-gzip";
	entries[3].data = pkginfo_data;
	entries[3].size = pkginfo_len;
	entries[3].compressed = 1;

	entries[4].name = "Distribution";
	entries[4].encoding = "application/x-gzip";
	entries[4].data = dist_data;
	entries[4].size = dist_len;
	entries[4].compressed = 1;

	size_t arch_len = 0;
	unsigned char *arch = xar_build(entries, nent, &arch_len);
	free(entries);

	if (arch == NULL) {
		fprintf(stderr, "%s: failed to assemble archive\n", program_name);
		return -1;
	}

	if (write_file(out, arch, arch_len) != 0) {
		fprintf(stderr, "%s: cannot write %s\n", program_name, out);
		free(arch);
		return -1;
	}
	free(arch);

	if (sign_identity != NULL)
		sign_package(out, sign_identity);

	return 0;
}

/* --- forward declarations --- */

static int synthesize_from_packages(struct pkg_list *pkgs,
    const char *identifier, const char *version, const char *out);
static int build_from_distribution(const char *dist_path,
    const char *pkg_search_path, const char *out,
    const char *sign_identity, int quiet);

/* --- mode 1: --root <dir> <install-path> <output> --- */

static int
build_root(const char *root, const char *install_path,
    const char *identifier, const char *version, const char *scripts_dir,
    const char *out, const char *sign_identity)
{
	unsigned char *bom = NULL;
	size_t bom_len = 0;
	struct payload pl;
	char *pkginfo = NULL;
	size_t pkginfo_len = 0;
	size_t install_kbytes = 0;

	if (build_component_pkg(root,
	    identifier ? identifier : "com.apple.unknown",
	    version ? version : "1.0", install_path, scripts_dir,
	    &bom, &bom_len, &pl, &pkginfo, &pkginfo_len,
	    &install_kbytes) != 0)
		return 1;

	char *dist = NULL;
	size_t dist_len = 0;
	dist = dist_synthesize(identifier ? identifier : "com.apple.unknown",
	    version ? version : "1.0", "root.pkg", install_path,
	    install_kbytes, NULL, &dist_len);
	if (dist == NULL) {
		fprintf(stderr, "%s: failed to synthesize Distribution\n",
		    program_name);
		free(pkginfo);
		free(bom);
		free(pl.data);
		return 1;
	}

	int rc = assemble_product(dist, dist_len, bom, bom_len,
	    pl.data, pl.size, pkginfo, pkginfo_len, out, sign_identity);

	free(dist);
	free(pkginfo);
	free(bom);
	free(pl.data);

	if (rc == 0)
		fprintf(stderr, "productbuild: %s\n", out);
	return 0;
}

/* --- main --- */

int
main(int argc, char **argv)
{
	program_name = argv[0];

	const char *root = NULL;
	const char *component = NULL;
	const char *content = NULL;
	const char *dist_path = NULL;
	const char *pkg_search_path = NULL;
	const char *identifier = NULL;
	const char *version = NULL;
	const char *scripts_dir = NULL;
	const char *out = NULL;
	const char *sign_identity = NULL;
	int synthesize = 0;
	int quiet = 0;
	int timestamp = 0;
	(void)timestamp;

	struct pkg_list pkgs;
	memset(&pkgs, 0, sizeof pkgs);

	static struct option longopts[] = {
		{"root",                   required_argument, 0, 'r'},
		{"component",              required_argument, 0, 0x100},
		{"content",                required_argument, 0, 0x101},
		{"distribution",           required_argument, 0, 'd'},
		{"package-path",           required_argument, 0, 'p'},
		{"package",                required_argument, 0, 'P'},
		{"synthesize",             no_argument,       0, 's'},
		{"product",                required_argument, 0, 'f'},
		{"scripts",                required_argument, 0, 'S'},
		{"resources",              required_argument, 0, 0x102},
		{"plugins",                required_argument, 0, 0x103},
		{"identifier",             required_argument, 0, 'i'},
		{"version",                optional_argument, 0, 0x107},
		{"sign",                   required_argument, 0, 'g'},
		{"timestamp",              no_argument,       0, 't'},
		{"timestamp=none",         no_argument,       0, 0x104},
		{"component-compression",  required_argument, 0, 0x105},
		{"quiet",                  no_argument,       0, 'q'},
		{"help",                   no_argument,       0, 'h'},
		{"tool-version",           no_argument,       0, 'V'},
		{NULL, 0, 0, 0},
	};

	int c;
	while ((c = getopt_long(argc, argv, "hVrd:p:P:sf:S:i:g:tq:",
	    longopts, NULL)) != -1) {
		switch (c) {
		case 'r': root = optarg; break;
		case 0x100: component = optarg; break;
		case 0x101: content = optarg; break;
		case 'd': dist_path = optarg; break;
		case 'p': pkg_search_path = optarg; break;
		case 'P':
			if (pkg_list_add(&pkgs, optarg) != 0)
				return 1;
			break;
		case 's': synthesize = 1; break;
		case 'f':  /* --product plist: accepted, not parsed in v1 */
			break;
		case 'S': scripts_dir = optarg; break;
		case 'i': identifier = optarg; break;
		case 0x107:
			if (optarg == NULL) {
				printf("%s %s\n", program_name, PRODUCTBUILD_VERSION);
				return 0;
			}
			version = optarg;
			break;
		case 'g': sign_identity = optarg; break;
		case 't': timestamp = 1; break;
		case 0x104: /* --timestamp=none */ break;
		case 0x105: /* --component-compression: accepted */ break;
		case 'q': quiet = 1; break;
		case 'h':
			fprintf(stdout,
			    "usage: %s [options] --root <dir> <install-path> <output>\n"
			    "       %s [options] --component <bundle> <install-path> <output>\n"
			    "       %s [options] --content <dir> <output>\n"
			    "       %s [options] --distribution <dist> [--package-path <path>] <output>\n"
			    "       %s --synthesize [--package <pkg>] <output>\n",
			    program_name, program_name, program_name,
			    program_name, program_name);
			return 0;
		case 0x106:
		case 'V':
			printf("%s %s\n", program_name, PRODUCTBUILD_VERSION);
			return 0;
		case '?':
		default:
			fprintf(stderr,
			    "usage: %s [options] --root <dir> <install-path> <output>\n"
			    "       %s [options] --component <bundle> <install-path> <output>\n"
			    "       %s [options] --content <dir> <output>\n"
			    "       %s [options] --distribution <dist> [--package-path <path>] <output>\n"
			    "       %s --synthesize [--package <pkg>] <output>\n",
			    program_name, program_name, program_name,
			    program_name, program_name);
			return 1;
		}
	}

	/* Mode 5: --synthesize writes only the distribution XML */
	if (synthesize) {
		if (optind >= argc) {
			fprintf(stderr, "%s: output path required\n", program_name);
			return 1;
		}
		out = argv[optind];
		int rc = synthesize_from_packages(&pkgs, identifier,
		    version ? version : "1.0", out);
		pkg_list_free(&pkgs);
		return rc;
	}

	/* Modes 1-4: need positional arguments */
	if (optind >= argc) {
		fprintf(stderr, "%s: output path required\n", program_name);
		return 1;
	}

	/* Mode 4: --distribution <output> */
	if (dist_path != NULL) {
		out = argv[optind];
		int rc = build_from_distribution(dist_path, pkg_search_path,
		    out, sign_identity, quiet);
		pkg_list_free(&pkgs);
		return rc;
	}

	/* Mode 3: --content <output> */
	if (content != NULL) {
		out = argv[optind];
		int rc = build_root(content, "/", identifier,
		    version ? version : "1.0", scripts_dir, out, sign_identity);
		pkg_list_free(&pkgs);
		return rc;
	}

	/* Mode 2: --component <install-path> <output> */
	if (component != NULL) {
		const char *install_path = "/Applications";
		const char *out_path = argv[optind];
		if (optind + 1 < argc) {
			install_path = argv[optind];
			out_path = argv[optind + 1];
		}
		int rc = build_root(component, install_path, identifier,
		    version ? version : "1.0", scripts_dir, out_path, sign_identity);
		pkg_list_free(&pkgs);
		return rc;
	}

	/* Mode 1: --root <install-path> <output> */
	if (root != NULL) {
		const char *install_path = "/";
		const char *out_path = argv[optind];
		if (optind + 1 < argc) {
			install_path = argv[optind];
			out_path = argv[optind + 1];
		}
		int rc = build_root(root, install_path, identifier,
		    version ? version : "1.0", scripts_dir, out_path, sign_identity);
		pkg_list_free(&pkgs);
		return rc;
	}

	/* Default: --package mode */
	if (pkgs.count > 0) {
		const char *out_path = argv[optind];

		char tmpl[] = "/tmp/productbuild.XXXXXX";
		char *stage = mkdtemp(tmpl);
		if (stage == NULL) {
			fprintf(stderr, "%s: cannot create staging dir\n",
			    program_name);
			pkg_list_free(&pkgs);
			return 1;
		}

		size_t dist_len = 0;
		char *dist = dist_synthesize(
		    identifier ? identifier : "com.apple.unknown",
		    version ? version : "1.0",
		    pkgs.items[0], out_path, 1, NULL, &dist_len);
		if (dist == NULL) {
			fprintf(stderr, "%s: failed to synthesize Distribution\n",
			    program_name);
			remove_tree(stage);
			pkg_list_free(&pkgs);
			return 1;
		}

		char dist_stage[PATH_MAX];
		snprintf(dist_stage, sizeof dist_stage, "%s/Distribution", stage);
		if (write_file(dist_stage, dist, dist_len) != 0) {
			fprintf(stderr, "%s: cannot write Distribution\n",
			    program_name);
			free(dist);
			remove_tree(stage);
			pkg_list_free(&pkgs);
			return 1;
		}
		free(dist);

		/* Copy component packages into staging */
		for (int i = 0; i < pkgs.count; i++) {
			char dest[PATH_MAX];
			const char *base = strrchr(pkgs.items[i], '/');
			base = base ? base + 1 : pkgs.items[i];
			snprintf(dest, sizeof dest, "%s/%s", stage, base);

			size_t plen = 0;
			unsigned char *pdata = read_file(pkgs.items[i], &plen);
			if (pdata != NULL) {
				write_file(dest, pdata, plen);
				free(pdata);
			}
		}

		/* Use system xar with --compression none */
		pid_t pid = fork();
		if (pid == 0) {
			char xar_cmd[PATH_MAX * 2];
			snprintf(xar_cmd, sizeof xar_cmd,
			    "cd '%s' && xar --compression none -c -f '%s' $(ls -1)",
			    stage, out_path);
			execlp("sh", "sh", "-c", xar_cmd, (char *)NULL);
			_exit(127);
		}
		int status = 0;
		waitpid(pid, &status, 0);
		remove_tree(stage);

		if (!(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
			fprintf(stderr, "%s: xar failed\n", program_name);
			pkg_list_free(&pkgs);
			return 1;
		}

		if (sign_identity != NULL)
			sign_package(out_path, sign_identity);

		fprintf(stderr, "productbuild: %s\n", out_path);
		pkg_list_free(&pkgs);
		return 0;
	}

	fprintf(stderr, "%s: one of --root, --component, --content, "
	    "--distribution, or --package must be specified\n", program_name);
	return 1;
}
/* --- mode 2: --component <bundle> <install-path> <output> --- */
/* A component bundle is packaged the same way as a root tree; the
 * "root" is effectively the bundle itself with the given install path. */

/* --- mode 3: --content <dir> <output> --- */
/* Content packaging: root = the content dir, install path = /. */

/* --- mode 5: --synthesize [--package <pkg>] <output> --- */

static int
synthesize_from_packages(struct pkg_list *pkgs,
    const char *identifier, const char *version, const char *out)
{
	if (pkgs->count == 0) {
		fprintf(stderr, "%s: --synthesize requires --package <pkg>\n",
		    program_name);
		return 1;
	}

	/* For each package, we need to extract its PackageInfo to determine
	 * the install location. We use pkgutil --expand-full. */
	char tmpdir[PATH_MAX];
	snprintf(tmpdir, sizeof tmpdir, "/tmp/productbuild-synth.%d",
	    (int)getpid());
	if (mkdir(tmpdir, 0755) != 0 && errno != EEXIST) {
		fprintf(stderr, "%s: cannot create temp dir\n", program_name);
		return 1;
	}

	const char *pkg = pkgs->items[0];
	char cmd[PATH_MAX * 2];
	snprintf(cmd, sizeof cmd, "pkgutil --expand-full '%s' %s/pkg 2>/dev/null",
	    pkg, tmpdir);
	int rc = system(cmd);
	if (rc != 0) {
		fprintf(stderr, "%s: cannot expand %s\n", program_name, pkg);
		snprintf(cmd, sizeof cmd, "rm -rf %s", tmpdir);
		system(cmd);
		return 1;
	}

	/* Read PackageInfo from the expanded package */
	char pi_path[PATH_MAX];
	snprintf(pi_path, sizeof pi_path, "%s/pkg/PackageInfo", tmpdir);
	size_t pi_len = 0;
	char *pkginfo = (char *)read_file(pi_path, &pi_len);
	if (pkginfo == NULL) {
		/* Try root.pkg/PackageInfo */
		snprintf(pi_path, sizeof pi_path, "%s/pkg/root.pkg/PackageInfo",
		    tmpdir);
		pkginfo = (char *)read_file(pi_path, &pi_len);
	}

	char install_loc[256] = "";
	char pkg_id[256] = "";
	if (pkginfo != NULL) {
		char *p = strstr(pkginfo, "identifier=\"");
		if (p) {
			p += 12;
			char *e = strchr(p, '"');
			if (e) {
				*e = '\0';
				strncpy(pkg_id, p, sizeof pkg_id - 1);
				pkg_id[sizeof pkg_id - 1] = '\0';
			}
		}
		p = strstr(pkginfo, "install-location=\"");
		if (p) {
			p += 17;
			char *e = strchr(p, '"');
			if (e) {
				*e = '\0';
				strncpy(install_loc, p, sizeof install_loc - 1);
				install_loc[sizeof install_loc - 1] = '\0';
			}
		}
		free(pkginfo);
	}

	snprintf(cmd, sizeof cmd, "rm -rf %s", tmpdir);
	system(cmd);

	/* Derive package base name for the #pkg-ref href */
	const char *base = strrchr(pkgs->items[0], '/');
	base = base ? base + 1 : pkgs->items[0];

	size_t dist_len = 0;
	char *dist = dist_synthesize(pkg_id[0] ? pkg_id :
	    (identifier ? identifier : "com.apple.unknown"),
	    version ? version : "1.0",
	    base,
	    install_loc[0] ? install_loc : NULL,
	    1, NULL, &dist_len);
	if (dist == NULL) {
		fprintf(stderr, "%s: failed to synthesize Distribution\n",
		    program_name);
		return 1;
	}

	int r = write_file(out, dist, dist_len);
	free(dist);
	if (r != 0) {
		fprintf(stderr, "%s: cannot write %s\n", program_name, out);
		return 1;
	}
	fprintf(stderr, "productbuild: %s\n", out);
	return 0;
}

/* --- mode 4: --distribution <dist> --package-path <path> <output> --- */

static int
build_from_distribution(const char *dist_path, const char *pkg_search_path,
    const char *out, const char *sign_identity, int quiet)
{
	size_t dist_len = 0;
	unsigned char *dist_data = read_file(dist_path, &dist_len);
	if (dist_data == NULL) {
		fprintf(stderr, "%s: cannot read %s\n", program_name, dist_path);
		return 1;
	}

	char tmpl[] = "/tmp/productbuild.XXXXXX";
	char *stage = mkdtemp(tmpl);
	if (stage == NULL) {
		fprintf(stderr, "%s: cannot create staging dir\n", program_name);
		free(dist_data);
		return 1;
	}

	/* Write Distribution file */
	char dist_stage[PATH_MAX];
	snprintf(dist_stage, sizeof dist_stage, "%s/Distribution", stage);
	if (write_file(dist_stage, dist_data, dist_len) != 0) {
		fprintf(stderr, "%s: cannot write Distribution\n", program_name);
		free(dist_data);
		remove_tree(stage);
		return 1;
	}

	const char *search = pkg_search_path ? pkg_search_path : ".";

	/* Scan for pkg-ref references in the Distribution XML.
	 * These can be href="#name" attributes or text content
	 * <pkg-ref ...>#name</pkg-ref>. */
	char *p = (char *)dist_data;
	while ((p = strstr(p, "pkg-ref")) != NULL) {
		char *pkg_name = NULL;
		char *end = NULL;

		/* Try href="#name" attribute first */
		char *href = strstr(p, "href=\"#");
		if (href != NULL) {
			href += 7; /* skip href="# */
			end = strchr(href, '"');
			if (end != NULL)
				*end = '\0';
			pkg_name = href;
		} else {
			/* Try text content: <pkg-ref ...>#name</pkg-ref> */
			char *gt = strchr(p, '>');
			if (gt != NULL) {
				char *hash = strchr(gt, '#');
				char *close = strstr(gt, "</pkg-ref");
				if (hash != NULL && close != NULL && hash < close) {
					pkg_name = hash + 1;
					end = strchr(pkg_name, '<');
					if (end != NULL)
						*end = '\0';
				}
			}
		}

		if (pkg_name == NULL || pkg_name[0] == '\0') {
			p += 7;
			continue;
		}

		/* Look for the .pkg file in the search path */
		char pkg_full[PATH_MAX];
		snprintf(pkg_full, sizeof pkg_full, "%s/%s", search, pkg_name);

		/* Extract the .pkg to staging/<name>/ using pkgutil --expand-full */
		char dest[PATH_MAX];
		snprintf(dest, sizeof dest, "%s/%s", stage, pkg_name);

		char cmd[PATH_MAX * 2];
		snprintf(cmd, sizeof cmd,
		    "pkgutil --expand-full '%s' '%s' 2>/dev/null", pkg_full, dest);
		if (system(cmd) != 0) {
			if (!quiet)
				fprintf(stderr, "%s: warning: cannot expand %s\n",
				    program_name, pkg_full);
			p = end ? end + 1 : p + 7;
			continue;
		}

		if (!quiet)
			fprintf(stderr, "%s: incorporating %s\n", program_name, pkg_name);

		p = end ? end + 1 : p + 7;
	}

	free(dist_data);

	/* Use system xar with --compression none to create the archive */
	pid_t pid = fork();
	if (pid == 0) {
		char xar_cmd[PATH_MAX * 2];
		snprintf(xar_cmd, sizeof xar_cmd,
		    "cd '%s' && xar --compression none -c -f '%s' $(ls -1)",
		    stage, out);
		execlp("sh", "sh", "-c", xar_cmd, (char *)NULL);
		_exit(127);
	}
	int status = 0;
	waitpid(pid, &status, 0);
	remove_tree(stage);

	if (!(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
		fprintf(stderr, "%s: xar failed (exit %d)\n", program_name,
		    WIFEXITED(status) ? WEXITSTATUS(status) : -1);
		return 1;
	}

	if (sign_identity != NULL)
		sign_package(out, sign_identity);

	fprintf(stderr, "productbuild: %s\n", out);
	return 0;
}


