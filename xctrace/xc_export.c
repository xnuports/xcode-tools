/*
 * xc_export - xctrace export subcommand implementation.
 *
 * Reads a .trace bundle (directory) and exports its contents.
 * Supports --toc (table of contents) mode.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include "xctrace.h"

static const char *program_name = "xctrace";

/* Determine if a path is a directory. */
static int
is_dir(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* Recursively print directory contents for TOC mode. */
static void
print_toc(const char *path, const char *prefix, FILE *out)
{
	struct dirent **entries;
	int n = scandir(path, &entries, NULL, alphasort);
	if (n < 0)
		return;

	for (int i = 0; i < n; i++) {
		if (strcmp(entries[i]->d_name, ".") == 0 ||
		    strcmp(entries[i]->d_name, "..") == 0) {
			free(entries[i]);
			continue;
		}

		char full[4096];
		snprintf(full, sizeof full, "%s/%s", path, entries[i]->d_name);

		struct stat st;
		if (stat(full, &st) == 0) {
			if (S_ISDIR(st.st_mode)) {
				fprintf(out, "%s%s/\n", prefix, entries[i]->d_name);
				char subprefix[256];
				snprintf(subprefix, sizeof subprefix,
				    "%s  ", prefix);
				print_toc(full, subprefix, out);
			} else {
				fprintf(out, "%s%s (%lld bytes)\n", prefix,
				    entries[i]->d_name,
				    (long long)st.st_size);
			}
		}
		free(entries[i]);
	}
	free(entries);
}

/*
 * export_command - handle "xctrace export ..."
 *
 * Options:
 *   --input <file>    .trace file to export
 *   --output <path>   Output file (default: stdout)
 *   --toc             Table of contents mode
 */
int
xc_export(int argc, char **argv, int optind, int quiet)
{
	const char *input = NULL;
	const char *output = NULL;
	int toc_mode = 0;

	for (int i = optind; i < argc; i++) {
		if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) {
			input = argv[++i];
		} else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
			output = argv[++i];
		} else if (strcmp(argv[i], "--toc") == 0) {
			toc_mode = 1;
		} else if (strcmp(argv[i], "--quiet") == 0) {
			quiet = 1;
		}
	}

	if (input == NULL) {
		fprintf(stderr, "%s: export requires --input <file>\n",
		    program_name);
		return 1;
	}

	FILE *out = stdout;
	if (output != NULL) {
		out = fopen(output, "w");
		if (out == NULL) {
			fprintf(stderr, "%s: cannot open %s for writing\n",
			    program_name, output);
			return 1;
		}
	}

	if (toc_mode) {
		fprintf(out, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
		    "<trace-toc>\n");
		if (is_dir(input)) {
			print_toc(input, "  ", out);
		} else {
			fprintf(stderr, "%s: %s is not a valid .trace bundle\n",
			    program_name, input);
			if (out != stdout)
				fclose(out);
			return 1;
		}
		fprintf(out, "</trace-toc>\n");
	} else {
		fprintf(stderr, "%s: export modes other than --toc are not "
		    "yet supported\n", program_name);
		if (out != stdout)
			fclose(out);
		return 1;
	}

	if (out != stdout)
		fclose(out);
	if (!quiet)
		fprintf(stderr, "%s: export complete\n", program_name);
	return 0;
}
