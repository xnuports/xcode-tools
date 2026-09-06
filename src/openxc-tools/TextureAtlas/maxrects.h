/*
 * maxrects.h -- the MaxRects bin packer, as TextureAtlas uses it.
 *
 * Jukka Jylanki's algorithm: the bin is described by a set of maximal free
 * rectangles, which may overlap.  Placing a rectangle splits every free
 * rectangle it intersects into the (up to four) maximal rectangles that
 * survive around it, after which any free rectangle wholly inside another
 * is dropped.  Only the best-long-side-fit choice is implemented; that is
 * the one Apple's TextureAtlas asks for, and the others differ only in the
 * score they hand back.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TEXTUREATLAS_MAXRECTS_H
#define TEXTUREATLAS_MAXRECTS_H

#include <stdbool.h>
#include <stddef.h>

struct mr_rect {
	int	x, y, w, h;
};

struct maxrects {
	int		 bin_w, bin_h;
	struct mr_rect	*free;
	size_t		 nfree, afree;
	struct mr_rect	*used;
	size_t		 nused, aused;
};

void	mr_init(struct maxrects *, int w, int h);
void	mr_free(struct maxrects *);

/*
 * Place a w-by-h rectangle.  Returns false when it does not fit, in which
 * case the packer is left untouched.  *rotated says whether the placement
 * turned the rectangle a quarter turn, in which case out->w and out->h are
 * the swapped extent it occupies.
 */
bool	mr_insert(struct maxrects *, int w, int h,
	    struct mr_rect *out, bool *rotated);

/* The extent actually covered, which is what the atlas gets cropped to. */
void	mr_trimmed_size(const struct maxrects *, int *w, int *h);

#endif /* TEXTUREATLAS_MAXRECTS_H */
