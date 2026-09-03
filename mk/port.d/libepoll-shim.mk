# libepoll-shim -- epoll(7) on top of kqueue.
#
# Darwin has kqueue and no epoll, so code written for Linux cannot be
# built here without either porting it or giving it the interface it
# expects.  This is the second option, and it is carried deliberately:
# it is what lets Linux-targeted source compile against this SDK.
#
# A library, so the headers and the built library go to usr/local
# rather than into the SDK -- the SDK is for what the platform itself
# provides, and this is not that.
P_BUILDSYS=	cmake
P_CMAKE_SRC=	.
P_OBJDIR=	${P_WORKDIR}/build
# usr/local is shared with the other libraries here, so these merge
# rather than replace: P_RELEASE_TREES removes the destination first,
# which had each of these wiping the one before it.
P_RELEASE_MERGE=	include usr/local/include \
			lib usr/local/lib

# A library: no programs, so nothing for the default P_PROGS to
# look for.  What it installs is checked through P_RELEASE_TREES.
P_PROGS=

# The headers install under include/libepoll-shim/sys/ so they cannot
# shadow anything by accident, which means a caller has to add that
# directory to its include path before <sys/epoll.h> resolves.  Linking
# them into usr/local/include/sys puts them where the name says they
# are, beside sys/inotify.h from libinotify-kqueue, so one -I reaches
# both.  Symlinks rather than copies: there is one file, and it is
# visible which library it came from.
#
# Nothing here shadows a real header -- Darwin has no epoll.h,
# eventfd.h, signalfd.h or timerfd.h of its own.
P_RELEASE_LINKDIR=	usr/local/include/libepoll-shim/sys \
			usr/local/include/sys

# The headers linked above include <epoll-shim/detail/common.h> among
# themselves, so that prefix has to be reachable from the same -I or
# the link buys nothing.  One directory symlink does it.
P_RELEASE_SYMLINK=	usr/local/include/libepoll-shim/epoll-shim \
			usr/local/include/epoll-shim
