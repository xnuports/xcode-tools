/*
 * maxrects.c -- the MaxRects bin packer, as TextureAtlas uses it.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "maxrects.h"

static void
push(struct mr_rect **v, size_t *n, size_t *a, struct mr_rect r)
{
	if (*n == *a) {
		size_t na = *a ? *a * 2 : 16;
		struct mr_rect *nv = realloc(*v, na * sizeof(*nv));

		if (nv == NULL)
			err(1, NULL);
		*v = nv;
		*a = na;
	}
	(*v)[(*n)++] = r;
}

void
mr_init(struct maxrects *p, int w, int h)
{
	memset(p, 0, sizeof(*p));
	p->bin_w = w;
	p->bin_h = h;
	push(&p->free, &p->nfree, &p->afree,
	    (struct mr_rect){ 0, 0, w, h });
}

void
mr_free(struct maxrects *p)
{
	free(p->free);
	free(p->used);
	memset(p, 0, sizeof(*p));
}

static bool
contained(const struct mr_rect *a, const struct mr_rect *b)
{
	return (a->x >= b->x && a->y >= b->y &&
	    a->x + a->w <= b->x + b->w && a->y + a->h <= b->y + b->h);
}

/*
 * Cut the used rectangle out of one free rectangle, appending the maximal
 * rectangles that remain around it.  Returns false, leaving the free
 * rectangle alone, when the two do not overlap.
 */
static bool
split(struct maxrects *p, struct mr_rect f, const struct mr_rect *u)
{
	if (u->x >= f.x + f.w || u->x + u->w <= f.x ||
	    u->y >= f.y + f.h || u->y + u->h <= f.y)
		return (false);

	if (u->x < f.x + f.w && u->x + u->w > f.x) {
		if (u->y > f.y && u->y < f.y + f.h)
			push(&p->free, &p->nfree, &p->afree,
			    (struct mr_rect){ f.x, f.y, f.w, u->y - f.y });
		if (u->y + u->h < f.y + f.h)
			push(&p->free, &p->nfree, &p->afree,
			    (struct mr_rect){ f.x, u->y + u->h, f.w,
			    f.y + f.h - (u->y + u->h) });
	}
	if (u->y < f.y + f.h && u->y + u->h > f.y) {
		if (u->x > f.x && u->x < f.x + f.w)
			push(&p->free, &p->nfree, &p->afree,
			    (struct mr_rect){ f.x, f.y, u->x - f.x, f.h });
		if (u->x + u->w < f.x + f.w)
			push(&p->free, &p->nfree, &p->afree,
			    (struct mr_rect){ u->x + u->w, f.y,
			    f.x + f.w - (u->x + u->w), f.h });
	}
	return (true);
}

static void
prune(struct maxrects *p)
{
	size_t i, j;

	for (i = 0; i < p->nfree; i++) {
		for (j = i + 1; j < p->nfree; j++) {
			if (contained(&p->free[i], &p->free[j])) {
				memmove(&p->free[i], &p->free[i + 1],
				    (p->nfree - i - 1) * sizeof(p->free[0]));
				p->nfree--;
				i--;
				break;
			}
			if (contained(&p->free[j], &p->free[i])) {
				memmove(&p->free[j], &p->free[j + 1],
				    (p->nfree - j - 1) * sizeof(p->free[0]));
				p->nfree--;
				j--;
			}
		}
	}
}

bool
mr_insert(struct maxrects *p, int w, int h, struct mr_rect *out, bool *rotated)
{
	struct mr_rect	best = { 0, 0, 0, 0 }, used;
	long		best_long = 0, best_short = 0;
	bool		best_rot = false, found = false;
	size_t		i;

	for (i = 0; i < p->nfree; i++) {
		const struct mr_rect *f = &p->free[i];
		int try[2][2] = { { w, h }, { h, w } };
		int t;

		for (t = 0; t < 2; t++) {
			int rw = try[t][0], rh = try[t][1];
			long lx, ly, sl, ll;

			if (rw > f->w || rh > f->h)
				continue;
			lx = f->w - rw;
			ly = f->h - rh;
			sl = lx < ly ? lx : ly;
			ll = lx < ly ? ly : lx;
			/*
			 * Best long side fit, with the short side breaking
			 * ties.  Strictly better wins, so among equals the
			 * earliest free rectangle -- and the unrotated
			 * orientation -- is kept.
			 */
			if (found && !(ll < best_long ||
			    (ll == best_long && sl < best_short)))
				continue;
			best = (struct mr_rect){ f->x, f->y, rw, rh };
			best_long = ll;
			best_short = sl;
			best_rot = (t == 1);
			found = true;
		}
	}
	if (!found)
		return (false);

	used = best;
	for (i = 0; i < p->nfree; i++) {
		struct mr_rect f = p->free[i];

		if (!split(p, f, &used))
			continue;
		memmove(&p->free[i], &p->free[i + 1],
		    (p->nfree - i - 1) * sizeof(p->free[0]));
		p->nfree--;
		i--;
	}
	prune(p);
	push(&p->used, &p->nused, &p->aused, used);

	*out = used;
	*rotated = best_rot;
	return (true);
}

void
mr_trimmed_size(const struct maxrects *p, int *w, int *h)
{
	int mw = 0, mh = 0;
	size_t i;

	for (i = 0; i < p->nused; i++) {
		if (p->used[i].x + p->used[i].w > mw)
			mw = p->used[i].x + p->used[i].w;
		if (p->used[i].y + p->used[i].h > mh)
			mh = p->used[i].y + p->used[i].h;
	}
	*w = mw;
	*h = mh;
}
