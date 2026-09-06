# NVIDIA Texture Tools, for BC6H and BC7.
#
# TextureConverter names NVTT as a back end for BC1 through BC7 and for the
# ETC family.  What this tree wants from it is the pair nothing else here
# provides: BC6H, the HDR block format, and BC7, the high quality RGBA one.
# stb covers BC1/BC3/BC4/BC5 and etc2comp covers ETC2/EAC, so only src/bc6h
# and src/bc7 are built, with the nvcore and nvmath they lean on.
#
# MIT for the core, Apache-2.0 for the two encoders (nVidia), BSD for
# poshlib.  extern/pvrtextool is deliberately not built and not staged: it
# is Imagination's PVRTexTool shipped as prebuilt x86 libraries, which is
# neither source, nor arm64, nor a licence this tree can carry.
#
# NVTT 2.1.2 predates Apple Silicon and does not build on it unmodified.
# Two defines settle it from the command line, which is configuration
# rather than patching -- submodule contents are left alone:
#
#   NV_CPU_AARCH64  nvcore/Debug.h tests this for 64-bit pointer checks,
#                   but nvcore.h only ever defines NV_CPU_ARM_64, so the
#                   32-bit branch was being taken and its casts truncate.
#   NV_CPU_ARM      the crash handler in Debug.cpp knows PPC, x86, x86_64
#                   and 32-bit ARM and reaches #error "Unknown CPU"
#                   otherwise.  The ARM branch reads uc_mcontext->__ss.__pc,
#                   which is the right field on arm64 macOS too.
#
# nvmath/PackedFloat.cpp is left out because it does not compile as
# released -- it names an identifier it never declares -- and nothing here
# calls it.  nvconfig.h is generated rather than configured, being five
# feature macros that cmake would set the same way on this platform.
P_BUILDSYS=	make
P_MAKE=		sh -c
P_MAKE_ARGS=	'set -e; \
		 mkdir -p dest/lib dest/include; \
		 printf "%s\n" "\#ifndef NV_CONFIG" "\#define NV_CONFIG" \
		     "\#define HAVE_UNISTD_H" "\#define HAVE_STDARG_H" \
		     "\#define HAVE_SIGNAL_H" "\#define HAVE_EXECINFO_H" \
		     "\#define HAVE_DISPATCH_H" "\#define NV_HAVE_STBIMAGE" \
		     "\#endif" > dest/include/nvconfig.h; \
		 srcs=`ls src/nvcore/*.cpp src/nvmath/*.cpp src/bc6h/*.cpp \
		     src/bc7/*.cpp | grep -v PackedFloat`; \
		 c++ -std=c++11 -O2 -DNV_CPU_AARCH64=1 -DNV_CPU_ARM=1 \
		     -Idest/include -Isrc -Iextern/poshlib -c $$srcs; \
		 ar rcs dest/lib/libnvtt-bc.a *.o; \
		 for d in nvcore nvmath bc6h bc7; do \
		     mkdir -p dest/include/$$d; \
		     cp -f src/$$d/*.h dest/include/$$d/; \
		     cp -f src/$$d/*.inl dest/include/$$d/ 2>/dev/null || true; \
		 done; \
		 cp -f extern/poshlib/posh.h dest/include/'
P_NOSTAGE=	yes
P_PROGS=

P_RELEASE_MERGE=	dest/lib usr/local/lib \
			dest/include usr/local/include
