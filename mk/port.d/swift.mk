# swift -- the Swift compiler and standard library.
#
# Built against the LLVM this tree already builds, rather than through
# Swift's build-script.  build-script builds its own LLVM: that is an
# hour and several gigabytes to produce a second copy of what
# mk/port.d/llvm.mk has already made, and the compiler we ship would
# then not be the clang we ship.  Driving CMake directly costs a longer
# argument list and nothing else.
#
# Three sibling checkouts are required, and the build names them
# directly.  swift-cmark is checked out under its own name here while
# build-script expects it at <workspace>/cmark, which is why the path is
# passed explicitly rather than inferred.
#
# BOOTSTRAPPING_MODE=HOSTTOOLS uses the host's Swift for the parts of
# the compiler written in Swift.  The host here is Apple's 6.3.3, the
# same version as the source, so there is nothing to bootstrap around.

SWIFT_SRC=	${TOP}/src/swiftlang-llvm
LLVM_BUILD=	${TOP}/build/ports/llvm/build
CMARK_BUILD=	${P_WORKDIR}/cmark-build

P_BUILDSYS=	cmake
P_CMAKE_SRC=	.

P_CONFIGURE_ARGS=	\
	-DLLVM_DIR=${LLVM_BUILD}/lib/cmake/llvm \
	-DClang_DIR=${LLVM_BUILD}/lib/cmake/clang \
	-DSWIFT_PATH_TO_CMARK_SOURCE=${SWIFT_SRC}/swift-cmark \
	-DSWIFT_PATH_TO_CMARK_BUILD=${CMARK_BUILD} \
	-DSWIFT_BUILD_SWIFT_SYNTAX=ON \
	-DSWIFT_PATH_TO_SWIFT_SYNTAX_SOURCE=${SWIFT_SRC}/swift-syntax \
	-DSWIFT_PATH_TO_STRING_PROCESSING_SOURCE=${SWIFT_SRC}/swift-experimental-string-processing \
	-DBOOTSTRAPPING_MODE=HOSTTOOLS \
	-DSWIFT_INCLUDE_TESTS=OFF \
	-DSWIFT_INCLUDE_DOCS=OFF \
	-DSWIFT_BUILD_SOURCEKIT=OFF \
	-DSWIFT_ENABLE_EXPERIMENTAL_CONCURRENCY=ON

# The compiler, and the standard library for this machine.  swift and
# swiftc are symlinks onto swift-frontend, which is what the configure
# reports when no separate driver is built -- so the frontend is the
# compiler here, not just a component of it.
P_MAKE_ARGS=	swift-frontend swift-stdlib-macosx-arm64 \
		libswiftDemangle.dylib

P_NOSTAGE=	yes

# libswiftDemangle is the C interface to Swift's demangler -- what a
# debugger or a crash reporter calls to turn $s11SwiftDriver... back
# into something a person can read.  Apple's toolchain carries it; this
# one did not, and it is an ordinary target of the swift build.
P_LIBS=		lib/libswiftDemangle.dylib

# It is built against the libc++ the LLVM port builds and finds it
# through an rpath of /usr/lib/swift, where nothing puts it -- so as
# built the library does not load at all.  Apple's links the platform's
# own /usr/lib/libc++.1.dylib, which is what a library shipped in a
# toolchain should depend on: libc++ is ABI-stable on macOS and the
# system always has one, whereas this toolchain ships none and Apple's
# does not either.  Point it at the same one Apple points at.
P_POST_BUILD=	install_name_tool -change @rpath/libc++.1.dylib \
		    /usr/lib/libc++.1.dylib lib/libswiftDemangle.dylib

P_PROGS=	bin/swift-frontend

# The standard library: the modules and dylibs swiftc needs to compile
# anything at all.  swift and swiftc are symlinks onto swift-frontend,
# added by bundle-aliases rather than listed here -- P_PROGS copies, and
# the frontend is 150 MB.
P_TREES=	lib/swift

# cmark is small and is only ever consumed by this build, so it is built
# here rather than carried as a port of its own -- the same arrangement
# mk/port.d/llvm.mk uses for tapi.
${P_WORKDIR}/.configured: ${CMARK_BUILD}/.built

${CMARK_BUILD}/.built:
	@mkdir -p ${CMARK_BUILD}
	@${ECHO} "port: swift: building cmark"
	@cd ${CMARK_BUILD} && cmake -G Ninja ${SWIFT_SRC}/swift-cmark \
	    -DCMAKE_BUILD_TYPE=Release -DCMARK_TESTS=OFF -DCMARK_SHARED=OFF \
	    > ${P_WORKDIR}/cmark-configure.log 2>&1
	@ninja -C ${CMARK_BUILD} > ${P_WORKDIR}/cmark-build.log 2>&1
	@touch ${.TARGET}
