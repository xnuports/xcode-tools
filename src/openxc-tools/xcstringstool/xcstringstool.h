/*
 * xcstringstool.h -- shared declarations.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __XCSTRINGSTOOL_H__
#define __XCSTRINGSTOOL_H__

#include "json.h"

#define XCSTRINGS_VERSION	"0.1.0"

/* Overall output selection, mirroring --format. */
enum xs_format {
	XS_STRINGS_AND_STRINGSDICT,
	XS_STRINGSDICT_ONLY
};

struct xs_compile_opts {
	const char *input;
	const char *output_dir;
	enum xs_format format;
	int binary;		/* --serialization-format binary */
	int dry_run;
	const char **languages;	/* --language, NULL for every language */
	size_t nlanguages;
};

int xs_print(const char *path);
int xs_compile(const struct xs_compile_opts *opts);

/* Read a whole file; caller frees.  Sets *len when not NULL. */
char *xs_read_file(const char *path, size_t *len);

#endif /* __XCSTRINGSTOOL_H__ */
