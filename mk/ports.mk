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
# GNU autoconf trees from dist-dev-tools.  Xcode ships all of these in
# XcodeDefault.xctoolchain/usr/bin, except make/gnumake which live in
# Developer/usr/bin.
# ------------------------------------------------------------------
PORTS+=	dist-dev-tools/gperf gperf ${XCTOOLCHAIN}/usr/bin
PORTS+=	dist-dev-tools/flex flex ${XCTOOLCHAIN}/usr/bin
PORTS+=	dist-dev-tools/gnumake gnumake usr/bin

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
#PORTS+=	dist-dev-tools/gm4 gm4 ${XCTOOLCHAIN}/usr/bin
#PORTS+=	dist-dev-tools/bison bison ${XCTOOLCHAIN}/usr/bin
#
# Apple also ships lex, yacc and m4 in the toolchain, but they are
# distinct binaries rather than links to flex/bison/gm4, so they are not
# covered by P_LINKS here.

.endif # MK_PORTS
