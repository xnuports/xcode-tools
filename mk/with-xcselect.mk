# mk/with-xcselect.mk
#
# Shared fragment: link libxcselect, as Apple's xcrun and xcode-select
# do.  The rpath is relative to the executable so the release tree can
# be moved without the tools losing the library.

T_CFLAGS+=	-I${TOP}/src/openxc-tools/libxcselect
T_LDADD+=	-L${TOP}/build/release/usr/lib -lxcselect \
		-Wl,-rpath,@executable_path/../lib
