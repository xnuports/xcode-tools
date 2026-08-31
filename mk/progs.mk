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
# Our own reimplementations (src/openxc-tools/openxc/, BSD-3-Clause).
# Apple ships all of these in Developer/usr/bin.
# ------------------------------------------------------------------
PROGS+=	openxc-tools/openxc/codesign codesign usr/bin
PROGS+=	openxc-tools/openxc/devicectl devicectl usr/bin
PROGS+=	openxc-tools/openxc/notarytool notarytool usr/bin
PROGS+=	openxc-tools/openxc/pkgbuild pkgbuild usr/bin
PROGS+=	openxc-tools/openxc/productbuild productbuild usr/bin
PROGS+=	openxc-tools/openxc/simctl simctl usr/bin
PROGS+=	openxc-tools/openxc/xcode-select xcode-select usr/bin
PROGS+=	openxc-tools/openxc/xcodebuild xcodebuild usr/bin
PROGS+=	openxc-tools/openxc/xcrun xcrun usr/bin
PROGS+=	openxc-tools/openxc/xctrace xctrace usr/bin

.if ${MK_TOOLCHAIN:tl} == "yes"
# ------------------------------------------------------------------
# cctools (src/apple-oss-distributions/distribution-Developer_Tools/cctools) -- MK_TOOLCHAIN tier.
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
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/cctools/misc bitcode_strip ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/cctools/misc codesign_allocate ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/cctools/misc ctf_insert ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/cctools/misc install_name_tool ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/cctools/misc lipo ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/cctools/misc libtool ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/cctools/misc nm-classic ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/cctools/misc nmedit ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/cctools/misc segedit ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/cctools/misc size-classic ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/cctools/misc strings ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/cctools/misc strip ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/cctools/misc vtool ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/cctools/ar ar ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/cctools/otool otool-classic ${XCTOOLCHAIN}/usr/bin

# ld64 -- the linker.  Needs libtapi, which the llvm port stages, so it
# only builds with MK_PORTS=yes; without it the link fails on tapi::*.
.if ${MK_PORTS:tl} == "yes"
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/ld64 ld ${XCTOOLCHAIN}/usr/bin
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
# developer_cmds (src/apple-oss-distributions/distribution-Developer_Tools/developer_cmds).
#
# Xcode ships these in XcodeDefault.xctoolchain/usr/bin -- not in
# Developer/usr/bin, and not to be confused with the system copies in
# /usr/bin, which are apple-core's territory.  lorder is a shell script.
# ------------------------------------------------------------------
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/developer_cmds/asa asa ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/developer_cmds/ctags ctags ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/developer_cmds/indent indent ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/developer_cmds/lorder lorder ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/developer_cmds/rpcgen rpcgen ${XCTOOLCHAIN}/usr/bin
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/developer_cmds/unifdef unifdef ${XCTOOLCHAIN}/usr/bin

# ------------------------------------------------------------------
# headerdoc (src/apple-oss-distributions/distribution-Developer_Tools/headerdoc) -- Perl, installed as scripts.
# Xcode ships these in Developer/usr/bin.
# ------------------------------------------------------------------
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/headerdoc headerdoc2html usr/bin
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/headerdoc/xmlman hdxml2manxml usr/bin
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/headerdoc/xmlman xml2man usr/bin
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/headerdoc/xmlman resolveLinks usr/bin
PROGS+=	apple-oss-distributions/distribution-Developer_Tools/headerdoc gatherheaderdoc usr/bin

# ------------------------------------------------------------------
# pngcrush (src/openxc-tools/pngcrush) -- Developer/usr/bin.  Bundles its own libpng
# and zlib, so the source list is pinned rather than discovered.
# ------------------------------------------------------------------
PROGS+=	openxc-tools/pngcrush/pngcrush pngcrush usr/bin

# ------------------------------------------------------------------
# vmmap (src/openxc-tools/vmmap) -- a third-party implementation of a tool Apple
# ships in Developer/usr/bin but has never open-sourced.
# ------------------------------------------------------------------
PROGS+=	openxc-tools/vmmap/src vmmap usr/bin

# ------------------------------------------------------------------
# bsdmake (src/extras/bsdmake).
#
# Note usr/local/bin, not usr/bin: Xcode ships neither bsdmake nor
# bmake, so putting them on the parity surface would misrepresent the
# tree.  They are ours, useful, and kept where that is obvious.
# ------------------------------------------------------------------
PROGS+=	extras/bsdmake bsdmake usr/local/bin

# ------------------------------------------------------------------
# PlistBuddy (src/openxc-tools/PlistBuddy).
#
# Note this one is not part of Xcode's Developer directory at all --
# stock macOS ships it at /usr/libexec/PlistBuddy.  It is carried here
# because the project added it as a submodule; usr/libexec is the
# closest match to where the system keeps it.
# ------------------------------------------------------------------
PROGS+=	openxc-tools/PlistBuddy PlistBuddy usr/libexec

# ------------------------------------------------------------------
# bsdmake (src/extras/bsdmake/) -- Apple's BSD make.
#
# Xcode ships this as Developer/usr/bin/bsdmake.  Its own Makefile
# depends on <bsd.prog.mk> and the full BSD make include chain, so
# we compile it directly via mk/tool.mk with an explicit source list.
# The mk/ files it ships are copied into usr/share/mk/ by the
# tool fragment (mk/tool.d/bsdmake.mk).
# ------------------------------------------------------------------
PROGS+=	extras/bsdmake bsdmake usr/bin
