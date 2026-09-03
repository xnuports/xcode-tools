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
#
# The unwinder is the system's.  libcxxabi defaults to LLVM's own and
# then refuses to configure unless libunwind is built too; on Darwin the
# unwinder lives in libSystem and is what Apple's libc++ uses, so the
# default is turned off rather than a third runtime added to satisfy it.
#
# It goes through RUNTIMES_CMAKE_ARGS because the runtimes are a separate
# CMake invocation that forwards only the variables it knows about -- set
# directly, the option is accepted here and never reaches libcxxabi.
#
# libcxx and libcxxabi are built for the C++ standard library: its
# headers are what an SDK carries at usr/include/c++/v1, and without them
# nothing including <string> compiles even though the libc++ stub is
# there to link against.
#
# compiler-rt is enabled for its builtins.  Without them a toolchain
# cannot link anything that tests an OS version: the @available check
# lowers to ___isPlatformVersionAtLeast, which lives in
# libclang_rt.osx.a and nowhere else, and Swift's own standard library
# will not link without it.  Only the builtins are wanted here -- the
# sanitizers, fuzzer and profiling runtimes are much larger and nothing
# in this tree asks for them yet.
#
# lld is enabled for ELF.  Apple's linker is Mach-O only -- ld64 rejects
# an ELF object outright -- while clang here already emits ELF for both
# targets and llvm-nm and llvm-objdump already read it, so the linker was
# the only thing missing.  ld64 is left alone and keeps the name `ld`:
# lld is an addition to the toolchain, not a replacement for it.

TAPI_PATCHES!=	ls ${TOP}/mk/patches/tapi/*.patch 2>/dev/null || true

P_BUILDSYS=	cmake
P_CMAKE_SRC=	llvm

# tapi would be built as an external project inside the LLVM tree --
# the only way it builds, since its CMakeLists uses llvm_add_library and
# reaches into CLANG_SOURCE_DIR -- and libtapi is the last thing standing
# between ld64 and a working `ld`.
#
# tapi 2.0.0 does not build against LLVM 21 as shipped; the tapi-src rule
# below copies it and applies mk/scripts/tapi-*.sh plus mk/patches/tapi/.
#
# LINKER_SUPPORTS_NO_INITS is forced off.  tapi asks the linker for
# -Wl,-no_inits, which Apple wants because the linker dlopens libtapi and
# they want no static-initializer cost; against LLVM 21 that fails, since
# two dozen LLVM objects now carry initializers.  Nothing here needs the
# property, so the check is answered in the negative rather than the flag
# being fought.
P_CONFIGURE_ARGS=	\
	-DLLVM_ENABLE_PROJECTS="clang;lld" \
	-DLLVM_ENABLE_RUNTIMES="compiler-rt;libcxx;libcxxabi" \
	-DRUNTIMES_CMAKE_ARGS="-DLIBCXXABI_USE_LLVM_UNWINDER=OFF" \
	-DCOMPILER_RT_BUILD_SANITIZERS=OFF \
	-DCOMPILER_RT_BUILD_XRAY=OFF \
	-DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
	-DCOMPILER_RT_BUILD_MEMPROF=OFF \
	-DCOMPILER_RT_BUILD_ORC=OFF \
	-DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
	-DLLVM_EXTERNAL_PROJECTS=tapi \
	-DLLVM_EXTERNAL_TAPI_SOURCE_DIR=${P_WORKDIR}/tapi-src \
	-DLINKER_SUPPORTS_NO_INITS=FALSE \
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

# LLVM's install target writes gigabytes of headers, libraries and
# tools we do not ship.  Take the binaries we want out of the build
# directory instead.
P_NOSTAGE=	yes

# Which of the built programs land in the toolchain.  Kept short on
# purpose: this is the set the rest of the tree actually needs, not
# everything LLVM installs.
# Build only the targets we ship, not "all".
#
# This matters for more than build time: with tapi in the tree, "all"
# also builds the tapi CLI tool and its APIVerifier/Frontend libraries,
# which carry drift beyond what the scripts and patches cover (clang's
# DiagnosticOptions is no longer reference-counted, for one).  libtapi
# itself -- the only part ld64 needs -- builds clean.
#
# The ELF set: lld links, llvm-ar archives, llvm-objcopy and llvm-readobj
# edit and inspect.  Each also builds the aliases beside it (ld.lld,
# llvm-ranlib, llvm-strip, llvm-readelf), which bundle-aliases links.
#
# llvm-libraries and clang-libraries are the whole static library sets,
# not just the ones the tools above happen to pull in.  Swift links both
# directly and needs more of them than we ship binaries for -- MCJIT and
# the rest of the execution engine, clangTooling and its neighbours --
# and discovering those one missing archive at a time is not a way to
# build anything.
P_MAKE_ARGS=	clang llvm-nm llvm-otool llvm-objdump llvm-size \
		llvm-strings dsymutil llvm-dwarfdump llvm-cov \
		llvm-profdata libtapi \
		lld llvm-ar llvm-objcopy llvm-readobj \
		llvm-libraries clang-libraries \
		LTO libclang libIndexStore.dylib \
		runtimes

# clang's resource directory -- its own stdarg.h, stddef.h and the rest.
# Without it clang finds no builtin headers and anything past trivial C
# fails with "'stdarg.h' file not found" from inside the SDK.  The
# version component is read from the build rather than hardcoded.
CLANG_RESOURCE_VER!=	ls ${P_WORKDIR}/build/lib/clang 2>/dev/null | head -1
P_TREES=	lib/clang/${CLANG_RESOURCE_VER}

# The libraries, staged into the toolchain's usr/lib rather than usr/bin.
#
# ld64 links libtapi.  The other two are what Apple's toolchain carries
# and this one did not: libLTO is the library a linker dlopens to do
# link time optimisation -- without it -flto has nothing to call -- and
# libclang is the C interface to clang that editors, indexers and
# anything else that wants to parse C without being a compiler links
# against.  Both are ordinary targets of the LLVM build; they were
# simply never asked for.
P_LIBS=		lib/libtapi.dylib \
		lib/libLTO.dylib \
		lib/libclang.dylib \
		lib/libIndexStore.dylib

#
# Only the real binaries are listed.  The alias names beside them in the
# build directory are symlinks, and copying a symlink here would copy the
# binary again -- lld is not small.  bundle-aliases links them instead.
P_PROGS=	bin/clang \
		bin/lld \
		bin/llvm-ar \
		bin/llvm-objcopy \
		bin/llvm-readobj \
		bin/llvm-nm \
		bin/llvm-otool \
		bin/llvm-objdump \
		bin/llvm-size \
		bin/llvm-strings \
		bin/dsymutil \
		bin/llvm-dwarfdump \
		bin/llvm-cov \
		bin/llvm-profdata

# tapi is built from a patched copy rather than the submodule: it calls
# llvm_check_linker_flag(), a helper current llvm-project no longer
# ships.  The copy is small, and the submodule stays read-only.
${P_WORKDIR}/tapi-src/.patched:
	@mkdir -p ${P_WORKDIR}
	@rsync -a --delete --exclude '.git' \
		${TOP}/src/apple/distribution-Developer_Tools/tapi/ ${P_WORKDIR}/tapi-src/
	@cd ${P_WORKDIR}/tapi-src && ${TOP}/mk/scripts/tapi-shim-linker-flag.sh \
		${TOP}/mk/scripts/tapi-linker-flag-shim.cmake
	@cd ${P_WORKDIR}/tapi-src && ${TOP}/mk/scripts/tapi-fix-diagnostics.sh
	@cd ${P_WORKDIR}/tapi-src && ${TOP}/mk/scripts/tapi-modernize-llvm-api.sh ${TOP}/src/swiftlang-llvm/llvm-project
# Patches come last, so they are written against the post-script tree.
# These are the changes the scripts cannot honestly express: real edits
# rather than renames.  bmake does not glob in .for, hence the != here.
.for pt in ${TAPI_PATCHES}
	@cd ${P_WORKDIR}/tapi-src && patch -s -p1 --forward < ${pt} && \
		${ECHO} "  applied ${pt:T}"
.endfor
	@touch ${.TARGET}

${P_WORKDIR}/.configured: ${P_WORKDIR}/tapi-src/.patched
