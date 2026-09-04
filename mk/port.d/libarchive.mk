# libarchive, from Apple's distribution.
#
# macOS's /usr/bin/tar and /usr/bin/cpio are this: both report
# "bsdtar 3.5.3 - libarchive 3.7.4" and "bsdcpio ...".  Apple's drop is
# their Xcode project wrapped around upstream's tree, and upstream's
# tree is what gets built here -- the xcodeproj drives a build this
# tree has no way to run, and the sources under libarchive/ are
# complete, with their own CMakeLists.txt.
P_BUILDSYS=	cmake
P_CMAKE_SRC=	libarchive

# Apple's own sources are listed in their xcodeproj and nowhere else,
# so upstream's CMakeLists does not build them, and teaching it means
# editing it.  P_COPY gives this port a private copy to edit -- cmake
# ports default to building beside a read-only submodule, and this one
# must not be the exception that writes into src/.
P_COPY=		yes
P_PREPARE=	${TOP}/mk/scripts/prepare-apple-libarchive.sh .

# What the SDK can supply.  zlib, bz2 and iconv are all there with
# headers and stubs.
#
# libb2 is off deliberately.  The option defaults on and finds whatever
# BLAKE2 is installed -- on this machine Homebrew's, which the binary
# then carries a dependency on.  Apple's tar links no libb2; the tree
# has its own archive_blake2s_ref.c and that is what gets used.
#
# lzma is not, and cannot be: Apple ships liblzma in the shared cache
# but publishes neither lzma.h nor any xz source, so their own public
# SDK cannot build against it either.  Apple's tar reports
# liblzma/5.4.3; ours will not, until there is a header to compile
# against.  Everything else matches.
# The tree's cmake_minimum_required predates this cmake, which refuses
# the old policy set outright and names this flag as the way through.
# Against the internal SDK: Apple's additions to this tree include
# os/variant_private.h and quarantine.h, both SPI and neither in the
# public SDK.
INTERNAL_SDK=	${TOP}/build/release/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.Internal.sdk

P_CONFIGURE_ARGS=	-DCMAKE_OSX_SYSROOT=${INTERNAL_SDK} \
		-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
		-DENABLE_ZLIB=ON \
		-DENABLE_BZip2=ON \
		-DENABLE_ICONV=ON \
		-DENABLE_LZMA=OFF \
		-DENABLE_LZ4=OFF \
		-DENABLE_ZSTD=OFF \
		-DENABLE_LIBXML2=OFF \
		-DENABLE_EXPAT=ON \
		-DENABLE_LIBB2=OFF \
		-DENABLE_OPENSSL=OFF \
		-DENABLE_TEST=OFF \
		-DENABLE_CAT=ON \
		-DENABLE_TAR=ON \
		-DENABLE_CPIO=ON \
		-DBUILD_SHARED_LIBS=OFF

P_PROGS=	bin/bsdtar \
		bin/bsdcpio \
		bin/bsdcat

# Apple installs them under the names the system uses.
P_RELEASE_SYMLINK=	usr/bin/bsdtar usr/bin/tar \
			usr/bin/bsdcpio usr/bin/cpio
