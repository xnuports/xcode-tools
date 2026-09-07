# Capstone, the disassembler machsec is written against: it opens a handle
# with cs_open and walks __TEXT with cs_disasm to find the mitigations that
# only show up in the instruction stream.  BSD-3-Clause, the same licence
# this tree is under.
#
# Library only, and static: nothing here wants a second dylib on the
# runtime path, and machsec is the one thing that links it.  The
# architectures are cut down to the two this tree targets -- building all
# fifteen of Capstone's back ends to disassemble arm64 and x86 is a large
# amount of compiling for nothing.
P_BUILDSYS=	cmake
P_CMAKE_SRC=	.
P_OBJDIR=	${P_WORKDIR}/build
P_NOSTAGE=	yes
P_PROGS=

P_CONFIGURE_ARGS=	-DBUILD_SHARED_LIBS=OFF \
			-DCAPSTONE_BUILD_CSTOOL=OFF \
			-DCAPSTONE_BUILD_CSTEST=OFF \
			-DCAPSTONE_BUILD_MACOS_THIN=ON \
			-DCAPSTONE_ARCHITECTURE_DEFAULT=OFF \
			-DCAPSTONE_ARM64_SUPPORT=ON \
			-DCAPSTONE_X86_SUPPORT=ON

# cmake's install rules want a prefix; the library and its headers are
# gathered by hand instead, the way astcenc's and zstd's are.
P_POST_BUILD=	mkdir -p dest/lib dest/include && \
		cp -f libcapstone.a dest/lib/ && \
		cp -Rf ${TOP}/src/extras/capstone/include/capstone \
		    dest/include/

P_RELEASE_MERGE=	dest/lib usr/local/lib \
			dest/include usr/local/include
