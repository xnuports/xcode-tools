# perl, from Apple's distribution.
#
# perl-175 carries perl 5.34.1, which is the version /usr/bin/perl
# reports, plus Apple's patches under 5.34/fix and 5.34/updates.
#
# Apple's Makefiles include $(MAKEFILEPATH)/CoreOS/ReleaseControl/
# GNUSource.make, which is their build harness rather than anything in
# the source drop.  Unlike DT_Signing.mk it is not optional -- it
# supplies the whole configure/build/install machinery this Makefile is
# written against -- but it does ship, inside Xcode, so it is pointed
# at rather than reimplemented.
P_BUILDSYS=	make

# This build installs where it wants to, and is then copied.
#
# Apple's Makefile builds each perl version into its own DSTROOT under
# OBJROOT and rsyncs them together at the end, passing
# DSTROOT="$(OBJROOT)/$$vers/DSTROOT" to each sub-make.  There is no way
# to redirect the final tree without breaking that:
#
#   - DSTROOT on the command line propagates through MAKEFLAGS and beats
#     the sub-make's own assignment, so every version installs straight
#     into the final tree, the per-version ones stay empty, and
#     mergeversions fails looking for what is not there.
#   - DSTROOT in the environment loses to Common.make, which assigns it
#     unconditionally.
#   - DESTDIR only feeds an "ifndef DSTROOT" that Common.make has
#     already made false.
#
# So the build runs exactly as Apple wrote it, into the DSTROOT
# Common.make picks -- /tmp/<project>/Release -- and what comes out is
# copied into the port's own tree afterwards.  Left alone the whole
# thing succeeds: one arm64 perl, merged, with Library, System and usr.
#
# P_NOSTAGE because there is nothing more to install; the copy below is
# the install.  It lands under the build directory rather than beside
# it because P_NOSTAGE reads P_PROGS out of P_OBJDIR.
APPLE_DSTROOT=	/tmp/perl/Release

P_NOSTAGE=	yes
P_POST_BUILD=	rm -rf DSTROOT && mkdir -p DSTROOT && \
		cp -R ${APPLE_DSTROOT}/. DSTROOT/ && \
		${TOP}/mk/scripts/fix-apple-perl-libperl.sh DSTROOT

# Apple's build runs as root, where the tarball's read-only files are
# still writable.  Ours does not.  See the script.
#
# The stale-state removal matters: Common.make's OBJROOT is a fixed
# /tmp/<project>/Build, so a second build reuses the first one's tree
# and skips steps whose stamps are already there -- which leaves the
# per-version DSTROOT half-populated and fails the merge.  Rebuilding
# the port has to mean rebuilding it.
P_PREPARE=	rm -rf /tmp/perl && \
		${TOP}/mk/scripts/prepare-apple-perl.sh .

MAKEFILEPATH!=	echo $$(xcode-select -p)/Makefiles

INTERNAL_SDK=	${TOP}/build/release/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.Internal.sdk

# Apple's Makefile hardcodes "MY_HOST := x86_64" -- their build machine
# when this was written -- and Platforms/MacOSX/Makefile.inc takes
# MY_ARCHS from RC_ARCHS or, failing that, MY_HOST.  Left alone it
# builds an x86_64 perl on an arm64 Mac.
RC_ARCHS!=	uname -m

P_MAKE_ARGS=	MAKEFILEPATH=${MAKEFILEPATH} \
		RC_ARCHS=${RC_ARCHS} \
		RC_TARGET_CONFIG=MacOSX \
		SDKROOT=${INTERNAL_SDK}

# The six wrappers Apple puts in /usr/bin, and the trees perl needs to
# run: System/Library/Perl is the library, Library/Perl is where a site
# installs, and usr/local/versioner is what the /usr/bin/perl stub reads
# to decide which version to exec.
P_PROGS=	DSTROOT/usr/bin/perl \
		DSTROOT/usr/bin/perl5.34

P_RELEASE_MERGE=	DSTROOT/System System \
			DSTROOT/Library Library \
			DSTROOT/usr/local usr/local \
			DSTROOT/usr/share usr/share
