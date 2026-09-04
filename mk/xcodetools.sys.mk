# mk/xcodetools.sys.mk
#
# Global build knobs shared by every driver Makefile in this tree.
# Everything is built with BSD bmake(1); no GNU make idioms are used.
#
# Layout:
#
#	build/			generated; safe to delete at any time
#	build/lib		static libraries built from submodule sources
#	build/obj/<dir>		per-tool object files
#	build/gen/<tool>	build-time generated sources
#	build/release/		final staged tree -- a drop-in Developer/
#		usr/{bin,lib,libexec,share}
#		Toolchains/XcodeDefault.xctoolchain/usr/bin
#		Platforms/<P>.platform/Developer/SDKs/<S>.sdk
#		Tools/
#
# The release layout mirrors Apple's Xcode Developer directory
# (see mk/progs.mk and docs/DOCUMENTATION.md section 1).

TOP?=		${.CURDIR}

CC?=		cc
CPPFLAGS+=	-I${TOP}/include -I${TOP}/build/include
CXX?=		c++

# bmake predefines CC (as "cc -pipe"), CFLAGS (as "-O2") and CXXFLAGS in
# its own sys.mk, so `?=` here is silently a no-op -- the same trap
# docs/CLAUDE.md section 8 documents for CC.  Assign plainly; a
# command-line `bmake CFLAGS=...` still wins over this.
CFLAGS=		-O2 -g -Wall -Wno-unused-parameter
CXXFLAGS=	${CFLAGS}

# Reproducible links.  With -g, ld records each object file's mtime in
# the debug map (N_OSO stab) and folds that into LC_UUID, so two clean
# builds of identical sources produce different binaries.  -reproducible
# zeroes those timestamps, which docs/CLAUDE.md section 12 rule 6
# ("reproducible from source alone") requires and section 11 tests.
LDFLAGS+=	-Wl,-reproducible

AR?=		ar
YACC?=		yacc
LEX?=		lex

INSTALL_DIR=	mkdir -p

ECHO=		echo

# Where the toolchain bundle's programs land.  cctools, ld64 and later
# clang/swiftc install here rather than into Developer/usr/bin, matching
# Apple (docs/DOCUMENTATION.md section 3.1).
XCTOOLCHAIN=	Toolchains/XcodeDefault.xctoolchain

#
# Our own sources under src/openxc-tools/ are held to -Werror; they build clean
# today and should stay that way.  Imported Apple/GNU sources predate
# most of these diagnostics and are deliberately not held to it -- see
# tool.mk, which applies this only to T_DIR values under openxc-tools/.
#
XCODE_STRICT_CFLAGS=	-Werror

#
# Optional program tiers.  Enable or disable with e.g.:
#
#	bmake MK_TOOLCHAIN=no
#
# MK_TOOLCHAIN gates the binutils tier (cctools, ld64) so the Xcode
# command-line tools alone can be built quickly.
#
MK_TOOLCHAIN?=	yes

# MK_PORTS gates the components that carry their own build system and
# are driven by mk/port.mk rather than compiled by mk/tool.mk.  Off by
# default: each one runs a full configure and make, which is far slower
# than the rest of the tree put together.
MK_PORTS?=	no

# Reserved for the foreign-build-system driver (see the plan, stage 5).
# These trees are not in the inventory yet; the names exist so the knobs
# stay stable when they land.
#	MK_LLVM, MK_SWIFT, MK_PYTHON, MK_GIT

# Per-program opt-outs.  Whole tiers are gated in mk/progs.mk instead --
# a duplicated list of program names here would go stale the moment a
# tool is renamed (as it did when nm/otool/size became the -classic
# names).  This stays for disabling individual programs.
DISABLED_PROGS=

.PHONY: all clean
