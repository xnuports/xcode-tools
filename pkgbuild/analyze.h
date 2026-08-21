/*
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef ANALYZE_H
#define ANALYZE_H

#include <stddef.h>

/*
 * Build the pkg-info Document for the package. Mirrors the XML emitted by
 * `pkgbuild`: a <payload> record with numberOfFiles + installKBytes plus
 * the standard empty bundle/upgrade/atomic/strict/relocate placeholders.
 * Returns a malloc'd NUL-terminated string; sets *out_len (excl. NUL).
 */
char *pkginfo_build(const char *identifier, const char *version,
    const char *install_location, int number_of_files,
    size_t install_kbytes, size_t *out_len);

#endif /* ANALYZE_H */
