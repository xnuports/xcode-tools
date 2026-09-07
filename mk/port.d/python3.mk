# CPython.  Named for the port, which is python3: mk/port.mk includes
# mk/port.d/<P_NAME>.mk, and P_NAME is the second word of the mk/ports.mk
# entry, not the directory.
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
#
# Xcode ships the interpreter twice, as python3 and python3.<minor>, and
# pydoc alongside it the same way; the versioned name is the program and the
# bare one a symlink to it.  Same here, with our own minor number rather than
# a constant, read from cpython the way pip.mk reads it.  All on one line: a
# "!=" command is not continued across backslash-newlines, and the "#" of
# each #define is escaped or it would start a comment.
#
# Apple also ship 2to3 and 2to3-3.9.  There is nothing here to point those
# at -- 2to3 was removed from CPython in 3.13 -- so they are a port of their
# own, built from fissix; see mk/port.d/2to3.mk.
PY_VERSION!=	awk '/^\#define PY_MAJOR_VERSION/{maj=$$3} /^\#define PY_MINOR_VERSION/{min=$$3} END{print maj "." min}' ${TOP}/src/python/cpython/Include/patchlevel.h

# pydoc is a generated script, and cpython writes the prefix into its
# shebang: --prefix=/usr leaves it asking for /usr/bin/python3.<minor>,
# which is not where any of this is.  Apple's says "#!/usr/bin/xcrun
# python3", so that whichever developer directory is selected supplies the
# interpreter, and ours says the same -- the pip port already writes that
# line into its two scripts for the same reason.
P_POST_BUILD=		sed -i '' '1s|^\#!.*|\#!/usr/bin/xcrun python3|' \
			build/scripts-${PY_VERSION}/pydoc${PY_VERSION}

P_CONFIGURE_ARGS=	--without-ensurepip \
			--enable-shared=no
P_PROGS=		bin/python${PY_VERSION} bin/pydoc${PY_VERSION}
P_RELEASE_SYMLINK=	usr/bin/python${PY_VERSION} usr/bin/python3 \
			usr/bin/pydoc${PY_VERSION} usr/bin/pydoc3
P_RELEASE_MERGE=	lib usr/lib
