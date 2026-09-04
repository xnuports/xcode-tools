# pcre2, for Apple's git.
#
# Apple's git links PCRE2 statically and looks for it in a specific
# place: its Makefile passes -L$(SDKROOT)/usr/local/lib/pcre2-static,
# so the library belongs in the internal SDK rather than in the
# release tree's usr/lib.  That is also what keeps the build honest --
# without it the link falls through to whatever libpcre2 happens to be
# installed on the machine, and on this one that is Homebrew's, which
# then fails to load at runtime over a Team ID mismatch.
#
# Static only.  A shared build would leave git with a dependency on a
# dylib this tree does not install anywhere.
#
# PCRE2 is upstream's rather than Apple's -- Apple publishes no pcre2 --
# and is pinned to a release tag, as Go is.  BSD-licensed.
P_BUILDSYS=	cmake

P_CONFIGURE_ARGS=	-DBUILD_SHARED_LIBS=OFF \
		-DPCRE2_BUILD_PCRE2_8=ON \
		-DPCRE2_BUILD_PCRE2_16=OFF \
		-DPCRE2_BUILD_PCRE2_32=OFF \
		-DPCRE2_BUILD_TESTS=OFF \
		-DPCRE2_BUILD_PCRE2GREP=OFF \
		-DPCRE2_SUPPORT_JIT=ON \
		-DCMAKE_POSITION_INDEPENDENT_CODE=ON

# A library, so nothing lands in usr/bin.
P_PROGS=

# lib/ becomes the pcre2-static directory Apple's Makefile names, and
# the headers go where the compiler already looks inside the SDK.
P_RELEASE_MERGE=	lib Platforms/MacOSX.platform/Developer/SDKs/MacOSX.Internal.sdk/usr/local/lib/pcre2-static \
			include Platforms/MacOSX.platform/Developer/SDKs/MacOSX.Internal.sdk/usr/local/include
