# llvm-cbe -- LLVM's C backend, from JuliaHub's fork.
#
# Turns LLVM IR back into C.  It is not one of Apple's tools and does
# not pretend to be: it sits in the toolchain as something the
# toolchain can do.  Licensed NCSA, the terms LLVM itself was released
# under.
#
# The source is copied and patched rather than built where it lies,
# because it is a submodule and three things about it have to change
# to build here.  Each is in mk/patches/llvm-cbe:
#
#   The version it asks for.  It wants LLVM 22.1 and this tree builds
#   21.1.6.  Its own history offers nothing to fall back to -- the
#   requirement went from 20.1 straight to 22.1 -- and the 20.1 commit
#   fails against LLVM 21 in six places where master fails in two.
#
#   Those two places.  TargetLowering gained an argument in LLVM 22,
#   and this tree's LLVM is swiftlang's fork, whose addPassesToEmitFile
#   carries a CAS parameter upstream has not got -- so no unmodified
#   version of this tool would match it whatever version it targeted.
#
#   What it links.  Standalone, it links the LLVM shared library, which
#   this tree does not build and Apple's toolchain does not ship
#   either: Apple ships libLTO, libclang, libIndexStore, libcodedirectory
#   and libtapi, and no libLLVM.  Building one to satisfy this would be
#   a step away from what Apple ships, so it links the component
#   libraries instead.
#
# It is also compiled without RTTI, because the LLVM it links was:
# LLVMConfig.cmake says LLVM_ENABLE_RTTI is OFF, and a mismatch shows
# up at the end as missing typeinfo for llvm::cl::Option.

P_BUILDSYS=	cmake
P_CMAKE_SRC=	.
P_COPY=		yes
P_OBJDIR=	${P_WORKDIR}/build
P_NOSTAGE=	yes

P_PREPARE=	for p in ${TOP}/mk/patches/llvm-cbe/*.patch; do \
		    patch -s -p1 --forward < "$$p" || exit 1; \
		done

LLVM_BUILD=	${TOP}/build/ports/llvm/build
XCTC=		${TOP}/build/release/${XCTOOLCHAIN}/usr/bin

P_CONFIGURE_ARGS= \
	-DLLVM_DIR=${LLVM_BUILD}/lib/cmake/llvm \
	-DCMAKE_C_COMPILER=${XCTC}/clang \
	-DCMAKE_CXX_COMPILER=${XCTC}/clang++ \
	-DCMAKE_CXX_FLAGS=-fno-rtti \
	-DLLVM_INCLUDE_TESTS=OFF

P_MAKE_ARGS=	llvm-cbe
P_PROGS=	tools/llvm-cbe/llvm-cbe
