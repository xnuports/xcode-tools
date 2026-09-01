# mk/with-json.mk
#
# Shared fragment: build against src/openxc-tools/openxc/common/json.c,
# a JSON document model.  Anything reading a format Apple describes in
# JSON -- string catalogs, and the result bundles later -- wants a whole
# tree rather than the key-at-a-time extraction notarytool carries.

T_CFLAGS+=	-I${TOP}/src/openxc-tools/openxc/common
T_SRCS+=	src/openxc-tools/openxc/common/json.c
