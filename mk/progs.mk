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
# Our own reimplementations (src/xcode/, BSD-3-Clause).
# Apple ships all of these in Developer/usr/bin.
# ------------------------------------------------------------------
PROGS+=	xcode/codesign codesign usr/bin
PROGS+=	xcode/devicectl devicectl usr/bin
PROGS+=	xcode/notarytool notarytool usr/bin
PROGS+=	xcode/pkgbuild pkgbuild usr/bin
PROGS+=	xcode/productbuild productbuild usr/bin
PROGS+=	xcode/simctl simctl usr/bin
PROGS+=	xcode/xcode-select xcode-select usr/bin
PROGS+=	xcode/xcodebuild xcodebuild usr/bin
PROGS+=	xcode/xcrun xcrun usr/bin
PROGS+=	xcode/xctrace xctrace usr/bin

.if ${MK_TOOLCHAIN:tl} == "yes"
# ------------------------------------------------------------------
# cctools (src/dist-dev-tools/cctools) -- MK_TOOLCHAIN tier.
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
PROGS+=	dist-dev-tools/cctools/misc bitcode_strip ${XCTOOLCHAIN}/usr/bin
PROGS+=	dist-dev-tools/cctools/misc codesign_allocate ${XCTOOLCHAIN}/usr/bin
PROGS+=	dist-dev-tools/cctools/misc ctf_insert ${XCTOOLCHAIN}/usr/bin
PROGS+=	dist-dev-tools/cctools/misc install_name_tool ${XCTOOLCHAIN}/usr/bin
PROGS+=	dist-dev-tools/cctools/misc lipo ${XCTOOLCHAIN}/usr/bin
PROGS+=	dist-dev-tools/cctools/misc libtool ${XCTOOLCHAIN}/usr/bin
PROGS+=	dist-dev-tools/cctools/misc nm-classic ${XCTOOLCHAIN}/usr/bin
PROGS+=	dist-dev-tools/cctools/misc nmedit ${XCTOOLCHAIN}/usr/bin
PROGS+=	dist-dev-tools/cctools/misc segedit ${XCTOOLCHAIN}/usr/bin
PROGS+=	dist-dev-tools/cctools/misc size-classic ${XCTOOLCHAIN}/usr/bin
PROGS+=	dist-dev-tools/cctools/misc strings ${XCTOOLCHAIN}/usr/bin
PROGS+=	dist-dev-tools/cctools/misc strip ${XCTOOLCHAIN}/usr/bin
PROGS+=	dist-dev-tools/cctools/misc vtool ${XCTOOLCHAIN}/usr/bin
PROGS+=	dist-dev-tools/cctools/ar ar ${XCTOOLCHAIN}/usr/bin
PROGS+=	dist-dev-tools/cctools/otool otool-classic ${XCTOOLCHAIN}/usr/bin

# ld64 -- the linker.  NOT enabled yet: every source compiles and the
# link resolves except for tapi::*, and libtapi builds only inside the
# LLVM tree (tapi's CMakeLists uses llvm_add_library and reaches into
# CLANG_SOURCE_DIR).  It therefore waits on stage 5.  Enabling it is
# exactly this line:
#
#PROGS+=	dist-dev-tools/ld64 ld ${XCTOOLCHAIN}/usr/bin

# libtool builds against src/cctools-helpers/, our reimplementation of
# make_obj_file_with_linker_options() -- the one thing libtool.c needs
# from Apple's unpublished libcctoolshelper.  ranlib is the same binary
# and comes from T_LINKS in mk/tool.d/libtool.mk.
#
# The same missing library is why strip is built without -DTRIE_SUPPORT.
# See docs/CLAUDE.md section 9, stage 2.

.endif # MK_TOOLCHAIN

# ------------------------------------------------------------------
# developer_cmds (src/dist-dev-tools/developer_cmds).
#
# Xcode ships these in XcodeDefault.xctoolchain/usr/bin -- not in
# Developer/usr/bin, and not to be confused with the system copies in
# /usr/bin, which are apple-core's territory.  lorder is a shell script.
# ------------------------------------------------------------------
PROGS+=	dist-dev-tools/developer_cmds/asa asa ${XCTOOLCHAIN}/usr/bin
PROGS+=	dist-dev-tools/developer_cmds/ctags ctags ${XCTOOLCHAIN}/usr/bin
PROGS+=	dist-dev-tools/developer_cmds/indent indent ${XCTOOLCHAIN}/usr/bin
PROGS+=	dist-dev-tools/developer_cmds/lorder lorder ${XCTOOLCHAIN}/usr/bin
PROGS+=	dist-dev-tools/developer_cmds/rpcgen rpcgen ${XCTOOLCHAIN}/usr/bin
PROGS+=	dist-dev-tools/developer_cmds/unifdef unifdef ${XCTOOLCHAIN}/usr/bin

# ------------------------------------------------------------------
# headerdoc (src/dist-dev-tools/headerdoc) -- Perl, installed as scripts.
# Xcode ships these in Developer/usr/bin.
# ------------------------------------------------------------------
PROGS+=	dist-dev-tools/headerdoc headerdoc2html usr/bin
PROGS+=	dist-dev-tools/headerdoc/xmlman hdxml2manxml usr/bin
PROGS+=	dist-dev-tools/headerdoc/xmlman xml2man usr/bin
PROGS+=	dist-dev-tools/headerdoc/xmlman resolveLinks usr/bin
PROGS+=	dist-dev-tools/headerdoc gatherheaderdoc usr/bin

# ------------------------------------------------------------------
# pngcrush (src/pngcrush) -- Developer/usr/bin.  Bundles its own libpng
# and zlib, so the source list is pinned rather than discovered.
# ------------------------------------------------------------------
PROGS+=	pngcrush/pngcrush pngcrush usr/bin

# ------------------------------------------------------------------
# PlistBuddy (src/PlistBuddy).
#
# Note this one is not part of Xcode's Developer directory at all --
# stock macOS ships it at /usr/libexec/PlistBuddy.  It is carried here
# because the project added it as a submodule; usr/libexec is the
# closest match to where the system keeps it.
# ------------------------------------------------------------------
PROGS+=	PlistBuddy PlistBuddy usr/libexec
