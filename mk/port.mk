# mk/port.mk
#
# Build driver for components that carry their own build system --
# autoconf, CMake, or anything else we drive rather than compile
# ourselves.  The counterpart to mk/tool.mk, which compiles sources
# directly.
#
# Not meant to be invoked by hand; ports/Makefile recurses into this
# file once per entry of mk/ports.mk:
#
#	bmake -f mk/port.mk TOP=<repo> P_DIR=dist-dev-tools/gperf \
#	                     P_NAME=gperf P_BIN=<install-suffix>
#
# Everything happens outside the submodule.  Autoconf trees are
# configured VPATH-style from a build directory under build/ports/, so
# the source tree is never written to -- the same rule tool.mk follows.
#
# Per-port customization belongs in mk/port.d/<name>.mk.  Recognized
# knobs:
#
#	P_PROGS		programs to stage, relative to the staged prefix
#			(default: bin/${P_NAME})
#	P_LIBS		libraries to stage, relative to the same prefix.
#			They land in P_LIBDIR rather than beside the
#			programs.
#	P_BUILDSYS	"autoconf" (default) or "cmake"
#	P_CONFIGURE	configure script, relative to the source dir
#			(autoconf only; default: configure)
#	P_CONFIGURE_ARGS  extra arguments to configure / cmake
#	P_MAKE		make(1) to drive the port with (default: make,
#			which is GNU make on macOS)
#	P_MAKE_ARGS	extra arguments to make
#	P_LINKS		extra names hardlinked beside each staged program
#	P_COPY		"no" to configure VPATH-style straight from the
#			submodule.  Default "yes": the source is copied
#			into the work directory and built in tree.  Old
#			autoconf/gnulib trees (bison, gm4, gnumake) have
#			rules that only work in tree -- they fail with
#			"No rule to make target 'alloca_.h'" or drop
#			objects entirely.  Copying costs disk but keeps
#			the submodule read-only either way.
#	P_PREPARE	shell command run inside the copied tree after
#			copying and before configure -- the ports-style
#			post-extract step.  Requires P_COPY (the default),
#			since it modifies the tree.
#	P_NOSTAGE	set to any value to skip the install step and take
#			P_PROGS straight out of the build directory.  For
#			a project like LLVM, whose install writes gigabytes
#			of headers and libraries we do not ship, running it
#			just to copy a dozen binaries is pure waste.
#	P_NOBUILD	set to any value to turn the entry into a no-op

TOP?=		${.CURDIR}
P_SRCDIR?=	${TOP}/src/${P_DIR}
P_WORKDIR?=	${TOP}/build/ports/${P_NAME}

P_STAGEDIR?=	${P_WORKDIR}/stage
P_BINDIR?=	${TOP}/build/release/${P_BIN}
P_LIBDIR?=	${TOP}/build/release/${XCTOOLCHAIN}/usr/lib

.include "${TOP}/mk/xcodetools.sys.mk"

sinclude ${TOP}/mk/port.d/${P_NAME}.mk

.MAIN: all

P_BUILDSYS?=		autoconf
P_CONFIGURE?=		configure
.if ${P_BUILDSYS:tl} == "cmake"
# CMake builds out of tree properly, so there is no reason to copy the
# source, and for a tree the size of LLVM copying is actively wasteful.
P_COPY?=		no
P_MAKE?=		ninja
.else
P_MAKE?=		make
.endif
P_PROGS?=		bin/${P_NAME}
P_COPY?=		yes

.if ${P_COPY:tl} == "yes"
# In-tree: configure and build happen inside our private copy, which is
# what these old autoconf projects require.
P_BUILDSRC=	${P_WORKDIR}/src
P_OBJDIR?=	${P_WORKDIR}/src
P_CONFDEP=	${P_WORKDIR}/.copied
.else
# Out-of-tree: build beside the submodule, which stays read-only.
P_BUILDSRC=	${P_SRCDIR}
P_OBJDIR?=	${P_WORKDIR}/build
P_CONFDEP=
.endif

# Ports are configured for a prefix of / and installed into a staging
# directory, so that what lands in the release tree is chosen here
# rather than by the port's own install rules.
P_PREFIX?=		/usr

# For a CMake port, the directory holding the top-level CMakeLists.txt,
# relative to the source dir (LLVM keeps its under llvm/).
P_CMAKE_SRC?=	.

.if defined(P_NOBUILD)
all clean:
	@${ECHO} "skip: ${P_NAME} (P_NOBUILD)"
.else

.if defined(P_NOSTAGE)
P_PROGSRC=	${P_OBJDIR}
all: ${P_WORKDIR}/.built
.else
P_PROGSRC=	${P_STAGEDIR}${P_PREFIX}
all: ${P_WORKDIR}/.staged
.endif
.for f in ${P_PROGS}
	@mkdir -p ${P_BINDIR}
	@cp ${P_PROGSRC}/${f} ${P_BINDIR}/${f:T}
.for l in ${P_LINKS}
	@ln -f ${P_BINDIR}/${f:T} ${P_BINDIR}/${l}
.endfor
.endfor
	@${ECHO} "built: ${P_BIN}/${P_PROGS:T}${P_LINKS:D (+${P_LINKS})}"
.for l in ${P_LIBS}
	@mkdir -p ${P_LIBDIR}
	@cp ${P_PROGSRC}/${l} ${P_LIBDIR}/${l:T}
	@${ECHO} "staged: ${XCTOOLCHAIN}/usr/lib/${l:T}"
.endfor

# --- configure --------------------------------------------------------

# The submodule is never written to: when P_COPY is on we build from a
# private copy, and when it is off we configure VPATH-style with the
# object directory elsewhere.
${P_WORKDIR}/.copied:
	@mkdir -p ${P_WORKDIR}
	@${ECHO} "port: copying ${P_NAME} sources"
	@rsync -a --delete --exclude '.git' ${P_SRCDIR}/ ${P_BUILDSRC}/
.if defined(P_PREPARE)
	@${ECHO} "port: preparing ${P_NAME}"
	@cd ${P_BUILDSRC} && ${P_PREPARE}
.endif
	@touch ${.TARGET}

${P_WORKDIR}/.configured: ${P_CONFDEP}
	@mkdir -p ${P_OBJDIR}
	@${ECHO} "port: configuring ${P_NAME}"
.if ${P_BUILDSYS:tl} == "cmake"
	cd ${P_OBJDIR} && cmake -G Ninja ${P_BUILDSRC}/${P_CMAKE_SRC} \
		-DCMAKE_INSTALL_PREFIX=${P_PREFIX} \
		-DCMAKE_BUILD_TYPE=Release \
		${P_CONFIGURE_ARGS} > ${P_WORKDIR}/configure.log 2>&1 || \
		{ ${ECHO} "port: ${P_NAME}: configure failed, see ${P_WORKDIR}/configure.log"; \
		  tail -20 ${P_WORKDIR}/configure.log; exit 1; }
.else
	cd ${P_OBJDIR} && ${P_BUILDSRC}/${P_CONFIGURE} \
		--prefix=${P_PREFIX} \
		--disable-dependency-tracking \
		--disable-nls \
		${P_CONFIGURE_ARGS} > ${P_WORKDIR}/configure.log 2>&1 || \
		{ ${ECHO} "port: ${P_NAME}: configure failed, see ${P_WORKDIR}/configure.log"; \
		  tail -20 ${P_WORKDIR}/configure.log; exit 1; }
.endif
	@touch ${.TARGET}

# --- build ------------------------------------------------------------

${P_WORKDIR}/.built: ${P_WORKDIR}/.configured
	@${ECHO} "port: building ${P_NAME}"
	cd ${P_OBJDIR} && ${P_MAKE} ${P_MAKE_ARGS} > ${P_WORKDIR}/build.log 2>&1 || \
		{ ${ECHO} "port: ${P_NAME}: build failed, see ${P_WORKDIR}/build.log"; \
		  tail -20 ${P_WORKDIR}/build.log; exit 1; }
	@touch ${.TARGET}

# --- stage ------------------------------------------------------------

${P_WORKDIR}/.staged: ${P_WORKDIR}/.built
	@${ECHO} "port: staging ${P_NAME}"
	cd ${P_OBJDIR} && ${P_MAKE} install DESTDIR=${P_STAGEDIR} \
		> ${P_WORKDIR}/stage.log 2>&1 || \
		{ ${ECHO} "port: ${P_NAME}: stage failed, see ${P_WORKDIR}/stage.log"; \
		  tail -20 ${P_WORKDIR}/stage.log; exit 1; }
	@touch ${.TARGET}

# Verifying here rather than in ports/Makefile, because only this file
# knows what a port actually produces: a port name is not a program name,
# and llvm alone installs ten of them plus a library.
check:
.for f in ${P_PROGS}
	@test -e ${P_BINDIR}/${f:T} || \
		{ ${ECHO} "MISSING: ${P_BIN}/${f:T}  (port ${P_NAME}, see ${P_WORKDIR}/*.log)"; exit 1; }
.endfor
.for l in ${P_LIBS}
	@test -e ${P_LIBDIR}/${l:T} || \
		{ ${ECHO} "MISSING: ${XCTOOLCHAIN}/usr/lib/${l:T}  (port ${P_NAME})"; exit 1; }
.endfor

clean:
	rm -rf ${P_WORKDIR}
.for f in ${P_PROGS}
	rm -f ${P_BINDIR}/${f:T}
.endfor
.for l in ${P_LIBS}
	rm -f ${P_LIBDIR}/${l:T}
.endfor

.endif

.PHONY: all check clean
