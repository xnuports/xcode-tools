# pl -- reads a property list on stdin and writes it back as text.
# Foundation is all it needs.
T_SRCS=		pl.m
T_CFLAGS+=	-fobjc-arc
T_LDADD+=	-framework Foundation
T_LDADD+=	-lobjc
