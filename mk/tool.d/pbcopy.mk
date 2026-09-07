# pbcopy -- and pbpaste, which macOS ships as a second name for the same
# binary and which this links the same way.  AppKit owns the pasteboard.
T_SRCS=		pbcopy.m
T_CFLAGS+=	-fobjc-arc
T_LDADD+=	-framework Foundation
T_LDADD+=	-framework CoreFoundation
T_LDADD+=	-framework AppKit
T_LDADD+=	-lobjc
T_LINKS=	pbpaste
