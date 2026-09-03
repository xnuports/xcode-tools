# zsign -- sign iOS application bundles.
#
# A signing utility rather than part of the toolchain's Mach-O
# handling, so usr/local/bin.  Its Makefile lives under build/macos
# rather than at the top, so make is pointed at it.
P_BUILDSYS=	make
P_MAKE_ARGS=	-C build/macos
P_NOSTAGE=	yes
P_PROGS=	bin/zsign
