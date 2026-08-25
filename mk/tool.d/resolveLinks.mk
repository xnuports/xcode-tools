# resolveLinks -- headerdoc's cross-reference resolver.  Shipped by Xcode
# in Developer/usr/bin, despite docs listing it as Apple-internal; the
# source is right here in headerdoc's xmlman/.
#
# Same directory, same pinning and libxml2 dependency as hdxml2manxml.
T_SRCS=	resolveLinks.c

SDKROOT_PATH!=	xcrun --show-sdk-path 2>/dev/null || echo /
T_CFLAGS+=	-I${SDKROOT_PATH}/usr/include/libxml2
T_LDADD+=	-lxml2
