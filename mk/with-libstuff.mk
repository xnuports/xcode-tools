# mk/with-libstuff.mk
#
# Shared fragment: link against cctools' libstuff, built by lib/Makefile.
# Implies mk/with-cctools.mk, since every libstuff consumer is a cctools
# program and needs the same include paths and defines.

.include "${TOP}/mk/with-cctools.mk"

T_LDADD+=	${TOP}/build/lib/libstuff.a
