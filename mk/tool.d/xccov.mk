# xccov -- reads the coverage report out of a result bundle.  Shares the
# bundle reader with xcresulttool; the keyed-archive reader is its own,
# the shared plist parser rendering every scalar as a string.
.include "${TOP}/mk/with-plist.mk"

T_SRCS+=	xccov.c bkeyed.c
T_SRCS+=	src/openxc-tools/common/xcresult.c
T_CFLAGS+=	-I${TOP}/src/openxc-tools/common
T_CFLAGS+=	-I${TOP}/build/release/usr/local/include
T_LDADD+=	${TOP}/build/release/usr/local/lib/libzstd.a
