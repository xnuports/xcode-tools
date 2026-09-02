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
#	bmake -f mk/port.mk TOP=<repo> P_DIR=distribution-Developer_Tools/gperf \
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
#	P_TREES		whole directories to stage, relative to the same
#			prefix, copied to the matching path under the
#			toolchain's usr/.  clang's resource directory is
#			the reason this exists: without lib/clang/<ver>,
#			clang cannot find its own stdarg.h and anything
#			beyond trivial C fails to compile.
#	P_RELEASE_TREES	directories to stage anywhere in the release
#			tree, as alternating <src> <dest> words: <src>
#			relative to the same place P_PROGS reads from,
#			<dest> relative to build/release.  For a port that
#			installs runtime data outside the toolchain --
#			bmake's mk fragments, for instance.
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
#	P_POST_BUILD	shell command run inside the build directory after
#			the port's build step and before anything is copied
#			out of it.  P_POST_STAGE is the equivalent for ports
#			that stage; a port with P_NOSTAGE has no stage step
#			for it to run in, and this is where it goes instead.
#	P_POST_STAGE	shell command run inside the source tree after
#			the port's install step completes.  The staged
#			prefix is at ${P_STAGEDIR}${P_PREFIX}.  Use this
#			to move files the port installed to non-standard
#			locations (e.g. bmake's mk files).
#	P_NOSTAGE	set to any value to skip the install step and take
#			P_PROGS straight out of the build directory.  For
#			a project like LLVM, whose install writes gigabytes
#			of headers and libraries we do not ship, running it
#			just to copy a dozen binaries is pure waste.
#	P_NOBUILD	set to any value to turn the entry into a no-op
#	P_NO_AUTOTOOLS_FLAGS
#			set to "yes" to suppress the --disable-dependency-
#			tracking and --disable-nls flags that port.mk
#			adds to every autoconf configure.  Needed for
#			projects whose configure does not recognise them
#			(e.g. bmake).

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
P_NO_AUTOTOOLS_FLAGS?=	no

# Compute the autotools flags at parse time so they can be used
# inside recipes without .if (which passes literally to the shell
# when tab-indented inside a recipe).
.if ${P_NO_AUTOTOOLS_FLAGS:tl} == "yes"
_AUTOTOOLS_FLAGS?=
.else
_AUTOTOOLS_FLAGS?=	--disable-dependency-tracking --disable-nls
.endif

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
	@rm -f ${P_BINDIR}/${f:T}
	@cp ${P_PROGSRC}/${f} ${P_BINDIR}/${f:T}
	@chmod u+w ${P_BINDIR}/${f:T}
.for l in ${P_LINKS}
	@ln -f ${P_BINDIR}/${f:T} ${P_BINDIR}/${l}
.endfor
.endfor
	@${ECHO} "built: ${P_BIN}/${P_PROGS:T}${P_LINKS:D (+${P_LINKS})}"
.for l in ${P_LIBS}
	@mkdir -p ${P_LIBDIR}
	@rm -f ${P_LIBDIR}/${l:T}
	@cp ${P_PROGSRC}/${l} ${P_LIBDIR}/${l:T}
	@${ECHO} "staged: ${XCTOOLCHAIN}/usr/lib/${l:T}"
.endfor
.for t in ${P_TREES}
	@mkdir -p ${TOP}/build/release/${XCTOOLCHAIN}/usr/${t:H}
	@rm -rf ${TOP}/build/release/${XCTOOLCHAIN}/usr/${t}
	@cp -R ${P_PROGSRC}/${t} ${TOP}/build/release/${XCTOOLCHAIN}/usr/${t}
	@${ECHO} "staged: ${XCTOOLCHAIN}/usr/${t}/"
.endfor
.for src dst in ${P_RELEASE_TREES}
	@mkdir -p ${TOP}/build/release/${dst:H}
	@rm -rf ${TOP}/build/release/${dst}
	@cp -R ${P_PROGSRC}/${src} ${TOP}/build/release/${dst}
	@${ECHO} "staged: ${dst}/"
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
		${_AUTOTOOLS_FLAGS} \
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
.if defined(P_POST_BUILD)
	@${ECHO} "port: post-build ${P_NAME}"
	@cd ${P_OBJDIR} && ${P_POST_BUILD}
.endif
	@touch ${.TARGET}

# --- stage ------------------------------------------------------------

${P_WORKDIR}/.staged: ${P_WORKDIR}/.built
	@${ECHO} "port: staging ${P_NAME}"
	cd ${P_OBJDIR} && ${P_MAKE} ${P_MAKE_ARGS} install DESTDIR=${P_STAGEDIR} \
		> ${P_WORKDIR}/stage.log 2>&1 || \
		{ ${ECHO} "port: ${P_NAME}: stage failed, see ${P_WORKDIR}/stage.log"; \
		  tail -20 ${P_WORKDIR}/stage.log; exit 1; }
.if defined(P_POST_STAGE)
	@${ECHO} "port: post-stage ${P_NAME}"
	@cd ${P_BUILDSRC} && ${P_POST_STAGE}
.endif
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
.for t in ${P_TREES}
	@test -d ${TOP}/build/release/${XCTOOLCHAIN}/usr/${t} || \
		{ ${ECHO} "MISSING: ${XCTOOLCHAIN}/usr/${t}/  (port ${P_NAME})"; exit 1; }
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
