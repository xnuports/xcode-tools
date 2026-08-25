# mk/with-libmacho.mk
#
# Shared fragment: link against cctools' libmacho, built by lib/Makefile.

.include "${TOP}/mk/with-cctools.mk"

T_LDADD+=	${TOP}/build/lib/libmacho.a
