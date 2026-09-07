# tiff2icns -- writes an .icns from a TIFF.  NSImage reads and the icon
# services write, so AppKit and ApplicationServices both come in.
T_SRCS=		tiff2icns.m
T_CFLAGS+=	-fobjc-arc
T_LDADD+=	-framework Foundation
T_LDADD+=	-framework CoreFoundation
T_LDADD+=	-framework AppKit
T_LDADD+=	-framework ApplicationServices
T_LDADD+=	-framework CoreServices
T_LDADD+=	-lobjc
