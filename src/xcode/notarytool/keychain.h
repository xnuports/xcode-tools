/*
 * notarytool - open source reimplementation of Apple's notarytool(1).
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef KEYCHAIN_H
#define KEYCHAIN_H

/*
 * Store notarytool credentials in the macOS keychain.
 * The credentials are stored as a generic password item with:
 *   service: "com.apple.notarytool.credentials"
 *   account: <profile-name>
 *   attributes: team-id, apple-id, key-id, issuer-id, etc.
 */

/*
 * Store credentials for a profile name in the keychain.
 * Returns 0 on success, -1 on failure.
 */
int keychain_store(const char *profile_name, const char *key_path,
    const char *key_id, const char *issuer_id, const char *team_id,
    const char *apple_id, const char *app_specific_password,
    const char *keychain_path);

/*
 * Retrieve credentials for a profile name from the keychain.
 * Fills in the provided fields (caller must free non-NULL results).
 * Returns 0 on success, -1 on failure.
 */
int keychain_retrieve(const char *profile_name, const char *keychain_path,
    char **key_path, char **key_id, char **issuer_id, char **team_id,
    char **apple_id, char **app_specific_password);

#endif /* KEYCHAIN_H */