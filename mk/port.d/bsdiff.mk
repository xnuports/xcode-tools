# bsdiff/bspatch -- binary diff and patch.
#
# General utilities rather than toolchain pieces, so usr/local/bin.
#
# The tree ships configure.ac and no configure, so autogen runs first.
# That is what P_PREPARE is for: it runs in the copied tree, before
# configure, which is exactly the ports-style post-extract step.
P_PREPARE=	./autogen.sh
P_NOSTAGE=	yes
P_PROGS=	bsdiff bspatch
