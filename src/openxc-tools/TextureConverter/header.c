/*
 * header.c -- writing the levels as a C header.
 *
 * The fifth output format, and the only one that is not a container: an
 * output path ending in .h gets a header that declares the texture's shape
 * as constants, the levels as byte arrays, and a function that hands the
 * whole thing to AppleTextureConverter's runtime.  Twenty bytes to a line,
 * every line ending in a comma and a space, which is Apple's layout and not
 * a formatting choice.
 *
 * The levels are packed tight here, as version 2 packs them: a 1x1 level of
 * R8 is one byte, where version 1 would pad it to four.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "header.h"
#include "formats.h"

struct text {
	char	*p;
	size_t	 len, cap;
	int	 failed;
};

static void
addf(struct text *t, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

static void
addf(struct text *t, const char *fmt, ...)
{
	va_list ap;
	int n;

	if (t->failed)
		return;
	for (;;) {
		size_t room = t->cap - t->len;

		va_start(ap, fmt);
		n = vsnprintf(t->p + t->len, room, fmt, ap);
		va_end(ap);
		if (n < 0) {
			t->failed = 1;
			return;
		}
		if ((size_t)n < room) {
			t->len += (size_t)n;
			return;
		}
		{
			size_t want = (t->len + (size_t)n + 1) * 2;
			char *np = realloc(t->p, want);

			if (np == NULL) {
				t->failed = 1;
				return;
			}
			t->p = np;
			t->cap = want;
		}
	}
}

char *
header_write(void **levels, const size_t *sizes, const int *widths,
    const int *heights, int nlevels, const char *name,
    const char *atc_format, const char *gamut, const char *ident,
    _Bool srgb, _Bool normal, int faces, size_t *out_len)
{
	struct text t = { NULL, 0, 0, 0 };
	int i;

	if (nlevels <= 0 || faces < 1)
		return (NULL);
	if ((t.p = malloc(t.cap = 4096)) == NULL)
		return (NULL);
	t.p[0] = '\0';

	addf(&t, "// %s.h\n\n", ident);
	addf(&t, "#include <stdint.h>\n");
	addf(&t, "#include \"AppleTextureConverter.h\"\n\n");
	addf(&t, "const uint32_t %s_type = atcTextureType%s;\n", ident,
	    faces > 1 ? "Cube" : "2D");
	addf(&t, "const uint32_t %s_format = %s;\n", ident, atc_format);
	addf(&t, "const uint32_t %s_colorGamut = %s;\n", ident, gamut);
	addf(&t, "const uint32_t %s_width = %d;\n", ident, widths[0]);
	addf(&t, "const uint32_t %s_height = %d;\n", ident, heights[0]);
	addf(&t, "const uint32_t %s_depth = 1;\n", ident);
	addf(&t, "const uint32_t %s_numMipmaps = %d;\n", ident, nlevels);
	addf(&t, "const uint32_t %s_numElements = 1;\n", ident);
	addf(&t, "const uint32_t %s_numChannels = %d;\n\n", ident,
	    format_atc_channels(name, srgb));

	for (i = 0; i < nlevels * faces; i++) {
		const uint8_t *b = levels[i];
		size_t j;

		/*
		 * The tab that opens a line is written whenever the count
		 * reaches a multiple of twenty, the end of the level
		 * included, so a level whose size is a multiple of twenty
		 * ends with a line holding nothing but the tab.  Apple do
		 * that and a reader would never notice, but a byte compare
		 * does.
		 */
		if (faces > 1)
			addf(&t, "uint8_t %s_Mip%dFace%d[%zu] = { \n", ident,
			    i / faces, i % faces, sizes[i / faces]);
		else
			addf(&t, "uint8_t %s_Mip%d[%zu] = { \n", ident, i,
			    sizes[i]);
		for (j = 0; j <= sizes[i / faces]; j++) {
			if (j % 20 == 0) {
				if (j != 0)
					addf(&t, "\n");
				addf(&t, "\t");
			}
			if (j < sizes[i / faces])
				addf(&t, "0x%02x, ", b[j]);
		}
		addf(&t, "\n};\n\n");
	}

	addf(&t, "static void SetSurface%s(const ATC_Texture* pTexture, "
	    "uint32_t element, uint32_t level, uint32_t width, "
	    "uint32_t height, uint32_t size, uint8_t* data)\n{\n", ident);
	addf(&t, "\tATC_Surface* surface = NULL;\n");
	addf(&t, "\tATC_GetSurface( NULL, pTexture, element, level, "
	    "&surface );\n");
	addf(&t, "\tsurface->width = width;\n");
	addf(&t, "\tsurface->height = height;\n");
	addf(&t, "\tsurface->rowBytes = 0;\n");
	addf(&t, "\tsurface->data = data;\n");
	addf(&t, "\tsurface->size = size;\n}\n\n");

	addf(&t, "const ATC_Texture* Get%s()\n{\n", ident);
	addf(&t, "\tconst ATC_Texture* pTexture = NULL;\n");
	/*
	 * The last argument is whether the texture is a normal map, which
	 * is also what takes the colour gamut to None: a direction has no
	 * gamut, and --gamut_out is ignored there.  A cubemap is made by a
	 * call of its own, which takes one side rather than two, and its
	 * surfaces are addressed by face where a plain texture's element
	 * is always zero.
	 */
	if (faces > 1)
		addf(&t, "\tATC_CreateTextureCube( NULL, %d, %d, %s, %s, "
		    "%s, &pTexture );\n", widths[0], nlevels, atc_format,
		    gamut, normal ? "true" : "false");
	else
		addf(&t, "\tATC_CreateTexture2D( NULL, %d, %d, %d, %s, %s, "
		    "%s, &pTexture );\n", widths[0], heights[0], nlevels,
		    atc_format, gamut, normal ? "true" : "false");
	for (i = 0; i < nlevels * faces; i++) {
		int lvl = i / faces;

		if (faces > 1)
			addf(&t, "%s\tSetSurface%s(pTexture, %d, %d, %d, "
			    "%d, %zu, %s_Mip%dFace%d);\n",
			    i % faces == 0 ? "\n" : "", ident, i % faces,
			    lvl, widths[lvl], heights[lvl], sizes[lvl],
			    ident, lvl, i % faces);
		else
			addf(&t, "\n\tSetSurface%s(pTexture, 0, %d, %d, "
			    "%d, %zu, %s_Mip%d);\n", ident, lvl, widths[lvl],
			    heights[lvl], sizes[lvl], ident, lvl);
	}
	addf(&t, "\n\treturn pTexture;\n}\n");

	if (t.failed) {
		free(t.p);
		return (NULL);
	}
	*out_len = t.len;
	return (t.p);
}
