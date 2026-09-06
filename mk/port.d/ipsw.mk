# ipsw -- read and take apart Apple firmware.
#
# Written in Go, which is why the Go toolchain above it exists in this
# tree at all.  Xcode ships nothing like it, so usr/local/bin.
#
# Built with the Go this tree builds, not whatever is on PATH: GOROOT
# points at usr/local/go, so the port depends on the go port having run
# and not on the machine having Go installed.
#
# One thing to know before building it: ipsw vendors nothing and its
# go.mod names 257 dependencies, so the build fetches them.  Everything
# else here builds from what is checked out; this one does not, and Go
# modules are simply built that way.
#
# Vendoring them to stop that has been looked at and does not work.
# "go mod vendor" resolves every package in the module under every build
# tag, not just the ones a build compiles, and upstream gitignores
# pkg/sandbox/ while keeping files that import it behind "//go:build
# sandbox" -- cmd/ipsw/cmd/sb/sb_reach.go and sb_diff.go, and
# internal/diff/sandbox.go.  So it fails on a package the public checkout
# is never given.  Deleting those five files in the build copy does let it
# through, and the result builds offline, but the tree is 230MB (129MB of
# it modernc.org's transpiled-C sqlite) and that is a poor trade for a
# fetch that go.sum already pins and verifies by hash, and that Go caches
# after the first build.
P_BUILDSYS=	make
P_NOSTAGE=	yes

GO_ROOT=	${TOP}/build/release/usr/local/go
# The version is linked in, not compiled in: built without these the
# binary answers "Version:" and nothing after it.  Taken from the
# submodule, so what ipsw reports is the tag that was checked out.
IPSW_VERSION!=	git -C ${TOP}/src/extras/ipsw describe --tags 2>/dev/null || echo unknown
IPSW_COMMIT!=	git -C ${TOP}/src/extras/ipsw rev-parse --short HEAD 2>/dev/null || echo unknown
IPSW_LDFLAGS=	-s -w \
		-X github.com/blacktop/ipsw/cmd/ipsw/cmd.AppVersion=${IPSW_VERSION} \
		-X github.com/blacktop/ipsw/cmd/ipsw/cmd.AppBuildCommit=${IPSW_COMMIT}

P_MAKE=		env GOROOT=${GO_ROOT} GOTOOLCHAIN=local ${GO_ROOT}/bin/go build
P_MAKE_ARGS=	-ldflags "${IPSW_LDFLAGS}" -o bin/ ./cmd/ipsw ./cmd/ipswd

P_PROGS=	bin/ipsw bin/ipswd
