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

#define SIMCTL_VERSION "0.1.0"

/* CoreSimulator data paths */
#define CS_DEVICES_DIR    "Library/Developer/CoreSimulator/Devices"
#define CS_PROFILES_DIR   "/Library/Developer/CoreSimulator/Profiles"
#define CS_VOLUMES_DIR    "/Library/Developer/CoreSimulator/Volumes"

/* State values from CoreSimulator device.plist (SimDeviceState) */
#define CS_STATE_UNKNOWN      0
#define CS_STATE_SHUTDOWN     1
#define CS_STATE_BOOTED       2
#define CS_STATE_CREATING     3
#define CS_STATE_DELETING     4

extern const char *state_names[];

/* Dispatch functions (implemented in simctl.c / sim_list.c / sim_ops.c) */
extern void list_dispatch(int argc, char **argv);
extern int  sim_create(int argc, char **argv);
extern int  sim_boot_or_shutdown(const char *cmd, int argc, char **argv);
extern int  sim_delete_erase(const char *cmd, int argc, char **argv);
extern int  sim_install_uninstall(const char *cmd, int argc, char **argv);
extern int  sim_spawn(int argc, char **argv);
extern int  sim_io(int argc, char **argv);
extern int  sim_location(int argc, char **argv);
extern int  sim_push(int argc, char **argv);
extern int  sim_rename(int argc, char **argv);
extern int  sim_clone(int argc, char **argv);
extern int  sim_openurl(int argc, char **argv);

/* List functions (implemented in sim_list.c / sim_list_dispatch.c) */
extern void list_devices(int json_mode, int verbose);
extern void list_devicetypes(int json_mode);
extern void list_runtimes(int json_mode);
extern void list_pairs(int json_mode);
