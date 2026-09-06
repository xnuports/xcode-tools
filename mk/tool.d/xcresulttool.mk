# xcresulttool -- reads a .xcresult bundle: Info.plist for the root object,
# then zstd-compressed objects out of Data/.
.include "${TOP}/mk/with-plist.mk"

T_SRCS+=	xcresulttool.c
T_CFLAGS+=	-I${TOP}/build/release/usr/local/include
T_LDADD+=	${TOP}/build/release/usr/local/lib/libzstd.a
