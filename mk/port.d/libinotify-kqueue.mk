# libinotify-kqueue -- inotify(7) on top of kqueue.
#
# The same idea as libepoll-shim beside it, for the other interface
# Linux has and Darwin does not: watching a file or directory for
# change.  Carried for the same reason -- so source written against
# inotify can be built here rather than rewritten.
#
# Ships configure.ac and no configure, so autogen runs first.
P_PREPARE=	./autogen.sh
# usr/local is shared with the other libraries here, so these merge
# rather than replace: P_RELEASE_TREES removes the destination first,
# which had each of these wiping the one before it.
P_RELEASE_MERGE=	include usr/local/include \
			lib usr/local/lib

# Its configure adds -Werror, and dep-list.c calls fdclosedir, which
# the headers mark as macOS 26.4 and newer.  Without a deployment
# target the compiler assumes an older floor and
# -Wunguarded-availability-new turns that into an error.  Saying which
# macOS this is built for is the fix; silencing the warning would only
# hide the same question.
XT_SDK_VERSION!=	xcrun --show-sdk-version 2>/dev/null || echo 26.5
# Its libtool link line carries -undefined dynamic_lookup, which the
# linker now refuses for anything eligible for the dyld shared cache.
# The flag below opts this library out of the cache, which is what the
# error itself suggests and is correct here: it is a compatibility
# shim built into this tree, not a system library the cache would ever
# hold.
P_CONFIGURE_ARGS=	CFLAGS=-mmacosx-version-min=${XT_SDK_VERSION} \
			LDFLAGS=-Wl,-not_for_dyld_shared_cache

# A library: no programs, so nothing for the default P_PROGS to
# look for.  What it installs is checked through P_RELEASE_TREES.
P_PROGS=
