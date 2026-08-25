# codesign -- ad-hoc signing and verification.
# OpenSSL for the digest work; Security/CoreFoundation for keychain and
# CFData handling in cs_file.c.
.include "${TOP}/mk/with-openssl.mk"
T_LDADD+=	-framework Security -framework CoreFoundation
