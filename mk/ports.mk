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
# GNU autoconf trees from apple-oss-distributions/distribution-Developer_Tools.  Xcode ships all of these in
# XcodeDefault.xctoolchain/usr/bin, except make/gnumake which live in
# Developer/usr/bin.
# ------------------------------------------------------------------
# bmake, the tool this project is built with -- see the note in
# mk/progs.mk about usr/local/bin.
PORTS+=	extras/bmake bmake usr/local/bin

PORTS+=	apple-oss-distributions/distribution-Developer_Tools/gperf gperf ${XCTOOLCHAIN}/usr/bin
PORTS+=	apple-oss-distributions/distribution-Developer_Tools/flex flex ${XCTOOLCHAIN}/usr/bin
PORTS+=	apple-oss-distributions/distribution-Developer_Tools/gnumake gnumake usr/bin

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

# llvm-cbe -- LLVM's C backend, which turns IR back into C.
#
# Not yet enabled, and the reason is worth writing down.  It is an
# out-of-tree LLVM tool and its standalone CMake links the LLVM shared
# library, which this tree does not build and Apple's toolchain does
# not ship either -- Apple ships libLTO, libclang, libIndexStore and
# libtapi, and no libLLVM.  Building one just to satisfy this would be
# a step away from what Apple ships rather than towards it, so the
# answer is to link the static component libraries instead, which
# needs a patch to the tool's own CMakeLists.
#
# The C++ side is already known to work: llvm-cbe's master compiles
# against the LLVM 21.1.6 this tree builds once two declarations in
# CTargetMachine are adjusted -- TargetLowering took an extra argument
# in LLVM 22, and this tree's LLVM is swiftlang's fork, whose
# addPassesToEmitFile carries a CAS parameter upstream does not have.
# Its own history offers no LLVM 21 version to fall back to: it went
# from 20.1 straight to 22.1, and the 20.1 commit fares worse here.
#PORTS+=	extras/llvm-cbe llvm-cbe ${XCTOOLCHAIN}/usr/bin

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
#PORTS+=	apple-oss-distributions/distribution-Developer_Tools/gm4 gm4 ${XCTOOLCHAIN}/usr/bin
#PORTS+=	apple-oss-distributions/distribution-Developer_Tools/bison bison ${XCTOOLCHAIN}/usr/bin
#
# Apple also ships lex, yacc and m4 in the toolchain, but they are
# distinct binaries rather than links to flex/bison/gm4, so they are not
# covered by P_LINKS here.

.endif # MK_PORTS
