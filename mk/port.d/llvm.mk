# llvm -- LLVM and Clang, the headline deliverable.
#
# Also the route to libtapi: tapi builds inside the LLVM tree (its
# CMakeLists uses llvm_add_library and reaches into CLANG_SOURCE_DIR),
# and libtapi is the one thing still standing between ld64 and a working
# `ld` -- every ld64 source already compiles.
#
# Scoped deliberately: only the two targets Apple ships for on this
# hardware, no tests, no docs, no examples, no bindings.  A full LLVM
# build is otherwise far larger than this tree needs.

P_BUILDSYS=	cmake
P_CMAKE_SRC=	llvm

P_CONFIGURE_ARGS=	\
	-DLLVM_ENABLE_PROJECTS=clang \
	-DLLVM_TARGETS_TO_BUILD="AArch64;X86" \
	-DLLVM_ENABLE_ASSERTIONS=OFF \
	-DLLVM_INCLUDE_TESTS=OFF \
	-DLLVM_INCLUDE_EXAMPLES=OFF \
	-DLLVM_INCLUDE_BENCHMARKS=OFF \
	-DLLVM_INCLUDE_DOCS=OFF \
	-DLLVM_ENABLE_BINDINGS=OFF \
	-DLLVM_ENABLE_ZSTD=OFF \
	-DCLANG_INCLUDE_TESTS=OFF \
	-DCLANG_INCLUDE_DOCS=OFF

# Which of the built programs land in the toolchain.  Kept short on
# purpose: this is the set the rest of the tree actually needs, not
# everything LLVM installs.
P_PROGS=	bin/clang \
		bin/llvm-nm \
		bin/llvm-otool \
		bin/llvm-objdump \
		bin/llvm-size \
		bin/llvm-strings \
		bin/dsymutil \
		bin/llvm-dwarfdump \
		bin/llvm-cov \
		bin/llvm-profdata
