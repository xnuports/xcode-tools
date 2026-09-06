/*
 * introsort.c -- libc++'s std::sort, in C.
 *
 * A transcription of libcxx/include/__algorithm/sort.h; the names below are
 * the ones it uses, minus the underscores.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "introsort.h"

#define LIMIT		24	/* below this, insertion sort */
#define NINTHER		128	/* above this, a ninther picks the pivot */

static inline void
swapp(void **a, void **b)
{
	void *t = *a;

	*a = *b;
	*b = t;
}

static bool
sort3(void **x, void **y, void **z, introsort_cmp c)
{
	if (!c(*y, *x)) {		/* x <= y */
		if (!c(*z, *y))		/* y <= z */
			return (false);
		swapp(y, z);
		if (c(*y, *x))
			swapp(x, y);
		return (true);
	}
	if (c(*z, *y)) {		/* x > y > z */
		swapp(x, z);
		return (true);
	}
	swapp(x, y);
	if (c(*z, *y))
		swapp(y, z);
	return (true);
}

static void
sort4(void **x1, void **x2, void **x3, void **x4, introsort_cmp c)
{
	sort3(x1, x2, x3, c);
	if (c(*x4, *x3)) {
		swapp(x3, x4);
		if (c(*x3, *x2)) {
			swapp(x2, x3);
			if (c(*x2, *x1))
				swapp(x1, x2);
		}
	}
}

static void
sort5(void **x1, void **x2, void **x3, void **x4, void **x5, introsort_cmp c)
{
	sort4(x1, x2, x3, x4, c);
	if (c(*x5, *x4)) {
		swapp(x4, x5);
		if (c(*x4, *x3)) {
			swapp(x3, x4);
			if (c(*x3, *x2)) {
				swapp(x2, x3);
				if (c(*x2, *x1))
					swapp(x1, x2);
			}
		}
	}
}

static void
insertion_sort(void **first, void **last, introsort_cmp c)
{
	void **i, **j, **k;
	void *t;

	if (first == last)
		return;
	for (i = first + 1; i != last; i++) {
		j = i - 1;
		if (!c(*i, *j))
			continue;
		t = *i;
		k = j;
		j = i;
		do {
			*j = *k;
			j = k;
		} while (j != first && c(t, *--k));
		*j = t;
	}
}

/*
 * The same, for a range that is known to have a smaller element in front of
 * it, so the inner loop needs no bound.
 */
static void
insertion_sort_unguarded(void **first, void **last, introsort_cmp c)
{
	void **i, **j, **k;
	void *t;

	if (first == last)
		return;
	for (i = first + 1; i != last; i++) {
		j = i - 1;
		if (!c(*i, *j))
			continue;
		t = *i;
		k = j;
		j = i;
		do {
			*j = *k;
			j = k;
		} while (c(t, *--k));
		*j = t;
	}
}

/* Gives up after eight moves, saying whether it finished. */
static bool
insertion_sort_incomplete(void **first, void **last, introsort_cmp c)
{
	void **i, **j, **k;
	void *t;
	unsigned count = 0;

	switch (last - first) {
	case 0:
	case 1:
		return (true);
	case 2:
		if (c(*(last - 1), *first))
			swapp(first, last - 1);
		return (true);
	case 3:
		sort3(first, first + 1, last - 1, c);
		return (true);
	case 4:
		sort4(first, first + 1, first + 2, last - 1, c);
		return (true);
	case 5:
		sort5(first, first + 1, first + 2, first + 3, last - 1, c);
		return (true);
	}
	j = first + 2;
	sort3(first, first + 1, j, c);
	for (i = j + 1; i != last; i++) {
		if (c(*i, *j)) {
			t = *i;
			k = j;
			j = i;
			do {
				*j = *k;
				j = k;
			} while (j != first && c(t, *--k));
			*j = t;
			if (++count == 8)
				return (++i == last);
		}
		j = i;
	}
	return (true);
}

/*
 * Partition around *first, keeping elements equivalent to the pivot on its
 * right.  Reports where the pivot ended up and whether nothing had to move.
 */
static void **
partition_equals_right(void **first, void **last, introsort_cmp c,
    bool *already)
{
	void **begin = first, **pivot_pos;
	void *pivot = *first;

	do {
		first++;
	} while (c(*first, pivot));

	if (begin == first - 1) {
		while (first < last && !c(*--last, pivot))
			;
	} else {
		do {
			last--;
		} while (!c(*last, pivot));
	}

	*already = first >= last;
	while (first < last) {
		swapp(first, last);
		do {
			first++;
		} while (c(*first, pivot));
		do {
			last--;
		} while (!c(*last, pivot));
	}
	pivot_pos = first - 1;
	if (begin != pivot_pos)
		*begin = *pivot_pos;
	*pivot_pos = pivot;
	return (pivot_pos);
}

/* The mirror of the above, keeping the equal elements on the left. */
static void **
partition_equals_left(void **first, void **last, introsort_cmp c)
{
	void **begin = first, **pivot_pos;
	void *pivot = *first;

	if (c(pivot, *(last - 1))) {
		do {
			first++;
		} while (!c(pivot, *first));
	} else {
		while (++first < last && !c(pivot, *first))
			;
	}
	if (first < last) {
		do {
			last--;
		} while (c(pivot, *last));
	}
	while (first < last) {
		swapp(first, last);
		do {
			first++;
		} while (!c(pivot, *first));
		do {
			last--;
		} while (c(pivot, *last));
	}
	pivot_pos = first - 1;
	if (begin != pivot_pos)
		*begin = *pivot_pos;
	*pivot_pos = pivot;
	return (first);
}

static void
sift_down(void **first, size_t n, size_t start, introsort_cmp c)
{
	size_t child = start;

	for (;;) {
		size_t l = 2 * child + 1;

		if (l >= n)
			return;
		if (l + 1 < n && c(first[l], first[l + 1]))
			l++;
		if (!c(first[child], first[l]))
			return;
		swapp(&first[child], &first[l]);
		child = l;
	}
}

/*
 * The depth-limit escape hatch.  Introsort only reaches it after 2*log2(n)
 * unlucky partitions, which needs input built to provoke it; a plain heap
 * sort is enough to keep the worst case bounded.
 *
 * ponytail: not libc++'s heap sort, so a range that got here could order
 * equal elements differently.  Match it if that ever shows up in a diff.
 */
static void
heap_sort(void **first, void **last, introsort_cmp c)
{
	size_t n = (size_t)(last - first), i;

	if (n < 2)
		return;
	for (i = n / 2; i-- > 0;)
		sift_down(first, n, i, c);
	for (i = n; i-- > 1;) {
		swapp(&first[0], &first[i]);
		sift_down(first, i, 0, c);
	}
}

static void
introsort_range(void **first, void **last, introsort_cmp c, long depth,
    bool leftmost)
{
	for (;;) {
		long len = (long)(last - first);
		long half;
		void **i;
		bool already;

		switch (len) {
		case 0:
		case 1:
			return;
		case 2:
			if (c(*--last, *first))
				swapp(first, last);
			return;
		case 3:
			sort3(first, first + 1, --last, c);
			return;
		case 4:
			sort4(first, first + 1, first + 2, --last, c);
			return;
		case 5:
			sort5(first, first + 1, first + 2, first + 3, --last,
			    c);
			return;
		}
		if (len < LIMIT) {
			if (leftmost)
				insertion_sort(first, last, c);
			else
				insertion_sort_unguarded(first, last, c);
			return;
		}
		if (depth == 0) {
			heap_sort(first, last, c);
			return;
		}
		depth--;

		half = len / 2;
		if (len > NINTHER) {
			sort3(first, first + half, last - 1, c);
			sort3(first + 1, first + (half - 1), last - 2, c);
			sort3(first + 2, first + (half + 1), last - 3, c);
			sort3(first + (half - 1), first + half,
			    first + (half + 1), c);
			swapp(first, first + half);
		} else
			sort3(first + half, first, last - 1, c);

		/*
		 * When this range is not the leftmost and its pivot matches
		 * the largest element to its left, everything up to the pivot
		 * is equal to it and only the right side needs sorting.
		 */
		if (!leftmost && !c(*(first - 1), *first)) {
			first = partition_equals_left(first, last, c);
			continue;
		}

		i = partition_equals_right(first, last, c, &already);
		if (already) {
			bool fs = insertion_sort_incomplete(first, i, c);

			if (insertion_sort_incomplete(i + 1, last, c)) {
				if (fs)
					return;
				last = i;
				continue;
			}
			if (fs) {
				first = i + 1;
				continue;
			}
		}
		introsort_range(first, i, c, depth, leftmost);
		leftmost = false;
		first = i + 1;
	}
}

void
introsort(void **first, size_t n, introsort_cmp c)
{
	long depth = 0;
	size_t v = n;

	if (n < 2)
		return;
	while (v >>= 1)		/* floor(log2(n)) */
		depth++;
	introsort_range(first, first + n, c, 2 * depth, true);
}
