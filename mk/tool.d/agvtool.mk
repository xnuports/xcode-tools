# agvtool -- edits the two version numbers in place, so it needs nothing
# beyond libc and libm's floor().
T_SRCS=		agvtool.c
T_LDADD+=	-lm
