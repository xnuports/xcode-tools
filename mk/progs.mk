# mk/progs.mk - the tool inventory.
#
# One entry per built program, three whitespace-separated fields:
#
#	PROGS+= <dir-under-src> <program-name> <install-suffix>
#
#   <dir>             directory containing the sources, under src/
#   <program-name>    final binary name
#   <install-suffix>  path under build/release/, mirroring where Xcode
#                     keeps each tool:
#                       usr/bin, usr/libexec,
#                       ${XCTOOLCHAIN}/usr/bin
#
# build/release/ is a drop-in replacement for
# /Applications/Xcode.app/Contents/Developer/.

# ------------------------------------------------------------------
# Our own reimplementations (src/openxc-tools/, BSD-3-Clause).
# Apple ships all of these in Developer/usr/bin.
# ------------------------------------------------------------------
PROGS+=	openxc-tools/codesign codesign usr/bin
PROGS+=	openxc-tools/devicectl devicectl usr/bin
PROGS+=	openxc-tools/notarytool notarytool usr/bin
PROGS+=	openxc-tools/pkgbuild pkgbuild usr/bin
PROGS+=	openxc-tools/productbuild productbuild usr/bin
PROGS+=	openxc-tools/simctl simctl usr/bin
PROGS+=	openxc-tools/xcode-select xcode-select usr/bin
PROGS+=	openxc-tools/xcodebuild xcodebuild usr/bin
PROGS+=	openxc-tools/xcrun xcrun usr/bin
PROGS+=	openxc-tools/xcstringstool xcstringstool usr/bin
PROGS+=	openxc-tools/xctrace xctrace usr/bin

# The resource tools.  Apple keep the binaries in Developer/usr/bin and
# symlink them into Developer/Tools, which is the name the older build
# systems know them by; mk/bundle.mk makes those links.
PROGS+=	openxc-tools/Rez GetFileInfo usr/bin
PROGS+=	openxc-tools/Rez SetFile usr/bin
PROGS+=	openxc-tools/Rez SplitForks usr/bin
PROGS+=	openxc-tools/Rez DeRez usr/bin
PROGS+=	openxc-tools/Rez ResMerger usr/bin
PROGS+=	openxc-tools/Rez Rez usr/bin

# TextureAtlas, the SpriteKit atlas compiler.
PROGS+=	openxc-tools/TextureAtlas TextureAtlas usr/bin

# genstrings, which Apple also install as extractLocStrings.  The second
# name is a symlink in their tree; mk/bundle.mk makes it here.
PROGS+=	openxc-tools/genstrings genstrings usr/bin

# agvtool, which moves an Xcode project's version numbers along.
PROGS+=	openxc-tools/agvtool agvtool usr/bin

# xarsigner, which puts a detached signature into a xar archive.
PROGS+=	openxc-tools/xarsigner xarsigner usr/bin

# xcdebug, which asks Xcode to attach to a process or run a scheme.
PROGS+=	openxc-tools/xcdebug xcdebug usr/bin

# xcresulttool needs libzstd, which the zstd port builds -- a .xcresult
# bundle stores its objects compressed and nothing on the system provides
# the library.  Gated for the same reason libtapi's consumer is: without
# the port there is nothing to link against.
# atos links LLVM's symbolizer, which the llvm port builds, so it is gated
# on the ports for the same reason the others here are.
PROGS+=	openxc-tools/atos atos usr/bin

# TextureConverter links the encoders it drives -- ARM's astc-encoder for
# ASTC and NVTT for the filters Apple's mip chains are built with -- so it
# is gated on the ports for the same reason xcresulttool is: without them
# there is nothing to link against.
.if ${MK_PORTS:tl} == "yes"
PROGS+=	openxc-tools/TextureConverter TextureConverter usr/bin
PROGS+=	openxc-tools/xcresulttool xcresulttool usr/bin
PROGS+=	openxc-tools/xccov xccov usr/bin
.endif

.if ${MK_TOOLCHAIN:tl} == "yes"
# ------------------------------------------------------------------
# cctools (src/apple/distribution-Developer_Tools/cctools) -- MK_TOOLCHAIN tier.
#
# Apple ships these in XcodeDefault.xctoolchain/usr/bin, not in
# Developer/usr/bin (docs/DOCUMENTATION.md section 3.1).
#
# Note the "-classic" names.  Modern Xcode has retired the cctools
# implementations of nm, otool and size from their plain names: in a
# stock toolchain `nm` and `otool` are symlinks to llvm-nm and
# llvm-otool, and `size` is a symlink to size-classic.  The cctools
# builds ship alongside them as nm-classic, otool-classic and
# size-classic.  We install under those same names, leaving nm and otool
# for llvm-project to provide in stage 5.
#
# misc/ is a flat directory of single-file programs, so each entry needs
# a mk/tool.d/<prog>.mk pinning T_SRCS to its own source; ar/ and otool/
# are per-program directories and auto-discover.
# ------------------------------------------------------------------
PROGS+=	apple/distribution-Developer_Tools/cctools/misc bitcode_strip ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple/distribution-Developer_Tools/cctools/misc codesign_allocate ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple/distribution-Developer_Tools/cctools/misc ctf_insert ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple/distribution-Developer_Tools/cctools/misc install_name_tool ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple/distribution-Developer_Tools/cctools/misc lipo ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple/distribution-Developer_Tools/cctools/misc libtool ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple/distribution-Developer_Tools/cctools/misc nm-classic ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple/distribution-Developer_Tools/cctools/misc nmedit ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple/distribution-Developer_Tools/cctools/misc segedit ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple/distribution-Developer_Tools/cctools/misc size-classic ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple/distribution-Developer_Tools/cctools/misc strings ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple/distribution-Developer_Tools/cctools/misc strip ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple/distribution-Developer_Tools/cctools/misc vtool ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple/distribution-Developer_Tools/cctools/ar ar ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple/distribution-Developer_Tools/cctools/otool otool-classic ${XCTOOLCHAIN}/usr/bin

# mig and migcom, from bootstrap_cmds.  Most of an SDK's mach/ headers
# are mig's output rather than files anyone wrote, so these come before
# anything that wants them.  migcom goes in libexec because that is
# where the mig script looks for it.
PROGS+=	apple/distribution-Developer_Tools/bootstrap_cmds/migcom.tproj migcom ${XCTOOLCHAIN}/usr/libexec
PROGS+=	apple/distribution-Developer_Tools/bootstrap_cmds/migcom.tproj mig ${XCTOOLCHAIN}/usr/bin

# ld64 -- the linker.  Needs libtapi, which the llvm port stages, so it
# only builds with MK_PORTS=yes; without it the link fails on tapi::*.
.if ${MK_PORTS:tl} == "yes"
PROGS+=	apple/distribution-Developer_Tools/ld64 ld ${XCTOOLCHAIN}/usr/bin
.endif

# libtool builds against src/cctools-helpers/, our reimplementation of
# make_obj_file_with_linker_options() -- the one thing libtool.c needs
# from Apple's unpublished libcctoolshelper.  ranlib is the same binary
# and comes from T_LINKS in mk/tool.d/libtool.mk.
#
# The same missing library is why strip is built without -DTRIE_SUPPORT.
# See docs/CLAUDE.md section 9, stage 2.

.endif # MK_TOOLCHAIN

# ------------------------------------------------------------------
# developer_cmds (src/apple/distribution-Developer_Tools/developer_cmds).
#
# Xcode ships these in XcodeDefault.xctoolchain/usr/bin -- not in
# Developer/usr/bin, and not to be confused with the system copies in
# /usr/bin, which are apple-core's territory.  lorder is a shell script.
# ------------------------------------------------------------------
PROGS+=	apple/distribution-Developer_Tools/developer_cmds/asa asa ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple/distribution-Developer_Tools/developer_cmds/ctags ctags ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple/distribution-Developer_Tools/developer_cmds/indent indent ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple/distribution-Developer_Tools/developer_cmds/lorder lorder ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple/distribution-Developer_Tools/developer_cmds/rpcgen rpcgen ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple/distribution-Developer_Tools/developer_cmds/unifdef unifdef ${XCTOOLCHAIN}/usr/bin

# ------------------------------------------------------------------
# headerdoc (src/apple/distribution-Developer_Tools/headerdoc) -- Perl, installed as scripts.
# Xcode ships these in Developer/usr/bin.
# ------------------------------------------------------------------
PROGS+=	apple/distribution-Developer_Tools/headerdoc headerdoc2html usr/bin
PROGS+=	apple/distribution-Developer_Tools/headerdoc/xmlman hdxml2manxml usr/bin
PROGS+=	apple/distribution-Developer_Tools/headerdoc/xmlman xml2man usr/bin
PROGS+=	apple/distribution-Developer_Tools/headerdoc/xmlman resolveLinks usr/bin
PROGS+=	apple/distribution-Developer_Tools/headerdoc gatherheaderdoc usr/bin

# ------------------------------------------------------------------
# pngcrush (src/other/pngcrush) -- Developer/usr/bin.  Bundles its own libpng
# and zlib, so the source list is pinned rather than discovered.
# ------------------------------------------------------------------
PROGS+=	other/pngcrush/pngcrush pngcrush usr/bin

# ------------------------------------------------------------------
# vmmap (src/other/vmmap) -- a third-party implementation of a tool Apple
# ships in Developer/usr/bin but has never open-sourced.
# ------------------------------------------------------------------
PROGS+=	other/vmmap/src vmmap usr/bin

# ------------------------------------------------------------------
# bsdmake (src/extras/bsdmake).
#
# Note usr/local/bin, not usr/bin: Xcode ships neither bsdmake nor
# bmake, so putting them on the parity surface would misrepresent the
# tree.  They are ours, useful, and kept where that is obvious.
# ------------------------------------------------------------------
PROGS+=	extras/bsdmake bsdmake usr/local/bin

# ------------------------------------------------------------------
# PlistBuddy (src/remorix/PlistBuddy).
#
# Note this one is not part of Xcode's Developer directory at all --
# stock macOS ships it at /usr/libexec/PlistBuddy.  It is carried here
# because the project added it as a submodule; usr/libexec is the
# closest match to where the system keeps it.
# ------------------------------------------------------------------
PROGS+=	remorix/PlistBuddy PlistBuddy usr/libexec

# ------------------------------------------------------------------
# foundation_cmds (src/remorix/foundation_cmds) and AKCmds
# (src/remorix/AKCmds) -- reimplementations of the property-list and
# Cocoa command line tools.
#
# Like PlistBuddy above, none of these belong to Xcode's Developer
# directory: stock macOS ships every one of them in /usr/bin, which is
# where they go here.  Each submodule builds its programs with a
# Makefile per subdirectory; we compile the sources directly instead,
# the way every other imported component in this tree is built, with
# the frameworks each Makefile names in mk/tool.d/<program>.mk.
#
# tiffutil is the one program of the eight left out.  Its Makefile
# bootstraps a private copy of libtiff, downloading and patching it,
# which is not something a build here can do.
#
# Two places where these do not yet match the system's, both in the
# submodules and so not ours to fix here:
#
#	plutil	a plist holding a <data> value fails to convert to JSON
#		with a differently worded message than Apple's, and
#		-extract of an unrelated key in the same file fails as
#		well -- the whole plist is checked against the output
#		format, not the object being extracted.
#	open	does not accept --arch.
#
# Everything else checked matches: plutil's -lint, -p, -convert to xml1,
# binary1 and json, and -extract to json and raw; defaults read and
# domains; pl; textutil -convert; tops replace; pbcopy and pbpaste.
# ------------------------------------------------------------------
PROGS+=	remorix/foundation_cmds/defaults defaults usr/bin
PROGS+=	remorix/foundation_cmds/pl pl usr/bin
PROGS+=	remorix/foundation_cmds/plutil plutil usr/bin

PROGS+=	remorix/AKCmds/open open usr/bin
PROGS+=	remorix/AKCmds/pbcopy pbcopy usr/bin
PROGS+=	remorix/AKCmds/textutil textutil usr/bin
PROGS+=	remorix/AKCmds/tiff2icns tiff2icns usr/bin
PROGS+=	remorix/AKCmds/tops tops usr/bin

# ------------------------------------------------------------------
# jonesforth (src/extras/jonesforth-macos) -- a FORTH compiler that is
# also a tutorial, ported to arm64 macOS.
#
# usr/local/bin, by the rule mk/ports.mk states for src/extras: this
# works on neither Mach-O nor a build, so it is a general utility and
# goes beside arm64th's "forth" rather than into the toolchain.
#
# One assembly source, nothing linked.  mk/tool.d/jonesforth.mk also
# installs the FORTH half of the language and the wrapper that reads
# it, which is what makes the binary usable.
# ------------------------------------------------------------------
PROGS+=	extras/jonesforth-macos jonesforth usr/local/bin

# ------------------------------------------------------------------
# machsec (src/extras/machsec) -- reports the hardening a Mach-O binary
# was built with.
#
# The toolchain, not usr/local: by the rule mk/ports.mk states for
# src/extras, a tool that works on Mach-O belongs beside clang and ld.
#
# Needs the capstone port, which mk/ports.mk builds; every port is
# built before any program, so the ordering holds.
# ------------------------------------------------------------------
PROGS+=	extras/machsec machsec ${XCTOOLCHAIN}/usr/bin
