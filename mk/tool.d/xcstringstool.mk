# xcstringstool -- string catalogs.
# T_SRCS is explicit because json.c lives outside this directory.
T_SRCS=	xcstringstool.c xs_compile.c
.include "${TOP}/mk/with-json.mk"
