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
# GNU autoconf trees from distribution-Developer_Tools.  Xcode ships all of these in
# XcodeDefault.xctoolchain/usr/bin, except make/gnumake which live in
# Developer/usr/bin.
# ------------------------------------------------------------------
# bmake, the tool this project is built with -- see the note in
# mk/progs.mk about usr/local/bin.
PORTS+=	bmake bmake usr/local/bin

PORTS+=	distribution-Developer_Tools/gperf gperf ${XCTOOLCHAIN}/usr/bin
PORTS+=	distribution-Developer_Tools/flex flex ${XCTOOLCHAIN}/usr/bin
PORTS+=	distribution-Developer_Tools/gnumake gnumake usr/bin

# ------------------------------------------------------------------
# LLVM and Clang.  By far the longest build in the tree -- most of an
# hour on ten cores -- which is the main reason MK_PORTS is off by
# default.  See mk/port.d/llvm.mk, including the notes on tapi.
# ------------------------------------------------------------------
PORTS+=	llvm-project llvm ${XCTOOLCHAIN}/usr/bin

# ------------------------------------------------------------------
# bmake -- the build system itself.  Ships as usr/bin/bmake with
# its mk files in usr/share/bmake/mk/.  Autoconf build.
#
# Note: bmake is what drives this project's own build, so the
# *system* bmake builds our bmake port.  The staged binary ends
# up in the release tree alongside bsdmake and gnumake.
# ------------------------------------------------------------------
PORTS+=	bmake bmake usr/bin

# ------------------------------------------------------------------
# bsdmake -- Apple's BSD make, carried as a submodule at src/bsdmake/.
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
#PORTS+=	distribution-Developer_Tools/gm4 gm4 ${XCTOOLCHAIN}/usr/bin
#PORTS+=	distribution-Developer_Tools/bison bison ${XCTOOLCHAIN}/usr/bin
#
# Apple also ships lex, yacc and m4 in the toolchain, but they are
# distinct binaries rather than links to flex/bison/gm4, so they are not
# covered by P_LINKS here.

.endif # MK_PORTS
