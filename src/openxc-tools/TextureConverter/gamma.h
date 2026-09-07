/*
 * gamma.h -- --gamma_in and --gamma_out.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TEXTURECONVERTER_GAMMA_H
#define TEXTURECONVERTER_GAMMA_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Raise the colour channels of a tightly packed RGBA float image to the
 * gamma, or to its reciprocal.  Alpha is left alone.
 */
void	image_gamma(float *rgba, int w, int h, float gamma, int to_linear);

#ifdef __cplusplus
}
#endif

#endif /* TEXTURECONVERTER_GAMMA_H */
