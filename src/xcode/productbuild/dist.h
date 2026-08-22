/*
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef DIST_H
#define DIST_H

#include <stddef.h>

/*
 * Synthesize an installer-gui-script Distribution XML for a product
 * archive. This mirrors the XML that Apple's productbuild generates when
 * you don't pass --distribution.
 *
 * Returns a malloc'd string (caller frees); sets *out_len (excl. NUL).
 */
char *dist_synthesize(const char *identifier, const char *version,
    const char *pkg_name, const char *install_location,
    size_t install_kbytes, const char *host_arch, size_t *out_len);

#endif /* DIST_H */
