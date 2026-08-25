# xcrun -- tool locator and executor.
# T_SRCS is explicit because devpath.c lives outside this directory.
T_SRCS=	ini.c xcrun.c
.include "${TOP}/mk/with-devpath.mk"
