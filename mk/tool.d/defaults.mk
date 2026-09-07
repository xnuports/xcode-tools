# defaults -- stock macOS ships this at /usr/bin/defaults.  Foundation does
# the property-list and preferences work, which is the whole program; the
# submodule's own Makefile links the same three.
T_SRCS=		defaults.m
T_CFLAGS+=	-fobjc-arc
T_LDADD+=	-framework Foundation
T_LDADD+=	-framework CoreFoundation
T_LDADD+=	-lobjc
