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
CCTOOLS=	${TOP}/src/dist-dev-tools/cctools

# Same flags Apple's libstuff.xcconfig and cctools.xcconfig set; see
# mk/with-cctools.mk for the reasoning behind each one.
CCTOOLS_VERSION?=	cctools-1030.6.3
CCTOOLS_CFLAGS=	-I${CCTOOLS}/include -I${CCTOOLS}/include/stuff \
		-I${TOP}/src/llvm-project/llvm/include \
		-include ${TOP}/mk/cctools-compat.h \
		-Wno-ambiguous-macro -DLTO_SUPPORT \
		-DCURRENT_PROJECT_VERSION=\"${CCTOOLS_VERSION}\"

all: ${OBJBASE}/libstuff.a ${OBJBASE}/libmacho.a

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
