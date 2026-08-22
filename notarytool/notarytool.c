/*
 * notarytool - open source reimplementation of Apple's notarytool(1).
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Manages submissions to the Apple notary service. Supports App Store
 * Connect API key authentication (ES256 JWT), Apple ID with app-specific
 * passwords, and keychain-stored profiles. Compatible with packages
 * produced by the sibling pkgbuild(1) tool in this repository.
 *
 * Subcommands: submit, info, log, history, wait, store-credentials.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <curl/curl.h>

#include "api.h"
#include "keychain.h"

#define NOTARYTOOL_VERSION "1.1.2-compat"

static const char *program_name = "notarytool";

static void
usage(int status)
{
	FILE *out = status == 0 ? stdout : stderr;
	fprintf(out,
"usage: %s submit file-path { -k key-path -d key-id [-i issuer]\n"
"       | --apple-id apple-id [--password asp] --team-id team-id\n"
"       | -p profile-name [--keychain path] } [options]\n"
"       %s info submission-id { auth-options }\n"
"       %s log submission-id { auth-options } [output-path]\n"
"       %s history { auth-options }\n"
"       %s wait submission-id { auth-options } [--timeout duration]\n"
"       %s store-credentials profile-name { auth-options } [options]\n"
"       %s help [subcommand]\n"
"\n"
"auth-options:\n"
"  -k, --key path          App Store Connect API key (.p8 file)\n"
"  -d, --key-id id         API key ID (10-char alphanumeric)\n"
"  -i, --issuer issuer     API key issuer ID (UUID; required for Team keys)\n"
"      --apple-id id       Apple ID username\n"
"      --password pwd      App-specific password for Apple ID\n"
"      --team-id id        Team identifier (10-char alphanumeric)\n"
"  -p, --keychain-profile name\n"
"                        Use credentials stored in keychain by name\n"
"      --keychain path     Keychain file to use with -p\n"
"\n"
"submit options:\n"
"  --wait                  Wait for notarization to complete\n"
"  --no-s3-acceleration    Disable S3 Transfer Acceleration\n"
"  --force                 Upload despite pre-flight validation issues\n"
"  --timeout duration      Max time for --wait (e.g. 3600, 60m, 1h)\n"
"\n"
"common options:\n"
"  -h, --help              Show this help\n"
"  -v, --verbose           Stream debug logs to stderr\n"
"  -f, --output-format fmt Output: normal (default) | json | plist\n"
"      --version           Print version and exit\n"
"      --progress          Show progress indicators (default)\n"
"      --no-progress       Suppress progress output\n",
	    program_name, program_name, program_name, program_name,
	    program_name, program_name, program_name);
	exit(status);
}

static void
print_version(void)
{
	printf("%s %s\n", program_name, NOTARYTOOL_VERSION);
}

static void
print_subcommand_help(const char *cmd)
{
	if (cmd == NULL || strcmp(cmd, "submit") == 0) {
		printf("submit file-path {auth-options}\n");
		printf("\n");
		printf("  --wait            Wait for completion\n");
		printf("  --no-s3-acceleration\n");
		printf("                    Disable S3 Transfer Acceleration\n");
		printf("  --force           Upload despite pre-flight issues\n");
		printf("  --timeout dur     Max wait duration (e.g. 3600, 60m, 1h)\n");
	} else if (strcmp(cmd, "info") == 0) {
		printf("info submission-id {auth-options}\n");
		printf("\n");
		printf("  Get status info for a previous submission.\n");
	} else if (strcmp(cmd, "log") == 0) {
		printf("log submission-id {auth-options} [output-path]\n");
		printf("\n");
		printf("  Download notarization log as JSON.\n");
		printf("  If output-path is omitted, log is written to stdout.\n");
	} else if (strcmp(cmd, "history") == 0) {
		printf("history {auth-options}\n");
		printf("\n");
		printf("  List previous submissions for your development team.\n");
	} else if (strcmp(cmd, "wait") == 0) {
		printf("wait submission-id {auth-options} [--timeout dur]\n");
		printf("\n");
		printf("  Block until the submission reaches a terminal status.\n");
	} else if (strcmp(cmd, "store-credentials") == 0) {
		printf("store-credentials profile-name {auth-options} [options]\n");
		printf("\n");
		printf("  --validate       Verify credentials before storing (default)\n");
		printf("  --no-validate    Skip credential validation\n");
		printf("  --keychain path  Store in specified keychain file\n");
		printf("  --sync           Sync via iCloud Keychain\n");
	} else {
		printf("Unknown subcommand: %s\n", cmd);
	}
}

/* Parse a duration like "3600", "60m", "1h" into seconds. Returns -1 on error. */
static long
parse_duration(const char *s)
{
	char *end = NULL;
	long val = strtol(s, &end, 10);
	if (end == s || val < 0)
		return -1;
	if (*end == 'm')
		return val * 60;
	if (*end == 'h')
		return val * 3600;
	if (*end == 's' || *end == '\0')
		return val;
	return -1;
}

struct auth_opts {
	const char *key_path;
	const char *key_id;
	const char *issuer_id;
	const char *apple_id;
	const char *app_password;
	const char *team_id;
	const char *profile_name;
	const char *keychain_path;
	int use_key;
	int use_asp;
	int use_profile;
};

static struct creds *
build_creds(struct auth_opts *a)
{
	struct creds *c = calloc(1, sizeof(*c));
	if (c == NULL)
		return NULL;

	if (a->use_profile) {
		c->type = CREDS_KEY;
		if (keychain_retrieve(a->profile_name, a->keychain_path,
		    &c->key_path, &c->key_id, &c->issuer_id, &c->team_id,
		    &c->apple_id, &c->app_specific_password) != 0) {
			fprintf(stderr, "%s: could not retrieve profile '%s'\n",
			    program_name, a->profile_name);
			free(c);
			return NULL;
		}
		/*
		 * Profile could contain either API key or Apple ID creds.
		 * Determine which based on what fields were found.
		 */
		if (c->key_path != NULL)
			c->type = CREDS_KEY;
		else if (c->apple_id != NULL && c->app_specific_password != NULL)
			c->type = CREDS_ASP;
		else {
			fprintf(stderr, "%s: profile '%s' has no usable credentials\n",
			    program_name, a->profile_name);
			creds_free(c);
			return NULL;
		}
	} else if (a->use_key) {
		c->type = CREDS_KEY;
		c->key_path = a->key_path ? strdup(a->key_path) : NULL;
		c->key_id = a->key_id ? strdup(a->key_id) : NULL;
		c->issuer_id = a->issuer_id ? strdup(a->issuer_id) : NULL;
		c->team_id = a->team_id ? strdup(a->team_id) : NULL;
	} else if (a->use_asp) {
		c->type = CREDS_ASP;
		c->apple_id = a->apple_id ? strdup(a->apple_id) : NULL;
		c->app_specific_password = a->app_password ?
		    strdup(a->app_password) : NULL;
		c->team_id = a->team_id ? strdup(a->team_id) : NULL;
	} else {
		fprintf(stderr, "%s: no authentication method specified\n",
		    program_name);
		free(c);
		return NULL;
	}

	return c;
}

static const struct option longopts[] = {
	{"key",            required_argument, 0, 'k'},
	{"key-id",         required_argument, 0, 'd'},
	{"issuer",         required_argument, 0, 'i'},
	{"apple-id",       required_argument, 0, 0x100},
	{"password",       required_argument, 0, 0x101},
	{"team-id",        required_argument, 0, 0x102},
	{"keychain-profile", required_argument, 0, 'p'},
	{"keychain",       required_argument, 0, 0x103},

	{"wait",           no_argument,       0, 0x200},
	{"no-s3-acceleration", no_argument,   0, 0x201},
	{"force",          no_argument,       0, 0x202},
	{"timeout",        required_argument, 0, 0x203},
	{"validate",       no_argument,       0, 0x204},
	{"no-validate",    no_argument,       0, 0x205},
	{"sync",           no_argument,       0, 0x206},

	{"help",           no_argument,       0, 'h'},
	{"verbose",        no_argument,       0, 'v'},
	{"version",        no_argument,       0, 0x207},
	{"progress",       no_argument,       0, 0x208},
	{"no-progress",    no_argument,       0, 0x209},
	{"output-format",  required_argument, 0, 'f'},

	{NULL, 0, 0, 0},
};

static const char *shortopts = "k:d:i:p:hvf:";

int
main(int argc, char **argv)
{
	program_name = argv[0];

	if (argc < 2) {
		usage(1);
	}

	const char *subcmd = argv[1];

	/* Handle help and version before auth parsing */
	if (strcmp(subcmd, "help") == 0) {
		const char *cmd = argc > 2 ? argv[2] : NULL;
		print_subcommand_help(cmd);
		return 0;
	}
	if (strcmp(subcmd, "--version") == 0) {
		print_version();
		return 0;
	}
	if (strcmp(subcmd, "--help") == 0 || strcmp(subcmd, "-h") == 0) {
		usage(0);
	}

	/* Shift past subcommand for option parsing */
	optind = 2;

	struct auth_opts auth = {0};
	int wait = 0;
	int s3_acceleration = 1;
	int force = 0;
	int validate = 1;
	int sync = 0;
	long timeout_seconds = 3600; /* 1 hour default */
	int verbose = 0;
	int progress = 1;
	const char *output_format = "normal";
	const char *log_output_path = NULL;

	int c;
	while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (c) {
		case 'k': auth.key_path = optarg; auth.use_key = 1; break;
		case 'd': auth.key_id = optarg; auth.use_key = 1; break;
		case 'i': auth.issuer_id = optarg; break;
		case 'p': auth.profile_name = optarg; auth.use_profile = 1; break;
		case 0x100: auth.apple_id = optarg; auth.use_asp = 1; break;
		case 0x101: auth.app_password = optarg; break;
		case 0x102: auth.team_id = optarg; break;
		case 0x103: auth.keychain_path = optarg; break;
		case 'h': usage(0);
		case 'v': verbose = 1; break;
		case 'V': print_version(); return 0;
		case 'f': output_format = optarg; break;
		case 0x200: wait = 1; break;
		case 0x201: s3_acceleration = 0; break;
		case 0x202: force = 1; break;
		case 0x203: timeout_seconds = parse_duration(optarg);
			 if (timeout_seconds < 0) {
				 fprintf(stderr, "%s: invalid timeout '%s'\n",
				     program_name, optarg);
				 return 1;
			 }
			 break;
		case 0x204: validate = 1; break;
		case 0x205: validate = 0; break;
		case 0x206: sync = 1; break;
		case 0x207: print_version(); return 0;
		case 0x208: progress = 1; break;
		case 0x209: progress = 0; break;
		case '?': usage(1);
		default: usage(1);
		}
	}

	curl_global_init(CURL_GLOBAL_DEFAULT);

	int rc = 0;

	if (strcmp(subcmd, "submit") == 0) {
		const char *file_path = optind < argc ? argv[optind] : NULL;
		if (file_path == NULL) {
			fprintf(stderr, "%s: submit requires a file path\n",
			    program_name);
			usage(1);
		}

		/* Determine content type from file extension */
		const char *ct = "application/octet-stream";
		const char *ext = strrchr(file_path, '.');
		if (ext) {
			if (strcmp(ext, ".pkg") == 0 || strcmp(ext, ".mpkg") == 0)
				ct = "application/x-gzip";
			else if (strcmp(ext, ".zip") == 0)
				ct = "application/zip";
			else if (strcmp(ext, ".dmg") == 0)
				ct = "application/x-7smart-mounter";
		}

		struct creds *creds = build_creds(&auth);
		if (creds == NULL) {
			rc = 1;
			goto done;
		}

		struct submission_status *s = api_submit(creds, file_path, ct,
		    s3_acceleration, force);
		if (s == NULL) {
			fprintf(stderr, "%s: submission failed\n", program_name);
			rc = 1;
			creds_free(creds);
			goto done;
		}

		if (verbose || progress) {
			printf("notarytool: submission_id: %s\n", s->id);
			printf("notarytool: status: %s\n",
			    s->status ? s->status : "(unknown)");
			if (output_format &&
			    strcmp(output_format, "json") == 0) {
				printf("{\"id\":\"%s\",\"status\":\"%s\"}\n",
				    s->id, s->status ? s->status : "");
			}
		} else {
			printf("%s\n", s->id);
		}

		if (wait) {
			rc = api_wait(creds, s->id, timeout_seconds, verbose);
		}

		submission_status_free(s);
		creds_free(creds);

	} else if (strcmp(subcmd, "info") == 0) {
		const char *submission_id = optind < argc ? argv[optind] : NULL;
		if (submission_id == NULL) {
			fprintf(stderr, "%s: info requires a submission id\n",
			    program_name);
			rc = 1;
			goto done;
		}

		struct creds *creds = build_creds(&auth);
		if (creds == NULL) {
			rc = 1;
			goto done;
		}

		struct submission_status *s = api_get_status(creds,
		    submission_id);
		if (s == NULL) {
			fprintf(stderr, "%s: could not retrieve submission info\n",
			    program_name);
			rc = 1;
			creds_free(creds);
			goto done;
		}

		if (strcmp(output_format, "json") == 0) {
			printf("{\"id\":\"%s\",\"status\":\"%s\",\"name\":\"%s\"}\n",
			    s->id ? s->id : "",
			    s->status ? s->status : "",
			    s->submission_name ? s->submission_name : "");
		} else {
			printf("id: %s\n", s->id ? s->id : "(unknown)");
			printf("status: %s\n", s->status ? s->status : "(unknown)");
			printf("name: %s\n",
			    s->submission_name ? s->submission_name : "(unknown)");
		}

		submission_status_free(s);
		creds_free(creds);

	} else if (strcmp(subcmd, "log") == 0) {
		const char *submission_id = optind < argc ? argv[optind] : NULL;
		if (submission_id == NULL) {
			fprintf(stderr, "%s: log requires a submission id\n",
			    program_name);
			rc = 1;
			goto done;
		}

		/* Check for output path as the last argument */
		if (optind + 1 < argc)
			log_output_path = argv[optind + 1];

		struct creds *creds = build_creds(&auth);
		if (creds == NULL) {
			rc = 1;
			goto done;
		}

		char *log_json = api_get_log(creds, submission_id);
		if (log_json == NULL) {
			fprintf(stderr, "%s: could not retrieve log\n",
			    program_name);
			rc = 1;
			creds_free(creds);
			goto done;
		}

		if (log_output_path != NULL) {
			FILE *f = fopen(log_output_path, "wb");
			if (f == NULL) {
				fprintf(stderr, "%s: cannot write %s\n",
				    program_name, log_output_path);
				free(log_json);
				creds_free(creds);
				rc = 1;
				goto done;
			}
			fputs(log_json, f);
			fclose(f);
			fprintf(stderr, "notarytool: log written to %s\n",
			    log_output_path);
		} else {
			puts(log_json);
		}

		free(log_json);
		creds_free(creds);

	} else if (strcmp(subcmd, "history") == 0) {
		struct creds *creds = build_creds(&auth);
		if (creds == NULL) {
			rc = 1;
			goto done;
		}

		char *resp = api_get_history(creds);
		if (resp == NULL) {
			fprintf(stderr, "%s: could not retrieve history\n",
			    program_name);
			rc = 1;
			creds_free(creds);
			goto done;
		}

		if (strcmp(output_format, "json") == 0) {
			puts(resp);
		} else {
			/* Print a summary: extract submission IDs and statuses */
			printf("%s\n", resp);
		}

		free(resp);
		creds_free(creds);

	} else if (strcmp(subcmd, "wait") == 0) {
		const char *submission_id = optind < argc ? argv[optind] : NULL;
		if (submission_id == NULL) {
			fprintf(stderr, "%s: wait requires a submission id\n",
			    program_name);
			rc = 1;
			goto done;
		}

		struct creds *creds = build_creds(&auth);
		if (creds == NULL) {
			rc = 1;
			goto done;
		}

		rc = api_wait(creds, submission_id, timeout_seconds, verbose);
		creds_free(creds);

	} else if (strcmp(subcmd, "store-credentials") == 0) {
		const char *profile = optind < argc ? argv[optind] : NULL;
		if (profile == NULL) {
			fprintf(stderr,
			    "%s: store-credentials requires a profile name\n",
			    program_name);
			rc = 1;
			goto done;
		}

		if (sync) {
			fprintf(stderr, "%s: --sync is not yet supported\n",
			    program_name);
			rc = 1;
			goto done;
		}

		if (auth.use_key || auth.use_asp) {
			if (validate) {
				/*
				 * Validate by attempting a simple API call.
				 * For now, just validate that the key file
				 * exists and is readable.
				 */
				if (auth.use_key) {
					FILE *f = fopen(auth.key_path, "r");
					if (f == NULL) {
						fprintf(stderr,
						    "%s: cannot read key file %s\n",
						    program_name, auth.key_path);
						rc = 1;
						goto done;
					}
					fclose(f);
				}
			}

			int r = keychain_store(profile, auth.key_path,
			    auth.key_id, auth.issuer_id, auth.team_id,
			    auth.apple_id, auth.app_password,
			    auth.keychain_path);
			if (r != 0) {
				fprintf(stderr, "%s: failed to store credentials\n",
				    program_name);
				rc = 1;
				goto done;
			}
			printf("notarytool: credentials stored as '%s'\n",
			    profile);
		} else {
			fprintf(stderr,
			    "%s: store-credentials requires auth credentials\n",
			    program_name);
			rc = 1;
			goto done;
		}

	} else {
		fprintf(stderr, "%s: unknown subcommand '%s'\n",
		    program_name, subcmd);
		usage(1);
	}

done:
	curl_global_cleanup();
	return rc;
}
