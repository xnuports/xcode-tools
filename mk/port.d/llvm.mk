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
	-DLLVM_ENABLE_PROJECTS=clang \
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
P_MAKE_ARGS=	clang llvm-nm llvm-otool llvm-objdump llvm-size \
		llvm-strings dsymutil llvm-dwarfdump llvm-cov \
		llvm-profdata libtapi

# libtapi is a library, not a program, and ld64 links against it.  It is
# staged into the toolchain's usr/lib rather than usr/bin.
P_LIBS=		lib/libtapi.dylib

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

# tapi is built from a patched copy rather than the submodule: it calls
# llvm_check_linker_flag(), a helper current llvm-project no longer
# ships.  The copy is small, and the submodule stays read-only.
${P_WORKDIR}/tapi-src/.patched:
	@mkdir -p ${P_WORKDIR}
	@rsync -a --delete --exclude '.git' \
		${TOP}/src/dist-dev-tools/tapi/ ${P_WORKDIR}/tapi-src/
	@cd ${P_WORKDIR}/tapi-src && ${TOP}/mk/scripts/tapi-shim-linker-flag.sh \
		${TOP}/mk/scripts/tapi-linker-flag-shim.cmake
	@cd ${P_WORKDIR}/tapi-src && ${TOP}/mk/scripts/tapi-fix-diagnostics.sh
	@cd ${P_WORKDIR}/tapi-src && ${TOP}/mk/scripts/tapi-modernize-llvm-api.sh
# Patches come last, so they are written against the post-script tree.
# These are the changes the scripts cannot honestly express: real edits
# rather than renames.  bmake does not glob in .for, hence the != here.
.for pt in ${TAPI_PATCHES}
	@cd ${P_WORKDIR}/tapi-src && patch -s -p1 --forward < ${pt} && \
		${ECHO} "  applied ${pt:T}"
.endfor
	@touch ${.TARGET}

${P_WORKDIR}/.configured: ${P_WORKDIR}/tapi-src/.patched
