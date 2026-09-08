/*
 * dds.c -- writing a DirectDraw surface.
 *
 * Apple's DDS is NVTT's, signature and all: the eleven reserved words carry
 * "NVTT" and its version, 2.1.2.  Everything in the header but the size,
 * the level count and the format is the same in every file their tool
 * writes, so it is written the same way here.
 *
 * The format is always named through the DX10 extension header rather than
 * a FourCC, so a format Direct3D has no DXGI enumerant for cannot be
 * written at all -- see format_dxgi_for.
 *
 * The levels run largest first and are packed tight, with no padding of
 * either the rows or the levels.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <string.h>

#include "dds.h"

static void
put32(uint8_t **p, uint32_t v)
{
	(*p)[0] = (uint8_t)v;
	(*p)[1] = (uint8_t)(v >> 8);
	(*p)[2] = (uint8_t)(v >> 16);
	(*p)[3] = (uint8_t)(v >> 24);
	*p += 4;
}

uint8_t *
dds_write(void **levels, const size_t *sizes, int width, int height,
    int nlevels, uint32_t dxgi_format, size_t *out_len)
{
	enum { HEADER = 4 + 124 + 20 };
	size_t total = HEADER;
	uint8_t *out, *p;
	int i;

	if (nlevels <= 0 || dxgi_format == 0)
		return (NULL);
	for (i = 0; i < nlevels; i++)
		total += sizes[i];
	if ((out = calloc(1, total)) == NULL)
		return (NULL);
	p = out;

	memcpy(p, "DDS ", 4);
	p += 4;
	put32(&p, 124);				/* dwSize */
	/*
	 * Caps, height, width, pixel format and depth are always set; the
	 * mipmap count bit only when there is a chain.
	 */
	put32(&p, nlevels > 1 ? 0x00821007 : 0x00801007);
	put32(&p, (uint32_t)height);
	put32(&p, (uint32_t)width);
	put32(&p, 0);				/* dwPitchOrLinearSize */
	put32(&p, 1);				/* dwDepth */
	put32(&p, (uint32_t)nlevels);		/* dwMipMapCount */
	for (i = 0; i < 11; i++)		/* dwReserved1 */
		put32(&p, i == 9 ? 0x5454564e :	/* "NVTT" */
		    i == 10 ? 0x00020102 : 0);	/* its version, 2.1.2 */

	put32(&p, 32);				/* pixel format size */
	put32(&p, 4);				/* DDPF_FOURCC */
	memcpy(p, "DX10", 4);
	p += 4;
	for (i = 0; i < 5; i++)			/* bit count, four masks */
		put32(&p, 0);
	put32(&p, 0x00401008);			/* complex, texture, mipmap */
	for (i = 0; i < 4; i++)			/* dwCaps2..4, dwReserved2 */
		put32(&p, 0);

	put32(&p, dxgi_format);
	put32(&p, 3);				/* D3D10_RESOURCE_2DTEXTURE */
	put32(&p, 0);				/* miscFlag */
	put32(&p, 1);				/* arraySize */
	put32(&p, 0);				/* miscFlags2 */

	for (i = 0; i < nlevels; i++) {
		memcpy(p, levels[i], sizes[i]);
		p += sizes[i];
	}
	*out_len = total;
	return (out);
}

static uint32_t
get32(const uint8_t *p)
{
	return ((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	    ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

/*
 * Reading one back.  Only the shape the writer above produces is accepted:
 * a DX10 header, no cubemap, no volume.  The levels behind it are packed
 * tight, largest first, so the caller walks them from the format.
 */
_Bool
dds_parse(const void *bytes, size_t len, uint32_t *dxgi, int *width,
    int *height, int *nlevels, const uint8_t **data, size_t *data_len)
{
	const uint8_t *p = bytes;
	enum { HEADER = 4 + 124 + 20 };
	uint32_t mips;

	if (len < HEADER || memcmp(p, "DDS ", 4) != 0 || get32(p + 4) != 124)
		return (0);
	if (memcmp(p + 84, "DX10", 4) != 0)
		return (0);
	if (get32(p + 128 + 4) != 3)		/* 2D textures only */
		return (0);
	mips = get32(p + 28);
	*dxgi = get32(p + 128);
	*height = (int)get32(p + 12);
	*width = (int)get32(p + 16);
	*nlevels = mips == 0 ? 1 : (int)mips;
	*data = p + HEADER;
	*data_len = len - HEADER;
	return (1);
}
