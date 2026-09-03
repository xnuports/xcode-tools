# libplist -- read and write Apple property lists.
#
# Of direct interest here and not only a dependency: property lists are
# how this tree's own tools talk -- the pbxproj a project is written
# in, SDKSettings, Info.plist -- and libplist is an independent
# implementation of the same formats, binary and XML both, which makes
# it worth having next to CoreFoundation's.
#
# It also brings plistutil, which reads and converts them from a shell.
# autogen derives the version from git, and P_PREPARE runs in the
# copied tree, which has no .git at all -- "PACKAGE_VERSION is not
# defined" is what that looks like.  The version is written out from
# the submodule first, which is the mechanism upstream provides for
# building outside a checkout.
LIBPLIST_VERSION!=	git -C ${TOP}/src/extras/libplist describe --tags 2>/dev/null || echo 2.7.0
P_PREPARE=	echo ${LIBPLIST_VERSION} > .tarball-version && ./autogen.sh
P_CONFIGURE_ARGS=	--without-cython
P_PROGS=	bin/plistutil
# usr/local is shared with the other libraries here, so these merge
# rather than replace: P_RELEASE_TREES removes the destination first,
# which had each of these wiping the one before it.
P_RELEASE_MERGE=	include usr/local/include \
			lib usr/local/lib
