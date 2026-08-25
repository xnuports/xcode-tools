# xml2man -- headerdoc's man-page generator, same directory and the same
# libxml2 dependency as hdxml2manxml.
T_SRCS=	xml2man.c

SDKROOT_PATH!=	xcrun --show-sdk-path 2>/dev/null || echo /
T_CFLAGS+=	-I${SDKROOT_PATH}/usr/include/libxml2
T_LDADD+=	-lxml2
