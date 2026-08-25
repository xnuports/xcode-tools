# mk/with-openssl.mk
#
# Shared fragment: include from a tool's mk/tool.d/<prog>.mk to build
# against OpenSSL.  Used by codesign (CodeDirectory hashing), notarytool
# (JWT signing) and productbuild (package signing).
#
#	.include "${TOP}/mk/with-openssl.mk"

OPENSSL_CFLAGS!=	pkg-config --cflags openssl 2>/dev/null || true
OPENSSL_LDFLAGS!=	pkg-config --libs openssl 2>/dev/null || true

T_CFLAGS+=	${OPENSSL_CFLAGS}
T_LDADD+=	${OPENSSL_LDFLAGS}
