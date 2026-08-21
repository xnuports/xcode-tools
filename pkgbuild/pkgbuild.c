/*
 * pkgbuild - open reimplementation of Apple's pkgbuild(1).
 *
 * Builds pkgutil-valid .pkg archives with a self-contained xar writer
 * (xar.c) and SHA-1 checksum (CommonCrypto), delegating cpio copy-out and
 * BOM generation to the system cpio(1)/mkbom(1). See README for rationale.
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
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "xar.h"
#include "analyze.h"
#include "payload.h"
#include "bom.h"

#define PKGBUILD_VERSION "0.1.0"

static const char *program_name = "pkgbuild";

static void
usage(int status)
{
	FILE *out = status == 0 ? stdout : stderr;
	fprintf(out,
"usage: %s --root <dir> [options] -o <pkg>\n"
"       %s --analyze --root <dir> [options]\n"
"       %s --inspect <pkg>\n"
"\n"
"options:\n"
"  --root <dir>            source root to package\n"
"  --component <dir>       alias of --root\n"
"  --identifier <id>       bundle identifier (default com.apple.unknown)\n"
"  --version <v>           package version (default 1.0)\n"
"  --install-location <dir> install path; omits it => relocatable\n"
"  --scripts <dir>         pre/post-install scripts (v1: accepted, not run)\n"
"  --sign <identity>       codesign the resulting archive\n"
"  --dev <dir>             developer dir for code signing (v1: accepted)\n"
"  --analyze               print the PackageInfo XML and exit\n"
"  --inspect <pkg>         list archive contents and exit\n"
"  -o <pkg>                output path (default ./package.pkg)\n"
"  -h, --help              show this help\n"
"  -V, --tool-version      print %s version and exit\n",
	    program_name, program_name, program_name, PKGBUILD_VERSION);
	exit(status);
}

static void
print_version(void)
{
	printf("%s %s\n", program_name, PKGBUILD_VERSION);
}

/* Read an entire file into a malloc'd buffer. */
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

int
main(int argc, char **argv)
{
	program_name = argv[0];

	const char *root = NULL;
	const char *component = NULL;
	const char *identifier = NULL;
	const char *version = NULL;
	const char *install_location = NULL;
	const char *scripts_dir = NULL;
	const char *sign_identity = NULL;
	const char *dev_dir = NULL;
	const char *inspect_pkg = NULL;
	const char *out_path = NULL;
	int analyze = 0;

	static struct option longopts[] = {
		{"root",            required_argument, 0, 'r'},
		{"component",       required_argument, 0, 'c'},
		{"identifier",      required_argument, 0, 'i'},
		{"version",         required_argument, 0, 'v'},
		{"install-location",required_argument, 0, 'L'},
		{"scripts",         required_argument, 0, 'S'},
		{"sign",            required_argument, 0, 'g'},
		{"dev",             required_argument, 0, 'd'},
		{"analyze",         no_argument,       0, 'a'},
		{"inspect",         required_argument, 0, 'I'},
		{"help",            no_argument,       0, 'h'},
		{"tool-version",    no_argument,       0, 'V'},
		{NULL, 0, 0, 0},
	};

	int c;
	while ((c = getopt_long(argc, argv, "a:hV:r:c:i:L:o:S:g:d:v:I:",
	    longopts, NULL)) != -1) {
		switch (c) {
		case 'r': root = optarg; break;
		case 'c': component = optarg; break;
		case 'i': identifier = optarg; break;
		case 'v': version = optarg; break;
		case 'L': install_location = optarg; break;
		case 'S': scripts_dir = optarg; break;
		case 'g': sign_identity = optarg; break;
		case 'd': dev_dir = optarg; break;
		case 'a': analyze = 1; break;
		case 'I': inspect_pkg = optarg; break;
		case 'o': out_path = optarg; break;
		case 'h': usage(0);
		case 'V': print_version(); return 0;
		case '?':
		default: usage(1);
		}
	}

	(void)scripts_dir;
	(void)dev_dir;

	/* Mode: inspect an existing archive. */
	if (inspect_pkg != NULL) {
		size_t plen = 0;
		unsigned char *pkg = read_file(inspect_pkg, &plen);
		if (pkg == NULL) {
			fprintf(stderr, "%s: cannot read %s\n", program_name,
			    inspect_pkg);
			return 1;
		}
		int rc = xar_dump(pkg, plen);
		free(pkg);
		return rc != 0;
	}

	const char *src = root ? root : component;
	if (src == NULL) {
		fprintf(stderr, "%s: must specify --root <dir>\n", program_name);
		usage(1);
	}

	/* Build the payload (cpio/odc + gzip) and BOM. */
	struct payload pl;
	if (payload_build(src, &pl) != 0) {
		fprintf(stderr, "%s: failed to build Payload\n", program_name);
		return 1;
	}
	size_t bom_len = 0;
	unsigned char *bom = bom_build(src, &bom_len);
	if (bom == NULL) {
		fprintf(stderr, "%s: failed to build BOM\n", program_name);
		free(pl.data);
		return 1;
	}

	size_t install_kbytes = (pl.uncompressed + 1023) / 1024;
	if (install_kbytes == 0)
		install_kbytes = 1;

	size_t pkginfo_len = 0;
	char *pkginfo = pkginfo_build(identifier, version ? version : "1.0",
	    install_location, pl.file_count, install_kbytes, &pkginfo_len);
	if (pkginfo == NULL) {
		fprintf(stderr, "%s: failed to build PackageInfo\n", program_name);
		free(pl.data);
		free(bom);
		return 1;
	}

	if (analyze) {
		fwrite(pkginfo, 1, pkginfo_len, stdout);
		free(pkginfo);
		free(bom);
		free(pl.data);
		return 0;
	}

	/* Assemble the xar archive. */
	struct xar_entry entries[3];
	entries[0].name = "Bom";
	entries[0].encoding = "application/x-gzip";
	entries[0].data = bom;
	entries[0].size = bom_len;
	entries[0].compressed = 1;

	entries[1].name = "Payload";
	entries[1].encoding = "application/octet-stream";
	entries[1].data = pl.data;
	entries[1].size = pl.size;       /* stored raw: <size> == bytes stored == gzip length */
	entries[1].compressed = 0;

	entries[2].name = "PackageInfo";
	entries[2].encoding = "application/x-gzip";
	entries[2].data = pkginfo;
	entries[2].size = pkginfo_len;
	entries[2].compressed = 1;

	size_t arch_len = 0;
	unsigned char *arch = xar_build(entries, 3, &arch_len);
	free(pkginfo);
	free(bom);
	free(pl.data);
	if (arch == NULL) {
		fprintf(stderr, "%s: failed to assemble archive\n", program_name);
		return 1;
	}

	const char *out = out_path ? out_path : "package.pkg";
	if (write_file(out, arch, arch_len) != 0) {
		fprintf(stderr, "%s: cannot write %s\n", program_name, out);
		free(arch);
		return 1;
	}
	free(arch);

	if (sign_identity != NULL) {
		pid_t pid = fork();
		if (pid == 0) {
			execlp("codesign", "codesign", "--force", "--sign",
			    sign_identity, out, (char *)NULL);
			_exit(127);
		}
		int status = 0;
		waitpid(pid, &status, 0);
		if (!(WIFEXITED(status) && WEXITSTATUS(status) == 0))
			fprintf(stderr, "%s: codesign failed\n", program_name);
	}

	fprintf(stderr, "pkgbuild: %s\n", out);
	return 0;
}
