/*
 * notarytool - open source reimplementation of Apple's notarytool(1).
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef JWT_H
#define JWT_H

#include <stddef.h>

/*
 * Build an ES256 (P-256 + SHA-256) JWT for App Store Connect API
 * authentication. The private key is read from key_path (PKCS#8 PEM).
 * Returns a malloc'd NUL-terminated JWT string, or NULL on failure.
 */
char *jwt_build(const char *key_path, const char *key_id,
    const char *issuer_id, size_t *out_len);

#endif /* JWT_H */