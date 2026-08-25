# hdxml2manxml -- headerdoc's XML-to-man-page converter.
#
# xmlman/ holds three separate programs (hdxml2manxml, xml2man,
# resolveLinks) plus strcompat.c, so the source list is pinned.
#
# strcompat.c is deliberately excluded: it defines strlcpy/strlcat, and
# Apple's own xmlman/Makefile only compiles it on Linux
# (COMPATIBILITY_BITS is empty on Darwin, where libc already provides
# them).  Building it here collides with the SDK's fortified builtins.
T_SRCS=	hdxml2manxml.c

SDKROOT_PATH!=	xcrun --show-sdk-path 2>/dev/null || echo /
T_CFLAGS+=	-I${SDKROOT_PATH}/usr/include/libxml2
T_LDADD+=	-lxml2
