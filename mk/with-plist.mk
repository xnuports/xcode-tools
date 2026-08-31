# mk/with-plist.mk
#
# Shared fragment: build against src/openxc-tools/openxc/common/plist.c, our property
# list parser.  Used by anything that has to read Apple's plist-based
# metadata -- SDKSettings.plist, ToolchainInfo.plist, Info.plist.

T_CFLAGS+=	-I${TOP}/src/openxc-tools/openxc/common
T_SRCS+=	src/openxc-tools/openxc/common/plist.c
T_SRCS+=	src/openxc-tools/openxc/common/xmlplist.c
T_SRCS+=	src/openxc-tools/openxc/common/bplist.c
