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

#define DEVICECTL_VERSION "0.1.0"

/* Global options */
extern int opt_verbose;
extern int opt_quiet;
extern int opt_timeout;
extern const char *opt_json_output;
extern const char *opt_log_output;
