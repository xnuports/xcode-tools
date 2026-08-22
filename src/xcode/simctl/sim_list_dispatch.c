/*
 * list_dispatch - wrapper for simctl list subcommand.
 *
 * Parses the list subcommand's arguments and calls the appropriate
 * list function (devices, devicetypes, runtimes, or pairs).
 */

#include <stdio.h>
#include <string.h>

#include "simctl.h"

static void
list_type_dispatch(const char *type, int json_mode, int verbose)
{
	if (strcmp(type, "devices") == 0) {
		list_devices(json_mode, verbose);
	} else if (strcmp(type, "devicetypes") == 0) {
		if (!json_mode)
			printf("== Device Types ==\n");
		list_devicetypes(json_mode);
	} else if (strcmp(type, "runtimes") == 0) {
		if (!json_mode)
			printf("== Runtimes ==\n");
		list_runtimes(json_mode);
	} else if (strcmp(type, "pairs") == 0) {
		if (!json_mode)
			printf("== Device Pairs ==\n");
		list_pairs(json_mode);
	}
}

void
list_dispatch(int argc, char **argv)
{
	int json_mode = 0;
	int verbose = 0;
	const char *list_type = NULL;

	/* Parse options first */
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-j") == 0 ||
		    strcmp(argv[i], "--json") == 0) {
			json_mode = 1;
		} else if (strcmp(argv[i], "-v") == 0) {
			verbose = 1;
		} else if (strcmp(argv[i], "devices") == 0 ||
		    strcmp(argv[i], "devicetypes") == 0 ||
		    strcmp(argv[i], "runtimes") == 0 ||
		    strcmp(argv[i], "pairs") == 0) {
			list_type = argv[i];
		} else if (strcmp(argv[i], "--no-escape-slashes") == 0 ||
		    strcmp(argv[i], "-e") == 0) {
		} else if (strncmp(argv[i], "--json-fd=", 10) == 0) {
			json_mode = 1;
		} else if (strncmp(argv[i], "--json-output=", 14) == 0) {
			json_mode = 1;
		}
	}

	if (argc <= 1 || list_type == NULL) {
		/* List everything with headers */
		if (!json_mode)
			printf("== Devices ==\n");
		list_devices(json_mode, verbose);
		if (!json_mode)
			printf("== Device Types ==\n");
		list_devicetypes(json_mode);
		if (!json_mode)
			printf("== Runtimes ==\n");
		list_runtimes(json_mode);
		if (!json_mode)
			printf("== Device Pairs ==\n");
		list_pairs(json_mode);
		return;
	}

	/* Single type specified */
	if (!json_mode) {
		if (strcmp(list_type, "devices") == 0)
			printf("== Devices ==\n");
	}
	list_type_dispatch(list_type, json_mode, verbose);
}
