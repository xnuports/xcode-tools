# xarsigner -- rewrites a xar table of contents around a detached signature.
# NSXMLDocument writes the table back out, which is what makes the result
# byte for byte Apple's; libz packs it, as xar has always done.
T_SRCS=		xarsigner.m
T_CFLAGS+=	-fobjc-arc
T_LDADD+=	-framework Foundation
T_LDADD+=	-lz
