# textutil -- converts between the text formats Cocoa knows.  WebKit is
# there for the HTML reader and writer, which is what NSAttributedString
# hands that work to.
T_SRCS=		textutil.m TextutilWebDelegate.m
T_CFLAGS+=	-fobjc-arc
T_LDADD+=	-framework Foundation
T_LDADD+=	-framework CoreFoundation
T_LDADD+=	-framework AppKit
T_LDADD+=	-framework WebKit
T_LDADD+=	-lobjc
