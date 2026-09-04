#!/bin/sh
#
# emit-darwin-modulemap.sh -- write the SDK's Darwin.modulemap to stdout.
#
# `import Darwin' is how Swift reaches the C library, and it is not a
# Swift module: it is a Clang module, declared over the SDK's own C
# headers by this file.  Without it the SDK compiles Swift that touches
# nothing outside the stdlib and no more -- Synchronization and
# Observation both stop at "missing required module 'Darwin'".
#
# Apple ships a Swift overlay beside it, usr/lib/swift/Darwin.swiftmodule,
# which is prebuilt and has no source in the swift repository (its
# stdlib/public/Platform carries Glibc, Musl, Android and WASILibc, and
# no Darwin).  It turns out not to be needed: Swift imports the Clang
# module directly when no overlay is present.
#
# THE LAYERING
#
# A Clang module graph has to be acyclic, and the obvious arrangement --
# one Darwin module over every header -- is not.  <stdint.h> resolves to
# clang's own, which include_next's the SDK's, which includes
# <sys/_types.h>; if that header belongs to Darwin then Darwin depends
# on _Builtin_stdint which depends on Darwin, and every import fails
# with "cyclic dependency in module 'Darwin'".  The same cycle appears
# again through machine/_endian.h, xlocale, ctype.h and others: the
# low-level headers are what everything is built out of, clang's
# builtins included.
#
# Apple's answer is to keep them in separate modules -- DarwinFoundation
# 1, 2 and 3 -- and declare Darwin above those.  The set below is that
# partition, which is a fact about how these headers include one
# another rather than a choice; ours puts it under a single name.  The
# names in Darwin proper are the interface, though, so they follow
# Apple's: Swift code says Darwin.C.stdio and Darwin.os.lock.
#
# Keeping both means doing what Apple does for the C library: stdio.h
# itself belongs to the foundation layer, so Darwin.C.stdio is declared
# over a one-line wrapper in _modules/ that includes it.  Darwin then
# depends on the foundation module and never the other way about.
#
# Every submodule is emitted only if its header is actually installed,
# so this grows with the SDK's coverage and needs no hand-editing.
#
# Usage: emit-darwin-modulemap.sh <sdk-include-dir>
#
set -e

INC="$1"
[ -n "${INC}" ] && [ -d "${INC}" ] ||
	{ echo "usage: $0 <sdk-include-dir>" >&2; exit 1; }

# The headers Apple keeps below Darwin, in DarwinFoundation1/2/3 and
# DarwinBasic.  Anything named here is declared in _DarwinFoundation and
# left out of Darwin.
# Apple splits these across DarwinFoundation1, 2 and 3, and the split
# is load-bearing rather than cosmetic.  stdint.h lives in layer 2 and
# _inttypes.h in layer 3, so the unavoidable detour through clang's
# _Builtin_stdint runs 3 -> builtin -> 2 and terminates.  Merged into
# one module the same detour runs straight back into the module being
# built, and nothing compiles.  So the three layers are kept.
#
# Apple's DarwinBasic is deliberately not folded in with them.  It is
# where mach-o/dyld.h lives, and that header includes <stdint.h>; put
# into a foundation layer it drags the same builtin detour back into
# the layers themselves.  Apple keeps it as a separate top-level
# module and so does this -- mach-o/ is declared in Darwin below.
#
# math.h and complex.h sit a layer higher than Apple puts them, and
# that is ours rather than theirs: Apple's are self-contained, while
# each of ours is the small adapter over lib/msun described in that
# directory's README, and the vendored header it includes reaches the
# layer-2 type headers.  Left in layer 1 they make layer 1 depend on
# layer 2, which already depends on it.
FOUNDATION1='
Availability.h
AvailabilityInternal.h
AvailabilityInternalLegacy.h
AvailabilityMacros.h
AvailabilityVersions.h
TargetConditionals.h
__xlocale.h
_assert.h
_bounds.h
_mb_cur_max.h
_static_assert.h
_types/_locale_t.h
arm/_endian.h
arm/_limits.h
arm/limits.h
assert.h
errno.h
fenv.h
float.h
i386/_endian.h
i386/_limits.h
i386/limits.h
iso646.h
limits.h
machine/_endian.h
machine/_limits.h
machine/limits.h
os/availability.h
setjmp.h
stddef.h
sys/__endian.h
sys/_posix_availability.h
sys/_symbol_aliasing.h
sys/_types/_errno_t.h
sys/_types/_u_char.h
sys/_types/_u_int.h
sys/_types/_u_int16_t.h
sys/_types/_u_int32_t.h
sys/_types/_u_int64_t.h
sys/_types/_u_int8_t.h
sys/_types/_u_short.h
sys/appleapiopts.h
sys/cdefs.h
sys/errno.h
sys/qos.h
sys/syslimits.h
'

FOUNDATION2='
_ctermid.h
_ctype.h
_locale.h
_locale_posix2008.h
_printf.h
_stdio.h
_string.h
_strings.h
_time.h
_types.h
_types/_intmax_t.h
_types/_nl_item.h
_types/_uint16_t.h
_types/_uint32_t.h
_types/_uint64_t.h
_types/_uint8_t.h
_types/_uintmax_t.h
alloca.h
arm/_types.h
arm/endian.h
arm/types.h
ctype.h
i386/_types.h
i386/endian.h
i386/types.h
libkern/_OSByteOrder.h
libkern/arm/_OSByteOrder.h
libkern/i386/_OSByteOrder.h
locale.h
machine/_types.h
machine/endian.h
machine/types.h
nl_types.h
runetype.h
secure/_common.h
secure/_stdio.h
secure/_string.h
secure/_strings.h
stdint.h
stdio.h
string.h
sys/_endian.h
sys/_pthread/_pthread_attr_t.h
sys/_pthread/_pthread_cond_t.h
sys/_pthread/_pthread_condattr_t.h
sys/_pthread/_pthread_key_t.h
sys/_pthread/_pthread_mutex_t.h
sys/_pthread/_pthread_mutexattr_t.h
sys/_pthread/_pthread_once_t.h
sys/_pthread/_pthread_rwlock_t.h
sys/_pthread/_pthread_rwlockattr_t.h
sys/_pthread/_pthread_t.h
sys/_pthread/_pthread_types.h
sys/_types.h
sys/_types/_blkcnt_t.h
sys/_types/_blksize_t.h
sys/_types/_caddr_t.h
sys/_types/_clock_t.h
sys/_types/_ct_rune_t.h
sys/_types/_dev_t.h
sys/_types/_fd_clr.h
sys/_types/_fd_copy.h
sys/_types/_fd_def.h
sys/_types/_fd_isset.h
sys/_types/_fd_set.h
sys/_types/_fd_setsize.h
sys/_types/_fd_zero.h
sys/_types/_fsblkcnt_t.h
sys/_types/_fsfilcnt_t.h
sys/_types/_gid_t.h
sys/_types/_id_t.h
sys/_types/_in_addr_t.h
sys/_types/_in_port_t.h
sys/_types/_ino64_t.h
sys/_types/_ino_t.h
sys/_types/_int16_t.h
sys/_types/_int32_t.h
sys/_types/_int64_t.h
sys/_types/_int8_t.h
sys/_types/_intptr_t.h
sys/_types/_key_t.h
sys/_types/_mach_port_t.h
sys/_types/_mode_t.h
sys/_types/_nlink_t.h
sys/_types/_null.h
sys/_types/_off_t.h
sys/_types/_offsetof.h
sys/_types/_pid_t.h
sys/_types/_ptrdiff_t.h
sys/_types/_rsize_t.h
sys/_types/_rune_t.h
sys/_types/_seek_set.h
sys/_types/_size_t.h
sys/_types/_ssize_t.h
sys/_types/_suseconds_t.h
sys/_types/_time_t.h
sys/_types/_timespec.h
sys/_types/_timeval.h
sys/_types/_uid_t.h
sys/_types/_uintptr_t.h
sys/_types/_useconds_t.h
sys/_types/_uuid_t.h
sys/_types/_va_list.h
sys/_types/_wchar_t.h
sys/_types/_wint_t.h
sys/stdio.h
sys/types.h
tgmath.h
time.h
xlocale/_ctype.h
xlocale/_stdio.h
xlocale/_string.h
xlocale/_time.h
'

FOUNDATION3='
___wctype.h
__wctype.h
_abort.h
_inttypes.h
_stdlib.h
_types/_wctrans_t.h
_types/_wctype_t.h
_wchar.h
_wctype.h
_xlocale.h
arm/_mcontext.h
arm/signal.h
complex.h
gethostuuid.h
i386/_mcontext.h
i386/signal.h
inttypes.h
mach/arm/_structs.h
mach/i386/_structs.h
mach/machine/_structs.h
machine/_mcontext.h
machine/signal.h
malloc/_malloc.h
malloc/_malloc_type.h
malloc/_ptrcheck.h
math.h
msun/complex.h
msun/math.h
pthread/pthread.h
pthread/pthread_impl.h
pthread/qos.h
pthread/sched.h
signal.h
stdlib.h
sys/_select.h
sys/_types/_mbstate_t.h
sys/_types/_posix_vdisable.h
sys/_types/_sigaltstack.h
sys/_types/_sigset_t.h
sys/_types/_ucontext.h
sys/resource.h
sys/select.h
sys/signal.h
sys/unistd.h
sys/wait.h
unistd.h
wchar.h
wctype.h
xlocale.h
xlocale/___wctype.h
xlocale/_inttypes.h
xlocale/_stdlib.h
xlocale/_wchar.h
xlocale/_wctype.h
'

is_foundation() {
	printf '%s\n%s\n%s\n' "${FOUNDATION1}" "${FOUNDATION2}" \
	    "${FOUNDATION3}" | grep -qxF "$1"
}

# A module name from a header path: sys/_types/_size_t.h -> sys__types__size_t
modname() {
	n=$(printf '%s' "${1%.h}" | tr -c 'A-Za-z0-9_' '_')
	case "${n}" in [0-9]*) n="_${n}";; esac
	printf '%s' "${n}"
}

# ---- the foundation module -------------------------------------------

# The headers clang provides itself are not declared here at all.
#
# stdint.h and its neighbours already belong to clang's own _Builtin_*
# modules, and the SDK's copies are reached through those: clang's
# <stdint.h> include_next's ours.  Declaring them again puts one header
# in two modules and draws an edge back from the builtin into whichever
# layer claimed it -- as stddef.h did, pulling layer 1 into layer 2,
# which already depends on layer 1.  Marking them `explicit' does not
# help: that governs implicit import, not what is compiled into the
# module.  Leaving them to clang removes the edge entirely, and a
# program that includes them still gets them.
builtin_shadowed() {
	case "$1" in
	stdint.h|inttypes.h|float.h|limits.h|stddef.h|stdarg.h) return 0;;
	stdbool.h|iso646.h|tgmath.h|stdalign.h|stdnoreturn.h) return 0;;
	stdatomic.h|varargs.h) return 0;;
	esac
	return 1
}

# One layer's module.
#
# Headers come in families and have to be declared as one.  Apple's
# _string is the example: string.h is a *textual* header there, the
# module's real content is _string.h, and xlocale/_string.h is a
# submodule beside it.  Split into three sibling modules instead --
# which is the obvious reading of the header list -- and a program that
# includes <string.h> gets the module owning string.h and not the one
# owning xlocale/_string.h, so strncasecmp_l is declared and invisible
# at the same time.  swift-foundation's string_shims.c stops exactly
# there.
#
# So each _X.h takes X.h with it as a textual header and xlocale/_X.h
# as a submodule, and neither is declared again on its own.
emit_layer() {  # $1 module name, $2 header list
	consumed="${TMPDIR:-/tmp}/darwinmap-used.$$"
	: > "${consumed}"
	printf '%s\n' "$2" | while read -r h; do
		case "${h}" in _*.h) ;; *) continue;; esac
		base="${h#_}"
		[ -f "${INC}/${base}" ] && echo "${base}" >> "${consumed}"
		[ -f "${INC}/xlocale/${h}" ] && echo "xlocale/${h}" >> "${consumed}"
	done

	printf 'module %s [system] {\n' "$1"
	printf '%s\n' "$2" | while read -r h; do
		[ -n "${h}" ] || continue
		[ -f "${INC}/${h}" ] || continue
		builtin_shadowed "${h}" && continue
		grep -qxF "${h}" "${consumed}" 2>/dev/null && continue
		case "${h}" in
		_*.h)
			base="${h#_}"
			printf '  module %s {\n' "$(modname "${h}")"
			[ -f "${INC}/${base}" ] &&
			    printf '    textual header "%s"\n' "${base}"
			printf '    header "%s"\n' "${h}"
			if [ -f "${INC}/xlocale/${h}" ]; then
				printf '    module xlocale { header "xlocale/%s" export * }\n' "${h}"
			fi
			printf '    export *\n  }\n'
			;;
		*)
			printf '  module %s { header "%s" export * }\n' \
			    "$(modname "${h}")" "${h}"
			;;
		esac
	done
	rm -f "${consumed}"
	printf '}\n\n'
}

emit_layer _DarwinFoundation1 "${FOUNDATION1}"
emit_layer _DarwinFoundation2 "${FOUNDATION2}"
emit_layer _DarwinFoundation3 "${FOUNDATION3}"

# ---- Darwin ----------------------------------------------------------

# Headers no module can be built over.
#
# A module is declared only over a header that compiles on its own, and
# that is checked here rather than assumed.  xnu's tree carries the
# kernel's headers next to the ones that ship, and sdk-headers takes
# more of mach/ and sys/ than Apple installs, so the SDK holds several
# dozen -- sys/*_internal.h, the dtrace implementation headers,
# mach/upl.h -- that reach vm/ or kernel-only types and cannot compile
# outside the kernel.  Nothing noticed before, because a header only
# has to compile when something builds a module over it, and until this
# file nothing ever did.
#
# Checking beats listing: a list would be one more thing to keep in
# step with xnu, and this way the map corrects itself.  It also means
# the map never declares a module that cannot be built, which is the
# failure that is hardest to read when it happens.
#
# This is not a fix for the over-inclusion, which is still to do.
SDKROOT="${INC%/usr/include}"
CC="${CC:-clang}"
TMPC="${TMPDIR:-/tmp}/darwinmap.$$.c"
trap 'rm -f "${TMPC}"' EXIT

compiles() {
	printf '#include <%s>\n' "$1" > "${TMPC}"
	${CC} -fsyntax-only -isysroot "${SDKROOT}" "${TMPC}" >/dev/null 2>&1
}

# A submodule for one header.  A header in the foundation layer is
# reached through a generated wrapper, so that Darwin.C.stdio exists
# under that name without stdio.h belonging to two modules at once.
mod() {
	[ -f "${INC}/$3" ] || return 0
	compiles "$3" || return 0
	if is_foundation "$3"; then
		w="_modules/_darwin_$2.h"
		mkdir -p "${INC}/_modules"
		printf '#include <%s>\n' "$3" > "${INC}/${w}"
		printf '%s  module %s { header "%s" export * }\n' "$1" "$2" "${w}"
	else
		printf '%s  module %s { header "%s" export * }\n' "$1" "$2" "$3"
	fi
}

# Every non-foundation header in a directory becomes a submodule named
# after it.  mach/ alone is over a hundred headers, all generated or
# installed rather than written, so listing them by hand would go stale
# on the next xnu build.
dirgroup() {
	[ -d "${INC}/$3" ] || return 0
	body=""
	for h in $(ls "${INC}/$3" 2>/dev/null | grep '[.]h$' | sort); do
		is_foundation "$3/${h}" && continue
		# Headers claimed by a top-level module of their own.
		if [ "$4" = MACHO_TOPLEVEL ]; then
			case "${h}" in dyld.h|utils.h) continue;; esac
		fi
		compiles "$3/${h}" || continue
		body="${body}$1    module $(modname "${h}") { header \"$3/${h}\" export * }
"
	done
	[ -n "${body}" ] || return 0
	printf '%s  module %s {\n' "$1" "$2"
	printf '%s' "${body}"
	printf '%s  }\n' "$1"
}

echo 'module Darwin [system] {'
echo '  export _DarwinFoundation1'
echo '  export _DarwinFoundation2'
echo '  export _DarwinFoundation3'

echo '  module C {'
for e in \
	assert:assert.h complex:complex.h ctype:ctype.h errno:errno.h \
	fenv:fenv.h locale:locale.h math:math.h setjmp:setjmp.h \
	signal:signal.h stdio:stdio.h stdlib:stdlib.h string:string.h \
	time:time.h wchar:wchar.h wctype:wctype.h ucontext:ucontext.h \
	copyfile:copyfile.h err:err.h readpassphrase:readpassphrase.h \
	util:util.h xattr_flags:xattr_flags.h
do
	mod '  ' "${e%%:*}" "${e#*:}"
done
echo '  }'

echo '  module POSIX {'
for e in \
	aio:aio.h cpio:cpio.h dirent:dirent.h dlfcn:dlfcn.h fcntl:fcntl.h \
	fmtmsg:fmtmsg.h fnmatch:fnmatch.h ftw:ftw.h glob:glob.h grp:grp.h \
	iconv:iconv.h ifaddrs:ifaddrs.h langinfo:langinfo.h \
	libgen:libgen.h monetary:monetary.h netdb:netdb.h \
	nl_types:nl_types.h poll:poll.h pthread:pthread.h pwd:pwd.h \
	regex:regex.h sched:sched.h semaphore:semaphore.h spawn:spawn.h \
	strings:strings.h syslog:syslog.h termios:termios.h \
	ulimit:ulimit.h unistd:unistd.h utime:utime.h wordexp:wordexp.h \
	inet:arpa/inet.h
do
	mod '  ' "${e%%:*}" "${e#*:}"
done
echo '  }'

dirgroup '' Mach mach
dirgroup '' mach_debug mach_debug
dirgroup '' sys sys
dirgroup '' net net
dirgroup '' netinet netinet
dirgroup '' netinet6 netinet6
dirgroup '' arpa arpa
dirgroup '' bsm bsm
dirgroup '' malloc malloc
dirgroup '' uuid uuid
dirgroup '' libkern libkern
	# Darwin.os, the low-level half.
	#
	# Apple splits os/ across two modules and both are needed.
	# Observation asks for Darwin.os.lock by that name, and `import
	# os' resolves the top-level module below.  A header belongs to
	# exactly one module, so the two lists are disjoint.
	#
	# Darwin gets lock.h alone.  base.h has to go to the top-level os
	# module rather than here, because os/atomic.h includes it: with
	# base.h inside Darwin, os depends on Darwin, and Darwin already
	# depends on os through sys/dtrace.h -- "cyclic dependency in
	# module 'Darwin': Darwin -> os -> Darwin".  Giving os its own
	# base.h leaves the dependency running one way only.
	body=""
	# clock.h is not here either: see the case above.
	for h in lock.h base.h proc.h; do
		[ -f "${INC}/os/${h}" ] || continue
		compiles "os/${h}" || continue
		# A wrapper apiece, pointing at the header the top-level os
		# module owns.  Both spellings have to resolve --
		# Observation says Darwin.os.lock and swift-foundation says
		# `import os' then os_unfair_lock -- and only one module can
		# own a header.  Pointing this way keeps the dependency
		# running Darwin -> os, which is the direction it already
		# runs: mach/message.h and sys/dtrace.h include os headers.
		w="_modules/_darwin_os_${h}"
		mkdir -p "${INC}/_modules"
		printf '#include <os/%s>\n' "${h}" > "${INC}/${w}"
		body="${body}    module $(modname "${h}") { header \"${w}\" export * }
"
	done
	if [ -n "${body}" ]; then
		printf '  module os {\n'
		printf '%s' "${body}"
		printf '  }\n'
	fi
dirgroup '' architecture architecture
dirgroup '' xlocale xlocale
dirgroup '' mach_o mach-o MACHO_TOPLEVEL

for e in \
	ar:ar.h AssertMacros:AssertMacros.h \
	ConditionalMacros:ConditionalMacros.h crt_externs:crt_externs.h \
	execinfo:execinfo.h fstab:fstab.h fts:fts.h getopt:getopt.h \
	libc:libc.h libproc:libproc.h MacTypes:MacTypes.h \
	membership:membership.h paths:paths.h sysexits:sysexits.h \
	sysdir:sysdir.h utmp:utmp.h
do
	mod '' "${e%%:*}" "${e#*:}"
done

if [ -f "${INC}/Block.h" ]; then
	printf '  module block {\n    requires blocks\n    header "Block.h"\n    export *\n  }\n'
fi

echo '}'

# os, at the top level.
#
# Apple keeps this out of Darwin, in its own os.modulemap named by
# three extern-module lines.  It has to be top-level: `import os'
# resolves a module called os, and nesting it as Darwin.os satisfies
# only `import Darwin.os'.  swift-foundation writes `internal import
# os', which is why the whole build stopped here.
#
# It stays in this one file rather than a second, and module.modulemap
# names it with its own extern line beside Darwin's.  os depends on the
# foundation layer through <stdint.h> and the like, and nothing in it
# reaches back into Darwin, so it sits beside Darwin without a cycle.
echo ''
echo 'module os [system] {'
echo '  export _DarwinFoundation1'
echo '  export _DarwinFoundation2'
echo '  export _DarwinFoundation3'
for h in $(ls "${INC}/os" 2>/dev/null | grep '[.]h$' | sort); do
	# lock.h belongs to Darwin.os above, and the foundation layers own
	# availability.h -- a header cannot be in two modules at once.
	# os.lock is still reachable, through the wrapper written below,
	# which is how Apple does it: os.modulemap declares its lock
	# submodule over _modules/_os_lock.h and marks it deprecated in
	# favour of Darwin.os.lock.  Both spellings have to work --
	# swift-foundation says `import os' and then os_unfair_lock.
	# Left undeclared: see the note at the end of this file.
	# clock.h goes with them, because it includes os/workgroup.h and
	# an undeclared header's includes land in whoever included it --
	# so declaring clock.h means os depends on mach/port.h, and
	# Darwin depends on os.
	# trace.h joins them, and for the same shape of reason: it
	# includes <xpc/xpc.h>, which includes <dispatch/dispatch.h>,
	# which is Darwin's.  It only became compilable when this SDK
	# started carrying the xpc headers, and being compilable is what
	# drew it into the module.
	# security_config.h is the same shape again: it includes
	# <sys/types.h>, which is Darwin's, and Darwin depends on os.
	# Apple ships the header too, and their os.modulemap does not
	# name it either.
	case "${h}" in workgroup*.h|clock.h|trace.h|security_config.h) continue;; esac
	# os owns the rest; Darwin reaches lock and base through the
	# wrappers written above.
	is_foundation "os/${h}" && continue
	# The SPI headers are installed for ld64's sake and Apple's SDK
	# ships none of them.  They also reach back into Darwin --
	# eventlink_private.h pulls mach/ -- and Darwin already depends on
	# os through mach/vm_statistics.h, so declaring them here is a
	# cycle: "os -> Darwin -> os".  They stay installed and undeclared.
	case "${h}" in *_private.h) continue;; esac
	compiles "os/${h}" || continue
	printf '  module %s { header "os/%s" export * }\n' "$(modname "${h}")" "${h}"
done
echo '}'

# MachO, at the top level.
#
# Apple declares this in DarwinBasic.modulemap, named by its own extern
# line, holding mach-o/dyld.h and mach-o/utils.h.  swift-foundation
# writes `import MachO.dyld', which resolves a top-level MachO and not
# Darwin.mach_o.
#
# The rest of mach-o/ -- loader.h, nlist.h and the others -- stays in
# Darwin, and these two include loader.h, so MachO depends on Darwin.
# That runs one way only: the single Darwin header that reaches back
# for dyld.h is sys/linker_set.h, which does not compile outside the
# kernel and is already left out of the map.
echo ''
echo 'module MachO [system] {'
for h in dyld.h utils.h; do
	[ -f "${INC}/mach-o/${h}" ] || continue
	compiles "mach-o/${h}" || continue
	printf '  module %s { header "mach-o/%s" export * }\n' "$(modname "${h}")" "${h}"
done
echo '}'

# objc/ is installed but not declared as a module.
#
# Apple declares an ObjectiveC module, an umbrella over objc/, and
# os/object.h imports objc/NSObject.h from inside the os module.  Here
# that closes a loop -- Darwin -> os -> ObjectiveC -> Darwin -- because
# Darwin depends on os (sys/mount.h and others include os/ headers) and
# the objc umbrella pulls objc-auto.h and so malloc/, which Darwin
# owns.  Apple's SDK has the same three edges and builds; whatever lets
# it is not visible in the module maps, and guessing at it costs more
# than the module is worth right now.
#
# Undeclared, the headers are still found and included textually, which
# is how os/object.h has been compiling all along.  What is lost is
# `import ObjectiveC' from Swift.

# The workgroup family is installed and left undeclared.
#
# Apple gives it a module of its own, os_workgroup, and that cannot be
# done here: those headers include <mach/port.h>, which Darwin owns,
# and os/clock.h includes them, so declaring them draws
# "os -> os_workgroup -> Darwin" while Darwin already depends on os
# through mach/vm_statistics.h.  Apple's Darwin and os divide up
# differently and theirs does not close.
#
# Undeclared they are still found and included textually, which is how
# os/clock.h has always reached them.  They compiled for the first time
# when SPI_AVAILABLE was defined, and being compilable is what pulled
# them into the module and broke it -- they were excluded before by
# failing, not by intent.
