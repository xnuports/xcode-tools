# libxslt, from Apple's distribution.
#
# /usr/bin/xsltproc, and the libxslt and libexslt it reports.  As with
# libxml2 the drop is an Xcode project around upstream's tree, and
# upstream's tree under libxslt/ is what gets built.  It uses the old
# configure.in spelling, which autoreconf handles.
P_PREPARE=	cd libxslt && autoreconf -fi >/dev/null 2>&1

P_CONFIGURE=	libxslt/configure

# Built outside the copied tree.  The source sits at src/libxslt and the
# default object directory is src, so configure's recursion into its own
# libxslt subdirectory lands back on the source and finds no generated
# Makefile there.
P_OBJDIR=	${P_WORKDIR}/build

# Built against the libxml2 this tree just built, not whatever the
# machine happens to have -- the same reason Apple's git had to be kept
# off Homebrew's pcre2.
LIBXML2_PREFIX=	${TOP}/build/ports/libxml2/stage/usr

# extensions.c passes an xmlHashScanner whose xmlChar * argument the
# newer libxml2 header declares const.  clang made that class of
# mismatch an error rather than a warning; the call is harmless and
# Apple built this with a compiler that only warned, so the warning is
# turned back down rather than the source edited.
P_CONFIGURE_ARGS=	CFLAGS=-Wno-error=incompatible-function-pointer-types \
			--without-python \
			--without-crypto \
			--with-libxml-prefix=${LIBXML2_PREFIX} \
			--disable-shared \
			--enable-static

P_PROGS=	bin/xsltproc

P_RELEASE_MERGE=	include usr/local/include \
			lib usr/local/lib
