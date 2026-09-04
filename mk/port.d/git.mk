# git, from Apple's distribution rather than upstream.
#
# Both carry the same upstream source -- v2.50.1 either way -- so what
# Apple's adds is everything around it: their gitconfig and
# gitattributes, the man pages, contrib, and the version suffix.  Built
# from upstream this answers "git version 2.50.1"; built from here it
# answers "git version 2.50.1 (Apple Git-155)", which is what Xcode
# ships, and matching that is the point of this tree.
#
# The version is not hardcoded.  Apple's Makefile builds the suffix
# from RC_ProjectSourceVersion, and distribution-Developer_Tools names
# the tag for every project it carries in release.json, so it is read
# from there -- the same file that says which cctools and ld64 this is.
#
# Apple's Makefile takes DSTROOT rather than DESTDIR, and puts things
# under DEVELOPER_INSTALL_DIR/usr when RC_DEVTOOLS is set, which is how
# Developer/usr/bin/git comes about.
P_BUILDSYS=	make

# Apple's Makefile includes AppleInternal/Makefiles/DT_Signing.mk
# unconditionally, and that file ships inside Apple and nowhere else,
# so make stops before doing anything.  It is made optional in the
# copied tree rather than patched out: it defines nothing this Makefile
# references -- checked -- so its absence changes nothing, and the day
# it does exist the include still works.
#
# The install rules also hand install(1) "-o root -g wheel" and follow
# up with chown, which is right for Apple's build system and impossible
# for anyone building unprivileged.  Ownership of a staged DSTROOT
# means nothing here -- the release tree is not a system install -- so
# both come out.
# usage.c turns on os/assumes.h's experimental libtrace crash path, which
# needs the userspace os/log_private.h.  That header is libtrace's and
# unpublished, so the internal SDK carries a reconstruction of it and
# Apple's source is left exactly as Apple wrote it.
P_PREPARE=	sed -i '' 's|^include .*DT_Signing.mk|-&|' Makefile && \
		sed -i '' -e 's|-o root -g wheel ||g' \
		    -e '/chown -R root:wheel/d' Makefile

# It asks xcrun for the macosx.internal SDK, which is exactly the one
# this tree assembles.  Naming it directly keeps the build off whatever
# xcrun happens to answer first.
INTERNAL_SDK=	${TOP}/build/release/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.Internal.sdk

GIT_TAG!=	python3 -c "import json;print([p['tag'] for p in \
		json.load(open('${TOP}/src/apple/distribution-Developer_Tools/release.json'))['projects'] \
		if p['project']=='Git'][0].split('-')[-1])" 2>/dev/null || echo 155

P_MAKE_ARGS=	RC_ProjectSourceVersion=${GIT_TAG} \
		SDKROOT=${INTERNAL_SDK} \
		RC_DEVTOOLS=1 \
		DEVELOPER_INSTALL_DIR= \
		DSTROOT=${P_STAGEDIR}

# The six Xcode ships in Developer/usr/bin, which is exactly what this
# build stages.
P_PROGS=	bin/git \
		bin/git-receive-pack \
		bin/git-shell \
		bin/git-upload-archive \
		bin/git-upload-pack \
		bin/scalar

# git is not one binary.  The 172 helpers under libexec/git-core are how
# most subcommands are actually run, and share/git-core holds the
# templates git init copies into a new repository -- without them git
# init warns and makes a repository with no hooks or description.
P_RELEASE_MERGE=	libexec usr/libexec \
			share usr/share
