# xar, from Apple's distribution.
#
# /usr/bin/xar -- the archive format a .pkg is.  The drop carries
# upstream's tree under xar/ with a configure already generated, so
# unlike libxml2 and libxslt there is nothing to autoreconf.
P_CONFIGURE=	xar/configure

# Out of tree, for the same reason libxslt is: the source sits at
# src/xar and the default object directory is src, so configure's own
# xar subdirectory would collide with it.
P_OBJDIR=	${P_WORKDIR}/build

# xar reads its table of contents with libxml2.  xml2-config comes from
# the libxml2 this tree builds, so the version it compiles against is
# the one here rather than whatever the machine has.
LIBXML2_STAGE=	${TOP}/build/ports/libxml2/stage/usr

# Against the internal SDK: archive.h includes
# <CommonCrypto/CommonDigestSPI.h>, which is SPI and lives only there.
INTERNAL_SDK=	${TOP}/build/release/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.Internal.sdk

# xml2-config reports the install prefix, so it hands configure a bare
# -I/usr/include/libxml2.  An absolute -I is not rewritten by
# -isysroot, and this system has no real /usr/include, so the header is
# not found; -iwithsysroot is the spelling that looks inside the SDK,
# and is what Apple's own builds use.
# In CPPFLAGS, not CFLAGS.  lib/Makefile.inc generates dependencies
# with "$(CC) -MM $(CPPFLAGS)" and no $(CFLAGS), under sh -ec, so an
# -isysroot given as a CFLAG is absent from the dependency pass: the
# compile finds CommonCrypto/CommonDigestSPI.h and the scan that runs
# beside it does not, and the build stops on a header it can plainly
# reach.
# src/Makefile.inc adds no include paths of its own and the top-level
# Makefile takes them entirely from configure, so nothing tells the
# src/ objects where lib/ is -- xar.c includes filetree.h, which lives
# there.  Apple build this with Xcode, whose header search paths cover
# it; the autoconf path has to be told.
XAR_SRC=	${TOP}/build/ports/xar/src/xar

P_CONFIGURE_ARGS=	CPPFLAGS=-isysroot\ ${INTERNAL_SDK}\ -iwithsysroot\ /usr/include/libxml2\ -I${XAR_SRC}/lib \
			CFLAGS=-isysroot\ ${INTERNAL_SDK} \
			--with-xml2-config=${LIBXML2_STAGE}/bin/xml2-config

# xar's sources include <xar/xar.h>, the installed spelling, and the
# build tree has no such directory.  See the script.
P_MAKE=		${TOP}/mk/scripts/xar-make.sh

P_PROGS=	bin/xar
