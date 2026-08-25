# productbuild -- product archive creation.
# Shares xar/bom/payload with pkgbuild (hence -lz); OpenSSL and Security
# for --sign.
.include "${TOP}/mk/with-openssl.mk"
T_LDADD+=	-lz -framework Security
