# bsdmake -- Apple's BSD make, carried as src/bsdmake/.
#
# Its own Makefile depends on <bsd.prog.mk> and the full BSD make
# include chain (bsd.init.mk, bsd.own.mk, bsd.sys.mk, ...) -- the
# very files it ships.  Rather than untangle that dependency, compile
# it directly with mk/tool.mk using the explicit source list from
# its Makefile.
#
# Apple ships this as Developer/usr/bin/bsdmake.  The binary hardcodes
# /usr/share/mk/ as its search path for mk files, so those go to
# usr/share/mk/ in the release tree.

T_SRCS=	arch.c buf.c cond.c dir.c for.c hash.c hash_tables.c job.c \
	lst.c main.c make.c parse.c proc.c shell.c str.c suff.c targ.c \
	util.c var.c

T_CFLAGS+=	-I${T_SRCDIR} \
		-DHAVE_CONFIG_H \
		-DMAKE_NATIVE \
		-DDEFSHELLNAME=\"sh\" \
		-DMAKE_VERSION=\"5200408120\"

T_CFLAGS+=	-Wno-unused-parameter \
		-Wno-sign-compare \
		-Wno-missing-field-initializers

# bsdmake's Makefile adds -mdynamic-no-pic for Apple builds.
.if ${MACHINE_ARCH:Marm64*} != "" || ${MACHINE_ARCH:Maarch64*} != ""
T_CFLAGS+=	-mdynamic-no-pic
.endif

# --- mk files install ------------------------------------------------
# bsdmake ships a set of bsd.*.mk files that go into usr/share/mk/.
# The binary expects to find them there at runtime.
#
# The copy rule depends on ${T_TARGET} (the built binary), so it
# runs right after linking.  Making 'all' depend on the copy rule
# ensures it runs as part of the normal build without creating a
# circular dependency.

BSDMAKE_MKDIR=	${TOP}/build/release/usr/share/mk

${BSDMAKE_MKDIR}/bsd.prog.mk: ${T_TARGET}
	@mkdir -p ${BSDMAKE_MKDIR}
	@cp ${T_SRCDIR}/mk/*.mk ${BSDMAKE_MKDIR}/
	@${ECHO} "staged: usr/share/mk/ (bsdmake mk files)"

all: ${BSDMAKE_MKDIR}/bsd.prog.mk
