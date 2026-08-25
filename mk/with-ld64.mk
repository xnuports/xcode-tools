# mk/with-ld64.mk
#
# Shared fragment for programs built out of src/dist-dev-tools/ld64.
# Translates ld64.xcodeproj's build settings, plus the generated headers
# its Xcode script phases produce (see mk/tool.d/ld.mk).
#
#	.include "${TOP}/mk/with-ld64.mk"

.if !defined(_WITH_LD64_MK)
_WITH_LD64_MK=	yes

LD64=		${TOP}/src/dist-dev-tools/ld64
LD64_SRC=	${LD64}/src
LD64_GEN=	${TOP}/build/gen/ld64

# ld64 uses std::string_view::starts_with, which is C++20.  C++ only:
# ld64 also has two C sources (debugline.c, libcodedirectory.c).
T_CXXFLAGS+=	-std=c++20

# Generated headers (configure.h, compile_stubs.h) and the targeted dyld
# shim, both produced by rules in mk/tool.d/ld.mk.  The shim directory
# must precede cctools' include path: cctools ships its own stripped
# mach-o/dyld_priv.h with no dyld_unwind_sections, and would otherwise
# win the search.  Exposing all of lib/dyld/include instead is not an
# option -- it also carries dlfcn.h and mach-o/dyld.h, which shadow the
# system and cctools headers respectively.
T_CFLAGS+=	-I${LD64_GEN} -I${LD64_GEN}/dyldshim -I${LD64_GEN}/osshim

T_CFLAGS+=	-I${LD64_SRC}/ld -I${LD64_SRC}/abstraction -I${LD64_SRC}/mach_o \
		-I${LD64_SRC}/ld/parsers -I${LD64_SRC}/ld/passes \
		-I${LD64_SRC}/ld/code-sign-blobs

# ld64's private-header dependencies, all carried as submodules:
#   include/ld-internals   Apple linker internals (Atom.h, Options.h, ...)
#   lib/libplatform        os/lock_private.h, for ld.cpp's assert lock
#   lib/corecrypto         ccdigest/ccsha1/ccsha2, for code directories
T_CFLAGS+=	-I${TOP}/include/ld-internals
T_CFLAGS+=	-I${TOP}/lib/libplatform/private

# lib/apple_internal_sdk carries the private headers that ship in neither
# the public SDK nor any open-source drop:
#
#   CrashReporterClient.h              Options.cpp (CRSetCrashLogMessage)
#   os/base_private.h                  reached via libplatform's lock_private.h
#   System/machine/cpu_capabilities.h  OutputFile.cpp, for Rosetta detection
#   Private/CommonCrypto/CommonDigestSPI.h  LinkEdit.hpp
#
# usr/include/System is needed as well: the non-KERNEL_PRIVATE branch of
# machine/cpu_capabilities.h reaches for <arm/cpu_capabilities.h>.
# Private/ is kept separate so its CommonCrypto does not shadow the SDK's.
AISDK=		${TOP}/lib/apple_internal_sdk/usr/include
T_CFLAGS+=	-I${AISDK} -I${AISDK}/System -I${AISDK}/Private

# corecrypto keeps its headers in a corecrypto/ directory under each
# module, so every module directory has to go on the search path.
CORECRYPTO_INC!=	find ${TOP}/lib/corecrypto -maxdepth 2 -type d -name corecrypto \
			-not -path '${TOP}/lib/corecrypto' 2>/dev/null \
			| sed 's|/corecrypto$$||' | sed 's|^|-I|' | tr '\n' ' '
T_CFLAGS+=	${CORECRYPTO_INC}

# bitcode_bundle.cpp wraps bitcode in a xar archive; the SDK ships
# libxar as a .tbd.
T_LDADD+=	-lxar

# libstuff and the cctools/tapi/llvm headers ld64 parses Mach-O with.
T_CFLAGS+=	-I${TOP}/src/dist-dev-tools/tapi/include
.include "${TOP}/mk/with-libstuff.mk"

.endif # _WITH_LD64_MK
