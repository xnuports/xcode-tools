# bldd -- which binaries link a given library.
#
# The reverse of ldd: given a library, find the executables that use
# it.  That is Mach-O work, so it goes to the toolchain beside otool
# and nm rather than to usr/local.
P_BUILDSYS=	cmake
P_CMAKE_SRC=	.
P_OBJDIR=	${P_WORKDIR}/build
P_NOSTAGE=	yes
P_PROGS=	bin/bldd
