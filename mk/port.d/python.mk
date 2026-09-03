# CPython.
#
# Xcode ships python3 at Developer/usr/bin/python3, so that is where
# this goes.  The version does not match: Apple's is 3.9.6, the old
# shim they have carried for years, and the checkout here is 3.14.6.
# Building current is the right call for a tree that is otherwise
# tracking the present, and it is worth knowing the two differ.
#
# python3 is useless without its standard library, so the lib tree is
# staged beside the binary.  Merged rather than replacing, because
# usr/lib is shared.
#
# --without-ensurepip keeps pip out of the build: it wants the network
# at install time, and a toolchain that reaches out while being built
# is not one you can build twice and get the same answer.
P_CONFIGURE_ARGS=	--without-ensurepip \
			--enable-shared=no
P_PROGS=		bin/python3
P_RELEASE_MERGE=	lib usr/lib
