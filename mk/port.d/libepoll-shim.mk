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
