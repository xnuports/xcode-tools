# bsdmake -- Apple's BSD make (FreeBSD make, 2006-04-12 drop).
#
# Installed as "bsdmake", not as "make": the port's own Makefile sets
# PROG=make, but usr/bin/make is GNU make in a Developer directory and
# stock macOS has always shipped this one as bsdmake.
#
# Defines mirror the port's Makefile: MAKE_VERSION and DEFSHELLNAME are
# both compiled in there rather than discovered, and __FBSDID maps to
# the NetBSD spelling the sources actually have.
T_CFLAGS+=	-I${TOP}/src/bsdmake
T_CFLAGS+=	-DMAKE_VERSION=\"5200408120\"
T_CFLAGS+=	-D__FBSDID=__RCSID
T_CFLAGS+=	-DDEFSHELLNAME=\"sh\"

T_SRCS=	arch.c buf.c cond.c dir.c for.c hash.c hash_tables.c job.c \
	lst.c main.c make.c parse.c proc.c shell.c str.c suff.c targ.c \
	util.c var.c
