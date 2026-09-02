# llvm-cbe -- LLVM's C backend, from JuliaHub's fork.
#
# Turns LLVM IR back into C.  It is not one of Apple's tools and does
# not pretend to be: it goes in the toolchain beside the others as
# something the toolchain can do, not as a replacement for anything.
#
# It is an out-of-tree LLVM tool, so it needs the LLVM this tree has
# already built rather than one of its own -- LLVM_DIR names the
# directory holding LLVMConfig.cmake in that build, which is where
# find_package looks.  Building it against a different LLVM than the
# one that produced the IR it will read would be asking for trouble.
#
# Licensed NCSA, the same terms LLVM itself was released under.

P_BUILDSYS=	cmake
P_CMAKE_SRC=	.

LLVM_BUILD=	${TOP}/build/ports/llvm/build

P_CONFIGURE_ARGS= \
	-DLLVM_DIR=${LLVM_BUILD}/lib/cmake/llvm \
	-DCMAKE_C_COMPILER=${TOP}/build/release/${XCTOOLCHAIN}/usr/bin/clang \
	-DCMAKE_CXX_COMPILER=${TOP}/build/release/${XCTOOLCHAIN}/usr/bin/clang++ \
	-DLLVM_INCLUDE_TESTS=OFF

P_MAKE_ARGS=	llvm-cbe
P_PROGS=	bin/llvm-cbe
