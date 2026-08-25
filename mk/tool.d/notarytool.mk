# notarytool -- notarization client.
# OpenSSL signs the App Store Connect JWT; libcurl talks to the API.
.include "${TOP}/mk/with-openssl.mk"
T_LDADD+=	-lcurl -framework Security -framework CoreFoundation
