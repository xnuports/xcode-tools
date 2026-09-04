# mk/with-devpath.mk
#
# Shared fragment: build against src/openxc-tools/common/devpath.c, which
# derives the Developer directory from the running binary's own location
# so the release tree stays relocatable.  Include from any tool that
# needs to find its own Developer directory.

T_CFLAGS+=	-I${TOP}/src/openxc-tools/common
T_SRCS+=	src/openxc-tools/common/devpath.c
