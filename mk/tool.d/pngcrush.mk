# pngcrush -- bundles its own libpng, and a partial copy of zlib.
#
# The bundled zlib is incomplete: gzguts.h and every gz*.c are missing,
# so zutil.c will not compile as shipped.  zlib's own switch for a build
# without the gz layer, -DZ_SOLO, does make it compile but then drops
# zcalloc/zcfree, which deflate and inflate need.  Rather than patch a
# submodule or hand-write zlib internals, link the system zlib instead --
# a configuration pngcrush's own Makefile supports (its commented-out
# "LIBS = -lz" line) and one that also resolves the "bundled rather than
# system zlib" concern in docs/DOCUMENTATION.md section 4.
#
# So: build pngcrush and libpng from the tree, and take zlib from the
# system.  filter_sse2_intrinsics.c is excluded as x86-only.
T_SRCS=	pngcrush.c \
	png.c pngerror.c pngget.c pngmem.c pngpread.c pngread.c pngrio.c \
	pngrtran.c pngrutil.c pngset.c pngtrans.c pngwio.c pngwrite.c \
	pngwtran.c pngwutil.c

# libpng's pngpriv.h reaches for <fp.h> -- the Classic Mac OS math header
# -- because TARGET_OS_MAC is defined on modern macOS too.  Its guard
# skips that include when <math.h> has already been seen, and macOS's
# math.h defines the __MATH_H__ the guard tests, so force-including it is
# the fix the header itself anticipates.
T_CFLAGS+=	-include math.h

# pngrutil.c calls png_init_filter_functions_neon, but this copy of
# libpng ships no arm/ directory to define it.  Turn the NEON path off.
T_CFLAGS+=	-DPNG_ARM_NEON_OPT=0

T_LDADD+=	-lz -lm

# Note: pngcrush -version reports the bundled zlib.h's version string
# (1.2.8), not the system libz it actually links against.  libpng
# includes "zlib.h" unquoted-local, so the in-tree header wins.  zlib
# has kept its ABI stable across 1.2.x, so this pairing is safe.
