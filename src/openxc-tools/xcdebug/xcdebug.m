/*
 * xcdebug -- start a debugging session in Xcode.
 *
 * Two modes, both of which end in a message to the Xcode application: spawn
 * a command and ask Xcode to attach to it, or ask Xcode to run a scheme.
 * The work is done through Xcode's own scripting vocabulary -- attach, debug
 * and create temporary debugging workspace are commands in its dictionary --
 * so this drives the same interface Apple's does and needs Xcode installed
 * to do anything, exactly as theirs does.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#import <Foundation/Foundation.h>

#include <signal.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define XCDEBUG_VERSION	"1.0.0"

extern char **environ;

/*
 * The help and the invalid-option line go to stderr and exit 1; only
 * --version goes to stdout, and only it exits 0.
 */
static void
usage(void)
{
	(void)fputs(
	    "xcdebug -- Start a debugging session in Xcode.\n"
	    "\n"
	    "There are two ways to use xcdebug:\n"
	    "\n"
	    "(1) xcdebug [options] <command> [args ...]\n"
	    "    Starts a 'process-based' debugging session: Spawns the given command with\n"
	    "    the given arguments in the current terminal and asks the most recently used\n"
	    "    Xcode window to attach to it (opens a new temporary debugging-only window if\n"
	    "    no windows are open).\n"
	    "\n"
	    "(2) xcdebug -s <name> [options] [args ...]\n"
	    "    Starts a 'scheme-based' debugging session: Triggers the 'Run Without\n"
	    "    Building' action in Xcode using the given scheme name. The process\n"
	    "    to run is defined by the scheme. If arguments are specified, they are\n"
	    "    appended as extra arguments when launching.\n"
	    "\n"
	    "Options (general):\n"
	    "    -h, --help                Show this information.\n"
	    "    -v, --version             Print version information.\n"
	    "    -p, --pid <pid>           Target Xcode with process id <pid>.\n"
	    "    -x, --xcode-path <path>   Target Xcode from path <path>.\n"
	    "    -w, --workspace <name>    Target workspace named <name>.\n"
	    "    -b, --background          Leave Xcode as a background app.\n"
	    "\n"
	    "Options (process-based):\n"
	    "    -S, --suspended           Start debugging in a suspended state.\n"
	    "    -t, --temporary-workspace Always create a new temporary workspace.\n"
	    "\n"
	    "Options (scheme-based):\n"
	    "    -s, --scheme <name>       Launch target using the scheme <name>.\n"
	    "    -d, --destination <spec>  Use a run destination matching <spec>.\n"
	    "    -B, --build               Perform 'Run' instead of 'Run Without Building'.\n"
	    "    -e, --environment <env>   Use an extra environment variable of NAME=VALUE.\n"
	    "                              This flag can be used multiple times.\n"	    , stderr);
}

/* Quote a string for embedding in an AppleScript literal. */
static NSString *
script_quoted(NSString *s)
{
	NSString *escaped = [s stringByReplacingOccurrencesOfString:@"\\"
	    withString:@"\\\\"];

	escaped = [escaped stringByReplacingOccurrencesOfString:@"\""
	    withString:@"\\\""];
	return [NSString stringWithFormat:@"\"%@\"", escaped];
}

/*
 * Run one statement against Xcode.  NSAppleScript rather than a generated
 * ScriptingBridge header: the vocabulary is small and fixed, and this keeps
 * the tool to Foundation.
 */
static bool
tell_xcode(NSString *statement, bool background)
{
	NSString *activate = background ? @"" : @"\tactivate\n";
	NSString *source = [NSString stringWithFormat:
	    @"tell application \"Xcode\"\n%@\t%@\nend tell\n", activate,
	    statement];
	NSDictionary *error = nil;
	NSAppleScript *script = [[NSAppleScript alloc] initWithSource:source];

	(void)[script executeAndReturnError:&error];
	if (error != nil) {
		NSString *message = error[NSAppleScriptErrorMessage];

		if (message != nil)
			(void)fprintf(stderr, "xcdebug: %s\n",
			    message.UTF8String);
		return (false);
	}
	return (true);
}

/* The workspace to talk to: a named one if asked for, else the front one. */
static NSString *
workspace_specifier(const char *named)
{
	if (named != NULL)
		return ([NSString stringWithFormat:@"workspace document %@",
		    script_quoted(@(named))]);

	return (@"front workspace document");
}

int
main(int argc, char *argv[])
{
	@autoreleasepool {
		NSMutableArray<NSString *> *envs = [NSMutableArray array];
		const char *scheme = NULL, *destination = NULL;
		const char *workspace = NULL;
		bool background = false, suspended = false;
		bool temporary = false, build = false;
		int i;

		for (i = 1; i < argc; i++) {
			const char *a = argv[i];
			bool takes_value;

			if (a[0] != '-' || a[1] == '\0')
				break;		/* the command begins here */

			if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
				usage();
				return (1);
			}
			if (strcmp(a, "-v") == 0 ||
			    strcmp(a, "--version") == 0) {
				(void)printf("xcdebug version %s\n",
				    XCDEBUG_VERSION);
				return (0);
			}
			if (strcmp(a, "-b") == 0 ||
			    strcmp(a, "--background") == 0) {
				background = true;
				continue;
			}
			if (strcmp(a, "-S") == 0 ||
			    strcmp(a, "--suspended") == 0) {
				suspended = true;
				continue;
			}
			if (strcmp(a, "-t") == 0 ||
			    strcmp(a, "--temporary-workspace") == 0) {
				temporary = true;
				continue;
			}
			if (strcmp(a, "-B") == 0 || strcmp(a, "--build") == 0) {
				build = true;
				continue;
			}

			takes_value = (strcmp(a, "-s") == 0 ||
			    strcmp(a, "--scheme") == 0 ||
			    strcmp(a, "-d") == 0 ||
			    strcmp(a, "--destination") == 0 ||
			    strcmp(a, "-e") == 0 ||
			    strcmp(a, "--environment") == 0 ||
			    strcmp(a, "-p") == 0 || strcmp(a, "--pid") == 0 ||
			    strcmp(a, "-x") == 0 ||
			    strcmp(a, "--xcode-path") == 0 ||
			    strcmp(a, "-w") == 0 ||
			    strcmp(a, "--workspace") == 0);

			/*
			 * An option that wants a value and has none is not a
			 * missing-argument error in Apple's: it is reported
			 * as an invalid option, which is what this says too.
			 */
			if (!takes_value || i + 1 >= argc) {
				(void)fprintf(stderr, "invalid option: %s\n", a);
				usage();
				return (1);
			}

			if (strcmp(a, "-s") == 0 || strcmp(a, "--scheme") == 0)
				scheme = argv[++i];
			else if (strcmp(a, "-d") == 0 ||
			    strcmp(a, "--destination") == 0)
				destination = argv[++i];
			else if (strcmp(a, "-e") == 0 ||
			    strcmp(a, "--environment") == 0)
				[envs addObject:@(argv[++i])];
			else if (strcmp(a, "-w") == 0 ||
			    strcmp(a, "--workspace") == 0)
				workspace = argv[++i];
			else
				i++;	/* -p and -x select which Xcode */
		}

		if (scheme != NULL) {
			NSMutableString *stmt = [NSMutableString
			    stringWithFormat:@"debug %@ scheme %@",
			    workspace_specifier(workspace),
			    script_quoted(@(scheme))];

			if (destination != NULL)
				[stmt appendFormat:
				    @" run destination specifier %@",
				    script_quoted(@(destination))];

			/* Without -B the action is Run Without Building. */
			[stmt appendFormat:@" skip building %@",
			    build ? @"false" : @"true"];

			if (i < argc) {
				NSMutableArray<NSString *> *args =
				    [NSMutableArray array];

				for (; i < argc; i++)
					[args addObject:
					    script_quoted(@(argv[i]))];
				[stmt appendFormat:
				    @" command line arguments {%@}",
				    [args componentsJoinedByString:@", "]];
			}

			return (tell_xcode(stmt, background) ? 0 : 1);
		}

		if (i >= argc) {
			usage();
			return (1);
		}

		/*
		 * Process mode.  The child is stopped before it runs so Xcode
		 * can attach to it from the first instruction; without -S it
		 * is continued once the attach has been asked for.
		 */
		{
			posix_spawnattr_t attr;
			pid_t pid;
			NSString *stmt;
			int status;

			(void)posix_spawnattr_init(&attr);
			(void)posix_spawnattr_setflags(&attr,
			    POSIX_SPAWN_START_SUSPENDED);

			if (posix_spawnp(&pid, argv[i], NULL, &attr,
			    &argv[i], environ) != 0) {
				(void)fprintf(stderr,
				    "xcdebug: could not run %s\n", argv[i]);
				return (1);
			}
			(void)posix_spawnattr_destroy(&attr);

			if (temporary)
				(void)tell_xcode(
				    @"create temporary debugging workspace",
				    background);

			stmt = [NSString stringWithFormat:
			    @"attach %@ to process identifier %d suspended %@",
			    workspace_specifier(workspace), (int)pid,
			    suspended ? @"true" : @"false"];

			if (!tell_xcode(stmt, background)) {
				(void)kill(pid, SIGKILL);
				return (1);
			}

			(void)kill(pid, SIGCONT);
			(void)waitpid(pid, &status, 0);
			return (WIFEXITED(status) ? WEXITSTATUS(status) : 1);
		}
	}
}
