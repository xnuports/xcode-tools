# mk/ports.mk -- the inventory of components that carry their own build
# system, driven by mk/port.mk.
#
# One entry per port, three whitespace-separated fields:
#
#	PORTS+= <dir-under-src> <port-name> <install-suffix>
#
# These are gated behind MK_PORTS because they are slow: each one runs
# a full configure and make.  See mk/xcodetools.sys.mk.

.if ${MK_PORTS:tl} == "yes"

# ------------------------------------------------------------------
# GNU autoconf trees from apple/distribution-Developer_Tools.  Xcode ships all of these in
# XcodeDefault.xctoolchain/usr/bin, except make/gnumake which live in
# Developer/usr/bin.
# ------------------------------------------------------------------
# bmake, the tool this project is built with -- see the note in
# mk/progs.mk about usr/local/bin.
PORTS+=	extras/bmake bmake usr/local/bin

PORTS+=	apple/distribution-Developer_Tools/gperf gperf ${XCTOOLCHAIN}/usr/bin
PORTS+=	apple/distribution-Developer_Tools/flex flex ${XCTOOLCHAIN}/usr/bin
PORTS+=	apple/distribution-Developer_Tools/gnumake gnumake usr/bin

# ------------------------------------------------------------------
# git and python, both of which Xcode ships in Developer/usr/bin.
# git comes from Apple's distribution, so it reports the same
# "(Apple Git-N)" Xcode's does; cpython does not match -- Xcode's
# python3 is 3.9.6, the old shim, and this is current.
#
# pcre2 comes first because Apple's git links it statically out of the
# internal SDK.  See mk/port.d/pcre2.mk.
# ------------------------------------------------------------------
# The third word is where P_PROGS would land, and pcre2 has none: it
# installs a static library into the internal SDK instead.  usr/local/lib
# is what the other library ports name.
PORTS+=	extras/pcre2 pcre2 usr/local/lib
PORTS+=	apple/distribution-Developer_Tools/Git git usr/bin
PORTS+=	python/cpython python3 usr/bin

# perl, also Apple's: perl-175 carries the 5.34.1 /usr/bin/perl reports.
# See mk/port.d/perl.mk for the two things their build assumes and this
# one cannot -- running as root, and an x86_64 host.
PORTS+=	apple/perl perl usr/bin

# ------------------------------------------------------------------
# Go, and ipsw which is written in it.  Neither is something Xcode
# ships, so both go to usr/local.  Go is pinned to a release tag --
# see mk/port.d/go.mk for the bootstrap it needs.
# ------------------------------------------------------------------
PORTS+=	golang/go go usr/local/bin
PORTS+=	extras/ipsw ipsw usr/local/bin

# ------------------------------------------------------------------
# LLVM and Clang.  By far the longest build in the tree -- most of an
# hour on ten cores -- which is the main reason MK_PORTS is off by
# default.  See mk/port.d/llvm.mk, including the notes on tapi.
# ------------------------------------------------------------------
PORTS+=	swiftlang-llvm/llvm-project llvm ${XCTOOLCHAIN}/usr/bin

# ------------------------------------------------------------------
# swift -- the Swift compiler, built against the LLVM above rather than
# the one build-script would produce for itself.  See mk/port.d/swift.mk
# for why, and for the three sibling checkouts it needs.
#
# Not cheap: this is the largest thing in the tree after LLVM.
# ------------------------------------------------------------------
PORTS+=	swiftlang-llvm/swift swift ${XCTOOLCHAIN}/usr/bin

# llvm-cbe reads the LLVM built above, so it comes after it.
PORTS+=	extras/llvm-cbe llvm-cbe ${XCTOOLCHAIN}/usr/bin

# ------------------------------------------------------------------
# src/extras.  Tools this tree carries that Apple does not publish,
# sorted by what they are rather than where they came from: anything
# that works on Mach-O or on a build goes to the toolchain beside
# clang and ld, and the general utilities go to usr/local/bin, which
# is the same reasoning bmake and bsdmake follow above.
# ------------------------------------------------------------------
PORTS+=	extras/ldid ldid ${XCTOOLCHAIN}/usr/bin
PORTS+=	extras/snaputil snaputil usr/local/bin
PORTS+=	extras/bldd bldd ${XCTOOLCHAIN}/usr/bin
PORTS+=	extras/arm64th forth usr/local/bin
PORTS+=	extras/zsign zsign usr/local/bin
PORTS+=	extras/bsdiff bsdiff usr/local/bin
PORTS+=	extras/patchelf patchelf usr/local/bin

# Compatibility libraries and libplist.  These are not tools; they are
# carried so that source written for Linux or BSD, and code that speaks
# property lists, can be built against this tree.  Headers and
# libraries go to usr/local, not into the SDK: the SDK describes what
# the platform provides and these are additions to it.
PORTS+=	extras/libepoll-shim libepoll-shim usr/local/lib
PORTS+=	extras/libinotify-kqueue libinotify-kqueue usr/local/lib
PORTS+=	extras/libplist plistutil usr/local/bin

# ------------------------------------------------------------------
# bsdmake -- Apple's BSD make, carried as a submodule at src/extras/bsdmake/.
# Built via mk/tool.mk (not mk/port.mk) because its Makefile depends
# on <bsd.prog.mk> and friends, which are the very files it ships.
# See mk/tool.d/bsdmake.mk.
# ------------------------------------------------------------------

# gm4 and bison are NOT enabled.  Both restore their missing gnulib
# templates fine (mk/port.d/), but then their bundled gnulib -- which
# predates the modern SDK by two decades -- substitutes its own
# <stdint.h>/<inttypes.h> and the system _inttypes.h no longer sees
# intmax_t:
#
#   _inttypes.h:238:8: error: unknown type name 'intmax_t'
#
# Getting them building means either forcing configure to accept the
# system headers or updating the vendored gnulib.  Their port.d
# fragments are in place for whoever picks that up.
#
#PORTS+=	apple/distribution-Developer_Tools/gm4 gm4 ${XCTOOLCHAIN}/usr/bin
#PORTS+=	apple/distribution-Developer_Tools/bison bison ${XCTOOLCHAIN}/usr/bin
#
# Apple also ships lex, yacc and m4 in the toolchain, but they are
# distinct binaries rather than links to flex/bison/gm4, so they are not
# covered by P_LINKS here.

.endif # MK_PORTS
