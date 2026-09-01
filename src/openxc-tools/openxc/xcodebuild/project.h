/* xcodebuild -- open source reimplementation of Apple's xcodebuild utility
 *
 * Project / workspace introspection: locating project.pbxproj, listing
 * targets, configurations and schemes, scanning available SDKs and
 * toolchains, and extracting a target's XCBuildConfiguration.buildSettings.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  3. Neither the name of the copyright holder nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __PROJECT_H__
#define __PROJECT_H__

#include <CoreFoundation/CoreFoundation.h>

#include "xcodebuild.h"
#include "plist.h"

/* Resolve <path>/project.pbxproj for a .xcodeproj / path. Returns a malloc'd
 * absolute path (caller frees), or NULL if not found. */
char *project_pbxproj_path(const char *project);

/* Load and parse a project.pbxproj into a plist tree. Caller releases with
 * plist_free(). Returns NULL on failure. */
CFTypeRef project_load_pbxproj(const char *project);

/* Resolve a target/config's buildSettings node (borrowed from `root`). */
CFTypeRef project_find_buildsettings(CFTypeRef root, const char *target,
                                     const char *configuration,
                                     char *chosen_name, size_t chosen_len);

/* A project's display name: its bundle's, minus the extension. */
void project_display_name(const char *path, char *buf, size_t len);

/* Return the root PBXProject object node for a parsed project (the object
 * referenced by rootObject). Returns NULL if absent. Borrowed from `root`. */
CFTypeRef project_get_project_object(CFTypeRef root);

/* Print the "xcodebuild -list" summary for the given project/workspace. */
int project_list(const char *project, const char *workspace, const xcodebuild_opts *opts);

/* Print available SDKs and toolchains from the developer directory. */
void project_show_sdks(const char *devpath);
void project_show_toolchains(const char *devpath);

#endif /* __PROJECT_H__ */
