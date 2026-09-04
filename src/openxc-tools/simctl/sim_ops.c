/*
 * sim_ops - simctl operational subcommands (create, boot, shutdown,
 * delete, erase, install, uninstall, spawn, io, location, push, rename,
 * clone, openurl).
 *
 * These commands require the CoreSimulator framework's XPC service, which
 * is a private framework not available to open-source reimplementations.
 * They delegate to the system simctl binary when available.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "simctl.h"

/*
 * Find and return the path to the system simctl binary.
 * Returns a static buffer, or NULL if not found.
 */
static const char *
find_system_simctl(void)
{
	static char path[1024];
	const char *candidates[] = {
		"/usr/bin/simctl",
		"/Applications/Xcode.app/Contents/Developer/usr/bin/simctl",
		NULL
	};

	for (int i = 0; candidates[i]; i++) {
		if (access(candidates[i], X_OK) == 0) {
			strlcpy(path, candidates[i], sizeof path);
			return path;
		}
	}
	return NULL;
}

/*
 * Delegate a command to the system simctl binary.
 * The subcommand and its arguments are passed through as-is.
 */
static int
delegate_to_system(int argc, char **argv, const char *subcommand)
{
	const char *simctl_bin = find_system_simctl();
	if (simctl_bin == NULL) {
		fprintf(stderr,
		    "simctl: %s requires the system simctl binary, "
		    "but it was not found.\n", subcommand);
		return 1;
	}

	char cmd[4096];
	int pos = snprintf(cmd, sizeof cmd, "\"%s\"", simctl_bin);

	for (int i = 1; i < argc && pos < (int)sizeof cmd - 256; i++) {
		pos += snprintf(cmd + pos, sizeof cmd - pos, " \"%s\"", argv[i]);
	}

	return system(cmd);
}

/* --- create --- */
int
sim_create(int argc, char **argv)
{
	if (argc < 4) {
		fprintf(stderr, "usage: simctl create <name> <device type id> "
		    "[<runtime id>]\n");
		return 1;
	}
	return delegate_to_system(argc, argv, "create");
}

/* --- boot / shutdown --- */
int
sim_boot_or_shutdown(const char *cmd, int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: simctl %s <device>\n", cmd);
		return 1;
	}
	return delegate_to_system(argc, argv, cmd);
}

/* --- delete / erase --- */
int
sim_delete_erase(const char *cmd, int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: simctl %s <device> [... <device n>] | "
		    "unavailable | all\n", cmd);
		return 1;
	}
	return delegate_to_system(argc, argv, cmd);
}

/* --- install / uninstall --- */
int
sim_install_uninstall(const char *cmd, int argc, char **argv)
{
	if (strcmp(cmd, "install") == 0) {
		if (argc < 3) {
			fprintf(stderr, "usage: simctl install <device> "
			    "<path>\n");
			return 1;
		}
	} else {
		if (argc < 3) {
			fprintf(stderr, "usage: simctl uninstall <device> "
			    "<bundle identifier>\n");
			return 1;
		}
	}
	return delegate_to_system(argc, argv, cmd);
}

/* --- spawn --- */
int
sim_spawn(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: simctl spawn <device> <path to "
		    "executable> [<argv 1> ...]\n");
		return 1;
	}
	return delegate_to_system(argc, argv, "spawn");
}

/* --- io --- */
int
sim_io(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: simctl io <device> <operation> "
		    "[arguments]\n");
		return 1;
	}
	return delegate_to_system(argc, argv, "io");
}

/* --- location --- */
int
sim_location(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: simctl location <device> <action> "
		    "[arguments]\n");
		return 1;
	}
	return delegate_to_system(argc, argv, "location");
}

/* --- push --- */
int
sim_push(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: simctl push <device> <bundle identifier> "
		    "<json file>\n");
		return 1;
	}
	return delegate_to_system(argc, argv, "push");
}

/* --- rename --- */
int
sim_rename(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: simctl rename <device> <name>\n");
		return 1;
	}
	return delegate_to_system(argc, argv, "rename");
}

/* --- clone --- */
int
sim_clone(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: simctl clone <source> <new name> "
		    "[<new udid>]\n");
		return 1;
	}
	return delegate_to_system(argc, argv, "clone");
}

/* --- openurl --- */
int
sim_openurl(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: simctl openurl <device> <URL>\n");
		return 1;
	}
	return delegate_to_system(argc, argv, "openurl");
}
