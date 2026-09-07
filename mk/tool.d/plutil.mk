# plutil -- the property-list utility from /usr/bin.  Four sources: the
# driver and the context object that does the conversions, plus the two
# mutable-collection subclasses it builds trees out of.
T_SRCS=		main.m PLUContext.m PLUMutableArray.m PLUMutableDictionary.m
T_CFLAGS+=	-fobjc-arc
T_LDADD+=	-framework Foundation
T_LDADD+=	-framework CoreFoundation
T_LDADD+=	-lobjc
