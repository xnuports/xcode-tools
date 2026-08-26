# mk/with-plist.mk
#
# Shared fragment: build against src/xcode/common/plist.c, our property
# list parser.  Used by anything that has to read Apple's plist-based
# metadata -- SDKSettings.plist, ToolchainInfo.plist, Info.plist.

T_CFLAGS+=	-I${TOP}/src/xcode/common
T_SRCS+=	src/xcode/common/plist.c
T_SRCS+=	src/xcode/common/xmlplist.c
T_SRCS+=	src/xcode/common/bplist.c
