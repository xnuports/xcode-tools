# bmake -- the build system itself.
#
# Ships as usr/bin/bmake.  Its own mk files go to usr/share/bmake/mk/
# rather than usr/share/mk/, because the latter is where Apple keeps
# bsdmake's mk files and where the system bmake already looks.
#
# bmake's Makefile uses BSD make syntax (.include, .-include) and
# cannot be driven by GNU make.  The system bmake (already present
# on any Mac with Xcode/CLT) builds our bmake from source.

# Use the system bmake to drive the build — its Makefile is not
# compatible with GNU make.
P_MAKE=			bmake

# bmake's configure does not understand --disable-dependency-tracking
# or --disable-nls.  Suppress the generic autotools flags.
P_NO_AUTOTOOLS_FLAGS=	yes

P_CONFIGURE_ARGS=	--with-default-sys-path=${P_PREFIX}/share/bmake/mk \
			--without-makefile

# bmake's configure has a re-exec that loses --prefix.  Work around
# by passing prefix= on the bmake command line, which overrides the
# ?= in Makefile.config.
P_MAKE_ARGS=		prefix=${P_PREFIX}

# bmake's install puts mk files in ${prefix}/share/mk/.  We want them
# in usr/share/bmake/mk/ instead, so they don't overwrite Apple's
# (bsdmake's) mk files and so bmake's own binary can find them via
# its compiled-in DEFAULT_SYS_PATH.
P_POST_STAGE=		mkdir -p ${TOP}/build/release/usr/share/bmake/mk && \
			cp -R ${P_STAGEDIR}${P_PREFIX}/share/mk/* \
				${TOP}/build/release/usr/share/bmake/mk/ && \
			rm -rf ${P_STAGEDIR}${P_PREFIX}/share/mk

P_PROGS=		bin/bmake
