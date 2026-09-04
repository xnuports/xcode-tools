/*
 * msgtracer_client.h -- MessageTracer, as far as it still exists.
 *
 * Apple's perl includes this.  Their fix/perlmain.c.ed inserts
 *
 *	#ifdef __APPLE__
 *	#include <msgtracer_client.h>
 *	#include <msgtracer_keys.h>
 *
 * into perlmain.c and miniperlmain.c, so the build stops without it.
 *
 * It includes <asl.h>, and that is not a guess.  Apple's patch adds
 * only these two headers plus libproc and sys/proc*, and the code it
 * adds calls asl_new(ASL_TYPE_MSG) and asl_set() -- so asl.h has to
 * arrive through one of the two, and the keys header is a list of
 * string constants.  MessageTracer was a thin layer over ASL, which
 * makes this the natural place for it in Apple's copy too.
 *
 * Beyond that it declares nothing, and that is deliberate.  MessageTracer is gone:
 * msgtracer_domain_new and msgtracer_log_with_keys are absent from
 * libSystem on this system -- dlsym finds neither -- and the generated
 * stubs carry no such symbol either.  Apple's perl does not call them;
 * the includes are all that survives of a code path removed some
 * releases ago, and both files mention msgtracer exactly twice, on
 * these two lines.
 *
 * Declaring the old API here would compile and then fail to link,
 * which is a worse error further from its cause.  Anything that really
 * calls msgtracer should stop at the call, with the function
 * undeclared, and be rewritten against os_log.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __MSGTRACER_CLIENT_H__
#define __MSGTRACER_CLIENT_H__

#include <asl.h>

#endif /* __MSGTRACER_CLIENT_H__ */
