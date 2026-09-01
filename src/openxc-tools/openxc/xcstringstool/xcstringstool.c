/*
 * xcstringstool - open source reimplementation of Apple's xcstringstool(1).
 *
 * A string catalog (.xcstrings) is JSON holding every localization of
 * every key in one file.  The build turns it back into the per-language
 * .strings and .stringsdict files the frameworks actually load, which is
 * what `compile` does; `print` lists the keys.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xcstringstool.h"

static const char *program_name = "xcstringstool";

static void
usage(FILE *fp)
{
	fprintf(fp,
	    "OVERVIEW: Work with .xcstrings files\n"
	    "\n"
	    "USAGE: xcstringstool <subcommand>\n"
	    "\n"
	    "OPTIONS:\n"
	    "  -h, --help              Show help information.\n"
	    "\n"
	    "SUBCOMMANDS:\n"
	    "  print                   Prints all string keys represented in an\n"
	    "                          xcstrings file.\n"
	    "  compile                 Produces build products for an .xcstrings\n"
	    "                          file.\n"
	    "\n"
	    "  See 'xcstringstool help <subcommand>' for detailed help.\n");
}

static void
print_usage(FILE *fp)
{
	fprintf(fp,
	    "OVERVIEW: Prints all string keys represented in an xcstrings file.\n"
	    "\n"
	    "USAGE: xcstringstool print <input-file>\n"
	    "\n"
	    "ARGUMENTS:\n"
	    "  <input-file>            The path to the .xcstrings file to print.\n"
	    "\n"
	    "OPTIONS:\n"
	    "  -h, --help              Show help information.\n");
}

static void
compile_usage(FILE *fp)
{
	fprintf(fp,
	    "OVERVIEW: Produces build products for an .xcstrings file.\n"
	    "\n"
	    "USAGE: xcstringstool compile <input-file>"
	    " --output-directory <output-directory>\n"
	    "       [--format <format>] [--language <language> ...]\n"
	    "       [--serialization-format <serialization-format>] [--dry-run]\n"
	    "\n"
	    "ARGUMENTS:\n"
	    "  <input-file>            The path to the .xcstrings file to compile.\n"
	    "\n"
	    "OPTIONS:\n"
	    "  -o, --output-directory <output-directory>\n"
	    "                          The directory to place output files.\n"
	    "  -f, --format <format>   The output format for the overall\n"
	    "                          compilation (values: stringsAndStringsdict,\n"
	    "                          stringsdictOnly;"
	    " default: stringsAndStringsdict)\n"
	    "  -l, --language <language>\n"
	    "                          Optionally specify particular languages to\n"
	    "                          compile.  Repeatable.\n"
	    "  --serialization-format <serialization-format>\n"
	    "                          The output format for individual files\n"
	    "                          (values: text, binary; default: text)\n"
	    "  --dry-run               Outputs a newline-separated list of output\n"
	    "                          paths that would be produced.\n"
	    "  -h, --help              Show help information.\n");
}

char *
xs_read_file(const char *path, size_t *len)
{
	FILE *fp;
	char *buf;
	long size;

	if ((fp = fopen(path, "rb")) == NULL)
		return NULL;

	if (fseek(fp, 0, SEEK_END) != 0 || (size = ftell(fp)) < 0) {
		fclose(fp);
		return NULL;
	}
	rewind(fp);

	if ((buf = malloc((size_t)size + 1)) == NULL) {
		fclose(fp);
		return NULL;
	}

	if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
		free(buf);
		fclose(fp);
		return NULL;
	}
	fclose(fp);

	buf[size] = '\0';
	if (len != NULL)
		*len = (size_t)size;

	return buf;
}

/*
 * Subcommands this does not implement.  They are named rather than
 * dropped so the failure says which tool is missing what, instead of
 * looking like a bad argument.
 */
static int
unimplemented(const char *sub)
{
	fprintf(stderr, "%s: %s is not implemented.\n", program_name, sub);
	fprintf(stderr, "%s: print and compile are.\n", program_name);
	return 1;
}

static int
do_compile(int argc, char **argv)
{
	struct xs_compile_opts opts;
	const char *languages[64];
	int i;

	memset(&opts, 0, sizeof(opts));
	opts.format = XS_STRINGS_AND_STRINGSDICT;
	opts.languages = languages;

	for (i = 0; i < argc; i++) {
		const char *a = argv[i];

		if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
			compile_usage(stdout);
			return 0;
		} else if ((strcmp(a, "-o") == 0 ||
		    strcmp(a, "--output-directory") == 0) && i + 1 < argc) {
			opts.output_dir = argv[++i];
		} else if ((strcmp(a, "-f") == 0 ||
		    strcmp(a, "--format") == 0) && i + 1 < argc) {
			const char *v = argv[++i];

			if (strcmp(v, "stringsAndStringsdict") == 0)
				opts.format = XS_STRINGS_AND_STRINGSDICT;
			else if (strcmp(v, "stringsdictOnly") == 0)
				opts.format = XS_STRINGSDICT_ONLY;
			else {
				fprintf(stderr, "%s: unknown format '%s'\n",
				    program_name, v);
				return 1;
			}
		} else if ((strcmp(a, "-l") == 0 ||
		    strcmp(a, "--language") == 0) && i + 1 < argc) {
			if (opts.nlanguages >=
			    sizeof(languages) / sizeof(languages[0])) {
				fprintf(stderr, "%s: too many --language options\n",
				    program_name);
				return 1;
			}
			languages[opts.nlanguages++] = argv[++i];
		} else if (strcmp(a, "--serialization-format") == 0 &&
		    i + 1 < argc) {
			const char *v = argv[++i];

			if (strcmp(v, "text") == 0)
				opts.binary = 0;
			else if (strcmp(v, "binary") == 0)
				opts.binary = 1;
			else {
				fprintf(stderr,
				    "%s: unknown serialization format '%s'\n",
				    program_name, v);
				return 1;
			}
		} else if (strcmp(a, "--dry-run") == 0) {
			opts.dry_run = 1;
		} else if (a[0] == '-') {
			fprintf(stderr, "%s: unknown option %s\n", program_name, a);
			compile_usage(stderr);
			return 1;
		} else if (opts.input == NULL) {
			opts.input = a;
		} else {
			fprintf(stderr, "%s: unexpected argument %s\n",
			    program_name, a);
			return 1;
		}
	}

	if (opts.input == NULL) {
		fprintf(stderr, "%s: compile requires an input file\n",
		    program_name);
		compile_usage(stderr);
		return 1;
	}
	if (opts.output_dir == NULL) {
		fprintf(stderr, "%s: compile requires --output-directory\n",
		    program_name);
		compile_usage(stderr);
		return 1;
	}

	return xs_compile(&opts);
}

static int
do_print(int argc, char **argv)
{
	const char *input = NULL;
	int i;

	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 ||
		    strcmp(argv[i], "--help") == 0) {
			print_usage(stdout);
			return 0;
		}
		if (argv[i][0] == '-') {
			fprintf(stderr, "%s: unknown option %s\n",
			    program_name, argv[i]);
			return 1;
		}
		if (input != NULL) {
			fprintf(stderr, "%s: unexpected argument %s\n",
			    program_name, argv[i]);
			return 1;
		}
		input = argv[i];
	}

	if (input == NULL) {
		fprintf(stderr, "%s: print requires an input file\n",
		    program_name);
		print_usage(stderr);
		return 1;
	}

	return xs_print(input);
}

int
main(int argc, char **argv)
{
	const char *sub;

	if (argc < 2) {
		usage(stderr);
		return 1;
	}

	sub = argv[1];

	if (strcmp(sub, "-h") == 0 || strcmp(sub, "--help") == 0 ||
	    strcmp(sub, "help") == 0) {
		/* "help <subcommand>" prints that subcommand's help. */
		if (argc >= 3) {
			if (strcmp(argv[2], "print") == 0)
				print_usage(stdout);
			else if (strcmp(argv[2], "compile") == 0)
				compile_usage(stdout);
			else
				usage(stdout);
			return 0;
		}
		usage(stdout);
		return 0;
	}

	if (strcmp(sub, "print") == 0)
		return do_print(argc - 2, argv + 2);
	if (strcmp(sub, "compile") == 0)
		return do_compile(argc - 2, argv + 2);

	if (strcmp(sub, "sync") == 0 || strcmp(sub, "extract") == 0 ||
	    strcmp(sub, "generate-symbols") == 0 ||
	    strcmp(sub, "installloc") == 0)
		return unimplemented(sub);

	fprintf(stderr, "%s: unknown subcommand '%s'\n", program_name, sub);
	usage(stderr);
	return 1;
}
