/*
 * introsort.h -- libc++'s std::sort, in C.
 *
 * TextureAtlas sorts its textures with std::sort, which is not stable: two
 * images of exactly equal area come out in an order that falls out of the
 * partitioning, and that order reaches the atlas.  Calling the host's
 * std::sort would only match a host whose libc++ is the same vintage, so the
 * algorithm is spelled out here instead: introsort with median-of-three (a
 * ninther past 128 elements), insertion sort under 24, and the equal
 * elements kept to the right of the pivot.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TEXTUREATLAS_INTROSORT_H
#define TEXTUREATLAS_INTROSORT_H

#include <stdbool.h>
#include <stddef.h>

/* True when a sorts before b, the sense std::sort's comparator uses. */
typedef bool (*introsort_cmp)(const void *a, const void *b);

void	introsort(void **first, size_t n, introsort_cmp);

#endif /* TEXTUREATLAS_INTROSORT_H */
