/*
 * dist - synthesize an installer-gui-script Distribution XML.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Mirrors the Distribution file that Apple's productbuild writes when
 * invoked with --root (or --component / --content), synthesizing a
 * distribution from the single component package.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dist.h"

char *
dist_synthesize(const char *identifier, const char *version,
    const char *pkg_name, const char *install_location,
    size_t install_kbytes, const char *host_arch, size_t *out_len)
{
	if (identifier == NULL)
		identifier = "com.apple.unknown";
	if (version == NULL)
		version = "1.0";
	if (pkg_name == NULL)
		pkg_name = "root.pkg";
	if (host_arch == NULL || *host_arch == '\0')
		host_arch = "x86_64,arm64";

	char *xml = NULL;
	if (install_location != NULL) {
		if (asprintf(&xml,
		    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
		    "<installer-gui-script minSpecVersion=\"1\">\n"
		    "    <pkg-ref id=\"%s\">\n"
		    "        <bundle-version/>\n"
		    "    </pkg-ref>\n"
		    "    <options customize=\"never\" require-scripts=\"false\" hostArchitectures=\"%s\"/>\n"
		    "    <choices-outline>\n"
		    "        <line choice=\"default\">\n"
		    "            <line choice=\"%s\"/>\n"
		    "        </line>\n"
		    "    </choices-outline>\n"
		    "    <choice id=\"default\"/>\n"
		    "    <choice id=\"%s\" visible=\"false\" customLocation=\"%s\">\n"
		    "        <pkg-ref id=\"%s\"/>\n"
		    "    </choice>\n"
		    "    <pkg-ref id=\"%s\" version=\"%s\" onConclusion=\"none\" installKBytes=\"%zu\" updateKBytes=\"0\">#%s</pkg-ref>\n"
		    "</installer-gui-script>\n",
		    identifier, host_arch,
		    pkg_name, pkg_name,
		    install_location, pkg_name,
		    pkg_name, version, install_kbytes, pkg_name) < 0 || xml == NULL)
			return NULL;
	} else {
		if (asprintf(&xml,
		    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
		    "<installer-gui-script minSpecVersion=\"1\">\n"
		    "    <pkg-ref id=\"%s\">\n"
		    "        <bundle-version/>\n"
		    "    </pkg-ref>\n"
		    "    <options customize=\"never\" require-scripts=\"false\" hostArchitectures=\"%s\"/>\n"
		    "    <choices-outline>\n"
		    "        <line choice=\"default\">\n"
		    "            <line choice=\"%s\"/>\n"
		    "        </line>\n"
		    "    </choices-outline>\n"
		    "    <choice id=\"default\"/>\n"
		    "    <choice id=\"%s\" visible=\"false\">\n"
		    "        <pkg-ref id=\"%s\"/>\n"
		    "    </choice>\n"
		    "    <pkg-ref id=\"%s\" version=\"%s\" onConclusion=\"none\" installKBytes=\"%zu\" updateKBytes=\"0\">#%s</pkg-ref>\n"
		    "</installer-gui-script>\n",
		    identifier, host_arch,
		    pkg_name, pkg_name,
		    pkg_name,
		    pkg_name, version, install_kbytes, pkg_name) < 0 || xml == NULL)
			return NULL;
	}

	*out_len = strlen(xml);
	return xml;
}
