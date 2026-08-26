# mk/with-sdkpath.mk
#
# Shared fragment: build against src/xcode/common/sdkpath.c, which knows
# where SDKs and toolchains live inside a Developer directory.  Implies
# the plist parser, since the Apple layout describes them with plists.

.include "${TOP}/mk/with-plist.mk"

T_SRCS+=	src/xcode/common/sdkpath.c
