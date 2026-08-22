/*
 * xc_record - xctrace record subcommand implementation.
 *
 * Launches Instruments.app (or falls back to the system xctrace if present)
 * to perform a trace recording with the specified template and target.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>

#include "xctrace.h"

static const char *program_name = "xctrace";

/* Locate the Instruments.app bundle. */
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

	/* Try deriving from xcode-select. */
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

	/* Fall back to /usr/bin/xctrace. */
	if (access("/usr/bin/xctrace", X_OK) == 0) {
		snprintf(buf, n, "/usr/bin/xctrace");
		return 0;
	}

	return -1;
}

/* Build a default output path like Instruments would. */
static void
default_output_path(char *buf, size_t n)
{
	time_t t = time(NULL);
	struct tm tm;
	gmtime_r(&t, &tm);
	char ts[32];
	strftime(ts, sizeof ts, "%Y-%m-%d_%H-%M-%S", &tm);
	snprintf(buf, n, "%s.trace", ts);
}

/*
 * xc_record - handle "xctrace record ..."
 *
 * Options we parse:
 *   --output <path>         Output .trace file
 *   --template <name|path>  Trace template to use
 *   --device <name|UDID>    Target device
 *   --attach <pid|name>     Attach to a process
 *   --all-processes         Record all processes
 *   --time-limit <dur>      Recording duration
 *   --launch -- command     Launch a process
 *   --no-prompt             Skip prompts
 */
int
xc_record(int argc, char **argv, int optind, int quiet)
{
	const char *output = NULL;
	const char *template = NULL;
	const char *device = NULL;
	const char *attach = NULL;
	int all_processes = 0;
	const char *time_limit = NULL;
	const char *launch_cmd = NULL;
	int no_prompt = 0;
	(void)device;
	(void)attach;
	(void)time_limit;
	(void)launch_cmd;
	(void)no_prompt;

	for (int i = optind; i < argc; i++) {
		if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
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
		} else if (argv[i][0] == '-' && argv[i][1] == '-' &&
		    strcmp(argv[i], "--append-run") != 0 &&
		    strcmp(argv[i], "--run-name") != 0 &&
		    strcmp(argv[i], "--instrument") != 0 &&
		    strcmp(argv[i], "--window") != 0 &&
		    strcmp(argv[i], "--package") != 0 &&
		    strcmp(argv[i], "--target-stdin") != 0 &&
		    strcmp(argv[i], "--target-stdout") != 0 &&
		    strcmp(argv[i], "--env") != 0 &&
		    strcmp(argv[i], "--notify-tracing-started") != 0) {
			if (!quiet)
				fprintf(stderr, "%s: unknown option %s\n",
				    program_name, argv[i]);
		}
	}

	/* If output not specified, generate a default one. */
	char default_out[256];
	if (output == NULL) {
		default_output_path(default_out, sizeof default_out);
		output = default_out;
	}

	/* Find Instruments.app. */
	char instr_path[1024];
	if (find_instruments_app(instr_path, sizeof instr_path) != 0) {
		fprintf(stderr, "%s: cannot locate Instruments.app or xctrace\n",
		    program_name);
		return 1;
	}

	if (!quiet) {
		fprintf(stderr, "%s: starting recording with template '%s'\n",
		    program_name, template ? template : "Time Profiler");
	}

	/* Try to use system xctrace if available. */
	if (strstr(instr_path, "/xctrace") != NULL) {
		/* Fall back to system xctrace. */
		pid_t pid = fork();
		if (pid == 0) {
			if (template && all_processes && time_limit) {
				execl(instr_path, "xctrace", "record",
				    "--output", output,
				    "--template", template,
				    "--all-processes",
				    "--time-limit", time_limit,
				    "--no-prompt", (char *)NULL);
			} else if (template && all_processes) {
				execl(instr_path, "xctrace", "record",
				    "--output", output,
				    "--template", template,
				    "--all-processes",
				    (char *)NULL);
			} else {
				execl(instr_path, "xctrace", "record",
				    "--output", output,
				    "--all-processes",
				    (char *)NULL);
			}
			_exit(127);
		}
		int status = 0;
		waitpid(pid, &status, 0);
		return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : 1;
	}

	/* Launch Instruments.app with the template. */
	char tmpl_path[1024];
	snprintf(tmpl_path, sizeof tmpl_path, "/Applications/Xcode.app/"
	    "Contents/Applications/Instruments.app/Contents/Resources/"
	    "templates/%s.tracetemplate", template ? template : "Time Profiler");

	struct stat st;
	char cmd[2048];
	if (stat(tmpl_path, &st) == 0) {
		snprintf(cmd, sizeof cmd,
		    "open -a Instruments '%s' 2>/dev/null", tmpl_path);
	} else {
		snprintf(cmd, sizeof cmd,
		    "open -a Instruments 2>/dev/null");
	}

	int rc = system(cmd);
	if (rc != 0 && !quiet)
		fprintf(stderr, "%s: failed to launch Instruments.app\n",
		    program_name);

	return rc;
}
