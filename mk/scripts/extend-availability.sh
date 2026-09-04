#!/bin/sh
#
# extend-availability.sh -- widen the availability macros to the number
# of platforms current headers actually use.
#
# __API_AVAILABLE and its siblings are variadic over platforms, and each
# is implemented by dispatching on argument count to a fixed set of
# helpers.  The copy of AvailabilityInternal.h that ships with xnu stops
# at four platforms; Apple's own SDK carries a newer one.  Since Apple
# added visionos, bridgeos and driverkit, headers elsewhere in their
# open-source releases call these macros with seven -- libmalloc's
# _malloc_type.h does -- and against xnu's copy the dispatch selects
# nothing and the declaration collapses into a syntax error.
#
# The helpers are mechanical: __API_AVAILABLE3(x,y,z) is __API_A(x)
# __API_A(y) __API_A(z), and so on.  The missing arities are appended
# here, and the dispatchers widened to match, so a header calling them
# with seven platforms expands the way it does against Apple's SDK.
#
# The deprecation families need the same treatment and for a sharper
# reason.  Security's SecProtocolMetadata.h deprecates across macos,
# ios, watchos, tvos and macCatalyst at once, which is five, and
# __API_DEPRECATED_WITH_REPLACEMENT stopped at four.  Overflowing
# __API_UNAVAILABLE is worse still: its dispatcher picks the fourth
# argument as the macro name and calls it, so the annotation silently
# becomes something else rather than failing.
#
# The __SPI_* family is here too.  Apple's SDK defines five of them and
# expands each to nothing; only __SPI_AVAILABLE was defined here, so a
# header carrying __SPI_DEPRECATED -- spawn.h does, on
# posix_spawn_file_actions_addchdir_np -- would not parse.
#
# This appends to the installed copy rather than editing anything in
# the source tree: the SDK is generated, and this is part of generating
# it.
#
# Usage: extend-availability.sh <sdk-include-dir>
#
# Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

set -e
[ $# -eq 1 ] || { echo "usage: $0 <sdk-include-dir>" >&2; exit 1; }

# Appended to Availability.h, not AvailabilityInternal.h: the public
# header redefines __API_AVAILABLE after including the internal one, so
# an override placed in the internal header is replaced by the very
# definition it was meant to widen.
H="$1/Availability.h"
[ -f "$H" ] || exit 0
grep -q 'xnuports: widened' "$H" 2>/dev/null && exit 0

{
	echo ""
	echo "/* xnuports: the platforms Apple added after this header was"
	echo "   written.  __API_A pastes its argument onto"
	echo "   __API_AVAILABLE_PLATFORM_, and xnu's copy defines that for"
	echo "   macos, macosx, ios, watchos and tvos only -- so a header"
	echo "   naming bridgeos, visionos or driverkit pastes onto nothing"
	echo "   and the attribute falls apart mid-declaration. */"
	for p in bridgeos visionos driverkit maccatalyst xros; do
		echo "#ifndef __API_AVAILABLE_PLATFORM_${p}"
		echo "#define __API_AVAILABLE_PLATFORM_${p}(x) ${p},introduced=x"
		echo "#endif"
		echo "#ifndef __API_DEPRECATED_PLATFORM_${p}"
		echo "#define __API_DEPRECATED_PLATFORM_${p}(x,y) ${p},introduced=x,deprecated=y"
		echo "#endif"
		echo "#ifndef __API_UNAVAILABLE_PLATFORM_${p}"
		echo "#define __API_UNAVAILABLE_PLATFORM_${p} ${p},unavailable"
		echo "#endif"
	done
	echo ""
	echo "/* xnuports: widened to the platform count current headers use."
	echo "   See mk/scripts/extend-availability.sh. */"
	echo "#undef __API_AVAILABLE_GET_MACRO"
	echo "#define __API_AVAILABLE5(a,b,c,d,e) __API_A(a) __API_A(b) __API_A(c) __API_A(d) __API_A(e)"
	echo "#define __API_AVAILABLE6(a,b,c,d,e,f) __API_AVAILABLE5(a,b,c,d,e) __API_A(f)"
	echo "#define __API_AVAILABLE7(a,b,c,d,e,f,g) __API_AVAILABLE6(a,b,c,d,e,f) __API_A(g)"
	echo "#define __API_AVAILABLE8(a,b,c,d,e,f,g,h) __API_AVAILABLE7(a,b,c,d,e,f,g) __API_A(h)"
	echo "#define __API_AVAILABLE_GET_MACRO(_1,_2,_3,_4,_5,_6,_7,_8,NAME,...) NAME"
	echo "#undef __API_AVAILABLE"
	echo "#define __API_AVAILABLE(...) __API_AVAILABLE_GET_MACRO(__VA_ARGS__, \\"
	echo "        __API_AVAILABLE8, __API_AVAILABLE7, __API_AVAILABLE6, \\"
	echo "        __API_AVAILABLE5, __API_AVAILABLE4, __API_AVAILABLE3, \\"
	echo "        __API_AVAILABLE2, __API_AVAILABLE1)(__VA_ARGS__)"
	echo ""
	echo "/* xnuports: __SPI_AVAILABLE marks an interface Apple publishes"
	echo "   as SPI.  Their SDK defines it; xnu's headers use it and do"
	echo "   not, so a declaration carrying one -- gethostuuid() does --"
	echo "   parses as a call to an undeclared function and takes the"
	echo "   declaration with it.  It says the same thing as"
	echo "   __API_AVAILABLE about the platforms it names. */"
	echo "#ifndef __SPI_AVAILABLE"
	echo "#define __SPI_AVAILABLE(...) __API_AVAILABLE(__VA_ARGS__)"
	echo "#endif"
	echo ""
	echo "/* xnuports: the rest of the __SPI_* family, which Apple's SDK"
	echo "   defines and expands to nothing.  spawn.h carries a"
	echo "   __SPI_DEPRECATED on the addchdir_np calls and does not parse"
	echo "   without it, which stops any configure check that includes"
	echo "   <spawn.h> -- libarchive then builds posix_spawn code with no"
	echo "   declarations in scope. */"
	echo "#ifndef __SPI_DEPRECATED"
	echo "#define __SPI_DEPRECATED(...)"
	echo "#endif"
	echo "#ifndef __SPI_DEPRECATED_WITH_REPLACEMENT"
	echo "#define __SPI_DEPRECATED_WITH_REPLACEMENT(...)"
	echo "#endif"
	echo "#ifndef __SPI_AVAILABLE_BEGIN"
	echo "#define __SPI_AVAILABLE_BEGIN(...)"
	echo "#endif"
	echo "#ifndef __SPI_AVAILABLE_END"
	echo "#define __SPI_AVAILABLE_END"
	echo "#endif"
	echo ""
	echo "#undef __API_UNAVAILABLE_GET_MACRO"
	echo "#define __API_UNAVAILABLE4(a,b,c,d) __API_U(a) __API_U(b) __API_U(c) __API_U(d)"
	echo "#define __API_UNAVAILABLE5(a,b,c,d,e) __API_UNAVAILABLE4(a,b,c,d) __API_U(e)"
	echo "#define __API_UNAVAILABLE6(a,b,c,d,e,f) __API_UNAVAILABLE5(a,b,c,d,e) __API_U(f)"
	echo "#define __API_UNAVAILABLE7(a,b,c,d,e,f,g) __API_UNAVAILABLE6(a,b,c,d,e,f) __API_U(g)"
	echo "#define __API_UNAVAILABLE8(a,b,c,d,e,f,g,h) __API_UNAVAILABLE7(a,b,c,d,e,f,g) __API_U(h)"
	echo "#define __API_UNAVAILABLE_GET_MACRO(_1,_2,_3,_4,_5,_6,_7,_8,NAME,...) NAME"
	echo "#undef __API_UNAVAILABLE"
	echo "#define __API_UNAVAILABLE(...) __API_UNAVAILABLE_GET_MACRO(__VA_ARGS__, \\"
	echo "        __API_UNAVAILABLE8, __API_UNAVAILABLE7, __API_UNAVAILABLE6, \\"
	echo "        __API_UNAVAILABLE5, __API_UNAVAILABLE4, __API_UNAVAILABLE3, \\"
	echo "        __API_UNAVAILABLE2, __API_UNAVAILABLE1)(__VA_ARGS__)"

	echo ""
	echo "/* xnuports: the deprecations, widened the same way.  The first"
	echo "   argument is the message or replacement, so MSG6 is five"
	echo "   platforms. */"
	echo "#define __API_DEPRECATED_MSG6(m,a,b,c,d,e) __API_DEPRECATED_MSG5(m,a,b,c,d) __API_D(m,e)"
	echo "#define __API_DEPRECATED_MSG7(m,a,b,c,d,e,f) __API_DEPRECATED_MSG6(m,a,b,c,d,e) __API_D(m,f)"
	echo "#define __API_DEPRECATED_MSG8(m,a,b,c,d,e,f,g) __API_DEPRECATED_MSG7(m,a,b,c,d,e,f) __API_D(m,g)"
	echo "#define __API_DEPRECATED_MSG9(m,a,b,c,d,e,f,g,h) __API_DEPRECATED_MSG8(m,a,b,c,d,e,f,g) __API_D(m,h)"
	echo "#undef __API_DEPRECATED_MSG_GET_MACRO"
	echo "#define __API_DEPRECATED_MSG_GET_MACRO(_1,_2,_3,_4,_5,_6,_7,_8,_9,NAME,...) NAME"
	echo "#undef __API_DEPRECATED"
	echo "#define __API_DEPRECATED(...) __API_DEPRECATED_MSG_GET_MACRO(__VA_ARGS__, \\"
	echo "        __API_DEPRECATED_MSG9, __API_DEPRECATED_MSG8, \\"
	echo "        __API_DEPRECATED_MSG7, __API_DEPRECATED_MSG6, \\"
	echo "        __API_DEPRECATED_MSG5, __API_DEPRECATED_MSG4, \\"
	echo "        __API_DEPRECATED_MSG3, __API_DEPRECATED_MSG2)(__VA_ARGS__)"

	echo ""
	echo "#ifdef __API_R"
	echo "#define __API_DEPRECATED_REP6(r,a,b,c,d,e) __API_DEPRECATED_REP5(r,a,b,c,d) __API_R(r,e)"
	echo "#define __API_DEPRECATED_REP7(r,a,b,c,d,e,f) __API_DEPRECATED_REP6(r,a,b,c,d,e) __API_R(r,f)"
	echo "#define __API_DEPRECATED_REP8(r,a,b,c,d,e,f,g) __API_DEPRECATED_REP7(r,a,b,c,d,e,f) __API_R(r,g)"
	echo "#define __API_DEPRECATED_REP9(r,a,b,c,d,e,f,g,h) __API_DEPRECATED_REP8(r,a,b,c,d,e,f,g) __API_R(r,h)"
	echo "#undef __API_DEPRECATED_REP_GET_MACRO"
	echo "#define __API_DEPRECATED_REP_GET_MACRO(_1,_2,_3,_4,_5,_6,_7,_8,_9,NAME,...) NAME"
	echo "#undef __API_DEPRECATED_WITH_REPLACEMENT"
	echo "#define __API_DEPRECATED_WITH_REPLACEMENT(...) __API_DEPRECATED_REP_GET_MACRO(__VA_ARGS__, \\"
	echo "        __API_DEPRECATED_REP9, __API_DEPRECATED_REP8, \\"
	echo "        __API_DEPRECATED_REP7, __API_DEPRECATED_REP6, \\"
	echo "        __API_DEPRECATED_REP5, __API_DEPRECATED_REP4, \\"
	echo "        __API_DEPRECATED_REP3, __API_DEPRECATED_REP2)(__VA_ARGS__)"
	echo "#endif"
} >> "$H"
