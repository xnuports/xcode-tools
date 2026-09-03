# The Go toolchain.
#
# Here because ipsw is written in Go and nothing else in this tree can
# build it.  Xcode ships no Go, so it goes to usr/local rather than onto
# the parity surface.
#
# Go builds in place: GOROOT is the source directory, and make.bash
# writes bin/ and pkg/ inside it.  P_COPY is what keeps that out of the
# submodule -- the port builds a copy under build/ports and the checkout
# stays as it was.
#
# Building Go needs a Go, which is the one bootstrap problem this tree
# cannot solve for itself.  GOROOT_BOOTSTRAP names whichever is
# installed; without one there is nothing to start from and the port
# says so rather than failing halfway through.
P_BUILDSYS=	make
P_OBJDIR=	${P_WORKDIR}/src/src
P_NOSTAGE=	yes

GOROOT_BOOTSTRAP!=	go env GOROOT 2>/dev/null || echo ""
P_MAKE=		env GOROOT_BOOTSTRAP=${GOROOT_BOOTSTRAP} ./make.bash

# The whole tree is the installation: a Go binary without its GOROOT --
# the standard library, the compiler's own packages -- cannot build
# anything.  So it is staged as a unit at usr/local/go, which is where
# a Go distribution normally lives, and the two commands are linked onto
# the path beside it.
P_PROGS=
P_RELEASE_TREES=	.. usr/local/go
P_RELEASE_SYMLINK=	usr/local/go/bin/go usr/local/bin/go \
			usr/local/go/bin/gofmt usr/local/bin/gofmt
