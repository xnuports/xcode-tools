# lib/Makefile
#
# Builds the static libraries consumed by the tools, from submodule
# sources, into build/lib/.  Submodules are read-only; sources are
# referenced in place and objects land under build/lib/<name>/.
#
# Consumers pull these in via mk/with-libstuff.mk and
# mk/with-libmacho.mk, which also carry the matching include paths.

TOP?=		${.CURDIR}/..

.include "${TOP}/mk/xcodetools.sys.mk"

OBJBASE=	${TOP}/build/lib
CCTOOLS=	${TOP}/src/apple/distribution-Developer_Tools/cctools

# Same flags Apple's libstuff.xcconfig and cctools.xcconfig set; see
# mk/with-cctools.mk for the reasoning behind each one.
CCTOOLS_VERSION?=	cctools-1030.6.3
CCTOOLS_CFLAGS=	-I${CCTOOLS}/include -I${CCTOOLS}/include/stuff \
		-I${TOP}/src/swiftlang-llvm/llvm-project/llvm/include \
		-include ${TOP}/mk/cctools-compat.h \
		-Wno-ambiguous-macro -DLTO_SUPPORT \
		-DCURRENT_PROJECT_VERSION=\"${CCTOOLS_VERSION}\"

RELEASE=	${TOP}/build/release
XCSELECT=	${TOP}/src/openxc-tools/openxc/libxcselect

all: ${OBJBASE}/libstuff.a ${OBJBASE}/libmacho.a \
     ${RELEASE}/usr/lib/libxcselect.dylib

# --- libxcselect ---------------------------------------------------
#
# The library Apple's xcrun and xcode-select link to agree on where the
# active developer directory is.  Ours is a shared library for the same
# reason theirs is: one copy of the answer, linked by both tools.
#
# The install name is @rpath-relative and the tools carry an rpath of
# @executable_path/../lib, so the release tree stays relocatable -- it
# is meant to be moved and still work, which an absolute /usr/lib name
# would break.
${RELEASE}/usr/lib/libxcselect.dylib: ${XCSELECT}/libxcselect.c ${XCSELECT}/xcselect.h
	@mkdir -p ${.TARGET:H}
	${CC} -dynamiclib -install_name @rpath/libxcselect.dylib \
	    ${CFLAGS} -I${XCSELECT} -o ${.TARGET} ${XCSELECT}/libxcselect.c
	@${ECHO} "built: usr/lib/libxcselect.dylib"

# --- libstuff -----------------------------------------------------
#
# Sources are discovered so the set tracks upstream rather than drifting
# from a hand-maintained list.

LIBSTUFF_SRCS!=	cd ${CCTOOLS}/libstuff && ls *.c 2>/dev/null || true

LIBSTUFF_OBJS=
.for f in ${LIBSTUFF_SRCS}
LIBSTUFF_OBJS+=	${OBJBASE}/libstuff/${f:R}.o
.endfor

.for f in ${LIBSTUFF_SRCS}
${OBJBASE}/libstuff/${f:R}.o: ${CCTOOLS}/libstuff/${f}
	@mkdir -p ${OBJBASE}/libstuff
	${CC} ${CPPFLAGS} ${CCTOOLS_CFLAGS} ${CFLAGS} -c ${.ALLSRC} -o ${.TARGET}
.endfor

${OBJBASE}/libstuff.a: ${LIBSTUFF_OBJS}
	rm -f ${.TARGET}
	${AR} crs ${.TARGET} ${LIBSTUFF_OBJS}

# --- libmacho -----------------------------------------------------

LIBMACHO_SRCS!=	cd ${CCTOOLS}/libmacho && ls *.c 2>/dev/null || true

LIBMACHO_OBJS=
.for f in ${LIBMACHO_SRCS}
LIBMACHO_OBJS+=	${OBJBASE}/libmacho/${f:R}.o
.endfor

.for f in ${LIBMACHO_SRCS}
${OBJBASE}/libmacho/${f:R}.o: ${CCTOOLS}/libmacho/${f}
	@mkdir -p ${OBJBASE}/libmacho
	${CC} ${CPPFLAGS} ${CCTOOLS_CFLAGS} ${CFLAGS} -c ${.ALLSRC} -o ${.TARGET}
.endfor

${OBJBASE}/libmacho.a: ${LIBMACHO_OBJS}
	rm -f ${.TARGET}
	${AR} crs ${.TARGET} ${LIBMACHO_OBJS}

clean:
	rm -rf ${OBJBASE}

list-libs:
	@${ECHO} "build/lib/libstuff.a <- ${LIBSTUFF_SRCS:[#]} sources"
	@${ECHO} "build/lib/libmacho.a <- ${LIBMACHO_SRCS:[#]} sources"

.PHONY: all clean list-libs
