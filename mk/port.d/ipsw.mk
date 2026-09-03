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
