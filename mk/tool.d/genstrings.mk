# genstrings -- scans source for localization macros and writes .strings.
# Apple also install it as extractLocStrings; mk/bundle.mk makes that link.
T_SRCS=		genstrings.m
T_CFLAGS+=	-fobjc-arc
T_LDADD+=	-framework Foundation
