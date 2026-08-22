/*
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef BOM_H
#define BOM_H

#include <stddef.h>

/*
 * Generate a BOM (Bill Of Materials) for a source root using mkbom(1).
 * Returns a malloc'd buffer (caller frees) or NULL on failure.
 */
unsigned char *bom_build(const char *root, size_t *out_len);

#endif /* BOM_H */
