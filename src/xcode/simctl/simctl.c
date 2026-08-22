/*
 * simctl - open source reimplementation of Apple's simctl(1).
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Command line utility to control the Simulator. Implements list, create,
 * boot, shutdown, delete, erase, install, uninstall, spawn, io, and
 * other subcommands. Read-only commands (list) are implemented natively
 * by reading CoreSimulator plist data; destructive/operational commands
 * delegate to the system simctl binary when available.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "simctl.h"

/* Help text matching real simctl's help output */
static void
simctl_help(void)
{
	printf(
"usage: simctl [--set <path>] [--profiles <path>] <subcommand> ...\n"
"       simctl help [subcommand]\n"
"Command line utility to control the Simulator\n"
"\n"
"For subcommands that require a <device> argument, you may specify a device UDID\n"
"or the special \"booted\" string which will cause simctl to pick a booted device.\n"
"If multiple devices are booted when the \"booted\" device is selected, simctl\n"
"will choose one of them.\n"
"\n"
"Subcommands:\n"
"	addmedia            Add photos, live photos, videos, or contacts to the library of a device.\n"
"	appinfo             Show information about an installed application.\n"
"	boot                Boot a device or device pair.\n"
"	clone               Clone an existing device.\n"
"	create              Create a new device.\n"
"	delete              Delete specified devices, unavailable devices, or all devices.\n"
"	diagnose            Collect diagnostic information and logs.\n"
"	erase               Erase a device's contents and settings.\n"
"	get_app_container   Print the path of the installed app's container\n"
"	getenv              Print an environment variable from a running device.\n"
"	help                Prints the usage for a given subcommand.\n"
"	icloud_sync         Trigger iCloud sync on a device.\n"
"	install             Install an app on a device.\n"
"	install_app_data    Install an xcappdata package to a device, replacing the current contents of the container.\n"
"	io                  Set up a device IO operation.\n"
"	keychain            Manipulate a device's keychain\n"
"	launch              Launch an application by identifier on a device.\n"
"	list                List available devices, device types, runtimes, or device pairs.\n"
"	listapps            Show the installed applications.\n"
"	location            Control a device's simulated location\n"
"	logverbose          enable or disable verbose logging for a device\n"
"	openurl             Open a URL in a device.\n"
"	pair                Create a new watch and phone pair.\n"
"	pair_activate       Set a given pair as active.\n"
"	pbcopy              Copy standard input onto the device pasteboard.\n"
"	pbpaste             Print the contents of the device's pasteboard to standard output.\n"
"	pbsync              Sync the pasteboard content from one pasteboard to another.\n"
"	personalization     Provides utility when working with personalization manifests\n"
"	privacy             Grant, revoke, or reset privacy and permissions\n"
"	push                Send a simulated push notification\n"
"	rename              Rename a device.\n"
"	runtime             Perform operations on runtimes\n"
"	shutdown            Shutdown a device.\n"
"	spawn               Spawn a process by executing a given executable on a device.\n"
"	status_bar          Set or clear status bar overrides\n"
"	terminate           Terminate an application by identifier on a device.\n"
"	ui                  Get or Set UI options\n"
"	uninstall           Uninstall an app from a device.\n"
"	unpair              Unpair a watch and phone pair.\n"
"	upgrade             Upgrade a device to a newer runtime.\n"
	);
}

/* Delegate to the system simctl binary */
static int
delegate_to_system(int argc, char **argv)
{
	char cmd[4096];
	char *simctl_bin = getenv("SIMCTL_BIN");

	if (simctl_bin == NULL) {
		/* Try to find the system simctl */
		const char *candidates[] = {
			"/usr/bin/simctl",
			"/Applications/Xcode.app/Contents/Developer/usr/bin/simctl",
			"/usr/local/bin/simctl",
			NULL
		};
		for (int i = 0; candidates[i]; i++) {
			if (access(candidates[i], X_OK) == 0) {
				simctl_bin = (char *)candidates[i];
				break;
			}
		}
	}

	if (simctl_bin == NULL) {
		fprintf(stderr, "simctl: operation requires Xcode simctl binary, "
		    "but it was not found.\n");
		return 1;
	}

	/* Build the command line */
	int pos = 0;
	pos += snprintf(cmd + pos, sizeof cmd - pos, "\"%s\"", simctl_bin);
	for (int i = 1; i < argc && pos < (int)sizeof cmd - 256; i++) {
		pos += snprintf(cmd + pos, sizeof cmd - pos, " \"%s\"", argv[i]);
	}

	return system(cmd);
}

int
main(int argc, char **argv)
{
	const char *subcommand = NULL;
	char **sub_args = argv;
	int sub_argc = argc;
	int help_mode = 0;
	int opt_index = 1;

	/* Parse global options before the subcommand */
	while (opt_index < argc) {
		if (strcmp(argv[opt_index], "--set") == 0 &&
		    opt_index + 1 < argc) {
			opt_index += 2;
		} else if (strncmp(argv[opt_index], "--set=", 6) == 0) {
			opt_index++;
		} else if (strcmp(argv[opt_index], "--profiles") == 0 &&
		    opt_index + 1 < argc) {
			opt_index += 2;
		} else if (strncmp(argv[opt_index], "--profiles=", 11) == 0) {
			opt_index++;
		} else if (strcmp(argv[opt_index], "-h") == 0 ||
		    strcmp(argv[opt_index], "--help") == 0) {
			help_mode = 1;
			opt_index++;
		} else {
			/* First non-option argument is the subcommand */
			subcommand = argv[opt_index];
			break;
		}
	}

	if (subcommand == NULL && help_mode) {
		simctl_help();
		return 0;
	}

	if (subcommand == NULL && opt_index >= argc) {
		simctl_help();
		return 0;
	}

	/* Set up the arguments for the subcommand */
	sub_args = argv + opt_index;
	sub_argc = argc - opt_index;

	if (help_mode && subcommand == NULL) {
		simctl_help();
		return 0;
	}

	if (subcommand != NULL && strcmp(subcommand, "help") == 0) {
		if (sub_argc > 1) {
			const char *sub = sub_args[1];
			if (strcmp(sub, "list") == 0) {
				printf(
"List available devices, device types, runtimes, or device pairs.\n"
"Usage: simctl list [-j | --json] [--json-fd=<fd>] [--json-output=<file path>]\n"
"       [-e | --no-escape-slashes] [-v] [devices|devicetypes|runtimes|pairs]\n"
"       [<search term>|available]\n"
"-j     Print as JSON (optionally specify a file descriptor with --json-fd or file path with --json-output)\n"
"-e     Encode slashes without escapes in JSON output\n"
"-v     More verbose output\n"
"\n"
"Specify one of 'devices', 'devicetypes', 'runtimes', or 'pairs' to list only\n"
"items of that type. If a type filter is specified you may also specify a search\n"
"term. Search terms use a simple case-insensitive contains check against the\n"
"item's description.\n"
				);
				return 0;
			}
		} else {
			simctl_help();
			return 0;
		}
		return delegate_to_system(argc, argv);
	}

	if (subcommand == NULL) {
		simctl_help();
		return 0;
	}

	/* Dispatch to subcommands */
	if (strcmp(subcommand, "list") == 0) {
		list_dispatch(sub_argc, sub_args);
		return 0;
	} else if (strcmp(subcommand, "create") == 0) {
		return sim_create(sub_argc, sub_args);
	} else if (strcmp(subcommand, "boot") == 0 ||
	    strcmp(subcommand, "shutdown") == 0) {
		return sim_boot_or_shutdown(subcommand, sub_argc, sub_args);
	} else if (strcmp(subcommand, "delete") == 0 ||
	    strcmp(subcommand, "erase") == 0) {
		return sim_delete_erase(subcommand, sub_argc, sub_args);
	} else if (strcmp(subcommand, "install") == 0 ||
	    strcmp(subcommand, "uninstall") == 0) {
		return sim_install_uninstall(subcommand, sub_argc, sub_args);
	} else if (strcmp(subcommand, "spawn") == 0) {
		return sim_spawn(sub_argc, sub_args);
	} else if (strcmp(subcommand, "io") == 0) {
		return sim_io(sub_argc, sub_args);
	} else if (strcmp(subcommand, "location") == 0) {
		return sim_location(sub_argc, sub_args);
	} else if (strcmp(subcommand, "push") == 0) {
		return sim_push(sub_argc, sub_args);
	} else if (strcmp(subcommand, "rename") == 0) {
		return sim_rename(sub_argc, sub_args);
	} else if (strcmp(subcommand, "clone") == 0) {
		return sim_clone(sub_argc, sub_args);
	} else if (strcmp(subcommand, "openurl") == 0) {
		return sim_openurl(sub_argc, sub_args);
	} else if (strcmp(subcommand, "pbcopy") == 0 ||
	    strcmp(subcommand, "pbpaste") == 0 ||
	    strcmp(subcommand, "pbsync") == 0) {
		return delegate_to_system(argc, argv);
	} else if (strcmp(subcommand, "appinfo") == 0 ||
	    strcmp(subcommand, "listapps") == 0) {
		return delegate_to_system(argc, argv);
	} else if (strcmp(subcommand, "privacy") == 0) {
		return delegate_to_system(argc, argv);
	} else if (strcmp(subcommand, "runtime") == 0) {
		return delegate_to_system(argc, argv);
	} else if (strcmp(subcommand, "addmedia") == 0 ||
	    strcmp(subcommand, "icloud_sync") == 0 ||
	    strcmp(subcommand, "keychain") == 0 ||
	    strcmp(subcommand, "launch") == 0 ||
	    strcmp(subcommand, "logverbose") == 0 ||
	    strcmp(subcommand, "pair") == 0 ||
	    strcmp(subcommand, "pair_activate") == 0 ||
	    strcmp(subcommand, "unpair") == 0 ||
	    strcmp(subcommand, "status_bar") == 0 ||
	    strcmp(subcommand, "terminate") == 0 ||
	    strcmp(subcommand, "ui") == 0 ||
	    strcmp(subcommand, "personalization") == 0 ||
	    strcmp(subcommand, "get_app_container") == 0 ||
	    strcmp(subcommand, "getenv") == 0 ||
	    strcmp(subcommand, "install_app_data") == 0 ||
	    strcmp(subcommand, "diagnose") == 0 ||
	    strcmp(subcommand, "upgrade") == 0) {
		return delegate_to_system(argc, argv);
	} else {
		fprintf(stderr, "simctl: unknown subcommand '%s'\n",
		    subcommand);
		simctl_help();
		return 1;
	}
}
