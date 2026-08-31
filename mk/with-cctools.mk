# mk/with-cctools.mk
#
# Shared fragment: include from any mk/tool.d/<prog>.mk built out of
# src/apple-oss-distributions/distribution-Developer_Tools/cctools.  Supplies the include paths and defines
# that Apple's cctools.xcconfig / public_tool.xcconfig set, translated
# out of Xcode build settings.
#
#	.include "${TOP}/mk/with-cctools.mk"

# Guard: with-libstuff.mk and with-libmacho.mk both pull this in, and a
# tool that links both would otherwise get every flag twice.
.if !defined(_WITH_CCTOOLS_MK)
_WITH_CCTOOLS_MK=	yes

CCTOOLS=	${TOP}/src/apple-oss-distributions/distribution-Developer_Tools/cctools

# Xcode uses a recursive "include/**" header search.  Only the top level
# and stuff/ are actually needed; adding the rest (mach/, mach-o/) would
# shadow SDK headers rather than supplement them.
T_CFLAGS+=	-I${CCTOOLS}/include -I${CCTOOLS}/include/stuff

# libstuff's lto.c wants <llvm-c/lto.h>, which the cctools drop does not
# bundle (its include/llvm-c/ holds only Disassembler.h).  Take it from
# the llvm-project submodule we already carry.
T_CFLAGS+=	-I${TOP}/src/swiftlang-llvm/llvm-project/llvm/include

# Definitions cctools' sources reference but nothing in the drop or the
# public SDK provides -- see the header for the full account.
T_CFLAGS+=	-include ${TOP}/mk/cctools-compat.h

# cctools deliberately overrides some CoreOS headers; clang is loud
# about it.  Apple silences it the same way in cctools.xcconfig.
T_CFLAGS+=	-Wno-ambiguous-macro

# LTO_SUPPORT is set unconditionally by both libstuff.xcconfig and
# public_tool.xcconfig.  libLTO itself is dlopen'd at runtime.
T_CFLAGS+=	-DLTO_SUPPORT

# Stamped into libstuff/apple_version.c and printed by several tools on
# request.  Apple defaults this to "cctools-localbuild" outside their
# build system; we know the real tag from distribution-Developer_Tools/release.json.
#
# Taken from the submodule's own tag rather than written here or read
# from distribution-Developer_Tools' release.json: both go stale the moment the
# submodule is bumped, and release.json in particular lags its own
# nested checkouts.
CCTOOLS_VERSION!=	git -C ${CCTOOLS} describe --tags --abbrev=0 2>/dev/null || echo cctools-unknown
T_CFLAGS+=	-DCURRENT_PROJECT_VERSION=\"${CCTOOLS_VERSION}\"

# NOT set: CODEDIRECTORY_SUPPORT.  Apple enables it for macOS and Xcode
# builds, but it links ${TOOLCHAIN_DIR}/usr/lib/libcodedirectory.dylib,
# a closed Apple binary with no published source.  Depending on it would
# defeat the point of this project, so the re-signing paths it guards
# are compiled out.

.endif # _WITH_CCTOOLS_MK
