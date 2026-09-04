/*
 * codesign.c - Main entry point for codesign CLI.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * A reimplementation of Apple's codesign tool for signing and verifying
 * Mach-O binaries and app bundles on macOS.
 */

#include "codesign.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <getopt.h>
#include <unistd.h>
#include <openssl/evp.h>

static const char *progname = "codesign";

static const char *usage =
"Usage: %s [options] [file ...]\n"
"  -s, --sign IDENTITY                   sign with IDENTITY (use - for ad-hoc)\n"
"  -i, --identifier IDENT                use IDENT as the signing identifier\n"
"  -f, --force                           force signing (replace existing)\n"
"      --verify                          verify signature\n"
"  -d, --display                         display signature info\n"
"      --remove-signature                remove signature\n"
"      --entitlements FILE               sign with entitlements from FILE\n"
"      --entitlements:-                  display entitlements as XML\n"
"      --entitlements:FILE               extract entitlements to FILE\n"
"      --requirements FILE               extract requirements to FILE\n"
"      --requirements:-                  extract requirements to stdout\n"
"      --extract-certificates PREFIX     extract signing certificates\n"
"      --timestamp                       request timestamp from authority\n"
"      --no-time-stamp                   do not request timestamp\n"
"      --preserve-metadata               preserve existing timestamps\n"
"      --deep                            recurse into nested code\n"
"      --strict                          strict validation\n"
"      --runtime                         enable hardened runtime\n"
"  -v, --verbose                         show verbose output\n"
"      --continue                        continue after errors\n"
"      --all-architectures               verify/disp all architectures\n"
"  -h, --help                            show help\n";

static int parse_log2(uint32_t val)
{
	if (val == 0)
		return 0;
	int log2 = 0;
	while ((1u << log2) < val)
		log2++;
	return log2;
}

static int
handle_long_opt(const char *name, const char *optarg,
    int *do_sign, int *do_verify, int *do_display, int *do_remove,
    int *force, int *deep, int *strict, int *runtime,
    int *timestamp, int *verbose, int *continue_on_error,
    int *all_archs, int *extract_certs, int *ent_xml_output,
    int *req_to_stdout,
    const char **identity,
    const char **entitlements_file,
    const char **entitlements_out,
    const char **requirements_out,
    const char **cert_prefix,
    uint32_t *cd_flags)
{
	if (strcmp(name, "sign") == 0) {
		*identity = optarg;
		*do_sign = 1;
	} else if (strcmp(name, "force") == 0) {
		*force = 1;
	} else if (strcmp(name, "verify") == 0) {
		*do_verify = 1;
	} else if (strcmp(name, "display") == 0) {
		*do_display = 1;
	} else if (strcmp(name, "remove-signature") == 0) {
		*do_remove = 1;
	} else if (strcmp(name, "entitlements") == 0) {
		if (optarg && strcmp(optarg, "-") == 0) {
			*ent_xml_output = 1;
		} else if (optarg) {
			*entitlements_file = optarg;
		}
	} else if (strcmp(name, "entitlements:-") == 0) {
		*ent_xml_output = 1;
	} else if (strcmp(name, "entitlements:") == 0) {
		if (optarg && strcmp(optarg, "-") == 0) {
			*ent_xml_output = 1;
		} else if (optarg) {
			*entitlements_file = optarg;
		}
	} else if (strcmp(name, "requirements") == 0 ||
	           strcmp(name, "requirements:") == 0) {
		if (optarg && strcmp(optarg, "-") == 0) {
			*req_to_stdout = 1;
		} else if (optarg) {
			*requirements_out = optarg;
		}
	} else if (strcmp(name, "requirements:-") == 0) {
		*req_to_stdout = 1;
	} else if (strcmp(name, "extract-certificates") == 0) {
		*cert_prefix = optarg;
		*extract_certs = 1;
	} else if (strcmp(name, "timestamp") == 0) {
		*timestamp = 1;
	} else if (strcmp(name, "no-time-stamp") == 0) {
		*timestamp = 0;
	} else if (strcmp(name, "preserve-metadata") == 0) {
		/* no-op */
	} else if (strcmp(name, "deep") == 0) {
		*deep = 1;
	} else if (strcmp(name, "strict") == 0) {
		*strict = 1;
	} else if (strcmp(name, "runtime") == 0) {
		*runtime = 1;
		*cd_flags |= CS_RUNTIME;
	} else if (strcmp(name, "verbose") == 0) {
		(*verbose)++;
	} else if (strcmp(name, "continue") == 0) {
		*continue_on_error = 1;
	} else if (strcmp(name, "all-architectures") == 0) {
		*all_archs = 1;
	} else if (strcmp(name, "help") == 0) {
		printf(usage, progname);
		exit(0);
	} else {
		return -1;
	}
	return 0;
}

int
main(int argc, char **argv)
{
	int do_sign = 0;
	int do_verify = 0;
	int do_display = 0;
	int do_remove = 0;
	int force = 0;
	int deep = 0;
	int strict = 0;
	int runtime = 0;
	int timestamp = 0;
	int verbose = 0;
	int continue_on_error = 0;
	int all_archs = 0;
	int extract_certs = 0;
	int ent_xml_output = 0;
	int req_to_stdout = 0;
	const char *identity = NULL;
	const char *identifier = NULL;
	const char *entitlements_file = NULL;
	const char *entitlements_out = NULL;
	const char *requirements_out = NULL;
	const char *cert_prefix = NULL;
	uint32_t cd_flags = 0;
	uint32_t page_size_log2 = 14;
	const char *cert_file = NULL;
	const char *key_file = NULL;
	const char *p12_file = NULL;
	const char *key_password = NULL;
	const char *req_str = NULL;

	static struct option opts[] = {
		{"sign",            required_argument, 0, 's'},
		{"identifier",      required_argument, 0, 'i'},
		{"force",           no_argument,       0, 'f'},
		{"verify",          no_argument,       0,  0},
		{"display",         no_argument,       0, 'd'},
		{"remove-signature", no_argument,      0,  0},
		{"entitlements",    required_argument, 0,  0},
		{"entitlements:-",  no_argument,       0,  0},
		{"requirements",    required_argument, 0,  0},
		{"requirements:-",  no_argument,       0,  0},
		{"extract-certificates", required_argument, 0, 0},
		{"preserve-metadata", no_argument,     0,  0},
		{"timestamp",       no_argument,       0,  0},
		{"no-time-stamp",   no_argument,       0,  0},
		{"deep",            no_argument,       0,  0},
		{"strict",          no_argument,       0,  0},
		{"runtime",         no_argument,       0,  0},
		{"verbose",         no_argument,       0, 'v'},
		{"continue",        no_argument,       0,  0},
		{"all-architectures", no_argument,     0,  0},
		{"help",            no_argument,       0, 'h'},
		{0, 0, 0, 0}
	};

	g_verbose = 0;
	g_continue_on_error = 0;

	int c;
	int opt_idx = 0;
	while ((c = getopt_long(argc, argv, "s:i:dvfh", opts, &opt_idx)) != -1) {
		if (c == 0) {
			if (handle_long_opt(opts[opt_idx].name, optarg,
			    &do_sign, &do_verify, &do_display, &do_remove,
			    &force, &deep, &strict, &runtime,
			    &timestamp, &verbose, &continue_on_error,
			    &all_archs, &extract_certs, &ent_xml_output,
			    &req_to_stdout, &identity,
			    &entitlements_file, &entitlements_out,
			    &requirements_out, &cert_prefix, &cd_flags) != 0) {
				fprintf(stderr, "%s: unknown option --%s\n",
				    progname, opts[opt_idx].name);
				return 1;
			}
		} else {
			switch (c) {
			case 's':
				identity = optarg;
				do_sign = 1;
				break;
			case 'i':
				identifier = optarg;
				break;
			case 'd':
				do_display = 1;
				break;
			case 'f':
				force = 1;
				break;
			case 'v':
				verbose++;
				break;
			case 'h':
				printf(usage, progname);
				return 0;
			default:
				fprintf(stderr, "%s: unknown option\n", progname);
				fprintf(stderr, usage, progname);
				return 1;
			}
		}
	}

	g_verbose = verbose;
	g_continue_on_error = continue_on_error;

	(void)deep;
	(void)strict;
	(void)req_str;
	(void)cert_file;
	(void)key_file;
	(void)p12_file;
	(void)key_password;
	(void)page_size_log2;
	(void)parse_log2;

	/* Handle identity */
	int adhoc = 0;
	if (identity) {
		if (strcmp(identity, "-") == 0 ||
		    strcmp(identity, "adhoc") == 0) {
			adhoc = 1;
		} else if (cs_file_exists(identity)) {
			/* Could be a .p12 file */
			p12_file = identity;
		} else {
			/*
			 * Anything else names an identity in the
			 * keychain, matched on the certificate's common
			 * name and used to sign in cs_keychain.c.  It is
			 * resolved here so a name that matches nothing
			 * fails before any file is touched.
			 */
			if (!keychain_identity_exists(identity)) {
				fprintf(stderr,
				    "%s: no identity found matching '%s'\n",
				    progname, identity);
				return 1;
			}
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "%s: no files specified\n", progname);
		fprintf(stderr, usage, progname);
		return 1;
	}

	int result = 0;
	for (int i = optind; i < argc; i++) {
		const char *path = argv[i];

		if (do_remove) {
			if (remove_signature(path) != 0) {
				if (!continue_on_error)
					return 1;
				result = 1;
			}
			continue;
		}

		if (do_verify) {
			if (verify_code(path, verbose) != 0) {
				if (!continue_on_error)
					return 1;
				result = 1;
			}
			continue;
		}

		if (do_display) {
			const char *ent_out = entitlements_out;
			if (!ent_out && ent_xml_output)
				ent_out = "-";
			const char *req_out = requirements_out;
			if (!req_out && req_to_stdout)
				req_out = "-";

			if (display_code(path, verbose,
			    ent_out, req_out,
			    extract_certs ? cert_prefix : NULL,
			    0, ent_xml_output, all_archs) != 0) {
				if (!continue_on_error)
					return 1;
				result = 1;
			}
			continue;
		}

		if (do_sign) {
			if (cs_is_directory(path)) {
				if (sign_bundle(path, identity, force, adhoc,
				    identifier, entitlements_file, cd_flags,
				    14, cert_file, key_file,
				    p12_file, key_password, req_str) != 0) {
					if (!continue_on_error)
						return 1;
					result = 1;
				}
			} else {
				if (sign_macho(path, identity, force, adhoc,
				    identifier, entitlements_file, cd_flags,
				    14, cert_file, key_file,
				    p12_file, key_password, req_str) != 0) {
					if (!continue_on_error)
						return 1;
					result = 1;
				}
			}
			continue;
		}

		fprintf(stderr, "%s: no operation specified\n", progname);
		return 1;
	}

	return result;
}
