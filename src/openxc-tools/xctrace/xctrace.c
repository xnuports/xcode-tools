/*
 * xctrace - open source reimplementation of Apple's xctrace(1).
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Supports: record, export, list (devices, templates, instruments),
 * import, remodel, symbolicate, version, help.
 *
 * Recording delegates to Instruments.app (launched via open(1)) or
 * falls back to the system xctrace. List commands enumerate devices
 * from CoreSimulator plists and Instruments.app bundles.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>

#include "xctrace.h"

#define VERSION_STRING  "xctrace " XCTRACE_VERSION " (compat 16.0)"

static const char *program_name = "xctrace";
static int quiet = 0;

/* --- forward declarations --- */

static int cmd_record(int argc, char **argv);
static int cmd_export(int argc, char **argv);
static int cmd_list(int argc, char **argv);

/* --- utility functions --- */

static const char *
basename_of(const char *path)
{
	const char *slash = strrchr(path, '/');
	return slash ? slash + 1 : path;
}

static void
sort_strings(char **arr, int n)
{
	for (int i = 1; i < n; i++) {
		int j = i;
		while (j > 0 && strcmp(arr[j - 1], arr[j]) > 0) {
			char *tmp = arr[j];
			arr[j] = arr[j - 1];
			arr[j - 1] = tmp;
			j--;
		}
	}
}

/* Locate the Instruments.app bundle. Returns 0 on success. */
static int
find_instruments_app(char *buf, size_t n)
{
	const char *candidates[] = {
		"/Applications/Xcode.app/Contents/Applications/Instruments.app",
		"/Applications/Xcode-Beta.app/Contents/Applications/Instruments.app",
		NULL,
	};

	struct stat st;

	for (int i = 0; candidates[i] != NULL; i++) {
		if (stat(candidates[i], &st) == 0 && S_ISDIR(st.st_mode)) {
			snprintf(buf, n, "%s", candidates[i]);
			return 0;
		}
	}

	/* Derive from xcode-select output. */
	FILE *fp = popen("xcode-select -p 2>/dev/null", "r");
	if (fp != NULL) {
		char xcode_path[1024];
		if (fgets(xcode_path, sizeof xcode_path, fp) != NULL) {
			size_t len = strlen(xcode_path);
			if (len > 0 && xcode_path[len - 1] == '\n')
				xcode_path[len - 1] = '\0';
			pclose(fp);

			char *suffix = strstr(xcode_path,
			    "/Contents/Developer");
			if (suffix != NULL) {
				*suffix = '\0';
				snprintf(buf, n,
				    "%s/Contents/Applications/"
				    "Instruments.app", xcode_path);
				struct stat st2;
				if (stat(buf, &st2) == 0 && S_ISDIR(st2.st_mode))
					return 0;
			}
		} else {
			pclose(fp);
		}
	}

	/* Fall back to system xctrace. */
	if (access("/usr/bin/xctrace", X_OK) == 0) {
		snprintf(buf, n, "/usr/bin/xctrace");
		return 0;
	}

	return -1;
}

/* --- list devices --- */

static void
list_simulators(void)
{
	const char *home = getenv("HOME");
	char path[1024];
	snprintf(path, sizeof path,
	    "%s/Library/Developer/CoreSimulator/Devices",
	    home ? home : ".");

	DIR *dir = opendir(path);
	if (dir == NULL)
		return;

	char *names[256];
	int count = 0;

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;

		char plist_path[1024];
		snprintf(plist_path, sizeof plist_path,
		    "%s/%s/device.plist", path, entry->d_name);

		FILE *f = fopen(plist_path, "r");
		if (f == NULL)
			continue;

		char line[512];
		char name[256] = "";
		char udid[256] = "";
		char runtime[256] = "";

		while (fgets(line, sizeof line, f) != NULL) {
			char *p = line;
			while (*p == ' ' || *p == '\t') p++;

			if (strstr(p, "<key>name</key>") != NULL) {
				if (fgets(line, sizeof line, f) == NULL)
					break;
				char *v = strstr(line, "<string>");
				if (v) {
					v += 8;
					char *end = strchr(v, '<');
					if (end) *end = '\0';
					strncpy(name, v, sizeof name - 1);
				}
			} else if (strstr(p, "<key>UDID</key>") != NULL) {
				if (fgets(line, sizeof line, f) == NULL)
					break;
				char *v = strstr(line, "<string>");
				if (v) {
					v += 8;
					char *end = strchr(v, '<');
					if (end) *end = '\0';
					strncpy(udid, v, sizeof udid - 1);
				}
			} else if (strstr(p, "<key>runtime</key>") != NULL) {
				if (fgets(line, sizeof line, f) == NULL)
					break;
				char *v = strstr(line, "<string>");
				if (v) {
					v += 8;
					char *end = strchr(v, '<');
					if (end) *end = '\0';
					strncpy(runtime, v,
					    sizeof runtime - 1);
				}
			}
		}
		fclose(f);

		if (name[0] != '\0' && udid[0] != '\0') {
			char os_ver[64] = "";
			if (strncmp(runtime,
			    "com.apple.CoreSimulator.SimRuntime.", 35) == 0) {
				const char *rest = runtime + 35;
				if (strncmp(rest, "iOS-", 4) == 0) {
					const char *ver = rest + 4;
					int major, minor;
					if (sscanf(ver, "%d-%d", &major,
					    &minor) == 2) {
						snprintf(os_ver, sizeof os_ver,
						    "%d.%d", major, minor);
					}
				}
			}

			if (os_ver[0] != '\0')
				snprintf(name, sizeof name,
				    "%s Simulator (%s) (%s)", name,
				    os_ver, udid);
			else
				snprintf(name, sizeof name,
				    "%s (%s)", name, udid);

			if (count < 256)
				names[count++] = strdup(name);
		}
	}

	closedir(dir);
	sort_strings(names, count);
	for (int i = 0; i < count; i++) {
		printf("%s\n", names[i]);
		free(names[i]);
	}
}

static void
list_physical_devices(void)
{
	FILE *fp = popen(
	    "system_profiler SPUSBDataType 2>/dev/null", "r");
	if (fp == NULL)
		printf("  (No connected physical devices detected)\n");

	char line[512];
	int found = 0;

	while (fgets(line, sizeof line, fp) != NULL) {
		char *p = line;
		while (*p == ' ' || *p == '\t') p++;

		if (strstr(p, "iPhone") || strstr(p, "iPad") ||
		    strstr(p, "iPod")) {
			char *nl = strchr(p, '\n');
			if (nl) *nl = '\0';
			size_t len = strlen(p);
			while (len > 0 && (p[len-1] == ' ' || p[len-1] == '\t'))
				p[--len] = '\0';
			printf("  %s\n", p);
			found = 1;
		}
	}
	pclose(fp);

	if (!found)
		printf("  (No connected physical devices detected)\n");
}

/* --- list templates --- */

static void
list_templates(void)
{
	char instr_path[1024];
	if (find_instruments_app(instr_path, sizeof instr_path) != 0) {
		fprintf(stderr, "%s: cannot locate Instruments.app\n",
		    program_name);
		return;
	}

	char *templates[128];
	int count = 0;

	char scan_path[1024];

	/* Scan Resources/templates */
	snprintf(scan_path, sizeof scan_path,
	    "%s/Contents/Resources/templates", instr_path);
	DIR *d = opendir(scan_path);
	if (d != NULL) {
		struct dirent *entry;
		while ((entry = readdir(d)) != NULL) {
			const char *base = basename_of(entry->d_name);
			size_t len = strlen(base);
			if (len > 14 &&
			    strcmp(base + len - 14, ".tracetemplate") == 0) {
				char *name = malloc(len - 13);
				if (name) {
					memcpy(name, base, len - 14);
					name[len - 14] = '\0';
					/* Skip "Blank" template — xctrace
					 * doesn't list it */
					if (strcmp(name, "Blank") != 0 &&
					    count < 128)
						templates[count++] = name;
				}
			}
		}
		closedir(d);
	}

	/* Scan Packages for .tracetemplate files */
	char pkg_dir[1024];
	snprintf(pkg_dir, sizeof pkg_dir,
	    "%s/Contents/Packages", instr_path);
	DIR *pd = opendir(pkg_dir);
	if (pd != NULL) {
		struct dirent *pde;
		while ((pde = readdir(pd)) != NULL) {
			if (pde->d_name[0] == '.')
				continue;
			if (strstr(pde->d_name, ".instrdst") == NULL)
				continue;

			char tdir[1024];
			snprintf(tdir, sizeof tdir,
			    "%s/%s/Contents/Templates", pkg_dir,
			    pde->d_name);
			DIR *td = opendir(tdir);
			if (td == NULL)
				continue;

			struct dirent *tde;
			while ((tde = readdir(td)) != NULL) {
				const char *base = basename_of(tde->d_name);
				size_t len = strlen(base);
				if (len > 14 &&
				    strcmp(base + len - 14,
					".tracetemplate") == 0) {
					char *name = malloc(len - 13);
					if (name) {
						memcpy(name, base, len - 14);
						name[len - 14] = '\0';
						if (strcmp(name, "Blank") != 0 &&
						    count < 128)
							templates[count++] = name;
					}
				}
			}
			closedir(td);
		}
		closedir(pd);
	}

	sort_strings(templates, count);
	for (int i = 0; i < count; i++) {
		printf("%s\n", templates[i]);
		free(templates[i]);
	}
}

/* --- list instruments --- */

static void
list_instruments(void)
{
	char instr_path[1024];
	if (find_instruments_app(instr_path, sizeof instr_path) != 0) {
		fprintf(stderr, "%s: cannot locate Instruments.app\n",
		    program_name);
		return;
	}

	/* Instrument display names mapped from package/plugin existence.
	 * Each package provides one or more instruments. */
	struct {
		const char *pkg_dir;	/* Packages or PlugIns */
		const char *pkg_name;	/* directory name + suffix */
		const char *display;	/* human-readable instrument name */
	} instruments[] = {
		{ "Packages", "ActivityMonitor.instrdst",    "Activity Monitor" },
		{ "Packages", "GPU.instrdst",                "Advanced Graphics Statistics" },
		{ "Plugins",  "CorePlugin.xrplugin",         "Allocations" },
		{ "Packages", "Audio.instrdst",              "Audio Client" },
		{ "Packages", "Audio.instrdst",              "Audio Server" },
		{ "Packages", "Audio.instrdst",              "Audio Statistics" },
		{ "Packages", "CPU Counters.instrdst",       "CPU Counters" },
		{ "Packages", "CPU Profiler.instrdst",       "CPU Profiler" },
		{ "Packages", "CoreAnimation.instrdst",      "Core Animation Activity" },
		{ "Packages", "CoreAnimation.instrdst",      "Core Animation Commits" },
		{ "Packages", "CoreAnimation.instrdst",      "Core Animation FPS" },
		{ "Packages", "CoreAnimation.instrdst",      "Core Animation Server" },
		{ "Packages", "CoreML.instrdst",             "Core ML" },
		{ "Packages", "DataPersistence.instrdst",    "Data Faults" },
		{ "Packages", "DataPersistence.instrdst",    "Data Fetches" },
		{ "Packages", "DataPersistence.instrdst",    "Data Saves" },
		{ "Packages", "IOKit.instrdst",              "Disk I/O Latency" },
		{ "Packages", "IOKit.instrdst",              "Disk Usage" },
		{ "Plugins",  "IOKitPlugin.xrplugin",        "Display" },
		{ "Packages", "IOKit.instrdst",              "Filesystem Activity" },
		{ "Packages", "IOKit.instrdst",              "Filesystem Suggestions" },
		{ "Packages", "Base.instrdst",               "Foundation Models" },
		{ "Packages", "Hitches.instrdst",            "Foveated Streaming Statistics" },
		{ "Packages", "Hitches.instrdst",            "Frame Lifetimes" },
		{ "Packages", "Dispatch.instrdst",           "GCD Performance" },
		{ "Plugins",  "GPUPlugin.xrplugin",          "GPU" },
		{ "Packages", "HTTPTracing.instrdst",        "HTTP Traffic" },
		{ "Packages", "Hangs.instrdst",              "Hangs" },
		{ "Packages", "Hitches.instrdst",            "Hitches" },
		{ "Templates", "Leaks.tracetemplate",         "Leaks" },
		{ "Packages", "Hangs.instrdst",              "Location Energy Model" },
		{ "Plugins",  "GPUPlugin.xrplugin",          "Metal Application" },
		{ "Plugins",  "GPUPlugin.xrplugin",          "Metal GPU Counters" },
		{ "Plugins",  "GPUPlugin.xrplugin",          "Metal Performance Overview" },
		{ "Plugins",  "GPUPlugin.xrplugin",          "Metal Resource Events" },
		{ "Plugins",  "NetworkingPlugin.xrplugin",    "Network Connections" },
		{ "Packages", "Neural Engine.instrdst",      "Neural Engine" },
		{ "Packages", "Hitches.instrdst",            "Points of Interest" },
		{ "Templates", "Power Profiler.tracetemplate", "Power Profiler" },
		{ "Packages", "CPU Counters.instrdst",       "Processor Trace" },
		{ "Packages", "RealityKit.instrdst",         "RealityKit Frames" },
		{ "Packages", "RealityKit.instrdst",         "RealityKit Metrics" },
		{ "Packages", "RunloopInstrument.instrdst",  "Runloops" },
		{ "Packages", "Sampling.instrdst",           "Sampler" },
		{ "Packages", "SceneKit.instrdst",           "SceneKit Application" },
		{ "Packages", "SwiftConcurrency.instrdst",   "Swift Actors" },
		{ "Packages", "SwiftConcurrency.instrdst",   "Swift Tasks" },
		{ "PlugIns",  "CorePlugin.xrplugin",         "SwiftUI" },
		{ "Packages", "ktrace.instrdst",             "System Call Trace" },
		{ "Packages", "SystemTrace.instrdst",        "System Load" },
		{ "Packages", "Thermal.instrdst",            "Thermal State" },
		{ "Packages", "Sampling.instrdst",           "Thread State Trace" },
		{ "Packages", "Sampling.instrdst",           "Time Profiler" },
		{ "Packages", "Sampling.instrdst",           "VM Tracker" },
		{ "Packages", "SwiftUI.instrdst",            "View Body (Legacy)" },
		{ "Packages", "SwiftUI.instrdst",            "View Properties (Legacy)" },
		{ "Packages", "ActivityMonitor.instrdst",    "Virtual Memory Trace" },
		{ "Packages", "dyld.instrdst",               "dyld Activity" },
		{ "Packages", "Narrative.instrdst",          "os_log" },
		{ "Packages", "Narrative.instrdst",          "os_signpost" },
		{ "Packages", "Narrative.instrdst",          "stdout/stderr" },
	};

	char *found_instruments[128];
	int found_count = 0;

	for (size_t i = 0; i < sizeof(instruments) / sizeof(instruments[0]); i++) {
		char pkg_path[1024];
		struct stat st;

		if (strcmp(instruments[i].pkg_dir, "Templates") == 0) {
			snprintf(pkg_path, sizeof pkg_path,
			    "%s/Contents/Resources/templates/%s",
			    instr_path, instruments[i].pkg_name);
		} else if (strcmp(instruments[i].pkg_dir, "Plugins") == 0) {
			snprintf(pkg_path, sizeof pkg_path,
			    "%s/Contents/PlugIns/%s", instr_path,
			    instruments[i].pkg_name);
		} else {
			snprintf(pkg_path, sizeof pkg_path,
			    "%s/Contents/%s/%s", instr_path,
			    instruments[i].pkg_dir, instruments[i].pkg_name);
		}

		if (stat(pkg_path, &st) == 0) {
			int is_dir = S_ISDIR(st.st_mode);
			int is_file = S_ISREG(st.st_mode);
			if ((strcmp(instruments[i].pkg_dir, "Templates") == 0 &&
			    is_file) ||
			    (strcmp(instruments[i].pkg_dir, "Templates") != 0 &&
			    is_dir)) {
				if (found_count < 128) {
					found_instruments[found_count] =
					    strdup(instruments[i].display);
					found_count++;
				}
			}
		}
	}

	sort_strings(found_instruments, found_count);
	for (int i = 0; i < found_count; i++) {
		printf("%s\n", found_instruments[i]);
		free(found_instruments[i]);
	}
}

/* --- main dispatch --- */

static void
print_usage(FILE *out)
{
	fprintf(out,
	    "usage:\n"
	    "\txctrace <command> [options]\n"
	    "\n"
	    "global options:\n"
	    "\t-q, --quiet\n"
	    "\n"
	    "commands:\n"
	    "\trecord                                      Perform new recording using specified template\n"
	    "\timport                                      Import file of a supported format into .trace file\n"
	    "\texport                                      Export .trace file content to an external format\n"
	    "\tremodel                                     Remodel .trace using currently installed modelers\n"
	    "\tsymbolicate                                 Symbolicate .trace file using specified dSYM\n"
	    "\tlist [devices|templates|instruments]        List capabilities of the current running environment\n"
	    "\tversion                                     Print version of the tool\n"
	    "\n"
	    "further help:\n"
	    "\t xctrace help <command>\n");
}

static void
print_command_help(const char *cmd)
{
	if (strcmp(cmd, "record") == 0) {
		printf(
		    "usage: xctrace record [<options>] [--attach | --all-processes "
		    "| --launch -- command ]\n"
		    "\n"
		    "description:\n"
		    "  Perform a new recording on the specified device and target "
		    "with the given template\n"
		    "\n"
		    "options:\n"
		    "  --output <path>                        Output .trace file "
		    "to the given path\n"
		    "  --append-run                           Appends a new run to "
		    "an existing trace file\n"
		    "  --run-name <name>                      Names the run\n"
		    "  --template <path|name>                 Record using given "
		    "trace template name or path\n"
		    "  --device <name|UDID>                   Record on device with "
		    "the given name or UDID\n"
		    "  --instrument <name>                    Add Instrument with "
		    "the given name to the recording\n"
		    "  --time-limit <time[ms|s|m|h]>          Limit recording time "
		    "to the specified value\n"
		    "  --window <duration[ms|s|m]>            Keep only events from "
		    "the specified end time window\n"
		    "  --package <file>                       Load Instruments Package "
		    "from given path for duration of the command\n"
		    "  --all-processes                        Record all processes\n"
		    "  --attach <pid|name>                    Attach and record "
		    "process with the given name or pid\n"
		    "  --launch -- command [arguments]        Launch process with "
		    "the given name or path\n"
		    "  --target-stdin <name>                  Redirect standard "
		    "input of the launched process\n"
		    "  --target-stdout <name>                 Redirect standard "
		    "output of the launched process\n"
		    "  --env <VAR=value>                      Set specified "
		    "environment variable for the launched process\n"
		    "  --notify-tracing-started               Send Darwin "
		    "notification with this name when a recording has started\n"
		    "  --no-prompt                            Skip any prompts that "
		    "would be otherwise presented (like privacy warnings)\n"
		    "\n"
		    "global options:\n"
		    "\t--quiet        Make terminal ouput less verbose\n"
		    "\n"
		    "examples:\n"
		    "\txctrace record --template 'Time Profiler' --all-processes "
		    "--time-limit 5s\n"
		    "\txctrace record --template 'Time Profiler' --all-processes "
		    "--output 'recording.trace'\n"
		    "\txctrace record --all-processes --append-run --run-name "
		    "Fix#2 --output 'existing.trace'\n"
		    "\txctrace record --template 'System Trace' --attach 4215\n"
		    "\txctrace record --template 'Time Profiler' --device "
		    "\"Chad's iPhone\" --attach 'Trailblazer'\n"
		    "\txctrace record --template 'Allocations' --env KEY=VALUE "
		    "--launch -- MyApp.app\n"
		    "\txctrace record --template 'Time Profiler' --target-stdout - "
		    "--launch -- /tmp/tool arg1 arg2\n");
	} else if (strcmp(cmd, "export") == 0) {
		printf(
		    "usage: xctrace export [<options>] [--toc | --xpath expression]\n"
		    "\n"
		    "description:\n"
		    "  Export given .trace using supplied query to the XML file "
		    "format that can be later read and post-processed\n"
		    "\n"
		    "options:\n"
		    "  --input <file>              Export data from the given .trace "
		    "file\n"
		    "  --output <path>             Command output is written to the "
		    "given path, if specified\n"
		    "  --toc                       Present entities to export in the "
		    "table of contents form\n"
		    "  --xpath <expression>        Choose elements to export using "
		    "specified XPath expression\n"
		    "  --har                       Export data as the HTTP Archive "
		    "file\n"
		    "\n"
		    "global options:\n"
		    "\t--quiet        Make terminal ouput less verbose\n"
		    "\n"
		    "notes:\n"
		    "\tIf output path is not specified, the export operation output "
		    "will be written to the standard output.\n"
		    "\tTable of Contents and XPath query are two separate modes and "
		    "they cannot be specified together.\n"
		    "\n"
		    "examples:\n"
		    "\txctrace export --input input.trace --toc\n"
		    "\txctrace export --input input.trace --toc --output "
		    "table_of_contents.xml\n"
		    "\txctrace export --input input.trace --xpath "
		    "'/trace-toc/run[@number=\"1\"]/data/table[@schema=\"my-table-schema\"]'\n");
	} else if (strcmp(cmd, "list") == 0) {
		printf(
		    "usage: xctrace list [devices|templates|instruments]\n"
		    "\n"
		    "description:\n"
		    "  List capabilities of the current running environment: "
		    "devices, templates and instruments\n"
		    "\n"
		    "global options:\n"
		    "\t--quiet        Make terminal ouput less verbose\n");
	} else if (strcmp(cmd, "import") == 0) {
		printf(
		    "usage: xctrace import [<options>]\n"
		    "\n"
		    "description:\n"
		    "  Import given supported file into Instruments .trace file "
		    "using specified template\n"
		    "  File can be later viewed using Instruments.app or exported "
		    "by using xctrace export command\n"
		    "\n"
		    "options:\n"
		    "  --input <file>                Create .trace file based on the "
		    "file from given path\n"
		    "  --output <path>               Output .trace file to given path\n"
		    "  --template <path|name>        Import using given trace template "
		    "name or path\n"
		    "  --instrument <name>           Add Instrument with the given name "
		    "to the import action\n"
		    "  --run-name <name>             Names the run\n"
		    "  --package <file>              Load Instruments Package from "
		    "given path for duration of the command\n"
		    "  --append-run                Appends a new run to an existing "
		    "trace file\n"
		    "\n"
		    "global options:\n"
		    "\t--quiet        Make terminal ouput less verbose\n"
		    "\n"
		    "notes:\n"
		    "\tIf an output path is not specified, a uniquely named file will "
		    "be created in the current directory.\n"
		    "\tIf the output path is a directory, then a unique file is "
		    "created in it. If the path contains the .trace\n"
		    "\textension — file will be created at the specified path.\n"
		    "\n"
		    "\tSome of the importable file types will have default template. "
		    "In these cases --template\n"
		    "\tparameter is optional. The default template for a given "
		    "import file UTI may change between releases.\n"
		    "\n"
		    "examples:\n"
		    "\txctrace import --input system_logs.logarchive\n"
		    "\txctrace import --input system_logs.logarchive --template "
		    "'Custom'\n"
		    "\txctrace import --input trace001.ktrace --template 'System "
		    "Trace' --output system.trace\n"
		    "\txctrace import --input trace001.ktrace --template 'Custom' "
		    "--run-name 'Initial Import' --output system.trace\n"
		    "\txctrace import --input trace001.ktrace --template 'Custom' "
		    "--package '/tmp/PackageToLoad.instrdst' --output system.trace\n");
	} else if (strcmp(cmd, "remodel") == 0) {
		printf(
		    "usage: xctrace remodel [<options>]\n"
		    "\n"
		    "description:\n"
		    "  Remodel given trace file using currently installed modelers\n"
		    "  New output trace file is generated as part of the remodel "
		    "operation\n"
		    "\n"
		    "options:\n"
		    "  --input <file>          Remodel data from the given .trace file\n"
		    "  --output <path>         Output remodeled .trace file to the "
		    "given path\n"
		    "  --package <file>        Load Instruments Package from given "
		    "path for duration of the command\n"
		    "\n"
		    "global options:\n"
		    "\t--quiet        Make terminal ouput less verbose\n"
		    "\n"
		    "notes:\n"
		    "\tIf an output path is not specified, a uniquely named file "
		    "will be created in the current directory.\n"
		    "\tIf the output path is a directory, then a unique file is "
		    "created in it. If the path contains the .trace\n"
		    "\textension — file will be created at the specified path.\n"
		    "\tTo remodel existing file in place, specify this path in the "
		    "output file argument\n"
		    "\n"
		    "examples:\n"
		    "\txctrace remodel --input input.trace --output output.trace "
		    "--package '/tmp/PackageToLoad.instrdst'\n");
	} else if (strcmp(cmd, "symbolicate") == 0) {
		printf(
		    "usage: xctrace symbolicate [<options>] [--dsym path]\n"
		    "\n"
		    "description:\n"
		    "  Symbolicate given .trace using supplied dSYM\n"
		    "\n"
		    "options:\n"
		    "  --input <file>        Symbolicate addresses in given .trace "
		    "file\n"
		    "  --output <path>      Output symbolicated .trace file to the "
		    "given path\n"
		    "  --dsym <path>        Use dSYM from given path as source of "
		    "symbol data\n"
		    "\n"
		    "global options:\n"
		    "\t--quiet        Make terminal ouput less verbose\n"
		    "\n"
		    "notes:\n"
		    "\tIf an output path is not specified, the symbolicated trace is "
		    "saved at the same path as the original one.\n"
		    "\tIf dsym path is a directory, it will be searched recursively "
		    "for relevant dSYMs.\n"
		    "\tIf dsym path is not specified, best effort will be made to "
		    "locate relevant dSYMs on the system.\n"
		    "\n"
		    "examples:\n"
		    "\txctrace symbolicate --input input.trace --output "
		    "symbolicated.trace --dsym SomeLibrary.dSYM\n");
	} else {
		print_usage(stdout);
	}
}

int
main(int argc, char **argv)
{
	static struct option longopts[] = {
		{"quiet",    no_argument,       0, 'q'},
		{"help",     no_argument,       0, 'h'},
		{NULL, 0, 0, 0},
	};

	int c;
	/* Use '+' prefix to stop at first non-option (the command name). */
	while ((c = getopt_long(argc, argv, "+qh", longopts, NULL)) != -1) {
		switch (c) {
		case 'q':
			quiet = 1;
			break;
		case 'h':
			print_usage(stdout);
			return 0;
		default:
			fprintf(stderr,
			    "usage: %s [options] <command> [command options]\n",
			    program_name);
			return 1;
		}
	}

	if (optind >= argc) {
		print_usage(stderr);
		return 1;
	}

	const char *cmd = argv[optind];
	int cmd_argc = argc - optind;
	char **cmd_argv = &argv[optind];

	if (strcmp(cmd, "help") == 0 || strcmp(cmd, "-h") == 0 ||
	    strcmp(cmd, "--help") == 0) {
		if (cmd_argc > 1)
			print_command_help(cmd_argv[1]);
		else
			print_usage(stdout);
		return 0;
	}

	if (strcmp(cmd, "version") == 0 || strcmp(cmd, "-V") == 0 ||
	    strcmp(cmd, "--version") == 0) {
		printf("%s\n", VERSION_STRING);
		return 0;
	}

	if (strcmp(cmd, "record") == 0) {
		return cmd_record(cmd_argc, cmd_argv);
	}

	if (strcmp(cmd, "export") == 0) {
		return cmd_export(cmd_argc, cmd_argv);
	}

	if (strcmp(cmd, "list") == 0) {
		return cmd_list(cmd_argc, cmd_argv);
	}

	if (strcmp(cmd, "import") == 0) {
		fprintf(stderr, "%s: import is not yet supported\n",
		    program_name);
		return 1;
	}

	if (strcmp(cmd, "remodel") == 0) {
		fprintf(stderr, "%s: remodel is not yet supported\n",
		    program_name);
		return 1;
	}

	if (strcmp(cmd, "symbolicate") == 0) {
		fprintf(stderr, "%s: symbolicate is not yet supported\n",
		    program_name);
		return 1;
	}

	fprintf(stderr, "%s: unknown command '%s'\n", program_name, cmd);
	print_usage(stderr);
	return 1;
}

/* --- cmd_record --- */

static int
cmd_record(int argc, char **argv)
{
	return xc_record(argc, argv, 1, quiet);
}

/* --- cmd_export --- */

static int
cmd_export(int argc, char **argv)
{
	return xc_export(argc, argv, 1, quiet);
}

/* --- cmd_list --- */

static int
cmd_list(int argc, char **argv)
{
	const char *subcmd = "all";

	if (argc > 1)
		subcmd = argv[1];

	if (strcmp(subcmd, "devices") == 0) {
		printf("== Devices ==\n");
		list_physical_devices();
		printf("\n== Simulators ==\n");
		list_simulators();
	} else if (strcmp(subcmd, "templates") == 0) {
		printf("== Standard Templates ==\n");
		list_templates();
	} else if (strcmp(subcmd, "instruments") == 0 ||
	    strcmp(subcmd, "instrument") == 0) {
		printf("== Standard Instruments ==\n");
		list_instruments();
	} else if (strcmp(subcmd, "all") == 0 ||
	    strcmp(subcmd, "list") == 0) {
		printf("== Devices ==\n");
		list_physical_devices();
		printf("\n== Simulators ==\n");
		list_simulators();
		printf("\n== Standard Templates ==\n");
		list_templates();
		printf("\n== Standard Instruments ==\n");
		list_instruments();
	} else {
		fprintf(stderr, "%s: list: unknown subcommand '%s'\n",
		    program_name, subcmd);
		fprintf(stderr,
		    "usage: %s list [devices|templates|instruments]\n",
		    program_name);
		return 1;
	}

	return 0;
}
