# ldid -- Mach-O code signing.
#
# Signs and inspects Mach-O signatures, which is the same work
# codesign_allocate and our own codesign do, so it belongs in the
# toolchain beside them rather than in usr/local.
#
# Its Makefile builds and installs by itself; there is no configure.
P_BUILDSYS=	make
P_MAKE_ARGS=	PREFIX=${P_PREFIX}
