# NVIDIA Texture Tools, for BC6H and BC7.
#
# TextureConverter names NVTT as a back end for BC1 through BC7 and for the
# ETC family.  What this tree wants from it is the pair nothing else here
# provides -- BC6H, the HDR block format, and BC7, the high quality RGBA
# one -- and, it turns out, its filters.
#
# Apple's mip chains are NVTT's.  Feeding their tool an impulse and reading
# the level it produced gives weights of 0.006729, 0.013252, -0.033884,
# -0.054839, 0.139929 and 0.428814, and NVTT's PolyphaseKernel over a
# KaiserFilter of width 3 and alpha 4, box-sampled 32 times per tap, gives
# exactly those six numbers.  So nvimage's Filter and FloatImage are built
# too, and the mip chains here are bit for bit the ones Apple writes rather
# than a second implementation that has to be argued about.
#
# All of src/nvtt is built, not just the two block encoders.  The reason is
# what Apple's tool actually does: asked for any of BC1 through BC7 with the
# default --compressor=Auto, it prints "Using Compressor: NVTT".  stb and
# ISPC are reachable only by naming them, so matching Apple on the BC family
# means going through NVTT's own compressor, not a second implementation of
# the same formats.
#
# That pulls in src/nvthread, which Context uses to spread blocks across
# cores, and six files of the bundled squish.  Six and not eleven: the rest
# of that directory -- alpha.cpp, clusterfit.cpp, rangefit.cpp,
# singlecolourfit.cpp, squish.cpp -- declares "namespace squish" while
# squish.h declares "namespace nvsquish", so it does not compile, and NVTT's
# own CMakeLists does not build it either.  The six here are exactly its
# SQUISH_SRCS list.
#
# The two .cpp files under cuda/ come along even with no CUDA anywhere:
# without HAVE_CUDA they compile to the stubs that say there is no hardware,
# and Context.cpp constructs a CudaContext and asks unconditionally.  The .cu
# kernels beside them are not built and there is nothing here to build them
# with.
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
# Two sources are left out, each because it does not compile as released:
# nvmath/PackedFloat.cpp and nvimage/ColorSpace.cpp both name an identifier
# they never declare.  nvimage/ImageIO.cpp does build, given NVTT's own
# bundled stb on the include path, and has to: FloatImage calls into Image,
# Image calls into ImageIO, and dropping either leaves a symbol missing from
# anything that links this at all.  nvconfig.h is generated rather than configured, being five
# feature macros that cmake would set the same way on this platform.
P_BUILDSYS=	make
P_MAKE=		sh -c
P_MAKE_ARGS=	'set -e; \
		 rm -rf dest gen; rm -f *.o; \
		 mkdir -p dest/lib dest/include gen; \
		 printf "%s\n" "\#ifndef NV_CONFIG" "\#define NV_CONFIG" \
		     "\#define HAVE_UNISTD_H" "\#define HAVE_STDARG_H" \
		     "\#define HAVE_SIGNAL_H" "\#define HAVE_EXECINFO_H" \
		     "\#define HAVE_DISPATCH_H" "\#define NV_HAVE_STBIMAGE" \
		     "\#endif" > gen/nvconfig.h; \
		 srcs=`ls src/nvcore/*.cpp src/nvmath/*.cpp src/bc6h/*.cpp \
		     src/bc7/*.cpp src/nvimage/*.cpp src/nvthread/*.cpp \
		     src/nvtt/*.cpp src/nvtt/cuda/*.cpp | \
		     grep -vE "PackedFloat|nvimage/ColorSpace|nvtt_wrapper"`; \
		 for f in fastclusterfit weightedclusterfit colourblock \
		     colourfit colourset maths; do \
		     srcs="$$srcs src/nvtt/squish/$$f.cpp"; \
		 done; \
		 c++ -std=c++11 -O2 -DNV_CPU_AARCH64=1 -DNV_CPU_ARM=1 \
		     -Igen -Isrc -Iextern/poshlib -Iextern/stb -c $$srcs; \
		 ar rcs dest/lib/libnvtt-bc.a *.o; \
		 for d in nvcore nvmath nvimage bc6h bc7 nvthread nvtt; do \
		     mkdir -p dest/include/$$d; \
		     cp -f src/$$d/*.h dest/include/$$d/; \
		     cp -f src/$$d/*.inl dest/include/$$d/ 2>/dev/null || true; \
		 done; \
		 cp -f extern/poshlib/posh.h gen/nvconfig.h dest/include/'
P_NOSTAGE=	yes
P_PROGS=

P_RELEASE_MERGE=	dest/lib usr/local/lib \
			dest/include usr/local/include
