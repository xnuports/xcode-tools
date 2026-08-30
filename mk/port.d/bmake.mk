# bmake -- NetBSD make, and the tool this project is built with.
#
# Building it matters for bootstrapping: without it the tree cannot be
# built at all, so shipping one removes the last external build
# dependency beyond a C compiler.
#
# It is an autoconf tree, but its configure wants a writable source
# directory for its own generated files, so the default P_COPY applies.
# bmake's Makefile installs under its own /usr/local regardless of the
# --prefix configure was given, so the staged paths carry that prefix.
P_PROGS=	local/bin/bmake

# Driven with bmake, not make: its Makefile is written in bmake syntax
# (".if exists", ".for"), and GNU make stops at "missing separator".
# Bootstrapping a make with a make is circular only in principle -- the
# host already has one, and this replaces the dependency on it.
P_MAKE=		bmake

# bmake needs its mk fragments at runtime or it reports "no system rules
# (sys.mk)".  Its install target places them, but only when driven by
# bmake -- under GNU make the install-mk rule is skipped silently, which
# is the other half of why P_MAKE is set above.
#
# The binary looks in its configured prefix by default, so point
# MAKESYSPATH at these when running it from the release tree:
#
#	MAKESYSPATH=<developer>/usr/local/share/mk bmake ...
P_RELEASE_TREES=	local/share/mk usr/local/share/mk
