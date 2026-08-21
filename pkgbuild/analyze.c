/*
 * analyze - PackageInfo XML construction.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "analyze.h"

static int
aprintf(char **out, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int r = vasprintf(out, fmt, ap);
	va_end(ap);
	return r;
}

char *
pkginfo_build(const char *identifier, const char *version,
    const char *install_location, int number_of_files,
    size_t install_kbytes, size_t *out_len)
{
	char loc_attr[512];
	if (install_location != NULL)
		snprintf(loc_attr, sizeof loc_attr,
		    " install-location=\"%s\"", install_location);
	else
		loc_attr[0] = '\0';

	char *head = NULL;
	if (aprintf(&head,
	    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
	    "<pkg-info overwrite-permissions=\"true\" relocatable=\"%s\" "
	    "identifier=\"%s\" postinstall-action=\"none\" version=\"%s\" "
	    "format-version=\"2\" generator-version=\"pkgbuild-devel\" "
	    "auth=\"root\"%s>\n"
	    "    <payload numberOfFiles=\"%d\" installKBytes=\"%zu\"/>\n",
	    install_location ? "false" : "true",
	    identifier ? identifier : "com.apple.unknown",
	    version ? version : "1.0",
	    loc_attr,
	    number_of_files > 0 ? number_of_files : 1,
	 install_kbytes) < 0 || head == NULL)
		return NULL;

	char *tail = NULL;
	if (aprintf(&tail,
	    "    <bundle-version/>\n"
	    "    <upgrade-bundle/>\n"
	    "    <update-bundle/>\n"
	    "    <atomic-update-bundle/>\n"
	    "    <strict-identifier/>\n"
	    "    <relocate/>\n"
	    "</pkg-info>\n") < 0 || tail == NULL) {
		free(head);
		return NULL;
	}

	size_t hl = strlen(head);
	size_t tl = strlen(tail);
	char *both = malloc(hl + tl + 1);
	if (both == NULL) {
		free(head);
		free(tail);
		return NULL;
	}
	memcpy(both, head, hl);
	memcpy(both + hl, tail, tl + 1);
	free(head);
	free(tail);

	*out_len = hl + tl;
	return both;
}
