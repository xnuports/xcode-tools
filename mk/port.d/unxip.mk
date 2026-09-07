# unxip -- extracts the .xip archives Apple ship Xcode itself in.  Nothing
# on the system will do that: xar reads the container but the payload is a
# pbzx stream inside it, and Archive Utility is the only thing that knows
# the whole shape.
#
# usr/local/bin.  It is an archive utility, not a Mach-O or build tool, so
# by the rule mk/ports.mk states for src/extras it goes there rather than
# into the toolchain.
#
# A Swift package, so swift build rather than a Makefile.  --scratch-path
# is the point of the arguments: SwiftPM would otherwise put .build inside
# the submodule, and nothing here writes to a submodule.
P_BUILDSYS=	make
P_COPY=		no
P_NOSTAGE=	yes
P_OBJDIR=	${P_WORKDIR}/build

P_MAKE=		swift
P_MAKE_ARGS=	build -c release \
		--package-path ${TOP}/src/extras/unxip \
		--scratch-path ${P_WORKDIR}/build

P_PROGS=	release/unxip
