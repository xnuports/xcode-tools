# xcrun -- tool locator and executor.
# T_SRCS is explicit because devpath.c lives outside this directory.
T_SRCS=	ini.c xcrun.c
.include "${TOP}/mk/with-devpath.mk"
.include "${TOP}/mk/with-sdkpath.mk"

# Apple's xcrun links libxcselect and nothing else beyond libSystem;
# ours links it for the same reason -- see mk/with-xcselect.mk.
.include "${TOP}/mk/with-xcselect.mk"
