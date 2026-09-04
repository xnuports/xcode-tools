/*
 * xc_record - xctrace record subcommand.
 *
 * Recording is not implemented, and this says so rather than arranging
 * for it to appear to work.
 *
 * What recording takes is a trace of the kernel's own instrumentation --
 * kdebug and kperf, reached through interfaces Apple does not publish --
 * written into the .trace bundle, whose format is likewise undocumented.
 * Neither is reimplemented here, and neither can be honestly faked.
 *
 * This file used to fork and exec /usr/bin/xctrace, or fall back to
 * opening Instruments.app.  That made the command appear to succeed on a
 * machine with Xcode installed, producing Apple's output from Apple's
 * binary -- a tool that runs the tool it replaces is not a replacement,
 * and one that looks like it works is worse than one that admits it does
 * not.  Options are still parsed, so the diagnostic can be specific and
 * so a caller's arguments are validated rather than ignored.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xctrace.h"

static const char *program_name = "xctrace";

static void
record_usage(FILE *fp)
{
	fprintf(fp,
	    "Usage: xctrace record [options]\n"
	    "  --output <path>          write the trace to <path>\n"
	    "  --template <name|path>   trace template to record with\n"
	    "  --device <name|udid>     device to record on\n"
	    "  --attach <pid|name>      attach to a running process\n"
	    "  --all-processes          record every process\n"
	    "  --time-limit <duration>  stop after <duration>\n"
	    "  --launch -- <command>    launch and record <command>\n"
	    "  --no-prompt              do not prompt\n"
	    "  --quiet                  suppress progress output\n"
	    "\n"
	    "Recording is not implemented; see xctrace list and"
	    " xctrace export.\n");
}

/*
 * xc_record - handle "xctrace record ..."
 */
int
xc_record(int argc, char **argv, int optind, int quiet)
{
	const char *output = NULL;
	const char *template = NULL;
	const char *device = NULL;
	const char *attach = NULL;
	const char *time_limit = NULL;
	const char *launch_cmd = NULL;
	int all_processes = 0;
	int no_prompt = 0;

	(void)output;
	(void)device;
	(void)attach;
	(void)time_limit;
	(void)launch_cmd;
	(void)all_processes;
	(void)no_prompt;

	for (int i = optind; i < argc; i++) {
		if (strcmp(argv[i], "--help") == 0 ||
		    strcmp(argv[i], "-h") == 0) {
			record_usage(stdout);
			return 0;
		} else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
			output = argv[++i];
		} else if (strcmp(argv[i], "--template") == 0 && i + 1 < argc) {
			template = argv[++i];
		} else if (strcmp(argv[i], "--device") == 0 && i + 1 < argc) {
			device = argv[++i];
		} else if (strcmp(argv[i], "--attach") == 0 && i + 1 < argc) {
			attach = argv[++i];
		} else if (strcmp(argv[i], "--all-processes") == 0) {
			all_processes = 1;
		} else if (strcmp(argv[i], "--time-limit") == 0 && i + 1 < argc) {
			time_limit = argv[++i];
		} else if (strcmp(argv[i], "--launch") == 0) {
			if (i + 1 < argc && strcmp(argv[i + 1], "--") == 0) {
				launch_cmd = argv[i + 2];
				i += 2;
			}
		} else if (strcmp(argv[i], "--no-prompt") == 0) {
			no_prompt = 1;
		} else if (strcmp(argv[i], "--quiet") == 0) {
			quiet = 1;
		} else if (argv[i][0] == '-') {
			fprintf(stderr, "%s: unknown option %s\n",
			    program_name, argv[i]);
			record_usage(stderr);
			return 1;
		}
	}

	fprintf(stderr, "%s: record is not implemented.\n", program_name);
	fprintf(stderr, "%s: recording reads the kernel trace facilities"
	    " through interfaces Apple\n", program_name);
	fprintf(stderr, "%s: does not publish, and writes the undocumented"
	    " .trace format; neither\n", program_name);
	fprintf(stderr, "%s: is reimplemented here.  list and export do work.\n",
	    program_name);

	if (template != NULL && !quiet)
		fprintf(stderr, "%s: (template '%s' was not used)\n",
		    program_name, template);

	return 1;
}
