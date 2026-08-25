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
