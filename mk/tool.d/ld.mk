# ld -- Apple's Mach-O linker (ld64).
#
# NOT YET IN mk/progs.mk.  33 of ld64's 36 sources compile; the last
# three need two headers that are in none of our submodules:
#
#   os/base_private.h      pulled in by libplatform's os/lock_private.h,
#                          which ld.cpp and OutputFile.cpp need.  It
#                          lives in libdispatch, not libplatform.
#   CrashReporterClient.h  Options.cpp, for CRSetCrashLogMessage.  Note
#                          the include sits under
#                          "#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1070"
#                          and ld64 carries an #else branch that uses
#                          __crashreporter_info__ directly.
#
# Add the entry to mk/progs.mk once those are available:
#
#   PROGS+= dist-dev-tools/ld64 ld ${XCTOOLCHAIN}/usr/bin
#
# ld-classic is the same source built with the classic code path; a
# stock toolchain ships it as its own binary.

.include "${TOP}/mk/with-ld64.mk"

T_SRCS!=	cd ${TOP} && ls src/dist-dev-tools/ld64/src/ld/*.cpp \
		    src/dist-dev-tools/ld64/src/ld/parsers/*.cpp \
		    src/dist-dev-tools/ld64/src/ld/passes/*.cpp \
		    src/dist-dev-tools/ld64/src/mach_o/*.cpp 2>/dev/null

# --- generated headers ------------------------------------------------
#
# ld64.xcodeproj produces these with script phases; we reproduce them.

# configure.h, from src/create_configure.  That script emits
# SUPPORT_ARCH_riscv32 0 whenever TOOLCHAIN_INSTALL_DIR matches
# XcodeDefault -- which is exactly the toolchain we build -- so riscv32
# is off here too.
${LD64_GEN}/configure.h:
	@mkdir -p ${LD64_GEN}
	@{ \
	  for a in i386 x86_64 x86_64h armv6 armv7 armv7s armv7m armv7em \
		   armv7k arm64 arm64e arm64_32 armv8m; do \
	    echo "#define SUPPORT_ARCH_$$a 1"; \
	  done; \
	  echo '#define SUPPORT_ARCH_riscv32 0'; \
	  echo '#define ALL_SUPPORTED_ARCHS "i386 x86_64 x86_64h armv6 armv7 armv7s armv7m armv7em armv7k arm64 arm64e arm64_32 armv8m.main armv8.1m.main"'; \
	  echo '#define BITCODE_XAR_VERSION "1.0"'; \
	  echo '#define LD64_VERSION_NUM ${LD64_VERSION}'; \
	  echo '#define LD_PAGE_SIZE 0x1000'; \
	} > ${.TARGET}

LD64_VERSION?=	956.6

# compile_stubs.h wraps the compile_stubs csh script as a C string,
# exactly as the xcodeproj's script phase does.
${LD64_GEN}/compile_stubs.h: ${LD64}/compile_stubs
	@mkdir -p ${LD64_GEN}
	@echo 'static const char *compile_stubs = ' > ${.TARGET}
	@sed 's/"/\\"/g; s/^/"/; s/$$/\\n"/' ${LD64}/compile_stubs >> ${.TARGET}
	@echo ';' >> ${.TARGET}

# A shim exposing only mach-o/dyld_priv.h from lib/dyld, so that dyld's
# other headers do not shadow the system's dlfcn.h or cctools'
# mach-o/dyld.h.  dyld's header annotates with bridgeos(), a platform the
# public SDK's availability macros do not know, so the annotations are
# neutered before the include -- irrelevant for a build tool.
${LD64_GEN}/dyldshim/mach-o/dyld_priv.h:
	@mkdir -p ${LD64_GEN}/dyldshim/mach-o
	@{ \
	  echo '#include <Availability.h>'; \
	  for m in __API_AVAILABLE __API_DEPRECATED \
		   __API_DEPRECATED_WITH_REPLACEMENT __API_UNAVAILABLE; do \
	    echo "#undef $$m"; echo "#define $$m(...)"; \
	  done; \
	  echo '#include "${TOP}/lib/dyld/include/mach-o/dyld_priv.h"'; \
	} > ${.TARGET}

# Every object depends on the generated headers.
.for s in ${T_SRCS}
${T_OBJDIR}/${s:T:R}.o: ${LD64_GEN}/configure.h ${LD64_GEN}/compile_stubs.h \
			${LD64_GEN}/dyldshim/mach-o/dyld_priv.h
.endfor
