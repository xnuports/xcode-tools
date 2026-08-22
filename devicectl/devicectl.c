/*
 * devicectl - open source reimplementation of Apple's devicectl(1).
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Command line utility for exercising Core Device functionality.
 * Interacts with devices connected to this host. Implements the CLI
 * interface and help text matching Apple's devicectl; device operations
 * delegate to the system devicectl binary since CoreDevice is a private
 * framework.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "devicectl.h"

int opt_verbose = 0;
int opt_quiet = 0;
int opt_timeout = 0;
const char *opt_json_output = NULL;
const char *opt_log_output = NULL;

/* Help text matching real devicectl's help output */
static void
devicectl_help(void)
{
	printf(
"OVERVIEW: Core Device Device Control: a command line utility for exercising\n"
"Core Device functionality.\n"
"\n"
"devicectl is a Core Device command line utility that you can use to interact\n"
"with devices connected to this host. Automation and integrating with other\n"
"tools is supported via JSON output captured to a file, which is versioned and\n"
"will remain stable across releases. Standard output is meant for human\n"
"consumption and is not guaranteed to be stable across releases.\n"
"\n"
"USAGE: devicectl [--verbose] [--quiet] [--timeout <seconds>] [--json-output <path>] [--log-output <path>] <subcommand>\n"
"\n"
"OUTPUT OPTIONS:\n"
"  -v, --verbose           If given, provide more logging output than normal.\n"
"  -q, --quiet             If given, output will include only errors.\n"
"  -t, --timeout <seconds> The overall command timeout in seconds. If this limit\n"
"                          is exceeded the command is abandoned as a failure.\n"
"  -j, --json-output <path>\n"
"                          An optional path to write a JSON file with command\n"
"                          results.\n"
"        Note: JSON output to a user-provided file on disk is the ONLY supported\n"
"        interface for scripts/programs to consume command output.\n"
"  -l, --log-output <path> An optional path to write all logging otherwise\n"
"                          passed to stdout/stderr.\n"
"\n"
"OPTIONS:\n"
"  --version               Show the version.\n"
"  -h, --help              Show help information.\n"
"\n"
"SUBCOMMANDS:\n"
"  device                  Commands to interact with devices.\n"
"  diagnose                Gather diagnostic information for debugging or filing\n"
"                          bug reports.\n"
"  list                    List things that devicectl knows about.\n"
"  manage                  Commands to change state between the system and\n"
"                          devices, or between devices themselves.\n"
"\n"
"  See 'devicectl help <subcommand>' for detailed help.\n"
	);
}

static void
devicectl_device_help(void)
{
	printf(
"OVERVIEW: Commands to interact with devices.\n"
"\n"
"Allows users to interact with devices that CoreDevice is aware of.\n"
"\n"
"USAGE: devicectl device [--verbose] [--quiet] [--timeout <seconds>] [--json-output <path>] [--log-output <path>] <subcommand>\n"
"\n"
"OUTPUT OPTIONS:\n"
"  -v, --verbose           If given, provide more logging output than normal.\n"
"  -q, --quiet             If given, output will include only errors.\n"
"  -t, --timeout <seconds> The overall command timeout in seconds. If this limit\n"
"                          is exceeded the command is abandoned as a failure.\n"
"  -j, --json-output <path>\n"
"                          An optional path to write a JSON file with command\n"
"                          results.\n"
"        Note: JSON output to a user-provided file on disk is the ONLY supported\n"
"        interface for scripts/programs to consume command output.\n"
"  -l, --log-output <path> An optional path to write all logging otherwise\n"
"                          passed to stdout/stderr.\n"
"\n"
"OPTIONS:\n"
"  --version               Show the version.\n"
"  -h, --help              Show help information.\n"
"\n"
"SUBCOMMANDS:\n"
"  copy                    Copy files.\n"
"  info                    Commands that provide information about a specific\n"
"                          device\n"
"  install                 Install content onto a device.\n"
"  notification            Post and observe Darwin notifications on a device.\n"
"  orientation             Control device orientation.\n"
"  process                 Interact with processes on devices.\n"
"  reboot                  Reboot a device.\n"
"  sysdiagnose             Gather a sysdiagnose for a device.\n"
"  uninstall               Uninstall content from a device.\n"
"\n"
"  See 'devicectl help device <subcommand>' for detailed help.\n"
	);
}

static void
devicectl_device_info_help(void)
{
	printf(
"OVERVIEW: Commands that provide information about a specific device\n"
"\n"
"Provides access to device information like processes, installed roots,\n"
"installed files, and other device state.\n"
"\n"
"USAGE: devicectl device info [--verbose] [--quiet] [--timeout <seconds>] [--json-output <path>] [--log-output <path>] <subcommand>\n"
"\n"
"OUTPUT OPTIONS:\n"
"  -v, --verbose           If given, provide more logging output than normal.\n"
"  -q, --quiet             If given, output will include only errors.\n"
"  -t, --timeout <seconds> The overall command timeout in seconds. If this limit\n"
"                          is exceeded the command is abandoned as a failure.\n"
"  -j, --json-output <path>\n"
"                          An optional path to write a JSON file with command\n"
"                          results.\n"
"        Note: JSON output to a user-provided file on disk is the ONLY supported\n"
"        interface for scripts/programs to consume command output.\n"
"  -l, --log-output <path> An optional path to write all logging otherwise\n"
"                          passed to stdout/stderr.\n"
"\n"
"OPTIONS:\n"
"  --version               Show the version.\n"
"  -h, --help              Show help information.\n"
"\n"
"SUBCOMMANDS:\n"
"  appIcon                 Request app icon generation from this device.\n"
"  apps                    List apps installed on the device.\n"
"  authListing             Get the device's AuthListing identifiers.\n"
"  ddiServices             Get information on the developer disk image services\n"
"                          on the device.\n"
"  details                 Get information about the current device.\n"
"  displays                Get the device's current display information.\n"
"  files                   List files on the device.\n"
"  lockState               Get the current locked state of the device.\n"
"  processes               List currently running processes on the device.\n"
"\n"
"  See 'devicectl help device info <subcommand>' for detailed help.\n"
	);
}

static void
devicectl_device_process_help(void)
{
	printf(
"OVERVIEW: Interact with processes on devices.\n"
"\n"
"Allows users to interact with processes on devices that CoreDevice is aware of.\n"
"\n"
"USAGE: devicectl device process [--verbose] [--quiet] [--timeout <seconds>] [--json-output <path>] [--log-output <path>] <subcommand>\n"
"\n"
"OUTPUT OPTIONS:\n"
"  -v, --verbose           If given, provide more logging output than normal.\n"
"  -q, --quiet             If given, output will include only errors.\n"
"  -t, --timeout <seconds> The overall command timeout in seconds. If this limit\n"
"                          is exceeded the command is abandoned as a failure.\n"
"  -j, --json-output <path>\n"
"                          An optional path to write a JSON file with command\n"
"                          results.\n"
"        Note: JSON output to a user-provided file on disk is the ONLY supported\n"
"        interface for scripts/programs to consume command output.\n"
"  -l, --log-output <path> An optional path to write all logging otherwise\n"
"                          passed to stdout/stderr.\n"
"\n"
"OPTIONS:\n"
"  --version               Show the version.\n"
"  -h, --help              Show help information.\n"
"\n"
"SUBCOMMANDS:\n"
"  launch                  Launch a remote application.\n"
"  resume                  Resume a process on a device.\n"
"  sendMemoryWarning       Sends a memory pressure warning to a process on a\n"
"                          device.\n"
"  signal                  Send a signal to a process on a device.\n"
"  suspend                 Suspend a process on a device.\n"
"  terminate               Terminate a process on a device.\n"
"\n"
"  See 'devicectl help device process <subcommand>' for detailed help.\n"
	);
}

static void
devicectl_device_install_help(void)
{
	printf(
"OVERVIEW: Install content onto a device.\n"
"\n"
"This command allows you to install content onto a device.\n"
"\n"
"USAGE: devicectl device install [--verbose] [--quiet] [--timeout <seconds>] [--json-output <path>] [--log-output <path>] <subcommand>\n"
"\n"
"OUTPUT OPTIONS:\n"
"  -v, --verbose           If given, provide more logging output than normal.\n"
"  -q, --quiet             If given, output will include only errors.\n"
"  -t, --timeout <seconds> The overall command timeout in seconds. If this limit\n"
"                          is exceeded the command is abandoned as a failure.\n"
"  -j, --json-output <path>\n"
"                          An optional path to write a JSON file with command\n"
"                          results.\n"
"        Note: JSON output to a user-provided file on disk is the ONLY supported\n"
"        interface for scripts/programs to consume command output.\n"
"  -l, --log-output <path> An optional path to write all logging otherwise\n"
"                          passed to stdout/stderr.\n"
"\n"
"OPTIONS:\n"
"  --version               Show the version.\n"
"  -h, --help              Show help information.\n"
"\n"
"SUBCOMMANDS:\n"
"  app                     Installs an app.\n"
"\n"
"  See 'devicectl help device install <subcommand>' for detailed help.\n"
	);
}

static void
devicectl_device_notification_help(void)
{
	printf(
"OVERVIEW: Post and observe Darwin notifications on a device.\n"
"\n"
"USAGE: devicectl device notification [--verbose] [--quiet] [--timeout <seconds>] [--json-output <path>] [--log-output <path>] <subcommand>\n"
"\n"
"OUTPUT OPTIONS:\n"
"  -v, --verbose           If given, provide more logging output than normal.\n"
"  -q, --quiet             If given, output will include only errors.\n"
"  -t, --timeout <seconds> The overall command timeout in seconds. If this limit\n"
"                          is exceeded the command is abandoned as a failure.\n"
"  -j, --json-output <path>\n"
"                          An optional path to write a JSON file with command\n"
"                          results.\n"
"        Note: JSON output to a user-provided file on disk is the ONLY supported\n"
"        interface for scripts/programs to consume command output.\n"
"  -l, --log-output <path> An optional path to write all logging otherwise\n"
"                          passed to stdout/stderr.\n"
"\n"
"OPTIONS:\n"
"  --version               Show the version.\n"
"  -h, --help              Show help information.\n"
"\n"
"SUBCOMMANDS:\n"
"  post                    Post a Darwin notification to a device.\n"
"  observe                 Observe a Darwin notification on a device.\n"
"\n"
"  See 'devicectl help device notification <subcommand>' for detailed help.\n"
	);
}

static void
devicectl_device_orientation_help(void)
{
	printf(
"OVERVIEW: Control device orientation.\n"
"\n"
"Query or set simulated device physical orientation (for devices that supported\n"
"it).\n"
"\n"
"USAGE: devicectl device orientation [--verbose] [--quiet] [--timeout <seconds>] [--json-output <path>] [--log-output <path>] <subcommand>\n"
"\n"
"OUTPUT OPTIONS:\n"
"  -v, --verbose           If given, provide more logging output than normal.\n"
"  -q, --quiet             If given, output will include only errors.\n"
"  -t, --timeout <seconds> The overall command timeout in seconds. If this limit\n"
"                          is exceeded the command is abandoned as a failure.\n"
"  -j, --json-output <path>\n"
"                          An optional path to write a JSON file with command\n"
"                          results.\n"
"        Note: JSON output to a user-provided file on disk is the ONLY supported\n"
"        interface for scripts/programs to consume command output.\n"
"  -l, --log-output <path> An optional path to write all logging otherwise\n"
"                          passed to stdout/stderr.\n"
"\n"
"OPTIONS:\n"
"  --version               Show the version.\n"
"  -h, --help              Show help information.\n"
"\n"
"SUBCOMMANDS:\n"
"  get                     Get Device Orientation\n"
"  rotate                  Rotate Device Orientation\n"
"  set                     Set Device Orientation\n"
"\n"
"  See 'devicectl help device orientation <subcommand>' for detailed help.\n"
	);
}

static void
devicectl_device_copy_help(void)
{
	printf(
"OVERVIEW: Copy files.\n"
"\n"
"This command allows you to copy files to and from a device.\n"
"\n"
"USAGE: devicectl device copy [--verbose] [--quiet] [--timeout <seconds>] [--json-output <path>] [--log-output <path>] <subcommand>\n"
"\n"
"OUTPUT OPTIONS:\n"
"  -v, --verbose           If given, provide more logging output than normal.\n"
"  -q, --quiet             If given, output will include only errors.\n"
"  -t, --timeout <seconds> The overall command timeout in seconds. If this limit\n"
"                          is exceeded the command is abandoned as a failure.\n"
"  -j, --json-output <path>\n"
"                          An optional path to write a JSON file with command\n"
"                          results.\n"
"        Note: JSON output to a user-provided file on disk is the ONLY supported\n"
"        interface for scripts/programs to consume command output.\n"
"  -l, --log-output <path> An optional path to write all logging otherwise\n"
"                          passed to stdout/stderr.\n"
"\n"
"OPTIONS:\n"
"  --version               Show the version.\n"
"  -h, --help              Show help information.\n"
"\n"
"SUBCOMMANDS:\n"
"  to                      Copy a file or directory to the device.\n"
"  from                    Receive a file from the remote device.\n"
"\n"
"  See 'devicectl help device copy <subcommand>' for detailed help.\n"
	);
}

/* Delegate to the system devicectl binary */
static int
delegate_to_system(int argc, char **argv)
{
	const char *devicectl_bin = getenv("DEVICECTL_BIN");
	if (devicectl_bin == NULL) {
		const char *candidates[] = {
			"/usr/bin/devicectl",
			"/Applications/Xcode.app/Contents/Developer/usr/bin/devicectl",
			NULL
		};
		for (int i = 0; candidates[i]; i++) {
			if (access(candidates[i], X_OK) == 0) {
				devicectl_bin = candidates[i];
				break;
			}
		}
	}

	if (devicectl_bin == NULL) {
		fprintf(stderr, "devicectl: operation requires the system "
		    "devicectl binary, but it was not found.\n");
		return 1;
	}

	char cmd[4096];
	int pos = snprintf(cmd, sizeof cmd, "\"%s\"", devicectl_bin);
	for (int i = 1; i < argc && pos < (int)sizeof cmd - 256; i++) {
		pos += snprintf(cmd + pos, sizeof cmd - pos, " \"%s\"", argv[i]);
	}

	return system(cmd);
}

int
main(int argc, char **argv)
{
	int opt_index = 1;
	int help_mode = 0;
	int version_mode = 0;

	/* Parse global options */
	while (opt_index < argc) {
		if (strcmp(argv[opt_index], "-v") == 0 ||
		    strcmp(argv[opt_index], "--verbose") == 0) {
			opt_verbose = 1;
			opt_index++;
		} else if (strcmp(argv[opt_index], "-q") == 0 ||
		    strcmp(argv[opt_index], "--quiet") == 0) {
			opt_quiet = 1;
			opt_index++;
		} else if ((strcmp(argv[opt_index], "-t") == 0 ||
		    strcmp(argv[opt_index], "--timeout") == 0) &&
		    opt_index + 1 < argc) {
			opt_timeout = atoi(argv[opt_index + 1]);
			opt_index += 2;
		} else if (strncmp(argv[opt_index], "--timeout=", 10) == 0) {
			opt_timeout = atoi(argv[opt_index] + 10);
			opt_index++;
		} else if ((strcmp(argv[opt_index], "-j") == 0 ||
		    strcmp(argv[opt_index], "--json-output") == 0) &&
		    opt_index + 1 < argc) {
			opt_json_output = argv[opt_index + 1];
			opt_index += 2;
		} else if (strncmp(argv[opt_index], "--json-output=", 14) == 0) {
			opt_json_output = argv[opt_index] + 14;
			opt_index++;
		} else if ((strcmp(argv[opt_index], "-l") == 0 ||
		    strcmp(argv[opt_index], "--log-output") == 0) &&
		    opt_index + 1 < argc) {
			opt_log_output = argv[opt_index + 1];
			opt_index += 2;
		} else if (strncmp(argv[opt_index], "--log-output=", 13) == 0) {
			opt_log_output = argv[opt_index] + 13;
			opt_index++;
		} else if (strcmp(argv[opt_index], "--version") == 0) {
			version_mode = 1;
			opt_index++;
		} else if (strcmp(argv[opt_index], "-h") == 0 ||
		    strcmp(argv[opt_index], "--help") == 0) {
			help_mode = 1;
			opt_index++;
		} else {
			/* First non-option argument is the subcommand */
			break;
		}
	}

	/* --version */
	if (version_mode && opt_index >= argc) {
		printf("%s\n", DEVICECTL_VERSION);
		return 0;
	}

	/* --help */
	if (help_mode && opt_index >= argc) {
		devicectl_help();
		return 0;
	}

	/* No subcommand */
	if (opt_index >= argc) {
		if (version_mode) {
			printf("%s\n", DEVICECTL_VERSION);
			return 0;
		}
		/* Delegate to system for full help text */
		return delegate_to_system(argc, argv);
	}

	/* Determine the subcommand */
	const char *cmd = argv[opt_index];
	const char *cmd2 = (opt_index + 1 < argc) ? argv[opt_index + 1] : NULL;
	const char *cmd3 = (opt_index + 2 < argc) ? argv[opt_index + 2] : NULL;

	/* Handle help subcommand */
	if (strcmp(cmd, "help") == 0) {
		if (cmd2 == NULL) {
			devicectl_help();
			return 0;
		}
		if (strcmp(cmd2, "device") == 0) {
			if (cmd3 == NULL) {
				devicectl_device_help();
				return 0;
			}
			if (strcmp(cmd3, "info") == 0) {
				devicectl_device_info_help();
				return 0;
			}
			if (strcmp(cmd3, "process") == 0) {
				devicectl_device_process_help();
				return 0;
			}
			if (strcmp(cmd3, "install") == 0) {
				devicectl_device_install_help();
				return 0;
			}
			if (strcmp(cmd3, "notification") == 0) {
				devicectl_device_notification_help();
				return 0;
			}
			if (strcmp(cmd3, "orientation") == 0) {
				devicectl_device_orientation_help();
				return 0;
			}
			if (strcmp(cmd3, "copy") == 0) {
				devicectl_device_copy_help();
				return 0;
			}
			/* Unknown device subcommand - delegate */
			return delegate_to_system(argc, argv);
		}
		/* Unknown help subcommand - delegate */
		return delegate_to_system(argc, argv);
	}

	/* Handle device subcommand */
	if (strcmp(cmd, "device") == 0) {
		if (cmd2 == NULL) {
			devicectl_device_help();
			return 1;
		}

		/* device --help */
		if (strcmp(cmd2, "--help") == 0 || strcmp(cmd2, "-h") == 0) {
			devicectl_device_help();
			return 0;
		}

		/* device info */
		if (strcmp(cmd2, "info") == 0) {
			if (cmd3 == NULL) {
				devicectl_device_info_help();
				return 1;
			}
			if (strcmp(cmd3, "--help") == 0 ||
			    strcmp(cmd3, "-h") == 0) {
				devicectl_device_info_help();
				return 0;
			}
			/* Delegate to system for actual info commands */
			return delegate_to_system(argc, argv);
		}

		/* device process */
		if (strcmp(cmd2, "process") == 0) {
			if (cmd3 == NULL) {
				devicectl_device_process_help();
				return 1;
			}
			if (strcmp(cmd3, "--help") == 0 ||
			    strcmp(cmd3, "-h") == 0) {
				devicectl_device_process_help();
				return 0;
			}
			/* Delegate to system */
			return delegate_to_system(argc, argv);
		}

		/* device install */
		if (strcmp(cmd2, "install") == 0) {
			if (cmd3 == NULL) {
				devicectl_device_install_help();
				return 1;
			}
			if (strcmp(cmd3, "--help") == 0 ||
			    strcmp(cmd3, "-h") == 0) {
				devicectl_device_install_help();
				return 0;
			}
			/* Delegate to system */
			return delegate_to_system(argc, argv);
		}

		/* device uninstall */
		if (strcmp(cmd2, "uninstall") == 0) {
			/* Delegate to system */
			return delegate_to_system(argc, argv);
		}

		/* device notification */
		if (strcmp(cmd2, "notification") == 0) {
			if (cmd3 == NULL) {
				devicectl_device_notification_help();
				return 1;
			}
			if (strcmp(cmd3, "--help") == 0 ||
			    strcmp(cmd3, "-h") == 0) {
				devicectl_device_notification_help();
				return 0;
			}
			return delegate_to_system(argc, argv);
		}

		/* device orientation */
		if (strcmp(cmd2, "orientation") == 0) {
			if (cmd3 == NULL) {
				devicectl_device_orientation_help();
				return 1;
			}
			if (strcmp(cmd3, "--help") == 0 ||
			    strcmp(cmd3, "-h") == 0) {
				devicectl_device_orientation_help();
				return 0;
			}
			return delegate_to_system(argc, argv);
		}

		/* device copy */
		if (strcmp(cmd2, "copy") == 0) {
			if (cmd3 == NULL) {
				devicectl_device_copy_help();
				return 1;
			}
			if (strcmp(cmd3, "--help") == 0 ||
			    strcmp(cmd3, "-h") == 0) {
				devicectl_device_copy_help();
				return 0;
			}
			return delegate_to_system(argc, argv);
		}

		/* device reboot */
		if (strcmp(cmd2, "reboot") == 0) {
			if (strcmp(cmd3, "--help") == 0 ||
			    strcmp(cmd3, "-h") == 0) {
				printf(
"OVERVIEW: Reboot a device.\n"
"\n"
"USAGE: devicectl device reboot [--verbose] [--quiet] [--timeout <seconds>]\n"
"       [--json-output <path>] [--log-output <path>] --device <id>\n"
				);
				return 0;
			}
			return delegate_to_system(argc, argv);
		}

		/* device sysdiagnose */
		if (strcmp(cmd2, "sysdiagnose") == 0) {
			if (strcmp(cmd3, "--help") == 0 ||
			    strcmp(cmd3, "-h") == 0) {
				printf(
"OVERVIEW: Gather a sysdiagnose for a device.\n"
"\n"
"USAGE: devicectl device sysdiagnose [--verbose] [--quiet] [--timeout "
"<seconds>\n"
"       [--json-output <path>] [--log-output <path>] --device <id>\n"
				);
				return 0;
			}
			return delegate_to_system(argc, argv);
		}

		/* Unknown device subcommand - delegate to system */
		return delegate_to_system(argc, argv);
	}

	/* Unknown subcommand - delegate to system for error message */
	return delegate_to_system(argc, argv);
}
