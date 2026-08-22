/*
 * xctrace - open source reimplementation of Apple's xctrace(1).
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Provides command-line access to trace recording, export, and capability
 * listing. Recording is delegated to Instruments.app (launched via the
 * system open(1) command), which provides the full analysis engine.
 * List commands (devices, templates, instruments) are implemented
 * natively by scanning the CoreSimulator and Instruments.app bundles.
 */

#define XCTRACE_VERSION "0.1.0"

/* Subcommand entry points */
int xc_record(int argc, char **argv, int optind, int quiet);
int xc_export(int argc, char **argv, int optind, int quiet);
